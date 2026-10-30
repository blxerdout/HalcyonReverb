#include "WaveformView.h"
#include "Parameters.h"
#include "Theme.h"

using namespace theme;
namespace p = halcyon::params;

namespace
{

constexpr double renderRate = 16000.0;
constexpr int bucketSamples = 8; // 0.5 ms at renderRate
constexpr float bucketSeconds = (float) bucketSamples / (float) renderRate;
constexpr float axisSeconds = 32.0f;
constexpr float axisPower = 0.35f; // stretches the first few hundred ms across the left of the plot
constexpr float floorDb = -75.0f; // the tail starts ~15 dB under the early reflections, so this ends it near the Decay time
constexpr float handleRadius = 7.0f;

bool sameParams (const halcyon::ReverbParams& a, const halcyon::ReverbParams& b)
{
    using juce::exactlyEqual;
    return exactlyEqual (a.mix, b.mix) && exactlyEqual (a.preDelayMs, b.preDelayMs)
           && exactlyEqual (a.decaySeconds, b.decaySeconds) && exactlyEqual (a.size, b.size)
           && exactlyEqual (a.diffusion, b.diffusion) && exactlyEqual (a.modRateHz, b.modRateHz)
           && exactlyEqual (a.modDepth, b.modDepth) && exactlyEqual (a.highCutHz, b.highCutHz)
           && exactlyEqual (a.lowCutHz, b.lowCutHz) && exactlyEqual (a.width, b.width) && a.mode == b.mode
           && a.color == b.color && a.lofi == b.lofi;
}

float toneFromHighCut (float hz) { return std::log (hz / 1000.0f) / std::log (20.0f); }
float highCutFromTone (float tone) { return 1000.0f * std::pow (20.0f, tone); }

} // namespace

WaveformView::WaveformView (HalcyonAudioProcessor& processorToUse)
    : juce::Thread ("Reblxerd waveform"), audioProcessor (processorToUse)
{
    shown = pending = audioProcessor.readParams();
    pendingVersion = 1;
    startThread();
    notify();
    startTimerHz (60);
}

WaveformView::~WaveformView()
{
    stopTimer();
    cancelPendingUpdate();
    signalThreadShouldExit();
    notify();
    stopThread (4000);
}

//==============================================================================
void WaveformView::run()
{
    int renderedVersion = 0;
    while (! threadShouldExit())
    {
        while (renderedVersion != pendingVersion.load() && ! threadShouldExit())
        {
            const int version = pendingVersion.load();
            halcyon::ReverbParams params;
            {
                const std::lock_guard<std::mutex> guard (lock);
                params = pending;
            }

            auto result = render (params, version);
            if (result.top.empty())
                continue; // superseded by newer settings part-way through

            {
                const std::lock_guard<std::mutex> guard (lock);
                latest = std::make_shared<const Envelope> (std::move (result));
            }
            renderedVersion = version;
            triggerAsyncUpdate();
        }

        wait (-1);
    }
}

WaveformView::Envelope WaveformView::render (halcyon::ReverbParams params, int version)
{
    params.mix = 1.0f;
    const float seconds = juce::jmin (axisSeconds, params.preDelayMs / 1000.0f + params.decaySeconds * 1.2f + 0.05f);
    const int numBuckets = (int) (seconds / bucketSeconds) + 1;

    halcyon::ReverbEngine engine;
    engine.prepare (renderRate);
    engine.setParams (params);
    engine.reset();

    std::vector<float> high ((size_t) numBuckets, 0.0f), low ((size_t) numBuckets, 0.0f);
    constexpr int block = 512;
    std::array<float, block> left {}, right {};
    float peak = 1.0e-9f;

    for (int start = 0; start < numBuckets * bucketSamples; start += block)
    {
        if (threadShouldExit() || pendingVersion.load() != version)
            return {};

        left.fill (0.0f);
        right.fill (0.0f);
        if (start == 0)
            left[0] = right[0] = 1.0f;
        engine.process (left.data(), right.data(), block);

        for (int i = 0; i < block; ++i)
        {
            const auto bucket = (size_t) ((start + i) / bucketSamples);
            if (bucket >= high.size())
                break;
            high[bucket] = juce::jmax (high[bucket], left[(size_t) i]);
            low[bucket] = juce::jmin (low[bucket], left[(size_t) i]);
            peak = juce::jmax (peak, std::abs (left[(size_t) i]));
        }
    }

    auto toLevel = [peak] (float v)
    {
        const float db = juce::Decibels::gainToDecibels (std::abs (v) / peak, floorDb);
        return juce::jlimit (0.0f, 1.0f, 1.0f - db / floorDb);
    };

    Envelope result;
    result.top.resize (high.size());
    result.bottom.resize (low.size());
    for (size_t i = 0; i < high.size(); ++i)
    {
        result.top[i] = toLevel (high[i]);
        result.bottom[i] = toLevel (low[i]);
    }
    return result;
}

