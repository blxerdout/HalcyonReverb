#include "PluginEditor.h"
#include "Parameters.h"
#include "Theme.h"

using namespace theme;

namespace
{

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

constexpr int headerHeight = 70;
constexpr int presetBarHeight = 44;
constexpr int tabBarHeight = 30;
constexpr int footerHeight = 26;
constexpr int margin = 20;
constexpr int userPresetIdOffset = 1000;

// The layout is built for the default size and only grows from there.
constexpr int defaultWidth = 640, defaultHeight = 530;
constexpr int maxWidth = 1920, maxHeight = 1440;

} // namespace

//==============================================================================
NeonKnob::NeonKnob()
    : juce::Slider (RotaryHorizontalVerticalDrag, TextBoxBelow)
{
}

void NeonKnob::valueChanged()
{
    holdFrames = 8; // keep lit between the steps of a slow drag
    wake();
}

void NeonKnob::mouseDown (const juce::MouseEvent& e)
{
    juce::Slider::mouseDown (e);
    wake();
}

void NeonKnob::wake()
{
    if (! isTimerRunning())
        startTimerHz (60);
}

void NeonKnob::timerCallback()
{
    const bool active = isMouseButtonDown() || holdFrames > 0;
    if (holdFrames > 0)
        --holdFrames;

    if (active)
        glow += 0.35f * (1.0f - glow);
    else
        glow *= 0.93f;

    if (! active && glow < 0.005f)
    {
        glow = 0.0f;
        stopTimer();
    }

    repaint();
}

//==============================================================================
NeonLookAndFeel::NeonLookAndFeel()
{
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::dimText);
    setColour (juce::ComboBox::backgroundColourId, colours::panel);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::arrowColourId, colours::cyan);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::headerTextColourId, colours::magenta);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::cyan.withAlpha (0.2f));
    setColour (juce::PopupMenu::highlightedTextColourId, colours::cyan);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::AlertWindow::backgroundColourId, colours::panel);
    setColour (juce::AlertWindow::textColourId, colours::text);
    setColour (juce::AlertWindow::outlineColourId, colours::magenta);
    setColour (juce::TextEditor::backgroundColourId, colours::track);
    setColour (juce::TextEditor::textColourId, colours::text);
    setColour (juce::TextEditor::highlightColourId, colours::cyan.withAlpha (0.3f));
    setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::focusedOutlineColourId, colours::cyan);
    setColour (juce::CaretComponent::caretColourId, colours::cyan);
}

void NeonLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                        float startAngle, float endAngle, juce::Slider& slider)
{
    const auto* knob = dynamic_cast<NeonKnob*> (&slider);
    const float glow = knob != nullptr ? knob->getGlow() : 0.0f;
    const auto neon = knob != nullptr ? knob->colour : colours::cyan;

    // Leave room for the bloom, which spreads past the arc.
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (10.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const float arcRadius = radius - 3.0f;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::track);
    g.strokePath (track, juce::PathStrokeType (3.0f));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
    strokeGlow (g, value, neon, 0.3f + 1.2f * glow);

    // The arc itself is segmented like an LED meter.
    juce::Path segments;
    const float dashes[] { 4.0f, 2.5f };
    juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt)
        .createDashedStroke (segments, value, dashes, 2);
    g.setColour (neon.interpolatedWith (juce::Colours::white, 0.45f * glow));
    g.fillPath (segments);

    // Body: dark disc with a faint neon rim that brightens while turning.
    const float bodyRadius = arcRadius - 9.0f;
    const auto body = juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre);
    g.setGradientFill (juce::ColourGradient (colours::track.brighter (0.15f), centre.x, body.getY(),
                                             colours::panel, centre.x, body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (neon.withAlpha (0.2f + 0.5f * glow));
    g.drawEllipse (body, 1.0f);

    juce::Path pointer;
    pointer.startNewSubPath (centre.getPointOnCircumference (bodyRadius * 0.3f, angle));
    pointer.lineTo (centre.getPointOnCircumference (bodyRadius * 0.85f, angle));
    strokeGlow (g, pointer, neon, 0.2f + 1.1f * glow);
    g.setColour (neon.interpolatedWith (juce::Colours::white, 0.3f + 0.5f * glow));
    g.strokePath (pointer, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Label* NeonLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    const auto* knob = dynamic_cast<NeonKnob*> (&slider);
    const auto neon = knob != nullptr ? knob->colour : colours::cyan;

    label->setFont (mono (12.0f, true));
    label->setColour (juce::Label::textColourId, neon);
    label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::outlineWhenEditingColourId, neon);
    label->setColour (juce::TextEditor::textColourId, colours::text);
    label->setColour (juce::TextEditor::backgroundColourId, colours::track);
    return label;
}

void NeonLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int,
                                    juce::ComboBox& box)
{
    const auto path = chamfered (juce::Rectangle<float> ((float) width, (float) height).reduced (1.0f), 6.0f);
    g.setColour (colours::panel);
    g.fillPath (path);
    g.setColour (colours::cyan.withAlpha (box.isPopupActive() ? 0.9f : 0.45f));
    g.strokePath (path, juce::PathStrokeType (1.2f));

    juce::Path arrow;
    const float cx = (float) width - 16.0f, cy = (float) height * 0.5f;
    arrow.startNewSubPath (cx - 4.0f, cy - 2.0f);
    arrow.lineTo (cx, cy + 2.0f);
    arrow.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (colours::cyan);
    g.strokePath (arrow, juce::PathStrokeType (1.5f));
}

