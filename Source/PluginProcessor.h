#pragma once
#include <JuceHeader.h>
#include "PluckedStringSynth.h"
#include "MidiPatternGenerator.h"
#include "ScopeFifo.h"

class KarplusStrong_PluckedStringAudioProcessor : public juce::AudioProcessor
{
public:
    KarplusStrong_PluckedStringAudioProcessor();
    ~KarplusStrong_PluckedStringAudioProcessor() override = default;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState& state() noexcept { return apvts; }
    ScopeFifo& scope() noexcept { return scopeFifo; }
    ScopeFifo& delayScope() noexcept { return delayFifo; }
    void setVisualisationEnabled(bool enabled) noexcept { visualisationEnabled.store(enabled); }
    float takePeak() noexcept { return peak.exchange(0.0f); }
    int activeVoiceCount() const noexcept { return displayedVoices.load(); }

private:
    enum Parameter { decay, damping, brightness, pickPosition, bodyResonance, stereoWidth,
                     detune, outputGain, patternOn, patternRate, patternType, patternVelocity,
                     exciteAmount, exciterType, impulseWidth, interpolation, pitchTransition, mono, count };
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float read(Parameter index) const noexcept;
    juce::AudioProcessorValueTreeState apvts;
    std::array<std::atomic<float>*, count> values {};
    VoiceManager voices;
    MidiPatternGenerator pattern;
    juce::SmoothedValue<float> gain, monoMix;
    ScopeFifo scopeFifo, delayFifo;
    std::atomic<bool> visualisationEnabled { false };
    std::atomic<float> peak { 0.0f };
    std::atomic<int> displayedVoices { 0 };
    bool wasPatternOn = false;
    bool prepared = false;
    float highPassR = 0.997f;
    std::array<float, 2> previousInput {}, previousOutput {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KarplusStrong_PluckedStringAudioProcessor)
};
