# Karplus–Strong Plucked-String Synthesiser

A polyphonic physical-modelling synthesiser developed with JUCE and based on an extended Karplus–Strong algorithm.

## Overview

This project implements a playable plucked-string audio plugin for use in a DAW. MIDI notes excite a delay-line feedback model that simulates the behaviour of a vibrating string.

The instrument is designed as a flexible synthetic plucked-string synthesiser rather than an exact emulation of a particular acoustic instrument.

## Features

- Polyphonic MIDI performance
- Karplus–Strong delay-line synthesis
- Adjustable brightness and damping
- Pick-position modelling
- Double-string detuning
- Stereo width control
- Simplified body resonance
- Automatic MIDI pattern generation
- Preset support

## Repository structure

- `Source/` — C++ source code
- `KarplusStrong_PluckedString.jucer` — JUCE project file
- `preset.RPL` — example REAPER preset

## Requirements

- JUCE
- A compatible C++ development environment
- A DAW supporting VST3 plugins for testing

## Building the project

1. Open `KarplusStrong_PluckedString.jucer` in Projucer.
2. Confirm that the JUCE module paths are configured correctly.
3. Save and open the generated IDE project.
4. Build the VST3 plugin.
5. Scan the generated plugin in a compatible DAW.

## Academic context

This project was originally developed for an Audio Programming assignment as part of the MSc Acoustics and Music Technology programme at the University of Edinburgh.

The repository contains the source code and project configuration. Compiled plugin binaries and assessment materials are not included.