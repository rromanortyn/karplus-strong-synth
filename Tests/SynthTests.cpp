#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <chrono>

static thread_local bool trackAllocations = false;
static thread_local size_t allocationCount = 0;
void* operator new(std::size_t size)
{
    if (trackAllocations) ++allocationCount;
    if (void* p = std::malloc(size == 0 ? 1 : size)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
#if __cpp_aligned_new
void* operator new(std::size_t size, std::align_val_t alignment)
{
    if (trackAllocations) ++allocationCount;
    void* p = nullptr;
   #if JUCE_WINDOWS
    p = _aligned_malloc(size, static_cast<size_t>(alignment));
   #else
    if (posix_memalign(&p, static_cast<size_t>(alignment), size) != 0) p = nullptr;
   #endif
    if (p) return p;
    throw std::bad_alloc();
}
void operator delete(void* p, std::align_val_t) noexcept
{
   #if JUCE_WINDOWS
    _aligned_free(p);
   #else
    std::free(p);
   #endif
}
void operator delete(void* p, std::size_t, std::align_val_t a) noexcept { ::operator delete(p, a); }
#endif

using Processor = KarplusStrong_PluckedStringAudioProcessor;
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void set(Processor& p, const char* id, float value)
{
    auto* parameter = p.state().getParameter(id);
    require(parameter != nullptr, "Missing parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
float get(Processor& p, const char* id) { return p.state().getRawParameterValue(id)->load(); }
void prepare(Processor& p, double rate = 48000, int block = 256)
{
    p.setRateAndBufferSizeDetails(rate, block); p.prepareToPlay(rate, block);
}
float process(Processor& p, juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    const size_t before = allocationCount;
    trackAllocations = true; p.processBlock(audio, midi); trackAllocations = false;
    require(allocationCount == before, "C++ heap allocation in audio callback");
    float peak = 0;
    for (int c = 0; c < audio.getNumChannels(); ++c)
        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            const float v = audio.getSample(c, i);
            require(std::isfinite(v), "Non-finite output"); peak = juce::jmax(peak, std::abs(v));
        }
    return peak;
}
void engineTests()
{
    for (double alpha : {0.05, 0.2, 0.6, 1.0})
    {
        const double dcDelay = (1.0 - alpha) / alpha;
        require(std::abs(StringTuning::filterPhaseDelay(0.0, alpha) - dcDelay) < 1.0e-12,
                "Incorrect DC filter delay");
        require(std::abs(StringTuning::filterPhaseDelay(1.0e-8, alpha) - dcDelay) < 1.0e-9,
                "Filter phase delay does not approach its low-frequency limit");
        for (double omega : {0.01, 0.1, 1.0, 2.0})
        {
            const auto q = std::polar(1.0, -omega);
            const auto filter = alpha / (1.0 - (1.0 - alpha) * q);
            const auto compensation = std::polar(1.0, omega * StringTuning::filterPhaseDelay(omega, alpha));
            require(std::abs(std::arg(filter * compensation)) < 1.0e-12,
                    "Adaptive delay does not cancel the one-pole filter phase");
        }
    }
    double worstPoleError = 0;
    for (double rate : {44100.0, 48000.0, 96000.0, 192000.0})
        for (double alpha : {0.05, 0.6, 1.0})
            for (int mode = 0; mode < 2; ++mode)
                for (int note = 0; note < 128; ++note)
                {
                    const double hz = 440.0 * std::pow(2.0, (note - 69) / 12.0);
                    const double omega = juce::MathConstants<double>::twoPi * hz / rate;
                    const double feedback = std::exp(-6.907755 / (hz * 10.0));
                    const auto t = StringTuning::calculate(omega, alpha, feedback, mode);
                    const auto q = std::polar(1.0 / t.radius, -omega);
                    const int integer = static_cast<int>(t.delay);
                    const double fraction = t.delay - integer;
                    const auto interpolator = mode == 0 ? 1.0 - fraction + fraction * q
                        : (t.coefficient + q) / (1.0 + t.coefficient * q);
                    const auto loop = feedback * alpha * std::pow(q, integer) * interpolator / (1.0 - (1.0 - alpha) * q);
                    worstPoleError = juce::jmax(worstPoleError, std::abs(loop - 1.0));
                    if (std::abs(loop - 1.0) > 1.0e-5)
                    {
                        std::cerr << "Pole error: note=" << note << " rate=" << rate << " alpha=" << alpha << " mode=" << mode << " error=" << std::abs(loop-1.0) << '\n';
                        throw std::runtime_error("Fractional delay failed its target resonance frequency");
                    }
                }
    std::cout << "PASS: full-range closed-loop tuning (3072 cases), maximum complex residual " << worstPoleError << '\n';
    Processor p;
    require(get(p, "damping") == 0.6f && get(p, "exciteAmount") == 0.7f, "Incorrect reference defaults");
    auto gainRange = p.state().getParameter("outputGain")->getNormalisableRange();
    require(gainRange.start == -60 && gainRange.end == 12, "Incorrect output range");
    auto alphaRange = p.state().getParameter("damping")->getNormalisableRange();
    require(alphaRange.start == 0.05f && alphaRange.end == 1, "Incorrect alpha range");
    juce::AudioBuffer<float> audio(2, 256); juce::MidiBuffer midi; midi.ensureSize(4096);
    prepare(p); require(process(p, audio, midi) == 0, "Instrument should be silent without MIDI");
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 127);
    require(process(p, audio, midi) > 0.0001f, "Note did not sound within its MIDI block");
    for (int i = 0; i < 127; ++i) require(audio.getSample(0, i) == 0, "MIDI was rendered before its timestamp");
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOff(1, 60), 33); process(p, audio, midi);
    midi.clear(); for (int i = 0; i < 100; ++i) process(p, audio, midi);
    require(p.activeVoiceCount() == 0, "Released note stayed active");
    std::cout << "PASS: parameter ranges, silence, MIDI timestamp, note release\n";

    for (double rate : {44100.0, 48000.0, 96000.0, 192000.0})
        for (int mode = 0; mode < 2; ++mode)
            for (int exciter = 0; exciter < 3; ++exciter)
            {
                set(p, "interpolation", static_cast<float>(mode)); set(p, "exciterType", static_cast<float>(exciter));
                prepare(p, rate);
                for (int note = 0; note < 128; ++note)
                {
                    midi.clear(); midi.addEvent(juce::MidiMessage::allSoundOff(1), 0);
                    midi.addEvent(juce::MidiMessage::noteOn(1, note, 0.8f), 0);
                    const float peak = process(p, audio, midi);
                    require(peak > 1.0e-8f, "A MIDI note failed to excite the string");
                }
            }
    std::cout << "PASS: all 128 MIDI notes, 3 exciters, 2 interpolators, 4 sample rates (3072 cases)\n";

    set(p, "damping", 1); set(p, "decay", 1); prepare(p);
    midi.clear();
    for (int n = 24; n < 40; ++n) midi.addEvent(juce::MidiMessage::noteOn(1, n, 0.7f), 0);
    process(p, audio, midi); require(p.activeVoiceCount() == 16, "Expected 16 simultaneous voices");
    midi.clear(); midi.addEvent(juce::MidiMessage::noteOn(1, 40, 0.7f), 0); process(p, audio, midi);
    require(p.activeVoiceCount() == 16, "Voice stealing changed the pool size");
    midi.clear(); midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
    for (int n = 24; n <= 40; ++n) midi.addEvent(juce::MidiMessage::noteOff(1, n), 1);
    process(p, audio, midi); midi.clear();
    for (int i = 0; i < 80; ++i) process(p, audio, midi);
    require(p.activeVoiceCount() == 16, "Sustain pedal failed to hold notes");
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 0), 0); process(p, audio, midi); midi.clear();
    for (int i = 0; i < 100; ++i) process(p, audio, midi);
    require(p.activeVoiceCount() == 0, "Sustain pedal release left stuck notes");
    std::cout << "PASS: 16 voices, voice stealing, sustain/release\n";

    prepare(p); p.setVisualisationEnabled(true);
    set(p, "patternOn", 1);
    float totalPeak = 0;
    for (int i = 0; i < 200; ++i)
    {
        set(p, "patternType", static_cast<float>(i % 8)); set(p, "damping", i % 2 ? 0.05f : 1.0f);
        set(p, "bodyResonance", (i % 10) * 0.1f); set(p, "detune", static_cast<float>(i % 20));
        set(p, "interpolation", static_cast<float>(i % 2)); set(p, "pitchTransition", static_cast<float>((i / 2) % 2));
        totalPeak = juce::jmax(totalPeak, process(p, audio, midi));
    }
    require(totalPeak > 0, "Internal pattern did not render");
    set(p, "patternOn", 0); process(p, audio, midi);
    for (int i = 0; i < 100; ++i) process(p, audio, midi);
    require(p.activeVoiceCount() == 0, "Pattern disable left stuck notes");
    std::array<float, 2048> scope {};
    require(p.scope().read(scope.data(), 2048) > 0 && p.delayScope().read(scope.data(), 2048) > 0,
            "Visualisation queues received no data");
    p.setVisualisationEnabled(false);
    std::cout << "PASS: parameter extremes/automation, pattern changes, full visual FIFOs, no C++ callback allocations\n";

    set(p, "exciterType", 2); set(p, "outputGain", -12); set(p, "mono", 1); set(p, "interpolation", 1);
    juce::MemoryBlock state; p.getStateInformation(state);
    Processor restored; restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    require(get(restored, "exciterType") == 2 && get(restored, "outputGain") == -12
            && get(restored, "mono") == 1 && get(restored, "interpolation") == 1, "State round trip failed");
    restored.setStateInformation(nullptr, 0);
    const char invalid[] = "invalid"; restored.setStateInformation(invalid, sizeof(invalid));
    require(get(restored, "outputGain") == -12, "Invalid state changed parameters");
    prepare(restored); midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0); process(restored, audio, midi);
    for (int i = 0; i < 256; ++i) require(audio.getSample(0, i) == audio.getSample(1, i), "Mono output differs across channels");
    std::cout << "PASS: state round trip, invalid-state handling, mono output\n";

    auto layout = restored.getBusesLayout(); layout.outputBuses.set(0, juce::AudioChannelSet::mono());
    restored.releaseResources(); require(restored.setBusesLayout(layout), "Mono bus unsupported"); prepare(restored);
    juce::AudioBuffer<float> monoAudio(1, 256); process(restored, monoAudio, midi);
    require(restored.getLatencySamples() == 0, "Unexpected plugin latency");
    std::cout << "PASS: mono host bus, zero added latency\n";

    Processor benchmark; prepare(benchmark); set(benchmark, "damping", 1); set(benchmark, "decay", 1);
    for (int number : {1, 16})
    {
        prepare(benchmark); midi.clear();
        for (int n = 0; n < number; ++n) midi.addEvent(juce::MidiMessage::noteOn(1, 36+n, 0.5f), 0);
        process(benchmark, audio, midi); midi.clear();
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < 1000; ++i) benchmark.processBlock(audio, midi);
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::cout << "BENCHMARK " << number << " voices: " << seconds * 1000.0 << " us/block, "
                  << seconds / (256000.0/48000.0) * 100 << "% of one realtime core budget\n";
    }
}

