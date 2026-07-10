/*
    FLUXCORE·12 — ADAA.h

    Anti-derivative anti-aliasing (Parker, Zavalishin & Le Bivic, DAFx-16) for
    memoryless waveshapers, 1st and 2nd order, with the standard ill-conditioned
    fallbacks so consecutive equal / near-equal samples don't divide by zero.

    A "Shaper" supplied to this template must provide three evaluators:
        double f0 (double x)  -> the nonlinearity itself
        double f1 (double x)  -> its first antiderivative
        double f2 (double x)  -> its second antiderivative
    Constants of integration are irrelevant: every use is a difference.

    Anti-aliasing costs group delay — half a sample (1st order) and one sample
    (2nd order) — which the host is told about via reported latency.
*/

#pragma once

#include "DspUtils.h"

namespace fluxcore
{
template <typename Shaper>
struct ADAA
{
    Shaper shaper;

    // input history
    double x1 = 0.0, x2 = 0.0;
    // cached antiderivative history to avoid recomputation
    double f1x1 = 0.0;                 // f1(x1)
    double f2x1 = 0.0, f2x2 = 0.0;     // f2(x1), f2(x2)

    int order = 2;                     // 1 or 2
    static constexpr double eps = 1.0e-6;

    void reset() noexcept
    {
        x1 = x2 = 0.0;
        f1x1 = shaper.f1 (0.0);
        f2x1 = shaper.f2 (0.0);
        f2x2 = shaper.f2 (0.0);
    }

    /** Recompute cached antiderivatives after the shaper's parameters change. */
    void refreshCache() noexcept
    {
        f1x1 = shaper.f1 (x1);
        f2x1 = shaper.f2 (x1);
        f2x2 = shaper.f2 (x2);
    }

    inline double process (double x0) noexcept
    {
        return (order <= 1) ? process1 (x0) : process2 (x0);
    }

    //== 1st order ============================================================
    inline double process1 (double x0) noexcept
    {
        const double f1x0 = shaper.f1 (x0);
        double y;
        const double diff = x0 - x1;
        if (std::abs (diff) < eps)
            y = shaper.f0 (0.5 * (x0 + x1));
        else
            y = (f1x0 - f1x1) / diff;

        x1 = x0;
        f1x1 = f1x0;
        return y;
    }

    //== 2nd order ============================================================
    inline double D2 (double a, double b, double f2a, double f2b) const noexcept
    {
        const double d = a - b;
        if (std::abs (d) < eps)
            return shaper.f1 (0.5 * (a + b));
        return (f2a - f2b) / d;
    }

    inline double process2 (double x0) noexcept
    {
        const double f2x0 = shaper.f2 (x0);
        double y;

        const double outer = x0 - x2;
        if (std::abs (outer) < eps)
        {
            // ill-conditioned outer difference: Taylor-style fallback around x1
            const double xBar  = 0.5 * (x0 + x2);
            const double delta = xBar - x1;
            if (std::abs (delta) < eps)
            {
                y = shaper.f0 (0.5 * (xBar + x1));
            }
            else
            {
                const double f2xBar = shaper.f2 (xBar);
                y = (2.0 / delta) * (shaper.f1 (xBar) + (f2x1 - f2xBar) / delta);
            }
        }
        else
        {
            const double a = D2 (x0, x1, f2x0, f2x1);
            const double b = D2 (x1, x2, f2x1, f2x2);
            y = (2.0 / outer) * (a - b);
        }

        // shift history
        x2 = x1;   f2x2 = f2x1;
        x1 = x0;   f2x1 = f2x0;
        return y;
    }
};

} // namespace fluxcore
