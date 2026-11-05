// Offline checks for the reverb engine. Builds without JUCE:
//   clang++ -std=c++17 -O2 -I Source tests/EngineTests.cpp Source/dsp/ReverbEngine.cpp -o engine_tests
// Pass a directory as the first argument to also write impulse responses as WAV files.

#include "dsp/FactoryPresets.h"
#include "dsp/OnsetDetector.h"
#include "dsp/ReverbEngine.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <random>
#include <string>

using namespace halcyon;

namespace
{

constexpr double sampleRate = 48000.0;
int failures = 0;

void check (bool ok, const std::string& what)
{
    std::printf ("  [%s] %s\n", ok ? "pass" : "FAIL", what.c_str());
    if (! ok)
        ++failures;
}

const char* modeName (Mode m)
{
    static const char* names[] { "Plate", "Hall", "Room", "Chamber", "Cathedral" };
    return names[(int) m];
}

const char* colorName (Color c)
{
    static const char* names[] { "Modern", "Vintage", "Dark" };
    return names[(int) c];
}

struct Stereo
{
    std::vector<float> l, r;
};

Stereo render (ReverbParams p, const Stereo& input, int extraSamples)
{
    ReverbEngine engine;
    engine.prepare (sampleRate);
    engine.setParams (p);
    engine.reset();

    Stereo out;
    out.l = input.l;
    out.r = input.r;
    out.l.resize (input.l.size() + (size_t) extraSamples, 0.0f);
    out.r.resize (input.r.size() + (size_t) extraSamples, 0.0f);

    constexpr int block = 256;
    for (size_t pos = 0; pos < out.l.size(); pos += block)
    {
        const int n = (int) std::min ((size_t) block, out.l.size() - pos);
        engine.process (out.l.data() + pos, out.r.data() + pos, n);
    }
    return out;
}

Stereo impulse()
{
    Stereo s;
    s.l = { 1.0f };
    s.r = { 1.0f };
    return s;
}

bool allFinite (const Stereo& s, float& peak)
{
    peak = 0.0f;
    for (size_t i = 0; i < s.l.size(); ++i)
    {
        if (! std::isfinite (s.l[i]) || ! std::isfinite (s.r[i]))
            return false;
        peak = std::max ({ peak, std::abs (s.l[i]), std::abs (s.r[i]) });
    }
    return true;
}

// RT60 from Schroeder backward integration, fitted between -5 and -35 dB.
double measureRT60 (const Stereo& s)
{
    std::vector<double> energy (s.l.size());
    double acc = 0.0;
    for (size_t i = s.l.size(); i-- > 0;)
    {
        acc += (double) s.l[i] * s.l[i] + (double) s.r[i] * s.r[i];
        energy[i] = acc;
    }

    const double total = energy[0];
    size_t t5 = 0, t35 = 0;
    for (size_t i = 0; i < energy.size(); ++i)
    {
        const double db = 10.0 * std::log10 (energy[i] / total + 1e-30);
        if (t5 == 0 && db <= -5.0) t5 = i;
        if (db <= -35.0) { t35 = i; break; }
    }
    if (t35 == 0)
        return -1.0;
    return 2.0 * (double) (t35 - t5) / sampleRate;
}

// Correlation of left and right wet signals; lower means wider.
double correlation (const Stereo& s)
{
    double lr = 0, ll = 0, rr = 0;
    for (size_t i = 0; i < s.l.size(); ++i)
    {
        lr += (double) s.l[i] * s.r[i];
        ll += (double) s.l[i] * s.l[i];
        rr += (double) s.r[i] * s.r[i];
    }
    return lr / std::sqrt (ll * rr + 1e-30);
}

void writeWav (const std::string& path, const Stereo& s)
{
    std::ofstream f (path, std::ios::binary);
    auto u32 = [&f] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto u16 = [&f] (uint16_t v) { f.write ((const char*) &v, 2); };

    const uint32_t frames = (uint32_t) s.l.size();
    const uint32_t dataBytes = frames * 2 * 4;
    f.write ("RIFF", 4); u32 (36 + dataBytes); f.write ("WAVE", 4);
    f.write ("fmt ", 4); u32 (16); u16 (3); u16 (2);
    u32 ((uint32_t) sampleRate); u32 ((uint32_t) sampleRate * 8); u16 (8); u16 (32);
    f.write ("data", 4); u32 (dataBytes);
    for (uint32_t i = 0; i < frames; ++i)
    {
        f.write ((const char*) &s.l[i], 4);
        f.write ((const char*) &s.r[i], 4);
    }
}

} // namespace

