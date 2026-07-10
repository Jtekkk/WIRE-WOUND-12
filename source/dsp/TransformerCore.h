/*
    FLUXCORE·12 — TransformerCore.h

    One channel of the modeller. Ties the physical chain together exactly as the
    signal-flow spec describes:

        source-Z  ->  CORE  (anhysteretic saturation + JA hysteresis memory
                             + frequency-dependent core loss + flux-dependent LF
                             saturation)  ->  winding network  ->  load-Z

    The anhysteretic saturation is anti-aliased (ADAA); the Jiles–Atherton model
    supplies the magnetic-memory "smear" and drives the B–H meter. Core 12 (the
    Nickel/Steel hybrid) additionally runs a split-band saturation — steel weight
    in the lows, nickel clarity up top — blended by the A/B morph control.

    JUCE-free: the entire engine is exercised by the unit tests.
*/

#pragma once

#include "CoreMaterial.h"
#include "Saturator.h"
#include "JilesAtherton.h"
#include "FrequencyLoss.h"
#include "ImpedanceNetwork.h"
#include "LFSaturation.h"

namespace fluxcore
{
//==============================================================================
struct CoreControls
{
    int    coreIndex  = 11;
    double coreMorph  = 0.5;
    double driveDb    = 0.0;
    double bias       = 0.0;
    double coreSat    = 0.35;
    double hysteresis = 0.3;
    double sourceZ    = 0.4;
    double loadZ      = 0.5;
    double windingRes = 0.35;
    double lfSat      = 0.4;
    double age        = 0.15;
    int    adaaOrder  = 2;
};

//==============================================================================
class TransformerCore
{
public:
    void prepare (double sampleRate, int adaaOrder)
    {
        fs = sampleRate;
        sat.prepare (adaaOrder);
        satLow.prepare (adaaOrder);
        satHigh.prepare (adaaOrder);
        lfsat.prepare (adaaOrder);
        dcOut.prepare (fs);
        reset();
    }

    void reset()
    {
        sat.reset(); satLow.reset(); satHigh.reset();
        ja.reset(); loss.reset(); net.reset(); lfsat.reset(); dcOut.reset();
        xLP.reset();
        meterH = meterM = 0.0;
    }

    /** Control-rate update. Call once per block (or per smoothed step). */
    void updateControls (const CoreControls& c)
    {
        mat = &getCore (c.coreIndex);

        driveGain = dbToGain (c.driveDb) * mat->perm;
        makeup    = dbToGain (mat->makeupDb);
        hystAmt   = c.hysteresis * kHystScale;
        splitBand = mat->splitBand;

        const double knee = mat->kneeSharp * (0.5 + 1.5 * c.coreSat);
        const double bias = clampT (mat->evenBias + 0.6 * c.bias, -0.9, 0.9);

        sat.setOrder (c.adaaOrder);
        sat.setShape (knee, bias);

        ja.setMaterial (mat->Ms, mat->a, mat->k, mat->c, mat->alpha);

        loss.update (fs, *mat, c.age);
        net.update  (fs, *mat, c.sourceZ, c.loadZ, c.windingRes, c.age);
        lfsat.setOrder (c.adaaOrder);
        lfsat.update (fs, *mat, c.lfSat);

        if (splitBand)
        {
            const double morph = clampT (c.coreMorph, 0.0, 1.0);
            xLP.setLowpass (fs, lerp (500.0, 900.0, morph), 0.707);
            satLow .setOrder (c.adaaOrder);
            satHigh.setOrder (c.adaaOrder);
            // steel weight down low, nickel clarity up top; morph biases each
            satLow .setShape (lerp (1.9, 1.4, morph) * (0.6 + c.coreSat),
                              clampT (bias + 0.03, -0.9, 0.9));
            satHigh.setShape (lerp (1.1, 0.9, morph) * (0.6 + c.coreSat), bias * 0.5);
        }
    }

    /** Process one sample at the (oversampled) working rate. */
    inline double processSample (double x, double humField) noexcept
    {
        const double xs = net.processPre (x);
        const double h  = xs * driveGain + humField;

        double flux;
        if (splitBand)
        {
            const double lo = xLP.process (h);
            const double hi = h - lo;
            flux = satLow.process (lo) + satHigh.process (hi);
        }
        else
        {
            flux = sat.process (h);
        }

        // magnetic memory from the Jiles–Atherton model
        ja.process (h);
        flux += hystAmt * ja.lag();

        // meter taps (field vs magnetisation -> B–H loop)
        meterH = h;
        meterM = ja.getM();

        // frequency-dependent core loss, then parallel LF saturation
        flux = loss.process (flux);
        flux += lfsat.process (xs);

        // winding + load network, DC block, per-core makeup
        flux = net.processPost (flux);
        flux = dcOut.process (flux) * makeup;
        return flux;
    }

    /** Linear tone-network magnitude (network × loss) for the Response-Curve UI. */
    double responseMagnitude (double f) const noexcept
    {
        return net.magnitudeAt (fs, f) * loss.magnitudeAt (fs, f);
    }

    double meterField()        const noexcept { return meterH; }
    double meterMagnetisation()const noexcept { return meterM; }
    const CoreMaterial* material() const noexcept { return mat; }

private:
    static constexpr double kHystScale = 0.8;

    double fs = 48000.0;
    const CoreMaterial* mat = &getCore (11);

    AASaturator      sat, satLow, satHigh;
    JilesAtherton    ja;
    FrequencyLoss    loss;
    ImpedanceNetwork net;
    LFSaturation     lfsat;
    DCBlocker        dcOut;
    Biquad           xLP;      // split-band crossover (hybrid core)

    double driveGain = 1.0, makeup = 1.0, hystAmt = 0.0;
    bool   splitBand = false;

    double meterH = 0.0, meterM = 0.0;
};

} // namespace fluxcore