juce::Component* find(juce::Component& root, const juce::String& name)
{
    if (root.getName() == name) return &root;
    for (auto* child : root.getChildren()) if (auto* found = find(*child, name)) return found;
    return nullptr;
}
void guiTests(const juce::File& directory)
{
    Processor p; prepare(p);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    require(dynamic_cast<KarplusStrong_PluckedStringAudioProcessorEditor*>(editor.get()) != nullptr, "Generic editor still in use");
    auto* slider = dynamic_cast<juce::Slider*>(find(*editor, "damping"));
    require(slider != nullptr, "Damping slider missing");
    bool checkedText = false;
    for (auto* child : slider->getChildren())
        if (auto* label = dynamic_cast<juce::Label*>(child))
        {
            require(std::abs(label->getFont().getHeight() - 16.0f) < 0.01f, "Control value font is not 16px");
            require(label->findColour(juce::Label::outlineColourId).isTransparent(), "Value textbox outline differs from reference");
            checkedText = true;
        }
    require(checkedText, "Slider value display missing");
    set(p, "damping", 0.8f);
    require(std::abs(slider->getValue() - 0.8) < 0.001, "Host automation did not update slider");
    slider->setValue(0.3, juce::sendNotificationSync);
    require(std::abs(get(p, "damping") - 0.3f) < 0.001, "Slider did not update processor");
    require(std::abs(slider->getDoubleClickReturnValue() - 0.6) < 0.001, "Double-click default wrong");
    auto* impulse = dynamic_cast<juce::TextButton*>(find(*editor, "exciter2"));
    impulse->setToggleState(true, juce::dontSendNotification); impulse->onClick();
    require(get(p, "exciterType") == 2, "Exciter radio button did not update DSP parameter");
    set(p, "exciterType", 1); set(p, "damping", 0.62f); set(p, "outputGain", -3);
    juce::AudioBuffer<float> audio(2, 256); juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 57, 0.8f), 0); process(p, audio, midi); midi.clear();
    for (int i = 0; i < 8; ++i) process(p, audio, midi);
    juce::Thread::sleep(50);
    juce::Timer::callPendingTimersSynchronously();
    directory.createDirectory();
    for (auto size : { juce::Point<int>(700,500), juce::Point<int>(600,400), juce::Point<int>(1200,800) })
    {
        editor->setSize(size.x, size.y);
        auto image = editor->createComponentSnapshot(editor->getLocalBounds());
        auto out = directory.getChildFile("editor-" + juce::String(size.x) + "x" + juce::String(size.y) + ".png").createOutputStream();
        if (out != nullptr) { out->setPosition(0); out->truncate(); }
        require(out != nullptr && juce::PNGImageFormat().writeImageToStream(image, *out), "Screenshot write failed");
    }
    editor->setSize(700, 500);
    auto* advanced = dynamic_cast<juce::TextButton*>(find(*editor, "advanced"));
    require(advanced != nullptr, "Advanced button missing");
    advanced->setToggleState(true, juce::dontSendNotification); advanced->onClick();
    auto out = directory.getChildFile("editor-advanced.png").createOutputStream();
    if (out != nullptr) { out->setPosition(0); out->truncate(); }
    require(out != nullptr && juce::PNGImageFormat().writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()), *out), "Advanced screenshot write failed");
    auto* view = dynamic_cast<juce::ComboBox*>(find(*editor, "visualisation"));
    require(view != nullptr, "Visualisation selector missing");
    for (int mode = 2; mode <= 3; ++mode)
    {
        view->setSelectedId(mode, juce::sendNotificationSync);
        for (int i = 0; i < 8; ++i) process(p, audio, midi);
        juce::Thread::sleep(40); juce::Timer::callPendingTimersSynchronously();
        auto stream = directory.getChildFile(mode == 2 ? "editor-spectrum.png" : "editor-delay.png").createOutputStream();
        require(stream != nullptr, "Visualisation screenshot open failed");
        stream->setPosition(0); stream->truncate();
        require(juce::PNGImageFormat().writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()), *stream), "Visualisation screenshot failed");
    }
    auto* viewport = dynamic_cast<juce::Viewport*>(find(*editor, "advanced-scroll"));
    require(viewport != nullptr, "Advanced viewport missing"); viewport->setViewPosition(0, 200);
    auto controls = directory.getChildFile("editor-advanced-controls.png").createOutputStream();
    require(controls != nullptr, "Advanced controls screenshot open failed");
    controls->setPosition(0); controls->truncate();
    require(juce::PNGImageFormat().writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()), *controls), "Advanced controls screenshot failed");
    std::cout << "PASS: custom editor, automation bindings, radio buttons, double-click defaults, 3 sizes, advanced panel\n";
}
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI initialise;
    try
    {
        if (argc > 1 && juce::String(argv[1]) == "--gui")
            guiTests(argc > 2 ? juce::File(argv[2]) : juce::File::getCurrentWorkingDirectory());
        else engineTests();
        return 0;
    }
    catch (const std::exception& e) { trackAllocations = false; std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
