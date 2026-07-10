/*
    FLUXCORE·12 — test_main.cpp

    Native, JUCE-free verification of the DSP engine. Exercises every physical
    subsystem end-to-end and prints a readable report:

      1. Per-core harmonic profile + stability sweep
      2. BIAS -> even harmonics
      3. DRIVE -> more saturation
      4. ADAA order & oversampling -> less aliasing
      5. Impedance networks move the frequency response
      6. Jiles–Atherton hysteresis -> real B–H loop area (memory)
      7. Oversampler reconstruction + reported latency
      8. Auto-Trim loudness match
      9. Dry/Wet + PDC: mix=0 returns the input delayed by exactly the latency
     10. Full-engine stability across all cores at extreme drive

    Exit code 0 iff every assertion passes.
*/

#include <cstdio>
#include <vector>
#include <array>
#include <string>

#include "../source/dsp/Engine.h"
#include "AudioTestUtils.h"

using namespace fctest;
using namespace fluxcore;

//==============================================================================
// Run a mono sine through a single bare TransformerCore (base rate, OS off).
static std::vector<double> runCore (int coreIndex, double fs, double f0, double amp,
                                    int n, double driveDb, double bias = 0.0,
                                    double coreSat = 0.35, double hyst = 0.3)
{
    TransformerCore core;
    core.prepare (fs, 2);
    CoreControls c;
    c.coreIndex = coreIndex; c.driveDb = driveDb; c.bias = bias;
    c.coreSat = coreSat; c.hysteresis = hyst; c.adaaOrder = 2;
    core.updateControls (c);

    auto in = makeSine (fs, f0, amp, n);
    std::vector<double> out ((size_t) n);
    for (int i = 0; i < n; ++i)
        out[(size_t) i] = core.processSample (in[(size_t) i], 0.0);
    return out;
}

