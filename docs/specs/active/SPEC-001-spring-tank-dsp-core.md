---
id: SPEC-001
title: 'Spring tank DSP core'
status: in-progress
created: 2026-10-29
author: Anthony Coffey
reviewers: []
affected_repos: ['birbverb']
---

## Reviewer Notes

<!-- Leave empty until code review. When requesting changes, reviewer adds feedback here: -->

### Implementation note, 2026-10-30 — the CPU budget is already exceeded

The Constraints section budgets **under 2% of one core at 48 kHz** for the whole
3-spring tank. With `DispersiveAllpass` implemented, the allpass cascades *alone* measure,
on Apple M-series silicon at `-O3 -march=native -flto`:

| Configuration | Sections/sample | Cost |
| --- | --- | --- |
| **As specified** — 3 x `M_low`=100 plus `M_high`=200 | 500 | **4.40%** |
| Chirp-straightened, both loops at `M/2` | 250 | **1.91%** |
| The three springs alone, no secondary loop | 300 | 2.52% |
| The secondary loop alone | 200 | 1.86% |

(At `-O2` without `-march=native` the first row is 6.53%.) Cost is linear in section
count, about 0.0088% per section per core. This is before `SpringLine` adds delay lines,
loss filters and reflections, and before SPEC-002's stages spend their own 6%. The
measuring hardware is also *faster* than the "2020-era laptop" the budget names.

So the constraint is not at risk, it is breached by roughly 2.2x.

**The striking line is the last one.** `M_high` = 200 costs 1.86% — 42% of the total, and
more than all three springs put together — to render a series that SPEC-001 requirement 11
puts *at least 30 dB down*. The most expensive single component in the tank is its
quietest. If an approximation has to go somewhere, that is where it is least audible.

#### Two structural ideas that do not work, both measured

Recorded so nobody spends an afternoon rediscovering them:

- **Decoupling into `integerDelay` independent sample streams.** True for integer `k`:
  the section reduces to `w[n] = (1-a^2)*s[n] - a*w[n-K]`, which is `K` independent
  streams. It is **false** for the fractional case we actually use, because the
  interpolator carries its own first-order recursion (`v[n]` depends on `v[n-1]`) that
  couples adjacent samples. Replacing the allpass interpolator with an FIR one would
  restore the decoupling but break unity magnitude, which is the one property the whole
  component exists to have.
- **Block-outer, section-inner processing** for cache locality. Measured at **0.35x — three
  times slower.** There is no locality problem to fix: one chain's entire state is about
  2.4 KB and already L1-resident, so restructuring only adds `M` round-trips of the audio
  block through memory. Output is bit-identical, which is how the comparison was made
  fair; it is simply slower.

At roughly 6.5 cycles per section, the cascade is **latency-bound on the serial dependency
chain inside each section**, not throughput-bound. Micro-optimising the arithmetic will
not move it.

#### Options, in the order worth considering

1. **Chirp-straighten the secondary loop only** (`M_high` 200 -> 100), taking the total to
   roughly 3.5%. Buys the largest single saving at the point the spec itself says is 30 dB
   down. Does not touch the main series at all.
2. **Chirp-straighten both loops**, which lands at **1.91% — inside budget**. Published,
   measured and reversible, but it introduces a known notch at the crossover and we have
   no listening reference to judge the trade against yet.
3. **SIMD across chains.** The three springs and the secondary loop are four fully
   independent cascades, which is the only real parallelism in the structure. The obstacle
   is that each spring has a different `integerDelay` by design — requirement 4 spreads
   `F_C` on purpose — so the lanes need either gather loads or a forced common
   `integerDelay`, which would push some springs' interpolator remainder outside the
   [0.5, 1.5] window where it is well conditioned.
4. **Revise the budget**, if a measurement on the actual target hardware says 2% was
   optimistic rather than the implementation being wasteful.

Needs a decision before `SpringLine` lands, because options 1 and 2 change the structure
`SpringLine` wraps.

---

# Feature: Spring tank DSP core

## Problem

Every preset in birbverb shares one component: the spring tank. The amps differ in how
hard they drive it and what they do with the return, but the tank is what makes any of it
sound like spring reverb rather than like a delay with a filter on it.

A generic reverb cannot do this. The defining artefact is **dispersion** — a wave in a
helical spring travels at a frequency-dependent speed, so an impulse returns as a chirp,
and successive round trips smear further.

There are **two** chirp series, and they go in opposite directions:

