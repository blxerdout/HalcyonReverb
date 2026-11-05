#include "ReverbEngine.h"

#include <algorithm>

namespace halcyon
{
namespace
{

struct ModeSettings
{
    std::array<float, ReverbEngine::numLines> lineMs;  // FDN delay lengths at size scale 1
    std::array<float, ReverbEngine::numLines> apMs;    // in-loop allpass lengths
    float tankApGain;   // in-loop density
    float diffScale;    // input diffusion multiplier
    float erLevel;      // early reflection level
    float erScale;      // early reflection time multiplier
    float modScale;     // modulation depth multiplier
};

// Delay lengths are chosen to be mutually incommensurate so the modes of the
// network don't pile up on common frequencies.
constexpr std::array<ModeSettings, 5> modeTable {{
    // Plate: short, very dense, no reflections — energy builds instantly.
    { { 28.3f, 33.7f, 37.1f, 41.9f, 46.3f, 51.7f, 55.1f, 60.7f },
      { 8.9f, 7.1f, 6.3f, 5.3f, 9.7f, 7.9f, 6.7f, 5.9f },
      0.62f, 1.0f, 0.0f, 0.6f, 0.6f },
    // Hall: long lines, moderate density, light reflections, lush motion.
    { { 47.3f, 53.9f, 61.3f, 68.9f, 76.1f, 83.7f, 91.3f, 99.1f },
      { 6.1f, 4.9f, 7.3f, 5.7f, 6.7f, 4.3f, 7.9f, 5.1f },
      0.45f, 0.9f, 0.25f, 1.2f, 1.0f },
    // Room: short lines, strong reflections, little motion.
    { { 19.1f, 23.3f, 27.7f, 31.9f, 35.3f, 39.7f, 43.1f, 47.9f },
      { 3.7f, 2.9f, 4.3f, 3.1f, 4.1f, 2.7f, 4.7f, 3.3f },
      0.5f, 0.8f, 0.6f, 0.7f, 0.4f },
    // Chamber: in between, with a fairly busy reflection pattern.
    { { 31.1f, 36.7f, 40.3f, 45.7f, 49.3f, 55.9f, 59.3f, 64.1f },
      { 5.3f, 4.1f, 6.1f, 4.7f, 5.9f, 3.9f, 6.7f, 4.4f },
      0.55f, 0.9f, 0.4f, 1.0f, 0.5f },
    // Cathedral: very long lines, slow build, deep modulation.
    { { 71.3f, 83.9f, 97.1f, 109.3f, 121.7f, 133.1f, 146.9f, 158.3f },
      { 9.1f, 7.7f, 10.3f, 8.3f, 9.9f, 7.3f, 10.9f, 8.7f },
      0.6f, 1.0f, 0.1f, 1.6f, 1.5f },
}};

struct ColorSettings
{
    float bandLimitHz;  // input bandwidth ceiling
    float dampMult;     // multiplier on the High Cut control
    float modMult;
    float drive;        // in-loop soft saturation amount
};

constexpr std::array<ColorSettings, 3> colorTable {{
    { 20000.0f, 1.0f, 1.0f, 0.0f },   // Modern: full bandwidth, clean
    { 10000.0f, 0.8f, 1.6f, 0.25f },  // Vintage: band-limited, wobbly, a little grit
    { 6000.0f, 0.5f, 1.2f, 0.1f },    // Dark: soft and woolly
}};

constexpr std::array<float, ReverbEngine::numDiffusers> diffuserMs { 4.77f, 3.59f, 12.73f, 9.31f };

constexpr std::array<float, ReverbEngine::numTaps> erMsL { 3.1f, 7.9f, 11.3f, 17.6f, 23.2f, 29.9f, 37.4f, 45.8f };
constexpr std::array<float, ReverbEngine::numTaps> erMsR { 4.3f, 8.7f, 13.9f, 19.1f, 25.7f, 31.3f, 39.8f, 48.6f };
constexpr std::array<float, ReverbEngine::numTaps> erGain { 0.84f, -0.71f, 0.62f, -0.55f, 0.47f, -0.40f, 0.33f, -0.27f };

// Rows of an 8x8 Hadamard matrix: mutually orthogonal, so the two outputs
// are decorrelated.
constexpr std::array<float, ReverbEngine::numLines> outSignL { 1, -1, -1, 1, 1, -1, -1, 1 };
constexpr std::array<float, ReverbEngine::numLines> outSignR { 1, -1, 1, -1, -1, 1, -1, 1 };
constexpr std::array<float, ReverbEngine::numLines> inSign { 1, -1, 1, 1, -1, 1, -1, -1 };

// Extra outputs partway along each line, so the tail builds continuously instead
// of arriving all at once after the shortest line, which leaves an audible gap
// after the early reflections in big rooms. Each uses its own pair of Hadamard
// rows to keep left and right decorrelated.
constexpr std::array<float, 2> midTapAt { 0.21f, 0.53f };
constexpr std::array<std::array<float, ReverbEngine::numLines>, 2> midSignL {{ { 1, 1, -1, -1, 1, 1, -1, -1 },
                                                                              { 1, 1, 1, 1, -1, -1, -1, -1 } }};
constexpr std::array<std::array<float, ReverbEngine::numLines>, 2> midSignR {{ { 1, -1, -1, 1, -1, 1, 1, -1 },
                                                                              { 1, 1, -1, -1, -1, -1, 1, 1 } }};
constexpr float midTapGain = 0.5f;
constexpr float lateGain = 0.35355339f / 1.2247449f; // 1 / sqrt (8), less the energy the mid taps add

constexpr float minSizeScale = 0.3f, maxSizeScale = 2.5f;
constexpr float maxModSamplesAt48k = 12.0f * 1.6f * 1.6f;
constexpr float outputGain = 0.5f;
constexpr int controlInterval = 32;

// Size 0..1 -> how much every delay in the room is stretched. Exponential, since
// we hear room size as a ratio; 0.5 stays at 1x so mid-size presets are unchanged.
float sizeToScale (float size)
{
    const float s = std::clamp (size, 0.0f, 1.0f) - 0.5f;
    return std::pow (s < 0.0f ? 1.0f / minSizeScale : maxSizeScale, 2.0f * s);
}

// The input diffusers follow the room, but less than proportionally: fully
// scaled they turn into audible slapback at large sizes.
float diffuserScale (float scale) { return std::pow (scale, 0.7f); }

void hadamard8 (float* x)
{
    for (int h = 1; h < 8; h *= 2)
        for (int i = 0; i < 8; i += 2 * h)
            for (int j = i; j < i + h; ++j)
            {
                const float a = x[j], b = x[j + h];
                x[j] = a + b;
                x[j + h] = a - b;
            }

    for (int i = 0; i < 8; ++i)
        x[i] *= 0.35355339f; // 1 / sqrt (8), keeps the matrix unitary
}

float saturate (float x, float drive)
{
    return x + drive * (x / (1.0f + std::abs (x)) - x);
}

} // namespace

void ReverbEngine::prepare (double sampleRate)
{
    fs = (float) sampleRate;
    smoothCoef = 1.0f - std::exp (-1.0f / (0.05f * fs)); // ~50 ms glide

    const float msToSamples = fs / 1000.0f;
    const int maxMod = (int) std::ceil (maxModSamplesAt48k * fs / 48000.0f);

    preDelayL.prepare ((int) (500.0f * msToSamples) + 8);
    preDelayR.prepare ((int) (500.0f * msToSamples) + 8);
    erLineL.prepare ((int) (60.0f * 1.6f * maxSizeScale * msToSamples) + 8);
    erLineR.prepare ((int) (60.0f * 1.6f * maxSizeScale * msToSamples) + 8);

    for (int i = 0; i < numDiffusers; ++i)
    {
        // The right channel runs 7% longer.
        const int maxLen = (int) (diffuserMs[(size_t) i] * 1.071f * diffuserScale (maxSizeScale) * msToSamples) + 8;
        diffL[(size_t) i].prepare (maxLen);
        diffR[(size_t) i].prepare (maxLen);
        diffuserLenTarget[(size_t) i] = diffuserMs[(size_t) i] * msToSamples; // until setParams
    }

    for (int i = 0; i < numLines; ++i)
    {
        lines[(size_t) i].prepare ((int) (160.0f * maxSizeScale * msToSamples) + maxMod + 8);
        tankAp[(size_t) i].prepare ((int) (11.0f * maxSizeScale * msToSamples) + 8);
        lfoPhase[(size_t) i] = (float) i / (float) numLines;
    }

    lofiL.prepare (fs);
    lofiR.prepare (fs);
    speakerStage.prepare (fs);

    reset();
}

void ReverbEngine::reset()
{
    preDelayL.clear();
    preDelayR.clear();
    erLineL.clear();
    erLineR.clear();
    lowCutL.lp.z = lowCutR.lp.z = 0.0f;
    bandLimitL.z = bandLimitR.z = 0.0f;

    for (auto& d : diffL) d.clear();
    for (auto& d : diffR) d.clear();
    for (auto& l : lines) l.clear();
    for (auto& a : tankAp) a.clear();
    for (auto& d : damping) d.z = 0.0f;
    lofiL.clear();
    lofiR.clear();
    speakerStage.snap();

    // Jump straight to targets so a fresh instance doesn't glide in.
    lineLen = lineLenTarget;
    apLen = apLenTarget;
    diffuserLen = diffuserLenTarget;
    erTapL = erTapLTarget;
    erTapR = erTapRTarget;
    preDelay = preDelayTarget;
    decay = decayTarget;
    modDepthSamples = modDepthTarget;
    mix = mixTarget;
    width = widthTarget;
    erLevel = erLevelTarget;
    lofiAmount = lofiTarget;
    updateLoopGains();
}

void ReverbEngine::setParams (const ReverbParams& p)
{
    const auto& mode = modeTable[(size_t) p.mode];
    const auto& color = colorTable[(size_t) p.color];
    const float msToSamples = fs / 1000.0f;
    const float scale = sizeToScale (p.size);
    const float erScale = mode.erScale * scale;

    for (size_t i = 0; i < (size_t) numLines; ++i)
    {
        lineLenTarget[i] = mode.lineMs[i] * scale * msToSamples;
        apLenTarget[i] = std::max (2.0f, mode.apMs[i] * scale * msToSamples);
        lfoInc[i] = p.modRateHz * (1.0f + 0.07f * (float) i) / fs;
    }

    for (size_t i = 0; i < (size_t) numDiffusers; ++i)
        diffuserLenTarget[i] = std::max (2.0f, diffuserMs[i] * diffuserScale (scale) * msToSamples);

    for (size_t i = 0; i < (size_t) numTaps; ++i)
    {
        erTapLTarget[i] = std::max (1.0f, erMsL[i] * erScale * msToSamples);
        erTapRTarget[i] = std::max (1.0f, erMsR[i] * erScale * msToSamples);
    }

    preDelayTarget = std::max (1.0f, std::clamp (p.preDelayMs, 0.0f, 500.0f) * msToSamples);
    decayTarget = std::clamp (p.decaySeconds, 0.1f, 60.0f);
    modDepthTarget = std::clamp (p.modDepth, 0.0f, 1.0f) * 12.0f * mode.modScale * color.modMult * fs / 48000.0f;
    mixTarget = std::clamp (p.mix, 0.0f, 1.0f);
    widthTarget = std::clamp (p.width, 0.0f, 1.0f);
    erLevelTarget = mode.erLevel;
    lofiTarget = p.lofi ? 1.0f : 0.0f;
    speakerStage.setModel (p.speaker);

    const float diffusion = std::clamp (p.diffusion, 0.0f, 1.0f);
    inputDiffGain = 0.75f * diffusion * mode.diffScale;
    tankApGain = mode.tankApGain * (0.4f + 0.6f * diffusion);
    drive = color.drive;

    const float nyquistSafe = 0.45f * fs;
    const float lowCut = std::clamp (p.lowCutHz, 10.0f, nyquistSafe);
    const float bandLimit = std::min (color.bandLimitHz, nyquistSafe);
    const float damp = std::clamp (p.highCutHz * color.dampMult, 200.0f, nyquistSafe);

    lowCutL.setCutoff (lowCut, fs);
    lowCutR.setCutoff (lowCut, fs);
    bandLimitL.setCutoff (bandLimit, fs);
    bandLimitR.setCutoff (bandLimit, fs);
    for (auto& d : damping)
        d.setCutoff (damp, fs);
}

void ReverbEngine::updateLoopGains()
{
    // Gain per pass so each line decays 60 dB in `decay` seconds, regardless
    // of its length: g = 10 ^ (-3 * loopSamples / (RT60 * fs)).
    for (size_t i = 0; i < (size_t) numLines; ++i)
    {
        const float loopSamples = lineLen[i] + apLen[i];
        loopGain[i] = std::pow (10.0f, -3.0f * loopSamples / (decay * fs));
    }
}

void ReverbEngine::process (float* left, float* right, int numSamples)
{
    const float k = smoothCoef;
    auto glide = [k] (float& v, float target) { v += k * (target - v); };
    float wetPeak = 0.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        for (size_t i = 0; i < (size_t) numLines; ++i)
        {
            glide (lineLen[i], lineLenTarget[i]);
            glide (apLen[i], apLenTarget[i]);
        }
        for (size_t i = 0; i < (size_t) numDiffusers; ++i)
            glide (diffuserLen[i], diffuserLenTarget[i]);
        for (size_t i = 0; i < (size_t) numTaps; ++i)
        {
            glide (erTapL[i], erTapLTarget[i]);
            glide (erTapR[i], erTapRTarget[i]);
        }
        glide (preDelay, preDelayTarget);
        glide (decay, decayTarget);
        glide (modDepthSamples, modDepthTarget);
        glide (mix, mixTarget);
        glide (width, widthTarget);
        glide (erLevel, erLevelTarget);
        glide (lofiAmount, lofiTarget);

        if (controlCounter++ % controlInterval == 0)
            updateLoopGains();

        const float dryL = left[n], dryR = right[n];

        // Pre-delay, then shape what enters the reverb.
        preDelayL.push (dryL);
        preDelayR.push (dryR);
        float inL = preDelayL.readLinear (preDelay - 1.0f);
        float inR = preDelayR.readLinear (preDelay - 1.0f);
        inL = bandLimitL.process (lowCutL.process (inL));
        inR = bandLimitR.process (lowCutR.process (inR));

        // Early reflections.
        erLineL.push (inL);
        erLineR.push (inR);
        float erL = 0.0f, erR = 0.0f;
        for (size_t i = 0; i < (size_t) numTaps; ++i)
        {
            erL += erGain[i] * erLineL.readLinear (erTapL[i]);
            erR += erGain[i] * erLineR.readLinear (erTapR[i]);
        }

        // Input diffusion smears transients before they hit the tank.
        for (size_t i = 0; i < (size_t) numDiffusers; ++i)
        {
            inL = diffL[i].process (inL, diffuserLen[i], inputDiffGain);
            inR = diffR[i].process (inR, diffuserLen[i] * 1.071f, inputDiffGain);
        }

        // Feedback delay network.
        std::array<float, numLines> tap {}, fb {};
        for (size_t i = 0; i < (size_t) numLines; ++i)
        {
            lfoPhase[i] += lfoInc[i];
            if (lfoPhase[i] >= 1.0f)
                lfoPhase[i] -= 1.0f;

            const float mod = modDepthSamples * std::sin (6.28318530718f * lfoPhase[i]);
            float y = lines[i].readFrac (std::max (2.0f, lineLen[i] + mod));
            y = damping[i].process (y);
            y = tankAp[i].process (y, apLen[i], tankApGain);
            tap[i] = y;
            fb[i] = saturate (y * loopGain[i], drive);
        }

        float lateL = 0.0f, lateR = 0.0f;
        for (size_t t = 0; t < midTapAt.size(); ++t)
        {
            for (size_t i = 0; i < (size_t) numLines; ++i)
            {
                const float y = midTapGain * lines[i].readLinear (std::max (1.0f, midTapAt[t] * lineLen[i]));
                lateL += midSignL[t][i] * y;
                lateR += midSignR[t][i] * y;
            }
        }

        hadamard8 (fb.data());

        for (size_t i = 0; i < (size_t) numLines; ++i)
        {
            const float inject = inSign[i] * (i < (size_t) numLines / 2 ? inL : inR);
            lines[i].push (fb[i] + inject);
            lateL += outSignL[i] * tap[i];
            lateR += outSignR[i] * tap[i];
        }

        float wetL = outputGain * (lateL * lateGain + erLevel * erL);
        float wetR = outputGain * (lateR * lateGain + erLevel * erR);

        if (lofiAmount > 1.0e-4f)
        {
            wetL += lofiAmount * (lofiL.process (wetL) - wetL);
            wetR += lofiAmount * (lofiR.process (wetR) - wetR);
        }

        // Width via mid/side.
        const float mid = 0.5f * (wetL + wetR);
        const float side = 0.5f * (wetL - wetR) * width;
        wetL = mid + side;
        wetR = mid - side;
        wetPeak = std::max (wetPeak, std::max (std::abs (wetL), std::abs (wetR)));

        left[n] = dryL + mix * (wetL - dryL);
        right[n] = dryR + mix * (wetR - dryR);
        speakerStage.process (left[n], right[n]);
    }

    lastWetPeak = wetPeak;
}

} // namespace halcyon
