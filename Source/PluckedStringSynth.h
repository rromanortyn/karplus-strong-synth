#pragma once
#include "KarplusStrongModel.h"
#include "BodyResonator.h"
#include <array>
#include <cstdint>

class PluckedStringSynthVoice
{
public:
    void prepare(double rate)
    {
        sampleRate = rate;
        fadeLength = juce::jmax(1, static_cast<int>(rate * 0.005));
        releaseMultiplier = static_cast<float>(std::exp(-6.907755 / (rate * 0.2)));
        for (auto& bank : banks)
        {
            bank.left.prepare(rate); bank.right.prepare(rate); bank.body.prepare(rate);
            bank.active = false;
        }
        fadeRemaining = 0;
    }
    void setParameters(const StringParameters& p) noexcept
    {
        const bool retune = parameters.detune != p.detune;
        parameters = p;
        for (auto& bank : banks)
        {
            bank.left.setParameters(p); bank.right.setParameters(p); bank.body.setAmount(p.body);
            if (retune && bank.active) tune(bank, bank.bend, false);
        }
    }
    void start(int note, int channel, float velocity, float bend, uint64_t age) noexcept
    {
        const bool stealing = isActive();
        const int next = 1 - current;
        if (stealing) { banks[current].releasing = true; fadeRemaining = fadeLength; }
        else fadeRemaining = 0;
        current = next;
        auto& bank = banks[current];
        bank.note = note; bank.channel = channel; bank.age = age;
        bank.velocity = velocity; bank.active = true; bank.releasing = false; bank.keyDown = true;
        bank.level = stealing ? 1.0f : 0.0f;
        bank.body.reset();
        tune(bank, bend, true);
    }
    void noteOff(int note, int channel, bool sustain) noexcept
    {
        for (auto& bank : banks)
            if (bank.active && bank.note == note && bank.channel == channel)
            {
                bank.keyDown = false;
                if (!sustain) bank.releasing = true;
            }
    }
    void releaseSustain(int channel) noexcept
    {
        for (auto& bank : banks)
            if (bank.channel == channel && !bank.keyDown) bank.releasing = true;
    }
    void allNotesOff(int channel, bool immediately) noexcept
    {
        for (auto& bank : banks)
            if (channel == 0 || bank.channel == channel)
            {
                bank.keyDown = false; bank.releasing = true;
                if (immediately) bank.active = false;
            }
    }
    void pitchBend(int channel, float bend) noexcept
    {
        for (auto& bank : banks)
            if (bank.active && bank.channel == channel) tune(bank, bend, false);
    }
    void render(float& left, float& right) noexcept
    {
        float l = 0.0f, r = 0.0f;
        renderBank(banks[current], l, r);
        if (fadeRemaining > 0)
        {
            float oldL = 0.0f, oldR = 0.0f;
            renderBank(banks[1 - current], oldL, oldR);
            const float mix = 1.0f - static_cast<float>(fadeRemaining--) / static_cast<float>(fadeLength);
            l = oldL * (1.0f - mix) + l * mix;
            r = oldR * (1.0f - mix) + r * mix;
            if (fadeRemaining == 0) banks[1 - current].active = false;
        }
        left += l * 0.25f; right += r * 0.25f;
    }
    bool isActive() const noexcept { return banks[current].active || fadeRemaining > 0; }
    uint64_t age() const noexcept { return banks[current].age; }
    void copySnapshot(float* data, int count) const noexcept { banks[current].left.copySnapshot(data, count); }
private:
    struct Bank
    {
        KarplusStrongString left, right;
        BodyResonator body;
        bool active = false, releasing = false, keyDown = false;
        int note = 60, channel = 1;
        uint64_t age = 0;
        float velocity = 0.0f, bend = 0.0f, level = 0.0f;
    };
    void tune(Bank& bank, float bend, bool pluck) noexcept
    {
        bank.bend = bend;
        const float hz = 440.0f * std::pow(2.0f, (static_cast<float>(bank.note) - 69.0f + bend) / 12.0f);
        const float spread = std::pow(2.0f, parameters.detune / 2400.0f);
        if (pluck)
        {
            bank.left.pluck(hz / spread, bank.velocity);
            bank.right.pluck(hz * spread, bank.velocity);
        }
        else { bank.left.tune(hz / spread, false); bank.right.tune(hz * spread, false); }
    }
    void renderBank(Bank& bank, float& l, float& r) noexcept
    {
        if (!bank.active) return;
        const float a = bank.left.process(), b = bank.right.process();
        const float centre = bank.body.process(0.5f * (a + b));
        const float side = (a - b) * 0.5f * parameters.stereoWidth;
        if (bank.releasing) bank.level *= releaseMultiplier;
        else bank.level = juce::jmin(1.0f, bank.level + static_cast<float>(1.0 / (sampleRate * 0.001)));
        l = (centre + side) * bank.level; r = (centre - side) * bank.level;
        if (bank.level < 1.0e-7f || (!bank.left.isActive() && !bank.right.isActive())) bank.active = false;
    }
    std::array<Bank, 2> banks;
    StringParameters parameters;
    double sampleRate = 48000.0;
    float releaseMultiplier = 0.999f;
    int current = 0, fadeLength = 240, fadeRemaining = 0;
};

