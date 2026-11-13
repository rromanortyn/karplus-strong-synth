# Karplus–Strong Virtual String Synth

A JUCE instrument implementing the UI and synthesis workflow in *Пояснювальна_записка_закінчено.pdf*, especially §§1.5, 2.3–2.5 and 3.4–3.6 and Figure 3.4. See [reference conformance](docs/reference-conformance.md) for the requirements, design decisions, and validation limits.

The 700×500 main editor provides **Damping**, **Excite**, **Output**, three excitation buttons, a live oscilloscope, and a peak meter. It scales between 600×400 and 1200×800. Double-click a knob to restore its default; values can also be typed. Controls support host automation and session recall.

**Advanced** opens the additional model controls, linear/allpass interpolation, glide/crossfade pitch changes, and the existing internal MIDI patterns. Scroll to reach the lower controls. The visualisation selector provides the waveform, spectrum, and an actual delay-buffer snapshot. **Mono** sums the two output channels. Impulse width affects newly triggered notes with Impulse excitation selected. The scope scales its waveform automatically; the peak meter shows the actual output level in dBFS.

The synthesis engine provides 16 voices, oldest-voice stealing with a 5 ms crossfade, velocity-sensitive Noise / Filtered Noise / triangular Impulse excitation, a one-pole loop filter, calibrated fractional delay, ±2-semitone MIDI pitch bend, sustain pedal, all-notes/all-sound-off, and smoothed damping, output gain, and pitch transitions. Existing body, detune, stereo-width and pattern controls remain available in Advanced.

## Build

Use JUCE **8.0.17-security** (commit `dabd5ad13cd869cba41046947fffc8e5ce5cd662`) and a C++17 compiler, CMake ≥3.22, and Ninja or another CMake generator.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DJUCE_PATH=/path/to/JUCE
cmake --build build --parallel 4
```

Outputs are under `build/KarplusStrong_PluckedString_artefacts/Release/`:

- `VST3/KarplusStrong_PluckedString.vst3`
- `Standalone/KarplusStrong_PluckedString` (platform-specific extension/bundle)
- `AU/` on macOS only

For a universal macOS build, also pass `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`. The minimum macOS deployment target is 11.0. The `.jucer` project also contains Visual Studio, Xcode, and Linux exporters; configure its JUCE module paths before exporting.

After pulling changes, Projucer users must reopen `KarplusStrong_PluckedString.jucer` and choose **Save Project and Open in IDE** before rebuilding. This regenerates `JuceLibraryCode/BinaryData.h` and its compiled sources from the bundled `Resources/Inter.ttf`. Existing generated Visual Studio/Xcode projects do not automatically pick up new binary resources. Keep the font's **Binary Resource** flag and **Include BinaryData in JuceHeader** enabled.

On Linux, development packages are needed for ALSA, X11/Xext/Xrandr/Xinerama/Xcursor, FreeType, Fontconfig, and OpenGL. The app does not require WebKit or network access at runtime. In the prepared cloud environment, activate its user-owned tools and headers first:

```bash
source /workspace/karplus-strong-env/activate.sh
cmake -S /workspace/karplus-strong-synth -B /workspace/karplus-strong-synth/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DJUCE_PATH=/workspace/karplus-strong-env/deps/JUCE
cmake --build /workspace/karplus-strong-synth/build --parallel 4
```

Use the existing checkout; cloud tasks are already isolated. Do not create a worktree unless explicitly requested.

## Validate

```bash
ctest --test-dir build --output-on-failure
# Linux GUI checks and screenshots without physical audio hardware:
xvfb-run -a ./build/SynthTests --gui "$PWD/build/screenshots"
# Independent VST3 host (numpy, mido and pedalboard are preinstalled in the cloud venv):
python Tests/host_smoke.py
```

The native suite tests parameter ranges, full-range tuning, every MIDI note across four sample rates and all exciters/interpolators, sample-accurate MIDI timing, 16-voice operation/stealing, sustain, state, mono routing, and absence of C++ heap allocations during the exercised audio callbacks. It also reports a local processing benchmark. GUI checks cover bindings, radio selection, reset defaults, the documented window sizes, and visualisation modes.

The independent host tests rendered tuning, chords, velocity, excitation types/width, pitch bend, mono, internal patterns, and preset restoration. Results and an audio example are written to `build/test-results/`. At the highest notes, strongly damped linear interpolation produces very short signals; their frequency is estimated from damped poles in the rendered audio rather than a biased finite-window FFT peak.

Windows/macOS, AU, and the five DAWs named in the PDF require validation on those platforms. Linux checks do not establish those compatibility claims or reproduce the PDF's hardware-specific benchmark.

## Compatibility and resources

Existing parameter IDs and their order are retained, with the reference controls appended. **Damping now means the document's alpha (0.05–1)**, and Output spans **−60 to +12 dB**. Old parameter values can be recalled, but the corrected model and changed ranges alter the sound and may change normalized host automation; do not expect old sessions or `preset.RPL` to sound identical. Keep a copy of an older build for faithful playback of older projects.

Inter is bundled under the SIL Open Font License; see [its license](Resources/Inter-LICENSE.txt). Project details and the original algorithm discussion are in `docs/`.
