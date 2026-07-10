# FLUXCORE·12 — DSP Deep-Dive

This document describes the physics and signal flow *as implemented*. Every claim
maps to a source file under `source/dsp/`. The whole engine is JUCE-free and
header-only, so each subsystem is exercised directly by `tests/test_main.cpp`.

---

## 1. Signal architecture

One channel ("lane") is `TransformerCore` (`TransformerCore.h`); the top-level
`Engine` (`Engine.h`) owns two lanes plus oversampling, routing, PDC, metering, and
loudness-matched trim.

The full base-rate chain, per lane, is:

```
input trim (Engine)
  → oversample ↑                         (Oversampler.h)
      → source-Z network (pre)           (ImpedanceNetwork.processPre)
      → H = x·driveGain + hum            (drive into the magnetic field)
      → anhysteretic saturation (ADAA)   (Saturator.h via ADAA.h)
      → + hysteresis lag  (JA memory)    (JilesAtherton.h)
      → frequency-dependent core loss    (FrequencyLoss.h)
      → + parallel LF saturation         (LFSaturation.h)
      → winding + load-Z network (post)  (ImpedanceNetwork.processPost)
      → DC block → per-core makeup       (Filters.h DCBlocker)
  → oversample ↓                         (Oversampler.h)
  → auto-trim → dry/wet (PDC) → output trim   (Engine)
```

Key ordering facts, straight from `TransformerCore::processSample`:

- The **source-Z** network runs *before* the drive gain, so it shapes what the core
  actually sees (`xs = net.processPre(x)`, then `H = xs·driveGain + humField`).
- The **same** field `H` feeds both the memoryless saturator and the Jiles–Atherton
  model; the JA *lag* is added on top of the saturated flux — it does not replace the
  saturation shape.
- **LF saturation** is a *parallel* injection driven from `xs` (the pre-drive,
  source-shaped signal), summed into the flux after the core loss.
- The **winding + load-Z** network is post-core, then a DC blocker removes the bias
  offset, then a per-core makeup gain is applied.

`Engine::process` wraps this with:

- **Routing.** Stereo / Dual-Mono use the raw L/R lanes; Mid-Side forms
  `M = ½(L+R)`, `S = ½(L−R)`, runs each lane independently (with optional extra
  **Side Drive**), then reconstructs `L = M+S`, `R = M−S`.
- **Oversampling** up before the lane and down after.
- **PDC.** The dry signal is pushed into a delay line and read back exactly
  `latencySamples()` late, so the dry/wet mix stays phase-coherent and the host is
  told the latency.
- **Auto-Trim.** Running dry/wet RMS envelopes drive a slewed makeup gain that
  loudness-matches the wet path (bounded to ±12 dB, i.e. gain ∈ [0.25, 4]).
- **Crossfaded core switching.** On a core change the incoming core is reset and
  faded in over ~1024 oversampled samples; two cores run per lane during the fade.

---

## 2. Jiles–Atherton hysteresis (`JilesAtherton.h`)

A genuine Jiles–Atherton model traces a real B–H loop (drawn by the Flux Meter) and,
more importantly for the sound, exposes the *lag* between the magnetisation `M` and
its anhysteretic value `M_an`. That lag is the physical magnetic memory.

**Effective field** (inter-domain mean-field coupling `α`):

```
He = H + α·M
```

**Anhysteretic magnetisation** via the Langevin function `L(z) = coth(z) − 1/z`:

```
M_an = Ms · L(He / a)
```

Both `L(z)` and its derivative use a series expansion near the origin for numerical
safety:

```
L(z)  ≈ z/3 − z³/45           (|z| < 1e-4)
L'(z) ≈ 1/3 − z²/15           (|z| < 1e-4)
```

**Irreversible / reversible split.** With `δ = sign(dH)` and
`dM_an/dHe = (Ms/a)·L'(He/a)`:

```
dM_irr/dH = (M_an − M) / ( k·δ − α·(M_an − M) )

dM/dH = [ (1−c)·dM_irr/dH + c·dM_an/dHe ]
        ────────────────────────────────────────────────
        [ 1 − α·c·dM_an/dHe − α·(1−c)·dM_irr/dH ]
```

`Ms, a, k, c, α` come from the selected `CoreMaterial`:
- `Ms` — saturation magnetisation,
- `a` — Langevin shape / domain-wall density (larger ⇒ later, softer),
- `k` — pinning (loop width / coercivity / loss),
- `c` — reversibility 0…1 (larger ⇒ cleaner, less memory),
- `α` — inter-domain coupling.

