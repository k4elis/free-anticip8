#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include <vector>
#include <cmath>
#include "CustomLookAndFeel.h"
#include "DSP/AudioRenderer.h"

namespace anticip8
{
    class WaveformComponent : public juce::Component, private juce::Timer
    {
    public:
        std::function<void()> onDragStart;
        std::function<void(float, float)> onTrimChanged;

        WaveformComponent()
        {
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
            startTimerHz (30);
            rebuildParticles();
        }

        void setResult (std::shared_ptr<const RenderedAudio> r)
        {
            result = std::move (r);
            rebuildPeaks();
            repaint();
        }

        void setInput (std::shared_ptr<const juce::AudioBuffer<float>> in)
        {
            input = std::move (in);
            rebuildInputPeaks();
            repaint();
        }

        void setFxMode (bool fx)               { if (fxMode != fx) { fxMode = fx; repaint(); } }
        void setTrim (float a, float b)        { trimA = a; trimB = b; repaint(); }
        void setBusy (bool b)                  { if (busy != b) { busy = b; repaint(); } }
        void setPlayhead (float fraction)      { playhead = fraction; }
        void setCanDrag (bool c)
        {
            canDrag = c;
            if (dragHandle < 0)
                setMouseCursor (c ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        }
        void setStatusNote (juce::String s)    { note = std::move (s); }
        void setLengthSeconds (float secs)
        {
            const float n = juce::jlimit (0.5f, 5.0f, secs);
            if (std::abs (n - lengthSec) > 0.01f)
            {
                lengthSec = n;
                rebuildParticles();
                repaint();
            }
        }

        void resized() override
        {
            rebuildPeaks();
            rebuildInputPeaks();
            rebuildParticles();
        }

        void paint (juce::Graphics& g) override
        {
            using namespace Palette;
            auto r = getLocalBounds().toFloat();
            g.setColour (panel);
            g.fillRoundedRectangle (r, 14.0f);
            g.setColour (violet.withAlpha (0.55f));
            g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.3f);

            auto area = r.reduced (18.0f, 16.0f);

            if (! fxMode)
                paintRaw (g, area);
            else
                paintFx (g, area);

            g.setColour (muted);
            g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::plain)));
            if (note.isNotEmpty())
                g.drawText (note, getLocalBounds().reduced (16, 12), juce::Justification::topLeft, false);
            g.drawText (busy ? "Rendu..." : (fxMode ? "Glisser-déposer dans la DAW" : "Coupe la zone source"),
                        getLocalBounds().reduced (16, 12), juce::Justification::topRight, false);

            if (fxMode && result == nullptr && ! busy)
            {
                g.setColour (text.withAlpha (0.35f));
                g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
                g.drawText ("Joue ton entrée une fois", getLocalBounds(), juce::Justification::centred, false);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            dragStarted = false;
            dragHandle = -1;
            if (! fxMode)
            {
                const auto area = getLocalBounds().toFloat().reduced (18.0f, 16.0f);
                const float xa = area.getX() + juce::jmin (trimA, trimB) * area.getWidth();
                const float xb = area.getX() + juce::jmax (trimA, trimB) * area.getWidth();
                if (std::abs (e.position.x - xa) < 10.0f) dragHandle = 0;
                else if (std::abs (e.position.x - xb) < 10.0f) dragHandle = 1;
            }
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (! fxMode && dragHandle >= 0)
            {
                const auto area = getLocalBounds().toFloat().reduced (18.0f, 16.0f);
                const float t = juce::jlimit (0.0f, 1.0f, (e.position.x - area.getX()) / area.getWidth());
                float a = juce::jmin (trimA, trimB), b = juce::jmax (trimA, trimB);
                if (dragHandle == 0) a = juce::jmin (t, b - 0.02f);
                else                 b = juce::jmax (t, a + 0.02f);
                trimA = a; trimB = b;
                if (onTrimChanged) onTrimChanged (a, b);
                repaint();
                return;
            }

            if (! dragStarted && canDrag && fxMode && e.getDistanceFromDragStart() > 6)
            {
                dragStarted = true;
                if (onDragStart) onDragStart();
            }
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            dragStarted = false;
            dragHandle = -1;
        }

        void mouseMove (const juce::MouseEvent& e) override
        {
            if (! fxMode)
            {
                const auto area = getLocalBounds().toFloat().reduced (18.0f, 16.0f);
                const float xa = area.getX() + juce::jmin (trimA, trimB) * area.getWidth();
                const float xb = area.getX() + juce::jmax (trimA, trimB) * area.getWidth();
                const bool near = std::abs (e.position.x - xa) < 10.0f || std::abs (e.position.x - xb) < 10.0f;
                setMouseCursor (near ? juce::MouseCursor::LeftRightResizeCursor
                                     : juce::MouseCursor::NormalCursor);
            }
        }

    private:
        struct Particle { float t, speed, size, spread, seed; };

        /** Rising curve; longer reverse = steeper late rise + denser right-side cloud. */
        juce::Point<float> pointOnRiser (juce::Rectangle<float> area, float t) const
        {
            const float lenN = juce::jlimit (0.0f, 1.0f, (lengthSec - 0.5f) / 4.5f);
            const float exp = 1.35f + lenN * 1.1f;          // longer → more dramatic rise
            const float eased = std::pow (juce::jlimit (0.0f, 1.0f, t), exp);
            return { area.getX() + t * area.getWidth(),
                     area.getBottom() - eased * area.getHeight() * (0.78f + lenN * 0.18f)
                                     - area.getHeight() * 0.04f };
        }

        void paintFx (juce::Graphics& g, juce::Rectangle<float> area)
        {
            using namespace Palette;
            const float lenN = juce::jlimit (0.0f, 1.0f, (lengthSec - 0.5f) / 4.5f);
            const float speedScale = juce::jmap (lenN, 0.55f, 1.25f); // longer = faster drift

            if (result != nullptr && ! peaks.empty())
            {
                const float midY = area.getCentreY();
                const float halfH = area.getHeight() * 0.42f;
                g.setColour (violet.withAlpha (busy ? 0.12f : 0.18f));
                for (size_t i = 0; i < peaks.size(); ++i)
                {
                    const float x = area.getX() + (float) i * area.getWidth() / (float) peaks.size();
                    const float top = midY - peaks[i].second * halfH;
                    const float bot = midY - peaks[i].first * halfH;
                    g.fillRect (x, top, 1.0f, juce::jmax (1.0f, bot - top));
                }
            }

            const float tAnim = (float) phase;
            juce::Path curve;
            const int steps = 72;
            for (int i = 0; i <= steps; ++i)
            {
                const float t = (float) i / (float) steps;
                const auto pt = pointOnRiser (area, t);
                if (i == 0) curve.startNewSubPath (pt);
                else        curve.lineTo (pt);
            }

            g.setColour (pink.withAlpha (0.16f + lenN * 0.12f));
            g.strokePath (curve, juce::PathStrokeType (5.0f + lenN * 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setGradientFill (juce::ColourGradient (pink.withAlpha (0.35f), area.getX(), area.getBottom(),
                                                     violetLight.withAlpha (0.95f), area.getRight(), area.getY(), false));
            g.strokePath (curve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            for (const auto& p : particles)
            {
                const float t = std::fmod (p.t + tAnim * p.speed * speedScale, 1.0f);
                const auto pt = pointOnRiser (area, t);
                const float jitter = std::sin (tAnim * 6.0f + p.seed) * p.spread * (0.6f + lenN);
                const float size = juce::jmap (t, 0.0f, 1.0f, 1.2f, p.size * (0.75f + lenN * 0.9f));
                const float a = juce::jmap (t, 0.0f, 1.0f, 0.20f, 0.25f + 0.75f * lenN) * (busy ? 0.45f : 1.0f);
                g.setColour ((t > 0.55f ? pinkLight : violetLight).withAlpha (a));
                g.fillEllipse (pt.x - size * 0.5f, pt.y - size * 0.5f + jitter, size, size);
            }

            const float ph = playhead >= 0.0f ? juce::jlimit (0.0f, 1.0f, playhead)
                                              : (0.35f + 0.25f * lenN + 0.06f * std::sin (tAnim * 1.4f));
            const auto pt = pointOnRiser (area, ph);
            g.setColour (pink.withAlpha (0.35f));
            g.fillEllipse (pt.x - 10.0f, pt.y - 10.0f, 20.0f, 20.0f);
            g.setColour (text);
            g.fillEllipse (pt.x - 4.5f, pt.y - 4.5f, 9.0f, 9.0f);
        }

        void paintRaw (juce::Graphics& g, juce::Rectangle<float> area)
        {
            using namespace Palette;
            const float midY = area.getCentreY();
            const float halfH = area.getHeight() * 0.45f;

            if (inputPeaks.empty())
            {
                g.setColour (dim.withAlpha (0.7f));
                g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
                g.drawText ("Pas encore d'enregistrement - ouvre ENTRÉE ou dépose un fichier", getLocalBounds(),
                            juce::Justification::centred, false);
                return;
            }

            g.setColour (violet.withAlpha (0.35f));
            for (size_t i = 0; i < inputPeaks.size(); ++i)
            {
                const float x = area.getX() + (float) i * area.getWidth() / (float) inputPeaks.size();
                const float top = midY - inputPeaks[i].second * halfH;
                const float bot = midY - inputPeaks[i].first * halfH;
                g.fillRect (x, top, 1.0f, juce::jmax (1.0f, bot - top));
            }

            const float xa = area.getX() + juce::jmin (trimA, trimB) * area.getWidth();
            const float xb = area.getX() + juce::jmax (trimA, trimB) * area.getWidth();

            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRect (area.getX(), area.getY(), xa - area.getX(), area.getHeight());
            g.fillRect (xb, area.getY(), area.getRight() - xb, area.getHeight());

            g.setColour (pink.withAlpha (0.9f));
            g.fillRect (xa - 1.0f, area.getY(), 2.0f, area.getHeight());
            g.fillRect (xb - 1.0f, area.getY(), 2.0f, area.getHeight());
            g.fillRoundedRectangle (xa - 5.0f, midY - 14.0f, 10.0f, 28.0f, 3.0f);
            g.fillRoundedRectangle (xb - 5.0f, midY - 14.0f, 10.0f, 28.0f, 3.0f);

            if (playhead >= 0.0f)
            {
                const float x = xa + (xb - xa) * juce::jlimit (0.0f, 1.0f, playhead);
                g.setColour (text.withAlpha (0.9f));
                g.fillRect (x, area.getY(), 1.5f, area.getHeight());
            }
        }

        void rebuildParticles()
        {
            particles.clear();
            const float lenN = juce::jlimit (0.0f, 1.0f, (lengthSec - 0.5f) / 4.5f);
            const int count = 40 + (int) std::lround (lenN * 90.0f); // 0.5s ≈ sparse, 5s ≈ dense cloud
            juce::Random rng (0xA71C18u ^ (uint32_t) juce::roundToInt (lengthSec * 100.0f));
            for (int i = 0; i < count; ++i)
            {
                Particle p;
                // bias toward the right (toward the word) as length grows
                p.t = std::pow (rng.nextFloat(), 1.0f - lenN * 0.45f);
                p.speed = 0.012f + rng.nextFloat() * (0.03f + lenN * 0.03f);
                p.size = 1.8f + rng.nextFloat() * (5.0f + lenN * 6.0f);
                p.spread = 1.5f + rng.nextFloat() * (6.0f + lenN * 10.0f);
                p.seed = rng.nextFloat() * 40.0f;
                particles.push_back (p);
            }
        }

        void rebuildPeaksFrom (const juce::AudioBuffer<float>& a, std::vector<std::pair<float, float>>& dest)
        {
            dest.clear();
            const int n = a.getNumSamples();
            const int w = juce::jmax (1, getWidth() - 36);
            if (n <= 0) return;
            dest.resize ((size_t) w);
            for (int x = 0; x < w; ++x)
            {
                const int s0 = (int) ((int64_t) x * n / w);
                const int s1 = juce::jmax (s0 + 1, (int) ((int64_t) (x + 1) * n / w));
                const int step = juce::jmax (1, (s1 - s0) / 256);
                float mn = 0.0f, mx = 0.0f;
                for (int c = 0; c < a.getNumChannels(); ++c)
                {
                    const float* d = a.getReadPointer (c);
                    for (int i = s0; i < s1 && i < n; i += step)
                    {
                        mn = juce::jmin (mn, d[i]);
                        mx = juce::jmax (mx, d[i]);
                    }
                }
                dest[(size_t) x] = { mn, mx };
            }
        }

        void rebuildPeaks()
        {
            peaks.clear();
            if (result != nullptr) rebuildPeaksFrom (result->audio, peaks);
        }

        void rebuildInputPeaks()
        {
            inputPeaks.clear();
            if (input != nullptr) rebuildPeaksFrom (*input, inputPeaks);
        }

        void timerCallback() override
        {
            phase += 0.012;
            if (phase > 1000.0) phase -= 1000.0;
            if (fxMode) repaint();
        }

        std::shared_ptr<const RenderedAudio> result;
        std::shared_ptr<const juce::AudioBuffer<float>> input;
        std::vector<std::pair<float, float>> peaks, inputPeaks;
        std::vector<Particle> particles;
        float playhead = -1.0f, trimA = 0.0f, trimB = 1.0f, lengthSec = 2.3f;
        double phase = 0.0;
        bool busy = false, canDrag = false, dragStarted = false, fxMode = true;
        int dragHandle = -1;
        juce::String note;
    };
}