| Series | Band | Group delay | Chirp | Level |
| --- | --- | --- | --- | --- |
| Main (`C_lf`) | below the transition frequency `F_C` | minimum at DC, **peaks at `F_C`** | **ascending** | reference |
| Secondary (`C_hf`) | full band | **maximum at DC**, falls with frequency | descending | 30 dB or more down |

An earlier draft of this spec said the impulse returns as a *descending* chirp. That was
wrong, and it was wrong about the series that carries the sound. The correction and its
sources are in
[spring-dispersion-and-allpass-fitting](../../documentation/deep-dives/spring-dispersion-and-allpass-fitting.md).
Requirement 1 and acceptance criterion 1 are written the way they are because of it.

See also
[accutronics-spring-tanks](../../documentation/circuits/accutronics-spring-tanks.md) and
[spring-reverb-dsp-approaches](../../documentation/deep-dives/spring-reverb-dsp-approaches.md).

This spec defines the tank model only: signal in at the input transducer, signal out at
the output transducer. Drive and recovery are [SPEC-002](SPEC-002-tube-drive-and-recovery-stages.md).

## Requirements

### Must have

1. WHEN an impulse is fed to the tank, the main echo series SHALL return an **ascending**
   chirp — group delay SHALL increase with frequency from DC to a maximum at the tank's
   transition frequency `F_C`.
2. WHEN `decay` is set to a value in [0.8, 4.5] seconds, the measured RT60 of the tank's
   impulse response SHALL match the requested value within ±15%.
3. WHEN the tank type is set, the system SHALL configure the modelled spring count, the
   per-spring echo spacing `T_D` and the per-spring transition frequency `F_C` for that
   type: Short 2-Spring, Long 2-Spring, Long 3-Spring.
4. WHEN more than one spring is modelled, the springs SHALL be deliberately mismatched
   according to the per-type design philosophy recorded in the dispersion study: a
   2-spring tank matches `T_D` within 15% and spreads `F_C` by 15–30%, a 3-spring tank
   spreads `T_D` by 20–35% and matches `F_C` within 12%.
5. WHEN an impulse is fed to the tank, the output SHALL contain multiple distinct
   arrivals spaced at `T_D`, consistent with reflection at both transducers rather than a
   single pass.
6. ABOVE `F_C`, the main series SHALL NOT be present — it SHALL be low-passed out rather
   than allowed to chirp on to Nyquist.
7. WHEN decay is frequency-weighted, high frequencies SHALL decay faster than low
   frequencies, as they do in a real tank.
8. WHEN fed any bounded input, including DC, full-scale noise, and parameter sweeps at
   block rate, the output SHALL remain finite and SHALL NOT exceed a bounded headroom
   limit.
9. WHEN `prepare()` is called at any sample rate between 44.1 kHz and 192 kHz, all
   time-domain behaviour (delays, RT60, `F_C`, dispersion curve) SHALL be sample-rate
   independent within the tolerances above. This requires a **fractional** stretch
   factor; integer `k` alone cannot satisfy it (see Design).
10. WHEN `reset()` is called, the tank SHALL return to silence with no residual state.
11. WHEN the secondary wideband series is active, its contribution SHALL sit at least
    30 dB below the main series, and its group delay SHALL **decrease** with frequency.

### Nice to have

- A `boing` control that exaggerates dispersion beyond the fitted curve. Explicitly a
  musical liberty, not a model of anything (see Constraints).
- Stereo decorrelation: a second tank instance with slightly different spring lengths for
  a wide wet image. Hardware is mono; this is a liberty and must be labelled as one.
- Modelled transducer resonance on the output pickup.
- Three parallel allpass chains per spring, one per propagation velocity. The physical
  dispersion relation is triple-valued below `F_C`; one chain models one velocity. Costs
  3x and the audible benefit is unknown.

### Non-goals (what this does NOT do)

- This spec does NOT model the drive stage, the reverb transformer, the recovery stage,
  the tone control, or the mix network. Those are SPEC-002 and SPEC-003.
- This spec does NOT model power supply sag, output stage distortion, or speaker
  response. birbverb models a reverb circuit, not an amplifier.
- This spec does NOT model nonlinear spring behaviour (the mechanical clank of a spring
  driven past its limit). ADR-001 records that the chosen approach does not give this for
  free. If wanted, it gets its own spec.