void WaveformView::handleAsyncUpdate()
{
    rebuildColumns();
    repaint();
}

void WaveformView::resized()
{
    rebuildColumns();
}

void WaveformView::rebuildColumns()
{
    std::shared_ptr<const Envelope> env;
    {
        const std::lock_guard<std::mutex> guard (lock);
        env = latest;
    }

    columnTop.clear();
    columnBottom.clear();
    if (env == nullptr)
        return;

    const auto plot = getPlotArea();
    for (float x = plot.getX(); x <= plot.getRight(); x += 1.0f)
    {
        const auto first = (size_t) (xToTime (x) / bucketSeconds);
        const auto last = juce::jmax (first, (size_t) (xToTime (x + 1.0f) / bucketSeconds));
        float top = 0.0f, bottom = 0.0f;
        for (size_t b = first; b <= last && b < env->top.size(); ++b)
        {
            top = juce::jmax (top, env->top[b]);
            bottom = juce::jmax (bottom, env->bottom[b]);
        }
        columnTop.push_back (top);
        columnBottom.push_back (bottom);
    }
}

void WaveformView::timerCallback()
{
    if (! isShowing())
        return;

    const auto current = audioProcessor.readParams();
    if (! sameParams (current, shown))
    {
        shown = current;
        {
            const std::lock_guard<std::mutex> guard (lock);
            pending = current;
        }
        ++pendingVersion;
        notify();
    }

    // A scan line sweeps through the tail in real time, then waits a beat.
    const float tailEnd = shown.preDelayMs / 1000.0f + shown.decaySeconds;
    sweepSeconds += 1.0f / 60.0f;
    if (sweepSeconds > juce::jmin (tailEnd, 8.0f) + 0.6f)
        sweepSeconds = 0.0f;

    dragGlow += ((dragging != Handle::none ? 1.0f : 0.0f) - dragGlow) * 0.2f;

    repaint();
}

//==============================================================================
juce::Rectangle<float> WaveformView::getPlotArea() const
{
    return getLocalBounds().toFloat().withTrimmedTop (34.0f).withTrimmedBottom (24.0f).reduced (18.0f, 0.0f);
}

float WaveformView::timeToX (float seconds) const
{
    const auto plot = getPlotArea();
    const float norm = std::pow (juce::jlimit (0.0f, 1.0f, seconds / axisSeconds), axisPower);
    return plot.getX() + norm * plot.getWidth();
}

float WaveformView::xToTime (float x) const
{
    const auto plot = getPlotArea();
    const float norm = juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / plot.getWidth());
    return axisSeconds * std::pow (norm, 1.0f / axisPower);
}

juce::Point<float> WaveformView::getHandlePosition (Handle handle) const
{
    const auto plot = getPlotArea();
    const float pre = shown.preDelayMs / 1000.0f;
    const float centre = plot.getCentreY(), half = plot.getHeight() * 0.5f;

    switch (handle)
    {
        case Handle::onset: return { timeToX (pre), centre - amplitudeForMix (shown.mix) * half };
        case Handle::body:  return { timeToX (pre + 0.35f * shown.decaySeconds), plot.getBottom() - shown.size * plot.getHeight() };
        case Handle::tail:  return { timeToX (pre + shown.decaySeconds),
                                     plot.getBottom() - toneFromHighCut (shown.highCutHz) * plot.getHeight() };
        case Handle::none:  break;
    }
    return {};
}

WaveformView::Handle WaveformView::findHandle (juce::Point<float> position) const
{
    // Nearest handle wins, so a drag anywhere on the plot grabs something.
    Handle best = Handle::none;
    float bestDistance = std::numeric_limits<float>::max();
    for (auto handle : { Handle::onset, Handle::body, Handle::tail })
    {
        const float distance = getHandlePosition (handle).getDistanceFrom (position);
        if (distance < bestDistance)
        {
            best = handle;
            bestDistance = distance;
        }
    }
    return best;
}

