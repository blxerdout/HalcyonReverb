# Reblxerd (formerly Halcyon Reverb)

Original multi-mode algorithmic reverb plugin (VST3, AU, Standalone) for macOS, built with JUCE + CMake.
Inspired by the character of Valhalla VintageVerb, but none of Valhalla's code, algorithms or UI is used or copied.

## Layout

- `Source/dsp/ReverbEngine.*`: the DSP, plain C++17 with **no JUCE dependency** (keep it that way so it stays testable offline)
- `Source/dsp/FactoryPresets.h`: 50 factory presets (10 per Mode, basic to experimental), JUCE-free. Each group of ten must match `modeNames` order; the tests check this
- `Source/Parameters.*`: APVTS layout. Parameter IDs are saved in DAW sessions, so never rename them
- `Source/PresetManager.*`: factory/user preset loading; user presets are `.halcyonpreset` XML in `~/Library/Audio/Presets/creativespxce/Reblxerd` (old `Halcyon Reverb` folder is moved there on first use). Current preset name is the `presetName` property on the APVTS state
- `Source/PluginProcessor.*`, `Source/PluginEditor.*`: JUCE wrapper and UI. Cyberpunk theme (`NeonLookAndFeel`); `NeonKnob` glows while turned. Editor is resizable (corner grip, 640x530 to 1920x1440); size is the `editorWidth`/`editorHeight` properties on the APVTS state, read before `setResizeLimits` (which resizes and would overwrite them)
- Shown name is `PRODUCT_NAME "Reblxerd"`; internal names (CMake target, `halcyon` namespace, plugin/manufacturer codes, bundle ID) deliberately unchanged so existing DAW sessions still load
- `Source/WaveformView.*`: Visual tab. Renders the IR with a private `ReverbEngine` at 16 kHz on a background thread (superseded renders abort early), maps it to dB, draws on a power-0.35 time axis; three drag handles set pre-delay/mix, size, decay/high cut. Tab choice is the `view` property on the APVTS state. Playhead is driven by `getActivity()`: `dsp/OnsetDetector.h` (JUCE-free, tested) runs on the input in `processBlock`, the view restarts on each new onset and runs on the wall clock, brightness follows `ReverbEngine::getLastWetPeak()`
- `Source/Theme.h`: shared colours and drawing helpers
- `docs/screenshot*.png`: rendered from the editor offscreen (no screen-recording permission here); regenerate after UI changes
- `packaging/`: `mac/build-dmg.sh` (pkg in a dmg; bundles marked non-relocatable, ad-hoc sealed unless `DEVELOPER_ID_*` is set) and `windows/Reblxerd.iss` (Inno Setup). `.github/workflows/build.yml` builds both on every push and makes a pre-release on `v*` tags. Windows can't be built on this Mac; CI is the only Windows build. Title font falls back to Bahnschrift on Windows
- `tests/EngineTests.cpp`: JUCE-free checks (stability, RT60 vs Decay, Size changes build-up with no gap before the tail, L/R correlation, CPU). Pass a folder arg to write IR WAVs

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
