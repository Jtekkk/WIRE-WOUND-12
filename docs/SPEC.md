# FLUXCORE·12 — Product Specification

**Product:** FLUXCORE·12 — Transformer Core Modeler
**Vendor:** TEKK Engineering Audio Labs
**Category:** Fx / Distortion (saturation, coloration, transformer emulation)
**Status:** DSP engine complete and unit-tested (16/16 native checks); JUCE VST3 +
Standalone wrapper with a resizable vector UI.

---

## 1. Concept

FLUXCORE·12 is a colored line- / coupling-transformer emulator. Rather than layering
static EQ and a fixed waveshaper, the user selects a **core metallurgy** and drives
real magnetic flux into it. All coloration — harmonic generation, low-frequency
saturation, high-frequency winding resonance, spectral tilt — emerges from the
underlying physics.

Twelve cores share one identical signal path; only the material coefficients change.
This delivers twelve distinct "units" for roughly the DSP cost of one, and guarantees
consistent behaviour and metering across all cores.

---

## 2. The 12 cores

Each core is defined by a Jiles–Atherton coefficient set (`Ms, a, k, c, α`) plus
audio-domain macro descriptors (saturation knee, even-harmonic seed, eddy corner,
spectral tilt, winding resonance, LF behaviour, per-core makeup). `Bs` is the
saturation flux density used for display and drive scaling. Values are tuned for
musical behaviour, not strict SI accuracy.

| # | Core | Bs (T) | Family | Voice |
|---|------|:------:|--------|-------|
| 01 | Grain-Oriented Silicon Steel (GOSS) | 2.00 | Steel | Big-iron console/output weight; the reference |
| 02 | High-Nickel 80% Permalloy (80Ni) | 0.75 | Nickel | Pristine, airy, near-linear |
| 03 | Laminated Iron (IRON) | 1.80 | Iron | Gritty, dark, vintage lo-fi |
| 04 | Cobalt-Iron / Permendur (COFE) | 2.35 | Cobalt | Highest-flux aggressive slam |
| 05 | Amorphous Co-based Metglas (AMOR) | 0.55 | Amorphous | Ultra-clean, low-loss |
| 06 | Nanocrystalline / Finemet (NANO) | 1.20 | Nanocrystalline | Transparent mastering glue |
| 07 | Ferrite MnZn (FERR) | 0.45 | Ferrite | Bright, HF-forward, broadcast-y |
| 08 | Carbonyl Powdered Iron (CARB) | 1.00 | Powdered iron | Soft, round, tape-like |
| 09 | Supermalloy 79Ni-5Mo (SUPM) | 0.70 | Nickel | The cleanest core; near-invisible |
| 10 | Mu-Metal (MUMT) | 0.65 | Nickel | Shielding-alloy smoothness |
| 11 | Non-Oriented Steel M19 (M19) | 1.90 | Steel | Mid-forward "honk"; motor-lamination steel |
| 12 | Nickel/Steel Hybrid Stack (HYBD) | 1.20 | Hybrid | Split-band steel-lows / nickel-highs; morphable — default |

**Harmonic behaviour.** Odd harmonics scale with drive and knee sharpness (Core Sat);
even harmonics come from transfer-curve asymmetry — each core's intrinsic even seed
plus the Bias control. The clean set (Permalloy, Amorphous, Nanocrystalline,
Supermalloy) is measurably lower-THD than the colour set (GOSS, Iron, Cobalt-Iron,
M19). Core 12 crosses over (≈500–900 Hz, moved by Core Morph) into a steel-weighted
low saturator and a nickel-clarity high saturator.

---

## 3. Signal architecture

Per channel ("lane"):

```
input trim
  → oversample ↑
      → source-Z network      (LF high-pass; tightness)
      → drive into field H    (H = source-shaped signal · drive gain + optional hum)
      → anhysteretic saturation   (ADAA algebraic sigmoid)
      → + Jiles–Atherton lag       (magnetic memory / "smear")
      → frequency-dependent core loss  (eddy LP + tilt shelf + presence peak)
      → + parallel LF saturation        (flux-dependent low-end)
      → winding + load-Z network        (interwinding resonance + brightness)
      → DC block → per-core makeup
  → oversample ↓
  → auto-trim → dry/wet (PDC-aligned) → output trim
```

