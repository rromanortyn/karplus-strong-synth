/*
  ==============================================================================

    KarplusStrongModel.h
    Created: 2026 April
    For Audio Programming Assignment 3

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "DelayLine.h"

#include <vector>
#include <cmath>
#include <algorithm>

/**
 * Real-time Karplus-Strong plucked-string model.
 *
 * The model is based on a fractional delay line inside a feedback loop:
 *
 * initial noise excitation
 *      -> brightness-controlled excitation filtering
 *      -> pick-position comb shaping
 *      -> fractional delay line
 *      -> damping filter
 *      -> feedback gain
 *      -> back into the delay line
 *
 * The delay length determines pitch, the feedback gain controls sustain, and
 * the damping filter controls the rate at which high-frequency energy is lost.
 *
 * Compared with the basic integer-delay Karplus-Strong formulation, this
 * implementation uses a fractional delay line with linear interpolation. This
 * allows more accurate tuning when the required period is not an integer number
 * of samples.
 */
class KarplusStrongString
{
public:
    KarplusStrongString()
    {
        // Keep the default decay parameter and feedback gain consistent.
        setDecay(decay);
    }

    /**
     * Prepare the string model before audio processing starts.
     *
     * This allocates the delay line and the excitation buffer. It should be
     * called from prepareToPlay(), because memory allocation should not happen
     * during real-time audio processing.
     *
     * @param newSampleRate current audio sample rate
     * @param maximumDelaySeconds maximum delay-line length in seconds
     */
    void prepare(double newSampleRate, float maximumDelaySeconds = 2.0f)
    {
        sampleRate = newSampleRate;

        const int maximumDelaySamples =
            static_cast<int>(sampleRate * static_cast<double>(maximumDelaySeconds));

        delayLine.setMaximumDelaySize(maximumDelaySamples);

        // Allocate the largest required excitation buffer once during preparation.
        excitationBuffer.clear();
        excitationBuffer.resize(maximumDelaySamples, 0.0f);

        reset();
    }

    /**
     * Clear the delay line and reset the internal state.
     *
     * This immediately removes any stored string energy from the model.
     */
    void reset()
    {
        delayLine.clear();

        previousDelaySample = 0.0f;
        previousExcitationSample = 0.0f;

        currentAmplitude = 0.0f;

        keyHeld = false;
        isActive = false;
        silenceCounter = 0;

        std::fill(excitationBuffer.begin(), excitationBuffer.end(), 0.0f);
    }

    /**
     * Trigger a new plucked-string note.
     *
     * The delay time is calculated from the target frequency. The delay line is
     * then filled with a short noise burst, representing the initial energy of
     * the pluck.
     *
     * Brightness controls the amount of smoothing applied to the excitation.
     * Pick position applies comb-like shaping to the excitation:
     *
     *     excitation[n] - excitation[n - pickDelay]
     *
     * This creates spectral notches related to the plucking point, changing the
     * harmonic balance of the note.
     *
     * The damping filter in the feedback loop contributes a small additional
     * phase delay. A compensation term is therefore subtracted from the delay
     * length to keep the perceived pitch closer to the requested frequency.
     *
     * @param frequency target string frequency in Hz
     * @param velocity MIDI velocity, usually between 0 and 1
     */
    void pluck(float frequency, float velocity)
    {
        if (sampleRate <= 0.0)
            return;

        // Prevent excessively long or physically unrealistic delay times.
        if (frequency < minimumFrequency)
            frequency = minimumFrequency;

        currentAmplitude = juce::jlimit(0.0f, 1.0f, velocity);

        keyHeld = true;
        isActive = true;
        silenceCounter = 0;

        previousDelaySample = 0.0f;
        previousExcitationSample = 0.0f;

        // The two-point averaging filter has approximately half a sample of
        // delay when fully applied. Since damping crossfades into this filter,
        // the compensation is scaled by damping.
        const float loopFilterDelay =
            0.5f * damping;

        const float delayTimeInSamples =
            juce::jmax(1.0f,
                static_cast<float>(sampleRate) / frequency - loopFilterDelay);

        delayLine.setDelayTimeInSamples(delayTimeInSamples);
        delayLine.clear();

        // Fill roughly one period of the delay line with excitation.
        const int excitationLength =
            juce::jmax(2, static_cast<int>(std::ceil(delayTimeInSamples)));

        int pickDelay =
            static_cast<int>(std::round(static_cast<float>(excitationLength) * pickPosition));

        // pickDelay must be non-zero, otherwise the comb difference would cancel
        // the excitation completely.
        pickDelay = juce::jlimit(1, excitationLength - 1, pickDelay);

        // This should already be true if prepare() was called correctly.
        if (excitationLength > static_cast<int>(excitationBuffer.size()))
            return;

        std::fill(excitationBuffer.begin(),
            excitationBuffer.begin() + excitationLength,
            0.0f);

        // Generate a brightness-controlled noise excitation.
        for (int i = 0; i < excitationLength; ++i)
        {
            const float rawNoise =
                random.nextFloat() * 2.0f - 1.0f;

            const float smoothedNoise =
                0.5f * (rawNoise + previousExcitationSample);

            const float excitationSample =
                brightness * rawNoise
                + (1.0f - brightness) * smoothedNoise;

            excitationBuffer[i] = excitationSample;

            previousExcitationSample = excitationSample;
        }

        // Apply pick-position comb shaping and write the excitation into the
        // delay line.
        for (int i = 0; i < excitationLength; ++i)
        {
            int delayedIndex = i - pickDelay;

            if (delayedIndex < 0)
                delayedIndex += excitationLength;

            float pickShapedSample =
                excitationBuffer[i] - excitationBuffer[delayedIndex];

            // Difference filtering can increase the peak level, so the result is
            // attenuated before entering the feedback loop.
            pickShapedSample *= 0.5f;

            delayLine.write(pickShapedSample * currentAmplitude * excitationGain);
            delayLine.advance();
        }
    }

