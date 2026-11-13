"""Independent VST3 host checks. Run with the prepared environment's Python."""
from pathlib import Path
import json
import sys
import time
import numpy as np
from mido import Message
from pedalboard import load_plugin
from pedalboard.io import AudioFile

root = Path(__file__).resolve().parents[1]
bundle = Path(sys.argv[1]) if len(sys.argv) > 1 else root / 'build/KarplusStrong_PluckedString_artefacts/Release/VST3/KarplusStrong_PluckedString.vst3'
results = []

def instrument(**parameters):
    plugin = load_plugin(str(bundle))
    assert plugin.is_instrument, 'VST3 was not classified as an instrument'
    for key, value in parameters.items():
        setattr(plugin, key, value)
    return plugin

def render(plugin, messages=None, duration=1.0, rate=48000, block=256, reset=True):
    audio = plugin(messages or [], duration=duration, sample_rate=rate, buffer_size=block, reset=reset)
    assert audio.shape == (2, round(duration * rate)), audio.shape
    assert np.isfinite(audio).all(), 'Non-finite output'
    return audio

def note(number=69, velocity=100):
    return Message('note_on', note=number, velocity=velocity, time=0)

def frequency(audio, expected, rate):
    signal = audio.astype(np.float64)
    size = 1 << max(20, (len(signal) - 1).bit_length())
    spectrum = np.abs(np.fft.rfft(signal * np.hanning(len(signal)), n=size))
    lower = max(1, int(expected * 2 ** (-80 / 1200) * size / rate))
    upper = min(len(spectrum) - 1, int(expected * 2 ** (80 / 1200) * size / rate) + 1)
    index = lower + int(np.argmax(spectrum[lower:upper]))
    y = np.log(spectrum[index-1:index+2] + 1e-30)
    shift = 0.5 * (y[0] - y[2]) / (y[0] - 2*y[1] + y[2])
    return (index + shift) * rate / size

def short_note_frequency(audio, expected, rate):
    # At the top of the range, a linear-interpolated string can decay within
    # milliseconds. A finite-window FFT peak is biased by that decay. Estimate
    # its damped oscillation frequency from the rendered samples instead.
    order = int(np.ceil(rate / expected)) + 4
    signal = audio[64:256].astype(np.float64)
    signal /= np.max(np.abs(signal))
    matrix = np.array([signal[i-order:i][::-1] for i in range(order, len(signal))])
    coefficients = np.linalg.lstsq(matrix, signal[order:], rcond=1e-8)[0]
    poles = np.roots(np.r_[1.0, -coefficients])
    frequencies = np.angle(poles) * rate / (2*np.pi)
    return float(frequencies[np.argmin(np.abs(frequencies - expected))])

start = time.monotonic()
p = instrument()
assert {'damping', 'excite_amount', 'output_level_db', 'excitation', 'impulse_width',
        'interpolation', 'pitch_transition', 'mono_output'} <= set(p.parameters)
assert not render(p, duration=0.1).any(), 'Instrument sounded without MIDI'

worst = 0.0
for mode in ('Linear', 'Allpass'):
    for number in range(128):
        p = instrument(damping=1.0, excitation='Impulse', interpolation=mode)
        expected = 440 * 2 ** ((number - 69) / 12)
        audio = render(p, [note(number)], duration=2.0 if number < 36 else 1.0)
        measured = (short_note_frequency(audio[0], expected, 48000) if mode == 'Linear' and number >= 108
                    else frequency(audio[0], expected, 48000))
        cents = 1200 * np.log2(measured / expected)
        assert abs(cents) <= 2, (mode, number, measured, expected, cents)
        worst = max(worst, abs(float(cents)))
results.append({'check': 'Rendered pitch: 128 MIDI notes, both interpolators, alpha=1, 48 kHz; FFT plus damped-pole estimation for short high linear-mode notes',
                'cases': 256, 'maximum_error_cents': worst})

