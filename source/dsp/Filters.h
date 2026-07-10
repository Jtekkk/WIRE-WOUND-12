/*
    FLUXCORE·12 — Filters.h

    Filter primitives used throughout the engine: a transposed-direct-form-II
    biquad, a one-pole low/high shelf pair and a DC blocker. All are RBJ-cookbook
    based, bilinear-transformed, and denormal-protected. Dependency-free.
*/

#pragma once

#include "DspUtils.h"

namespace fluxcore
{
//==============================================================================
/** Direct-form-I biquad with settable coefficients (b0,b1,b2,a1,a2; a0==1). */
struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0; // transposed-df2 state

    void reset() noexcept { z1 = z2 = 0.0; }

    inline double process (double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        z1 = flush (z1);
        z2 = flush (z2);
        return y;
    }

    void setCoeffs (double nb0, double nb1, double nb2, double na1, double na2) noexcept
    {
        b0 = nb0; b1 = nb1; b2 = nb2; a1 = na1; a2 = na2;
    }

    //== RBJ cookbook designers (fs in Hz) ====================================
    void setLowpass (double fs, double f, double q) noexcept
    {
        f = clampT (f, 10.0, fs * 0.49);
        const double w = kTwoPi * f / fs, cs = std::cos (w), sn = std::sin (w);
        const double al = sn / (2.0 * std::max (q, 0.01));
        const double a0 = 1.0 + al;
        setCoeffs ((1.0 - cs) * 0.5 / a0, (1.0 - cs) / a0, (1.0 - cs) * 0.5 / a0,
                   (-2.0 * cs) / a0, (1.0 - al) / a0);
    }

    void setHighpass (double fs, double f, double q) noexcept
    {
        f = clampT (f, 5.0, fs * 0.49);
        const double w = kTwoPi * f / fs, cs = std::cos (w), sn = std::sin (w);
        const double al = sn / (2.0 * std::max (q, 0.01));
        const double a0 = 1.0 + al;
        setCoeffs ((1.0 + cs) * 0.5 / a0, -(1.0 + cs) / a0, (1.0 + cs) * 0.5 / a0,
                   (-2.0 * cs) / a0, (1.0 - al) / a0);
    }

    void setPeak (double fs, double f, double q, double gainDb) noexcept
    {
        f = clampT (f, 10.0, fs * 0.49);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = kTwoPi * f / fs, cs = std::cos (w), sn = std::sin (w);
        const double al = sn / (2.0 * std::max (q, 0.01));
        const double a0 = 1.0 + al / A;
        setCoeffs ((1.0 + al * A) / a0, (-2.0 * cs) / a0, (1.0 - al * A) / a0,
                   (-2.0 * cs) / a0, (1.0 - al / A) / a0);
    }

    void setHighShelf (double fs, double f, double gainDb, double slope = 1.0) noexcept
    {
        f = clampT (f, 10.0, fs * 0.49);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = kTwoPi * f / fs, cs = std::cos (w), sn = std::sin (w);
        const double al = sn / 2.0 * std::sqrt ((A + 1.0 / A) * (1.0 / slope - 1.0) + 2.0);
        const double tsa = 2.0 * std::sqrt (A) * al;
        const double a0 = (A + 1.0) - (A - 1.0) * cs + tsa;
        setCoeffs (A * ((A + 1.0) + (A - 1.0) * cs + tsa) / a0,
                   -2.0 * A * ((A - 1.0) + (A + 1.0) * cs) / a0,
                   A * ((A + 1.0) + (A - 1.0) * cs - tsa) / a0,
                   2.0 * ((A - 1.0) - (A + 1.0) * cs) / a0,
                   ((A + 1.0) - (A - 1.0) * cs - tsa) / a0);
    }

    void setLowShelf (double fs, double f, double gainDb, double slope = 1.0) noexcept
    {
        f = clampT (f, 10.0, fs * 0.49);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = kTwoPi * f / fs, cs = std::cos (w), sn = std::sin (w);
        const double al = sn / 2.0 * std::sqrt ((A + 1.0 / A) * (1.0 / slope - 1.0) + 2.0);
        const double tsa = 2.0 * std::sqrt (A) * al;
        const double a0 = (A + 1.0) + (A - 1.0) * cs + tsa;
        setCoeffs (A * ((A + 1.0) - (A - 1.0) * cs + tsa) / a0,
                   2.0 * A * ((A - 1.0) - (A + 1.0) * cs) / a0,
                   A * ((A + 1.0) - (A - 1.0) * cs - tsa) / a0,
                   -2.0 * ((A - 1.0) + (A + 1.0) * cs) / a0,
                   ((A + 1.0) + (A - 1.0) * cs - tsa) / a0);
    }

    /** Magnitude response (linear) at frequency f — used by the response-curve UI. */
    double magnitudeAt (double fs, double f) const noexcept
    {
        const double w = kTwoPi * f / fs;
        const double cw = std::cos (w),  sw = std::sin (w);
        const double c2 = std::cos (2 * w), s2 = std::sin (2 * w);
        const double numRe = b0 + b1 * cw + b2 * c2;
        const double numIm = -(b1 * sw + b2 * s2);
        const double denRe = 1.0 + a1 * cw + a2 * c2;
        const double denIm = -(a1 * sw + a2 * s2);
        const double num = std::sqrt (numRe * numRe + numIm * numIm);
        const double den = std::sqrt (denRe * denRe + denIm * denIm);
        return den > 1.0e-12 ? num / den : 0.0;
    }
};

//==============================================================================
/** First-order one-pole low/high pass. */
struct OnePole
{
    double a = 0.0, b = 1.0, z = 0.0;
    bool   highpass = false;

    void reset() noexcept { z = 0.0; }

    void setLowpass (double fs, double f) noexcept
    {
        f = clampT (f, 1.0, fs * 0.49);
        b = std::exp (-kTwoPi * f / fs);
        a = 1.0 - b;
        highpass = false;
    }
    void setHighpass (double fs, double f) noexcept
    {
        setLowpass (fs, f);
        highpass = true;
    }
    inline double process (double x) noexcept
    {
        z = flush (a * x + b * z);
        return highpass ? x - z : z;
    }
    inline double low()  const noexcept { return z; }
};

//==============================================================================
/** DC blocker (leaky differentiator). */
struct DCBlocker
{
    double x1 = 0.0, y1 = 0.0, R = 0.9995;

    void prepare (double fs) noexcept { R = 1.0 - (kTwoPi * 8.0 / fs); }
    void reset() noexcept { x1 = y1 = 0.0; }

    inline double process (double x) noexcept
    {
        const double y = x - x1 + R * y1;
        x1 = x; y1 = flush (y);
        return y;
    }
};

} // namespace fluxcore
