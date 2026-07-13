/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

    Main processor implementation for the polyphonic extended Karplus-Strong
    plucked-string synthesiser.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
KarplusStrong_PluckedStringAudioProcessor::KarplusStrong_PluckedStringAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
#endif
    apvts(*this, nullptr, "MyTreeState",
        {
            // ===== Karplus-Strong string basic parameters =====
            std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID("decay", 1),
                "Decay",
                0.0f,
                1.0f,
                0.85f),

            std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID("damping", 1),
                "Damping (Bright to Mellow)",
                0.0f,
                1.0f,
                0.4f),

            std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID("brightness", 1),
                "Brightness (Soft to Sharp)",
                0.0f,
                1.0f,
                0.9f),

            std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID("pickPosition", 1),
                "Pick Position (Bridge to Centre)",
                juce::NormalisableRange<float>(0.05f, 0.50f, 0.001f),
                0.20f),

            //==================================================================
            // Body, stereo and output parameters

            std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID("bodyResonance", 1),
                "Body Resonance",
                0.0f,
                1.0f,
                0.3f),

            std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID("stereoWidth", 1),
                "Stereo Width",
                0.0f,
                1.0f,
                0.6f),

            std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID("detune", 1),
                "Detune (Cents)",
                juce::NormalisableRange<float>(0.0f, 20.0f, 0.1f),
                5.0f),

            std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID("outputGain", 1),
                "Output Level (dB)",
                -24.0f,
                0.0f,
                -8.0f),

            //==================================================================
            // Internal Midi pattern generator parameters
            std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID("patternOn", 1),
                "Pattern Generator On/Off",
                false),

            std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID("patternRate", 1),
                "Pattern Speed (Slow to Fast)",
                0.5f,
                16.0f,
                4.0f),

            std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID("patternType", 1),
                "MIDI Pattern",
                juce::StringArray {
                    "Major",
                    "Minor",
                    "Open Fifths",
                    "Cluster",
                    "Bass Octave",
                    "Wide Harp Sweep",
                    "Suspended Pedal",
                    "Chromatic Tension"
                },
                0),

            std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID("patternVelocity", 1),
                "Pattern Strength",
                0.0f,
                1.0f,
                0.8f)
        })
{
    // Store raw parameter pointers for real-time-safe parameter reads in
    // processBlock().
    decayParam = apvts.getRawParameterValue("decay");
    dampingParam = apvts.getRawParameterValue("damping");
    brightnessParam = apvts.getRawParameterValue("brightness");

    pickPositionParam = apvts.getRawParameterValue("pickPosition");
    bodyResonanceParam = apvts.getRawParameterValue("bodyResonance");

    stereoWidthParam = apvts.getRawParameterValue("stereoWidth");
    detuneParam = apvts.getRawParameterValue("detune");

    outputGainParam = apvts.getRawParameterValue("outputGain");

    patternOnParam = apvts.getRawParameterValue("patternOn");
    patternRateParam = apvts.getRawParameterValue("patternRate");
    patternTypeParam = apvts.getRawParameterValue("patternType");
    patternVelocityParam = apvts.getRawParameterValue("patternVelocity");


    // Build the polyphonic synthesiser.
    synth.clearVoices();

    for (int i = 0; i < 8; ++i)
        synth.addVoice(new PluckedStringSynthVoice());

    synth.clearSounds();
    synth.addSound(new PluckedStringSynthSound());

}

KarplusStrong_PluckedStringAudioProcessor::~KarplusStrong_PluckedStringAudioProcessor()
{
}


//==============================================================================
void KarplusStrong_PluckedStringAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{

    juce::ignoreUnused(samplesPerBlock);

    synth.setCurrentPlaybackSampleRate(sampleRate);

    midiPatternGenerator.prepare(sampleRate);

    // Prepare all custom voices. Each voice owns its own string models and body
    // resonator, so each one must receive the current sample rate.
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<PluckedStringSynthVoice*>(synth.getVoice(i)))
        {
            voice->prepare(sampleRate);
        }
    }
}

