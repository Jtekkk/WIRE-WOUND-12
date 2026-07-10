/*
    FLUXCORE·12 — Transformer Core Modeler
    Parameters.h

    Single source of truth for every user-facing parameter: its string ID,
    range, default and skew. Both the JUCE-independent DSP engine and the
    APVTS layout in the plugin layer include this header so the two can never
    drift out of sync.

    This header is intentionally free of any JUCE dependency so the whole DSP
    engine can be compiled and unit-tested with a bare C++ toolchain.
*/

#pragma once

#include <array>
#include <cstddef>

namespace fluxcore
{
//==============================================================================
/** Number of physically-modelled transformer cores. */
inline constexpr int kNumCores = 12;

/** Number of harmonics reported by the analyser (H2..H9 => 8 bins). */
inline constexpr int kNumAnalyzerHarmonics = 8; // H2 .. H9

//==============================================================================
/** Discrete choice parameters. Keep the ordering stable — indices are stored
    in host sessions and preset files. */
enum class StereoMode
{
    stereo = 0,   // linked L/R, correlated component processing
    dualMono,     // independent L and R paths, no cross-coupling
    midSide       // independent Mid and Side drive
};

enum class OversampleMode
{
    off = 0,
    x2,
    x4,
    x8,
    x16,
    automatic     // scales with drive / quality
};

enum class QualityMode
{
    eco = 0,      // ADAA 1st order, short filters, real-time friendly
    standard,     // ADAA 2nd order, balanced
    insane        // ADAA 2nd order + longer AA filters, max oversampling
};

//==============================================================================
/** Parameter identifier strings. Used verbatim as APVTS parameter IDs. */
namespace pid
{
    inline constexpr const char* core        = "core";
    inline constexpr const char* coreMorph   = "coreMorph";   // A/B morph (hybrid core)
    inline constexpr const char* drive       = "drive";       // FLUX drive, dB
    inline constexpr const char* bias        = "bias";        // DC asymmetry -> even harmonics
    inline constexpr const char* coreSat     = "coreSat";     // saturation intensity / knee
    inline constexpr const char* hysteresis  = "hysteresis";  // magnetic memory amount
    inline constexpr const char* output      = "output";      // output trim, dB
    inline constexpr const char* mix         = "mix";         // dry/wet

    inline constexpr const char* sourceZ     = "sourceZ";     // driving-stage output impedance
    inline constexpr const char* loadZ       = "loadZ";       // termination impedance
    inline constexpr const char* windingRes  = "windingRes";  // interwinding HF resonance
    inline constexpr const char* lfSat       = "lfSat";       // low-frequency saturation depth

    inline constexpr const char* age         = "age";         // component tolerance drift
    inline constexpr const char* hum         = "hum";         // mains bleed depth
    inline constexpr const char* humFreq     = "humFreq";     // 50 / 60 Hz
    inline constexpr const char* stereoMode  = "stereoMode";
    inline constexpr const char* autoTrim    = "autoTrim";    // loudness-matched bypass
    inline constexpr const char* oversample  = "oversample";
    inline constexpr const char* quality     = "quality";
    inline constexpr const char* bypass      = "bypass";
}

//==============================================================================
/** A plain description of a continuous parameter's range so the DSP layer and
    the plugin layer agree on limits/defaults without sharing a JUCE type. */
struct ParamRange
{
    const char* id;
    float       min;
    float       max;
    float       def;
    float       skew;   // 1.0 = linear; <1 emphasises the low end
    const char* unit;
    const char* name;
};

// Continuous parameter table. (Choice/bool params are declared in the layout.)
inline constexpr std::array<ParamRange, 13> kContinuousParams { {
    { pid::coreMorph,  0.0f,   1.0f,  0.5f,  1.0f, "",   "Core Morph"  },
    { pid::drive,    -24.0f,  24.0f,  0.0f,  1.0f, "dB", "Drive"       },
    { pid::bias,      -1.0f,   1.0f,  0.0f,  1.0f, "",   "Bias"        },
    { pid::coreSat,    0.0f,   1.0f,  0.35f, 1.0f, "",   "Core Sat"    },
    { pid::hysteresis, 0.0f,   1.0f,  0.3f,  1.0f, "",   "Hysteresis"  },
    { pid::output,   -24.0f,  24.0f,  0.0f,  1.0f, "dB", "Output"      },
    { pid::mix,        0.0f,   1.0f,  1.0f,  1.0f, "",   "Mix"         },
    { pid::sourceZ,    0.0f,   1.0f,  0.4f,  1.0f, "",   "Source Z"    },
    { pid::loadZ,      0.0f,   1.0f,  0.5f,  1.0f, "",   "Load Z"      },
    { pid::windingRes, 0.0f,   1.0f,  0.35f, 1.0f, "",   "Winding Res" },
    { pid::lfSat,      0.0f,   1.0f,  0.4f,  1.0f, "",   "LF Sat"      },
    { pid::age,        0.0f,   1.0f,  0.15f, 1.0f, "",   "Age"         },
    { pid::hum,        0.0f,   1.0f,  0.0f,  1.0f, "",   "Hum"         },
} };

} // namespace fluxcore
