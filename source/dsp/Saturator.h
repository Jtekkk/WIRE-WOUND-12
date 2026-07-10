/*
    FLUXCORE·12 — Saturator.h

    The memoryless anhysteretic nonlinearity that generates the bulk of the
    harmonic content and is the element placed behind ADAA.

    Built on the algebraic sigmoid  f(u) = u / sqrt(1 + u^2)  because — unlike
    tanh — it has closed-form first *and* second antiderivatives, which is what
    makes clean 2nd-order ADAA possible:

        f (u)  = u (1+u^2)^(-1/2)
        F1(u)  = sqrt(1 + u^2)
        F2(u)  = 0.5 * ( u*sqrt(1+u^2) + asinh(u) )

    A "knee" gain g sets how hard the curve bends relative to the operating
    level; the shape stays unity-slope at the origin:

        s (x)  = (1/g) f(g x)              s'(0) = 1

    A DC bias b introduces asymmetry — the source of even-order harmonics — with
    the operating-point offset removed so the transfer curve still passes through
    the origin:

        N (x)  = s(x + b) - s(b)
*/

#pragma once

#include "DspUtils.h"
#include "ADAA.h"

namespace fluxcore
{
struct AlgebraicSaturator
{
    double g  = 1.0;   // knee gain (>0). larger => harder/earlier saturation
    double b  = 0.0;   // DC bias (asymmetry -> even harmonics)
    double sb = 0.0;   // cached s(b)

    void setKnee (double knee) noexcept { g  = std::max (0.05, knee); recomputeBias(); }
    void setBias (double bias) noexcept { b  = bias;                   recomputeBias(); }

    void recomputeBias() noexcept { sb = s (b); }

    //== core scaled sigmoid and its antiderivatives ==========================
    inline double s (double x) const noexcept
    {
        const double u = g * x;
        return (u / std::sqrt (1.0 + u * u)) / g;
    }
    inline double S1 (double x) const noexcept
    {
        const double u = g * x;
        return std::sqrt (1.0 + u * u) / (g * g);
    }
    inline double S2 (double x) const noexcept
    {
        const double u = g * x;
        return 0.5 * (u * std::sqrt (1.0 + u * u) + std::asinh (u)) / (g * g * g);
    }

    //== the biased nonlinearity presented to ADAA ============================
    inline double f0 (double x) const noexcept { return s (x + b) - sb; }
    inline double f1 (double x) const noexcept { return S1 (x + b) - sb * x; }
    inline double f2 (double x) const noexcept { return S2 (x + b) - sb * 0.5 * x * x; }
};

//==============================================================================
/** Anti-aliased saturator: ADAA wrapper around the algebraic sigmoid. */
struct AASaturator
{
    ADAA<AlgebraicSaturator> adaa;

    void prepare (int adaaOrder = 2) noexcept
    {
        adaa.order = clampT (adaaOrder, 1, 2);
        adaa.shaper.setKnee (1.0);
        adaa.shaper.setBias (0.0);
        adaa.reset();
    }

    void reset() noexcept { adaa.reset(); }

    void setOrder (int adaaOrder) noexcept { adaa.order = clampT (adaaOrder, 1, 2); }

    /** Update knee / bias together, then refresh the antiderivative cache. */
    void setShape (double knee, double bias) noexcept
    {
        adaa.shaper.setKnee (knee);
        adaa.shaper.setBias (bias);
        adaa.refreshCache();
    }

    inline double process (double x) noexcept { return adaa.process (x); }

    /** Direct (non-anti-aliased) evaluation — used by the transfer-curve UI. */
    inline double evaluate (double x) const noexcept { return adaa.shaper.f0 (x); }
};

} // namespace fluxcore
