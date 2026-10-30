#pragma once

#include "dsp/FactoryPresets.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace halcyon
{

// Factory presets come from FactoryPresets.h; user presets are XML files in
// ~/Library/Audio/Presets/creativespxce/Reblxerd. Must be used from the
// message thread.
class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState&);

    static juce::File getUserPresetDirectory();
    juce::Array<juce::File> getUserPresets() const;

    void loadFactory (int index);
    bool loadUser (const juce::File&);
    bool saveUser (const juce::String& name);
    bool deleteUser (const juce::File&);

    juce::String getCurrentPresetName() const;

    static constexpr const char* fileExtension = ".halcyonpreset";

private:
    void setCurrentPresetName (const juce::String&);
    void setParameter (const juce::String& id, float value);

    juce::AudioProcessorValueTreeState& apvts;

    JUCE_DECLARE_NON_COPYABLE (PresetManager)
};

} // namespace halcyon
