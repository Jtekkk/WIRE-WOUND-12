/*
    FLUXCORE·12 — ResponseCurve.h
    Live frequency-response curve of the transformer's linear tone network. The
    magnitude is probed from the current control values (Analysis::responseCurveDb)
    and redraws whenever CORE / SOURCE Z / LOAD Z / WINDING RES change — proof that
    the tone comes from the impedance network, not a fixed preset EQ.

    Follows the FluxMeter reference pattern: a juce::Component that owns a
    juce::Timer, pulls JUCE-free analysis data on a timer tick, caches it, and
    repaints.
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "../plugin/PluginProcessor.h"
#include "../dsp/Analysis.h"
#include <vector>
#include <cmath>

namespace fluxcore
{
class ResponseCurve : public juce::Component, private juce::Timer
{
public:
    explicit ResponseCurve (FluxcoreProcessor& p) : proc (p) { startTimerHz (30); }
    ~ResponseCurve() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        const auto& pal = theme::current();
        auto b = getLocalBounds().toFloat().reduced (2.0f);

        g.setColour (pal.panel);
        g.fillRoundedRectangle (b, theme::kCorner);
        g.setColour (pal.panelEdge);
        g.drawRoundedRectangle (b, theme::kCorner, 1.0f);

        auto plot = b.reduced (10.0f);

        const double fmax = curFmax > 20.0 ? curFmax : 22000.0;

        auto xFor = [&] (double hz)
        {
            const double t = std::log10 (hz / 20.0) / std::log10 (fmax / 20.0);
            return plot.getX() + (float) t * plot.getWidth();
        };
        auto yFor = [&] (double db)
        {
            const double t = (db - kDbMin) / (kDbMax - kDbMin);
            float y = plot.getY() + (float) (1.0 - t) * plot.getHeight();
            return juce::jlimit (plot.getY(), plot.getBottom(), y);
        };

        // horizontal gridlines + dB labels (-12, 0, +12)
        g.setFont (11.0f);
        const double hLines[] = { -12.0, 0.0, 12.0 };
        for (double db : hLines)
        {
            const float y = yFor (db);
            g.setColour (db == 0.0 ? pal.grid.withMultipliedAlpha (2.0f) : pal.grid);
            g.drawLine (plot.getX(), y, plot.getRight(), y, db == 0.0 ? 1.4f : 1.0f);
            g.setColour (pal.textDim);
            g.drawText (juce::String ((int) db) + " dB",
                        juce::Rectangle<int> ((int) plot.getX() + 2, (int) y - 12, 44, 12),
                        juce::Justification::topLeft);
        }

        // vertical gridlines + frequency labels (100, 1k, 10k)
        struct FL { double hz; const char* label; };
        const FL vLines[] = { { 100.0, "100" }, { 1000.0, "1k" }, { 10000.0, "10k" } };
        for (const auto& fl : vLines)
        {
            if (fl.hz > fmax) continue;
            const float x = xFor (fl.hz);
            g.setColour (pal.grid);
            g.drawLine (x, plot.getY(), x, plot.getBottom(), 1.0f);
            g.setColour (pal.textDim);
            g.drawText (fl.label,
                        juce::Rectangle<int> ((int) x - 20, (int) plot.getBottom() - 12, 40, 12),
                        juce::Justification::centred);
        }

        // response curve + fill under to the 0 dB line
        if (pts.size() > 1)
        {
            const float y0 = yFor (0.0);

            juce::Path curve;
            curve.startNewSubPath (xFor (pts[0].hz), yFor (pts[0].db));
            for (size_t i = 1; i < pts.size(); ++i)
                curve.lineTo (xFor (pts[i].hz), yFor (pts[i].db));

            juce::Path fill = curve;
            fill.lineTo (xFor (pts.back().hz), y0);
            fill.lineTo (xFor (pts.front().hz), y0);
            fill.closeSubPath();
            g.setColour (pal.curve.withAlpha (0.10f));
            g.fillPath (fill);

            g.setColour (pal.curve);
            g.strokePath (curve, juce::PathStrokeType (2.0f));
        }

        g.setColour (pal.textDim);
        g.setFont (12.0f);
        g.drawText ("RESPONSE", plot.removeFromTop (14).toNearestInt(),
                    juce::Justification::topLeft);
    }

    void resized() override {}

private:
    struct Point { double hz; float db; };

    static constexpr double kDbMin = -24.0;
    static constexpr double kDbMax =  18.0;

    void timerCallback() override
    {
        auto c = proc.uiControls();
        const double fs = proc.getCurrentSampleRate();

        if (c.coreIndex != lastCore ||
            std::abs (c.sourceZ    - lastSourceZ) > 0.5   ||
            std::abs (c.loadZ      - lastLoadZ)   > 0.5   ||
            std::abs (c.windingRes - lastWindRes) > 0.001 ||
            std::abs (fs - lastFs) > 1.0)
        {
            const double fmax = std::min (22000.0, 0.49 * fs);
            constexpr int kNumPoints = 200;

            std::vector<Point> next;
            next.reserve ((size_t) kNumPoints);
            const double ratio = std::log10 (fmax / 20.0);
            for (int i = 0; i < kNumPoints; ++i)
            {
                const double t  = (double) i / (double) (kNumPoints - 1);
                const double hz = 20.0 * std::pow (10.0, t * ratio);
                next.push_back ({ hz, responseCurveDb (c, fs, hz) });
            }

            pts       = std::move (next);
            curFmax   = fmax;
            lastCore  = c.coreIndex;
            lastSourceZ = c.sourceZ; lastLoadZ = c.loadZ; lastWindRes = c.windingRes;
            lastFs    = fs;
        }
        repaint();
    }

    FluxcoreProcessor& proc;
    std::vector<Point> pts;
    double curFmax = 22000.0;

    int    lastCore    = -1;
    double lastSourceZ = -1.0, lastLoadZ = -1.0, lastWindRes = -1.0;
    double lastFs      = -1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResponseCurve)
};

} // namespace fluxcore
