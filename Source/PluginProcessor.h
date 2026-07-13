/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

    Main audio processor for the polyphonic extended Karplus-Strong plucked-string synthesiser.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluckedStringSynth.h"
#include "MidiPatternGenerator.h"

//==============================================================================
/**
*/
class KarplusStrong_PluckedStringAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    KarplusStrong_PluckedStringAudioProcessor();
    ~KarplusStrong_PluckedStringAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;


private:
    //==============================================================================
    // Parameter system
    //
    // APVTS provides host automation, parameter storage and preset/state saving.
    juce::AudioProcessorValueTreeState apvts;

    // Core string parameters
    std::atomic<float>* decayParam = nullptr;
    std::atomic<float>* dampingParam = nullptr;
    std::atomic<float>* brightnessParam = nullptr;
    std::atomic<float>* pickPositionParam = nullptr;

    // Tone and stereo parameters
    std::atomic<float>* bodyResonanceParam = nullptr;
    std::atomic<float>* stereoWidthParam = nullptr;
    std::atomic<float>* detuneParam = nullptr;
    std::atomic<float>* outputGainParam = nullptr;

    // Internal PatternGenerator parameters
    std::atomic<float>* patternOnParam = nullptr;
    std::atomic<float>* patternRateParam = nullptr;
    std::atomic<float>* patternTypeParam = nullptr;
    std::atomic<float>* patternVelocityParam = nullptr;

    // Tracks arpeggiator on/off changes so that timing can be reset cleanly.
    bool previousPatternOn = false;

    //==============================================================================
    // Sound generation
    juce::Synthesiser synth;
    MidiPatternGenerator midiPatternGenerator;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KarplusStrong_PluckedStringAudioProcessor)
};
