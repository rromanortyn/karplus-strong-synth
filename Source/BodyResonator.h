/*
  ==============================================================================

    BodyResonator.h
    Created: 2026 April
    For Audio Programming Assignment 3

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <cmath>

/**
 * Lightweight body-resonance-inspired post-filter.
 *
 * The Karplus-Strong model mainly simulates string vibration. On its own, the
 * raw string output can sound thin because it lacks the spectral colouring of
 * an acoustic instrument body.
 *
 * This class applies a resonant peak filter in the low-mid frequency region and
 * mixes the filtered signal with the dry string signal:
 *
 * string output -> peak filter -> dry/wet mix -> output
 *
 * This is not a full physical model of an instrument body. It is a simple and
 * efficient tone-shaping stage designed to make the plucked-string sound fuller.
 */
class BodyResonator
{
public:
    BodyResonator() = default;

    /**
     * Prepare the resonator before audio processing starts.
     *
     * @param newSampleRate current audio sample rate
     */
    void prepare(double newSampleRate)
    {
        sampleRate = newSampleRate;

        bodyFilter.reset();
        updateFilterCoefficients();
    }

    /**
     * Reset the internal filter state.
     *
     * This prevents filter memory from a previous note from leaking into a new
     * plucked note.
     */
    void reset()
    {
        bodyFilter.reset();
    }

    /**
     * Set the body resonance amount.
     *
     * The value controls both the dry/wet mix and the strength of the resonant
     * peak filter.
     *
     * @param newAmount value between 0 and 1
     */
    void setAmount(float newAmount)
    {
        newAmount = juce::jlimit(0.0f, 1.0f, newAmount);

        // Avoid recalculating filter coefficients when the parameter change is
        // too small to matter.
        if (std::abs(newAmount - amount) > 0.0001f)
        {
            amount = newAmount;
            updateFilterCoefficients();
        }
    }

    /**
     * Process one audio sample through the resonator.
     *
     * @param inputSample raw Karplus-Strong string sample
     * @return body-coloured output sample
     */
    float processSample(float inputSample)
    {
        if (amount <= 0.0001f)
            return inputSample;

        const float resonatedSample =
            bodyFilter.processSingleSampleRaw(inputSample);

        // Keep some dry signal so the pluck attack remains clear.
        const float mix = 0.65f * amount;

        return (1.0f - mix) * inputSample
            + mix * resonatedSample;
    }

private:

    /**
     * Update the resonant peak filter coefficients.
     *
     * The centre frequency is placed in the low-mid region, where simple
     * plucked-instrument body colouration is clearly perceived.
     */
    void updateFilterCoefficients()
    {
        if (sampleRate <= 0.0)
            return;

        const float centreFrequency = 280.0f;

        // Higher amount values produce a narrower and stronger resonance.
        const float q = 0.7f + amount * 4.0f;

        // 0 dB at amount = 0, up to approximately +9 dB at amount = 1.
        const float gainDb = amount * 9.0f;
        const float gainFactor =
            juce::Decibels::decibelsToGain(gainDb);

        bodyFilter.setCoefficients(
            juce::IIRCoefficients::makePeakFilter(
                sampleRate,
                centreFrequency,
                q,
                gainFactor
            )
        );
    }

    double sampleRate = 44100.0;

    float amount = 0.0f;

    juce::IIRFilter bodyFilter;
};