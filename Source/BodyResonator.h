#pragma once
#include <JuceHeader.h>
#include <array>
// Fixed-storage biquad: coefficient changes do not allocate or acquire locks.
class BodyResonator
{
public:
    void prepare(double rate) { sampleRate = rate; amount = -1.0f; reset(); setAmount(0.0f); }
    void reset() noexcept { z1 = z2 = 0.0f; }
    void setAmount(float value) noexcept
    {
        if (value == amount) return;
        amount = value;
        const auto c = juce::IIRCoefficients::makePeakFilter(sampleRate,
            juce::jmin(280.0, sampleRate * 0.1), 0.7 + value * 4.0,
            juce::Decibels::decibelsToGain(value * 9.0f));
        std::copy(std::begin(c.coefficients), std::end(c.coefficients), coefficients.begin());
    }
    float process(float x) noexcept
    {
        const float y = coefficients[0] * x + z1;
        z1 = coefficients[1] * x - coefficients[3] * y + z2;
        z2 = coefficients[2] * x - coefficients[4] * y;
        return x + amount * 0.65f * (y - x);
    }
private:
    std::array<float, 5> coefficients {};
    double sampleRate = 48000.0;
    float amount = 0.0f, z1 = 0.0f, z2 = 0.0f;
};