- This spec does NOT model physically shocking the tank (kicking the amp).
- This spec does NOT implement Parker's chirp-straightening / multirate optimisation. It
  is documented in the dispersion study as the first thing to reach for if CPU binds.

## Design

Per [ADR-001](../adrs/ADR-001-spring-reverb-modelling-approach.md): a **dispersive
waveguide** — delay loops with cascaded stretched allpass sections providing
frequency-dependent propagation, and loss filtering in the loop setting decay.

```
         +---------------------- one spring -----------------------+
in --+-->| delay(T_D) -> A^Mlow(z^k) -> loss LPF --+--> out -------+--+--> sum --> out
     |   |                   ^                      |              |  |
     |   |                   +------ reflection ----+              |  |
     |   +--------------------------------------------------------+   |
     +-->| spring 2, mismatched per requirement 4 |-------------------+
     +-->| spring 3 (3-spring types only)         |-------------------+
     |
     +-->| C_hf : delay -> A^Mhigh(z), 30 dB down |-------------------+
```

### Components

| Component | File | Responsibility |
| --- | --- | --- |
| `Dispersion` | `Source/dsp/Dispersion.h` | The three closed forms: group delay of a stretched cascade, stretch factor from `F_C`, coefficient from a target peak delay. Pure functions, no state |
| `DispersiveAllpass` | `Source/dsp/DispersiveAllpass.h` | One stretched first-order allpass section with a fractional stretch factor |
| `SpringLine` | `Source/dsp/SpringLine.h` | One spring: delay loop + allpass cascade + loss filter + transducer reflections |
| `SpringTank` | `Source/dsp/SpringTank.h` | N `SpringLine`s with mismatched parameters, plus the secondary wideband loop, summed; owns tank-type configuration |
| `TankConfig` | `Source/dsp/TankConfig.h` | Plain struct of tank parameters, so presets and tests configure a tank the same way |

All header-only, `namespace bv`, no JUCE. Tests include these headers directly.

### Dispersion curve — decided

The gating decision for this spec. Full derivation, validation and sources:
[spring-dispersion-and-allpass-fitting](../../documentation/deep-dives/spring-dispersion-and-allpass-fitting.md).

**Fit the cascade to Parker (2011) eq. (4):**

```
D(omega) = k*M*(1 - a^2) / ( 1 + 2*a*cos(omega*k) + a^2 )
```

Chosen over Abel et al. 2006 and Valimaki et al. 2010 — which are the original models —
because Parker 2011 is **open access (CC BY)** and Bilbao & Parker (DAFx-09) is free from
the DAFx archive, so every constant below traces to a source a contributor can actually
open. The paywalled papers are cited as the primary literature; no birbverb constant
depends on them.

Three design identities, implemented in `Dispersion.h`:

```
k  = fs / (2 * F_C)                            // D peaks at fs/(2k)
a  = (D_peak - k*M) / (D_peak + k*M)           // from D(f_peak) = k*M*(1+a)/(1-a)
D(DC) = k*M*(1 - a) / (1 + a)
```

with `D_peak = T_D` (the chirp fills the echo period) and `M_low = 100`, `M_high = 200`
from Parker 2011 section 4.

`k` is fractional by construction. Integer `k` would move `F_C` with the sample rate — at
48 kHz, `k = 5` gives 4800 Hz and `k = 6` gives 4000 Hz — which breaks requirement 9.
Fractional `k` uses an interpolating first-order allpass for one internal unit delay,
designed with Parker eq. (6).

The physical targets `F_C` and `T_D` come from Bilbao & Parker eqs. (2)–(4) applied to
their own calipered spring geometries. Note that `F_C` depends on wire and helix radius
only — **not on spring length** — which is why requirement 4 can specify matching one
parameter while spreading the other.

### Tank type parameters

All values derived via Bilbao & Parker eqs. (2)–(4); see the dispersion study for the
geometry each row came from. `a_lf` is at 48 kHz; it is recomputed per sample rate.

