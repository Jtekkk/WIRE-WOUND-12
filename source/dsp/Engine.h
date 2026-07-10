/*
    FLUXCORE·12 — Engine.h

    The top-level, JUCE-free processing engine. It owns the per-lane transformer
    cores and wraps them in everything the signal-flow spec asks for:

        input trim -> oversample up -> CORE -> oversample down/ADAA
                   -> auto-trim -> dry/wet -> output trim

    Responsibilities:
      • Stereo / Dual-Mono / Mid-Side routing (independent M and S drive in MS).
      • Off/2×/4×/8×/16×/Auto oversampling; Eco/Standard/Insane quality (ADAA
        order + filter length).
      • PDC: the dry path is delayed to match the oversampler's latency so the
        dry/wet mix stays phase-coherent; the latency is reported to the host.
      • Loudness-matched Auto-Trim.
      • Click-free, crossfaded core switching (two cores per lane during a fade).
      • Meter/analysis taps (B–H field & magnetisation, I/O levels).

    Operates on double* channel buffers so it can be unit-tested directly.
*/

#pragma once

#include <vector>
#include <array>
#include "TransformerCore.h"
#include "Oversampler.h"
#include "Hum.h"

namespace fluxcore
{
//==============================================================================
struct EngineParameters
{
    CoreControls   core;                 // shared core controls
    StereoMode     stereoMode = StereoMode::stereo;
    OversampleMode oversample = OversampleMode::automatic;
    QualityMode    quality    = QualityMode::standard;

    double inputDb     = 0.0;
    double outputDb    = 0.0;
    double mix         = 1.0;            // 0..1
    double sideDriveDb = 0.0;           // MS: extra drive on the Side lane
    double humDepth    = 0.0;
    double humFreq     = 50.0;
    bool   autoTrim    = false;
    bool   bypass      = false;
};

//==============================================================================
class Engine
{
public:
    //== lifecycle ============================================================
    void prepare (double sampleRate, int maxBlockSize, int numChannels)
    {
        fs       = sampleRate;
        maxBlock = std::max (16, maxBlockSize);
        numCh    = clampT (numChannels, 1, 2);

        adaaOrder     = 2;
        currentFactor = 2;
        currentTaps   = 63;

        for (auto& lane : lanes)
            lane.prepare (fs, adaaOrder, kMaxFactor, kMaxTaps, maxBlock, currentFactor);

        for (auto& d : dryDelay) d.assign (kDelayLen, 0.0);
        for (auto& v : laneBuf)  v.assign ((size_t) maxBlock, 0.0);

        lastCoreIndex = params.core.coreIndex;
        reset();
    }

    void reset()
    {
        for (auto& lane : lanes) lane.reset();
        for (auto& d : dryDelay) std::fill (d.begin(), d.end(), 0.0);
        dryWrite = 0;
        trimGain = 1.0; dryEnv = wetEnv = 1.0e-6;
        inLevel = outLevel = 0.0;
    }

    int latencySamples() const noexcept { return lanes[0].os.latencySamples(); }

    //== parameters ===========================================================
    void setParameters (const EngineParameters& p)
    {
        const bool coreChanged = (p.core.coreIndex != lastCoreIndex);

        const int order  = (p.quality == QualityMode::eco) ? 1 : 2;
        const int taps   = (p.quality == QualityMode::eco)      ? 31
                         : (p.quality == QualityMode::standard) ? 63 : 127;
        const int factor = resolveFactor (p);

        if (factor != currentFactor || taps != currentTaps || order != adaaOrder)
        {
            adaaOrder     = order;
            currentFactor = factor;
            currentTaps   = taps;
            for (auto& lane : lanes)
                lane.reconfigure (fs, order, factor, taps, maxBlock);
        }

        for (auto& lane : lanes)
        {
            lane.hum.setFreq  (fs * currentFactor, p.humFreq);
            lane.hum.setDepth (p.humDepth);
        }

        CoreControls cc = p.core;
        cc.adaaOrder = adaaOrder;

        if (p.stereoMode == StereoMode::midSide)
        {
            lanes[0].setControls (cc, coreChanged);          // Mid
            CoreControls sc = cc;
            sc.driveDb += p.sideDriveDb;
            lanes[1].setControls (sc, coreChanged);          // Side
        }
        else
        {
            lanes[0].setControls (cc, coreChanged);
            lanes[1].setControls (cc, coreChanged);
        }

        params        = p;
        lastCoreIndex = p.core.coreIndex;
        inGain  = dbToGain (p.inputDb);
        outGain = dbToGain (p.outputDb);
    }

