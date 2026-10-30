#pragma once

#include "PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class HalcyonLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    HalcyonLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
};

class HalcyonAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit HalcyonAudioProcessorEditor (HalcyonAudioProcessor&);
    ~HalcyonAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    struct Choice
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<ComboBoxAttachment> attachment;
    };

    void setUpChoice (Choice&, const char* paramId, const juce::String& name, const juce::StringArray& items);

    void refreshPresetList();
    void loadPresetById (int id);
    void stepPreset (int delta);
    void savePreset();
    void deletePreset();
    void showDialog (const juce::String& title, const juce::String& message, const juce::String& confirmText,
                     bool withNameField, std::function<void (const juce::String&)> onConfirm);

    HalcyonAudioProcessor& audioProcessor;
    HalcyonLookAndFeel lookAndFeel;
    std::array<Knob, 10> knobs;
    Choice mode, color;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "Save" }, deleteButton { "Delete" };
    juce::Array<juce::File> userPresets;
    std::vector<int> presetOrder; // combo IDs in menu order, for the arrows

    juce::TextButton lofiButton { "LOFI" };
    std::unique_ptr<ButtonAttachment> lofiAttachment;

    std::unique_ptr<juce::AlertWindow> dialog;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HalcyonAudioProcessorEditor)
};
