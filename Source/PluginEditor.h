#pragma once

#include "PluginProcessor.h"
#include "WaveformView.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Rotary slider that lights up while it's being turned, then fades back down.
class NeonKnob final : public juce::Slider, private juce::Timer
{
public:
    NeonKnob();

    float getGlow() const { return glow; }
    juce::Colour colour;

    void valueChanged() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void wake();
    void timerCallback() override;

    float glow = 0.0f;
    int holdFrames = 0;
};

class NeonLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    NeonLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown, int buttonX, int buttonY,
                       int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isHighlighted, bool isDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    juce::Font getPopupMenuFont() override;

    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowFont() override;
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
        NeonKnob slider;
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
    void drawTitle (juce::Graphics&);
    void showVisual (bool visual);
    juce::Rectangle<int> getBodyArea() const;

    void refreshPresetList();
    void loadPresetById (int id);
    void stepPreset (int delta);
    void savePreset();
    void deletePreset();
    void showDialog (const juce::String& title, const juce::String& message, const juce::String& confirmText,
                     bool withNameField, std::function<void (const juce::String&)> onConfirm);

    HalcyonAudioProcessor& audioProcessor;
    NeonLookAndFeel lookAndFeel;
    std::array<Knob, 10> knobs;
    Choice mode, color;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" }, deleteButton { "DELETE" };
    juce::Array<juce::File> userPresets;
    std::vector<int> presetOrder; // combo IDs in menu order, for the arrows

    juce::TextButton knobsTab { "KNOBS" }, visualTab { "VISUAL" };
    WaveformView waveform;

    juce::TextButton lofiButton { "LOFI" };
    std::unique_ptr<ButtonAttachment> lofiAttachment;

    std::unique_ptr<juce::AlertWindow> dialog;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HalcyonAudioProcessorEditor)
};
