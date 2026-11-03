#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>

namespace anticip8
{
    enum class Character { Classic = 0, Phone, Chorus };

    /** Characters + optional tremolo overlay. Operates offline, in place.
        Applied on the reversed dry *before* reverb so the riser really changes colour. */
    class CharacterProcessor
    {
    public:
        static void process (juce::AudioBuffer<float>& b, double sr, Character c, float rateHz, float depth01)
        {
            switch (c)
            {
                case Character::Classic: classic (b, sr); break;
                case Character::Phone:   phone (b, sr); break;
                case Character::Chorus:  chorus (b, sr, rateHz, depth01); break;
            }
        }

        /** Pulse that builds toward the end of the buffer (toward the word), then gates. */
        static void applyTremolo (juce::AudioBuffer<float>& b, double sr, float rateHz, float depth01)
        {
            const int n = b.getNumSamples();
            if (n <= 0) return;
            const double inc = std::max (0.05, (double) rateHz) / sr;
            const float depth = juce::jlimit (0.0f, 1.0f, depth01);
            for (int c = 0; c < b.getNumChannels(); ++c)
            {
                auto* d = b.getWritePointer (c);
                double ph = 0.25 * c;
                for (int i = 0; i < n; ++i)
                {
                    const float t = n > 1 ? (float) i / (float) (n - 1) : 1.0f;
                    const float env = t < 0.92f ? (t / 0.92f) : juce::jmax (0.0f, 1.0f - (t - 0.92f) / 0.08f * 4.0f);
                    const float lfo = 0.5f - 0.5f * (float) std::cos (2.0 * juce::MathConstants<double>::pi * ph);
                    d[i] *= 1.0f - depth * env * lfo;
                    ph += inc; if (ph >= 1.0) ph -= 1.0;
                }
            }
        }

        static float divisionToHz (int divisionIndex, double bpm)
        {
            const double b = bpm > 1.0 ? bpm : 120.0;
            static const double mult[] = { 2.0, 4.0, 8.0, 16.0 };
            const int i = juce::jlimit (0, 3, divisionIndex);
            return (float) ((b / 60.0) * mult[i]);
        }

    private:
        struct Biquad
        {
            double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

            void set (double sr, double freq, double q, bool highPass)
            {
                freq = std::min (freq, sr * 0.45);
                const double w0 = 2.0 * juce::MathConstants<double>::pi * freq / sr;
                const double cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
                const double a0 = 1.0 + alpha;
                if (highPass) { b0 = (1 + cw) * 0.5; b1 = -(1 + cw); b2 = (1 + cw) * 0.5; }
                else          { b0 = (1 - cw) * 0.5; b1 =  (1 - cw); b2 = (1 - cw) * 0.5; }
                b0 /= a0; b1 /= a0; b2 /= a0;
                a1 = -2.0 * cw / a0; a2 = (1.0 - alpha) / a0;
                z1 = z2 = 0.0;
            }

            void setPeak (double sr, double freq, double q, double gainDb)
            {
                freq = std::min (freq, sr * 0.45);
                const double A = std::pow (10.0, gainDb / 40.0);
                const double w0 = 2.0 * juce::MathConstants<double>::pi * freq / sr;
                const double cw = std::cos (w0), sw = std::sin (w0);
                const double alpha = sw / (2.0 * q);
                const double a0 = 1.0 + alpha / A;
                b0 = (1.0 + alpha * A) / a0;
                b1 = (-2.0 * cw) / a0;
                b2 = (1.0 - alpha * A) / a0;
                a1 = (-2.0 * cw) / a0;
                a2 = (1.0 - alpha / A) / a0;
                z1 = z2 = 0.0;
            }

            float process (float x) noexcept
            {
                const double y = b0 * x + z1;
                z1 = b1 * x - a1 * y + z2;
                z2 = b2 * x - a2 * y;
                return (float) y;
            }
        };