juce::Font NeonLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return mono (13.0f);
}

void NeonLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                            bool isHighlighted, bool isDown)
{
    const auto path = chamfered (button.getLocalBounds().toFloat().reduced (1.5f), 6.0f);
    const bool on = button.getToggleState();
    const bool isTab = button.getProperties().contains ("tab");
    const auto neon = on && ! isTab ? colours::magenta : colours::cyan;
    const float enabled = button.isEnabled() ? 1.0f : 0.35f;

    g.setColour (neon.withAlpha ((on ? 0.25f : isDown ? 0.2f : isHighlighted ? 0.1f : 0.03f) * enabled));
    g.fillPath (path);
    if (on)
    {
        g.setColour (neon.withAlpha (0.3f));
        g.strokePath (path, juce::PathStrokeType (5.0f));
    }
    g.setColour (neon.withAlpha ((on || isHighlighted ? 0.95f : 0.45f) * enabled));
    g.strokePath (path, juce::PathStrokeType (1.2f));
}

juce::Font NeonLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return mono (12.0f, true);
}

void NeonLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (colours::panel);
    g.setColour (colours::cyan.withAlpha (0.4f));
    g.drawRect (0, 0, width, height);
}

juce::Font NeonLookAndFeel::getPopupMenuFont()
{
    return mono (14.0f);
}

juce::Font NeonLookAndFeel::getAlertWindowTitleFont() { return mono (15.0f, true); }
juce::Font NeonLookAndFeel::getAlertWindowMessageFont() { return mono (13.0f); }
juce::Font NeonLookAndFeel::getAlertWindowFont() { return mono (13.0f); }

