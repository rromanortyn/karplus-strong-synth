# Reference conformance

Reference: the user-provided *Пояснювальна_записка_закінчено.pdf*, printed page numbers below. Its descriptions and figures are treated as product requirements. Appendix listings contain omissions and do not override safe or working implementations.

## UI

| Requirement | Implementation / verification |
| --- | --- |
| Figure 3.4, pp.55–58: 700×500 charcoal/orange editor, Inter, three rotary controls, three excitation radio buttons, live waveform and peak meter | Custom editor returned by `createEditor`; bundled Inter; palette #1E1E22 / #2A2A2F / #3A3A40 / #B0B0B0 / #E0E0E0 / #FF7C2B. GUI screenshots and control-binding tests. |
| p.41: adaptive 600×400–1200×800; p.55: default 700×500 | 700×500 default with the stated resize limits. Main layout verified at all three sizes. This reconciles the fixed-size implementation description with the earlier adaptive-layout requirement. |
| pp.36,81: alpha 0.05–1 (default .6), excitation 0–1 (default .7), output −60…+12 dB (default 0) | Host parameters, editable value boxes, defaults, and native range tests. |
| pp.41,57: Noise / Filtered Noise / Impulse; adjustable impulse width | Mutually exclusive attached radio buttons. Triangle width available in Advanced and disabled for non-Impulse types. Host tests confirm three distinct waveforms and width changes. |
| p.41: decay, linear/allpass, mono/stereo, extended spectrum and delay-state display | Advanced model controls and visualisation selector; main Mono button. FFT is performed on the GUI thread. Delay display reads a real string-buffer snapshot. |
| pp.42,57: two-way automation/state attachments, double-click defaults | JUCE parameter attachments for all controls. Native GUI/state tests verify both directions and reset values. |
| pp.57,60: 2048-sample waveform history, 30 Hz display, nonblocking audio→GUI transfer | Consumer-owned 2048-sample history; 30 Hz timer; fixed SPSC `AbstractFifo` queues. Full queues drop complete visual packets instead of blocking audio. The appendix's non-advancing FIFO example is corrected. |

The main screen follows Figure 3.4. Controls mentioned in the prose but omitted from that figure live in Advanced, consistent with the document's minimal main-screen principle. The waveform auto-scales to remain readable; the output peak meter is not auto-scaled.

## Synthesis and integration

| Requirement | Implementation / verification |
| --- | --- |
| pp.27,38,51: one-pole feedback filter `y = alpha*x + (1-alpha)*y_previous` | `DampingFilter`, 20 ms coefficient smoothing, alpha floor .05. No averaging-filter substitution. |
| pp.27,37–39: fractional delay, full MIDI range, selectable linear/allpass | `DelayLine` and `StringTuning`: calibrate the damped closed-loop resonance including the interpolator/filter phase. A fixed −.5-sample correction is insufficient for the specified one-pole filter. 3072 closed-loop tuning cases span 128 notes, both interpolators, alpha .05/.6/1, and 44.1/48/96/192 kHz. |
| pp.37,52: white noise, fixed-.5 filtered noise, triangular excitation, velocity × excitation amount | `Exciter`, preallocated buffers, deterministic per-instance random generator, zero-mean excitation. Independent host checks excitation/velocity/width. |
| pp.38–39,53: 16 voices, free voice first then oldest, 5–10 ms stealing fade | Fixed 16-voice manager; two preallocated banks per voice; 5 ms crossfade. Native 16-voice/stealing tests. |
| pp.40,59,62: ±2-semitone pitch bend, glide or crossfade | Channel-scoped pitch bend with 20 ms delay glide or two-read-head crossfade. Independent host checks both directions in all four mode combinations. |
| pp.27,60: MIDI Note On/Off/velocity, no late onset, no allocation/locking in DSP | MIDI buffer segmented at each event timestamp; fixed voice/pattern tables; no `juce::Synthesiser` lock or temporary MIDI buffer. Native tests instrument C++ `new`/`new[]`/aligned-new during normal rendering, note triggers, stealing, parameter changes, and full visual queues. These are allocation tests, not a claim that RTSanitizer was run. |
| pp.58–61: smooth output/control changes, DC and denormal handling, finite output | 50 ms gain and 20 ms damping/pitch/mono transitions; zero-mean excitation, 20 Hz output DC blocker, `ScopedNoDenormals`, finite-state guard. The DC blocker is placed after the loop to avoid suppressing low MIDI fundamentals inside the resonator. |
| pp.42,49: save/restore host state | Versioned APVTS XML; invalid-state input ignored; parameter values bounded; state round-trip checked natively and through VST3 presets. |
| pp.27,44: VST3, AU, Standalone; Windows/macOS | CMake and Projucer targets provided. AU is generated on macOS only. Current execution validates Linux VST3 and Standalone; Windows/macOS and actual DAW scans remain unverified. |

Sustain (CC64), all-notes/all-sound-off (CC123/120), and the existing internal pattern generator are also supported. The original body, stereo and detune extensions remain available without cluttering the main screen.

## Validation evidence and limits

### Adaptive damping compensation

For `H(z) = alpha / (1 - (1-alpha) z^-1)`, the phase delay in samples is
`Dphase(omega) = atan2((1-alpha) sin(omega), 1-(1-alpha) cos(omega)) / omega`.
Its DC limit is `(1-alpha)/alpha`; that expression is not the group delay at
every frequency. The initial delay estimate is `Fs/f0 - Dphase(omega)`, then
the closed-loop solver accounts for decay and fractional interpolation.
There is no additional fixed half-sample subtraction. Changing Damping calls
`tune` for active strings, updating compensation for the new alpha. The filter
and delay transitions take 20 ms; transient pitch modulation during a control
change is still possible because smoothing alpha and delay independently does
not enforce the resonance condition at every intermediate sample.

Tests verify the DC limit, cancellation of the filter phase at several
frequencies/alpha values, and the complete loop resonance across the MIDI range.

- Native engine suite: parameter ranges, 3072 tuning cases, 3072 note/exciter/interpolator/rate rendering cases, sample-accurate MIDI, release, 16 voices/stealing, sustain, automation extremes, visual queues, state/invalid state, mono buses and zero added latency.
- Independent host: 256 rendered pitch cases at 48 kHz (128 notes × two modes, alpha=1), eight pitch-bend mode/direction combinations, stereo chords at 44.1/48/96 kHz, velocity/excitation/width, mono, patterns, and preset restoration. It enforces a ±2-cent limit. Sustaining notes use FFT; the very short highest linear-mode notes use autoregressive damped-pole estimation from rendered samples. The current run's numeric results are in `build/test-results/host-smoke.json`.
- Native GUI suite: main controls, automation, radio selection, double-click defaults, 600×400 / 700×500 / 1200×800 snapshots, Advanced, spectrum and delay displays. Artifacts are overwritten rather than appended.
- Local benchmark: the native executable reports one/16-voice 256-sample blocks at 48 kHz. This measures the current cloud CPU; it does not reproduce the thesis's Intel i5-1135G7 figures.
- Not executed here: Windows/macOS builds, AU hosting, Ableton/FL Studio/Logic/Reaper/Cubase compatibility, physical MIDI/audio hardware and expert listening tests. The document's assertions that these passed are reference claims, not evidence for this build.

The old parameter identifiers/order are preserved, but damping semantics, output range, polyphony and the synthesis model change to match the reference. Old normalized automation and example REAPER presets require review; bit-identical compatibility is not claimed.