    /**
     * Process one sample of the string model.
     *
     * Each sample performs one feedback-loop recursion:
     *
     * 1. Read the delayed sample.
     * 2. Apply the damping filter.
     * 3. Apply the feedback gain.
     * 4. Write the result back into the delay line.
     * 5. Advance the circular-buffer positions.
     *
     * @return next output sample
     */
    float processSample()
    {
        if (!isActive)
            return 0.0f;

        const float delayedSample = delayLine.read();

        // Classic Karplus-Strong uses a two-point averaging filter. Here, the
        // damping parameter crossfades between no averaging and full averaging.
        const float twoPointAverage =
            0.5f * (delayedSample + previousDelaySample);

        const float dampedSample =
            (1.0f - damping) * delayedSample
            + damping * twoPointAverage;

        previousDelaySample = delayedSample;

        // After note-off, the feedback gain is slightly reduced so that released
        // notes decay more quickly.
        const float effectiveFeedbackGain =
            keyHeld
            ? feedbackGain
            : feedbackGain * releaseFeedbackMultiplier;

        const float feedbackSample =
            dampedSample * effectiveFeedbackGain;

        delayLine.write(feedbackSample);
        delayLine.advance();

        // Deactivate only after several quiet samples, avoiding false triggering
        // at zero crossings.
        if (std::abs(delayedSample) < silenceThreshold)
        {
            ++silenceCounter;

            if (silenceCounter > silenceSamplesRequired)
                isActive = false;
        }
        else
        {
            silenceCounter = 0;
        }

        return delayedSample;
    }

    /**
     * Set the brightness of newly plucked notes.
     *
     * This parameter is applied during excitation generation, so it affects new
     * notes rather than notes that are already ringing.
     *
     * @param newBrightness value between 0 and 1
     */
    void setBrightness(float newBrightness)
    {
        brightness = juce::jlimit(0.0f, 1.0f, newBrightness);
    }

    /**
     * Set the feedback decay amount.
     *
     * The 0-1 decay parameter is mapped onto a stable feedback-gain range.
     * Higher values give longer sustain.
     *
     * @param newDecay value between 0 and 1
     */
    void setDecay(float newDecay)
    {
        decay = juce::jlimit(0.0f, 1.0f, newDecay);

        feedbackGain = juce::jmap(decay,
            minimumFeedbackGain,
            maximumFeedbackGain);
    }

    /**
     * Set the damping amount in the feedback loop.
     *
     * Higher damping applies more two-point averaging, causing high-frequency
     * energy to decay faster and producing a darker sound.
     *
     * @param newDamping value between 0 and 1
     */
    void setDamping(float newDamping)
    {
        damping = juce::jlimit(0.0f, 1.0f, newDamping);
    }

    /**
     * Set the pick position used for newly triggered notes.
     *
     * The value represents the relative position along the string. A smaller
     * value means picking closer to the bridge, while 0.5 represents picking
     * near the middle of the string.
     *
     * @param newPickPosition value between 0.05 and 0.50
     */
    void setPickPosition(float newPickPosition)
    {
        pickPosition = juce::jlimit(0.05f, 0.50f, newPickPosition);
    }

    /**
     * Set whether the MIDI key is currently held.
     *
     * When the key is released, the model uses a slightly lower effective
     * feedback gain to create a faster release.
     *
     * @param shouldBeHeld true while the MIDI key is held
     */
    void setKeyHeld(bool shouldBeHeld)
    {
        keyHeld = shouldBeHeld;
    }

    /**
     * Check whether the string is still producing sound.
     *
     * @return true if the model is active
     */
    bool getIsActive() const
    {
        return isActive;
    }

private:
    // Core delay-line model
    DelayLine delayLine;

    // Excitation generation
    juce::Random random;
    std::vector<float> excitationBuffer;

    // System state
    double sampleRate = 44100.0;

    bool isActive = false;
    bool keyHeld = false;

    int silenceCounter = 0;

    // User-controlled parameters
    float brightness = 0.8f;
    float damping = 0.5f;
    float decay = 0.85f;
    float pickPosition = 0.20f;

    // Internal signal state
    float feedbackGain = 0.98f;
    float currentAmplitude = 0.0f;
    float previousDelaySample = 0.0f;
    float previousExcitationSample = 0.0f;

    // Excitation scaling
    static constexpr float excitationGain = 0.5f;

    // Stable feedback-gain range
    static constexpr float maximumFeedbackGain = 0.9992f;
    static constexpr float minimumFeedbackGain = 0.70f;

    // Minimum playable frequency
    static constexpr float minimumFrequency = 20.0f;

    // Additional damping after note-off
    static constexpr float releaseFeedbackMultiplier = 0.995f;

    // Silence detection
    static constexpr float silenceThreshold = 0.00001f;
    static constexpr int silenceSamplesRequired = 2048;
};