void KarplusStrong_PluckedStringAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that do not have corresponding input channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    buffer.clear();

    //====================== Read APVTS parameters =============================
    // ===== Read sound parameters =====
    const float decay = decayParam->load();
    const float damping = dampingParam->load();
    const float brightness = brightnessParam->load();

    const float pickPosition = pickPositionParam->load();
    const float bodyResonance = bodyResonanceParam->load();

    const float stereoWidth = stereoWidthParam->load();
    const float detuneCents = detuneParam->load();

    const float outputGainDb = outputGainParam->load();

    // ===== Read MIDI pattern generator parameters =====
    const bool patternOn = patternOnParam->load() > 0.5f;
    const float patternRate = patternRateParam->load();
    const int patternType = static_cast<int>(patternTypeParam->load());
    const float patternVelocity = patternVelocityParam->load();

    // ========== Update all synth voices with the latest parameter values ==========
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<PluckedStringSynthVoice*>(synth.getVoice(i)))
        {
            voice->setStringParameters(decay,
                damping,
                brightness,
                pickPosition,
                bodyResonance,
                stereoWidth,
                detuneCents);
        }
    }

    //======================================================================
    // Handle MIDI pattern generator on/off transitions.
    //
    // When the pattern generator is switched off, all internally generated notes are
    // released to avoid stuck notes.
    if (previousPatternOn && !patternOn)
    {
        midiPatternGenerator.reset();
        synth.allNotesOff(1, true);
    }

    previousPatternOn = patternOn;

    midiPatternGenerator.setEnabled(patternOn);
    midiPatternGenerator.setRateHz(patternRate);
    midiPatternGenerator.setPattern(patternType);
    midiPatternGenerator.setVelocity(patternVelocity);


    //======================================================================
    // Select the MIDI source.
    juce::MidiBuffer processedMidi;

    if (patternOn)
    {
        // Pattern mode:
        // The plug-in generates its own MIDI note pattern internally.
        midiPatternGenerator.process(processedMidi, buffer.getNumSamples());
    }
    else
    {
        // Manual MIDI mode:
        // The plug-in responds to external MIDI input from the host.
        processedMidi.addEvents(midiMessages,
            0,
            buffer.getNumSamples(),
            0);
    }

    //======================================================================
    // Render the polyphonic synthesiser.
    synth.renderNextBlock(buffer,
        processedMidi,
        0,
        buffer.getNumSamples());

    // Apply final output gain after all voices have been summed.
    const float outputGainLinear =
        juce::Decibels::decibelsToGain(outputGainDb);

    buffer.applyGain(outputGainLinear);
}


//==============================================================================
const juce::String KarplusStrong_PluckedStringAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool KarplusStrong_PluckedStringAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool KarplusStrong_PluckedStringAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool KarplusStrong_PluckedStringAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double KarplusStrong_PluckedStringAudioProcessor::getTailLengthSeconds() const
{
    return 2.0;
}

int KarplusStrong_PluckedStringAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int KarplusStrong_PluckedStringAudioProcessor::getCurrentProgram()
{
    return 0;
}

void KarplusStrong_PluckedStringAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String KarplusStrong_PluckedStringAudioProcessor::getProgramName (int index)
{
    return {};
}

void KarplusStrong_PluckedStringAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================

void KarplusStrong_PluckedStringAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool KarplusStrong_PluckedStringAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif


//==============================================================================
bool KarplusStrong_PluckedStringAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* KarplusStrong_PluckedStringAudioProcessor::createEditor()
{
 //    return new KarplusStrong_PluckedStringAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void KarplusStrong_PluckedStringAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.

    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);

}

void KarplusStrong_PluckedStringAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.

    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
    {
        if (xmlState->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        }
    }

}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KarplusStrong_PluckedStringAudioProcessor();
}
