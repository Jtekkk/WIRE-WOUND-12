/*
    FLUXCORE·12 — Analysis.h

    JUCE-free analysis helpers that drive the UI visualisers. Each one builds a
    throwaway TransformerCore from the current control values and probes it, so
    the meters reflect exactly the current CORE / DRIVE / BIAS / impedance
    settings without ever touching (or racing) the live audio-thread engine.

        • computeHarmonics  -> the Harmonic Analyzer (H2..H9 + THD)
        • computeBHLoop     -> the Flux Meter (B–H loop trajectory)
        • responseCurveDb   -> the Response Curve (network magnitude vs freq)

    Being plain functions over plain arrays, they are trivially unit-tested and
    give GUI code a stable, self-contained data source.
*/

#pragma once

#include <array>
#include <vector>
#include "TransformerCore.h"

namespace fluxcore
{
//==============================================================================
/** Harmonic ratios H2..H9 (relative to the fundamental) and total THD, from a
    1 kHz probe tone through the current core. */
inline void computeHarmonics (const CoreControls& c, double fs,
                              std::array<float, kNumAnalyzerHarmonics>& barsRel,
                              float& thdPercent)
{
    TransformerCore core;
    core.prepare (fs, c.adaaOrder > 0 ? c.adaaOrder : 2);
    core.updateControls (c);

    const double f0  = 1000.0;
    const int    n   = 8192;
    const double amp = 0.35;
    const double w   = 2.0 * kPi * f0 / fs;

    // Goertzel accumulators for H1..H9
    constexpr int H = kNumAnalyzerHarmonics + 1; // H1..H9
    double s1[H] = {0}, s2[H] = {0}, cf[H];
    for (int k = 0; k < H; ++k) cf[k] = 2.0 * std::cos (w * (k + 1));

    for (int i = 0; i < n; ++i)
    {
        const double x = amp * std::sin (w * i);
        const double y = core.processSample (x, 0.0);
        for (int k = 0; k < H; ++k)
        {
            const double s0 = y + cf[k] * s1[k] - s2[k];
            s2[k] = s1[k]; s1[k] = s0;
        }
    }

    double mag[H];
    for (int k = 0; k < H; ++k)
    {
        const double wk = w * (k + 1);
        const double re = s1[k] - s2[k] * std::cos (wk);
        const double im = s2[k] * std::sin (wk);
        mag[k] = 2.0 * std::sqrt (re * re + im * im) / n;
    }

    const double h1 = std::max (mag[0], 1.0e-9);
    double restSq = 0.0;
    for (int k = 1; k < H; ++k)
    {
        barsRel[(size_t) (k - 1)] = (float) (mag[k] / h1);
        restSq += mag[k] * mag[k];
    }
    thdPercent = (float) (std::sqrt (restSq) / h1 * 100.0);
}

//==============================================================================
/** B–H loop trajectory (normalised to ±1) from a low-frequency probe. Returns
    one settled cycle in H (field) and M (magnetisation). */
inline void computeBHLoop (const CoreControls& c, double fs, int /*unused*/,
                           std::vector<float>& H, std::vector<float>& M)
{
    TransformerCore core;
    core.prepare (fs, 2);
    CoreControls cc = c;
    cc.hysteresis = std::max (0.35, c.hysteresis); // ensure a visible loop
    core.updateControls (cc);

    const double lf    = 55.0;
    const int    per   = (int) (fs / lf);
    const int    total = per * 6;
    const double w     = 2.0 * kPi * lf / fs;

    H.clear(); M.clear();
    H.reserve ((size_t) per); M.reserve ((size_t) per);

    double hMax = 1.0e-6, mMax = 1.0e-6;
    std::vector<double> hh, mm;
    hh.reserve ((size_t) per); mm.reserve ((size_t) per);
    for (int i = 0; i < total; ++i)
    {
        const double x = 0.85 * std::sin (w * i);
        core.processSample (x, 0.0);
        if (i >= total - per)
        {
            const double hv = core.meterField();
            const double mv = core.meterMagnetisation();
            hh.push_back (hv); mm.push_back (mv);
            hMax = std::max (hMax, std::abs (hv));
            mMax = std::max (mMax, std::abs (mv));
        }
    }
    for (size_t i = 0; i < hh.size(); ++i)
    {
        H.push_back ((float) (hh[i] / hMax));
        M.push_back ((float) (mm[i] / mMax));
    }
}

//==============================================================================
/** Linear-network response magnitude (dB) at a given frequency. */
inline float responseCurveDb (const CoreControls& c, double fs, double hz)
{
    TransformerCore core;
    core.prepare (fs, 2);
    core.updateControls (c);
    const double m = core.responseMagnitude (hz);
    return (float) (20.0 * std::log10 (std::max (m, 1.0e-4)));
}

} // namespace fluxcore
