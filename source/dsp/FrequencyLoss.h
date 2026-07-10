/*
    FLUXCORE·12 — FrequencyLoss.h

    Eddy-current + hysteresis loss, modelled as frequency-weighted damping. This
    is what tilts each core's top end: a gentle low-pass at the material's eddy
    corner, a high-shelf for the broadband tilt, and a mid peak for cores with a
    presence bump (M19 "honk", etc.). AGE nudges the corners down and adds a hair
    of extra HF loss, emulating tolerance drift in a used unit.
*/

#pragma once

#include "Filters.h"
#include "CoreMaterial.h"

namespace fluxcore
{
struct FrequencyLoss
{
    Biquad eddyLP;    // eddy-current HF loss
    Biquad tiltShelf; // broadband tilt
    Biquad midPeak;   // presence / honk

    void reset() noexcept { eddyLP.reset(); tiltShelf.reset(); midPeak.reset(); }

    void update (double fs, const CoreMaterial& m, double age) noexcept
    {
        const double ageCorner = 1.0 - 0.08 * age;   // corners drift down with age
        const double ageTilt    = -1.5 * age;         // slightly darker when aged

        const double eddy = clampT (m.eddyCornerHz * ageCorner, 1000.0, fs * 0.47);
        eddyLP.setLowpass (fs, eddy, 0.55);

        tiltShelf.setHighShelf (fs, 8000.0, m.hfTiltDb + ageTilt);

        if (std::abs (m.midPresenceDb) > 0.01)
            midPeak.setPeak (fs, 2500.0, 0.9, m.midPresenceDb);
        else
            midPeak.setCoeffs (1, 0, 0, 0, 0); // bypass
    }

    inline double process (double x) noexcept
    {
        return midPeak.process (tiltShelf.process (eddyLP.process (x)));
    }

    double magnitudeAt (double fs, double f) const noexcept
    {
        return eddyLP.magnitudeAt (fs, f)
             * tiltShelf.magnitudeAt (fs, f)
             * midPeak.magnitudeAt (fs, f);
    }
};

} // namespace fluxcore
