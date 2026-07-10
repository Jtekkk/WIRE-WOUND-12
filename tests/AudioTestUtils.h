/*
    FLUXCORE·12 — AudioTestUtils.h
    Signal generation and measurement helpers for the native DSP tests:
    sines, RMS/peak, Goertzel magnitude, THD + per-harmonic breakdown, a small
    radix-2 FFT and an aliasing-energy ratio.
*/

#pragma once

#include <vector>
#include <cmath>
#include <complex>
#include <string>
#include <cstdio>
#include <algorithm>

namespace fctest
{
constexpr double PI = 3.14159265358979323846;

inline std::vector<double> makeSine (double fs, double freq, double amp, int n, double phase = 0.0)
{
    std::vector<double> v ((size_t) n);
    const double w = 2.0 * PI * freq / fs;
    for (int i = 0; i < n; ++i) v[(size_t) i] = amp * std::sin (w * i + phase);
    return v;
}

inline double rms (const std::vector<double>& v)
{
    double s = 0.0;
    for (double x : v) s += x * x;
    return v.empty() ? 0.0 : std::sqrt (s / (double) v.size());
}

inline double peak (const std::vector<double>& v)
{
    double p = 0.0;
    for (double x : v) p = std::max (p, std::abs (x));
    return p;
}

inline bool allFinite (const std::vector<double>& v)
{
    for (double x : v) if (! std::isfinite (x)) return false;
    return true;
}

/** Goertzel single-bin magnitude (linear). Most accurate when freq*n/fs is an
    integer (an exact number of cycles in the window). */
inline double goertzelMag (const std::vector<double>& v, double fs, double freq)
{
    const int    n  = (int) v.size();
    const double w  = 2.0 * PI * freq / fs;
    const double cw = std::cos (w);
    const double c  = 2.0 * cw;
    double s0 = 0.0, s1 = 0.0, s2 = 0.0;
    for (int i = 0; i < n; ++i)
    {
        s0 = v[(size_t) i] + c * s1 - s2;
        s2 = s1; s1 = s0;
    }
    const double re = s1 - s2 * cw;
    const double im = s2 * std::sin (w);
    return 2.0 * std::sqrt (re * re + im * im) / (double) n;
}

/** THD and per-harmonic magnitudes (H1..Hmax). Returns THD ratio (H2..Hmax / H1). */
inline double thd (const std::vector<double>& v, double fs, double f0,
                   int maxH, std::vector<double>& harmonics)
{
    harmonics.assign ((size_t) maxH, 0.0);
    double h1 = 0.0, restSq = 0.0;
    for (int k = 1; k <= maxH; ++k)
    {
        const double f = f0 * k;
        if (f >= fs * 0.5) break;
        const double m = goertzelMag (v, fs, f);
        harmonics[(size_t) (k - 1)] = m;
        if (k == 1) h1 = m;
        else        restSq += m * m;
    }
    return (h1 > 1.0e-12) ? std::sqrt (restSq) / h1 : 0.0;
}

//==============================================================================
/** In-place iterative radix-2 FFT (n must be a power of two). */
inline void fft (std::vector<std::complex<double>>& a)
{
    const int n = (int) a.size();
    for (int i = 1, j = 0; i < n; ++i)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[(size_t) i], a[(size_t) j]);
    }
    for (int len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * PI / len;
        const std::complex<double> wlen (std::cos (ang), std::sin (ang));
        for (int i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0, 0.0);
            for (int k = 0; k < len / 2; ++k)
            {
                const std::complex<double> u = a[(size_t) (i + k)];
                const std::complex<double> t = w * a[(size_t) (i + k + len / 2)];
                a[(size_t) (i + k)]           = u + t;
                a[(size_t) (i + k + len / 2)] = u - t;
                w *= wlen;
            }
        }
    }
}

/** Ratio of inharmonic ("alias") energy to fundamental energy. A Hann window is
    applied; bins within `guard` of DC, the fundamental and its in-band harmonics
    are excluded, so what remains is dominated by aliased spectral content. */
inline double aliasRatio (const std::vector<double>& sig, double fs, double f0)
{
    int n = 1;
    while (n * 2 <= (int) sig.size()) n <<= 1; // largest power of two that fits
    std::vector<std::complex<double>> a ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const double win = 0.5 - 0.5 * std::cos (2.0 * PI * i / (n - 1));
        a[(size_t) i] = std::complex<double> (sig[(size_t) i] * win, 0.0);
    }
    fft (a);

    const double binHz = fs / n;
    const int    half  = n / 2;
    const int    guard = 3;

    auto isHarmonicBin = [&] (int bin)
    {
        const double f = bin * binHz;
        if (f < 25.0) return true;                       // DC region
        for (int k = 1; k * f0 < fs * 0.5; ++k)
        {
            const int hb = (int) std::lround (k * f0 / binHz);
            if (std::abs (bin - hb) <= guard) return true;
        }
        return false;
    };

    double fundE = 0.0, aliasE = 0.0;
    const int f0bin = (int) std::lround (f0 / binHz);
    for (int b = 1; b < half; ++b)
    {
        const double mag = std::abs (a[(size_t) b]);
        const double e   = mag * mag;
        if (std::abs (b - f0bin) <= guard) fundE += e;
        else if (! isHarmonicBin (b))      aliasE += e;
    }
    return (fundE > 1.0e-20) ? std::sqrt (aliasE / fundE) : 0.0;
}

//==============================================================================
struct Runner
{
    int passed = 0, failed = 0;

    bool check (const std::string& name, bool cond, const std::string& detail = "")
    {
        if (cond) { ++passed; std::printf ("  [PASS] %s\n", name.c_str()); }
        else      { ++failed; std::printf ("  [FAIL] %s   %s\n", name.c_str(), detail.c_str()); }
        return cond;
    }

    void section (const std::string& s) { std::printf ("\n=== %s ===\n", s.c_str()); }

    int summarise() const
    {
        std::printf ("\n----------------------------------------\n");
        std::printf ("  %d passed, %d failed\n", passed, failed);
        std::printf ("----------------------------------------\n");
        return failed == 0 ? 0 : 1;
    }
};

} // namespace fctest
