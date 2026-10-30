#pragma once

// Shared colours and drawing helpers for the cyberpunk UI.

#include <juce_gui_basics/juce_gui_basics.h>

namespace theme
{

namespace colours
{
inline const juce::Colour background { 0xff07040d };
inline const juce::Colour horizon { 0xff1c0833 };
inline const juce::Colour panel { 0xff0f0919 };
inline const juce::Colour track { 0xff2a1d3d };
inline const juce::Colour cyan { 0xff00f0ff };
inline const juce::Colour magenta { 0xffff2bd6 };
inline const juce::Colour yellow { 0xfff8ff3a };
inline const juce::Colour text { 0xffe9e4ff };
inline const juce::Colour dimText { 0xff7d6f9e };
} // namespace colours

inline juce::FontOptions mono (float size, bool bold = false)
{
    return juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size,
                              bold ? juce::Font::bold : juce::Font::plain);
}

// Rectangle with its top-left and bottom-right corners cut off.
inline juce::Path chamfered (juce::Rectangle<float> r, float cut)
{
    juce::Path p;
    p.startNewSubPath (r.getX() + cut, r.getY());
    p.lineTo (r.getRight(), r.getY());
    p.lineTo (r.getRight(), r.getBottom() - cut);
    p.lineTo (r.getRight() - cut, r.getBottom());
    p.lineTo (r.getX(), r.getBottom());
    p.lineTo (r.getX(), r.getY() + cut);
    p.closeSubPath();
    return p;
}

// Cheap bloom: the path stroked wide and faint, then narrower and brighter.
inline void strokeGlow (juce::Graphics& g, const juce::Path& path, juce::Colour colour, float intensity)
{
    constexpr std::array<std::pair<float, float>, 3> layers {{ { 14.0f, 0.07f }, { 8.0f, 0.14f }, { 4.5f, 0.28f } }};
    for (auto [width, alpha] : layers)
    {
        g.setColour (colour.withAlpha (juce::jmin (1.0f, alpha * intensity)));
        g.strokePath (path, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

} // namespace theme