void NeonLookAndFeel::drawCornerResizer (juce::Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging)
{
    const auto neon = isMouseDragging ? colours::magenta : colours::cyan;
    const float alpha = isMouseOver || isMouseDragging ? 0.95f : 0.45f;
    const float size = (float) juce::jmin (w, h);

    juce::Path grip;
    for (float inset : { 0.3f, 0.6f })
    {
        grip.startNewSubPath ((float) w - size * (1.0f - inset), (float) h - 3.0f);
        grip.lineTo ((float) w - 3.0f, (float) h - size * (1.0f - inset));
    }

    if (isMouseDragging)
        strokeGlow (g, grip, neon, 0.6f);
    g.setColour (neon.withAlpha (alpha));
    g.strokePath (grip, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
HalcyonAudioProcessorEditor::HalcyonAudioProcessorEditor (HalcyonAudioProcessor& p)
    : AudioProcessorEditor (p), audioProcessor (p), waveform (p)
{
    setLookAndFeel (&lookAndFeel);

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& knob = knobs[i];
        knob.slider.colour = i < knobs.size() / 2 ? colours::cyan : colours::magenta;
        addAndMakeVisible (knob.slider);
        // After the colour is set, so the text box picks it up.
        knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);

        knob.label.setText (juce::String (knobSpecs[i].name).toUpperCase(), juce::dontSendNotification);
        knob.label.setJustificationType (juce::Justification::centred);
        knob.label.setFont (mono (11.0f, true));
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
    // The glow around LOFI is drawn by the editor, outside the button's bounds.
    lofiButton.onStateChange = [this] { repaint (lofiButton.getBounds().expanded (12)); };
    addAndMakeVisible (lofiButton);
    lofiAttachment = std::make_unique<ButtonAttachment> (p.apvts, halcyon::params::lofi, lofiButton);

    for (auto* tab : { &knobsTab, &visualTab })
    {
        tab->getProperties().set ("tab", true);
        tab->setColour (juce::TextButton::textColourOffId, colours::dimText);
        tab->setClickingTogglesState (true);
        tab->setRadioGroupId (1);
        addAndMakeVisible (tab);
    }
    // Turning one tab on turns the other off and fires its onClick too, so only react to "on".
    knobsTab.onClick = [this] { if (knobsTab.getToggleState()) showVisual (false); };
    visualTab.onClick = [this] { if (visualTab.getToggleState()) showVisual (true); };
    addChildComponent (waveform);

    refreshPresetList();

    // Resizable from the corner. The size lives in the plugin state so the
    // window reopens at the size it was left at.
    // Read before setResizeLimits, whose resize would overwrite the saved size.
    const int savedWidth = p.apvts.state.getProperty ("editorWidth", defaultWidth);
    const int savedHeight = p.apvts.state.getProperty ("editorHeight", defaultHeight);
    setResizable (true, true);
    setResizeLimits (defaultWidth, defaultHeight, maxWidth, maxHeight);
    setSize (juce::jlimit (defaultWidth, maxWidth, savedWidth), juce::jlimit (defaultHeight, maxHeight, savedHeight));
    showVisual (p.apvts.state.getProperty ("view").toString() == "visual");
}

HalcyonAudioProcessorEditor::~HalcyonAudioProcessorEditor()
{
    dialog.reset();
    setLookAndFeel (nullptr);
}

void HalcyonAudioProcessorEditor::showVisual (bool visual)
{
    (visual ? visualTab : knobsTab).setToggleState (true, juce::dontSendNotification);
    waveform.setVisible (visual);
    for (auto& knob : knobs)
    {
        knob.slider.setVisible (! visual);
        knob.label.setVisible (! visual);
    }

    // Kept in the plugin state so the editor reopens on the same tab.
    audioProcessor.apvts.state.setProperty ("view", visual ? "visual" : "knobs", nullptr);
    repaint();
}

void HalcyonAudioProcessorEditor::setUpChoice (Choice& choice, const char* paramId, const juce::String& name,
                                               const juce::StringArray& items)
{
    // Items must exist before the attachment syncs the box to the parameter.
    choice.box.addItemList (items, 1);
    addAndMakeVisible (choice.box);

    choice.label.setText (name, juce::dontSendNotification);
    choice.label.setFont (mono (11.0f, true));
    choice.label.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (choice.label);

    choice.attachment = std::make_unique<ComboBoxAttachment> (audioProcessor.apvts, paramId, choice.box);
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

juce::Rectangle<int> HalcyonAudioProcessorEditor::getBodyArea() const
{
    return getLocalBounds()
        .withTrimmedTop (headerHeight + presetBarHeight + tabBarHeight)
        .withTrimmedBottom (footerHeight)
        .reduced (margin, 0);
}

void HalcyonAudioProcessorEditor::drawTitle (juce::Graphics& g)
{
    const auto header = getLocalBounds().removeFromTop (headerHeight).reduced (margin, 0);
    const float baseline = (float) header.getCentreY() + 12.0f;

    juce::GlyphArrangement glyphs;
   #if JUCE_WINDOWS
    // Avenir isn't on Windows; Bahnschrift (Windows 10 and later) is the closest condensed face.
    const juce::Font titleFont (juce::FontOptions ("Bahnschrift SemiBold Condensed", 38.0f, juce::Font::plain));
   #else
    const juce::Font titleFont (juce::FontOptions ("Avenir Next Condensed", 36.0f, juce::Font::bold));
   #endif
    glyphs.addLineOfText (titleFont.withExtraKerningFactor (0.06f), "REBLXERD", (float) header.getX(), baseline);
    juce::Path title;
    glyphs.createPath (title);

    // Magenta bloom, a cyan ghost knocked slightly out of register, then the letters.
    strokeGlow (g, title, colours::magenta, 0.6f);
    g.setColour (colours::cyan.withAlpha (0.7f));
    g.fillPath (title, juce::AffineTransform::translation (-2.0f, 0.0f));
    g.setColour (colours::magenta.interpolatedWith (juce::Colours::white, 0.35f));
    g.fillPath (title);

    g.setColour (colours::cyan);
    g.setFont (mono (10.0f, true));
    g.drawText ("by CreativeSpxce", juce::Rectangle<float> (title.getBounds().getRight() + 14.0f, baseline - 11.0f, 120.0f, 12.0f),
                juce::Justification::bottomLeft);
}

void HalcyonAudioProcessorEditor::paint (juce::Graphics& g)
{
    const int w = getWidth(), h = getHeight();

    g.setGradientFill (juce::ColourGradient (colours::background, 0.0f, 0.0f, colours::horizon, 0.0f, (float) h, false));
    g.fillAll();

    g.setColour (colours::cyan.withAlpha (0.035f));
    for (int x = 0; x < w; x += 24)
        g.drawVerticalLine (x, 0.0f, (float) h);
    for (int y = 0; y < h; y += 24)
        g.drawHorizontalLine (y, 0.0f, (float) w);

    g.setColour (juce::Colours::black.withAlpha (0.22f));
    for (int y = 0; y < h; y += 3)
        g.fillRect (0, y, w, 1);

    drawTitle (g);

    // Knob panel with bright brackets on its two square corners.
    const auto body = getBodyArea().toFloat();
    const auto panelPath = chamfered (body, 16.0f);
    g.setColour (colours::panel.withAlpha (0.88f));
    g.fillPath (panelPath);
    g.setColour (colours::cyan.withAlpha (0.28f));
    g.strokePath (panelPath, juce::PathStrokeType (1.0f));

    juce::Path brackets;
    brackets.startNewSubPath (body.getRight() - 26.0f, body.getY());
    brackets.lineTo (body.getRight(), body.getY());
    brackets.lineTo (body.getRight(), body.getY() + 26.0f);
    brackets.startNewSubPath (body.getX(), body.getBottom() - 26.0f);
    brackets.lineTo (body.getX(), body.getBottom());
    brackets.lineTo (body.getX() + 26.0f, body.getBottom());
    g.setColour (colours::cyan);
    g.strokePath (brackets, juce::PathStrokeType (2.0f));

    if (! waveform.isVisible())
    {
        g.setColour (colours::magenta.withAlpha (0.18f));
        g.drawHorizontalLine ((int) body.getCentreY(), body.getX() + 24.0f, body.getRight() - 24.0f);
    }

    if (lofiButton.getToggleState())
        strokeGlow (g, chamfered (lofiButton.getBounds().toFloat().reduced (1.5f), 6.0f), colours::magenta, 1.0f);

    auto footer = getLocalBounds().removeFromBottom (footerHeight).reduced (margin, 0);
    g.setFont (mono (10.0f));
    g.setColour (colours::dimText);
    g.drawText ("// REBLXERD v" JucePlugin_VersionString " :: ALGORITHMIC REVERB", footer, juce::Justification::centredLeft);
    g.drawText ("CREATIVESPXCE", footer, juce::Justification::centredRight);
}

void HalcyonAudioProcessorEditor::resized()
{
    audioProcessor.apvts.state.setProperty ("editorWidth", getWidth(), nullptr);
    audioProcessor.apvts.state.setProperty ("editorHeight", getHeight(), nullptr);

    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (margin, 18);
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
    deleteButton.setBounds (bar.removeFromRight (70));
    bar.removeFromRight (6);
    saveButton.setBounds (bar.removeFromRight (64));
    bar.removeFromRight (8);
    presetBox.setBounds (bar);

    auto tabs = getLocalBounds().withTrimmedTop (headerHeight + presetBarHeight).removeFromTop (tabBarHeight)
                    .reduced (margin, 0).withTrimmedBottom (8);
    knobsTab.setBounds (tabs.removeFromLeft (84));
    tabs.removeFromLeft (6);
    visualTab.setBounds (tabs.removeFromLeft (84));

    if (dialog != nullptr)
        dialog->setCentrePosition (getLocalBounds().getCentre());

    waveform.setBounds (getBodyArea().reduced (6, 10));

    auto body = getBodyArea().reduced (12);
    const int rowHeight = body.getHeight() / 2;
    const int columns = 5;

    for (int row = 0; row < 2; ++row)
    {
        auto rowArea = body.removeFromTop (rowHeight);
        const int cellWidth = rowArea.getWidth() / columns;
        for (int col = 0; col < columns; ++col)
        {
            auto& knob = knobs[(size_t) (row * columns + col)];
            auto cell = rowArea.removeFromLeft (cellWidth).reduced (2);
            knob.label.setBounds (cell.removeFromTop (18));
            knob.slider.setBounds (cell);
        }
    }
}