**Guarded forward integration.** `M` is advanced by `dM_dH · dH`. Two guards are
required for stability at audio rates and are both present in `process()`:

1. **Clamped denominators** — the irreversible denominator is held ≥ `1e-9` in
   magnitude and the total denominator ≥ `1e-6`, preserving sign.
2. **δ_M gate** — if the irreversible term would push magnetisation *against* the
   field change (`(M_an − M)·δ < 0`), `dM_irr/dH` is forced to zero. Finally `M` is
   flushed of denormals and clamped to `±1.5·Ms`.

**The memory term.** `lag() = M − M_an` is what the core adds:

```
flux += hystAmt · ja.lag()          // hystAmt = Hysteresis · 0.8
```

so **Hysteresis** scales the smear without altering the saturation curve itself.
`getM()` and `getField` (`H`) are tapped for the B–H meter.

*Verification:* test §6 drives a 50 Hz tone and computes the shoelace area of the
`(H, M)` trajectory over one settled period. Lossy iron (high `k`) shows a
non-trivial loop area, and a larger one than whisper-clean supermalloy (tiny `k`).

---

## 3. Antiderivative anti-aliasing (`ADAA.h`, `Saturator.h`)

The memoryless nonlinearity generates the bulk of the harmonics, so it is the element
that most needs anti-aliasing. FLUXCORE uses ADAA (Parker, Zavalishin & Le Bivic,
DAFx-16).

### Why the algebraic sigmoid

The waveshaper is the **algebraic sigmoid**

```
f(u) = u / √(1 + u²)
```

chosen over `tanh` because it has closed-form first *and* second antiderivatives —
which is exactly what clean 2nd-order ADAA needs:

```
f (u) = u (1 + u²)^(−1/2)
F1(u) = √(1 + u²)
F2(u) = ½ ( u·√(1 + u²) + asinh(u) )
```

A **knee gain** `g` sets how hard the curve bends while keeping unit slope at the
origin, and a **DC bias** `b` introduces the asymmetry that produces even harmonics,
with the operating point removed so the curve still passes through the origin:

```
s(x) = (1/g)·f(g·x)          s'(0) = 1
N(x) = s(x + b) − s(b)
```

The scaled antiderivatives carried into ADAA (`AlgebraicSaturator`) are:

```
S1(x) = √(1 + (g x)²) / g²
S2(x) = ½ ( (g x)·√(1 + (g x)²) + asinh(g x) ) / g³

f0(x) = s(x+b) − s(b)
f1(x) = S1(x+b) − s(b)·x
f2(x) = S2(x+b) − s(b)·½x²
```

(constants of integration are irrelevant — every use is a difference).

### The divided-difference formulas

**1st order** (half-sample group delay). With `x0`, previous `x1`:

```
y = ( F1(x0) − F1(x1) ) / (x0 − x1)                    if |x0 − x1| ≥ ε
y = f0( ½(x0 + x1) )                                    (ill-conditioned fallback)
```

**2nd order** (one-sample group delay). With the first divided difference
`D(a,b) = (F2(a) − F2(b))/(a − b)` (itself falling back to `F1(½(a+b))` when
`|a−b| < ε`):

```
y = (2 / (x0 − x2)) · ( D(x0,x1) − D(x1,x2) )           if |x0 − x2| ≥ ε
```

When the **outer** difference `x0 − x2` is ill-conditioned, a Taylor-style fallback
around `x1` is used with `x̄ = ½(x0 + x2)`, `δ = x̄ − x1`:

```
y = (2/δ) · ( F1(x̄) + (F2(x1) − F2(x̄))/δ )             if |δ| ≥ ε
y = f0( ½(x̄ + x1) )                                     if |δ| < ε (fully degenerate)
```

`ε = 1e-6`. Antiderivative history (`f1(x1)`, `f2(x1)`, `f2(x2)`) is cached and
`refreshCache()` is called after the shaper's knee/bias change so the caches stay
consistent.

### Cost

Anti-aliasing costs group delay — half a sample (1st order) and one sample (2nd
order). This is folded into the reported PDC latency. Quality mode selects the order:
**Eco → 1st order**, **Standard / Insane → 2nd order**.

