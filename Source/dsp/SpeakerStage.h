#pragma once

// Output stage that plays the whole signal (dry and wet) "through" a real-world
// speaker: a phone, a TV, a boom box. Each model is a band limit, cabinet
// resonances, amp or tape saturation, and optionally a phone-line codec, noise
// that follows the signal, a CRT whine or cassette wow and flutter.
//
// Plain C++17, no JUCE, real-time safe. Switching models fades out, swaps and
// fades back in, so it can be automated without clicks.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace halcyon
{

enum class Speaker
{
    Off,
    CellPhone,
    Landline,
    CrtTv,
    BoomBox,
    Arcade
};

inline constexpr int numSpeakers = 6;

namespace detail
{

// RBJ cookbook biquad, transposed direct form II.
struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    void clear() { z1 = z2 = 0.0f; }

    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setLowpass (float hz, float q, float fs) { set (hz, q, fs, 0.0f, Type::lowpass); }
    void setHighpass (float hz, float q, float fs) { set (hz, q, fs, 0.0f, Type::highpass); }
    void setPeak (float hz, float q, float gainDb, float fs) { set (hz, q, fs, gainDb, Type::peak); }

private:
    enum class Type { lowpass, highpass, peak };

    void set (float hz, float q, float fs, float gainDb, Type type)
    {
        const double w = 6.283185307179586 * std::min ((double) hz, 0.45 * fs) / fs;
        const double cosw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        double nb0, nb1, nb2, na0, na1, na2;

        if (type == Type::peak)
        {
            const double a = std::pow (10.0, gainDb / 40.0);
            nb0 = 1.0 + alpha * a; nb1 = -2.0 * cosw; nb2 = 1.0 - alpha * a;
            na0 = 1.0 + alpha / a; na1 = -2.0 * cosw; na2 = 1.0 - alpha / a;
        }
        else
        {
            const double k = type == Type::lowpass ? 1.0 - cosw : 1.0 + cosw;
            nb0 = k / 2.0; nb1 = type == Type::lowpass ? k : -k; nb2 = k / 2.0;
            na0 = 1.0 + alpha; na1 = -2.0 * cosw; na2 = 1.0 - alpha;
        }

        b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0);
        a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
    }
};

struct SpeakerModel
{
    float highpassHz; int highpassStages;   // each stage is 12 dB/oct
    float lowpassHz;  int lowpassStages;
    std::array<std::array<float, 3>, 2> peaks; // { Hz, Q, dB }, 0 Hz = unused
    float drive;        // into the amp/tape saturation; 0 = clean
    float holdHz;       // sample-and-hold rate, 0 = off
    bool muLaw;         // 8-bit mu-law companding, as on a digital phone line
    float width;        // 0 = mono speaker, 1 = untouched stereo
    float hissDb;       // noise level relative to the signal, -200 = none
    float whineHz, whineDb;
    bool wowFlutter;    // cassette pitch wobble
    float gain;         // makes up the level lost to the band limit
};

// clang-format off
//                                  HP Hz  n   LP Hz    n    peak 1                peak 2                 drive hold     mu     width hiss    whine            wow    gain
inline constexpr std::array<SpeakerModel, numSpeakers> speakerModels {{
    /* Off        */ {    0.0f, 0,     0.0f, 0, {{ {   0, 0, 0 }, {    0, 0, 0 } }}, 0.0f,     0.0f, false, 1.0f, -200.0f,     0.0f, -200.0f, false, 1.0f },
    /* Cell Phone */ {  650.0f, 2,  7500.0f, 2, {{ { 2200, 1.2f, 6 }, { 4500, 2.0f, 3 } }}, 2.0f,     0.0f, false, 0.0f, -200.0f,     0.0f, -200.0f, false, 1.6f },
    /* Landline   */ {  330.0f, 2,  3300.0f, 3, {{ { 1700, 1.0f, 5 }, {    0, 0, 0 } }}, 3.0f,  8000.0f, true,  0.0f,  -38.0f,     0.0f, -200.0f, false, 2.5f },
    /* CRT TV     */ {  160.0f, 1,  6500.0f, 1, {{ {  900, 1.4f, 5 }, { 3000, 2.0f, -3 } }}, 1.5f,    0.0f, false, 0.0f,  -55.0f, 15734.0f,  -48.0f, false, 1.5f },
    /* Boom Box   */ {   70.0f, 1,  9000.0f, 2, {{ {  110, 0.8f, 6 }, { 3500, 1.0f, 3 } }}, 2.2f,     0.0f, false, 0.7f,  -45.0f,     0.0f, -200.0f, true,  1.3f },
    /* Arcade     */ {  140.0f, 2,  5000.0f, 2, {{ {  450, 1.0f, 4 }, { 2500, 1.5f, 4 } }}, 5.0f, 22050.0f, false, 0.0f,  -60.0f,     0.0f, -200.0f, false, 2.1f },
}};
// clang-format on

