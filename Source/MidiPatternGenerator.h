/*
  ==============================================================================

    MidiPatternGenerator.h
    Created: 2026 April
    For Audio Programming Assignment 3

    A simple internal MIDI Pattern Generator.

    This class does not generate audio directly. Instead, it generates MIDI
    note-on and note-off messages, which are then passed into the same
    juce::Synthesiser used by external MIDI input.

    Therefore, manually played MIDI notes and MidiPatternGenerator notes share the same
    PluckedStringVoice, KarplusStrongString, and sound-shaping parameters.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>

/**
 * Simple internal pattern generator.
 *
 * The MidiPatternGenerator works as an optional internal MIDI generator.
 * When enabled, it adds a repeating pattern of MIDI notes to a MidiBuffer.
 * These MIDI notes can then trigger the same synth voices as normal external
 * MIDI input.
 */
class MidiPatternGenerator
{
public:
    MidiPatternGenerator()
    {
        setPattern(0);
    }

    /**
     * Prepare the PatternGenerator before audio processing starts.
     *
     * @param newSampleRate The current audio sample rate.
     */
    void prepare(double newSampleRate)
    {
        sampleRate = newSampleRate;
        updateStepLength();
        reset();
    }

    /**
     * Reset the PatternGenerator timing and note state.
     *
     * This should be called when playback starts, when sample rate changes,
     * or when turning the PatternGenerator off to avoid stuck notes.
     */
    void reset()
    {
        currentStep = 0;
        sampleCounter = 0.0;
        lastNote = -1;
    }

    /**
     * Enable or disable the PatternGenerator.
     *
     * This only changes whether new notes are generated.
     */
    void setEnabled(bool shouldBeEnabled)
    {
        enabled = shouldBeEnabled;
    }

    /**
     * Return whether the PatternGenerator is enabled.
     */
    bool isEnabled() const
    {
        return enabled;
    }

    /**
     * Set the PatternGenerator rate in notes per second.
     *
     * Example:
     * 2.0  = two notes per second
     * 4.0  = four notes per second
     * 8.0  = eight notes per second
     */
    void setRateHz(float newRateHz)
    {
        rateHz = juce::jlimit(0.5f, 16.0f, newRateHz);
        updateStepLength();
    }

    /**
     * Set the velocity used by generated MIDI note-on messages.
     *
     * Range:
     * 0.0 = silent
     * 1.0 = maximum MIDI velocity
     */
    void setVelocity(float newVelocity)
    {
        velocity = juce::jlimit(0.0f, 1.0f, newVelocity);
    }

    /**
     * Choose one of several fixed patterns.
     *
     * Pattern index:
     * 0 = Major
     * 1 = Minor
     * 2 = Fifths
     * 3 = Cluster
     * 4 = Bass Octave
     * 5 = wide harp sweep
     * 6 = suspended pedal
     * 7 = chromatic tension
     *
     * @param patternIndex pattern number
     */
    void setPattern(int patternIndex)
    {
        patternIndex = juce::jlimit(0, 7, patternIndex);

        if (patternIndex == currentPatternIndex && !pattern.empty())
            return;

        currentPatternIndex = patternIndex;

        switch (patternIndex)
        {
        case 1:
            // A minor-like pattern
            pattern = { 57, 60, 64, 69, 64, 60 };
            break;

        case 2:
            // Open fifths / resonant-wall style
            pattern = { 48, 55, 60, 67, 72, 67 };
            break;

        case 3:
            // Slightly more experimental cluster
            pattern = { 60, 62, 65, 69, 74, 77 };
            break;

        case 4:
            // Lower octave pattern for bass pluck demonstration
            pattern = { 36, 43, 48, 55, 48, 43 };
            break;

        case 5:
            // Wide harp sweep over more than two octaves.
            pattern = { 36, 48, 55, 60, 67, 72, 79, 84 };
            break;

        case 6:
            // Suspended pattern with repeated pedal tones.
            pattern = { 50, 57, 60, 65, 60, 57, 62, 69 };
            break;

        case 7:
            // Chromatic tension with close semitone movement.
            pattern = { 60, 61, 60, 58, 59, 60, 64, 63 };
            break;

        case 0:
        default:
            // C major-like pattern
            pattern = { 60, 64, 67, 72, 67, 64 };
            break;
        }

        currentStep = 0;
    }

    /**
     * Add generated MIDI events to the supplied MidiBuffer.
     *
     * @param midiBuffer  The buffer to which generated MIDI events are added.
     * @param numSamples  Number of samples in the current audio block.
     */
    void process(juce::MidiBuffer& midiBuffer, int numSamples)
    {
        if (!enabled || pattern.empty() || sampleRate <= 0.0 || stepLengthInSamples <= 0.0)
            return;

        int blockSamplePosition = 0;

        while (blockSamplePosition < numSamples)
        {
            const int samplesUntilNextTrigger =
                static_cast<int>(stepLengthInSamples - sampleCounter);

            const int samplesRemainingInBlock =
                numSamples - blockSamplePosition;

            if (samplesUntilNextTrigger <= samplesRemainingInBlock)
            {
                blockSamplePosition += samplesUntilNextTrigger;
                sampleCounter = 0.0;

                triggerNextNote(midiBuffer, blockSamplePosition);
            }
            else
            {
                sampleCounter += samplesRemainingInBlock;
                break;
            }
        }
    }



private:
    /**
     * Recalculate how many samples there are between notes.
     */
    void updateStepLength()
    {
        if (sampleRate > 0.0 && rateHz > 0.0f)
            stepLengthInSamples = sampleRate / static_cast<double>(rateHz);
    }

    /**
     * Generate note-off for the previous note, then note-on for the next note in the pattern.
     */
    void triggerNextNote(juce::MidiBuffer& midiBuffer, int sampleOffset)
    {
        constexpr int midiChannel = 1;

        // Stop the previous note.
        if (lastNote >= 0)
        {
            midiBuffer.addEvent(
                juce::MidiMessage::noteOff(midiChannel, lastNote),
                sampleOffset);
        }

        // Start the next note.
        const int newNote = pattern[static_cast<size_t>(currentStep)];

        midiBuffer.addEvent(
            juce::MidiMessage::noteOn(midiChannel, newNote, velocity),
            sampleOffset);

        lastNote = newNote;

        // Move to the next step.
        currentStep++;

        if (currentStep >= static_cast<int>(pattern.size()))
            currentStep = 0;
    }



    bool enabled = false;

    double sampleRate = 44100.0;

    float rateHz = 4.0f;
    float velocity = 0.8f;

    double stepLengthInSamples = 11025.0;
    double sampleCounter = 0.0;

    int currentStep = 0;
    int currentPatternIndex = -1;
    int lastNote = -1;

    std::vector<int> pattern;
};