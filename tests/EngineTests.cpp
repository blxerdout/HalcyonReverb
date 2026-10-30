// Offline checks for the reverb engine. Builds without JUCE:
//   clang++ -std=c++17 -O2 -I Source tests/EngineTests.cpp Source/dsp/ReverbEngine.cpp -o engine_tests
// Pass a directory as the first argument to also write impulse responses as WAV files.

#include "dsp/FactoryPresets.h"
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

        for (size_t i = 0; i < factoryPresets.size(); ++i)
        {
            const auto& preset = factoryPresets[i];
            if ((int) preset.params.mode != (int) i / presetsPerMode)
                ++misfiled;

            float peak = 0.0f;
            if (! allFinite (render (preset.params, noise, 4 * 48000), peak) || peak > 20.0f)
            {
                std::printf ("  %s: peak %.2f\n", preset.name, peak);
                ++unstable;
            }
        }
        check (misfiled == 0, "each preset sits in its Mode's group of ten");
        check (unstable == 0, std::to_string (factoryPresets.size()) + " presets finite and bounded");
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
