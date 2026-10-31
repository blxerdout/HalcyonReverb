#pragma once

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <mutex>

// Visual tab. Draws the reverb's actual impulse response, rendered with the
// engine on a background thread, on a stretched time axis with a dB-scaled
// envelope. Three handles reshape it:
//   ONSET: x = pre-delay, y = mix      BODY: y = size      TAIL: x = decay, y = tone (high cut)
class WaveformView final : public juce::Component,
                           private juce::Timer,
                           private juce::Thread,
                           private juce::AsyncUpdater
{
public:
    explicit WaveformView (HalcyonAudioProcessor&);
    ~WaveformView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    enum class Handle
    {
        none,
        onset,
        body,
        tail
    };

    // Per 0.5 ms bucket, the highest and lowest point on a 0..1 scale
    // (0 = -75 dB below the peak, 1 = the peak).
    struct Envelope
    {
        std::vector<float> top, bottom;
    };

    void run() override;
    void timerCallback() override;
    void handleAsyncUpdate() override;

    Envelope render (halcyon::ReverbParams, int version);
    void rebuildColumns();

    juce::Rectangle<float> getPlotArea() const;
    float timeToX (float seconds) const;
    float xToTime (float x) const;
    float amplitudeForMix (float mix) const { return 0.2f + 0.8f * mix; }
    juce::Point<float> getHandlePosition (Handle) const;
    Handle findHandle (juce::Point<float>) const;
    std::vector<juce::RangedAudioParameter*> getParameters (Handle) const;
    void dragTo (Handle, juce::Point<float>);
    void setParameter (const char* id, float value);

    HalcyonAudioProcessor& audioProcessor;
    halcyon::ReverbParams shown;

    std::mutex lock;
    halcyon::ReverbParams pending;
    std::shared_ptr<const Envelope> latest;
    std::atomic<int> pendingVersion { 0 };

    // The latest envelope reduced to one top/bottom pair per pixel column.
    std::vector<float> columnTop, columnBottom;

    Handle hovered = Handle::none, dragging = Handle::none;

    // Playhead: restarts on every new sound the processor hears, then runs on
    // the wall clock so it moves smoothly between audio blocks.
    int lastOnsetCount = 0;
    double onsetWallTime = 0.0;
    bool playheadActive = false;
    float playheadSeconds = 0.0f;
    float playheadLevel = 0.0f; // 0..1, reverb output on the same dB scale as the drawing
    float dragGlow = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformView)
};