| Tank type | Spring | `T_D` | 1-pass | `F_C` | `k` @48k | `a_lf` @48k | Provenance |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Short 2-Spring | A | 30.61 ms | 15.31 ms | 3244 Hz | 7.397 | +0.33027 | **measured** (Olson X-82 #1) |
| | B | 35.10 ms | 17.55 ms | 2543 Hz | 9.439 | +0.28186 | **measured** (Olson X-82 #2) |
| Long 2-Spring | A | 52.69 ms | 26.34 ms | 4471 Hz | 5.368 | +0.64983 | *interpolated* |
| | B | 57.28 ms | 28.64 ms | 3784 Hz | 6.342 | +0.62517 | *interpolated* |
| Long 3-Spring | A | 41.62 ms | 20.81 ms | 4887 Hz | 4.911 | +0.60539 | **measured** (Leem #1) |
| | B | 40.24 ms | 20.12 ms | 4672 Hz | 5.137 | +0.57981 | **measured** (Leem #2) |
| | C | 52.69 ms | 26.34 ms | 4471 Hz | 5.368 | +0.64983 | **measured** (Leem #3) |

Default RT60 and loop gain:

| Tank type | Default RT60 | `T_loop` (mean `T_D`) | loop gain `g` | Source |
| --- | --- | --- | --- | --- |
| Short 2-Spring | 1.6 s | 32.86 ms | 0.86775 | datasheet decay code 1 (1.2–2.0 s) |
| Long 2-Spring | 3.4 s | 54.99 ms | 0.89430 | datasheet decay code 3 (2.75–4.0 s) |
| Long 3-Spring | 3.0 s | 44.85 ms | 0.90188 | tank study |

Secondary loop, all types: `M_high = 200`, `k = 1`, `a_hf = -0.6`, level −30 dB. That
coefficient gives 16.7 ms group delay at DC, 2.5 ms at 10 kHz and 1.0 ms at Nyquist,
matching Parker's "little dispersion above 10 kHz".

### Numbers that are still inferences

Down from two whole categories to a short list. Carried from the dispersion study's
"Still unknown":

- **Long 2-Spring geometry.** The only *interpolated* row above. Closable with calipers on
  a real `4AB3C1B`: eqs. (2)–(4) then give `T_D` and `F_C` with no fitting.
- **Series or parallel springs.** Both measured units are parallel and birbverb models
  parallel. Series would roughly double `T_D`.
- **Transducer reflection coefficient** and **loss filter shape.** Fitted, not derived.

### The tension in requirement 7

Group delay *peaks* at `F_C`, so energy there makes fewer round trips per second than
energy at DC. With a flat loss filter, `F_C` would therefore decay **slower** than the
rest of the band — the opposite of requirement 7. The loss filter must be a low-pass with
enough slope to overcome that. Acceptance criterion 7 is checking a real structural
tension, not a formality.

## Edge cases

- [ ] Sample rate change mid-session — `prepare()` must recompute `k` and `a` per spring
      without clicking or changing RT60 or `F_C`
- [ ] `decay` swept quickly — loss filter coefficients must be smoothed, no zipper noise
      and no instability at the extremes
- [ ] Tank type switched during playback — changing spring count must not produce a click
      or leave stale delay-line content audible
- [ ] Very short delay at high sample rate — stretch factor must stay valid
      (`k * M` must not exceed the loop delay, or the cascade outruns its own loop)
- [ ] `F_C` above Nyquist/2 at 44.1 kHz — clamp `k` at its minimum rather than wrapping
- [ ] Denormals in the decay tail — must not cost CPU; flush or add tiny DC offset
- [ ] Block size of 1 sample, and block size of 8192 samples
- [ ] Mono input, stereo input, and mismatched in/out channel counts

## Acceptance criteria

1. Impulse in: measured group delay **increases** monotonically from 200 Hz to 0.9 × `F_C`,
   and the maximum falls within ±15% of `F_C`. Verified by `tests/test_dispersion.cpp`.
2. Impulse in at `decay = 3.4 s`: measured RT60 is 3.4 s ±15%. Verified by
   `tests/test_decay.cpp`.
3. RT60 tracks the `decay` parameter across [0.8, 4.5] s within ±15% at 44.1, 48, 96 and
   192 kHz.
4. Impulse in: autocorrelation of the tail shows its first peak within ±10% of the
   configured `T_D` for the active tank type.
5. Spring mismatch matches the per-type philosophy: 2-spring tanks have `T_D` spread below
   15% and `F_C` spread between 15% and 30%; the 3-spring tank has `T_D` spread between
   20% and 35% and `F_C` spread below 12%.
6. Group delay above 1.5 × `F_C` is flat within ±0.5 ms **and** magnitude there is at
   least 24 dB below the in-band level, demonstrating the main series has been removed
   rather than left to chirp on.
7. High-frequency RT60 (above 2 kHz) is shorter than low-frequency RT60 (200–500 Hz) by at
   least 30%.
8. 60 s of full-scale white noise, DC, and 100 randomised parameter sweeps: output stays
   finite and peak magnitude stays below +12 dBFS.
9. After `reset()`, 1 s of output following a full-scale impulse is below −120 dBFS.
10. All tests deterministic — seeded noise, identical results across two consecutive runs.
11. `Dispersion.h` closed forms match the published values: `D(0)` for `a = -0.7, k = 1,
    M = 1` is 5.6667 samples ±0.1%; `stretchFactor` and `coeffForPeakDelay` round-trip
    every row of the tank parameter table to within 0.01%.
12. Secondary loop in isolation: group delay **decreases** monotonically from 200 Hz to
    10 kHz, and its level is 30 dB or more below the main series.

## Constraints

- Header-only, no JUCE, `namespace bv`. This is what makes the offline suite possible.
- No allocation on the audio thread. All delay-line memory allocated in `prepare()`.
- CPU budget: the tank alone under 2% of one core at 48 kHz on a 2020-era laptop, for the
  3-spring configuration. Drive and recovery stages get their own budget in SPEC-002.
  With `M_low = 100` per spring plus `M_high = 200`, a 3-spring tank is 500 allpass
  sections per sample — this budget is the binding constraint on the design, and
  Parker's chirp-straightening is the documented escape hatch.
- The `boing` control, if implemented, MUST be documented in the UI and in the preset
  descriptions as an effect control rather than a modelled property. The project's whole
  claim is that behaviour traces to hardware; a knob that does not must say so.

## Tasks

- [x] Choose the dispersion curve to fit and record the coefficients in this spec —
      Parker 2011 eq. (4), physical targets from Bilbao & Parker eqs. (2)–(4). See
      [the dispersion study](../../documentation/deep-dives/spring-dispersion-and-allpass-fitting.md)
- [x] Resolve the single-pass delay times per tank type — derived, two of three tank types
      from measured geometry
- [x] Resolve the dispersion transition frequency — derived per spring, 2543–4887 Hz
- [x] Correct the chirp direction in this spec, ADR-001 and the tank study
- [x] GREEN: `Dispersion.h` — the three closed forms
- [x] `tests/test_dispersion_curve.cpp` — assert the closed forms against the published
      numbers. Runs without `SpringTank`, so it is not tagged `[.red]`
- [x] RED: `tests/test_dispersion.cpp` — ascending group delay, `F_C` location, secondary
      series direction
- [x] RED: `tests/test_decay.cpp` — RT60 measurement helper, then the tracking assertions
- [x] GREEN: `DispersiveAllpass` — stretched section with fractional `k` via Parker eq. (6).
      `tests/test_dispersive_allpass.cpp` was written against a passthrough skeleton, watched
      to fail, then promoted out of `[.red]` once the structure realised the curve
- [x] GREEN: `SpringLine` — delay loop, allpass cascade, loss filter, reflections.
      `tests/test_spring_line.cpp` covers requirements 1, 2, 5, 6, 7, 8, 9 and 10 at the
      single-spring level; the tank-level criteria stay with `SpringTank`
- [x] GREEN: `SpringTank` — multi-spring sum, secondary loop, tank-type configuration
- [x] RED then GREEN: echo-spacing, spring-mismatch, frequency-dependent decay tests
- [x] RED then GREEN: stability and reset tests
- [x] Sample-rate independence tests at 44.1 / 48 / 96 / 192 kHz
- [x] Denormal handling and a CPU measurement against the budget — see Reviewer Notes;
      the budget is breached and the decision is deferred until there is something to hear
- [ ] REFACTOR: review every constant in the DSP files for a comment naming its source
- [x] Wire into `tools/render.cpp` for listening tests

## Notes

- Approach decision: [ADR-001](../adrs/ADR-001-spring-reverb-modelling-approach.md)
- Dispersion curve and coefficients:
  [spring-dispersion-and-allpass-fitting](../../documentation/deep-dives/spring-dispersion-and-allpass-fitting.md)
- Hardware research: [accutronics-spring-tanks](../../documentation/circuits/accutronics-spring-tanks.md)
- Test harness: [SPEC-006](SPEC-006-offline-dsp-test-harness.md)
- This spec reached `ready` when the dispersion curve was recorded. It was `draft` while
  that was open, because fitting a curve is the one decision the whole model rests on and
  leaving it to implementation time is how projects end up with an allpass chain nobody
  can explain.
