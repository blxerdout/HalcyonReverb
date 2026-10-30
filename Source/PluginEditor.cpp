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
constexpr int presetBarHeight = 44;
constexpr int margin = 20;
constexpr int userPresetIdOffset = 1000;

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
    setColour (juce::TextButton::buttonColourId, colours::track);
    setColour (juce::TextButton::buttonOnColourId, colours::accent);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::textColourOnId, colours::background);
    setColour (juce::AlertWindow::backgroundColourId, colours::panel);
    setColour (juce::AlertWindow::textColourId, colours::text);
    setColour (juce::AlertWindow::outlineColourId, colours::accent);
    setColour (juce::TextEditor::backgroundColourId, colours::track);
    setColour (juce::TextEditor::textColourId, colours::text);
    setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::focusedOutlineColourId, colours::accent);
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

HalcyonAudioProcessorEditor::HalcyonAudioProcessorEditor (HalcyonAudioProcessor& p)
    : AudioProcessorEditor (p), audioProcessor (p)
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

        knob.attachment = std::make_unique<SliderAttachment> (p.apvts, knobSpecs[i].id, knob.slider);
    }

    setUpChoice (mode, halcyon::params::mode, "MODE", halcyon::params::modeNames);
    setUpChoice (color, halcyon::params::color, "COLOR", halcyon::params::colorNames);

    presetBox.setTextWhenNothingSelected ("Default");
    presetBox.onChange = [this] { loadPresetById (presetBox.getSelectedId()); };
    addAndMakeVisible (presetBox);

    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    saveButton.onClick = [this] { savePreset(); };
    deleteButton.onClick = [this] { deletePreset(); };
    for (auto* button : { &prevButton, &nextButton, &saveButton, &deleteButton })
        addAndMakeVisible (button);

    lofiButton.setClickingTogglesState (true);
    lofiButton.setTooltip ("Filtered, fuzzy 8-bit version of the current reverb");
    addAndMakeVisible (lofiButton);
    lofiAttachment = std::make_unique<ButtonAttachment> (p.apvts, halcyon::params::lofi, lofiButton);

    refreshPresetList();

    setSize (640, 470);
}

HalcyonAudioProcessorEditor::~HalcyonAudioProcessorEditor()
{
    dialog.reset();
    setLookAndFeel (nullptr);
}

void HalcyonAudioProcessorEditor::refreshPresetList()
{
    presetBox.clear (juce::dontSendNotification);
    presetOrder.clear();
    userPresets = audioProcessor.presets.getUserPresets();

    auto* menu = presetBox.getRootMenu();
    for (int m = 0; m < halcyon::params::modeNames.size(); ++m)
    {
        juce::PopupMenu category;
        for (int i = m * halcyon::presetsPerMode; i < (m + 1) * halcyon::presetsPerMode; ++i)
        {
            category.addItem (i + 1, halcyon::factoryPresets[(size_t) i].name);
            presetOrder.push_back (i + 1);
        }
        menu->addSubMenu (halcyon::params::modeNames[m], category);
    }

    if (! userPresets.isEmpty())
    {
        menu->addSectionHeader ("User");
        for (int i = 0; i < userPresets.size(); ++i)
        {
            menu->addItem (userPresetIdOffset + i + 1, userPresets[i].getFileNameWithoutExtension());
            presetOrder.push_back (userPresetIdOffset + i + 1);
        }
    }

    // Show the current name, preferring a user preset when names clash.
    const auto current = audioProcessor.presets.getCurrentPresetName();
    int selected = 0;
    for (int id : presetOrder)
        if (presetBox.getItemText (presetBox.indexOfItemId (id)) == current)
            selected = id;

    if (selected != 0)
        presetBox.setSelectedId (selected, juce::dontSendNotification);
    else
        presetBox.setText (current, juce::dontSendNotification);

    deleteButton.setEnabled (selected > userPresetIdOffset);
}

void HalcyonAudioProcessorEditor::loadPresetById (int id)
{
    if (id > userPresetIdOffset)
    {
        const int index = id - userPresetIdOffset - 1;
        if (juce::isPositiveAndBelow (index, userPresets.size()))
            audioProcessor.presets.loadUser (userPresets[index]);
    }
    else if (id > 0)
    {
        audioProcessor.presets.loadFactory (id - 1);
    }

    deleteButton.setEnabled (id > userPresetIdOffset);
}

