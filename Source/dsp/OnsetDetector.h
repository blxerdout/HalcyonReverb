#pragma once

// Finds the start of each new sound (a hit, a note, a phrase) in a stereo
// signal, so the UI can line its playhead up with what's actually playing.
//
// A fast envelope follower is compared with a slow one: an onset is a jump of
// the fast one well above the slow one. It then has to settle back down before
// it can fire again, so a held note or a steady loop triggers once, not
// continuously. Plain C++17, no JUCE, real-time safe.

#include <algorithm>
#include <cmath>

namespace halcyon
{

class OnsetDetector
{
public:
    void prepare (double sampleRate)
    {
        const auto coef = [sampleRate] (double seconds) { return (float) (1.0 - std::exp (-1.0 / (seconds * sampleRate))); };
        attack = coef (0.0005);
        release = coef (0.03);
        slowCoef = coef (0.3);
        minGapSamples = (long) (0.08 * sampleRate);
        reset();
    }

    void reset()
    {
        fast = slow = 0.0f;
        armed = true;
        sinceOnset = minGapSamples;
    }

    // Returns the index of the first onset in the block, or -1.
    int process (const float* left, const float* right, int numSamples)
    {
        int found = -1;
        for (int i = 0; i < numSamples; ++i)
        {
            const float x = std::max (std::abs (left[i]), std::abs (right[i]));
            fast += (x > fast ? attack : release) * (x - fast);
            slow += slowCoef * (x - slow);
            ++sinceOnset;

            if (armed && fast > threshold && fast > 2.0f * slow && sinceOnset >= minGapSamples)
            {
                armed = false;
                sinceOnset = 0;
                if (found < 0)
                    found = i;
            }
            else if (! armed && fast < 1.3f * slow)
            {
                armed = true;
            }
        }
        return found;
    }

private:
    static constexpr float threshold = 0.003f; // about -50 dBFS

    float attack = 0.0f, release = 0.0f, slowCoef = 0.0f;
    float fast = 0.0f, slow = 0.0f;
    bool armed = true;
    long sinceOnset = 0, minGapSamples = 0;
};

} // namespace halcyon