// Fixed voice pool and raw short-MIDI dispatch: no Synthesiser critical section,
// temporary MIDI buffers, heap allocations, or container resizing in render().
class VoiceManager
{
public:
    static constexpr int maxVoices = 16;
    void prepare(double rate)
    {
        for (auto& voice : voices) voice.prepare(rate);
        bends.fill(0.0f); sustain.fill(false); counter = 0;
    }
    void setParameters(const StringParameters& p) noexcept { for (auto& v : voices) v.setParameters(p); }
    void noteOn(int note, int channel, float velocity) noexcept
    {
        if (velocity <= 0.0f) { noteOff(note, channel); return; }
        auto* chosen = &voices.front();
        for (auto& voice : voices)
        {
            if (!voice.isActive()) { chosen = &voice; break; }
            if (voice.age() < chosen->age()) chosen = &voice;
        }
        chosen->start(note, channel, velocity, bends[static_cast<size_t>(channel - 1)], ++counter);
    }
    void noteOff(int note, int channel) noexcept
    {
        for (auto& v : voices) v.noteOff(note, channel, sustain[static_cast<size_t>(channel - 1)]);
    }
    void allNotesOff(int channel = 0, bool immediately = false) noexcept
    {
        for (auto& v : voices) v.allNotesOff(channel, immediately);
    }
    void handleMidi(const uint8_t* bytes, int size) noexcept
    {
        if (size < 1 || bytes[0] < 0x80 || bytes[0] >= 0xf0) return;
        const int status = bytes[0] & 0xf0, channel = (bytes[0] & 0x0f) + 1;
        if (size < 3) return;
        const int a = bytes[1] & 0x7f, b = bytes[2] & 0x7f;
        if (status == 0x90) noteOn(a, channel, static_cast<float>(b) / 127.0f);
        else if (status == 0x80) noteOff(a, channel);
        else if (status == 0xe0)
        {
            const int wheel = a + (b << 7);
            const float bend = wheel >= 8192 ? (wheel - 8192) * (2.0f / 8191.0f)
                                             : (wheel - 8192) * (2.0f / 8192.0f);
            bends[static_cast<size_t>(channel - 1)] = bend;
            for (auto& v : voices) v.pitchBend(channel, bend);
        }
        else if (status == 0xb0)
        {
            if (a == 64)
            {
                sustain[static_cast<size_t>(channel - 1)] = b >= 64;
                if (b < 64) for (auto& v : voices) v.releaseSustain(channel);
            }
            else if (a == 120 || a == 123) allNotesOff(channel, a == 120);
        }
    }
    void render(juce::AudioBuffer<float>& buffer, int start, int count) noexcept
    {
        auto* left = buffer.getWritePointer(0);
        auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
        for (int i = start; i < start + count; ++i)
        {
            float l = 0.0f, r = 0.0f;
            for (auto& voice : voices) if (voice.isActive()) voice.render(l, r);
            left[i] = right != nullptr ? l : 0.5f * (l + r);
            if (right != nullptr) right[i] = r;
        }
    }
    int activeVoices() const noexcept
    {
        int count = 0; for (const auto& v : voices) if (v.isActive()) ++count; return count;
    }
    void copySnapshot(float* data, int count) const noexcept
    {
        for (const auto& v : voices) if (v.isActive()) { v.copySnapshot(data, count); return; }
        std::fill(data, data + count, 0.0f);
    }
private:
    std::array<PluckedStringSynthVoice, maxVoices> voices;
    std::array<float, 16> bends {};
    std::array<bool, 16> sustain {};
    uint64_t counter = 0;
};