    //== audio ================================================================
    void process (double* const* ch, int nCh, int n)
    {
        nCh = clampT (nCh, 1, 2);
        if (params.bypass) return;

        const bool ms = (params.stereoMode == StereoMode::midSide) && (nCh == 2);

        for (auto& v : laneBuf) if ((int) v.size() < n) v.assign ((size_t) n, 0.0);

        // capture input peak
        double inPeak = 0.0;
        for (int c = 0; c < nCh; ++c)
            for (int i = 0; i < n; ++i)
                inPeak = std::max (inPeak, std::abs (ch[c][i]));
        inLevel = inPeak;

        // --- form lane buffers ---------------------------------------------
        if (ms)
        {
            for (int i = 0; i < n; ++i)
            {
                const double L = ch[0][i] * inGain;
                const double R = ch[1][i] * inGain;
                laneBuf[0][(size_t) i] = 0.5 * (L + R); // Mid
                laneBuf[1][(size_t) i] = 0.5 * (L - R); // Side
            }
        }
        else
        {
            for (int c = 0; c < nCh; ++c)
                for (int i = 0; i < n; ++i)
                    laneBuf[c][(size_t) i] = ch[c][i] * inGain;
        }

        const int lanesToRun = ms ? 2 : nCh;
        for (int l = 0; l < lanesToRun; ++l)
            lanes[l].processBlock (laneBuf[l].data(), n);

        // --- reconstruct, PDC-align dry, mix, auto-trim, output ------------
        const int lat = latencySamples();
        double outPeak = 0.0;

        for (int i = 0; i < n; ++i)
        {
            double wet[2] = { 0.0, 0.0 };
            if (ms)
            {
                const double M = laneBuf[0][(size_t) i];
                const double S = laneBuf[1][(size_t) i];
                wet[0] = M + S;   // L
                wet[1] = M - S;   // R
            }
            else
            {
                wet[0] = laneBuf[0][(size_t) i];
                wet[1] = (nCh == 2) ? laneBuf[1][(size_t) i] : laneBuf[0][(size_t) i];
            }

            // stash the (undelayed) original dry, then read it back `lat` late
            for (int c = 0; c < nCh; ++c)
                dryDelay[(size_t) c][(size_t) dryWrite] = ch[c][i];
            const int rdIdx = ((dryWrite - lat) % kDelayLen + kDelayLen) % kDelayLen;

            double dryMS = 0.0, wetMS = 0.0;
            for (int c = 0; c < nCh; ++c)
            {
                const double dry = dryDelay[(size_t) c][(size_t) rdIdx];
                dryMS += dry * dry;
                wetMS += wet[c] * wet[c];
            }
            dryEnv += kEnvA * (dryMS - dryEnv);
            wetEnv += kEnvA * (wetMS - wetEnv);

            const double target = params.autoTrim
                ? clampT (std::sqrt ((dryEnv + 1.0e-12) / (wetEnv + 1.0e-12)), 0.25, 4.0)
                : 1.0;
            trimGain += 0.001 * (target - trimGain);

            for (int c = 0; c < nCh; ++c)
            {
                const double dry = dryDelay[(size_t) c][(size_t) rdIdx];
                const double y = lerp (dry, wet[c] * trimGain, params.mix) * outGain;
                ch[c][i] = y;
                outPeak = std::max (outPeak, std::abs (y));
            }

            dryWrite = (dryWrite + 1) % kDelayLen;
        }
        outLevel = outPeak;
    }

    //== meters / analysis ====================================================
    double meterField()         const noexcept { return lanes[0].meterField(); }
    double meterMagnetisation() const noexcept { return lanes[0].meterMagnetisation(); }
    double inputLevel()         const noexcept { return inLevel; }
    double outputLevel()        const noexcept { return outLevel; }
    double autoTrimGain()       const noexcept { return trimGain; }
    double responseMagnitude (double f) const noexcept { return lanes[0].responseMagnitude (f); }
    const CoreMaterial* material() const noexcept { return lanes[0].material(); }

private:
    //== per-lane processing ==================================================
    struct Lane
    {
        TransformerCore coreA, coreB;
        Oversampler     os;
        Hum             hum;
        bool   activeA = true;
        double fadePos = 1.0;      // 1 == not fading
        double fadeInc = 0.0;
        CoreControls curC;