        /** Full-band, airy — mild presence lift, gentle HPF. */
        static void classic (juce::AudioBuffer<float>& b, double sr)
        {
            for (int c = 0; c < b.getNumChannels(); ++c)
            {
                Biquad hp, air, body;
                hp.set (sr, 40.0, 0.707, true);
                body.setPeak (sr, 220.0, 0.7, 1.5);
                air.setPeak (sr, 5500.0, 0.8, 3.0);
                auto* d = b.getWritePointer (c);
                for (int i = 0; i < b.getNumSamples(); ++i)
                    d[i] = air.process (body.process (hp.process (d[i])));
            }
        }

        /** Telephone: tight band + crunch — obviously different from Classic. */
        static void phone (juce::AudioBuffer<float>& b, double sr)
        {
            float ref = 0.0f;
            for (int c = 0; c < b.getNumChannels(); ++c)
                ref = std::max (ref, b.getMagnitude (c, 0, b.getNumSamples()));
            if (ref < 1.0e-9f) return;

            const float drive = 2.8f;
            const float norm = 1.0f / std::tanh (drive);
            for (int c = 0; c < b.getNumChannels(); ++c)
            {
                Biquad hp1, hp2, lp1, lp2, mid;
                hp1.set (sr, 380.0, 0.707, true);
                hp2.set (sr, 380.0, 0.9, true);
                lp1.set (sr, 2800.0, 0.707, false);
                lp2.set (sr, 2800.0, 0.9, false);
                mid.setPeak (sr, 1200.0, 1.2, 6.0);
                auto* d = b.getWritePointer (c);
                for (int i = 0; i < b.getNumSamples(); ++i)
                {
                    float x = mid.process (lp2.process (lp1.process (hp2.process (hp1.process (d[i])))));
                    const float t = b.getNumSamples() > 1 ? (float) i / (float) (b.getNumSamples() - 1) : 1.0f;
                    // band opens a bit toward the hit
                    const float open = 0.85f + 0.35f * t;
                    d[i] = std::tanh (drive * x / ref) * norm * ref * open * 1.15f;
                }
            }
        }

        /** Wide modulated double — clearly thicker / swirly vs Classic. */
        static void chorus (juce::AudioBuffer<float>& b, double sr, float /*rateHz*/, float /*depth01*/)
        {
            const int n = b.getNumSamples();
            const int maxDelay = (int) (0.045 * sr) + 8;
            // Fixed musical rates (settings are decorative / always maxed)
            const double lfoHz = 0.85;
            const double modMs = 7.5;
            const double baseMs[3] = { 11.0, 17.0, 23.0 };
            const float voiceGain = 0.55f;

            for (int c = 0; c < b.getNumChannels(); ++c)
            {
                std::vector<float> line ((size_t) maxDelay, 0.0f);
                int w = 0;
                auto* d = b.getWritePointer (c);
                const double chanPhase = 0.33 * c;
                double ph = 0.0;
                const double inc = lfoHz / sr;

                Biquad hp, air;
                hp.set (sr, 80.0, 0.707, true);
                air.setPeak (sr, 4000.0, 0.7, 2.5);

                for (int i = 0; i < n; ++i)
                {
                    const float x = d[i];
                    line[(size_t) w] = x;
                    float wet = 0.0f;
                    for (int v = 0; v < 3; ++v)
                    {
                        const double lfo = std::sin (2.0 * juce::MathConstants<double>::pi
                                                     * (ph + chanPhase + 0.37 * v));
                        const double delay = (baseMs[v] + modMs * lfo) * 0.001 * sr;
                        double rp = (double) w - delay;
                        while (rp < 0.0) rp += maxDelay;
                        const int i0 = ((int) rp) % maxDelay;
                        const float fr = (float) (rp - (int) rp);
                        const int i1 = (i0 + 1) % maxDelay;
                        wet += line[(size_t) i0] * (1.0f - fr) + line[(size_t) i1] * fr;
                    }
                    wet *= voiceGain / 3.0f;
                    // strong wet mix so Chorus is unmistakable
                    float y = 0.45f * x + 0.70f * wet;
                    // slight stereo-ish offset already via chanPhase; brighten
                    y = air.process (hp.process (y));
                    d[i] = y;
                    if (++w >= maxDelay) w = 0;
                    ph += inc; if (ph >= 1.0) ph -= 1.0;
                }
            }
        }
    };
}
