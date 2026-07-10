/*
    FLUXCORE·12 — LevelMeters.h
    Side-by-side INPUT / OUTPUT level meters plus a compact readout of the
    saturation-induced level movement (output-vs-input in dB) and the engine's
    auto-trim (loudness-match) gain.

    Same pattern as FluxMeter: a juce::Component owning a juce::Timer that pulls
    the processor's thread-safe published floats on a 30 Hz tick and repaints.
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include "Theme.h"
#include "../plugin/PluginProcessor.h"

namespace fluxcore
{
class LevelMeters : public juce::Component, private juce::Timer
{
public:
    explicit LevelMeters (FluxcoreProcessor& p) : proc (p) { startTimerHz (30); }
    ~LevelMeters() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        const auto& pal = theme::current();
        auto b = getLocalBounds().toFloat().reduced (2.0f);

        g.setColour (pal.panel);
        g.fillRoundedRectangle (b, theme::kCorner);
        g.setColour (pal.panelEdge);
        g.drawRoundedRectangle (b, theme::kCorner, 1.0f);

        auto area = b.reduced (10.0f);

        // reserve room at the bottom for the movement / trim readout and labels
        auto readout  = area.removeFromBottom (34.0f);
        auto labelRow = area.removeFromBottom (14.0f);

        // vertical extent shared by the meter bars and the side dB ticks
        const float mBot = area.getBottom();
        const float mH   = area.getHeight();

        auto dbToFrac = [] (float db)
        {
            return juce::jlimit (0.0f, 1.0f, juce::jmap (db, -60.0f, 6.0f, 0.0f, 1.0f));
        };
        const float zeroFrac = dbToFrac (0.0f);

        // dB tick gutter on the left
        auto gutter = area.removeFromLeft (24.0f);
        g.setFont (10.0f);
        for (float db : { 0.0f, -12.0f, -24.0f, -48.0f })
        {
            const float y = mBot - dbToFrac (db) * mH;
            g.setColour (pal.grid);
            g.drawLine (gutter.getRight(), y, area.getRight(), y, 1.0f);
            g.setColour (pal.textDim);
            g.drawText (juce::String ((int) db),
                        juce::Rectangle<float> (gutter.getX(), y - 6.0f, gutter.getWidth() - 3.0f, 12.0f).toNearestInt(),
                        juce::Justification::centredRight);
        }

        // two equal columns
        auto inCol  = area.removeFromLeft (area.getWidth() * 0.5f);
        auto outCol = area;

        auto drawBar = [&] (juce::Rectangle<float> col, float lin, float peakLin)
        {
            auto bar = col.withSizeKeepingCentre (juce::jmin (col.getWidth() - 8.0f, 28.0f), mH);

            g.setColour (pal.bg);
            g.fillRoundedRectangle (bar, 2.0f);
            g.setColour (pal.panelEdge);
            g.drawRoundedRectangle (bar, 2.0f, 1.0f);

            const float db      = juce::Decibels::gainToDecibels (lin);
            const float frac    = dbToFrac (db);
            const float fillTop = bar.getBottom() - frac * bar.getHeight();

            // main fill up to 0 dB in pal.meter
            juce::Rectangle<float> fill (bar.getX(), fillTop, bar.getWidth(), bar.getBottom() - fillTop);
            g.setColour (pal.meter);
            g.fillRect (fill.reduced (2.0f, 0.0f));

            // segment above 0 dB (linear > 1.0) in pal.warn
            if (frac > zeroFrac)
            {
                const float zeroY = bar.getBottom() - zeroFrac * bar.getHeight();
                g.setColour (pal.warn);
                g.fillRect (juce::Rectangle<float> (bar.getX(), fillTop, bar.getWidth(), zeroY - fillTop)
                                .reduced (2.0f, 0.0f));
            }

            // slowly-decaying peak-hold line
            const float pFrac = dbToFrac (juce::Decibels::gainToDecibels (peakLin));
            const float pY    = bar.getBottom() - pFrac * bar.getHeight();
            g.setColour (pFrac > zeroFrac ? pal.warn : pal.text);
            g.drawLine (bar.getX() + 1.0f, pY, bar.getRight() - 1.0f, pY, 2.0f);
        };

        drawBar (inCol,  dispIn,  peakIn);
        drawBar (outCol, dispOut, peakOut);

        // "IN" / "OUT" labels under the bars
        g.setColour (pal.textDim);
        g.setFont (10.0f);
        g.drawText ("IN",  juce::Rectangle<float> (inCol.getX(),  labelRow.getY(), inCol.getWidth(),  labelRow.getHeight()).toNearestInt(),
                    juce::Justification::centred);
        g.drawText ("OUT", juce::Rectangle<float> (outCol.getX(), labelRow.getY(), outCol.getWidth(), labelRow.getHeight()).toNearestInt(),
                    juce::Justification::centred);

        // level-movement + auto-trim readout
        float moveDb = juce::Decibels::gainToDecibels (proc.outputLevel() / (proc.inputLevel() + 1e-6f));
        if (! std::isfinite (moveDb)) moveDb = 0.0f;
        moveDb = juce::jlimit (-24.0f, 24.0f, moveDb);

        float trimDb = juce::Decibels::gainToDecibels (proc.autoTrimGain());
        if (! std::isfinite (trimDb)) trimDb = 0.0f;
        trimDb = juce::jlimit (-24.0f, 24.0f, trimDb);

        auto sign = [] (float v) { return v >= 0.0f ? juce::String ("+") : juce::String(); };

        const juce::String deltaStr = juce::String::fromUTF8 ("\xce\x94") + " " + sign (moveDb) + juce::String (moveDb, 1) + " dB";
        const juce::String trimStr  = "TRIM " + sign (trimDb) + juce::String (trimDb, 1) + " dB";

        g.setColour (pal.text);
        g.setFont (13.0f);
        g.drawText (deltaStr, readout.removeFromTop (17.0f).toNearestInt(), juce::Justification::centred);
        g.setFont (11.0f);
        g.setColour (pal.textDim);
        g.drawText (trimStr, readout.toNearestInt(), juce::Justification::centred);
    }

    void resized() override {}

private:
    void timerCallback() override
    {
        const float inL  = proc.inputLevel();
        const float outL = proc.outputLevel();

        // fast attack / slow release ballistic on the displayed values
        auto ballistic = [] (float& disp, float target)
        {
            disp += (target > disp ? 0.5f : 0.08f) * (target - disp);
        };
        ballistic (dispIn,  inL);
        ballistic (dispOut, outL);

        // slowly-decaying peak hold
        peakIn  = inL  > peakIn  ? inL  : peakIn  * 0.985f;
        peakOut = outL > peakOut ? outL : peakOut * 0.985f;

        repaint();
    }

    FluxcoreProcessor& proc;
    float dispIn = 0.0f, dispOut = 0.0f;
    float peakIn = 0.0f, peakOut = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeters)
};

} // namespace fluxcore
