/*
    FLUXCORE·12 — DspUtils.h
    Small, dependency-free numeric helpers shared across the engine.
*/

#pragma once

#include <cmath>
#include <algorithm>

namespace fluxcore
{
inline constexpr double kPi     = 3.14159265358979323846;
inline constexpr double kTwoPi  = 2.0 * kPi;

//==============================================================================
/** Flush denormals to zero — cheap protection for recursive filters running
    on decaying tails. */
template <typename T>
inline T flush (T x) noexcept
{
    return (std::abs (x) < (T) 1.0e-20) ? (T) 0 : x;
}

template <typename T>
inline T clampT (T x, T lo, T hi) noexcept
{
    return std::min (std::max (x, lo), hi);
}

/** Linear interpolation. */
template <typename T>
inline T lerp (T a, T b, T t) noexcept
{
    return a + (b - a) * t;
}

/** Map a normalised 0..1 control across a log range (e.g. a frequency knob). */
inline double logMap (double norm, double lo, double hi) noexcept
{
    norm = clampT (norm, 0.0, 1.0);
    return lo * std::pow (hi / lo, norm);
}

inline double dbToGain (double db) noexcept  { return std::pow (10.0, db / 20.0); }
inline double gainToDb (double g)  noexcept  { return 20.0 * std::log10 (std::max (g, 1.0e-12)); }

/** Numerically-stable log(cosh(x)) — antiderivative of tanh. */
inline double logCosh (double x) noexcept
{
    const double ax = std::abs (x);
    // log(cosh x) = |x| + log(1 + e^{-2|x|}) - log 2
    return ax + std::log1p (std::exp (-2.0 * ax)) - 0.6931471805599453;
}

/** One-pole smoothing coefficient for a given time constant. */
inline double onePoleCoeff (double timeMs, double sampleRate) noexcept
{
    if (timeMs <= 0.0) return 0.0;
    return std::exp (-1.0 / (0.001 * timeMs * sampleRate));
}

} // namespace fluxcore
