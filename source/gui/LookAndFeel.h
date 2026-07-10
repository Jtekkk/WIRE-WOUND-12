/*
    FLUXCORE·12 — LookAndFeel.h
    Vector look-and-feel: chunky "iron" rotaries with a copper indicator, flat
    panels and combo boxes. Theme-driven (dark / light).
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

namespace fluxcore
{
class FluxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FluxLookAndFeel()
    {
        refreshColours();
    }

    void refreshColours()
    {
        const auto& p = theme::current();
        setColour (juce::ResizableWindow::backgroundColourId, p.bg);
        setColour (juce::Slider::textBoxTextColourId, p.text);
        setColour (juce::Slider::textBoxBackgroundColourId, p.panel);
        setColour (juce::Slider::textBoxOutlineColourId, p.panelEdge);
        setColour (juce::Label::textColourId, p.text);
        setColour (juce::ComboBox::backgroundColourId, p.panel);
        setColour (juce::ComboBox::textColourId, p.text);
        setColour (juce::ComboBox::outlineColourId, p.panelEdge);
        setColour (juce::ComboBox::arrowColourId, p.accent);
        setColour (juce::PopupMenu::backgroundColourId, p.panel);
        setColour (juce::PopupMenu::textColourId, p.text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, p.accent.withAlpha (0.35f));
        setColour (juce::TextButton::buttonColourId, p.panel);
        setColour (juce::TextButton::textColourOffId, p.textDim);
        setColour (juce::TextButton::textColourOnId, p.accent);
    }

    //== rotary ===============================================================
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float pos, float startAngle, float endAngle,
                           juce::Slider&) override
    {
        const auto& pal = theme::current();
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
        const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);

        // body
        g.setColour (pal.panelEdge);
        g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (c));
        g.setColour (pal.panel.brighter (0.05f));
        g.fillEllipse (juce::Rectangle<float> (r * 1.7f, r * 1.7f).withCentre (c));

        // arc track
        juce::Path track;
        track.addCentredArc (c.x, c.y, r * 0.95f, r * 0.95f, 0.0f, startAngle, endAngle, true);
        g.setColour (pal.grid.withAlpha (0.5f));
        g.strokePath (track, juce::PathStrokeType (2.0f));

        // arc value
        juce::Path val;
        val.addCentredArc (c.x, c.y, r * 0.95f, r * 0.95f, 0.0f, startAngle, angle, true);
        g.setColour (pal.accent);
        g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // indicator
        juce::Path ind;
        ind.startNewSubPath (c.x, c.y);
        ind.lineTo (c.x + std::cos (angle - juce::MathConstants<float>::halfPi) * r * 0.85f,
                    c.y + std::sin (angle - juce::MathConstants<float>::halfPi) * r * 0.85f);
        g.setColour (pal.text);
        g.strokePath (ind, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (pal.accent.withAlpha (0.9f));
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (c));
    }

    juce::Font getLabelFont (juce::Label& l) override
    {
        return l.getFont();
    }
};

} // namespace fluxcore
