#include "PluginEditor.h"

void KarplusStrong_PluckedStringAudioProcessorEditor::setupControl(Control& control, juce::Component& parent,
    const char* id, const char* caption, const char* tooltip)
{
    auto* parameter = processor.state().getParameter(id);
    control.slider.setLookAndFeel(&style);
    control.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    control.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, 24);
    control.slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                                       juce::MathConstants<float>::pi * 2.8f, true);
    control.slider.setDoubleClickReturnValue(true, parameter->convertFrom0to1(parameter->getDefaultValue()));
    control.slider.setTooltip(tooltip); control.slider.setTitle(caption); control.slider.setName(id);
    control.slider.setNumDecimalPlacesToDisplay(2);
    if (juce::String(id) == "outputGain") control.slider.setTextValueSuffix(" dB");
    control.label.setText(caption, juce::dontSendNotification);
    control.label.setFont(juce::FontOptions("Inter", 13.0f, juce::Font::bold));
    control.label.setJustificationType(juce::Justification::centred);
    parent.addAndMakeVisible(control.slider); parent.addAndMakeVisible(control.label);
    control.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.state(), id, control.slider);
}

KarplusStrong_PluckedStringAudioProcessorEditor::KarplusStrong_PluckedStringAudioProcessorEditor(
    KarplusStrong_PluckedStringAudioProcessor& p) : AudioProcessorEditor(p), processor(p), scope(p)
{
    setLookAndFeel(&style);
    setResizable(true, true); setResizeLimits(600, 400, 1200, 800);
    addAndMakeVisible(canvas); canvas.addAndMakeVisible(scope);
    setupControl(mainControls[0], canvas, "damping", "DAMPING", "Loop-filter alpha: 0.05 is mellow; 1.00 is bright. Double-click to reset.");
    setupControl(mainControls[1], canvas, "exciteAmount", "EXCITE", "Initial string excitation, multiplied by MIDI velocity.");
    setupControl(mainControls[2], canvas, "outputGain", "OUTPUT", "Output gain: -60 to +12 dB, smoothed over 50 ms.");
    constexpr std::array<const char*, 3> names {{ "Noise", "Filtered Noise", "Impulse" }};
    for (size_t i = 0; i < exciterButtons.size(); ++i)
    {
        auto& button = exciterButtons[i];
        button.setButtonText(names[i]); button.setClickingTogglesState(true); button.setRadioGroupId(110);
        button.setName("exciter" + juce::String(static_cast<int>(i)));
        button.onClick = [this, i] { exciterAttachment->setValueAsCompleteGesture(static_cast<float>(i)); };
        canvas.addAndMakeVisible(button);
    }
    exciterAttachment = std::make_unique<juce::ParameterAttachment>(*processor.state().getParameter("exciterType"),
        [this](float value)
        {
            for (size_t i = 0; i < exciterButtons.size(); ++i)
                exciterButtons[i].setToggleState(static_cast<int>(value) == static_cast<int>(i), juce::dontSendNotification);
        }, nullptr);
    exciterAttachment->sendInitialUpdate();
    canvas.addAndMakeVisible(advancedButton); advancedButton.setClickingTogglesState(true);
    advancedButton.setName("advanced");
    advancedButton.setTooltip("Additional model, pitch, stereo and pattern controls");
    advancedButton.onClick = [this] { showAdvanced(advancedButton.getToggleState()); };
    canvas.addAndMakeVisible(monoButton); monoButton.setClickingTogglesState(true);
    monoButton.setTooltip("Sum both output channels to mono");
    monoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.state(), "mono", monoButton);
    canvas.addChildComponent(advancedViewport);
    advancedViewport.setName("advanced-scroll");
    advancedViewport.setViewedComponent(&advancedContent, false);
    advancedViewport.setScrollBarsShown(true, false);
    constexpr std::array<const char*, 10> ids {{ "decay", "impulseWidth", "brightness", "pickPosition", "bodyResonance",
                                               "stereoWidth", "detune", "patternRate", "patternVelocity", "exciteAmount" }};
    constexpr std::array<const char*, 10> labels {{ "DECAY", "IMPULSE WIDTH", "BRIGHTNESS", "PICK POSITION", "BODY",
                                                   "STEREO WIDTH", "DETUNE", "PATTERN RATE", "STRENGTH", "EXCITE" }};
    constexpr std::array<const char*, 10> tips {{ "Feedback decay time", "Triangle peak position; active for Impulse excitation",
        "Optional excitation brightness", "Optional bridge-to-centre comb shaping", "Optional instrument body colour",
        "Stereo spread between the strings", "Double-string detuning in cents", "Pattern notes per second",
        "Pattern MIDI velocity", "Initial excitation intensity" }};
    for (size_t i = 0; i < advancedControls.size(); ++i)
        setupControl(advancedControls[i], advancedContent, ids[i], labels[i], tips[i]);
    interpolationBox.addItemList({"Linear", "Allpass"}, 1);
    pitchBox.addItemList({"Glide (20 ms)", "Crossfade (20 ms)"}, 1);
    patternBox.addItemList({"Major", "Minor", "Open Fifths", "Cluster", "Bass Octave", "Wide Harp Sweep", "Suspended Pedal", "Chromatic Tension"}, 1);
    std::array<juce::ComboBox*, 3> boxes {{ &interpolationBox, &pitchBox, &patternBox }};
    const std::array<const char*, 3> captions {{ "INTERPOLATION", "PITCH TRANSITION", "MIDI PATTERN" }};
    for (size_t i = 0; i < boxes.size(); ++i)
    {
        advancedContent.addAndMakeVisible(*boxes[i]); advancedContent.addAndMakeVisible(comboLabels[i]);
        comboLabels[i].setText(captions[i], juce::dontSendNotification);
        comboLabels[i].setFont(juce::FontOptions("Inter", 11.0f, juce::Font::plain));
    }
    interpolationAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.state(), "interpolation", interpolationBox);
    pitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.state(), "pitchTransition", pitchBox);
    patternChoiceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.state(), "patternType", patternBox);
    advancedContent.addAndMakeVisible(patternButton); patternButton.setClickingTogglesState(true);
    patternButton.setTooltip("Generate MIDI internally instead of using the host's MIDI input");
    patternButtonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.state(), "patternOn", patternButton);
    viewBox.addItemList({"Waveform", "Spectrum", "Delay line"}, 1); viewBox.setSelectedId(1);
    viewBox.setName("visualisation");
    canvas.addChildComponent(viewBox); viewBox.onChange = [this] { scope.setMode(viewBox.getSelectedItemIndex()); };
    setSize(700, 500);
    processor.setVisualisationEnabled(true); startTimerHz(30);
}