//==============================================================================
int main()
{
    Runner R;
    const double fs = 48000.0;

    std::printf ("FLUXCORE·12  —  DSP verification\n");

    //--------------------------------------------------------------------------
    R.section ("1. Per-core harmonic profile @ 1 kHz, -6 dBFS, +6 dB drive");
    std::printf ("  %-28s %8s %8s %8s %8s %8s\n", "core", "THD%", "H2%", "H3%", "H5%", "peak");

    const int    N   = 48000;   // 1 s -> 1000 exact cycles of 1 kHz
    const double f0  = 1000.0;
    const double amp = 0.5;     // -6 dBFS

    std::array<double, kNumCores> thdByCore {};
    bool allStable = true;
    for (int ci = 0; ci < kNumCores; ++ci)
    {
        auto y = runCore (ci, fs, f0, amp, N, 6.0);
        std::vector<double> h;
        const double t = thd (y, fs, f0, 9, h);
        thdByCore[(size_t) ci] = t;
        const double h1 = h[0] > 1e-12 ? h[0] : 1.0;
        const double pk = peak (y);
        if (! allFinite (y) || pk > 4.0) allStable = false;
        std::printf ("  %-28s %8.3f %8.3f %8.3f %8.3f %8.3f\n",
                     getCore (ci).name, t * 100.0,
                     h[1] / h1 * 100.0, h[2] / h1 * 100.0, h[4] / h1 * 100.0, pk);
    }
    R.check ("all cores finite & bounded", allStable);

    // clean set vs colour set THD ordering
    const int cleanIdx[]  = { 1, 4, 5, 8 };   // permalloy, amorphous, nanocrystalline, supermalloy
    const int colourIdx[] = { 0, 2, 3, 10 };  // GOSS, iron, cobalt, M19
    double cleanAvg = 0, colourAvg = 0;
    for (int i : cleanIdx)  cleanAvg  += thdByCore[(size_t) i];
    for (int i : colourIdx) colourAvg += thdByCore[(size_t) i];
    cleanAvg /= 4; colourAvg /= 4;
    R.check ("reference cores are cleaner than colour cores",
             cleanAvg < colourAvg,
             "clean=" + std::to_string (cleanAvg) + " colour=" + std::to_string (colourAvg));

    //--------------------------------------------------------------------------
    R.section ("2. BIAS introduces even harmonics (GOSS)");
    {
        std::vector<double> h0, hb;
        auto y0 = runCore (0, fs, f0, amp, N, 6.0, 0.0);
        auto yb = runCore (0, fs, f0, amp, N, 6.0, 0.7);
        thd (y0, fs, f0, 9, h0);
        thd (yb, fs, f0, 9, hb);
        const double h2_0 = h0[1] / h0[0];
        const double h2_b = hb[1] / hb[0];
        std::printf ("  H2 at bias 0.0 = %.4f%%,  bias 0.7 = %.4f%%\n", h2_0 * 100, h2_b * 100);
        R.check ("bias raises 2nd harmonic", h2_b > h2_0 * 1.5);
    }

    //--------------------------------------------------------------------------
    R.section ("3. DRIVE increases saturation (cobalt-iron)");
    {
        std::vector<double> hl, hh;
        auto yl = runCore (3, fs, f0, amp, N, 0.0);
        auto yh = runCore (3, fs, f0, amp, N, 18.0);
        const double tl = thd (yl, fs, f0, 9, hl);
        const double th = thd (yh, fs, f0, 9, hh);
        std::printf ("  THD @ 0 dB = %.3f%%,  @ +18 dB = %.3f%%\n", tl * 100, th * 100);
        R.check ("more drive -> more THD", th > tl * 1.5);
    }

    //--------------------------------------------------------------------------
    R.section ("4. ADAA order & oversampling reduce aliasing (5 kHz @ 44.1 k, hot)");
    {
        const double fsa = 44100.0;
        const int    Na  = 1 << 15;
        const double fa  = 5000.0;

        auto runEngine = [&] (OversampleMode osm, QualityMode q)
        {
            Engine e;
            e.prepare (fsa, 1024, 1);
            EngineParameters p;
            p.core.coreIndex  = 2;     // laminated iron: plenty of harmonics
            p.core.driveDb    = 20.0;
            p.core.coreSat    = 0.8;
            p.core.hysteresis = 0.0;   // isolate the ADAA'd nonlinearity: the
            p.core.lfSat      = 0.0;   // JA memory / LF paths are not ADAA'd
            p.oversample      = osm;
            p.quality         = q;
            p.mix             = 1.0;
            e.setParameters (p);

            auto in = makeSine (fsa, fa, 0.7, Na);
            std::vector<double> buf = in;
            // process in blocks
            for (int i = 0; i < Na; i += 512)
            {
                const int nb = std::min (512, Na - i);
                double* b[1] = { buf.data() + i };
                e.process (b, 1, nb);
            }
            return aliasRatio (buf, fsa, fa);
        };

        const double aOff1 = runEngine (OversampleMode::off, QualityMode::eco);      // ADAA 1
        const double aOff2 = runEngine (OversampleMode::off, QualityMode::standard); // ADAA 2
        const double aOS4  = runEngine (OversampleMode::x4,  QualityMode::standard);
        std::printf ("  alias ratio:  ADAA1/OS-off = %.4f   ADAA2/OS-off = %.4f   ADAA2/OS4 = %.4f\n",
                     aOff1, aOff2, aOS4);
        R.check ("2nd-order ADAA aliases less than 1st-order", aOff2 < aOff1);
        R.check ("oversampling aliases less than OS-off",       aOS4  < aOff2);
    }

    //--------------------------------------------------------------------------
    R.section ("5. Impedance networks shape the frequency response");
    {
        auto magAt = [&] (double sourceZ, double loadZ, double windingRes, double f)
        {
            TransformerCore core;
            core.prepare (fs, 2);
            CoreControls c; c.coreIndex = 0;
            c.sourceZ = sourceZ; c.loadZ = loadZ; c.windingRes = windingRes;
            core.updateControls (c);
            return core.responseMagnitude (f);
        };
        const double resF = getCore (0).windingResHz;
        const double wLow  = magAt (0.4, 0.5, 0.1, resF);
        const double wHigh = magAt (0.4, 0.5, 0.9, resF);
        std::printf ("  winding resonance mag @ %.0f Hz: knob 0.1 = %.3f, 0.9 = %.3f\n", resF, wLow, wHigh);
        R.check ("winding-res knob raises the resonance", wHigh > wLow * 1.2);

        const double lfLow  = magAt (0.1, 0.5, 0.35, 40.0);
        const double lfHigh = magAt (0.9, 0.5, 0.35, 40.0);
        std::printf ("  LF mag @ 40 Hz: sourceZ 0.1 = %.3f, 0.9 = %.3f\n", lfLow, lfHigh);
        R.check ("higher source-Z tightens (reduces) lows", lfHigh < lfLow);

        const double hfLow  = magAt (0.4, 0.1, 0.2, 12000.0);
        const double hfHigh = magAt (0.4, 0.9, 0.2, 12000.0);
        std::printf ("  HF mag @ 12 kHz: loadZ 0.1 = %.3f, 0.9 = %.3f\n", hfLow, hfHigh);
        R.check ("lighter load brightens the top", hfHigh > hfLow);
    }

    //--------------------------------------------------------------------------
    R.section ("6. Jiles–Atherton hysteresis -> real B–H loop area (memory)");
    {
        auto loopArea = [&] (int coreIndex)
        {
            TransformerCore core; core.prepare (fs, 2);
            CoreControls c; c.coreIndex = coreIndex; c.driveDb = 6.0; c.hysteresis = 1.0;
            core.updateControls (c);
            const double lf = 50.0;
            const int    per = (int) (fs / lf);
            const int    total = per * 8;
            auto in = makeSine (fs, lf, 0.8, total);
            std::vector<double> H, M;
            H.reserve ((size_t) per); M.reserve ((size_t) per);
            for (int i = 0; i < total; ++i)
            {
                core.processSample (in[(size_t) i], 0.0);
                if (i >= total - per)      // last full period
                {
                    H.push_back (core.meterField());
                    M.push_back (core.meterMagnetisation());
                }
            }
            // shoelace area of the (H,M) trajectory
            double area = 0.0;
            const int m = (int) H.size();
            for (int i = 0; i < m; ++i)
            {
                const int j = (i + 1) % m;
                area += H[(size_t) i] * M[(size_t) j] - H[(size_t) j] * M[(size_t) i];
            }
            return std::abs (0.5 * area);
        };
        const double aIron = loopArea (2);   // high k
        const double aSupm = loopArea (8);   // whisper-clean, tiny k
        std::printf ("  loop area  iron = %.5f   supermalloy = %.5f\n", aIron, aSupm);
        R.check ("hysteretic core has non-trivial loop area", aIron > 1e-4);
        R.check ("lossy iron loop area > clean supermalloy",  aIron > aSupm);
    }

    //--------------------------------------------------------------------------
    R.section ("7. Oversampler reconstruction + reported latency");
    {
        Oversampler os;
        os.prepare (4, 63, 512);
        const int lat = os.latencySamples();
        const int expLat = (int) std::lround ((63 - 1) * (1.0 - 1.0 / 4.0));
        std::printf ("  latency = %d samples (expected %d)\n", lat, expLat);
        R.check ("latency matches (L-1)(1-1/N) formula", lat == expLat);

        // The linear-phase FIR cascade has a legitimate (possibly half-sample)
        // group delay, so we verify reconstruction by passband *magnitude*.
        const int    n  = 1 << 14;
        const double tf = 1000.0;
        auto in = makeSine (fs, tf, 0.5, n);
        std::vector<double> out ((size_t) n);
        for (int i = 0; i < n; i += 256)
        {
            int nUp = 0;
            double* up = os.upsample (in.data() + i, 256, nUp);
            os.downsample (up, nUp, out.data() + i, 256);
        }
        // discard the startup transient, compare steady-state magnitude
        std::vector<double> outSS (out.begin() + 2048, out.end());
        std::vector<double> inSS  (in.begin()  + 2048, in.end());
        const double gain = goertzelMag (outSS, fs, tf) / goertzelMag (inSS, fs, tf);
        std::printf ("  passband gain (1 kHz) = %.4f\n", gain);
        R.check ("up/down reconstructs the passband (unity gain)",
                 std::abs (gain - 1.0) < 0.03,
                 "gain=" + std::to_string (gain));
    }

    //--------------------------------------------------------------------------
    R.section ("8. Auto-Trim loudness match");
    {
        Engine e; e.prepare (fs, 512, 1);
        EngineParameters p;
        p.core.coreIndex = 3; p.core.driveDb = 18.0; p.core.coreSat = 0.7;
        p.autoTrim = true; p.oversample = OversampleMode::x2; p.mix = 1.0;
        e.setParameters (p);

        auto in = makeSine (fs, 220.0, 0.25, 48000); // -12 dBFS
        std::vector<double> buf = in;
        for (int i = 0; i < 48000; i += 512)
        {
            const int nb = std::min (512, 48000 - i);
            double* b[1] = { buf.data() + i };
            e.process (b, 1, nb);
        }
        // measure RMS over the settled tail
        std::vector<double> tail (buf.end() - 16000, buf.end());
        std::vector<double> inTail (in.end() - 16000, in.end());
        const double rOut = rms (tail), rIn = rms (inTail);
        const double dbDiff = 20.0 * std::log10 ((rOut + 1e-12) / (rIn + 1e-12));
        std::printf ("  output vs input level after auto-trim = %+.2f dB\n", dbDiff);
        R.check ("auto-trim matches loudness within 3 dB", std::abs (dbDiff) < 3.0);
    }

    //--------------------------------------------------------------------------
    R.section ("9. Dry/Wet + PDC: mix=0 returns input delayed by exactly latency");
    {
        Engine e; e.prepare (fs, 512, 1);
        EngineParameters p;
        p.core.coreIndex = 0; p.core.driveDb = 12.0;
        p.oversample = OversampleMode::x4; p.mix = 0.0; // fully dry
        e.setParameters (p);
        const int lat = e.latencySamples();

        const int n = 8192;
        std::vector<double> in ((size_t) n), buf ((size_t) n);
        for (int i = 0; i < n; ++i)
            in[(size_t) i] = std::sin (0.11 * i) * 0.4 + std::sin (0.017 * i) * 0.3;
        buf = in;
        for (int i = 0; i < n; i += 512)
        {
            const int nb = std::min (512, n - i);
            double* b[1] = { buf.data() + i };
            e.process (b, 1, nb);
        }
        double err = 0.0, ref = 0.0;
        for (int i = lat + 10; i < n; ++i)
        {
            const double d = buf[(size_t) i] - in[(size_t) (i - lat)];
            err += d * d; ref += in[(size_t) (i - lat)] * in[(size_t) (i - lat)];
        }
        const double nrmse = std::sqrt (err / (ref + 1e-20));
        std::printf ("  dry-path NRMSE vs input delayed by %d = %.6f\n", lat, nrmse);
        R.check ("dry path is bit-aligned (PDC correct)", nrmse < 1e-6);
    }

    //--------------------------------------------------------------------------
    R.section ("10. Full-engine stability, all cores, +24 dB, stereo");
    {
        bool ok = true;
        for (int ci = 0; ci < kNumCores; ++ci)
        {
            Engine e; e.prepare (fs, 256, 2);
            EngineParameters p;
            p.core.coreIndex = ci; p.core.driveDb = 24.0; p.core.coreSat = 1.0;
            p.core.hysteresis = 1.0; p.core.lfSat = 1.0; p.humDepth = 0.3;
            p.oversample = OversampleMode::automatic; p.quality = QualityMode::insane;
            e.setParameters (p);

            const int n = 8192;
            std::vector<double> l ((size_t) n), r ((size_t) n);
            for (int i = 0; i < n; ++i)
            {
                const double s = std::sin (0.07 * i) + 0.5 * std::sin (0.31 * i);
                l[(size_t) i] = s; r[(size_t) i] = 0.9 * s;
            }
            for (int i = 0; i < n; i += 256)
            {
                const int nb = std::min (256, n - i);
                double* b[2] = { l.data() + i, r.data() + i };
                e.process (b, 2, nb);
            }
            if (! allFinite (l) || ! allFinite (r) || peak (l) > 8.0 || peak (r) > 8.0)
            { ok = false; std::printf ("  core %d unstable (peak %.2f)\n", ci, peak (l)); }
        }
        R.check ("every core stable at extreme settings", ok);
    }

    return R.summarise();
}