- **Stereo** and **Dual-Mono** process L/R directly; **Mid-Side** encodes
  `M = ½(L+R)`, `S = ½(L−R)`, processes each independently (with optional extra Side
  drive), and reconstructs.
- The dry path is delay-compensated by exactly the reported latency, keeping dry/wet
  phase-coherent.
- Changing core crossfades between the old and new core (~1024 samples) for click-free
  switching.

---

## 4. DSP engine

- **Jiles–Atherton hysteresis** — genuine B–H loop; effective field `He = H + αM`,
  Langevin anhysteretic curve, irreversible/reversible split with δ_M gating and
  clamped denominators for audio-rate stability. The lag `M − M_an` is the magnetic
  memory, scaled by the Hysteresis control.
- **Anti-derivative anti-aliasing (ADAA)** — 1st- and 2nd-order on an algebraic-sigmoid
  saturator `f(u) = u/√(1+u²)`, chosen for its closed-form first and second
  antiderivatives; includes the standard ill-conditioned divided-difference fallbacks.
- **Oversampling** — Off / 2× / 4× / 8× / 16× / Auto, via a cascade of 2× Kaiser
  half-band FIR stages. Latency is exactly `(L−1)·(1 − 1/N)` and is reported to the
  host for PDC.
- **Frequency-dependent core loss** — eddy-current low-pass at the material corner,
  broadband high-shelf tilt, and an optional presence/"honk" mid peak.
- **Bilinear R/L/C impedance networks** — source-Z LF rolloff, core bass extension,
  interwinding resonance, and load-Z brightness; corners and peaks track the knobs.
- **Separate flux-dependent LF saturation** — parallel low-band ADAA path that adds
  weight on sustained low end.
- **Component aging** — the Age control drifts loss corners down, adds HF loss, and
  detunes the winding resonance.
- **Optional mains hum** — 50/60 Hz bleed (with a little 2nd/3rd) injected into the
  field so it passes through the nonlinearity.
- **64-bit float** internal processing throughout, with denormal flushing.
- **Loudness-matched Auto-Trim** for honest bypass A/B.
- **Quality modes** — Eco (ADAA 1st order, 31-tap AA, ≤2× OS), Standard (ADAA 2nd
  order, 63-tap, ≤8× OS), Insane (ADAA 2nd order, 127-tap, ≤16× OS).

---

## 5. Controls

### Main
| Control | Range / values | Default | Function |
|---------|----------------|:-------:|----------|
| Core | 12 materials | 12 (Hybrid) | Selects the modelled core |
| Core Morph | 0 – 1 | 0.5 | A/B morph for the hybrid core (crossover + low/high knees) |
| Input | −24…+24 dB | 0 | Input trim |
| Drive | −24…+24 dB | 0 | Flux drive into the magnetic field |
| Bias | −1…+1 | 0 | Transfer-curve asymmetry → even harmonics |
| Core Sat | 0 – 1 | 0.35 | Saturation intensity / knee → odd harmonics |
| Hysteresis | 0 – 1 | 0.3 | Magnetic-memory (JA lag) amount |
| Output | −24…+24 dB | 0 | Output trim |
| Mix | 0 – 1 | 1.0 | Dry/wet (PDC-aligned) |

### Impedance / Tone
| Control | Range | Default | Function |
|---------|-------|:-------:|----------|
| Source Z | 0 – 1 | 0.4 | Driving-stage output impedance → LF tightness |
| Load Z | 0 – 1 | 0.5 | Termination impedance → brightness + resonance height |
| Winding Res | 0 – 1 | 0.35 | Interwinding resonance frequency + height |
| LF Sat | 0 – 1 | 0.4 | Low-frequency saturation depth |