*Verification:* test §4 pushes a hot 5 kHz tone through laminated iron at 44.1 kHz
and measures the alias ratio. 2nd-order ADAA aliases less than 1st-order, and adding
oversampling reduces it further. (Hysteresis and LF-sat paths are zeroed in that test
because only the anhysteretic saturator is ADAA'd.)

---

## 4. Oversampling (`Oversampler.h`)

A self-contained integer oversampler for 2× / 4× / 8× / 16×, built from a cascade of
2× **half-band** stages. Each stage is a windowed-sinc (Kaiser, `β = 8`,
`fc = 0.25`) linear-phase FIR, used both for anti-imaging on the way up and
anti-aliasing on the way down. Upsampling zero-stuffs and low-passes with a ×2 gain to
compensate the inserted zero; downsampling low-passes then decimates by 2.

### Exact latency

For an `L`-tap stage filter over an `N`× cascade the round-trip group delay in
base-rate samples is **exactly**:

```
latency = (L − 1) · (1 − 1/N)
```

`L` (tap count) is set by Quality — **Eco 31, Standard 63, Insane 127** — forced odd
so the linear-phase delay is an integer number of high-rate samples. This is reported
to the host for PDC and matched by the dry-path delay line.

*Verification:* test §7 checks `latencySamples()` against `(L−1)(1−1/N)` exactly, and
confirms the up/down round-trip reconstructs a 1 kHz passband tone at unity gain
(within 3%). Test §9 confirms the whole engine at `mix = 0` returns the input delayed
by *exactly* the reported latency (dry-path NRMSE < 1e-6).

**Auto** (`Engine::resolveFactor`) scales the factor with drive: `>14 dB → 8×`,
`>6 dB → 4×`, else `2×`, then capped by Quality (Eco 2×, Standard 8×, Insane 16×).

---

## 5. Frequency-dependent core loss (`FrequencyLoss.h`)

Eddy-current + hysteresis loss, modelled as frequency-weighted damping — this is what
tilts each core's top end. Three RBJ-cookbook, bilinear-transformed biquads
(`Filters.h`):

- **Eddy low-pass** at `eddyCornerHz` (Q 0.55) — the material's HF loss corner.
- **Broadband tilt** — a high shelf at 8 kHz of `hfTiltDb` (negative = darker).
- **Presence / honk** — a peak at 2.5 kHz (Q 0.9) of `midPresenceDb`, bypassed when
  the material's mid descriptor is ~0. (M19's `+2 dB` mid is the "honk".)

**Age** drifts the corners down (`×(1 − 0.08·age)`) and darkens the tilt
(`−1.5·age dB`), emulating tolerance drift in a used unit.

---

## 6. Impedance networks (`ImpedanceNetwork.h`)

The "secret sauce": bilinear-transformed R/L/C sections whose corners and peaks move
with the knobs, so the resonance and rolloff are a *property of the network*, not a
static shelf. Four biquads:

| Section | Filter | Knob mapping |
|---------|--------|--------------|
| **Source-Z LF rolloff** (`sourceHP`) | high-pass, Q 0.7 | corner `logMap(sourceZ, 8…90 Hz)` — higher Z pushes the corner up (tighter, leaner lows) |
| **Core bass extension** (`lfHP`) | high-pass, Q 0.7 | corner `m.lfCornerHz·(0.8 + 0.4·loadZ)`, clamped 5–300 Hz — a lighter load extends the bass |
| **Interwinding resonance** (`windingPeak`) | peak | freq `m.windingResHz·(0.75 + 0.5·windingRes)`, height `(1.5 + 7·windingRes)·m.windingQ·(0.5 + loadZ)`, Q `0.8 + 2.5·windingRes·(0.6 + 0.6·loadZ)` — knob moves the frequency *and* grows the peak; a lighter load makes it taller and sharper |
| **Load-Z brightness** (`loadShelf`) | high shelf @ 5 kHz | gain `(loadZ − 0.5)·8 dB` — heavy load darkens, light load brightens |

`processPre` applies the source-side sections (source-Z HP → bass extension) *before*
the core; `processPost` applies winding + load *after*. **Age** slightly detunes the
resonance (`freq ×(1 − 0.05·age)`) and lowers its gain/Q.

A pure magnitude query (`magnitudeAt`, product of the four biquad magnitudes) feeds
the Response-Curve UI, so the drawn curve is the exact response the network produces.

