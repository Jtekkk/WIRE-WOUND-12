# FLUXCORE·12

**Transformer Core Modeler — twelve physically-modelled magnetic cores in one plug-in.**

FLUXCORE·12 is a colored line- / coupling-transformer emulator. Instead of stacking
static EQ curves and a fixed waveshaper, you pick a *metallurgy* and drive real
magnetic flux into it. Harmonics, low-frequency saturation, and high-frequency
winding resonance all fall out of the physics: a genuine Jiles–Atherton hysteresis
loop, an anti-aliased anhysteretic saturator, frequency-dependent eddy/hysteresis
loss, and bilinear R/L/C impedance networks whose corners move with the knobs. Only
the material coefficients change between the twelve cores — the signal path is
identical — so you get twelve "units" for roughly the cost of one.

Built by **TEKK Engineering Audio Labs**.

---

## Status

- **DSP engine: implemented and unit-tested.** The native, JUCE-free verification
  suite (`tests/test_main.cpp`) passes **16/16** checks: per-core harmonic
  profiling, BIAS → even harmonics, DRIVE → more saturation, ADAA + oversampling
  anti-aliasing, impedance-network frequency response, the Jiles–Atherton B–H loop
  area (magnetic memory), oversampler passband reconstruction + reported latency,
  loudness-matched Auto-Trim, bit-exact PDC (dry path delayed by exactly the
  reported latency), and full-engine stability across all twelve cores at extreme
  drive.
- **Plug-in wrapper: JUCE VST3 + Standalone.** Resizable vector UI with a live
  harmonic analyzer (H2–H9 + THD), a B–H flux meter with a live operating-point
  dot, a tone-network response curve, and I/O level meters.

The engine is header-only and JUCE-free by design, which is what makes it directly
unit-testable with a bare C++ toolchain.

---

## 🧲 The 12 Cores

Each core is a small coefficient set (Jiles–Atherton `Ms, a, k, c, α` plus audio-domain
macro descriptors) read by the shared DSP path. Values are tuned for musical
behaviour rather than strict SI accuracy. `Bs` is the saturation flux density used for
display and drive scaling.

| # | Alloy / Name | Bs (T) | Harmonic tilt | Character |
|---|--------------|:------:|---------------|-----------|
| 01 | Grain-Oriented Silicon Steel (GOSS) | 2.00 | Odd-dominant, a little even (knee 1.6 / even 0.06), gently dark (−1.5 dB) | Big-iron console/output weight — the reference "large iron" |
| 02 | High-Nickel 80% Permalloy (80Ni) | 0.75 | Very low harmonics, faintly bright (knee 1.0 / even 0.02, +0.8 dB) | Pristine, airy, near-linear — a reference/clean core |
| 03 | Laminated Iron (Standard, IRON) | 1.80 | Rich odd + strong even (knee 2.2 / even 0.10), distinctly dark (−4 dB) | Gritty, lo-fi, vintage iron with a mid bump |
| 04 | Cobalt-Iron / Permendur (COFE) | 2.35 | Hardest knee, aggressive odd (knee 3.0 / even 0.05) | Highest-flux slam; loud transient saturation |
| 05 | Amorphous Co-based Metglas (AMOR) | 0.55 | Minimal harmonics, flat (knee 1.0 / even 0.015) | Ultra-clean, low-loss — a reference/clean core |
| 06 | Nanocrystalline / Finemet (NANO) | 1.20 | Lowest even tendency (knee 1.0 / even 0.010) | Transparent mastering glue — a reference/clean core |
| 07 | Ferrite MnZn (FERR) | 0.45 | Odd-leaning, brightest top (knee 1.8 / even 0.03, +3 dB) | HF-forward, broadcast-y; tightest lows, peakiest winding |
| 08 | Carbonyl Powdered Iron (CARB) | 1.00 | Softest knee, gentle even (knee 0.8 / even 0.05), a touch dark | Round, tape-like softness |
| 09 | Supermalloy 79Ni-5Mo (SUPM) | 0.70 | Whisper-clean, tiniest hysteresis (knee 0.9 / even 0.01, k 0.03) | The cleanest core; near-invisible coloration — reference/clean |
| 10 | Mu-Metal (MUMT) | 0.65 | Low-to-moderate, gentle (knee 1.1 / even 0.03, −1 dB) | Shielding-alloy smoothness |
| 11 | Non-Oriented Steel M19 (M19) | 1.90 | Odd + even with a strong mid honk (knee 2.0 / even 0.07, +2 dB @ 2.5 kHz) | Mid-forward motor-lamination steel |
| 12 | Nickel/Steel Hybrid Stack (HYBD) | 1.20 | Split-band: steel odd in the lows, nickel clarity up top (knee 1.4 / even 0.04) | Best-of-both hybrid, morphable — the default core |

