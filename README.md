# Halcyon Reverb

A multi-mode algorithmic reverb for macOS (VST3, AU and a standalone app), built with JUCE.
It's an original design in the spirit of classic "vintage" digital reverbs: several
modes with distinct character, plus a colour control that shapes bandwidth, motion and grit.

## Controls

| Control | What it does |
| --- | --- |
| **Mode** | Plate, Hall, Room, Chamber, Cathedral. Each mode has its own delay network, density and early reflections |
| **Color** | Modern (full bandwidth, clean), Vintage (band-limited, more wobble, slight saturation), Dark (soft, woolly) |
| Pre-Delay | Gap before the reverb starts, 0 to 500 ms |
| Size | Scales the space; also changes the early reflection spacing |
| Decay | RT60, 0.2 to 30 s |
| Diffusion | How quickly transients smear into a smooth wash |
| Mix | Dry/wet. Use 100% when the plugin sits on a send/return track |
| Low Cut / High Cut | Input high-pass, and damping inside the tank (darker tails at lower settings) |
| Mod Rate / Mod Depth | Delay-line modulation for chorus-like movement and to break up metallic ringing |
| Width | Stereo width of the wet signal |

## How it works

`Source/dsp/ReverbEngine.*` is plain C++17 with no JUCE dependency:

1. Pre-delay, then a low-cut and colour-dependent band-limit filter
2. A tapped delay line for early reflections
3. Four series allpass diffusers
4. An 8-line feedback delay network with Hadamard mixing. Each line has a
   modulated, cubic-interpolated read, a damping lowpass and an in-loop allpass.
   The feedback gains are derived from the Decay setting so every line reaches
   −60 dB at the same time.
5. Orthogonal output taps for decorrelated left/right, then width and mix

The JUCE layer (`PluginProcessor`, `PluginEditor`, `Parameters`) only maps parameters
and draws the UI.

## Building

Requirements: Apple's command line tools (`xcode-select --install`), CMake 3.22+ and Ninja.

```bash
git submodule update --init --recursive
cmake --preset release
cmake --build --preset release
```

The build copies the plugin to `~/Library/Audio/Plug-Ins/VST3` and `~/Library/Audio/Plug-Ins/Components`.
Rescan plugins in your DAW to pick it up.

In VS Code, install the recommended extensions (CMake Tools, clangd, CodeLLDB), pick a preset
in the CMake panel and press Build.

## Tests

The engine tests build without JUCE. They check stability, measured RT60 against the
Decay setting, stereo decorrelation and CPU use:

```bash
ctest --preset release
```

Pass a folder to `engine_tests` to also write impulse responses as WAV files.

## Licence notes

JUCE 8 is dual-licensed: AGPLv3, or a JUCE commercial licence (which includes a free
Starter tier below a revenue limit). Check which one applies before you sell or
distribute closed-source builds.
