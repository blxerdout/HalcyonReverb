#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"

namespace p = halcyon::params;

HalcyonAudioProcessor::HalcyonAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HalcyonState", p::createLayout())
{
    raw.mix = apvts.getRawParameterValue (p::mix);
    raw.preDelay = apvts.getRawParameterValue (p::preDelay);
    raw.decay = apvts.getRawParameterValue (p::decay);
    raw.size = apvts.getRawParameterValue (p::size);
    raw.diffusion = apvts.getRawParameterValue (p::diffusion);
    raw.modRate = apvts.getRawParameterValue (p::modRate);
    raw.modDepth = apvts.getRawParameterValue (p::modDepth);
    raw.highCut = apvts.getRawParameterValue (p::highCut);
    raw.lowCut = apvts.getRawParameterValue (p::lowCut);
    raw.width = apvts.getRawParameterValue (p::width);
    raw.mode = apvts.getRawParameterValue (p::mode);
    raw.color = apvts.getRawParameterValue (p::color);
    raw.lofi = apvts.getRawParameterValue (p::lofi);
    raw.speaker = apvts.getRawParameterValue (p::speaker);
}

halcyon::ReverbParams HalcyonAudioProcessor::readParams() const
{
    halcyon::ReverbParams r;
    r.mix = raw.mix->load();
    r.preDelayMs = raw.preDelay->load();
    r.decaySeconds = raw.decay->load();
    r.size = raw.size->load();
    r.diffusion = raw.diffusion->load();
    r.modRateHz = raw.modRate->load();
    r.modDepth = raw.modDepth->load();
    r.highCutHz = raw.highCut->load();
    r.lowCutHz = raw.lowCut->load();
    r.width = raw.width->load();
    r.mode = (halcyon::Mode) juce::jlimit (0, p::modeNames.size() - 1, (int) raw.mode->load());
    r.color = (halcyon::Color) juce::jlimit (0, p::colorNames.size() - 1, (int) raw.color->load());
    r.lofi = raw.lofi->load() >= 0.5f;
    r.speaker = (halcyon::Speaker) juce::jlimit (0, p::speakerNames.size() - 1, (int) raw.speaker->load());
    return r;
}

void HalcyonAudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    engine.setParams (readParams());
    engine.reset();
    onsets.prepare (sampleRate);
    currentSampleRate = sampleRate;
}

void HalcyonAudioProcessor::reset()
{
    engine.reset();
    onsets.reset();
}

bool HalcyonAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
           && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void HalcyonAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    if (numOut < 2)
        return;

    // Mono in, stereo out: feed the reverb the same signal on both sides.
    if (numIn == 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    for (int ch = std::max (numIn, 2); ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    const auto blockStart = samplesProcessed.load();
    const int onset = onsets.process (buffer.getReadPointer (0), buffer.getReadPointer (1), numSamples);
    if (onset >= 0)
    {
        lastOnsetSample = blockStart + onset;
        ++onsetCount;
    }

    engine.setParams (readParams());
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples);

    wetPeak = engine.getLastWetPeak();
    samplesProcessed = blockStart + numSamples;
}

HalcyonAudioProcessor::Activity HalcyonAudioProcessor::getActivity() const
{
    Activity activity;
    activity.onsetCount = onsetCount.load();
    activity.secondsSinceOnset = (double) (samplesProcessed.load() - lastOnsetSample.load()) / currentSampleRate.load();
    activity.wetPeak = wetPeak.load();
    return activity;
}

double HalcyonAudioProcessor::getTailLengthSeconds() const
{
    return raw.decay->load() * 1.5 + raw.preDelay->load() / 1000.0;
}

juce::AudioProcessorEditor* HalcyonAudioProcessor::createEditor()
{
    return new HalcyonAudioProcessorEditor (*this);
}

void HalcyonAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void HalcyonAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HalcyonAudioProcessor();
}