for rate, block in ((44100, 64), (48000, 512), (96000, 256)):
    events = [note(n) for n in (48, 52, 55, 60, 64, 67, 72, 76)]
    events += [Message('note_off', note=n, time=0.5) for n in (48, 52, 55, 60, 64, 67, 72, 76)]
    p = instrument(stereo_width=0.8, detune_cents=8)
    audio = render(p, events, duration=1.5, rate=rate, block=block)
    assert np.max(np.abs(audio)) > 0.001
    assert np.max(np.abs(audio[:, -rate//4:])) < 1e-5, 'Note release left audio running'
    assert np.max(np.abs(audio[0]-audio[1])) > 1e-4, 'Stereo spread had no effect'
results.append({'check': '8-note stereo chords and release at 44.1/48/96 kHz, blocks 64/512/256'})

for transition in ('Glide', 'Crossfade'):
    for mode in ('Linear', 'Allpass'):
        for wheel, semitones in ((-8192, -2), (8191, 2)):
            p = instrument(damping=1.0, excitation='Impulse', interpolation=mode, pitch_transition=transition)
            events = [note(), Message('pitchwheel', pitch=wheel, time=0.15)]
            audio = render(p, events, duration=1.0)
            expected = 440 * 2 ** (semitones / 12)
            cents = 1200 * np.log2(frequency(audio[0, 14400:], expected, 48000) / expected)
            assert abs(cents) < 2, (transition, mode, wheel, cents)
            # Transition may alter the waveform, but must remain bounded.
            assert np.max(np.abs(audio)) < 1.0
results.append({'check': 'Pitch bend +/-2 semitones in both pitch-transition and interpolation modes', 'cases': 8})

loud = render(instrument(excitation='Impulse'), [note(60, 127)], duration=0.2)
soft = render(instrument(excitation='Impulse'), [note(60, 32)], duration=0.2)
assert np.allclose(soft, loud * (32/127), atol=1e-6), 'MIDI velocity did not scale excitation linearly'
quiet = render(instrument(excitation='Impulse', excite_amount=0), [note()], duration=0.2)
assert not quiet.any(), 'Zero excitation was not silent'
waveforms = [render(instrument(excitation=kind), [note(60)], duration=0.2) for kind in ('Noise','Filtered Noise','Impulse')]
assert all(not np.allclose(waveforms[a], waveforms[b]) for a,b in ((0,1),(1,2),(0,2)))
a = render(instrument(excitation='Impulse', impulse_width=0.2), [note(60)], duration=0.2)
b = render(instrument(excitation='Impulse', impulse_width=0.8), [note(60)], duration=0.2)
assert not np.allclose(a,b), 'Impulse width had no effect'
results.append({'check': 'Velocity scaling, excitation amount, three distinct exciters, impulse width'})

p = instrument(pattern_generator_on_off=True, excitation='Filtered Noise', output_level_db=-12,
               interpolation='Allpass', pitch_transition='Crossfade', mono_output=True)
audio = render(p, duration=1.0)
assert np.max(np.abs(audio)) > 1e-4
assert np.array_equal(audio[0],audio[1]), 'Mono output differs between channels'
saved = p.preset_data
p.output_level_db = -3
p.excitation = 'Noise'
p.preset_data = saved
assert abs(float(p.output_level_db) + 12) < 0.05 and str(p.excitation) == 'Filtered Noise'
p.pattern_generator_on_off = False
released = render(p, duration=1.0, reset=False)
assert np.max(np.abs(released[:, -12000:])) < 1e-5, 'Pattern disable left stuck notes'
results.append({'check': 'Internal pattern, mono, preset restoration, pattern disable'})

output = root / 'build/test-results'
output.mkdir(parents=True, exist_ok=True)
with AudioFile(str(output / 'reference-chord.wav'), 'w', 48000, 2) as file:
    file.write(render(instrument(excitation='Filtered Noise'), [note(57), note(60), note(64)], duration=2.0))
report = {'status': 'passed', 'plugin': str(bundle), 'seconds': time.monotonic()-start, 'results': results}
(output / 'host-smoke.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))
