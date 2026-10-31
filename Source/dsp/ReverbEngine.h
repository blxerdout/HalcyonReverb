#pragma once

// Halcyon reverb engine.
//
// Plain C++17 with no JUCE dependency, so the algorithm can be unit-tested
// and auditioned offline. Signal flow per channel pair:
//
//   input -> pre-delay -> low cut / colour band-limit -> 4 series allpass
//   diffusers -> 8-line feedback delay network (Hadamard mixing, modulated
//   delay reads, in-loop damping + allpass) -> stereo taps
//
// Early reflections are a tapped delay line fed from the pre-delayed input.
// Each Mode supplies its own delay-length set, density and reflection level;
// each Color shapes bandwidth, modulation and in-loop saturation. Lofi runs
// the wet signal through a band-pass, fuzz, sample-and-hold and 8-bit
// quantiser, crossfaded in so toggling it doesn't click.

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace halcyon
{

enum class Mode
{
    Plate,
    Hall,
    Room,
    Chamber,
    Cathedral
};

enum class Color
{
    Modern,
    Vintage,
    Dark
};

struct ReverbParams
{
    float mix = 0.35f;          // 0..1, dry/wet
    float preDelayMs = 10.0f;   // 0..500
    float decaySeconds = 2.5f;  // RT60, 0.2..30
    float size = 0.5f;          // 0..1
    float diffusion = 0.75f;    // 0..1
    float modRateHz = 0.6f;     // 0.05..5
    float modDepth = 0.4f;      // 0..1
    float highCutHz = 8000.0f;  // in-loop damping, 1k..20k
    float lowCutHz = 80.0f;     // input high-pass, 20..1000
    float width = 1.0f;         // 0 = mono wet, 1 = full stereo
    Mode mode = Mode::Hall;
    Color color = Color::Modern;
    bool lofi = false;
};

namespace detail
{

class DelayLine
{
public:
    void prepare (int maxDelaySamples)
    {
        int size = 1;
        while (size < maxDelaySamples + 4)
            size <<= 1;
        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writePos = 0;
    }

    void clear() { std::fill (buffer.begin(), buffer.end(), 0.0f); }

    void push (float x)
    {
        buffer[(size_t) writePos] = x;
        writePos = (writePos + 1) & mask;
    }

    // read (0) is the most recently pushed sample.
    float read (int delay) const { return buffer[(size_t) ((writePos - 1 - delay) & mask)]; }

    // Cubic Hermite interpolation; delay must be >= 1.
    float readFrac (float delay) const
    {
        const int i = (int) delay;
        const float f = delay - (float) i;
        const float y0 = read (i - 1), y1 = read (i), y2 = read (i + 1), y3 = read (i + 2);
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

    float readLinear (float delay) const
    {
        const int i = (int) delay;
        const float f = delay - (float) i;
        const float a = read (i), b = read (i + 1);
        return a + f * (b - a);
    }

private:
    std::vector<float> buffer;
    int mask = 0;
    int writePos = 0;
};

// Schroeder allpass with a fractional (smoothly changeable) delay.
class Allpass
{
public:
    void prepare (int maxDelaySamples) { line.prepare (maxDelaySamples); }
    void clear() { line.clear(); }

    float process (float x, float delaySamples, float g)
    {
        const float delayed = line.readFrac (delaySamples - 1.0f);
        const float w = x + g * delayed;
        line.push (w);
        return delayed - g * w;
    }

private:
    DelayLine line;
};

struct OnePoleLowpass
{
    float a = 1.0f;
    float z = 0.0f;

    void setCutoff (float hz, float sampleRate)
    {
        a = 1.0f - std::exp (-6.28318530718f * hz / sampleRate);
    }

    float process (float x)
    {
        z += a * (x - z);
        return z;
    }
};

struct OnePoleHighpass
{
    OnePoleLowpass lp;
    void setCutoff (float hz, float sampleRate) { lp.setCutoff (hz, sampleRate); }
    float process (float x) { return x - lp.process (x); }
};

// Band-pass -> fuzz -> sample-and-hold -> 8-bit quantise -> smoothing low-pass.
class LofiStage
{
public:
    void prepare (float sampleRate)
    {
        highpass.setCutoff (300.0f, sampleRate);
        bandLimit1.setCutoff (3200.0f, sampleRate);
        bandLimit2.setCutoff (3200.0f, sampleRate);
        smoothing.setCutoff (5500.0f, sampleRate);
        holdInc = 11025.0f / sampleRate;
        clear();
    }

    void clear()
    {
        highpass.lp.z = bandLimit1.z = bandLimit2.z = smoothing.z = 0.0f;
        holdPhase = 1.0f; // take a fresh sample on the first call
        held = 0.0f;
    }

    float process (float x)
    {
        x = bandLimit2.process (bandLimit1.process (highpass.process (x)));

        const float driven = 6.0f * x;
        x = driven / (1.0f + std::abs (driven));

        holdPhase += holdInc;
        if (holdPhase >= 1.0f)
        {
            holdPhase -= 1.0f;
            held = std::round (x * 127.0f) / 127.0f;
        }

        return 0.5f * smoothing.process (held);
    }

private:
    OnePoleHighpass highpass;
    OnePoleLowpass bandLimit1, bandLimit2, smoothing;
    float holdInc = 0.25f;
    float holdPhase = 1.0f;
    float held = 0.0f;
};

} // namespace detail

class ReverbEngine
{
public:
    static constexpr int numLines = 8;
    static constexpr int numDiffusers = 4;
    static constexpr int numTaps = 8;

    void prepare (double sampleRate);
    void reset();
    void setParams (const ReverbParams& p);

    // In-place stereo processing. Both pointers must be valid.
    void process (float* left, float* right, int numSamples);

    // Peak level of the wet signal (before mix) during the last process() call.
    float getLastWetPeak() const { return lastWetPeak; }

private:
    void updateLoopGains();

    float fs = 48000.0f;
    float smoothCoef = 0.0f;
    int controlCounter = 0;

    // Smoothed values: current and target.
    std::array<float, numLines> lineLen {}, lineLenTarget {};
    std::array<float, numLines> apLen {}, apLenTarget {};
    std::array<float, numTaps> erTapL {}, erTapLTarget {}, erTapR {}, erTapRTarget {};
    float preDelay = 0.0f, preDelayTarget = 0.0f;
    float decay = 2.5f, decayTarget = 2.5f;
    float modDepthSamples = 0.0f, modDepthTarget = 0.0f;
    float mix = 0.35f, mixTarget = 0.35f;
    float width = 1.0f, widthTarget = 1.0f;
    float erLevel = 0.0f, erLevelTarget = 0.0f;
    float lofiAmount = 0.0f, lofiTarget = 0.0f;
    float lastWetPeak = 0.0f;

    // Non-smoothed settings.
    float inputDiffGain = 0.6f;
    float tankApGain = 0.5f;
    float drive = 0.0f;
    std::array<float, numLines> loopGain {};
    std::array<float, numLines> lfoPhase {}, lfoInc {};
    std::array<float, numDiffusers> diffuserLen {};

    detail::DelayLine preDelayL, preDelayR;
    detail::DelayLine erLineL, erLineR;
    detail::OnePoleHighpass lowCutL, lowCutR;
    detail::OnePoleLowpass bandLimitL, bandLimitR;
    std::array<detail::Allpass, numDiffusers> diffL, diffR;
    std::array<detail::DelayLine, numLines> lines;
    std::array<detail::Allpass, numLines> tankAp;
    std::array<detail::OnePoleLowpass, numLines> damping;
    detail::LofiStage lofiL, lofiR;
};

} // namespace halcyon
