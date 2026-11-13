#pragma once
#include "PluckedStringSynth.h"
#include <array>

// Optional demonstration sequencer. Pattern tables have fixed storage; generated
// events are dispatched directly to the voices at exact segment boundaries.
class MidiPatternGenerator
{
public:
    void prepare(double rate) noexcept { sampleRate = rate; reset(); }
    void reset() noexcept { untilNext = 0.0; step = 0; lastNote = -1; }
    void render(VoiceManager& voices, juce::AudioBuffer<float>& audio,
                float rate, int pattern, float velocity) noexcept
    {
        pattern = juce::jlimit(0, 7, pattern);
        if (pattern != lastPattern) { step = 0; lastPattern = pattern; }
        const double period = sampleRate / juce::jlimit(0.5f, 16.0f, rate);
        untilNext = juce::jmin(untilNext, period);
        int offset = 0;
        while (offset < audio.getNumSamples())
        {
            if (untilNext <= 0.0)
            {
                if (lastNote >= 0) voices.noteOff(lastNote, 1);
                lastNote = patterns[static_cast<size_t>(pattern)][static_cast<size_t>(step)];
                voices.noteOn(lastNote, 1, velocity);
                step = (step + 1) % lengths[static_cast<size_t>(pattern)];
                untilNext += period;
            }
            const int count = juce::jmin(audio.getNumSamples() - offset,
                                        juce::jmax(1, static_cast<int>(std::ceil(untilNext))));
            voices.render(audio, offset, count); offset += count; untilNext -= count;
        }
    }
private:
    inline static constexpr std::array<std::array<int, 8>, 8> patterns {{
        {{60,64,67,72,67,64,0,0}}, {{57,60,64,69,64,60,0,0}},
        {{48,55,60,67,72,67,0,0}}, {{60,62,65,69,74,77,0,0}},
        {{36,43,48,55,48,43,0,0}}, {{36,48,55,60,67,72,79,84}},
        {{50,57,60,65,60,57,62,69}}, {{60,61,60,58,59,60,64,63}}
    }};
    inline static constexpr std::array<int, 8> lengths {{6,6,6,6,6,8,8,8}};
    double sampleRate = 48000.0, untilNext = 0.0;
    int step = 0, lastNote = -1, lastPattern = -1;
};
