/*
  ==============================================================================

    PluckedStringSynth.h
    Created: 2026 April
    For Audio Programming Assignment 3

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <cmath>

#include "KarplusStrongModel.h"
#include "BodyResonator.h"

/**
 * Sound class for the plucked-string synthesiser.
 *
 * This sound can be triggered by any MIDI note on any MIDI channel. The actual
 * synthesis is implemented in PluckedStringSynthVoice.
 */
class PluckedStringSynthSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int midiNoteNumber) override
    {
        juce::ignoreUnused(midiNoteNumber);
        return true;
    }

    bool appliesToChannel(int midiChannel) override
    {
        juce::ignoreUnused(midiChannel);
        return true;
    }
};

/**
 * Polyphonic plucked-string synthesiser voice.
 *
 * Each voice contains two Karplus-Strong string models. The two strings are
 * tuned symmetrically around the target MIDI pitch to create a subtle
 * double-string / detune effect. A lightweight body resonator is then applied
 * as a shared post-filter, and the two strings are spread across the stereo
 * output according to the stereo width parameter.
 */
class PluckedStringSynthVoice : public juce::SynthesiserVoice
{
public:
    PluckedStringSynthVoice() = default;

    /**
     * Prepare the internal DSP objects before audio processing starts.
     *
     * This should be called from PluginProcessor::prepareToPlay(), because the
     * string models allocate their internal buffers during preparation.
     *
     * @param sampleRate current audio sample rate
     */
    void prepare(double sampleRate)
    {
        stringLeft.prepare(sampleRate);
        stringRight.prepare(sampleRate);

        bodyResonator.prepare(sampleRate);
    }

    /**
     * Update the synthesis parameters for this voice.
     *
     * These values are normally read from the AudioProcessorValueTreeState in
     * PluginProcessor::processBlock() and passed into every active voice.
     *
     * @param newDecay feedback gain / sustain amount
     * @param newDamping high-frequency damping in the feedback loop
     * @param newBrightness brightness of the initial pluck excitation
     * @param newPickPosition relative plucking position along the string
     * @param newBodyResonance body-resonance post-filter amount
     * @param newStereoWidth stereo spread of the two detuned strings
     * @param newDetuneCents total detune amount in cents
     */
    void setStringParameters(float newDecay,
        float newDamping,
        float newBrightness,
        float newPickPosition,
        float newBodyResonance,
        float newStereoWidth,
        float newDetuneCents)
    {
        decay = newDecay;
        damping = newDamping;
        brightness = newBrightness;
        pickPosition = newPickPosition;

        bodyResonance = juce::jlimit(0.0f, 1.0f, newBodyResonance);
        stereoWidth = juce::jlimit(0.0f, 1.0f, newStereoWidth);
        detuneCents = juce::jlimit(0.0f, 20.0f, newDetuneCents);

        stringLeft.setDecay(decay);
        stringLeft.setDamping(damping);
        stringLeft.setBrightness(brightness);
        stringLeft.setPickPosition(pickPosition);

        stringRight.setDecay(decay);
        stringRight.setDamping(damping);
        stringRight.setBrightness(brightness);
        stringRight.setPickPosition(pickPosition);

        bodyResonator.setAmount(bodyResonance);
    }

    /**
     * Start a new MIDI note.
     *
     * The MIDI note is converted to frequency, then used to trigger two
     * Karplus-Strong strings. The left string is tuned slightly below the target
     * pitch and the right string slightly above it.
     *
     * @param midiNoteNumber MIDI note number
     * @param velocity MIDI velocity, normally between 0 and 1
     * @param sound sound object assigned by the JUCE synthesiser
     * @param currentPitchWheelPosition current pitch wheel value
     */
    void startNote(int midiNoteNumber,
        float velocity,
        juce::SynthesiserSound* sound,
        int currentPitchWheelPosition) override
    {
        juce::ignoreUnused(sound, currentPitchWheelPosition);

        const float baseFrequency =
            static_cast<float>(juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber));

        // Pick position, brightness and damping must be set before pluck(),
        // because they affect the excitation and feedback loop of the new note.
        stringLeft.setDecay(decay);
        stringLeft.setDamping(damping);
        stringLeft.setBrightness(brightness);
        stringLeft.setPickPosition(pickPosition);

        stringRight.setDecay(decay);
        stringRight.setDamping(damping);
        stringRight.setBrightness(brightness);
        stringRight.setPickPosition(pickPosition);

        // Symmetrical detuning around the target pitch.
        //
        // detuneCents is the total distance between the two strings:
        // left  = -detuneCents / 2
        // right = +detuneCents / 2
        const float leftFrequency =
            baseFrequency * std::pow(2.0f, -detuneCents / 2400.0f);

        const float rightFrequency =
            baseFrequency * std::pow(2.0f, detuneCents / 2400.0f);

        stringLeft.pluck(leftFrequency, velocity);
        stringRight.pluck(rightFrequency, velocity);

        // Reset the resonator so that filter memory from a previous note does
        // not colour the start of the new note.
        bodyResonator.reset();
        bodyResonator.setAmount(bodyResonance);

        stringLeft.setKeyHeld(true);
        stringRight.setKeyHeld(true);

