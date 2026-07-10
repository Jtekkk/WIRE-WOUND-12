/*
    FLUXCORE·12 — JilesAtherton.h

    A genuine Jiles–Atherton hysteresis model. It traces a real B–H loop (which
    the Flux Meter draws) and, more importantly for the sound, exposes the *lag*
    between the actual magnetisation M and its anhysteretic curve M_an. That lag
    is the physical "magnetic memory": adding a scaled amount of it to the
    anti-aliased saturation gives the core its thickness / smear without altering
    the fundamental saturation shape.

    Standard formulation (Jiles & Atherton 1986; Jiles 1992):
        effective field   He   = H + α·M
        anhysteretic      M_an  = Ms · L(He / a)          L = Langevin
        irreversible      dM_irr/dH = (M_an - M) / (k·δ - α·(M_an - M))
        total             dM/dH = [ (1-c)·dM_irr/dH + c·dM_an/dHe ]
                                  / [ 1 - α·c·dM_an/dHe - α·(1-c)·dM_irr/dH ]

    Integrated with a guarded forward step. Denominators are clamped away from
    zero and the irreversible term is gated (δ_M) so magnetisation never runs
    against the field — both are required for numerical stability at audio rates.
*/

#pragma once

#include "DspUtils.h"

namespace fluxcore
{
struct JilesAtherton
{
    // material coefficients
    double Ms = 1.0, a = 1.0, k = 0.5, c = 0.5, alpha = 1.0e-3;

    // state
    double M = 0.0, Hprev = 0.0, Man = 0.0;

    void setMaterial (double ms, double aa, double kk, double cc, double al) noexcept
    {
        Ms = ms; a = std::max (1.0e-4, aa); k = std::max (1.0e-5, kk);
        c = clampT (cc, 0.0, 0.999); alpha = al;
    }

    void reset() noexcept { M = 0.0; Hprev = 0.0; Man = 0.0; }

    //== Langevin function and its derivative, series-safe near the origin ====
    static inline double langevin (double z) noexcept
    {
        if (std::abs (z) < 1.0e-4)
            return z * (1.0 / 3.0) - z * z * z * (1.0 / 45.0);
        return 1.0 / std::tanh (z) - 1.0 / z;
    }
    static inline double dLangevin (double z) noexcept
    {
        if (std::abs (z) < 1.0e-4)
            return 1.0 / 3.0 - z * z * (1.0 / 15.0);
        const double sh = std::sinh (z);
        return 1.0 / (z * z) - 1.0 / (sh * sh);
    }

    /** Advance one (oversampled) sample with field H; returns magnetisation M. */
    inline double process (double H) noexcept
    {
        const double dH = H - Hprev;
        Hprev = H;
        const double delta = (dH >= 0.0) ? 1.0 : -1.0;

        const double He = H + alpha * M;
        const double z  = He / a;
        Man = Ms * langevin (z);
        const double dMan_dHe = (Ms / a) * dLangevin (z);

        const double diff = Man - M;

        double denomIrr = k * delta - alpha * diff;
        if (std::abs (denomIrr) < 1.0e-9)
            denomIrr = (denomIrr < 0.0 ? -1.0 : 1.0) * 1.0e-9;

        double dMirr_dH = diff / denomIrr;

        // δ_M gate: forbid irreversible magnetisation opposing the field change
        if (diff * delta < 0.0)
            dMirr_dH = 0.0;

        double num = (1.0 - c) * dMirr_dH + c * dMan_dHe;
        double den = 1.0 - alpha * c * dMan_dHe - alpha * (1.0 - c) * dMirr_dH;
        if (std::abs (den) < 1.0e-6)
            den = (den < 0.0 ? -1.0 : 1.0) * 1.0e-6;

        const double dM_dH = num / den;
        M = flush (M + dM_dH * dH);
        M = clampT (M, -1.5 * Ms, 1.5 * Ms);
        return M;
    }

    /** Hysteretic lag: how far the magnetisation trails its anhysteretic value. */
    inline double lag() const noexcept { return M - Man; }

    double getM()   const noexcept { return M; }
    double getMan() const noexcept { return Man; }
};

} // namespace fluxcore
