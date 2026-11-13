#pragma once
#include <JuceHeader.h>
#include <BinaryData.h>

class KSLookAndFeel : public juce::LookAndFeel_V4
{
public:
    inline static const juce::Colour background { 0xff1e1e22 }, panel { 0xff2a2a2f },
        border { 0xff3a3a40 }, accent { 0xffff7c2b }, text { 0xffe0e0e0 }, muted { 0xffb0b0b0 };
    KSLookAndFeel()
    {
        typeface = juce::Typeface::createSystemTypefaceFor(BinaryData::Inter_ttf, BinaryData::Inter_ttfSize);
        setColour(juce::Slider::rotarySliderFillColourId, accent);
        setColour(juce::Slider::rotarySliderOutlineColourId, border);
        setColour(juce::Slider::thumbColourId, text);
        setColour(juce::Slider::textBoxTextColourId, text);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Label::textColourId, muted);
        setColour(juce::ComboBox::backgroundColourId, panel);
        setColour(juce::ComboBox::textColourId, text);
        setColour(juce::ComboBox::outlineColourId, border);
        setColour(juce::PopupMenu::backgroundColourId, panel);
        setColour(juce::PopupMenu::textColourId, text);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, accent);
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        setColour(juce::TextButton::textColourOffId, muted);
    }
    juce::Typeface::Ptr getTypefaceForFont(const juce::Font&) override { return typeface; }
    juce::Label* createSliderTextBox(juce::Slider& slider) override
    {
        auto* label = juce::LookAndFeel_V4::createSliderTextBox(slider);
        label->setFont(juce::FontOptions("Inter", 16.0f, juce::Font::plain));
        label->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
        label->setColour(juce::Label::outlineWhenEditingColourId, accent);
        label->setColour(juce::TextEditor::outlineColourId, accent);
        return label;
    }
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float position,
                          float start, float end, juce::Slider&) override
    {
        const float radius = juce::jmin(w, h) * 0.5f - 7.0f;
        const float cx = x + w * 0.5f, cy = y + h * 0.5f;
        g.setColour(panel); g.fillEllipse(cx - radius, cy - radius, radius * 2, radius * 2);
        g.setColour(border); g.drawEllipse(cx - radius, cy - radius, radius * 2, radius * 2, 2.0f);
        const float angle = start + position * (end - start);
        juce::Path arc; arc.addCentredArc(cx, cy, radius - 9, radius - 9, 0, start, angle, true);
        g.setColour(accent); g.strokePath(arc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved,
                                                                 juce::PathStrokeType::rounded));
        const float length = radius - 15;
        g.setColour(text);
        g.drawLine(cx, cy, cx + std::sin(angle) * length, cy - std::cos(angle) * length, 3.0f);
    }
    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&,
                              bool hover, bool down) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(button.getToggleState() ? accent : panel.brighter(hover || down ? 0.1f : 0.0f));
        g.fillRoundedRectangle(bounds, 2.0f);
        g.setColour(border); g.drawRoundedRectangle(bounds, 2.0f, 1.0f);
    }
    void drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        g.setColour(panel); g.fillRoundedRectangle(0.5f, 0.5f, w - 1.0f, h - 1.0f, 3.0f);
        g.setColour(border); g.drawRoundedRectangle(0.5f, 0.5f, w - 1.0f, h - 1.0f, 3.0f, 1.0f);
        juce::Path arrow; arrow.startNewSubPath(w - 17.0f, h * 0.45f);
        arrow.lineTo(w - 12.0f, h * 0.65f); arrow.lineTo(w - 7.0f, h * 0.45f);
        g.setColour(muted); g.strokePath(arrow, juce::PathStrokeType(1.2f));
    }
private:
    juce::Typeface::Ptr typeface;
};
