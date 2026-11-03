#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace anticip8
{
    namespace Palette
    {
        inline const juce::Colour bg          { 0xff07060c };
        inline const juce::Colour bgLow       { 0xff0c0a14 };
        inline const juce::Colour panel       { 0xff100e1a };
        inline const juce::Colour panelEdge   { 0xff3a2a6a };
        inline const juce::Colour violet      { 0xffa855f7 };
        inline const juce::Colour violetLight { 0xffc084fc };
        inline const juce::Colour violetDeep  { 0xff6d28d9 };
        inline const juce::Colour blue        { 0xff6c72ff };   // active FX / CLASSIC (real VST)
        inline const juce::Colour blueLight   { 0xff8b90ff };
        inline const juce::Colour pink        { 0xffec4899 };
        inline const juce::Colour pinkLight   { 0xfff472b6 };
        inline const juce::Colour text        { 0xfff5f3ff };
        inline const juce::Colour dim         { 0xff8b7fb0 };
        inline const juce::Colour muted       { 0xff5c5278 };
        inline const juce::Colour error       { 0xffff6b81 };
    }

    class CustomLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        CustomLookAndFeel()
        {
            using namespace Palette;
            setColour (juce::ResizableWindow::backgroundColourId, bg);
            setColour (juce::Label::textColourId, dim);
            setColour (juce::Slider::textBoxTextColourId, text);
            setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            setColour (juce::Slider::textBoxHighlightColourId, violetDeep);
            setColour (juce::TextButton::textColourOffId, text);
            setColour (juce::TextButton::textColourOnId, text);
        }

        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider& s) override
        {
            using namespace Palette;
            const bool en = s.isEnabled();
            auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
            const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
            const auto c = bounds.getCentre();
            const float lw = juce::jmax (6.0f, radius * 0.16f);
            const float arcR = radius - lw * 0.5f;
            const float ang = startAngle + pos * (endAngle - startAngle);

            // soft outer glow
            if (en)
            {
                juce::Path glow;
                glow.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, ang, true);
                g.setColour (violet.withAlpha (0.22f));
                g.strokePath (glow, juce::PathStrokeType (lw + 10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (pink.withAlpha (0.10f));
                g.strokePath (glow, juce::PathStrokeType (lw + 18.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            juce::Path track;
            track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
            g.setColour (juce::Colour (0xff1a1430));
            g.strokePath (track, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            juce::Path val;
            val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, ang, true);
            g.setGradientFill (juce::ColourGradient (violetLight, c.x - arcR, c.y,
                                                     pink, c.x + arcR, c.y, false));
            g.setColour (en ? violet : dim.withAlpha (0.35f));
            if (en)
                g.setGradientFill (juce::ColourGradient (violetLight, c.x, c.y - arcR,
                                                         pink, c.x + arcR * 0.6f, c.y + arcR * 0.4f, false));
            g.strokePath (val, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            const float bodyR = arcR - lw - 4.0f;
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2245), c.x, c.y - bodyR,
                                                     juce::Colour (0xff100e1c), c.x, c.y + bodyR, false));
            g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
            g.setColour (juce::Colour (0xff3d2f66).withAlpha (en ? 1.0f : 0.4f));
            g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.2f);

            // specular highlight
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.fillEllipse (c.x - bodyR * 0.55f, c.y - bodyR * 0.72f, bodyR * 1.1f, bodyR * 0.55f);

            const float tipR = juce::jmax (3.0f, bodyR * 0.08f);
            const juce::Point<float> tip (c.x + std::sin (ang) * (bodyR - tipR - 2.0f),
                                          c.y - std::cos (ang) * (bodyR - tipR - 2.0f));
            g.setColour (en ? pinkLight : dim.withAlpha (0.4f));
            g.fillEllipse (tip.x - tipR, tip.y - tipR, tipR * 2.0f, tipR * 2.0f);
        }

        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bgc,
                                   bool over, bool down) override
        {
            using namespace Palette;
            auto r = b.getLocalBounds().toFloat().reduced (0.5f);
            auto col = bgc;
            if (! b.isEnabled()) col = col.withMultipliedAlpha (0.4f);
            else if (down)       col = col.brighter (0.15f);
            else if (over)       col = col.brighter (0.08f);
            g.setColour (col);
            g.fillRoundedRectangle (r, 10.0f);
            g.setColour (b.isEnabled() ? violet.withAlpha (0.7f) : panelEdge);
            g.drawRoundedRectangle (r, 10.0f, 1.2f);
        }

        juce::Font getTextButtonFont (juce::TextButton&, int) override
        {
            return juce::Font (juce::FontOptions (12.0f, juce::Font::bold));
        }
    };
}
