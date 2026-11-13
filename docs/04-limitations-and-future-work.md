# Limitations and Future Development

Several areas could be developed further.

## Fractional Delay

Both linear and allpass fractional delay now use calibrated closed-loop tuning.
The independent host suite checks all 128 notes in both modes against a
±2-cent target. Very high linear-mode notes decay quickly because interpolation
removes energy; allpass mode preserves the sustain more effectively.

## Body Modelling

The simplified body resonator could be replaced by:

- convolution with measured instrument-body impulse responses;
- a bank of resonant filters;
- a more detailed physical body model.

## Excitation Modelling

The excitation stage could be extended to simulate different plucking
materials, gestures, and playing techniques.

## User Interface

The custom editor now follows the supplied reference, including the live scope,
peak meter and advanced controls. Future work includes testing it in the Windows
and macOS hosts listed in the reference and conducting listening evaluations.
