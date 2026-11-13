#pragma once
#include "DelayLine.h"
#include "DampingFilter.h"
#include "Exciter.h"
#include "Parameters.h"
#include "StringTuning.h"
#include <vector>

class KarplusStrongString
{
public:
    void prepare(double rate)
    {
        sampleRate = rate;
        const int maximum = static_cast<int>(std::ceil(rate / 6.0)) + 8;
        delay.prepare(maximum, rate); excitation.resize(static_cast<size_t>(maximum));
        filter.prepare(rate); feedback.reset(rate, 0.02); reset();
    }
    void reset() noexcept { delay.clear(); filter.reset(); active = false; quietSamples = 0; }
    void setParameters(const StringParameters& value) noexcept
    {
        const bool tuningChanged = value.damping != parameters.damping
            || value.interpolation != parameters.interpolation || value.decay != parameters.decay;
        parameters = value; filter.setCoefficient(value.damping);
        if (active && tuningChanged) tune(frequency, false);
    }
    void tune(float hz, bool immediate) noexcept
    {
        frequency = juce::jlimit(6.1f, static_cast<float>(sampleRate * 0.45), hz);
        const double omega = juce::MathConstants<double>::twoPi * frequency / sampleRate;
        const float seconds = 0.1f + parameters.decay * 11.9f;
        const float gain = std::exp(-6.907755f / (frequency * seconds));
        const auto tuning = StringTuning::calculate(omega, parameters.damping, gain, parameters.interpolation);
        delay.setDelay(static_cast<float>(tuning.delay), static_cast<float>(omega),
                       parameters.interpolation, parameters.pitchTransition, immediate,
                       static_cast<float>(tuning.coefficient));
        if (immediate) feedback.setCurrentAndTargetValue(gain);
        else feedback.setTargetValue(gain);
    }
    void pluck(float hz, float velocity) noexcept
    {
        reset(); tune(hz, true); filter.setCoefficient(parameters.damping, true);
        const int length = juce::jlimit(2, static_cast<int>(excitation.size()) - 1,
                                      static_cast<int>(std::ceil(sampleRate / frequency)));
        exciter.fill(excitation.data(), length, parameters.exciterType, parameters.impulseWidth,
                     parameters.brightness, parameters.exciteAmount * velocity);
        const int pick = juce::jlimit(1, length - 1, static_cast<int>(length * parameters.pickPosition));
        const float comb = (0.5f - parameters.pickPosition) / 0.45f;
        for (int i = 0; i < length; ++i)
        {
            const int other = (i + length - pick) % length;
            delay.write(excitation[static_cast<size_t>(i)] - comb * excitation[static_cast<size_t>(other)]);
            delay.advance();
        }
        active = true; quietSamples = 0;
    }
    float process() noexcept
    {
        if (!active) return 0.0f;
        const float output = delay.read();
        const float next = filter.process(output) * feedback.getNextValue();
        if (!std::isfinite(next)) { reset(); return 0.0f; }
        delay.write(next); delay.advance();
        if (std::abs(output) < 1.0e-7f) ++quietSamples; else quietSamples = 0;
        if (quietSamples > static_cast<int>(sampleRate / frequency) * 2 + 256) active = false;
        return output;
    }
    bool isActive() const noexcept { return active; }
    void copySnapshot(float* data, int count) const noexcept { delay.copySnapshot(data, count); }
private:
    DelayLine delay;
    DampingFilter filter;
    Exciter exciter;
    std::vector<float> excitation;
    StringParameters parameters;
    juce::SmoothedValue<float> feedback;
    double sampleRate = 48000.0;
    float frequency = 440.0f;
    bool active = false;
    int quietSamples = 0;
};
