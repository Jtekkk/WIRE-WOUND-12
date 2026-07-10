/*
    FLUXCORE·12 — FluxMeter.h
    Real-time B–H loop display. The loop trajectory is probed from the current
    core settings (Analysis::computeBHLoop); a live operating-point dot rides the
    curve using the processor's published field / magnetisation.

    Reference pattern for the other visualisers: a juce::Component that owns a
    juce::Timer, pulls JUCE-free analysis data on a timer tick, and repaints.
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "../plugin/PluginProcessor.h"
#include "../dsp/Analysis.h"

namespace fluxcore
{
class FluxMeter : public juce::Component, private juce::Timer
{
public:
    explicit FluxMeter (FluxcoreProcessor& p) : proc (p) { startTimerHz (30); }
    ~FluxMeter() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        const auto& pal = theme::current();
        auto b = getLocalBounds().toFloat().reduced (2.0f);

        g.setColour (pal.panel);
        g.fillRoundedRectangle (b, theme::kCorner);
        g.setColour (pal.panelEdge);
        g.drawRoundedRectangle (b, theme::kCorner, 1.0f);

        auto plot = b.reduced (10.0f);
        // axes
        g.setColour (pal.grid);
        g.drawLine (plot.getX(), plot.getCentreY(), plot.getRight(), plot.getCentreY(), 1.0f);
        g.drawLine (plot.getCentreX(), plot.getY(), plot.getCentreX(), plot.getBottom(), 1.0f);

        auto toXY = [&] (float H, float M)
        {
            return juce::Point<float> (plot.getCentreX() + H * plot.getWidth()  * 0.46f,
                                       plot.getCentreY() - M * plot.getHeight() * 0.46f);
        };

        // loop
        if (loopH.size() > 2)
        {
            juce::Path path;
            path.startNewSubPath (toXY (loopH[0], loopM[0]));
            for (size_t i = 1; i < loopH.size(); ++i)
                path.lineTo (toXY (loopH[i], loopM[i]));
            path.closeSubPath();
            g.setColour (pal.accent2.withAlpha (0.85f));
            g.strokePath (path, juce::PathStrokeType (2.0f));
            g.setColour (pal.accent2.withAlpha (0.12f));
            g.fillPath (path);
        }

        // live operating point
        const float lh = juce::jlimit (-1.0f, 1.0f, proc.liveField());
        const float lm = juce::jlimit (-1.0f, 1.0f, proc.liveMag());
        auto pt = toXY (lh, lm);
        g.setColour (pal.accent);
        g.fillEllipse (juce::Rectangle<float> (8, 8).withCentre (pt));

        g.setColour (pal.textDim);
        g.setFont (12.0f);
        g.drawText ("B-H  FLUX", plot.removeFromTop (14).toNearestInt(),
                    juce::Justification::topLeft);
        g.drawText ("H", juce::Rectangle<int> ((int) plot.getRight() - 14, (int) plot.getCentreY() - 16, 14, 14),
                    juce::Justification::centred);
    }

    void resized() override {}

private:
    void timerCallback() override
    {
        auto c = proc.uiControls();
        if (c.coreIndex != lastCore || std::abs (c.driveDb - lastDrive) > 0.25 ||
            std::abs (c.hysteresis - lastHyst) > 0.02 || std::abs (c.coreSat - lastSat) > 0.02 ||
            std::abs (c.bias - lastBias) > 0.02)
        {
            std::vector<float> H, M;
            computeBHLoop (c, proc.getCurrentSampleRate(), 0, H, M);
            loopH = std::move (H);
            loopM = std::move (M);
            lastCore = c.coreIndex; lastDrive = c.driveDb; lastHyst = c.hysteresis;
            lastSat = c.coreSat; lastBias = c.bias;
        }
        repaint();
    }

    FluxcoreProcessor& proc;
    std::vector<float> loopH, loopM;
    int lastCore = -1;
    double lastDrive = -999, lastHyst = -1, lastSat = -1, lastBias = -999;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FluxMeter)
};

} // namespace fluxcore