class SpeakerStage
{
public:
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        fadeStep = 1.0f / (0.02f * fs); // 20 ms each way
        envAttack = 1.0f - std::exp (-1.0f / (0.005f * fs));
        envRelease = 1.0f - std::exp (-1.0f / (0.2f * fs));
        wobble.assign ((size_t) (0.01f * fs) + 8, {});
        configure();
        clear();
    }

    void setModel (Speaker s) { pending = s; }

    // Jump straight to the requested model, without the fade.
    void snap()
    {
        current = pending;
        amount = current == Speaker::Off ? 0.0f : 1.0f;
        configure();
        clear();
    }

    void clear()
    {
        for (auto& ch : channels)
        {
            for (auto& b : ch.filters) b.clear();
            ch.held = 0.0f;
        }
        std::fill (wobble.begin(), wobble.end(), std::array<float, 2> {});
        wobblePos = 0;
        holdPhase = 1.0f;
        envelope = 0.0f;
    }

    void process (float& left, float& right)
    {
        // Fade out, switch, fade back in.
        if (pending != current)
        {
            amount -= fadeStep;
            if (amount <= 0.0f)
            {
                amount = 0.0f;
                current = pending;
                configure();
                clear();
            }
        }
        else if (current != Speaker::Off && amount < 1.0f)
        {
            amount = std::min (1.0f, amount + fadeStep);
        }

        if (current == Speaker::Off || amount <= 0.0f)
            return;

        const auto& m = speakerModels[(size_t) current];
        float l = left, r = right;

        const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * m.width;
        l = mid + side;
        r = mid - side;

        if (m.wowFlutter)
        {
            // Slow wow plus faster flutter, as on a worn cassette deck.
            wowPhase = std::fmod (wowPhase + 0.55f / fs, 1.0f);
            flutterPhase = std::fmod (flutterPhase + 6.3f / fs, 1.0f);
            const float delayMs = 3.0f + 0.9f * std::sin (6.2831853f * wowPhase) + 0.12f * std::sin (6.2831853f * flutterPhase);
            wobble[wobblePos] = { l, r };
            const float delay = delayMs * 0.001f * fs;
            const auto size = wobble.size();
            const auto i = (size_t) delay;
            const float frac = delay - (float) i;
            const auto& a = wobble[(wobblePos + size - i) % size];
            const auto& b = wobble[(wobblePos + size - i - 1) % size];
            l = a[0] + frac * (b[0] - a[0]); // linear interpolation dulls the top a little, like tape
            r = a[1] + frac * (b[1] - a[1]);
            wobblePos = (wobblePos + 1) % size;
        }

        const float level = std::max (std::abs (l), std::abs (r));
        envelope += (level > envelope ? envAttack : envRelease) * (level - envelope);

        bool sampleNow = true;
        if (m.holdHz > 0.0f)
        {
            holdPhase += m.holdHz / fs;
            sampleNow = holdPhase >= 1.0f;
            if (sampleNow)
                holdPhase -= 1.0f;
        }

        float out[2] { l, r };
        for (size_t c = 0; c < 2; ++c)
        {
            auto& ch = channels[c];
            float x = out[c];
            size_t f = 0;

            for (int i = 0; i < m.highpassStages; ++i) x = ch.filters[f++].process (x);
            for (int i = 0; i < 2; ++i) if (m.peaks[(size_t) i][0] > 0.0f) x = ch.filters[f++].process (x);

            if (m.drive > 0.0f)
            {
                const float d = m.drive * x;
                x = d / (1.0f + std::abs (d)) / m.drive; // unity gain for quiet signals
            }

            for (int i = 0; i < m.lowpassStages; ++i) x = ch.filters[f++].process (x);

            if (m.holdHz > 0.0f)
            {
                if (sampleNow)
                    ch.held = m.muLaw ? muLaw8 (x) : x;
                x = ch.held;

                // Reconstruction filter: removes the images sampling creates above the band.
                for (int i = 0; i < m.lowpassStages; ++i) x = ch.filters[f++].process (x);
            }

            out[c] = x;
        }

        // Noise and whine ride on the signal, so silence stays silent.
        float extra = 0.0f;
        if (m.hissDb > -199.0f)
            extra += hissGain * envelope * nextNoise();
        if (m.whineHz > 0.0f)
        {
            whinePhase = std::fmod (whinePhase + m.whineHz / fs, 1.0f);
            extra += whineGain * envelope * std::sin (6.2831853f * whinePhase);
        }

        const float wetL = m.gain * (out[0] + extra), wetR = m.gain * (out[1] + extra);
        left += amount * (wetL - left);
        right += amount * (wetR - right);
    }