std::vector<juce::RangedAudioParameter*> WaveformView::getParameters (Handle handle) const
{
    auto& apvts = audioProcessor.apvts;
    switch (handle)
    {
        case Handle::onset: return { apvts.getParameter (p::preDelay), apvts.getParameter (p::mix) };
        case Handle::body:  return { apvts.getParameter (p::size) };
        case Handle::tail:  return { apvts.getParameter (p::decay), apvts.getParameter (p::highCut) };
        case Handle::none:  break;
    }
    return {};
}

void WaveformView::setParameter (const char* id, float value)
{
    if (auto* param = audioProcessor.apvts.getParameter (id))
        param->setValueNotifyingHost (param->convertTo0to1 (value));
}

void WaveformView::dragTo (Handle handle, juce::Point<float> position)
{
    const auto plot = getPlotArea();
    const float pre = shown.preDelayMs / 1000.0f;
    const float centre = plot.getCentreY(), half = plot.getHeight() * 0.5f;
    const float fromBottom = juce::jlimit (0.0f, 1.0f, (plot.getBottom() - position.y) / plot.getHeight());

    switch (handle)
    {
        case Handle::onset:
            setParameter (p::preDelay, juce::jlimit (0.0f, 500.0f, xToTime (position.x) * 1000.0f));
            setParameter (p::mix, juce::jlimit (0.0f, 1.0f, ((centre - position.y) / half - 0.2f) / 0.8f));
            break;
        case Handle::body:
            setParameter (p::size, fromBottom);
            break;
        case Handle::tail:
            setParameter (p::decay, juce::jlimit (0.2f, 30.0f, xToTime (position.x) - pre));
            setParameter (p::highCut, highCutFromTone (fromBottom));
            break;
        case Handle::none:
            break;
    }
}

void WaveformView::mouseMove (const juce::MouseEvent& e)
{
    const auto handle = findHandle (e.position);
    hovered = getHandlePosition (handle).getDistanceFrom (e.position) < 24.0f ? handle : Handle::none;
    setMouseCursor (hovered == Handle::none ? juce::MouseCursor::CrosshairCursor
                                            : juce::MouseCursor::DraggingHandCursor);
}

void WaveformView::mouseExit (const juce::MouseEvent&)
{
    hovered = Handle::none;
}

void WaveformView::mouseDown (const juce::MouseEvent& e)
{
    dragging = findHandle (e.position);
    for (auto* param : getParameters (dragging))
        param->beginChangeGesture();
    dragTo (dragging, e.position);
}

void WaveformView::mouseDrag (const juce::MouseEvent& e)
{
    dragTo (dragging, e.position);
}

void WaveformView::mouseUp (const juce::MouseEvent&)
{
    for (auto* param : getParameters (dragging))
        param->endChangeGesture();
    dragging = Handle::none;
}

