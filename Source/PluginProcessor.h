#pragma once

#include "PresetManager.h"
#include "dsp/OnsetDetector.h"
#include "dsp/ReverbEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

class HalcyonAudioProcessor final : public juce::AudioProcessor
{
public:
    HalcyonAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    halcyon::PresetManager presets { apvts };

    // Current parameter values in engine form. Safe to call from any thread.
    halcyon::ReverbParams readParams() const;

    // What the audio thread last heard, for the Visual tab's playhead.
    struct Activity
    {
        int onsetCount = 0;              // increments on every new sound at the input
        double secondsSinceOnset = 0.0;  // audio time since the most recent one
        float wetPeak = 0.0f;            // reverb output level of the latest block
    };
    Activity getActivity() const;

private:

    halcyon::ReverbEngine engine;
    halcyon::OnsetDetector onsets;

    std::atomic<std::int64_t> samplesProcessed { 0 }, lastOnsetSample { 0 };
    std::atomic<int> onsetCount { 0 };
    std::atomic<double> currentSampleRate { 48000.0 };
    std::atomic<float> wetPeak { 0.0f };

    struct
    {
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* preDelay = nullptr;
        std::atomic<float>* decay = nullptr;
        std::atomic<float>* size = nullptr;
        std::atomic<float>* diffusion = nullptr;
        std::atomic<float>* modRate = nullptr;
        std::atomic<float>* modDepth = nullptr;
        std::atomic<float>* highCut = nullptr;
        std::atomic<float>* lowCut = nullptr;
        std::atomic<float>* width = nullptr;
        std::atomic<float>* mode = nullptr;
        std::atomic<float>* color = nullptr;
        std::atomic<float>* lofi = nullptr;
        std::atomic<float>* speaker = nullptr;
    } raw;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HalcyonAudioProcessor)
};
