#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout
KarplusStrong_PluckedStringAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto number = [&layout](const char* id, const char* name, float lo, float hi, float initial)
    {
        layout.add(std::make_unique<AudioParameterFloat>(ParameterID(id, 1), name,
                    NormalisableRange<float>(lo, hi), initial,
                    AudioParameterFloatAttributes().withStringFromValueFunction(
                        [](float value, int) { return String(value, 2); })));
    };
    // Keep existing parameter IDs/order so saved sessions can still find controls.
    number("decay", "Decay", 0, 1, 0.85f);
    number("damping", "Damping", 0.05f, 1, 0.6f);
    number("brightness", "Brightness (Soft to Sharp)", 0, 1, 1);
    number("pickPosition", "Pick Position (Bridge to Centre)", 0.05f, 0.5f, 0.5f);
    number("bodyResonance", "Body Resonance", 0, 1, 0);
    number("stereoWidth", "Stereo Width", 0, 1, 0);
    number("detune", "Detune (Cents)", 0, 20, 0);
    number("outputGain", "Output Level (dB)", -60, 12, 0);
    layout.add(std::make_unique<AudioParameterBool>(ParameterID("patternOn", 1), "Pattern Generator On/Off", false));
    number("patternRate", "Pattern Speed (Slow to Fast)", 0.5f, 16, 4);
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID("patternType", 1), "MIDI Pattern",
        StringArray { "Major", "Minor", "Open Fifths", "Cluster", "Bass Octave", "Wide Harp Sweep",
                      "Suspended Pedal", "Chromatic Tension" }, 0));
    number("patternVelocity", "Pattern Strength", 0, 1, 0.8f);
    number("exciteAmount", "Excite Amount", 0, 1, 0.7f);
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID("exciterType", 1), "Excitation",
        StringArray { "Noise", "Filtered Noise", "Impulse" }, 0));
    number("impulseWidth", "Impulse Width", 0.05f, 0.95f, 0.5f);
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID("interpolation", 1), "Interpolation",
        StringArray { "Linear", "Allpass" }, 0));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID("pitchTransition", 1), "Pitch Transition",
        StringArray { "Glide", "Crossfade" }, 0));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID("mono", 1), "Mono Output", false));
    return layout;
}

KarplusStrong_PluckedStringAudioProcessor::KarplusStrong_PluckedStringAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "MyTreeState", createLayout())
{
    constexpr std::array<const char*, count> ids {{ "decay", "damping", "brightness", "pickPosition",
        "bodyResonance", "stereoWidth", "detune", "outputGain", "patternOn", "patternRate",
        "patternType", "patternVelocity", "exciteAmount", "exciterType", "impulseWidth",
        "interpolation", "pitchTransition", "mono" }};
    for (size_t i = 0; i < ids.size(); ++i) values[i] = apvts.getRawParameterValue(ids[i]);
}

float KarplusStrong_PluckedStringAudioProcessor::read(Parameter index) const noexcept
{
    const float v = values[static_cast<size_t>(index)]->load(std::memory_order_relaxed);
    return std::isfinite(v) ? v : 0.0f;
}

void KarplusStrong_PluckedStringAudioProcessor::prepareToPlay(double rate, int)
{
    prepared = false;
    if (!std::isfinite(rate) || rate < 8000.0) return;
    voices.prepare(rate); pattern.prepare(rate);
    gain.reset(rate, 0.05); gain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(read(outputGain)));
    monoMix.reset(rate, 0.02); monoMix.setCurrentAndTargetValue(read(mono));
    highPassR = static_cast<float>(std::exp(-juce::MathConstants<double>::twoPi * 20.0 / rate));
    previousInput.fill(0.0f); previousOutput.fill(0.0f);
    // An editor may remain open across host re-preparation. Leave FIFO cursors
    // owned by their producer/consumer rather than resetting them under the GUI.
    peak.store(0); wasPatternOn = false;
    setLatencySamples(0); prepared = true;
}

void KarplusStrong_PluckedStringAudioProcessor::releaseResources()
{
    voices.allNotesOff(0, true); prepared = false;
}

bool KarplusStrong_PluckedStringAudioProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    return layout.getMainInputChannelSet().isDisabled()
        && (layout.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
            || layout.getMainOutputChannelSet() == juce::AudioChannelSet::stereo());
}

