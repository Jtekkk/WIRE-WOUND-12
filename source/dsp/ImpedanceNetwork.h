/*
    FLUXCORE·12 — ImpedanceNetwork.h

    The source- and load-impedance networks, and the winding resonance. These
    are the "secret sauce": bilinear-transformed R/L/C sections whose corners
    and peaks move with the knobs, so the HF resonance and LF rolloff are a
    property of the network, not a static shelf.

        SOURCE Z   -> the driving stage's output impedance forms a high-pass with
                      the primary inductance: higher Z => tighter / leaner lows.
        WINDING RES-> leakage inductance + interwinding capacitance form a
                      resonant peak; the knob sets its frequency and height.
        LOAD Z     -> termination: light load => brighter, taller resonance;
                      heavy load => damped, darker.
        LF ROLLOFF -> the core's own bass extension, gently moved by the load.

    A pure magnitude query is exposed so the Response-Curve UI can draw the exact
    frequency response the network is producing.
*/

#pragma once

#include "Filters.h"
#include "CoreMaterial.h"

namespace fluxcore
{
struct ImpedanceNetwork
{
    Biquad sourceHP;    // source-impedance LF rolloff (tightness)
    Biquad lfHP;        // core bass extension
    Biquad windingPeak; // interwinding HF resonance
    Biquad loadShelf;   // termination HF damping / brightness

    void reset() noexcept
    {
        sourceHP.reset(); lfHP.reset(); windingPeak.reset(); loadShelf.reset();
    }

    void update (double fs, const CoreMaterial& m,
                 double sourceZ, double loadZ, double windingRes, double age) noexcept
    {
        sourceZ    = clampT (sourceZ,    0.0, 1.0);
        loadZ      = clampT (loadZ,      0.0, 1.0);
        windingRes = clampT (windingRes, 0.0, 1.0);

        // Source impedance -> LF high-pass. Higher Z pushes the corner up.
        const double srcCorner = logMap (sourceZ, 8.0, 90.0);
        sourceHP.setHighpass (fs, srcCorner, 0.7);

        // Core bass extension, gently lifted by a lighter load.
        const double lfCorner = clampT (m.lfCornerHz * (0.8 + 0.4 * loadZ), 5.0, 300.0);
        lfHP.setHighpass (fs, lfCorner, 0.7);

        // Interwinding resonance. Knob moves the frequency and grows the peak;
        // a lighter load (higher loadZ) makes it taller and sharper.
        const double ageFreq = 1.0 - 0.05 * age;
        const double resFreq = clampT (m.windingResHz * (0.75 + 0.5 * windingRes) * ageFreq,
                                       6000.0, fs * 0.46);
        const double resGain = clampT ((1.5 + 7.0 * windingRes) * m.windingQ * (0.5 + loadZ)
                                         - 1.5 * age,
                                       0.0, 12.0);
        const double resQ    = clampT (0.8 + 2.5 * windingRes * (0.6 + 0.6 * loadZ)
                                         - 0.3 * age,
                                       0.7, 4.0);
        windingPeak.setPeak (fs, resFreq, resQ, resGain);

        // Termination brightness: heavy load darkens, light load brightens.
        loadShelf.setHighShelf (fs, 5000.0, (loadZ - 0.5) * 8.0);
    }

    /** Source-side network, applied before the core (per the signal flow). */
    inline double processPre (double x) noexcept
    {
        x = sourceHP.process (x);
        x = lfHP.process (x);
        return x;
    }

    /** Winding + load network, applied after the core. */
    inline double processPost (double x) noexcept
    {
        x = windingPeak.process (x);
        x = loadShelf.process (x);
        return x;
    }

    inline double process (double x) noexcept { return processPost (processPre (x)); }

    double magnitudeAt (double fs, double f) const noexcept
    {
        return sourceHP.magnitudeAt (fs, f)
             * lfHP.magnitudeAt (fs, f)
             * windingPeak.magnitudeAt (fs, f)
             * loadShelf.magnitudeAt (fs, f);
    }
};

} // namespace fluxcore
