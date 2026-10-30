#include "PresetManager.h"
#include "Parameters.h"

namespace halcyon
{
namespace
{
const juce::Identifier presetNameProperty { "presetName" };
} // namespace

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state)
    : apvts (state)
{
}

juce::File PresetManager::getUserPresetDirectory()
{
    const auto root = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                          .getChildFile ("Audio/Presets/creativespxce");
    const auto dir = root.getChildFile ("Reblxerd");

    // Carry over presets saved before the plugin was renamed.
    const auto oldDir = root.getChildFile ("Halcyon Reverb");
    if (oldDir.isDirectory() && ! dir.exists())
        oldDir.moveFileTo (dir);

    return dir;
}

juce::Array<juce::File> PresetManager::getUserPresets() const
{
    auto files = getUserPresetDirectory().findChildFiles (juce::File::findFiles, false,
                                                          juce::String ("*") + fileExtension);
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    return files;
}

void PresetManager::loadFactory (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) factoryPresets.size()))
        return;

    const auto& preset = factoryPresets[(size_t) index];
    const auto& r = preset.params;

    setParameter (params::mix, r.mix);
    setParameter (params::preDelay, r.preDelayMs);
    setParameter (params::decay, r.decaySeconds);
    setParameter (params::size, r.size);
    setParameter (params::diffusion, r.diffusion);
    setParameter (params::modRate, r.modRateHz);
    setParameter (params::modDepth, r.modDepth);
    setParameter (params::highCut, r.highCutHz);
    setParameter (params::lowCut, r.lowCutHz);
    setParameter (params::width, r.width);
    setParameter (params::mode, (float) r.mode);
    setParameter (params::color, (float) r.color);
    setParameter (params::lofi, r.lofi ? 1.0f : 0.0f);

    setCurrentPresetName (preset.name);
}

bool PresetManager::loadUser (const juce::File& file)
{
    const auto xml = juce::parseXML (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return false;

    // Go parameter by parameter so the host hears every change, and anything
    // the file predates falls back to its default.
    for (auto* param : apvts.processor.getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            const auto* child = xml->getChildByAttribute ("id", ranged->paramID);
            const float value = child != nullptr ? (float) child->getDoubleAttribute ("value")
                                                 : ranged->convertFrom0to1 (ranged->getDefaultValue());
            setParameter (ranged->paramID, value);
        }
    }

    setCurrentPresetName (file.getFileNameWithoutExtension());
    return true;
}

bool PresetManager::saveUser (const juce::String& name)
{
    const auto legalName = juce::File::createLegalFileName (name.trim());
    if (legalName.isEmpty())
        return false;

    const auto dir = getUserPresetDirectory();
    if (! dir.createDirectory())
        return false;

    setCurrentPresetName (legalName);
    const auto xml = apvts.copyState().createXml();
    return xml != nullptr && xml->writeTo (dir.getChildFile (legalName + fileExtension));
}

bool PresetManager::deleteUser (const juce::File& file)
{
    return file.isAChildOf (getUserPresetDirectory()) && file.deleteFile();
}

juce::String PresetManager::getCurrentPresetName() const
{
    return apvts.state.getProperty (presetNameProperty, "Default").toString();
}

void PresetManager::setCurrentPresetName (const juce::String& name)
{
    apvts.state.setProperty (presetNameProperty, name, nullptr);
}

void PresetManager::setParameter (const juce::String& id, float value)
{
    if (auto* param = apvts.getParameter (id))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (value));
        param->endChangeGesture();
    }
}

} // namespace halcyon