> Core **12 (HYBD)** runs a split-band saturator: a crossover (≈500–900 Hz, moved by
> **Core Morph**) sends steel-weight saturation to the lows and nickel-clarity
> saturation to the highs.

---

## 🎛 Controls

### Main
| Control | Range / values | What it does |
|---------|----------------|--------------|
| **Core** | 12 materials | Selects the modelled core; only the coefficients swap |
| **Core Morph** | 0 – 1 (def 0.5) | A/B morph for the hybrid core — moves the split-band crossover and the low/high knees |
| **Input** | −24…+24 dB | Input trim into the engine |
| **Drive** | −24…+24 dB | FLUX drive — how hard signal is pushed into the magnetic field |
| **Bias** | −1…+1 | DC asymmetry of the transfer curve → **even** harmonics |
| **Core Sat** | 0 – 1 (def 0.35) | Saturation intensity / knee sharpness → **odd** harmonic character |
| **Hysteresis** | 0 – 1 (def 0.3) | Magnetic-memory amount (the Jiles–Atherton lag added as "smear") |
| **Output** | −24…+24 dB | Output trim |
| **Mix** | 0 – 1 (def 1.0) | Dry/wet (dry path is PDC-delayed to stay phase-coherent) |

### Impedance / Tone
| Control | Range | What it does |
|---------|-------|--------------|
| **Source Z** | 0 – 1 (def 0.4) | Driving-stage output impedance → LF high-pass (≈8–90 Hz); higher = tighter, leaner lows |
| **Load Z** | 0 – 1 (def 0.5) | Termination impedance; lighter load = brighter, taller winding resonance |
| **Winding Res** | 0 – 1 (def 0.35) | Interwinding resonance — sets the peak's frequency and height |
| **LF Sat** | 0 – 1 (def 0.4) | Depth of the separate flux-dependent low-frequency saturation |

### Character / Utility
| Control | Range / values | What it does |
|---------|----------------|--------------|
| **Age** | 0 – 1 (def 0.15) | Component tolerance drift — nudges loss corners down, adds a hair of HF loss, detunes the resonance |
| **Hum** | 0 – 1 (def 0.0) | Mains bleed depth (injected into the field so it passes through the nonlinearity) |
| **Hum Freq** | 50 / 60 Hz | Mains frequency |
| **Side Drive** | −12…+12 dB | Extra drive on the Side lane in Mid-Side mode |
| **Stereo Mode** | Stereo / Dual-Mono / Mid-Side | Channel routing |
| **Auto-Trim** | on / off | Loudness-matched output for honest bypass A/B |
| **Oversample** | Off / 2× / 4× / 8× / 16× / Auto | Anti-aliasing oversampling factor (Auto scales with drive) |
| **Quality** | Eco / Standard / Insane | ADAA order + AA filter length (and oversampling cap) |
| **Bypass** | on / off | Hard bypass |

---

## Signature Presets

Eight factory "Signature Chains" (plus an **Init** starting point) live in
`source/plugin/PresetManager.h`.

| Preset | Core | Vibe |
|--------|------|------|
| **Bus Glue** | 12 — Hybrid Stack | Gentle mix-bus glue; light drive, subtle even bias |
| **Neve-ish Weight** | 01 — GOSS | Big console low-end weight with LF saturation |
| **Drum Slam** | 04 — Cobalt-Iron | Aggressive high-flux drum saturation |
| **Vocal Air** | 02 — 80% Permalloy | Clean, airy vocal top with a lifted winding resonance |
| **Tape-ish Round** | 08 — Carbonyl Iron | Soft, tape-like rounding with plenty of hysteresis |
| **Mastering Sheen** | 06 — Nanocrystalline | Transparent master-bus sheen, Auto-Trim engaged |
| **Lo-Fi Iron** | 03 — Laminated Iron | Gritty, worn, aged lo-fi iron |
| **Broken Broadcast** | 07 — Ferrite | Hummy, resonant, heavily-aged broken-radio |

---

## DSP Engine

- **Jiles–Atherton hysteresis** (`JilesAtherton.h`) — a genuine B–H loop. Its *lag*
  between magnetisation `M` and the anhysteretic curve `M_an` is the physical
  "magnetic memory" added to the saturated flux (scaled by **Hysteresis**).
