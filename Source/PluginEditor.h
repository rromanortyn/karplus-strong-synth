#pragma once
#include "PluginProcessor.h"
#include "OscilloscopeView.h"
#include "LookAndFeel.h"

class KarplusStrong_PluckedStringAudioProcessorEditor : public juce::AudioProcessorEditor,
                                                       private juce::Timer
{
public:
    explicit KarplusStrong_PluckedStringAudioProcessorEditor(KarplusStrong_PluckedStringAudioProcessor&);
    ~KarplusStrong_PluckedStringAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    struct Control
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    void setupControl(Control&, juce::Component&, const char* id, const char* label, const char* tooltip);
    void timerCallback() override;
    void showAdvanced(bool);
    void layoutDesign();
    KarplusStrong_PluckedStringAudioProcessor& processor;
    KSLookAndFeel style;
    juce::Component canvas, advancedContent;
    OscilloscopeView scope;
    std::array<Control, 3> mainControls;
    std::array<Control, 10> advancedControls;
    std::array<juce::TextButton, 3> exciterButtons;
    juce::TextButton advancedButton { "ADVANCED" }, monoButton { "MONO" }, patternButton { "PATTERN ON" };
    juce::ComboBox interpolationBox, pitchBox, patternBox, viewBox;
    std::array<juce::Label, 3> comboLabels;
    juce::Viewport advancedViewport;
    std::unique_ptr<juce::ParameterAttachment> exciterAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> monoAttachment, patternButtonAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> interpolationAttachment, pitchAttachment, patternChoiceAttachment;
    juce::TooltipWindow tooltips { this, 600 };
    bool advanced = false;
    float displayedPeak = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KarplusStrong_PluckedStringAudioProcessorEditor)
};