int main (int argc, char** argv)
{
    const std::string wavDir = argc > 1 ? argv[1] : "";

    std::printf ("Silence in, silence out\n");
    {
        Stereo silence;
        silence.l.assign (48000, 0.0f);
        silence.r.assign (48000, 0.0f);
        float peak = 0.0f;
        const bool finite = allFinite (render ({}, silence, 0), peak);
        check (finite && peak == 0.0f, "no output from silence");
    }

    std::printf ("\nImpulse responses (mix 100%%, decay 2.0 s, size 50%%)\n");
    std::printf ("  %-10s %-8s %8s %8s %8s\n", "mode", "color", "RT60", "peak", "L/R corr");
    for (int m = 0; m < 5; ++m)
        for (int c = 0; c < 3; ++c)
        {
            ReverbParams p;
            p.mix = 1.0f;
            p.decaySeconds = 2.0f;
            p.highCutHz = 20000.0f;
            p.lowCutHz = 20.0f;
            p.preDelayMs = 0.0f;
            p.mode = (Mode) m;
            p.color = (Color) c;

            const Stereo ir = render (p, impulse(), (int) (8 * sampleRate));
            float peak = 0.0f;
            const bool finite = allFinite (ir, peak);
            const double rt = measureRT60 (ir);
            std::printf ("  %-10s %-8s %7.2fs %8.3f %8.3f\n", modeName (p.mode), colorName (p.color), rt, peak, correlation (ir));

            check (finite, "finite output");
            // Damping and band-limiting shorten the broadband RT60 a little, so allow a window.
            check (rt > 1.2 && rt < 2.6, "RT60 near the 2.0 s setting");
            check (peak < 1.5f, "impulse response peak bounded");

            if (! wavDir.empty() && c == 0)
                writeWav (wavDir + "/ir_" + modeName (p.mode) + ".wav", ir);
        }

    std::printf ("\nDecay control tracks setting (Hall, Modern)\n");
    for (float target : { 0.5f, 1.0f, 4.0f, 8.0f })
    {
        ReverbParams p;
        p.mix = 1.0f;
        p.decaySeconds = target;
        p.highCutHz = 20000.0f;
        p.lowCutHz = 20.0f;
        const double rt = measureRT60 (render (p, impulse(), (int) (target * 3.0f * sampleRate)));
        char label[64];
        std::snprintf (label, sizeof (label), "setting %.1f s measured %.2f s", target, rt);
        check (rt > target * 0.75 && rt < target * 1.25, label);
    }

    std::printf ("\nSize reshapes the room (decay 2.0 s)\n");
    std::printf ("  %-10s %22s %22s %10s\n", "mode", "build-up 0/50/100%", "RT60 0/100%", "worst gap");
    for (int m = 0; m < 5; ++m)
    {
        double centre[3] {}, rt[3] {}, worstGap = 0.0;
        for (int s = 0; s < 3; ++s)
        {
            ReverbParams p;
            p.mix = 1.0f;
            p.decaySeconds = 2.0f;
            p.preDelayMs = 0.0f;
            p.modDepth = 0.0f;
            p.mode = (Mode) m;
            p.size = 0.5f * (float) s;
            const Stereo ir = render (p, impulse(), (int) (6 * sampleRate));
            rt[s] = measureRT60 (ir);

            // Centre time of the first 500 ms: when the room has "filled in".
            double sum = 0.0, weighted = 0.0;
            for (size_t i = 0; i < (size_t) (0.5 * sampleRate); ++i)
            {
                const double e = (double) ir.l[i] * ir.l[i] + (double) ir.r[i] * ir.r[i];
                sum += e;
                weighted += e * (double) i;
            }
            centre[s] = weighted / sum / sampleRate * 1000.0;

            // In 20 ms windows (5 ms apart), the deepest drop below what came before,
            // up to the peak. A big one is a silent hole between the reflections and
            // the tail; shorter gaps, like those between single reflections, aren't heard.
            std::vector<double> bins;
            for (size_t b = 0; b + 4 <= (size_t) (0.5 * sampleRate) / 240; ++b)
            {
                double e = 0.0;
                for (size_t i = b * 240; i < (b + 4) * 240; ++i)
                    e += (double) ir.l[i] * ir.l[i] + (double) ir.r[i] * ir.r[i];
                bins.push_back (10.0 * std::log10 (e + 1e-30));
            }
            const auto peakBin = std::max_element (bins.begin(), bins.end());
            double runningMax = -300.0;
            for (auto it = bins.begin(); it != peakBin; ++it)
            {
                if (runningMax > *peakBin - 20.0)
                    worstGap = std::max (worstGap, runningMax - *it);
                runningMax = std::max (runningMax, *it);
            }
        }

        std::printf ("  %-10s %6.0f/%4.0f/%4.0f ms %13.2fs/%.2fs %8.1f dB\n", modeName ((Mode) m),
                     centre[0], centre[1], centre[2], rt[0], rt[2], worstGap);
        check (centre[0] < centre[1] && centre[1] < centre[2] && centre[2] > 1.4 * centre[0],
               "bigger size fills in later");
        check (rt[0] > 1.2 && rt[0] < 2.6 && rt[2] > 1.2 && rt[2] < 2.6, "RT60 still follows Decay at both ends");
        check (worstGap < 8.0, "no hole between reflections and tail");
    }

    std::printf ("\nWorst-case stability (30 s decay, max diffusion and modulation, loud noise)\n");
    for (int c = 0; c < 3; ++c)
    {
        ReverbParams p;
        p.mix = 1.0f;
        p.decaySeconds = 30.0f;
        p.diffusion = 1.0f;
        p.modDepth = 1.0f;
        p.modRateHz = 5.0f;
        p.size = 1.0f;
        p.highCutHz = 20000.0f;
        p.mode = Mode::Cathedral;
        p.color = (Color) c;

        std::mt19937 rng (1234);
        std::uniform_real_distribution<float> dist (-1.0f, 1.0f);
        Stereo noise;
        for (int i = 0; i < 2 * 48000; ++i)
        {
            noise.l.push_back (dist (rng));
            noise.r.push_back (dist (rng));
        }

        float peak = 0.0f;
        const bool finite = allFinite (render (p, noise, 20 * 48000), peak);
        char label[64];
        std::snprintf (label, sizeof (label), "%s: finite and bounded (peak %.2f)", colorName (p.color), peak);
        check (finite && peak < 20.0f, label);
    }

    std::printf ("\nParameter changes while running\n");
    {
        ReverbEngine engine;
        engine.prepare (sampleRate);
        ReverbParams p;
        p.mix = 1.0f;
        engine.setParams (p);
        engine.reset();

        std::mt19937 rng (99);
        std::uniform_real_distribution<float> dist (-0.5f, 0.5f);
        std::vector<float> l (256), r (256);
        bool finite = true;
        for (int b = 0; b < 2000; ++b)
        {
            for (int i = 0; i < 256; ++i) { l[(size_t) i] = dist (rng); r[(size_t) i] = dist (rng); }
            p.size = 0.5f + 0.5f * std::sin ((float) b * 0.05f);
            p.preDelayMs = 250.0f + 250.0f * std::sin ((float) b * 0.03f);
            if (b % 200 == 0)
                p.mode = (Mode) ((b / 200) % 5);
            engine.setParams (p);
            engine.process (l.data(), r.data(), 256);
            for (int i = 0; i < 256; ++i)
                finite = finite && std::isfinite (l[(size_t) i]) && std::isfinite (r[(size_t) i]);
        }
        check (finite, "size, pre-delay and mode sweeps stay finite");
    }

    std::printf ("\nLofi\n");
    {
        ReverbParams p;
        p.mix = 1.0f;
        p.mode = Mode::Hall;
        const Stereo clean = render (p, impulse(), 2 * 48000);
        p.lofi = true;
        const Stereo crushed = render (p, impulse(), 2 * 48000);

        float peak = 0.0f;
        const bool finite = allFinite (crushed, peak);
        double diff = 0.0, energy = 0.0;
        for (size_t i = 0; i < clean.l.size(); ++i)
        {
            diff += std::pow (crushed.l[i] - clean.l[i], 2.0);
            energy += std::pow (clean.l[i], 2.0);
        }
        check (finite && peak < 1.5f, "finite and bounded");
        check (diff > 0.1 * energy, "audibly changes the wet signal");

        // Toggling mid-stream crossfades rather than jumping.
        ReverbEngine engine;
        engine.prepare (sampleRate);
        p.lofi = false;
        engine.setParams (p);
        engine.reset();
        std::vector<float> l (4800, 0.2f), r (4800, 0.2f);
        engine.process (l.data(), r.data(), 4800);
        p.lofi = true;
        engine.setParams (p);
        std::vector<float> l2 (2, 0.2f), r2 (2, 0.2f);
        engine.process (l2.data(), r2.data(), 2);
        check (std::abs (l2[0] - l.back()) < 0.05f, "switching on does not click");
    }

    std::printf ("\nFactory presets (loud noise, then tail)\n");
    {
        int unstable = 0, misfiled = 0;
        std::mt19937 rng (7);
        std::uniform_real_distribution<float> dist (-1.0f, 1.0f);
        Stereo noise;
        for (int i = 0; i < 48000; ++i)
        {
            noise.l.push_back (dist (rng));
            noise.r.push_back (dist (rng));
        }

        for (int i = 0; i < numFactoryPresets; ++i)
        {
            const auto& preset = *getFactoryPreset (i);
            if (i < 5 * presetsPerMode && (int) preset.params.mode != i / presetsPerMode)
                ++misfiled;

            float peak = 0.0f;
            if (! allFinite (render (preset.params, noise, 4 * 48000), peak) || peak > 20.0f)
            {
                std::printf ("  %s: peak %.2f\n", preset.name, peak);
                ++unstable;
            }
        }
        check (misfiled == 0, "each preset sits in its Mode's group of ten");
        check (unstable == 0, std::to_string (numFactoryPresets) + " presets finite and bounded");
        check (getFactoryPreset (numFactoryPresets - 1) != nullptr && getFactoryPreset (numFactoryPresets) == nullptr,
               "preset count matches the tables");
    }

    std::printf ("\nSpeakers (dry signal only)\n");
    {
        // Level of a sine after the speaker, relative to the input, in dB.
        auto sineGain = [] (Speaker sp, float hz)
        {
            ReverbParams p;
            p.mix = 0.0f;
            p.speaker = sp;
            Stereo sine;
            for (int i = 0; i < 48000; ++i)
            {
                const float v = 0.05f * std::sin (6.2831853f * hz * (float) i / 48000.0f);
                sine.l.push_back (v);
                sine.r.push_back (v);
            }
            const Stereo out = render (p, sine, 0);
            double in = 0.0, outE = 0.0;
            for (size_t i = 9600; i < sine.l.size(); ++i)
            {
                in += (double) sine.l[i] * sine.l[i];
                outE += (double) out.l[i] * out.l[i];
            }
            return 10.0 * std::log10 (outE / in + 1e-30);
        };

        const char* names[] { "Off", "Cell Phone", "Landline", "CRT TV", "Boom Box", "Arcade" };
        std::printf ("  %-11s %7s %7s %7s\n", "speaker", "120 Hz", "1 kHz", "5 kHz");
        double gain[numSpeakers][3] {};
        for (int s = 0; s < numSpeakers; ++s)
        {
            gain[s][0] = sineGain ((Speaker) s, 120.0f);
            gain[s][1] = sineGain ((Speaker) s, 1000.0f);
            gain[s][2] = sineGain ((Speaker) s, 5000.0f);
            std::printf ("  %-11s %7.1f %7.1f %7.1f\n", names[s], gain[s][0], gain[s][1], gain[s][2]);

            ReverbParams p;
            p.speaker = (Speaker) s;
            Stereo silence;
            silence.l.assign (48000, 0.0f);
            silence.r.assign (48000, 0.0f);
            float peak = 0.0f;
            check (allFinite (render (p, silence, 0), peak) && peak == 0.0f,
                   std::string (names[s]) + ": silence in, silence out (no idle hiss)");
            check (std::abs (gain[s][1]) < 9.0, std::string (names[s]) + ": 1 kHz level in range");
        }

        const auto cell = (int) Speaker::CellPhone, line = (int) Speaker::Landline, box = (int) Speaker::BoomBox;
        check (gain[0][0] == 0.0 && gain[0][1] == 0.0 && gain[0][2] == 0.0, "Off passes audio untouched");
        check (gain[cell][0] < gain[cell][1] - 30.0, "Cell Phone has no bass");
        check (gain[line][0] < gain[line][1] - 25.0 && gain[line][2] < gain[line][1] - 20.0,
               "Landline is band-limited to the telephone band");
        check (gain[box][0] > gain[box][1], "Boom Box keeps (boosts) the bass");

        // Switching models mid-signal fades through, with no jump.
        ReverbParams p;
        p.mix = 0.0f;
        ReverbEngine engine;
        engine.prepare (sampleRate);
        engine.setParams (p);
        engine.reset();
        float worstStep = 0.0f, last = 0.0f;
        for (int block = 0; block < 400; ++block)
        {
            if (block % 40 == 0)
            {
                p.speaker = (Speaker) ((block / 40) % numSpeakers);
                engine.setParams (p);
            }
            std::array<float, 64> l {}, r {};
            for (size_t i = 0; i < l.size(); ++i)
                l[i] = r[i] = 0.3f * std::sin (6.2831853f * 220.0f * (float) (block * 64 + (int) i) / 48000.0f);
            engine.process (l.data(), r.data(), (int) l.size());
            for (float v : l)
            {
                worstStep = std::max (worstStep, std::abs (v - last));
                last = v;
            }
        }
        check (worstStep < 0.1f, "switching speakers doesn't click");
    }

    std::printf ("\nOnset detector (drives the Visual tab playhead)\n");
    {
        auto countOnsets = [] (const std::vector<float>& signal, std::vector<int>& positions)
        {
            OnsetDetector detector;
            detector.prepare (sampleRate);
            constexpr int block = 256;
            positions.clear();
            for (size_t pos = 0; pos < signal.size(); pos += block)
            {
                const int n = (int) std::min ((size_t) block, signal.size() - pos);
                const int hit = detector.process (signal.data() + pos, signal.data() + pos, n);
                if (hit >= 0)
                    positions.push_back ((int) pos + hit);
            }
            return (int) positions.size();
        };

        std::vector<int> at;
        check (countOnsets (std::vector<float> (48000, 0.0f), at) == 0, "silence: no onsets");

        // Two drum-like hits half a second apart.
        std::vector<float> hits (96000, 0.0f);
        for (int start : { 4800, 28800 })
            for (int i = 0; i < 4800; ++i)
                hits[(size_t) (start + i)] = 0.8f * std::exp (-(float) i / 600.0f) * std::sin ((float) i * 0.3f);
        const bool twoHits = countOnsets (hits, at) == 2 && std::abs (at[0] - 4800) < 48 && std::abs (at[1] - 28800) < 48;
        check (twoHits, "two hits found within 1 ms of where they start");

        // A sustained tone triggers once at its start, not continuously.
        std::vector<float> held (96000, 0.0f);
        for (size_t i = 9600; i < held.size(); ++i)
            held[i] = 0.5f * std::sin ((float) i * 0.05f);
        check (countOnsets (held, at) == 1, "held note triggers once");

        // Very quiet noise stays under the threshold.
        std::mt19937 rng (5);
        std::uniform_real_distribution<float> dist (-0.001f, 0.001f);
        std::vector<float> hiss (48000);
        for (auto& v : hiss)
            v = dist (rng);
        check (countOnsets (hiss, at) == 0, "background hiss ignored");
    }

    std::printf ("\nCPU\n");
    {
        ReverbParams p;
        p.mode = Mode::Cathedral;
        Stereo noise;
        noise.l.assign (10 * 48000, 0.1f);
        noise.r.assign (10 * 48000, 0.1f);
        const auto start = std::chrono::steady_clock::now();
        render (p, noise, 0);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
        std::printf ("  10 s of stereo audio in %.3f s (%.1f%% of one core)\n", secs, secs / 10.0 * 100.0);
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