private:
    struct Channel
    {
        std::array<Biquad, 12> filters; // 2 high-pass, 2 peaks, 2 x 3 low-pass
        float held = 0.0f;
    };

    void configure()
    {
        const auto& m = speakerModels[(size_t) current];
        for (auto& ch : channels)
        {
            size_t f = 0;
            for (int i = 0; i < m.highpassStages; ++i)
                ch.filters[f++].setHighpass (m.highpassHz, 0.7071f, fs);
            for (const auto& peak : m.peaks)
                if (peak[0] > 0.0f)
                    ch.filters[f++].setPeak (peak[0], peak[1], peak[2], fs);
            for (int pass = 0; pass < (m.holdHz > 0.0f ? 2 : 1); ++pass)
                for (int i = 0; i < m.lowpassStages; ++i)
                    ch.filters[f++].setLowpass (m.lowpassHz, 0.7071f, fs);
        }
        hissGain = std::pow (10.0f, m.hissDb / 20.0f);
        whineGain = m.whineHz < 0.45f * fs ? std::pow (10.0f, m.whineDb / 20.0f) : 0.0f;
    }

    static float muLaw8 (float x)
    {
        constexpr float mu = 255.0f;
        const float c = std::copysign (std::log1p (mu * std::min (std::abs (x), 1.0f)) / std::log1p (mu), x);
        const float q = std::round (c * 127.0f) / 127.0f;
        return std::copysign ((std::pow (1.0f + mu, std::abs (q)) - 1.0f) / mu, q);
    }

    float nextNoise()
    {
        noiseState = noiseState * 1664525u + 1013904223u;
        return (float) (noiseState >> 8) / 8388608.0f - 1.0f;
    }

    float fs = 48000.0f;
    Speaker current = Speaker::Off, pending = Speaker::Off;
    float amount = 0.0f, fadeStep = 0.001f;
    std::array<Channel, 2> channels;
    std::vector<std::array<float, 2>> wobble;
    std::size_t wobblePos = 0;
    float holdPhase = 1.0f;
    float envelope = 0.0f, envAttack = 0.0f, envRelease = 0.0f;
    float hissGain = 0.0f, whineGain = 0.0f;
    float wowPhase = 0.0f, flutterPhase = 0.0f, whinePhase = 0.0f;
    std::uint32_t noiseState = 22222u;
};

} // namespace detail
} // namespace halcyon