- **ADAA saturator** (`ADAA.h`, `Saturator.h`) — 1st- and 2nd-order antiderivative
  anti-aliasing on an algebraic-sigmoid waveshaper `f(u) = u/√(1+u²)`, chosen
  because it has *closed-form* first and second antiderivatives (`F1 = √(1+u²)`,
  `F2 = ½(u√(1+u²) + asinh u)`). Includes the standard ill-conditioned
  divided-difference fallbacks for equal/near-equal consecutive samples.
- **Oversampling** (`Oversampler.h`) — Off / 2× / 4× / 8× / 16× / Auto, built from a
  cascade of 2× Kaiser half-band FIR stages. Latency is exact and reported to the
  host for PDC: `(L−1)·(1 − 1/N)` base-rate samples for an `L`-tap stage over an
  `N`× cascade.
- **Frequency-dependent core loss** (`FrequencyLoss.h`) — eddy-current low-pass at
  the material corner, broadband high-shelf tilt, and an optional presence/"honk"
  mid peak.
- **Bilinear R/L/C impedance networks** (`ImpedanceNetwork.h`) — source-Z LF
  high-pass, core bass extension, interwinding resonance peak, and load-Z brightness
  shelf. A pure magnitude query feeds the Response-Curve UI.
- **Separate flux-dependent LF saturation** (`LFSaturation.h`) — a low-passed copy is
  driven into its own ADAA saturator and only the *added* harmonic content is summed
  back, so bass distorts harder on sustained low end than on transients.
- **64-bit internal** processing end to end (`double`), with denormal flushing.
- **Crossfaded core switching** — two cores run per lane during a ~1024-sample fade,
  so changing material is click-free.
- **Stereo / Dual-Mono / Mid-Side** routing, with independent Mid and Side drive in
  MS mode.

Full chain per lane (`TransformerCore.h`, `Engine.h`):

```
input trim → oversample ↑ → [ source-Z → CORE (ADAA sat + JA memory
          → eddy/hyst loss → parallel LF sat) → winding + load-Z ]
          → oversample ↓ → auto-trim → dry/wet (PDC) → output trim
```

See [`docs/DSP.md`](docs/DSP.md) for the equations and a subsystem-by-subsystem
walkthrough.

---

## 🔧 Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

JUCE (tag **8.0.4**) is fetched automatically via `FetchContent` — no submodule
needed. To build against a local checkout instead, drop JUCE into `./JUCE` and it is
preferred automatically (handy for offline / pinned CI).

**Linux dev dependencies** (Debian/Ubuntu names):

```bash
sudo apt-get install build-essential cmake pkg-config \
  libasound2-dev libjack-jackd2-dev \
  libfreetype-dev libx11-dev libxext-dev libxrandr-dev \
  libxinerama-dev libxcursor-dev libglu1-mesa-dev
```

(The build sets `JUCE_WEB_BROWSER=0` and `JUCE_USE_CURL=0`, so WebKit and cURL
dev packages are **not** required.)

### Running the native tests

The DSP test target is JUCE-free. Either via CTest (tests build by default):

```bash
ctest --test-dir build --output-on-failure
```

…or compile `tests/test_main.cpp` directly — it uses only relative includes and the
standard library:

```bash
g++ -std=c++17 -O2 tests/test_main.cpp -o fluxcore_tests
./fluxcore_tests      # prints the report; exit code 0 iff all 16 checks pass
```

### Windows installer

CI (GitHub Actions) packages the VST3 bundle and the Standalone app with Inno Setup
(`packaging/installer.iss`) into **`FLUXCORE-12-Installer.exe`**, published on `v*`
tags.

---

## Formats

- **VST3** and **Standalone** (AU / AAX planned).
- Sample rates **44.1 kHz – 192 kHz**, **64-bit float** internal processing.
- Mono or stereo I/O.

---

## Repository layout

```
WIRE-WOUND-12/
├── source/
│   ├── dsp/          JUCE-free, header-only DSP engine (cores, JA, ADAA,
│   │                 oversampler, impedance networks, loss, LF sat, engine)
│   ├── plugin/       JUCE wrapper: processor, APVTS layout, preset manager
│   └── gui/          Resizable vector UI + visualisers (analyzer, flux
│                     meter, response curve, level meters)
├── tests/            Native JUCE-free verification suite (test_main.cpp)
├── packaging/        Windows Inno Setup installer script
├── docs/             DSP deep-dive (DSP.md) + product spec (SPEC.md)
└── CMakeLists.txt
```

---

© TEKK Engineering Audio Labs. FLUXCORE·12.