### Character / Utility
| Control | Range / values | Default | Function |
|---------|----------------|:-------:|----------|
| Age | 0 – 1 | 0.15 | Component tolerance drift (darker, detuned resonance) |
| Hum | 0 – 1 | 0.0 | Mains bleed depth |
| Hum Freq | 50 / 60 Hz | 50 Hz | Mains frequency |
| Side Drive | −12…+12 dB | 0 | Extra Side-lane drive (Mid-Side) |
| Stereo Mode | Stereo / Dual-Mono / Mid-Side | Stereo | Channel routing |
| Auto-Trim | on / off | off | Loudness-matched output |
| Oversample | Off / 2× / 4× / 8× / 16× / Auto | Auto | Oversampling factor |
| Quality | Eco / Standard / Insane | Standard | ADAA order + AA filter length + OS cap |
| Bypass | on / off | off | Hard bypass |

All parameters are host-automatable via a single APVTS layout shared with the DSP
engine, so host state and processing can never disagree.

---

## 6. Metering & visualization

The resizable vector UI is data-driven from JUCE-free analysis probes that measure the
*current* control values, so the displays always reflect the settings that produced
them:

- **Harmonic Analyzer** — live H2–H9 bar graph with a total-THD readout; shows the
  odd/even balance the Bias and Core Sat controls create.
- **Flux Meter (B–H loop)** — the modelled hysteresis loop, with a live
  operating-point dot riding the curve from the processor's published field /
  magnetisation.
- **Response Curve** — the transformer's linear tone-network magnitude vs frequency,
  redrawing as Core / Source Z / Load Z / Winding Res change — proof the tone comes
  from the impedance network, not a fixed EQ.
- **Level Meters** — input and output levels, plus the Auto-Trim gain.

---

## 7. Signature Chains (factory presets)

Eight curated presets plus an **Init** starting point:

| Preset | Core | Category | Description |
|--------|------|----------|-------------|
| Bus Glue | 12 — Hybrid Stack | Bus | Gentle mix-bus glue |
| Neve-ish Weight | 01 — GOSS | Bus | Big console low-end weight |
| Drum Slam | 04 — Cobalt-Iron | Drums | Aggressive high-flux drum saturation |
| Vocal Air | 02 — 80% Permalloy | Vox | Clean, airy vocal top |
| Tape-ish Round | 08 — Carbonyl Iron | Bus | Soft, tape-like rounding |
| Mastering Sheen | 06 — Nanocrystalline | Master | Transparent master-bus sheen (Auto-Trim) |
| Lo-Fi Iron | 03 — Laminated Iron | Weird | Gritty, worn, aged iron |
| Broken Broadcast | 07 — Ferrite | Weird | Hummy, resonant broken-radio |

User banks save/restore the full parameter state as XML.

---

## 8. I/O & formats

- **Plug-in formats:** VST3, Standalone. (AU and AAX planned.)
- **Channel configurations:** mono → mono, stereo → stereo (input and output layouts
  must match).
- **Sample rates:** 44.1 kHz – 192 kHz.
- **Internal precision:** 64-bit float end to end.
- **Latency:** reported to the host for full plug-in delay compensation; equals the
  oversampler round-trip delay `(L−1)·(1 − 1/N)` for the active factor and quality.

---

## 9. System requirements

- **Windows:** 64-bit (x64). Distributed as `FLUXCORE-12-Installer.exe` (Inno Setup),
  built by CI on `v*` tags; installs the VST3 to the common VST3 folder and the
  Standalone app to Program Files.
- **A VST3-compatible host** (DAW) for the plug-in, or run the Standalone directly.
- **Build from source:** CMake ≥ 3.22 and a C++17 toolchain. JUCE 8.0.4 is fetched
  automatically via `FetchContent` (or use a local `./JUCE` checkout). The native
  DSP test target has no JUCE dependency.

---

## 10. Verification

The engine ships with a native, JUCE-free test suite (`tests/test_main.cpp`, **16/16**
checks passing) covering: per-core harmonic profiling and clean-vs-colour ordering,
Bias → even harmonics, Drive → more saturation, ADAA/oversampling anti-aliasing, the
impedance-network frequency response, the Jiles–Atherton B–H loop area (magnetic
memory), oversampler reconstruction and the exact latency formula, Auto-Trim loudness
match, bit-exact PDC, and full-engine stability across all twelve cores at extreme
drive.

---

*FLUXCORE·12 — TEKK Engineering Audio Labs. Specification revision matches source
version 0.1.0.*
