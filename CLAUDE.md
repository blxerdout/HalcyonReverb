# Halcyon Reverb

Original multi-mode algorithmic reverb plugin (VST3, AU, Standalone) for macOS, built with JUCE + CMake.
Inspired by the character of Valhalla VintageVerb, but none of Valhalla's code, algorithms or UI is used or copied.

## Layout

- `Source/dsp/ReverbEngine.*`: the DSP, plain C++17 with **no JUCE dependency** (keep it that way so it stays testable offline)
- `Source/Parameters.*`: APVTS layout. Parameter IDs are saved in DAW sessions, so never rename them
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

1. JUCE isn't downloaded yet. Next step: `git submodule add https://github.com/juce-framework/JUCE.git JUCE`, check out the latest 8.x tag, then build and fix any compile errors in the wrapper/editor (they have never been compiled)
2. Not pushed to GitHub yet: no remote, and no gh CLI or SSH keys on this Mac. The user intends to publish through VS Code's Source Control → Publish Branch
3. Test in the user's DAWs: Ableton Live 12, FL Studio. Pro Tools would need AAX (Avid SDK), which is out of scope for now

## Conventions

- JUCE code style: `func (arg)` spacing, 4-space indent, braces on new lines
- Repo-local git identity is set to the user's account