//==============================================================================
void WaveformView::paint (juce::Graphics& g)
{
    const auto plot = getPlotArea();
    const float centre = plot.getCentreY(), half = plot.getHeight() * 0.5f;
    const float amplitude = amplitudeForMix (shown.mix) * half;
    const float pre = shown.preDelayMs / 1000.0f;

    // Time grid.
    g.setFont (mono (9.0f));
    for (auto [seconds, label] : std::initializer_list<std::pair<float, const char*>> {
             { 0.1f, "100ms" }, { 0.5f, "500ms" }, { 1.0f, "1s" }, { 2.0f, "2s" },
             { 5.0f, "5s" }, { 10.0f, "10s" }, { 20.0f, "20s" } })
    {
        const float x = timeToX (seconds);
        g.setColour (colours::cyan.withAlpha (0.07f));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (colours::dimText);
        g.drawText (label, juce::Rectangle<float> (x - 30.0f, plot.getBottom() + 6.0f, 60.0f, 12.0f),
                    juce::Justification::centred);
    }
    g.setColour (colours::magenta.withAlpha (0.2f));
    g.drawHorizontalLine ((int) centre, plot.getX(), plot.getRight());

    // Waveform.
    if (! columnTop.empty())
    {
        juce::Path upper, lower;
        for (size_t i = 0; i < columnTop.size(); ++i)
        {
            const float x = plot.getX() + (float) i;
            const float yTop = centre - columnTop[i] * amplitude, yBottom = centre + columnBottom[i] * amplitude;
            if (i == 0)
            {
                upper.startNewSubPath (x, yTop);
                lower.startNewSubPath (x, yBottom);
            }
            else
            {
                upper.lineTo (x, yTop);
                lower.lineTo (x, yBottom);
            }
        }

        juce::Path fill (upper);
        for (size_t i = columnBottom.size(); i-- > 0;)
            fill.lineTo (plot.getX() + (float) i, centre + columnBottom[i] * amplitude);
        fill.closeSubPath();

        const auto tint = shown.lofi ? colours::magenta : colours::cyan;
        g.setGradientFill (juce::ColourGradient (tint.withAlpha (0.55f), plot.getX(), 0.0f,
                                                 colours::magenta.withAlpha (0.35f), plot.getRight(), 0.0f, false));
        g.fillPath (fill);
        strokeGlow (g, upper, tint, 0.35f + 0.5f * dragGlow);
        strokeGlow (g, lower, tint, 0.35f + 0.5f * dragGlow);
        g.setColour (tint.interpolatedWith (juce::Colours::white, 0.3f));
        g.strokePath (upper, juce::PathStrokeType (1.0f));
        g.strokePath (lower, juce::PathStrokeType (1.0f));
    }

    // Scan line.
    if (sweepSeconds <= pre + shown.decaySeconds)
    {
        const float x = timeToX (sweepSeconds);
        g.setGradientFill (juce::ColourGradient (colours::text.withAlpha (0.0f), x - 18.0f, 0.0f,
                                                 colours::text.withAlpha (0.18f), x, 0.0f, false));
        g.fillRect (juce::Rectangle<float> (x - 18.0f, plot.getY(), 18.0f, plot.getHeight()));
        g.setColour (colours::text.withAlpha (0.5f));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
    }

    // Handles.
    struct HandleStyle { Handle handle; juce::Colour colour; const char* tag; };
    const HandleStyle styles[] { { Handle::onset, colours::cyan, "PRE / MIX" },
                                 { Handle::body, colours::yellow, "SIZE" },
                                 { Handle::tail, colours::magenta, "DECAY / TONE" } };

    for (const auto& style : styles)
    {
        const auto pos = getHandlePosition (style.handle);
        const bool active = style.handle == dragging || (dragging == Handle::none && style.handle == hovered);

        g.setColour (style.colour.withAlpha (0.25f));
        g.drawVerticalLine ((int) pos.x, plot.getY(), plot.getBottom());

        juce::Path ring;
        ring.addEllipse (juce::Rectangle<float> (handleRadius * 2.0f, handleRadius * 2.0f).withCentre (pos));
        strokeGlow (g, ring, style.colour, active ? 1.2f : 0.45f);
        g.setColour (active ? style.colour : colours::panel);
        g.fillPath (ring);
        g.setColour (style.colour);
        g.strokePath (ring, juce::PathStrokeType (1.5f));

        g.setFont (mono (9.0f, true));
        g.setColour (style.colour.withAlpha (active ? 1.0f : 0.7f));
        const bool labelLeft = pos.x > plot.getRight() - 90.0f;
        g.drawText (style.tag, juce::Rectangle<float> (labelLeft ? pos.x - 96.0f : pos.x + 12.0f, pos.y - 6.0f, 84.0f, 12.0f),
                    labelLeft ? juce::Justification::centredRight : juce::Justification::centredLeft);
    }

    // Readout for whatever is being touched, otherwise a hint.
    auto header = getLocalBounds().toFloat().removeFromTop (30.0f).reduced (18.0f, 0.0f);
    const auto focus = dragging != Handle::none ? dragging : hovered;
    juce::String readout;
    for (auto* param : getParameters (focus))
        readout << (readout.isEmpty() ? "" : "   ") << param->getName (32).toUpperCase() << " "
                << param->getCurrentValueAsText();

    g.setFont (mono (11.0f, true));
    g.setColour (focus == Handle::none ? colours::dimText : colours::text);
    g.drawText (readout.isEmpty() ? juce::String ("DRAG ON THE WAVEFORM TO RESHAPE THE REVERB") : readout, header,
                juce::Justification::centredLeft);

    g.setColour (colours::cyan.withAlpha (0.8f));
    g.drawText (p::modeNames[(int) shown.mode].toUpperCase() + " // " + p::colorNames[(int) shown.color].toUpperCase()
                    + (shown.lofi ? " // LOFI" : ""),
                header, juce::Justification::centredRight);
}
