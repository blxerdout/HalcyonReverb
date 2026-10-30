#include "PluginEditor.h"
#include "Parameters.h"

namespace
{

namespace colours
{
const juce::Colour background { 0xff15171c };
const juce::Colour panel { 0xff1e2128 };
const juce::Colour track { 0xff2c313b };
const juce::Colour accent { 0xffe3a45a };
const juce::Colour text { 0xffd9dce2 };
const juce::Colour dimText { 0xff8a909c };
} // namespace colours

struct KnobSpec
{
    const char* id;
    const char* name;
};

// Two rows of five: input/space on top, tone/motion/output below.
constexpr std::array<KnobSpec, 10> knobSpecs {{
    { halcyon::params::preDelay, "Pre-Delay" },
    { halcyon::params::size, "Size" },
    { halcyon::params::decay, "Decay" },
    { halcyon::params::diffusion, "Diffusion" },
    { halcyon::params::mix, "Mix" },
    { halcyon::params::lowCut, "Low Cut" },
    { halcyon::params::highCut, "High Cut" },
    { halcyon::params::modRate, "Mod Rate" },
    { halcyon::params::modDepth, "Mod Depth" },
    { halcyon::params::width, "Width" },
}};

constexpr int headerHeight = 64;
constexpr int margin = 20;

} // namespace

HalcyonLookAndFeel::HalcyonLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::dimText);
    setColour (juce::ComboBox::backgroundColourId, colours::track);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::arrowColourId, colours::accent);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, colours::text);
}

void HalcyonLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                           float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const float stroke = 3.5f;
    const float arcRadius = radius - stroke;

    juce::Path background;
    background.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::track);
    g.strokePath (background, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
    g.setColour (colours::accent);
    g.strokePath (value, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float bodyRadius = arcRadius - 7.0f;
    g.setColour (colours::panel.brighter (0.15f));
    g.fillEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);

    const auto tip = centre.getPointOnCircumference (bodyRadius * 0.8f, angle);
    const auto base = centre.getPointOnCircumference (bodyRadius * 0.25f, angle);
    g.setColour (colours::text);
    g.drawLine ({ base, tip }, 2.5f);
}

HalcyonAudioProcessorEditor::HalcyonAudioProcessorEditor (HalcyonAudioProcessor& processor)
    : AudioProcessorEditor (processor)
{
    setLookAndFeel (&lookAndFeel);

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& knob = knobs[i];
        knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
        addAndMakeVisible (knob.slider);

        knob.label.setText (knobSpecs[i].name, juce::dontSendNotification);
        knob.label.setJustificationType (juce::Justification::centred);
        knob.label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        addAndMakeVisible (knob.label);

        knob.attachment = std::make_unique<SliderAttachment> (processor.apvts, knobSpecs[i].id, knob.slider);
    }

    setUpChoice (mode, halcyon::params::mode, "MODE", halcyon::params::modeNames);
    setUpChoice (color, halcyon::params::color, "COLOR", halcyon::params::colorNames);

    setSize (640, 420);
}

HalcyonAudioProcessorEditor::~HalcyonAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void HalcyonAudioProcessorEditor::setUpChoice (Choice& choice, const char* paramId, const juce::String& name,
                                               const juce::StringArray& items)
{
    // Items must exist before the attachment syncs the box to the parameter.
    choice.box.addItemList (items, 1);
    addAndMakeVisible (choice.box);

    choice.label.setText (name, juce::dontSendNotification);
    choice.label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    choice.label.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (choice.label);

    choice.attachment = std::make_unique<ComboBoxAttachment> (
        dynamic_cast<HalcyonAudioProcessor&> (processor).apvts, paramId, choice.box);
}

void HalcyonAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (margin, 0);
    g.setColour (colours::text);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("HALCYON", header, juce::Justification::centredLeft);

    g.setColour (colours::accent);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("REVERB", header.withTrimmedLeft (124), juce::Justification::centredLeft);

    auto body = getLocalBounds().withTrimmedTop (headerHeight).reduced (margin, 0).withTrimmedBottom (margin);
    g.setColour (colours::panel);
    g.fillRoundedRectangle (body.toFloat(), 10.0f);
}

void HalcyonAudioProcessorEditor::resized()
{
    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (margin, 16);
    auto placeChoice = [&header] (Choice& choice)
    {
        choice.box.setBounds (header.removeFromRight (110));
        choice.label.setBounds (header.removeFromRight (56));
        header.removeFromRight (12);
    };
    placeChoice (color);
    placeChoice (mode);

    auto body = getLocalBounds().withTrimmedTop (headerHeight).reduced (margin, 0).withTrimmedBottom (margin).reduced (10);
    const int rowHeight = body.getHeight() / 2;
    const int columns = 5;

    for (int row = 0; row < 2; ++row)
    {
        auto rowArea = body.removeFromTop (rowHeight);
        const int cellWidth = rowArea.getWidth() / columns;
        for (int col = 0; col < columns; ++col)
        {
            auto& knob = knobs[(size_t) (row * columns + col)];
            auto cell = rowArea.removeFromLeft (cellWidth).reduced (4);
            knob.label.setBounds (cell.removeFromTop (20));
            knob.slider.setBounds (cell);
        }
    }
}
