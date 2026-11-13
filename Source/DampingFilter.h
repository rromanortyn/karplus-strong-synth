#pragma once
#include <JuceHeader.h>
// One-pole loop filter: y[n] = alpha*x[n] + (1-alpha)*y[n-1].
class DampingFilter
{
public:
    void prepare(double rate) { alpha.reset(rate, 0.02); reset(); }
    void setCoefficient(float value, bool immediately = false) noexcept
    {
        value = juce::jlimit(0.05f, 1.0f, value);
        if (immediately) alpha.setCurrentAndTargetValue(value);
        else alpha.setTargetValue(value);
    }
    float process(float input) noexcept { state += alpha.getNextValue() * (input - state); return state; }
    void reset() noexcept { state = 0.0f; }
private:
    juce::SmoothedValue<float> alpha { 0.6f };
    float state = 0.0f;
};
