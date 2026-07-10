/*
    FLUXCORE·12 — Hum.h
    Optional mains bleed: fundamental (50/60 Hz) plus a little 2nd/3rd, injected
    into the magnetic field so it passes through the core nonlinearity like real
    hum would. Defeatable; depth-controlled.
*/

#pragma once

#include "DspUtils.h"

namespace fluxcore
{
struct Hum
{
    double phase = 0.0, inc = 0.0, depth = 0.0;

    void prepare (double fs, double freq) noexcept
    {
        inc = kTwoPi * freq / fs;
    }
    void setFreq (double fs, double freq) noexcept { inc = kTwoPi * freq / fs; }
    void setDepth (double d) noexcept { depth = clampT (d, 0.0, 1.0); }
    void reset() noexcept { phase = 0.0; }

    inline double tick() noexcept
    {
        if (depth <= 0.0) { return 0.0; }
        const double s1 = std::sin (phase);
        const double s2 = std::sin (2.0 * phase) * 0.30;   // slight 2nd
        const double s3 = std::sin (3.0 * phase) * 0.15;   // slight 3rd
        phase += inc;
        if (phase >= kTwoPi) phase -= kTwoPi;
        // small in absolute terms — hum is a bleed, not a signal
        return depth * 0.02 * (s1 + s2 + s3);
    }
};

} // namespace fluxcore
