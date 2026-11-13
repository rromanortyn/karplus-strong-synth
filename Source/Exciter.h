#pragma once
#include <JuceHeader.h>
#include <cmath>
class Exciter
{
public:
    void fill(float* destination, int length, int type, float width, float brightness, float amplitude) noexcept
    {
        float filtered = 0.0f, mean = 0.0f;
        for (int i = 0; i < length; ++i)
        {
            const float phase = static_cast<float>(i) / static_cast<float>(length);
            float value;
            if (type == 2)
                value = phase < width ? -1.0f + 2.0f * phase / width
                                      : 1.0f - 2.0f * (phase - width) / (1.0f - width);
            else
            {
                const float noise = random.nextFloat() * 2.0f - 1.0f;
                filtered += 0.5f * (noise - filtered);
                value = type == 1 ? filtered : noise;
                value = brightness * value + (1.0f - brightness) * filtered;
            }
            destination[i] = value; mean += value;
        }
        // Remove excitation DC so that it cannot accumulate in the loop.
        mean /= static_cast<float>(length);
        for (int i = 0; i < length; ++i) destination[i] = (destination[i] - mean) * amplitude;
    }
private:
    juce::Random random { 0x4b535354 };
};
