#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>
#include <vector>
#include <cmath>
#include "CustomLookAndFeel.h"

namespace anticip8
{
    /** Large hero dial — readout % · s, orbit particles scale with duration. */
    class ReverseKnob : public juce::Component, private juce::Timer
    {
    public:
        ReverseKnob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID)
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            // Match real VST arc (about 7 o'clock → 5 o'clock)
            slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.15f,
                                        juce::MathConstants<float>::pi * 2.85f, true);
            if (auto* p = apvts.getParameter (paramID))
                slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
            addAndMakeVisible (slider);
            attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramID, slider);
            rebuildOrbit();
            startTimerHz (30);
        }

        float getSeconds() const { return (float) slider.getValue(); }
        float getPercent() const
        {
            const float secs = getSeconds();
            return juce::jlimit (0.0f, 100.0f, (secs - 0.5f) / (5.0f - 0.5f) * 100.0f);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            readoutArea = r.removeFromBottom (42);
            labelArea = readoutArea.removeFromBottom (16);
            slider.setBounds (r.reduced (8));
            rebuildOrbit();
        }

        void paint (juce::Graphics& g) override
        {
            using namespace Palette;
            // orbital square particles around the ring — denser/faster with longer reverse
            const float pct = getPercent() * 0.01f;
            const auto knob = slider.getBounds().toFloat();
            const auto c = knob.getCentre();
            const float radius = juce::jmin (knob.getWidth(), knob.getHeight()) * 0.42f;

            for (auto& p : orbit)
            {
                const float ang = p.base + phase * p.speed * (0.35f + pct);
                const float rr = radius + p.radial + pct * 6.0f;
                const float x = c.x + std::cos (ang) * rr;
                const float y = c.y + std::sin (ang) * rr;
                const float a = (0.25f + 0.55f * pct) * p.alpha;
                g.setColour ((p.pink ? pinkLight : violetLight).withAlpha (a));
                const float sz = p.size * (0.7f + pct * 0.8f);
                g.fillRect (x - sz * 0.5f, y - sz * 0.5f, sz, sz);
            }

            const float secs = getSeconds();
            const auto line = juce::String (juce::roundToInt (getPercent())) + "%  -  "
                            + juce::String (secs, 1) + " s";

            g.setColour (text);
            g.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));
            g.drawText (line, readoutArea.withTrimmedBottom (2), juce::Justification::centred, false);

            g.setColour (blueLight);
            g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
            g.drawText ("INVERSE", labelArea, juce::Justification::centred, false);
        }

        void paintOverChildren (juce::Graphics&) override {}

    private:
        struct Speck { float base, speed, radial, size, alpha; bool pink; };

        void rebuildOrbit()
        {
            orbit.clear();
            juce::Random rng (0x51C8u);
            for (int i = 0; i < 36; ++i)
            {
                Speck s;
                s.base = rng.nextFloat() * juce::MathConstants<float>::twoPi;
                s.speed = 0.4f + rng.nextFloat() * 1.2f;
                s.radial = 4.0f + rng.nextFloat() * 18.0f;
                s.size = 1.5f + rng.nextFloat() * 3.5f;
                s.alpha = 0.35f + rng.nextFloat() * 0.65f;
                s.pink = rng.nextBool();
                orbit.push_back (s);
            }
        }

        void timerCallback() override
        {
            phase += 0.045;
            if (phase > 1000.0) phase -= 1000.0;
            repaint();
        }

        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        juce::Rectangle<int> readoutArea, labelArea;
        std::vector<Speck> orbit;
        double phase = 0.0;
    };

    class PillButton : public juce::Component
    {
    public:
        enum Style { Outline, Filled, SoftPink };

        std::function<void()> onClick;

        PillButton (juce::String textIn, Style styleIn = Outline)
            : label (std::move (textIn)), style (styleIn) {}

        void setSelected (bool s) { if (selected != s) { selected = s; repaint(); } }
        bool isSelected() const { return selected; }
        void setLabel (juce::String t) { label = std::move (t); repaint(); }
        void setStyle (Style s) { style = s; repaint(); }

        void paint (juce::Graphics& g) override
        {
            using namespace Palette;
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            const float rad = juce::jmin (12.0f, r.getHeight() * 0.45f);
            const bool en = isEnabled();
            const bool on = selected || style == Filled;

            if (on && en)
            {
                const bool usePink = (style == SoftPink);
                g.setColour ((usePink ? pinkLight : blue).withAlpha (0.40f));
                g.fillRoundedRectangle (r.expanded (2.0f), rad + 2.0f);
                g.setGradientFill (juce::ColourGradient (usePink ? pinkLight : blueLight, r.getX(), r.getY(),
                                                         usePink ? Palette::pink : blue, r.getRight(), r.getBottom(), false));
                g.fillRoundedRectangle (r, rad);
            }
            else
            {
                g.setColour (bgLow.withAlpha (en ? 1.0f : 0.5f));
                g.fillRoundedRectangle (r, rad);
                const auto edge = (style == SoftPink ? pink : violet).withAlpha (en ? (over ? 0.95f : 0.7f) : 0.3f);
                g.setColour (edge);
                g.drawRoundedRectangle (r, rad, 1.4f);
            }

            g.setColour (on && en ? juce::Colour (0xff0a0614) : (en ? text : dim.withAlpha (0.5f)));
            g.setFont (juce::Font (juce::FontOptions (juce::jmin (13.0f, r.getHeight() * 0.42f), juce::Font::bold)));
            g.drawText (label, getLocalBounds(), juce::Justification::centred, false);
        }

        void mouseEnter (const juce::MouseEvent&) override { over = true;  repaint(); }
        void mouseExit  (const juce::MouseEvent&) override { over = false; repaint(); }
        void mouseDown  (const juce::MouseEvent&) override { if (isEnabled() && onClick) onClick(); }

    private:
        juce::String label;
        Style style;
        bool selected = false, over = false;
    };

    class SegmentedControl : public juce::Component
    {
    public:
        SegmentedControl (juce::RangedAudioParameter& p, juce::StringArray labelsIn, bool invertOrder = false)
            : labels (std::move (labelsIn)), invert (invertOrder),
              attachment (p, [this] (float v) { setFromValue (v); })
        {
            attachment.sendInitialUpdate();
        }

        void paint (juce::Graphics& g) override
        {
            using namespace Palette;
            const auto r = getLocalBounds().toFloat();
            g.setColour (bgLow);
            g.fillRoundedRectangle (r, 10.0f);
            g.setColour (panelEdge.withAlpha (0.9f));
            g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.2f);

            const int n = labels.size();
            const float w = r.getWidth() / (float) n;
            for (int i = 0; i < n; ++i)
            {
                auto seg = juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight()).reduced (3.0f);
                const bool sel = (i == selected);
                if (sel)
                {
                    g.setColour (blue.withAlpha (0.40f));
                    g.fillRoundedRectangle (seg.expanded (1.5f), 8.0f);
                    g.setGradientFill (juce::ColourGradient (blueLight, seg.getX(), seg.getY(),
                                                             blue, seg.getRight(), seg.getBottom(), false));
                    g.fillRoundedRectangle (seg, 7.0f);
                }
                else if (i == hovered)
                {
                    g.setColour (panelEdge);
                    g.fillRoundedRectangle (seg, 7.0f);
                }
                g.setColour (sel ? juce::Colour (0xff0a0614) : (i == hovered ? text : dim));
                g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
                g.drawText (labels[i], seg, juce::Justification::centred, false);
            }
        }

        void mouseMove (const juce::MouseEvent& e) override { setHover (indexAt (e.x)); }
        void mouseExit (const juce::MouseEvent&) override   { setHover (-1); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            const int idx = indexAt (e.x);
            const int n = labels.size();
            attachment.setValueAsCompleteGesture ((float) (invert ? (n - 1 - idx) : idx));
        }

    private:
        int indexAt (int x) const
        {
            return juce::jlimit (0, labels.size() - 1, (int) ((float) x / (float) getWidth() * (float) labels.size()));
        }
        void setHover (int i) { if (i != hovered) { hovered = i; repaint(); } }
        void setFromValue (float v)
        {
            const int n = labels.size();
            const int val = juce::jlimit (0, n - 1, juce::roundToInt (v));
            selected = invert ? (n - 1 - val) : val;
            repaint();
        }

        juce::StringArray labels;
        bool invert;
        int selected = 0, hovered = -1;
        juce::ParameterAttachment attachment;
    };

    /** Force FX mode (true) — matches real ANTICIP8: any character/tremolo/grid click leaves RAW. */
    inline void forceFxMode (juce::RangedAudioParameter& fxParam)
    {
        if (fxParam.getValue() < 0.5f)
            fxParam.setValueNotifyingHost (1.0f);
    }

    class CharacterRow : public juce::Component
    {
    public:
        CharacterRow (juce::RangedAudioParameter& characterParam,
                      juce::RangedAudioParameter& tremoloParam,
                      juce::RangedAudioParameter& fxParam)
            : fx (fxParam),
              charAtt (characterParam, [this] (float v) { setChar (v); }),
              tremAtt (tremoloParam, [this] (float v) { setTrem (v); })
        {
            static const char* names[] = { "CLASSIQUE", "TÉLÉPHONE", "CHORUS" };
            for (int i = 0; i < 3; ++i)
            {
                buttons[i] = std::make_unique<PillButton> (names[i], PillButton::Outline);
                const int idx = i;
                buttons[i]->onClick = [this, idx]
                {
                    forceFxMode (fx); // RAW → FX when picking a character
                    charAtt.setValueAsCompleteGesture ((float) idx);
                };
                addAndMakeVisible (*buttons[i]);
            }
            tremoloBtn = std::make_unique<PillButton> ("TREMOLO", PillButton::SoftPink);
            tremoloBtn->onClick = [this]
            {
                forceFxMode (fx); // RAW → FX when toggling tremolo
                const bool on = tremoloBtn->isSelected();
                tremAtt.setValueAsCompleteGesture (on ? 0.0f : 1.0f);
            };
            addAndMakeVisible (*tremoloBtn);
            charAtt.sendInitialUpdate();
            tremAtt.sendInitialUpdate();
        }

        void resized() override
        {
            auto r = getLocalBounds();
            const int gap = 8;
            const int w = (r.getWidth() - gap * 3) / 4;
            for (int i = 0; i < 3; ++i)
            {
                buttons[i]->setBounds (r.removeFromLeft (w));
                r.removeFromLeft (gap);
            }
            tremoloBtn->setBounds (r.removeFromLeft (w));
        }

    private:
        void setChar (float v)
        {
            const int sel = juce::jlimit (0, 2, juce::roundToInt (v));
            for (int i = 0; i < 3; ++i)
                buttons[i]->setSelected (i == sel);
        }
        void setTrem (float v) { tremoloBtn->setSelected (v >= 0.5f); }

        juce::RangedAudioParameter& fx;
        std::unique_ptr<PillButton> buttons[3];
        std::unique_ptr<PillButton> tremoloBtn;
        juce::ParameterAttachment charAtt, tremAtt;
    };

    class DivisionRow : public juce::Component
    {
    public:
        DivisionRow (juce::RangedAudioParameter& divisionParam,
                     juce::RangedAudioParameter& fxParam,
                     juce::RangedAudioParameter& tremoloParam)
            : fx (fxParam), tremolo (tremoloParam),
              attachment (divisionParam, [this] (float v) { setFromValue (v); })
        {
            static const char* names[] = { "1/8", "1/16", "1/32", "1/64" };
            for (int i = 0; i < 4; ++i)
            {
                buttons[i] = std::make_unique<PillButton> (names[i], i == 0 ? PillButton::Outline : PillButton::SoftPink);
                const int idx = i;
                buttons[i]->onClick = [this, idx]
                {
                    forceFxMode (fx);                 // RAW → FX
                    if (tremolo.getValue() < 0.5f)    // grid implies tremolo on
                        tremolo.setValueNotifyingHost (1.0f);
                    attachment.setValueAsCompleteGesture ((float) idx);
                };
                addAndMakeVisible (*buttons[i]);
            }
            attachment.sendInitialUpdate();
        }

        void resized() override
        {
            auto r = getLocalBounds();
            const int gap = 6;
            const int w = (r.getWidth() - gap * 3) / 4;
            for (int i = 0; i < 4; ++i)
            {
                buttons[i]->setBounds (r.removeFromLeft (w));
                if (i < 3) r.removeFromLeft (gap);
            }
        }

    private:
        void setFromValue (float v)
        {
            const int sel = juce::jlimit (0, 3, juce::roundToInt (v));
            for (int i = 0; i < 4; ++i)
                buttons[i]->setSelected (i == sel);
        }

        juce::RangedAudioParameter& fx;
        juce::RangedAudioParameter& tremolo;
        std::unique_ptr<PillButton> buttons[4];
        juce::ParameterAttachment attachment;
    };

    class IconButton : public juce::Component
    {
    public:
        enum Kind { Help, Settings };
        std::function<void()> onClick;
        explicit IconButton (Kind k) : kind (k) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

        void paint (juce::Graphics& g) override
        {
            using namespace Palette;
            auto r = getLocalBounds().toFloat().reduced (1.0f);
            g.setColour (over ? violet.withAlpha (0.25f) : juce::Colours::transparentBlack);
            g.fillEllipse (r);
            g.setColour (over ? violetLight : dim);
            g.drawEllipse (r, 1.2f);

            if (kind == Help)
            {
                g.setFont (juce::Font (juce::FontOptions (r.getHeight() * 0.55f, juce::Font::bold)));
                g.drawText ("?", getLocalBounds(), juce::Justification::centred, false);
            }
            else
            {
                const float cx = r.getCentreX(), cy = r.getCentreY();
                const float h = r.getHeight() * 0.42f;
                for (int i = -1; i <= 1; ++i)
                {
                    const float x = cx + (float) i * 5.0f;
                    const float top = cy - h * (i == 0 ? 1.0f : (i < 0 ? 0.7f : 0.55f));
                    const float bot = cy + h * (i == 0 ? 0.55f : 0.85f);
                    g.drawLine (x, top, x, bot, 1.6f);
                    g.fillEllipse (x - 2.0f, (i == 0 ? top : bot) - 2.0f, 4.0f, 4.0f);
                }
            }
        }

        void mouseEnter (const juce::MouseEvent&) override { over = true;  repaint(); }
        void mouseExit  (const juce::MouseEvent&) override { over = false; repaint(); }
        void mouseDown  (const juce::MouseEvent&) override { if (onClick) onClick(); }

    private:
        Kind kind;
        bool over = false;
    };
}
