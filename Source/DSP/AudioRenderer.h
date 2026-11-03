#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <functional>
#include <memory>
#include "ReverseProcessor.h"
#include "ReverbEngine.h"
#include "CharacterProcessor.h"

namespace anticip8
{
    struct RenderSettings
    {
        float lengthSeconds = 2.3f;
        bool  fx = true;
        int   character = 0;          // Classic / Phone / Chorus
        bool  tremolo = false;
        float tremoloHz = 4.0f;       // from BPM × division
        float trimStart = 0.0f;       // 0..1 of input
        float trimEnd = 1.0f;
    };

    struct RenderedAudio
    {
        juce::AudioBuffer<float> audio;
        double sampleRate = 44100.0;
        int preRollSamples = 0;
        juce::String sourceName;
        RenderSettings settings;
    };

    class AudioRenderer
    {
    public:
        static constexpr float kTargetPeak = 0.8913f;

        // Settings gear is decorative — DSP always runs wide open (like real ANTICIP8).
        static constexpr float kModRateHz = 12.0f;
        static constexpr float kModDepth  = 1.0f;
        static constexpr float kDamping   = 1.0f;

        static juce::AudioBuffer<float> extractTrim (const juce::AudioBuffer<float>& input,
                                                     float trimStart, float trimEnd)
        {
            const int N = input.getNumSamples();
            const int nch = input.getNumChannels();
            const float a = juce::jlimit (0.0f, 1.0f, juce::jmin (trimStart, trimEnd));
            const float b = juce::jlimit (0.0f, 1.0f, juce::jmax (trimStart, trimEnd));
            int s0 = juce::jlimit (0, N, (int) std::lround (a * N));
            int s1 = juce::jlimit (0, N, (int) std::lround (b * N));
            if (s1 - s0 < 32) { s0 = 0; s1 = N; }
            juce::AudioBuffer<float> out (nch, s1 - s0);
            for (int c = 0; c < nch; ++c)
                out.copyFrom (c, 0, input, c, s0, s1 - s0);
            return out;
        }

        static std::shared_ptr<RenderedAudio> render (const juce::AudioBuffer<float>& fullInput, double sr,
                                                      const RenderSettings& s,
                                                      const std::function<bool()>& cancel,
                                                      juce::String& error)
        {
            auto input = extractTrim (fullInput, s.trimStart, s.trimEnd);
            const int N = input.getNumSamples();
            const int nch = juce::jmin (2, input.getNumChannels());
            if (N <= 0 || nch <= 0 || sr <= 0.0) { error = "L'audio d'entree est vide."; return nullptr; }

            float dryPeak = 0.0f;
            for (int c = 0; c < nch; ++c) dryPeak = std::max (dryPeak, input.getMagnitude (c, 0, N));
            if (dryPeak < 1.0e-9f) { error = "L'audio d'entree est silencieux."; return nullptr; }

            // Riser length only — the dry phrase is NOT appended (drop end of run-up on the word in the DAW).
            const int Lp = juce::jlimit ((int) (0.05 * sr), (int) (8.0 * sr),
                                         (int) std::lround (s.lengthSeconds * sr));

            auto result = std::make_shared<RenderedAudio>();
            result->sampleRate = sr;
            result->preRollSamples = Lp;
            result->settings = s;
            auto& out = result->audio;

            const auto character = (Character) juce::jlimit (0, 2, s.character);

            if (s.fx)
            {
                // Work buffer: reversed word + room for reverb tail (= riser length).
                const int total = Lp + N;
                juce::AudioBuffer<float> wet (nch, total);
                wet.clear();
                ReverseProcessor::copyReversed (input, wet, nch);

                // Character on reversed dry FIRST so Phone/Chorus colour the whole reverb tail.
                CharacterProcessor::process (wet, sr, character, kModRateHz, kModDepth);

                ReverbEngine reverb;
                reverb.prepare (sr, nch, s.lengthSeconds * 0.95f, kDamping);
                if (! reverb.process (wet, cancel))
                    return nullptr;
                if (cancel && cancel()) return nullptr;

                if (s.tremolo)
                    CharacterProcessor::applyTremolo (wet, sr, s.tremoloHz, kModDepth);

                // [reversed word | tail] → reverse → [riser | word]. Keep riser only.
                ReverseProcessor::reverseInPlace (wet);

                out.setSize (nch, Lp);
                for (int c = 0; c < nch; ++c)
                    out.copyFrom (c, 0, wet, c, 0, Lp);

                float wetPeak = 0.0f;
                for (int c = 0; c < nch; ++c)
                    wetPeak = std::max (wetPeak, out.getMagnitude (c, 0, Lp));
                if (wetPeak > 1.0e-9f)
                    out.applyGain (kTargetPeak / wetPeak);
            }
            else
            {
                // RAW: trimmed dry only (LISTEN plays this path via processor preview).
                out.setSize (nch, N);
                for (int c = 0; c < nch; ++c)
                    out.copyFrom (c, 0, input, c, 0, N);
                out.applyGain (kTargetPeak / dryPeak);
            }

            for (int c = 0; c < nch; ++c)
            {
                auto* d = out.getWritePointer (c);
                const int n = out.getNumSamples();
                for (int i = 0; i < n; ++i)
                    if (! std::isfinite (d[i])) d[i] = 0.0f;
            }

            const int nOut = out.getNumSamples();
            const int fi = juce::jmin (nOut / 4, (int) (0.008 * sr));
            const int fo = juce::jmin (nOut / 4, (int) (0.025 * sr));
            for (int c = 0; c < nch; ++c)
            {
                if (fi > 0) out.applyGainRamp (c, 0, fi, 0.0f, 1.0f);
                if (fo > 0) out.applyGainRamp (c, nOut - fo, fo, 1.0f, 0.0f);
            }
            return result;
        }
    };
}
