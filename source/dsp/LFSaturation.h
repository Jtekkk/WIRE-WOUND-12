/*
    FLUXCORE·12 — LFSaturation.h

    A separate flux-dependent low-frequency saturation path. Real transformer
    bass distortion is both level- and frequency-dependent: the core saturates
    harder on sustained low end than on transients. A low-passed copy of the
    signal is driven into its own anti-aliased saturator and only the *added*
    harmonic content is returned, to be summed back into the main flux. Depth
    scales with the LF SAT knob and the core's own lfSatDepth.
*/

#pragma once

#include "Filters.h"
#include "Saturator.h"
#include "CoreMaterial.h"

namespace fluxcore
{
struct LFSaturation
{
    OnePole     band;      // isolates the low band
    AASaturator sat;       // anti-aliased LF saturator
    double      depth = 0.0;
    double      lfDrive = 2.0;

    void prepare (int adaaOrder) noexcept { sat.prepare (adaaOrder); }
    void reset()                noexcept { band.reset(); sat.reset(); }
    void setOrder (int o)       noexcept { sat.setOrder (o); }

    void update (double fs, const CoreMaterial& m, double lfSatKnob) noexcept
    {
        band.setLowpass (fs, 180.0);
        depth   = clampT (lfSatKnob * m.lfSatDepth, 0.0, 1.0);
        lfDrive = 1.5 + 4.0 * lfSatKnob;
        sat.setShape (1.6, 0.0);
    }

    /** Returns only the added LF harmonic content (parallel injection). */
    inline double process (double x) noexcept
    {
        const double lo  = band.process (x);
        const double slo = sat.process (lo * lfDrive) / lfDrive;
        return depth * (slo - lo);
    }
};

} // namespace fluxcore