*Verification:* test §5 shows the winding-res knob raises the resonance, higher
source-Z reduces 40 Hz energy (tighter lows), and a lighter load brightens 12 kHz.

---

## 7. Low-frequency saturation (`LFSaturation.h`)

Real transformer bass distortion is both level- and frequency-dependent: the core
saturates harder on sustained low end than on transients. FLUXCORE models this as a
**parallel** path:

1. A one-pole low-pass (`180 Hz`) isolates the low band `lo`.
2. `lo` is driven into its own ADAA saturator (fixed knee 1.6, no bias) at
   `lfDrive = 1.5 + 4·lfSatKnob`, then divided back down.
3. Only the *added* harmonic content is returned and summed into the main flux:

```
process(x): lo  = band(x)
            slo = sat(lo · lfDrive) / lfDrive
            return depth · (slo − lo)          depth = clamp(lfSatKnob · m.lfSatDepth)
```

Because it is fed from `xs` (the pre-drive, source-shaped signal) and injected after
the core loss, it adds low-end weight independent of the main saturation stage.

---

## 8. Cores and the split-band hybrid (`CoreMaterial.h`, `TransformerCore.h`)

Each `CoreMaterial` is plain-old-data: the JA coefficients `Ms, a, k, c, α`, plus
audio-domain macros — `Bs`, `perm` (drive→flux gain), `headroom`, `kneeSharp`
(odd-harmonic character), `evenBias` (even seed), the loss/tilt/mid descriptors, the
winding resonance parameters, LF behaviour, and a per-core `makeupDb` trim so the
cores broadly match at unity.

At control rate (`updateControls`):

```
driveGain = dbToGain(driveDb) · perm
knee      = kneeSharp · (0.5 + 1.5·coreSat)          // Core Sat → odd
bias      = clamp(evenBias + 0.6·Bias, ±0.9)         // Bias → even
```

so **odd** harmonics come from drive + knee (Core Sat), and **even** harmonics come
from the total transfer-curve asymmetry (the core's intrinsic `evenBias` plus the
**Bias** knob).

Core 12 (**HYBD**, `splitBand = true`) additionally runs two saturators around a
crossover low-pass whose frequency morphs 500 → 900 Hz with **Core Morph**: steel
weight (harder knee, slight extra even) on the lows, nickel clarity (softer knee, half
the bias) up top. The bands are summed to form the flux.

*Verification:* test §1 confirms the reference/clean set (permalloy, amorphous,
nanocrystalline, supermalloy) has lower average THD than the colour set (GOSS, iron,
cobalt, M19); test §2 confirms Bias raises the 2nd harmonic; test §3 confirms more
drive raises THD.

---

## 9. Supporting primitives

- **`Filters.h`** — transposed-DF-II `Biquad` (RBJ low/high-pass, peak, shelves) with
  an analytic `magnitudeAt`; a one-pole low/high pass (`OnePole`); and a leaky-integrator
  `DCBlocker` (~8 Hz corner) that removes the bias offset at the core output.
- **`Hum.h`** — optional mains bleed: `50/60 Hz` fundamental plus a little 2nd/3rd,
  injected into the field so it passes through the core nonlinearity like real hum.
- **`DspUtils.h`** — `flush` (denormal guard), `clampT`, `lerp`, `logMap` (log-range
  control mapping), `dbToGain`/`gainToDb`, and a numerically-stable `logCosh`.
- **`Analysis.h`** — JUCE-free probes that build a throwaway `TransformerCore` from the
  current controls and measure it, feeding the UI: `computeHarmonics` (Goertzel H1–H9
  → the analyzer), `computeBHLoop` (the flux meter trajectory), and `responseCurveDb`
  (the network magnitude curve). They never touch the live audio-thread engine.

---

## 10. What the tests prove

`tests/test_main.cpp` is JUCE-free and exits 0 iff all **16** checks pass, across ten
sections: (1) per-core harmonic profile + clean-vs-colour THD ordering + stability,
(2) Bias → even harmonics, (3) Drive → more THD, (4) ADAA order and oversampling
reduce aliasing, (5) impedance networks shape the response (winding res, source-Z,
load-Z), (6) the JA B–H loop has real area and iron > supermalloy, (7) oversampler
latency formula + unity-gain passband reconstruction, (8) Auto-Trim loudness match
within 3 dB, (9) dry/wet + PDC bit-alignment, (10) full-engine stability across all
twelve cores at +24 dB, Insane quality, stereo.