KarplusStrong_PluckedStringAudioProcessorEditor::~KarplusStrong_PluckedStringAudioProcessorEditor()
{
    stopTimer(); processor.setVisualisationEnabled(false);
    advancedViewport.setViewedComponent(nullptr, false);
    setLookAndFeel(nullptr);
}
void KarplusStrong_PluckedStringAudioProcessorEditor::showAdvanced(bool show)
{
    advanced = show; advancedViewport.setVisible(show); viewBox.setVisible(show);
    for (auto& c : mainControls) { c.slider.setVisible(!show); c.label.setVisible(!show); }
    for (auto& b : exciterButtons) b.setVisible(!show);
    advancedButton.setButtonText(show ? "MAIN" : "ADVANCED"); repaint();
}
void KarplusStrong_PluckedStringAudioProcessorEditor::layoutDesign()
{
    scope.setBounds(18, 48, 664, 170);
    for (size_t i = 0; i < mainControls.size(); ++i)
    {
        const int x = 45 + static_cast<int>(i) * 160;
        mainControls[i].label.setBounds(x, 240, 125, 28);
        mainControls[i].slider.setBounds(x, 272, 125, 148);
    }
    for (size_t i = 0; i < exciterButtons.size(); ++i)
        exciterButtons[i].setBounds(515, 280 + static_cast<int>(i) * 35, 125, 29);
    monoButton.setBounds(475, 450, 72, 25); advancedButton.setBounds(557, 450, 110, 25);
    viewBox.setBounds(510, 20, 157, 24);
    advancedViewport.setBounds(18, 233, 664, 203);
    advancedContent.setSize(642, 345);
    for (size_t i = 0; i < advancedControls.size(); ++i)
    {
        const int x = 5 + static_cast<int>(i % 5) * 127, y = static_cast<int>(i / 5) * 125;
        advancedControls[i].label.setBounds(x, y, 124, 20);
        advancedControls[i].label.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::bold));
        advancedControls[i].slider.setBounds(x + 12, y + 20, 100, 104);
    }
    std::array<juce::ComboBox*, 3> boxes {{ &interpolationBox, &pitchBox, &patternBox }};
    for (size_t i = 0; i < boxes.size(); ++i)
    {
        const int x = 10 + static_cast<int>(i) * 210;
        comboLabels[i].setBounds(x, 258, 195, 20); boxes[i]->setBounds(x, 280, 195, 28);
    }
    patternButton.setBounds(430, 315, 195, 25);
}
void KarplusStrong_PluckedStringAudioProcessorEditor::resized()
{
    canvas.setBounds(0, 0, 700, 500);
    canvas.setTransform(juce::AffineTransform::scale(getWidth() / 700.0f, getHeight() / 500.0f));
    layoutDesign();
}
void KarplusStrong_PluckedStringAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(KSLookAndFeel::background);
    juce::Graphics::ScopedSaveState saved(g);
    g.addTransform(juce::AffineTransform::scale(getWidth() / 700.0f, getHeight() / 500.0f));
    g.setColour(KSLookAndFeel::muted); g.setFont(juce::FontOptions("Inter", 15.0f, juce::Font::bold));
    g.drawText("KARPLUS-STRONG VIRTUAL STRING SYNTH", 18, 9, advanced ? 480 : 664, 28, juce::Justification::centred);
    if (!advanced)
    {
        g.setFont(juce::FontOptions("Inter", 13.0f, juce::Font::bold));
        g.drawText("EXCITATION", 515, 240, 125, 28, juce::Justification::centred);
    }
    g.setFont(juce::FontOptions("Inter", 10.0f, juce::Font::plain));
    g.setColour(KSLookAndFeel::muted.withAlpha(0.6f));
    g.drawText("PEAK", 22, 450, 45, 25, juce::Justification::left);
    const float db = juce::Decibels::gainToDecibels(displayedPeak, -60.0f);
    for (int i = 0; i < 24; ++i)
    {
        g.setColour(db >= -60.0f + (i + 1) * 2.5f ? (i >= 21 ? juce::Colour(0xffff3535) : KSLookAndFeel::accent) : KSLookAndFeel::border);
        g.fillRect(72 + i * 13, 458, 11, 11);
    }
    g.setColour(KSLookAndFeel::muted);
    g.drawText(db < -59.9f ? "-inf" : juce::String(db, 1) + " dB", 388, 450, 80, 25, juce::Justification::centred);
}
void KarplusStrong_PluckedStringAudioProcessorEditor::timerCallback()
{
    scope.update(); displayedPeak = juce::jmax(processor.takePeak(), displayedPeak * 0.85f);
    advancedControls[1].slider.setEnabled(processor.state().getRawParameterValue("exciterType")->load() > 1.5f);
    repaint();
}
