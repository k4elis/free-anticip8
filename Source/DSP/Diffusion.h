#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace anticip8
{
    /** Schroeder all-pass: y[n] = -g*x[n] + x[n-D] + g*y[n-D]. */
    class Allpass
    {
    public:
        void prepare (int delaySamples, float gain)
        {
            buf.assign ((size_t) std::max (1, delaySamples), 0.0f);
            pos = 0;
            g = gain;
        }

        float process (float x) noexcept
        {
            const float d = buf[(size_t) pos];
            const float w = x + g * d;
            buf[(size_t) pos] = w;
            if (++pos >= (int) buf.size()) pos = 0;
            return d - g * w;
        }

    private:
        std::vector<float> buf;
        int pos = 0;
        float g = 0.5f;
    };

    /** A chain of all-pass stages used to smear echoes into a dense, smooth wash. */
    class Diffusion
    {
    public:
        void prepare (double sampleRate, const std::vector<double>& delaysMs, const std::vector<float>& gains, int extraSamples)
        {
            stages.clear();
            stages.resize (delaysMs.size());
            for (size_t i = 0; i < delaysMs.size(); ++i)
                stages[i].prepare ((int) std::lround (delaysMs[i] * 0.001 * sampleRate) + extraSamples, gains[i]);
        }

        float process (float x) noexcept
        {
            for (auto& s : stages)
                x = s.process (x);
            return x;
        }

    private:
        std::vector<Allpass> stages;
    };
}
