/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class KarplusStrong_PluckedStringAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    KarplusStrong_PluckedStringAudioProcessorEditor (KarplusStrong_PluckedStringAudioProcessor&);
    ~KarplusStrong_PluckedStringAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    KarplusStrong_PluckedStringAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KarplusStrong_PluckedStringAudioProcessorEditor)
};
