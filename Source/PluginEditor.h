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

    HalcyonLookAndFeel lookAndFeel;
    std::array<Knob, 10> knobs;
    Choice mode, color;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HalcyonAudioProcessorEditor)
};
