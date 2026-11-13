#pragma once
#include "PluginProcessor.h"
#include "LookAndFeel.h"
#include <juce_dsp/juce_dsp.h>

class OscilloscopeView : public juce::Component
{
public:
    explicit OscilloscopeView(KarplusStrong_PluckedStringAudioProcessor& p) : processor(p)
    { setAccessible(true); setTitle("Live signal visualisation"); }
    void setMode(int value) { mode = value; repaint(); }
    void update()
    {
        std::array<float, 2048> incoming {};
        int count;
        bool changed = false;
        while ((count = processor.scope().read(incoming.data(), 2048)) > 0)
        {
            for (int i = 0; i < count; ++i) { history[static_cast<size_t>(position)] = incoming[static_cast<size_t>(i)]; position = (position + 1) % 2048; }
            changed = true;
        }
        while ((count = processor.delayScope().read(incoming.data(), 256)) > 0)
            std::copy_n(incoming.begin(), count, delay.begin());
        if (changed && mode == 1)
        {
            spectrum.fill(0.0f);
            for (int i = 0; i < 2048; ++i)
                spectrum[static_cast<size_t>(i)] = history[static_cast<size_t>((position + i) % 2048)]
                    * (0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / 2047.0f));
            fft.performFrequencyOnlyForwardTransform(spectrum.data());
        }
        repaint();
    }
    void paint(juce::Graphics& g) override
    {
        using Style = KSLookAndFeel;
        auto bounds = getLocalBounds().toFloat().reduced(1);
        g.setColour(Style::panel); g.fillRoundedRectangle(bounds, 3);
        g.setColour(Style::border); g.drawRoundedRectangle(bounds, 3, 1.0f);
        g.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::plain));
        g.setColour(Style::muted.withAlpha(0.65f));
        g.drawText(mode == 1 ? "SPECTRUM  /  20 Hz - 20 kHz" : mode == 2 ? "DELAY LINE  /  ONE PERIOD" : "OSC  /  AUTO",
                   18, 5, getWidth() - 36, 16, juce::Justification::left);
        auto plot = bounds.reduced(10, 22);
        const float middle = plot.getCentreY();
        g.setColour(Style::border.withAlpha(0.5f));
        g.drawHorizontalLine(static_cast<int>(middle), plot.getX(), plot.getRight());
        juce::Path path;
        float amplitude = 0.02f;
        if (mode == 0) for (const float sample : history) amplitude = juce::jmax(amplitude, std::abs(sample));
        else if (mode == 2) for (const float sample : delay) amplitude = juce::jmax(amplitude, std::abs(sample));
        // Trigger at a rising crossing so a held note remains visually stable.
        int start = position;
        if (mode == 0)
            for (int i = 0; i < 1024; ++i)
            {
                const int index = (position + i) % 2048;
                if (history[static_cast<size_t>(index)] <= 0 && history[static_cast<size_t>((index + 1) % 2048)] > 0)
                { start = index; break; }
            }
        for (int x = 0; x < static_cast<int>(plot.getWidth()); ++x)
        {
            const float fraction = x / juce::jmax(1.0f, plot.getWidth() - 1.0f);
            float y;
            if (mode == 1)
            {
                const float hz = 20.0f * std::pow(1000.0f, fraction);
                const int bin = juce::jlimit(1, 1023, static_cast<int>(hz * 2048.0 / juce::jmax(8000.0, processor.getSampleRate())));
                const float db = juce::Decibels::gainToDecibels(spectrum[static_cast<size_t>(bin)] / 512.0f, -80.0f);
                y = plot.getBottom() - juce::jlimit(0.0f, 1.0f, (db + 80.0f) / 80.0f) * plot.getHeight();
            }
            else
            {
                const float sample = mode == 2 ? delay[static_cast<size_t>(juce::jlimit(0, 255, static_cast<int>(fraction * 255)))]
                    : history[static_cast<size_t>((start + static_cast<int>(fraction * 1023)) % 2048)];
                y = middle - juce::jlimit(-1.0f, 1.0f, sample / amplitude) * plot.getHeight() * 0.45f;
            }
            if (x == 0) path.startNewSubPath(plot.getX(), y);
            else path.lineTo(plot.getX() + x, y);
        }
        auto fill = path;
        fill.lineTo(plot.getRight(), mode == 1 ? plot.getBottom() : middle);
        fill.lineTo(plot.getX(), mode == 1 ? plot.getBottom() : middle); fill.closeSubPath();
        g.setColour(Style::accent.withAlpha(0.12f)); g.fillPath(fill);
        g.setColour(Style::accent); g.strokePath(path, juce::PathStrokeType(1.5f));
    }
private:
    KarplusStrong_PluckedStringAudioProcessor& processor;
    std::array<float, 2048> history {};
    std::array<float, 256> delay {};
    std::array<float, 4096> spectrum {};
    juce::dsp::FFT fft { 11 };
    int position = 0, mode = 0;
};
