# JUCE Desktop Timbre Adapter v2

This is a clean ASCII-only rebuild of the desktop prototype.

## Purpose

Load a short audio sample (1 to 15 seconds), analyse a few timbre features locally, and map the result to a synth-style panel that can be used as a starting point for recreating the sound in FL Studio or similar synths.

## Why this v2 folder exists

The first prototype used Chinese UI strings directly inside C++ source files. On some Windows / Visual Studio setups this can trigger source-encoding issues during compilation. This v2 folder keeps source files ASCII-only and also enables UTF-8 explicitly for MSVC.

## Build

```bash
cmake -S . -B build
cmake --build build --config Release
```

## Features

- Local audio loading
- Play / stop preview
- Fast feature extraction
  - RMS
  - Zero crossing rate
  - Attack estimate
  - Stereo width
  - Spectral centroid
  - Spectral rolloff
  - Spectral flatness
  - Peakiness
  - Dominant pitch
- Parameter mapping
  - Cutoff
  - Resonance
  - Brightness
  - Drive
  - Unison
  - Detune
  - Harmonics
  - Noise
  - Stereo
  - FM depth
  - ADSR
- Piano preview for dominant pitch and simple harmonic hints
- Export JSON snapshot

## Folder layout

- `CMakeLists.txt`
- `Source/Main.cpp`
- `Source/MainComponent.h`
- `Source/MainComponent.cpp`
