#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <cmath>
#include <vector>

// Allocated in prepare only. Two read heads allow pitch changes to crossfade.
class DelayLine
{
public:
    void prepare(int maximumSize, double sampleRate)
    {
        buffer.assign(static_cast<size_t>(maximumSize), 0.0f);
        ramp.reset(sampleRate, 0.02);
        fadeLength = juce::jmax(1, static_cast<int>(sampleRate * 0.02));
        clear();
    }
    void clear() noexcept
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePosition = 0; current = {}; previous = {}; fadeRemaining = 0;
        targetDelay = 1.0f;
        ramp.setCurrentAndTargetValue(1.0f);
    }
    void setDelay(float delay, float omega, int interpolation, int transition, bool immediate = false,
                  float coefficient = 0.0f) noexcept
    {
        delay = juce::jlimit(1.0f, static_cast<float>(buffer.size() - 2), delay);
        if (!immediate && std::abs(delay - targetDelay) < 0.000001f && interpolation == current.mode
            && std::abs(coefficient - targetCoefficient) < 0.000001f)
            return;
        targetDelay = delay;
        targetCoefficient = coefficient;
        if (immediate)
        {
            current = { delay, omega, interpolation, 0.0f, 0.0f };
            ramp.setCurrentAndTargetValue(delay); fadeRemaining = 0;
        }
        else if (transition == 1 || interpolation != current.mode)
        {
            previous = current;
            current = { delay, omega, interpolation, current.x1, current.y1 };
            ramp.setCurrentAndTargetValue(delay); fadeRemaining = fadeLength;
        }
        else
        {
            current.omega = omega; ramp.setTargetValue(delay);
        }
        updateCoefficient(current);
        if (!ramp.isSmoothing()) current.coefficient = targetCoefficient;
    }
    float read() noexcept
    {
        if (ramp.isSmoothing())
        {
            current.delay = ramp.getNextValue(); updateCoefficient(current);
            if (!ramp.isSmoothing()) current.coefficient = targetCoefficient;
        }
        const float value = readTap(current);
        if (fadeRemaining == 0) return value;
        const float mix = 1.0f - static_cast<float>(fadeRemaining--) / static_cast<float>(fadeLength);
        return readTap(previous) * (1.0f - mix) + value * mix;
    }
    void write(float value) noexcept { buffer[static_cast<size_t>(writePosition)] = value; }
    void advance() noexcept { if (++writePosition == static_cast<int>(buffer.size())) writePosition = 0; }
    void copySnapshot(float* destination, int count) const noexcept
    {
        const int period = juce::jmax(1, static_cast<int>(current.delay));
        for (int i = 0; i < count; ++i)
        {
            int index = writePosition - period + i * period / count;
            if (index < 0) index += static_cast<int>(buffer.size());
            destination[i] = buffer[static_cast<size_t>(index)];
        }
    }
private:
    struct Tap
    {
        float delay = 1.0f, omega = 0.0f;
        int mode = 0;
        float x1 = 0.0f, y1 = 0.0f, coefficient = 0.0f;
    };
    static void updateCoefficient(Tap& tap) noexcept
    {
        if (tap.mode == 0) return;
        const float fraction = tap.delay - std::floor(tap.delay);
        const float denominator = std::sin(0.5f * tap.omega * (1.0f + fraction));
        tap.coefficient = denominator > 1.0e-9f
            ? std::sin(0.5f * tap.omega * (1.0f - fraction)) / denominator
            : (1.0f - fraction) / (1.0f + fraction);
    }
    float readTap(Tap& tap) noexcept
    {
        const int integer = static_cast<int>(tap.delay);
        const float fraction = tap.delay - static_cast<float>(integer);
        int index = writePosition - integer;
        if (index < 0) index += static_cast<int>(buffer.size());
        const float x = buffer[static_cast<size_t>(index)];
        if (tap.mode == 1 && fraction > 0.00001f)
        {
            const float y = tap.coefficient * (x - tap.y1) + tap.x1;
            tap.x1 = x; tap.y1 = y; return y;
        }
        if (--index < 0) index += static_cast<int>(buffer.size());
        return x + fraction * (buffer[static_cast<size_t>(index)] - x);
    }
    std::vector<float> buffer;
    Tap current, previous;
    juce::SmoothedValue<float> ramp;
    int writePosition = 0, fadeLength = 960, fadeRemaining = 0;
    float targetDelay = 1.0f;
    float targetCoefficient = 0.0f;
};
