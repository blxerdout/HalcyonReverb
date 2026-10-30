#include "Parameters.h"

namespace halcyon::params
{
namespace
{

juce::NormalisableRange<float> skewedRange (float min, float max, float centre)
{
    juce::NormalisableRange<float> range (min, max);
    range.setSkewForCentre (centre);
    return range;
}

juce::String formatTime (float ms)
{
    return ms >= 1000.0f ? juce::String (ms / 1000.0f, 2) + " s" : juce::String (juce::roundToInt (ms)) + " ms";
}

juce::String formatHz (float hz)
{
    return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz";
}

juce::String formatPercent (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }

std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* id, const char* name,
                                                      juce::NormalisableRange<float> range, float defaultValue,
                                                      std::function<juce::String (float)> format)
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, range, defaultValue,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [format] (float v, int) { return format (v); }));
}

} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (makeFloat (mix, "Mix", { 0.0f, 1.0f }, 0.35f, formatPercent));
    layout.add (makeFloat (preDelay, "Pre-Delay", skewedRange (0.0f, 500.0f, 60.0f), 10.0f, formatTime));
    layout.add (makeFloat (decay, "Decay", skewedRange (0.2f, 30.0f, 3.0f), 2.5f,
                           [] (float s) { return juce::String (s, s < 10.0f ? 2 : 1) + " s"; }));
    layout.add (makeFloat (size, "Size", { 0.0f, 1.0f }, 0.5f, formatPercent));
    layout.add (makeFloat (diffusion, "Diffusion", { 0.0f, 1.0f }, 0.75f, formatPercent));
    layout.add (makeFloat (modRate, "Mod Rate", skewedRange (0.05f, 5.0f, 0.8f), 0.6f,
                           [] (float hz) { return juce::String (hz, 2) + " Hz"; }));
    layout.add (makeFloat (modDepth, "Mod Depth", { 0.0f, 1.0f }, 0.4f, formatPercent));
    layout.add (makeFloat (highCut, "High Cut", skewedRange (1000.0f, 20000.0f, 6000.0f), 8000.0f, formatHz));
    layout.add (makeFloat (lowCut, "Low Cut", skewedRange (20.0f, 1000.0f, 150.0f), 80.0f, formatHz));
    layout.add (makeFloat (width, "Width", { 0.0f, 1.0f }, 1.0f, formatPercent));

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { mode, 1 }, "Mode", modeNames, 1));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { color, 1 }, "Color", colorNames, 0));

    return layout;
}

} // namespace halcyon::params
