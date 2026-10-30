#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace halcyon::params
{

// Parameter IDs are saved in DAW sessions. Never rename one after release.
inline constexpr const char* mix = "mix";
inline constexpr const char* preDelay = "predelay";
inline constexpr const char* decay = "decay";
inline constexpr const char* size = "size";
inline constexpr const char* diffusion = "diffusion";
inline constexpr const char* modRate = "modrate";
inline constexpr const char* modDepth = "moddepth";
inline constexpr const char* highCut = "highcut";
inline constexpr const char* lowCut = "lowcut";
inline constexpr const char* width = "width";
inline constexpr const char* mode = "mode";
inline constexpr const char* color = "color";
inline constexpr const char* lofi = "lofi";

inline const juce::StringArray modeNames { "Plate", "Hall", "Room", "Chamber", "Cathedral" };
inline const juce::StringArray colorNames { "Modern", "Vintage", "Dark" };

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

} // namespace halcyon::params