        currentLevel = 1.0f;
        noteIsReleasing = false;

        // For debugging
        /*
        DBG("Double-string note started: " << midiNoteNumber
            << " base frequency: " << baseFrequency
            << " left frequency: " << leftFrequency
            << " right frequency: " << rightFrequency
            << " detune cents: " << detuneCents
            << " stereo width: " << stereoWidth);
        */
    }

    /**
     * Respond to a MIDI note-off message.
     *
     * If tail-off is allowed, the strings continue to decay naturally with a
     * slightly faster release. Otherwise the voice is cleared immediately.
     *
     * @param velocity note-off velocity
     * @param allowTailOff true if the voice should decay naturally
     */
    void stopNote(float velocity, bool allowTailOff) override
    {
        juce::ignoreUnused(velocity);

        stringLeft.setKeyHeld(false);
        stringRight.setKeyHeld(false);

        noteIsReleasing = true;

        if (!allowTailOff)
        {
            stringLeft.reset();
            stringRight.reset();
            bodyResonator.reset();

            clearCurrentNote();
        }
    }

    /**
     * Render this voice into the output buffer.
     *
     * The voice generates two detuned string signals, applies a shared body
     * colouration, then distributes the strings across the stereo field. The
     * output is added to the existing buffer contents, allowing multiple voices
     * to play polyphonically.
     *
     * @param outputBuffer buffer to receive the rendered voice
     * @param startSample first sample position to write into
     * @param numSamples number of samples to render
     */
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
        int startSample,
        int numSamples) override
    {
        bodyResonator.setAmount(bodyResonance);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            float leftStringSample = stringLeft.processSample();
            float rightStringSample = stringRight.processSample();

            // The body resonator is applied to the combined string signal,
            // representing a shared resonating instrument body.
            const float bodyInput =
                0.5f * (leftStringSample + rightStringSample);

            const float bodySample =
                bodyResonator.processSample(bodyInput);

            // Blend individual string signals with the shared body colour.
            leftStringSample =
                0.75f * leftStringSample + 0.25f * bodySample;

            rightStringSample =
                0.75f * rightStringSample + 0.25f * bodySample;

            if (noteIsReleasing)
                currentLevel *= releaseMultiplier;

            leftStringSample *= currentLevel;
            rightStringSample *= currentLevel;

            // stereoWidth = 0: both strings are centred.
            // stereoWidth = 1: the two strings are spread left and right.
            const float centreGain = 0.5f * (1.0f - stereoWidth);
            const float sideGain = 0.5f + 0.5f * stereoWidth;

            const float leftOutput =
                sideGain * leftStringSample + centreGain * rightStringSample;

            const float rightOutput =
                centreGain * leftStringSample + sideGain * rightStringSample;

            if (outputBuffer.getNumChannels() == 1)
            {
                const float monoOutput =
                    0.5f * (leftOutput + rightOutput);

                outputBuffer.addSample(0,
                    startSample + sample,
                    monoOutput);
            }
            else
            {
                outputBuffer.addSample(0,
                    startSample + sample,
                    leftOutput);

                outputBuffer.addSample(1,
                    startSample + sample,
                    rightOutput);

                // Extra output channels receive a safe mono sum.
                for (int channel = 2; channel < outputBuffer.getNumChannels(); ++channel)
                {
                    const float monoOutput =
                        0.5f * (leftOutput + rightOutput);

                    outputBuffer.addSample(channel,
                        startSample + sample,
                        monoOutput);
                }
            }

            // Free the voice once both strings are inactive or the release fade
            // has become effectively silent.
            if ((!stringLeft.getIsActive() && !stringRight.getIsActive())
                || currentLevel < silenceLevel)
            {
                stringLeft.reset();
                stringRight.reset();
                bodyResonator.reset();

                clearCurrentNote();
                break;
            }
        }
    }

    /**
     * Pitch wheel input is not used in this implementation.
     */
    void pitchWheelMoved(int newPitchWheelValue) override
    {
        juce::ignoreUnused(newPitchWheelValue);
    }

    /**
     * MIDI controller input is not used in this implementation.
     */
    void controllerMoved(int controllerNumber, int newControllerValue) override
    {
        juce::ignoreUnused(controllerNumber, newControllerValue);
    }

    /**
     * Check whether this voice can play the supplied sound.
     *
     * @param sound sound assigned by the JUCE synthesiser
     * @return true if the sound is a PluckedStringSynthSound
     */
    bool canPlaySound(juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<PluckedStringSynthSound*>(sound) != nullptr;
    }

private:
    // Internal sound generators
    KarplusStrongString stringLeft;
    KarplusStrongString stringRight;

    // Shared body colouration stage
    BodyResonator bodyResonator;

    // Current synthesis parameters
    float decay = 0.85f;
    float damping = 0.4f;
    float brightness = 0.9f;
    float pickPosition = 0.20f;
    float bodyResonance = 0.3f;
    float stereoWidth = 0.5f;
    float detuneCents = 5.0f;

    // Voice release state
    float currentLevel = 0.0f;
    bool noteIsReleasing = false;

    static constexpr float releaseMultiplier = 0.9995f;
    static constexpr float silenceLevel = 0.0001f;
};