        void prepare (double fs, int order, int maxFactor, int maxTaps,
                      int maxBlock, int factor)
        {
            coreA.prepare (fs, order);
            coreB.prepare (fs, order);
            os.prepare (maxFactor, maxTaps, maxBlock); // reserve capacity at 16×
            os.prepare (factor,    maxTaps, maxBlock); // set current factor
            hum.prepare (fs * factor, 50.0);
            activeA = true; fadePos = 1.0;
        }

        void reconfigure (double fs, int order, int factor, int taps, int maxBlock)
        {
            os.prepare (factor, taps, maxBlock);
            coreA.prepare (fs, order);
            coreB.prepare (fs, order);
            coreA.updateControls (curC);
            coreB.updateControls (curC);
            activeA = true; fadePos = 1.0;
        }

        void reset()
        {
            coreA.reset(); coreB.reset(); os.reset(); hum.reset();
            activeA = true; fadePos = 1.0;
        }

        void setControls (const CoreControls& c, bool coreChanged)
        {
            TransformerCore& active   = activeA ? coreA : coreB;
            TransformerCore& incoming = activeA ? coreB : coreA;

            if (coreChanged && fadePos >= 1.0)
            {
                incoming.reset();
                incoming.updateControls (c);
                fadePos = 0.0;
                fadeInc = 1.0 / std::max (1.0, kFadeSamples);
            }
            else if (fadePos < 1.0)
            {
                incoming.updateControls (c);
            }
            else
            {
                active.updateControls (c);
            }
            curC = c;
        }

        void processBlock (double* buf, int n)
        {
            int nUp = 0;
            double* up = os.upsample (buf, n, nUp);

            TransformerCore& active   = activeA ? coreA : coreB;
            TransformerCore& incoming = activeA ? coreB : coreA;

            for (int i = 0; i < nUp; ++i)
            {
                const double humField = hum.tick();
                if (fadePos < 1.0)
                {
                    const double a = active.processSample   (up[i], humField);
                    const double b = incoming.processSample (up[i], humField);
                    up[i] = lerp (a, b, fadePos);
                    fadePos += fadeInc;
                    if (fadePos >= 1.0) { fadePos = 1.0; activeA = !activeA; }
                }
                else
                {
                    up[i] = active.processSample (up[i], humField);
                }
            }
            os.downsample (up, nUp, buf, n);
        }

        double meterField()         const noexcept { return (activeA ? coreA : coreB).meterField(); }
        double meterMagnetisation() const noexcept { return (activeA ? coreA : coreB).meterMagnetisation(); }
        double responseMagnitude (double f) const noexcept { return (activeA ? coreA : coreB).responseMagnitude (f); }
        const CoreMaterial* material() const noexcept { return (activeA ? coreA : coreB).material(); }

        static constexpr double kFadeSamples = 1024.0;
    };

    int resolveFactor (const EngineParameters& p) const
    {
        const int cap = (p.quality == QualityMode::eco)      ? 2
                      : (p.quality == QualityMode::standard) ? 8 : 16;
        int f = 1;
        switch (p.oversample)
        {
            case OversampleMode::off: f = 1;  break;
            case OversampleMode::x2:  f = 2;  break;
            case OversampleMode::x4:  f = 4;  break;
            case OversampleMode::x8:  f = 8;  break;
            case OversampleMode::x16: f = 16; break;
            case OversampleMode::automatic:
            {
                const double d = p.core.driveDb;
                f = (d > 14.0) ? 8 : (d > 6.0) ? 4 : 2;
                break;
            }
        }
        return std::min (f, cap);
    }

    //== state ================================================================
    static constexpr int kMaxFactor = 16;
    static constexpr int kMaxTaps   = 127;
    static constexpr int kDelayLen  = 512;
    static constexpr double kEnvA   = 0.002;

    double fs = 48000.0;
    int    maxBlock = 512, numCh = 2;
    int    adaaOrder = 2, currentFactor = 2, currentTaps = 63;

    EngineParameters params;
    int    lastCoreIndex = 11;

    std::array<Lane, 2> lanes;
    std::array<std::vector<double>, 2> laneBuf;
    std::array<std::vector<double>, 2> dryDelay;
    int dryWrite = 0;

    double inGain = 1.0, outGain = 1.0;
    double trimGain = 1.0, dryEnv = 1.0e-6, wetEnv = 1.0e-6;
    double inLevel = 0.0, outLevel = 0.0;
};

} // namespace fluxcore
