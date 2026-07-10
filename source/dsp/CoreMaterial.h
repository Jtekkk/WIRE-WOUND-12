/*
    FLUXCORE·12 — Transformer Core Modeler
    CoreMaterial.h

    Twelve physically-motivated core materials. Each is a small, plain-old-data
    coefficient set: Jiles–Atherton hysteresis parameters (Ms, a, k, c, α) plus
    the audio-domain macro descriptors (knee, loss corner, winding resonance,
    LF behaviour) that the shared DSP path reads. Only the coefficients swap
    between cores — the signal path is identical — which is how twelve "units"
    are delivered for the cost of roughly one.

    All values are normalised / tuned for musical behaviour rather than strict
    SI accuracy; the lineage column in the product spec is the design intent.

    JUCE-free by design so the engine remains unit-testable.
*/

#pragma once

#include <array>
#include "Parameters.h"

namespace fluxcore
{
//==============================================================================
struct CoreMaterial
{
    const char* name;        // full display name
    const char* shortName;   // compact label for the selector / meters

    //== Jiles–Atherton hysteresis (drives the magnetic-memory stage) ==========
    double Ms;      // saturation magnetisation (normalised)
    double a;       // Langevin shape / domain-wall density — larger => later, softer
    double k;       // pinning: hysteresis-loop width / coercivity / loss
    double c;       // reversibility 0..1 — larger => cleaner, less memory
    double alpha;   // inter-domain mean-field coupling

    //== Audio-domain macro descriptors ========================================
    double Bs;         // saturation flux density (Tesla) — display + drive scaling
    double perm;       // relative permeability proxy -> drive-to-flux gain
    double headroom;   // relative level to reach hard break-up (knee position)
    double kneeSharp;  // saturation knee sharpness -> odd-harmonic character
    double evenBias;   // intrinsic even-harmonic tendency (asymmetry seed)

    //== Frequency-dependent core loss (eddy-current + hysteresis) =============
    double eddyCornerHz;  // HF loss corner at nominal load
    double hfTiltDb;      // broadband tilt referenced ~10 kHz (negative = darker)
    double midPresenceDb; // mid-band emphasis (~2.5 kHz) for "honk"/presence

    //== Winding network (leakage L + interwinding C) ==========================
    double windingResHz;  // nominal interwinding resonance frequency
    double windingQ;      // resonance sharpness
    double leakage;       // leakage inductance -> HF rolloff amount 0..1

    //== Low-frequency behaviour ==============================================
    double lfSatDepth;    // strength of flux-dependent LF saturation
    double lfCornerHz;    // LF rolloff corner (bass tightness)

    //== Housekeeping =========================================================
    double makeupDb;      // per-core level trim so cores broadly match at unity
    bool   splitBand;     // core 12: nickel-top / steel-low crossover behaviour
};

//==============================================================================
/** The twelve cores. Index == the CORE parameter value. */
inline constexpr std::array<CoreMaterial, kNumCores> kCoreMaterials { {
    //  name / short
    //  Ms    a     k     c     alpha    Bs    perm  head  knee  even |  eddy    tilt  mid  |  wRes   wQ    leak |  lfD   lfHz | makeup split
    { "Grain-Oriented Silicon Steel", "GOSS",
       1.00, 0.90, 0.45, 0.55, 0.00160, 2.00, 1.00, 1.00, 1.60, 0.060, 12000.0, -1.5, 0.5, 22000.0, 0.90, 0.35, 0.60, 22.0, -1.0, false },

    { "High-Nickel 80% Permalloy",    "80Ni",
       0.75, 0.35, 0.08, 0.90, 0.00090, 0.75, 1.50, 0.70, 1.00, 0.020, 26000.0,  0.8, 0.0, 30000.0, 0.50, 0.15, 0.20, 12.0,  0.5, false },

    { "Laminated Iron (Standard)",    "IRON",
       1.00, 1.00, 0.60, 0.40, 0.00200, 1.80, 0.95, 0.90, 2.20, 0.100,  7000.0, -4.0, 1.0, 16000.0, 0.70, 0.50, 0.70, 30.0, -2.5, false },

    { "Cobalt-Iron (Permendur)",      "COFE",
       1.20, 1.40, 0.40, 0.60, 0.00180, 2.35, 0.90, 1.60, 3.00, 0.050, 14000.0, -1.0, 0.3, 20000.0, 1.00, 0.30, 0.80, 20.0, -0.5, false },

    { "Amorphous (Co-based Metglas)", "AMOR",
       0.60, 0.30, 0.05, 0.95, 0.00070, 0.55, 1.60, 0.75, 1.00, 0.015, 30000.0,  0.0, 0.0, 34000.0, 0.40, 0.10, 0.25,  8.0,  1.0, false },

    { "Nanocrystalline (Finemet)",    "NANO",
       1.10, 0.40, 0.04, 0.97, 0.00060, 1.20, 1.70, 1.10, 1.00, 0.010, 32000.0,  0.0, 0.0, 33000.0, 0.40, 0.08, 0.40, 10.0,  0.5, false },

    { "Ferrite (MnZn)",               "FERR",
       0.50, 0.45, 0.25, 0.55, 0.00120, 0.45, 1.20, 0.65, 1.80, 0.030, 40000.0,  3.0,-0.5, 26000.0, 1.60, 0.20, 0.15, 45.0,  0.0, false },

    { "Carbonyl Powdered Iron",       "CARB",
       0.90, 1.30, 0.20, 0.75, 0.00140, 1.00, 0.80, 1.00, 0.80, 0.050, 11000.0, -2.0, 0.2, 18000.0, 0.60, 0.40, 0.50, 24.0, -1.0, false },

    { "Supermalloy (79Ni-5Mo)",       "SUPM",
       0.70, 0.28, 0.03, 0.98, 0.00050, 0.70, 1.80, 0.70, 0.90, 0.010, 28000.0,  0.3, 0.0, 31000.0, 0.45, 0.12, 0.15,  9.0,  1.0, false },

    { "Mu-Metal",                     "MUMT",
       0.65, 0.40, 0.09, 0.90, 0.00090, 0.65, 1.50, 0.75, 1.10, 0.030, 20000.0, -1.0, 0.2, 27000.0, 0.60, 0.20, 0.30, 16.0,  0.3, false },

    { "Non-Oriented Steel (M19)",     "M19",
       1.00, 0.95, 0.50, 0.45, 0.00190, 1.90, 0.95, 0.90, 2.00, 0.070, 10000.0, -1.5, 2.0, 17000.0, 0.80, 0.42, 0.60, 28.0, -2.0, false },

    { "Nickel/Steel Hybrid Stack",    "HYBD",
       0.95, 0.60, 0.20, 0.75, 0.00120, 1.20, 1.30, 1.00, 1.40, 0.040, 24000.0,  0.5, 0.2, 28000.0, 0.70, 0.20, 0.55, 18.0,  0.0, true  },
} };

//==============================================================================
/** Bounds-checked accessor. */
inline const CoreMaterial& getCore (int index) noexcept
{
    if (index < 0)             index = 0;
    if (index >= kNumCores)    index = kNumCores - 1;
    return kCoreMaterials[(size_t) index];
}

} // namespace fluxcore
