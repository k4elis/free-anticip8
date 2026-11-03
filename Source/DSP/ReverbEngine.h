#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <functional>
#include <vector>
#include <cmath>
#include "Diffusion.h"

namespace anticip8
{
    /**
        Offline reverb: input diffusion (4 all-pass) -> 8 parallel damped feedback combs
        -> output diffusion (3 all-pass). Comb feedback is derived from the requested RT60
        so the tail really decays by 60 dB over the chosen time. Each channel uses slightly
        different delay lengths (stereo decorrelation).
    */
    class ReverbEngine
    {
    public:
        void prepare (double sampleRate, int numChannels, float rt60Seconds, float damping01)
        {
            channels = juce::jlimit (1, 2, numChannels);
            const double rt60 = std::max (0.05, (double) rt60Seconds);
            const float damp = 0.05f + 0.60f * juce::jlimit (0.0f, 1.0f, damping01);
            const int spread = (int) std::lround (23.0 * sampleRate / 44100.0);

            static const double combMs[8] = { 31.3, 34.9, 38.3, 41.7, 45.1, 48.7, 52.3, 56.9 };

            for (int c = 0; c < channels; ++c)
            {
                combs[c].clear();
                for (double ms : combMs)
                {
                    const int len = (int) std::lround (ms * 0.001 * sampleRate) + c * spread;
                    const double g = std::pow (10.0, -3.0 * (double) len / (rt60 * sampleRate));
                    Comb cb;
                    cb.prepare (len, (float) std::min (g, 0.998), damp);
                    combs[c].push_back (std::move (cb));
                }
                const int extra = (int) std::lround (c * 0.00037 * sampleRate);
                inDiff[c].prepare (sampleRate, { 4.77, 3.59, 12.73, 9.31 }, { 0.75f, 0.75f, 0.625f, 0.625f }, extra);
                outDiff[c].prepare (sampleRate, { 7.1, 11.3, 17.9 }, { 0.6f, 0.55f, 0.5f }, extra);
            }
        }

        /** Wet-only, in place. Returns false if cancelled. */
        bool process (juce::AudioBuffer<float>& b, const std::function<bool()>& shouldCancel)
        {
            const int n = b.getNumSamples();
            for (int c = 0; c < std::min (channels, b.getNumChannels()); ++c)
            {
                float* d = b.getWritePointer (c);
                for (int i = 0; i < n; ++i)
                {
                    if ((i & 8191) == 0 && shouldCancel && shouldCancel())
                        return false;

                    const float x = inDiff[c].process (d[i] * 0.1f);
                    float s = 0.0f;
                    for (auto& cb : combs[c])
                        s += cb.process (x);
                    d[i] = outDiff[c].process (s * 0.125f);
                }
            }
            return true;
        }

    private:
        struct Comb
        {
            void prepare (int len, float feedback, float damping)
            {
                buf.assign ((size_t) std::max (1, len), 0.0f);
                pos = 0; fb = feedback; damp = damping; store = 0.0f;
            }
            float process (float in) noexcept
            {
                const float out = buf[(size_t) pos];
                store = out * (1.0f - damp) + store * damp;      // one-pole low-pass in the loop
                buf[(size_t) pos] = in + store * fb;
                if (++pos >= (int) buf.size()) pos = 0;
                return out;
            }
            std::vector<float> buf;
            int pos = 0;
            float fb = 0.0f, damp = 0.0f, store = 0.0f;
        };

        int channels = 2;
        std::vector<Comb> combs[2];
        Diffusion inDiff[2], outDiff[2];
    };
}
