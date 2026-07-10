/*
    FLUXCORE·12 — Oversampler.h

    A self-contained integer oversampler (2× / 4× / 8× / 16×) built from a
    cascade of two-times half-band stages. Each stage is a windowed-sinc (Kaiser)
    linear-phase FIR used both for anti-imaging on the way up and anti-aliasing
    on the way down. Kept JUCE-free so the whole anti-aliasing chain can be
    measured in the unit tests.

    Round-trip group delay (base-rate samples) for an L-tap stage filter over an
    N-times cascade is exactly  (L-1)·(1 - 1/N)  — reported to the host for PDC.
*/

#pragma once

#include <vector>
#include "DspUtils.h"

namespace fluxcore
{
//==============================================================================
/** Linear-phase FIR with a circular delay line; single-sample streaming. */
struct FIR
{
    std::vector<double> h;      // taps
    std::vector<double> z;      // delay line
    int pos = 0;

    void setTaps (std::vector<double> taps) noexcept
    {
        h = std::move (taps);
        z.assign (h.size(), 0.0);
        pos = 0;
    }
    void reset() noexcept { std::fill (z.begin(), z.end(), 0.0); pos = 0; }

    inline double process (double x) noexcept
    {
        const int n = (int) h.size();
        z[(size_t) pos] = x;
        double acc = 0.0;
        int idx = pos;
        for (int k = 0; k < n; ++k)
        {
            acc += h[(size_t) k] * z[(size_t) idx];
            if (--idx < 0) idx = n - 1;
        }
        if (++pos >= n) pos = 0;
        return acc;
    }
};

//==============================================================================
namespace osdetail
{
    inline double besselI0 (double x) noexcept
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 40; ++k)
        {
            term *= (x * 0.5) / k;
            sum  += term * term;
            if (term * term < 1.0e-18 * sum) break;
        }
        return sum;
    }

    /** Kaiser-windowed sinc low-pass, DC-normalised. fc normalised to the
        stage's own (high) sample rate. */
    inline std::vector<double> designLowpass (int L, double fc, double beta)
    {
        std::vector<double> h ((size_t) L, 0.0);
        const int   M    = L - 1;
        const double i0b = besselI0 (beta);
        double sum = 0.0;
        for (int n = 0; n < L; ++n)
        {
            const double m = n - M * 0.5;
            const double sinc = (std::abs (m) < 1.0e-9)
                                  ? 2.0 * fc
                                  : std::sin (kTwoPi * fc * m) / (kPi * m);
            const double r = 2.0 * n / (double) M - 1.0;
            const double w = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0b;
            h[(size_t) n] = sinc * w;
            sum += h[(size_t) n];
        }
        for (auto& v : h) v /= sum; // unity DC gain
        return h;
    }
}

//==============================================================================
struct Oversampler
{
    static constexpr int kMaxStages = 4; // up to 16×

    int factor = 1;
    int numStages = 0;
    int tapCount = 63;

    FIR up[kMaxStages];
    FIR down[kMaxStages];

    std::vector<double> scratchA, scratchB, upBuffer;

    /** @param f       oversampling factor (1,2,4,8,16)
        @param taps    FIR length per stage (odd; longer => steeper)
        @param maxN    maximum base-rate block size */
    void prepare (int f, int taps, int maxN)
    {
        factor    = (f < 1) ? 1 : f;
        numStages = 0;
        for (int t = factor; t > 1; t >>= 1) ++numStages;
        numStages = clampT (numStages, 0, kMaxStages);
        tapCount  = taps | 1; // force odd

        auto coeffs = osdetail::designLowpass (tapCount, 0.25, 8.0);
        for (int s = 0; s < numStages; ++s)
        {
            up[s].setTaps (coeffs);
            down[s].setTaps (coeffs);
        }
        scratchA.assign ((size_t) maxN * (size_t) factor, 0.0);
        scratchB.assign ((size_t) maxN * (size_t) factor, 0.0);
        upBuffer.assign ((size_t) maxN * (size_t) factor, 0.0);
    }

    void reset() noexcept
    {
        for (int s = 0; s < numStages; ++s) { up[s].reset(); down[s].reset(); }
    }

    int latencySamples() const noexcept
    {
        if (factor <= 1) return 0;
        return (int) std::lround ((tapCount - 1) * (1.0 - 1.0 / factor));
    }

    /** Upsample nIn base-rate samples into the internal buffer.
        @returns pointer to nIn*factor oversampled samples (writable in place). */
    double* upsample (const double* in, int nIn, int& nUpOut) noexcept
    {
        if (factor == 1)
        {
            for (int i = 0; i < nIn; ++i) upBuffer[(size_t) i] = in[i];
            nUpOut = nIn;
            return upBuffer.data();
        }

        // stage 0 reads 'in', writes scratchA
        int curN = nIn;
        const double* src = in;
        double* dst = scratchA.data();

        for (int s = 0; s < numStages; ++s)
        {
            const int outN = curN * 2;
            for (int i = 0; i < curN; ++i)
            {
                // zero-stuff + LP; ×2 gain compensates the inserted zero
                dst[(size_t) (2 * i)]     = up[s].process (2.0 * src[i]);
                dst[(size_t) (2 * i + 1)] = up[s].process (0.0);
            }
            curN = outN;
            src = dst;
            dst = (dst == scratchA.data()) ? scratchB.data() : scratchA.data();
        }

        for (int i = 0; i < curN; ++i) upBuffer[(size_t) i] = src[i];
        nUpOut = curN;
        return upBuffer.data();
    }

    /** Downsample the (processed) oversampled buffer back to base rate. */
    void downsample (const double* up_, int nUp, double* out, int nIn) noexcept
    {
        if (factor == 1)
        {
            for (int i = 0; i < nIn; ++i) out[i] = up_[(size_t) i];
            return;
        }

        int curN = nUp;
        // copy input into scratchA to allow in-place ping-pong
        for (int i = 0; i < curN; ++i) scratchA[(size_t) i] = up_[(size_t) i];
        double* src = scratchA.data();
        double* dst = scratchB.data();

        for (int s = 0; s < numStages; ++s)
        {
            const int outN = curN / 2;
            for (int i = 0; i < outN; ++i)
            {
                down[s].process (src[(size_t) (2 * i)]);
                dst[(size_t) i] = down[s].process (src[(size_t) (2 * i + 1)]);
            }
            curN = outN;
            std::swap (src, dst);
        }

        for (int i = 0; i < nIn; ++i) out[i] = src[(size_t) i];
    }
};

} // namespace fluxcore
