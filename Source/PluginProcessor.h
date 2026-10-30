#pragma once

#include "PresetManager.h"
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

private:

    halcyon::ReverbEngine engine;

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
    } raw;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HalcyonAudioProcessor)
};