void HalcyonAudioProcessorEditor::stepPreset (int delta)
{
    if (presetOrder.empty())
        return;

    const auto it = std::find (presetOrder.begin(), presetOrder.end(), presetBox.getSelectedId());
    const int count = (int) presetOrder.size();
    const int pos = it == presetOrder.end() ? (delta > 0 ? -1 : 0) : (int) (it - presetOrder.begin());
    presetBox.setSelectedId (presetOrder[(size_t) ((pos + delta + count) % count)]);
}

void HalcyonAudioProcessorEditor::savePreset()
{
    const auto current = presetBox.getSelectedId() > userPresetIdOffset ? presetBox.getText() : juce::String();
    showDialog ("Save Preset", "Name this preset:", "Save", true, [this] (const juce::String& name)
    {
        if (audioProcessor.presets.saveUser (name))
            refreshPresetList();
        else
            showDialog ("Save Preset", "Couldn't save \"" + name + "\".", "OK", false, nullptr);
    });

    if (dialog != nullptr)
        dialog->getTextEditor ("name")->setText (current);
}

void HalcyonAudioProcessorEditor::deletePreset()
{
    const int index = presetBox.getSelectedId() - userPresetIdOffset - 1;
    if (! juce::isPositiveAndBelow (index, userPresets.size()))
        return;

    const auto file = userPresets[index];
    showDialog ("Delete Preset", "Delete \"" + file.getFileNameWithoutExtension() + "\"?", "Delete", false,
                [this, file] (const juce::String&)
                {
                    audioProcessor.presets.deleteUser (file);
                    refreshPresetList();
                });
}

void HalcyonAudioProcessorEditor::showDialog (const juce::String& title, const juce::String& message,
                                              const juce::String& confirmText, bool withNameField,
                                              std::function<void (const juce::String&)> onConfirm)
{
    // A child of the editor rather than a desktop window, so it can't end up behind the host.
    dialog = std::make_unique<juce::AlertWindow> (title, message, juce::MessageBoxIconType::NoIcon);
    if (withNameField)
        dialog->addTextEditor ("name", {});
    dialog->addButton (confirmText, 1, juce::KeyPress (juce::KeyPress::returnKey));
    if (onConfirm != nullptr)
        dialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    addAndMakeVisible (*dialog);
    dialog->setCentrePosition (getLocalBounds().getCentre());

    juce::Component::SafePointer<HalcyonAudioProcessorEditor> safeThis (this);
    dialog->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis, withNameField, onConfirm] (int result)
    {
        if (safeThis == nullptr)
            return;

        const auto name = withNameField ? safeThis->dialog->getTextEditorContents ("name") : juce::String();

        // The modal manager is still using the window, so delete it afterwards.
        juce::MessageManager::callAsync ([safeThis, window = safeThis->dialog.release()]
        {
            delete window;
        });

        if (result == 1 && onConfirm != nullptr)
            onConfirm (name);
    }), false);

    if (withNameField)
        dialog->getTextEditor ("name")->grabKeyboardFocus();
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
        audioProcessor.apvts, paramId, choice.box);
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

    auto body = getLocalBounds().withTrimmedTop (headerHeight + presetBarHeight).reduced (margin, 0).withTrimmedBottom (margin);
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

    auto bar = getLocalBounds().withTrimmedTop (headerHeight).removeFromTop (presetBarHeight).reduced (margin, 0)
                   .withTrimmedBottom (12);
    lofiButton.setBounds (bar.removeFromRight (80));
    bar.removeFromRight (24);
    prevButton.setBounds (bar.removeFromLeft (32));
    bar.removeFromLeft (4);
    nextButton.setBounds (bar.removeFromLeft (32));
    bar.removeFromLeft (8);
    deleteButton.setBounds (bar.removeFromRight (64));
    bar.removeFromRight (6);
    saveButton.setBounds (bar.removeFromRight (64));
    bar.removeFromRight (8);
    presetBox.setBounds (bar);

    if (dialog != nullptr)
        dialog->setCentrePosition (getLocalBounds().getCentre());

    auto body = getLocalBounds().withTrimmedTop (headerHeight + presetBarHeight).reduced (margin, 0).withTrimmedBottom (margin).reduced (10);
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
