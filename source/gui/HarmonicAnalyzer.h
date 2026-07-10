/*
    FLUXCORE·12 — HarmonicAnalyzer.h
    Live harmonic bar graph (H2..H9) with a total-THD readout. Harmonic ratios
    are probed from the current core settings (Analysis::computeHarmonics) so the
    user can SEE the odd/even balance the BIAS knob produces.

    Follows the FluxMeter reference pattern: a juce::Component that owns a
    juce::Timer, pulls JUCE-free analysis data on a timer tick, caches it against
    the controls that produced it, and repaints.
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include "../plugin/PluginProcessor.h"
#include "../dsp/Analysis.h"

namespace fluxcore
{
class HarmonicAnalyzer : public juce::Component, private juce::Timer
{
public:
    explicit HarmonicAnalyzer (FluxcoreProcessor& p) : proc (p) { startTimerHz (30); }
    ~HarmonicAnalyzer() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        const auto& pal = theme::current();
        auto b = getLocalBounds().toFloat().reduced (2.0f);

        g.setColour (pal.panel);
        g.fillRoundedRectangle (b, theme::kCorner);
        g.setColour (pal.panelEdge);
        g.drawRoundedRectangle (b, theme::kCorner, 1.0f);

        auto inner = b.reduced (10.0f);

        // header row: title (left) + THD readout (right)
        auto header = inner.removeFromTop (16.0f);
        g.setColour (pal.textDim);
        g.setFont (juce::Font (12.0f));
        g.drawText ("HARMONICS", header.toNearestInt(), juce::Justification::topLeft);
        g.setColour (pal.text);
        g.drawText (juce::String ("THD ") + juce::String (thdPercent, 2) + "%",
                    header.toNearestInt(), juce::Justification::topRight);

        // bottom row: H2..H9 labels
        auto labels = inner.removeFromBottom (14.0f);
        inner.removeFromTop (4.0f);       // gap under header
        inner.removeFromBottom (2.0f);    // gap above labels

        auto plot = inner;

        // dBc gridlines with faint labels. Height fraction maps -60..0 dBc -> 0..1.
        const int gridDb[] = { -12, -24, -36, -48 };
        for (int d : gridDb)
        {
            const float frac = ((float) d + 60.0f) / 60.0f;
            const float y = plot.getBottom() - frac * plot.getHeight();
            g.setColour (pal.grid);
            g.drawLine (plot.getX(), y, plot.getRight(), y, 1.0f);
            g.setColour (pal.textDim);
            g.setFont (juce::Font (9.0f));
            g.drawText (juce::String (d),
                        juce::Rectangle<int> ((int) plot.getX() + 2, (int) y - 10, 22, 10),
                        juce::Justification::topLeft);
        }

        // bars H2..H9
        const int   n     = (int) kNumAnalyzerHarmonics; // 8
        const float slotW = plot.getWidth() / (float) n;
        const float barW  = slotW * 0.6f;

        for (int i = 0; i < n; ++i)
        {
            const float cx   = plot.getX() + slotW * ((float) i + 0.5f);
            const float frac = juce::jlimit (0.0f, 1.0f,
                                   (juce::Decibels::gainToDecibels (bars[(size_t) i] + 1.0e-6f) + 60.0f) / 60.0f);
            const float h    = frac * plot.getHeight();

            juce::Rectangle<float> bar (cx - barW * 0.5f, plot.getBottom() - h, barW, h);

            // EVEN harmonics (H2,H4,H6,H8 -> indices 0,2,4,6) -> accent2
            // ODD  harmonics (H3,H5,H7,H9 -> indices 1,3,5,7) -> accent
            const bool isEven = (i % 2) == 0;
            g.setColour (isEven ? pal.accent2 : pal.accent);
            g.fillRoundedRectangle (bar, 2.0f);

            // per-bar label ("H2".."H9") under the plot
            g.setColour (pal.textDim);
            g.setFont (juce::Font (10.0f));
            g.drawText ("H" + juce::String (i + 2),
                        juce::Rectangle<float> (cx - slotW * 0.5f, labels.getY(), slotW, labels.getHeight()).toNearestInt(),
                        juce::Justification::centred);
        }
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
            computeHarmonics (c, proc.getCurrentSampleRate(), bars, thdPercent);
            lastCore = c.coreIndex; lastDrive = c.driveDb; lastHyst = c.hysteresis;
            lastSat = c.coreSat; lastBias = c.bias;
        }
        repaint();
    }

    FluxcoreProcessor& proc;
    std::array<float, kNumAnalyzerHarmonics> bars { {} };
    float thdPercent = 0.0f;
    int lastCore = -1;
    double lastDrive = -999, lastHyst = -1, lastSat = -1, lastBias = -999;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonicAnalyzer)
};

} // namespace fluxcore
