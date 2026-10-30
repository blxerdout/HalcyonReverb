# Halcyon Reverb

Original multi-mode algorithmic reverb plugin (VST3, AU, Standalone) for macOS, built with JUCE + CMake.
Inspired by the character of Valhalla VintageVerb, but none of Valhalla's code, algorithms or UI is used or copied.

## Layout

- `Source/dsp/ReverbEngine.*`: the DSP, plain C++17 with **no JUCE dependency** (keep it that way so it stays testable offline)
- `Source/dsp/FactoryPresets.h`: 50 factory presets (10 per Mode, basic to experimental), JUCE-free. Each group of ten must match `modeNames` order; the tests check this
- `Source/Parameters.*`: APVTS layout. Parameter IDs are saved in DAW sessions, so never rename them
- `Source/PresetManager.*`: factory/user preset loading; user presets are `.halcyonpreset` XML in `~/Library/Audio/Presets/creativespxce/Halcyon Reverb`. Current preset name is the `presetName` property on the APVTS state
- `Source/PluginProcessor.*`, `Source/PluginEditor.*`: JUCE wrapper and UI
- `tests/EngineTests.cpp`: JUCE-free checks (stability, RT60 vs Decay, L/R correlation, CPU). Pass a folder arg to write IR WAVs

## Build

CMake and Ninja were installed via pip at `~/Library/Python/3.14/bin` (no Homebrew, no Xcode; only Command Line Tools).

```bash
export PATH="$HOME/Library/Python/3.14/bin:$PATH"
cmake --preset release && cmake --build --preset release && ctest --preset release
```

Without `JUCE/`, CMake builds only the engine and tests. `COPY_PLUGIN_AFTER_BUILD` installs to `~/Library/Audio/Plug-Ins`.

## Status and next steps

1. JUCE is a submodule pinned to tag `8.0.16-security` (official security patch on top of 8.0.15). Plugin builds warning-free as a universal binary, `auval -v aufx Hlcy Cspx` passes, and the standalone app launches. Not yet tested in a DAW
2. Published at https://github.com/blxerdout/HalcyonReverb (`origin`, HTTPS via VS Code sign-in; no gh CLI or SSH keys on this Mac). Clone with `--recursive` or run `git submodule update --init --recursive` to fetch JUCE
3. Presets and Lofi mode added (Oct 30 2026). `getNumPrograms` stays at 1 on purpose: exposing factory presets as host programs risks hosts calling `setCurrentProgram` after restoring a session
4. Test in the user's DAWs: Ableton Live 12, FL Studio. Pro Tools would need AAX (Avid SDK), which is out of scope for now

## Conventions

- JUCE code style: `func (arg)` spacing, 4-space indent, braces on new lines
- Repo-local git identity is set to the user's account