void KarplusStrong_PluckedStringAudioProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    audio.clear();
    if (!prepared || audio.getNumChannels() == 0 || audio.getNumSamples() == 0) return;
    StringParameters p;
    p.damping = juce::jlimit(0.05f, 1.0f, read(damping));
    p.decay = juce::jlimit(0.0f, 1.0f, read(decay));
    p.exciteAmount = juce::jlimit(0.0f, 1.0f, read(exciteAmount));
    p.exciterType = juce::jlimit(0, 2, static_cast<int>(read(exciterType)));
    p.impulseWidth = juce::jlimit(0.05f, 0.95f, read(impulseWidth));
    p.interpolation = static_cast<int>(read(interpolation)) == 1 ? 1 : 0;
    p.pitchTransition = static_cast<int>(read(pitchTransition)) == 1 ? 1 : 0;
    p.brightness = juce::jlimit(0.0f, 1.0f, read(brightness));
    p.pickPosition = juce::jlimit(0.05f, 0.5f, read(pickPosition));
    p.body = juce::jlimit(0.0f, 1.0f, read(bodyResonance));
    p.stereoWidth = juce::jlimit(0.0f, 1.0f, read(stereoWidth));
    p.detune = juce::jlimit(0.0f, 20.0f, read(detune));
    voices.setParameters(p);
    const bool internal = read(patternOn) > 0.5f;
    if (internal != wasPatternOn)
    {
        voices.allNotesOff(); pattern.reset(); wasPatternOn = internal;
    }
    if (internal)
        pattern.render(voices, audio, read(patternRate), static_cast<int>(read(patternType)), read(patternVelocity));
    else
    {
        int offset = 0;
        for (const auto metadata : midi)
        {
            const int position = juce::jlimit(offset, audio.getNumSamples(), metadata.samplePosition);
            voices.render(audio, offset, position - offset);
            if (position < audio.getNumSamples()) voices.handleMidi(metadata.data, metadata.numBytes);
            offset = position;
        }
        voices.render(audio, offset, audio.getNumSamples() - offset);
    }
    gain.setTargetValue(juce::Decibels::decibelsToGain(juce::jlimit(-60.0f, 12.0f, read(outputGain))));
    monoMix.setTargetValue(read(mono) > 0.5f ? 1.0f : 0.0f);
    float blockPeak = 0.0f;
    std::array<float, 256> visual {};
    const bool show = visualisationEnabled.load(std::memory_order_relaxed);
    for (int i = 0; i < audio.getNumSamples(); ++i)
    {
        const float g = gain.getNextValue(), mix = monoMix.getNextValue();
        const float centre = audio.getNumChannels() > 1
            ? 0.5f * (audio.getSample(0, i) + audio.getSample(1, i)) : audio.getSample(0, i);
        float visualSample = 0.0f;
        for (int channel = 0; channel < juce::jmin(2, audio.getNumChannels()); ++channel)
        {
            const float input = (audio.getSample(channel, i) * (1.0f - mix) + centre * mix) * g;
            const auto c = static_cast<size_t>(channel);
            const float output = input - previousInput[c] + highPassR * previousOutput[c];
            previousInput[c] = input; previousOutput[c] = output;
            audio.setSample(channel, i, output);
            blockPeak = juce::jmax(blockPeak, std::abs(output)); visualSample += output;
        }
        visual[static_cast<size_t>(i % 256)] = visualSample / static_cast<float>(juce::jmin(2, audio.getNumChannels()));
        if (show && (i % 256 == 255 || i == audio.getNumSamples() - 1))
            scopeFifo.push(visual.data(), i % 256 + 1);
    }
    if (show)
    {
        voices.copySnapshot(visual.data(), 256); delayFifo.push(visual.data(), 256);
    }
    peak.store(juce::jmax(peak.load(std::memory_order_relaxed), blockPeak), std::memory_order_relaxed);
    displayedVoices.store(voices.activeVoices(), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* KarplusStrong_PluckedStringAudioProcessor::createEditor()
{
    return new KarplusStrong_PluckedStringAudioProcessorEditor(*this);
}
void KarplusStrong_PluckedStringAudioProcessor::getStateInformation(juce::MemoryBlock& data)
{
    auto tree = apvts.copyState(); tree.setProperty("schemaVersion", 2, nullptr);
    if (auto xml = tree.createXml()) copyXmlToBinary(*xml, data);
}
void KarplusStrong_PluckedStringAudioProcessor::setStateInformation(const void* data, int size)
{
    if (data == nullptr || size <= 0) return;
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml(*xml);
            for (auto child : tree)
            {
                if (auto* parameter = apvts.getParameter(child.getProperty("id").toString()))
                {
                    const float v = static_cast<float>(child.getProperty("value"));
                    child.setProperty("value", std::isfinite(v)
                        ? parameter->convertFrom0to1(parameter->convertTo0to1(v))
                        : parameter->convertFrom0to1(parameter->getDefaultValue()), nullptr);
                }
            }
            apvts.replaceState(tree);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KarplusStrong_PluckedStringAudioProcessor();
}
