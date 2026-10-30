---
id: ADR-001
title: 'Spring reverb modelling approach'
status: proposed
date: 2026-10-29
deciders: ['Anthony Coffey']
supersedes: ''
superseded_by: ''
---

# ADR-001: Spring reverb modelling approach

## Status

`proposed`

## Context

birbverb has to reproduce the sound of four vintage spring reverb circuits. The spring
itself is the hard part: a wave in a helical spring is dispersive, so an impulse returns
as a chirp, and no conventional reverb topology produces that because conventional
reverbs are built from non-dispersive delays.

There are two chirp series. The dominant one exists only below a transition frequency
`F_C`, where group delay peaks, so it **ascends**. A second, wideband series has maximum
group delay at DC and descends, at least 30 dB down. An earlier draft of this ADR said
the impulse returns as a descending chirp, full stop; that described the quiet series and
missed the loud one. Corrected in
[spring-dispersion-and-allpass-fitting](../../documentation/deep-dives/spring-dispersion-and-allpass-fitting.md).

Five published approaches were surveyed in
[spring-reverb-dsp-approaches](../../documentation/deep-dives/spring-reverb-dsp-approaches.md):
dispersive waveguide with allpass cascades, finite-difference physical simulation, modal
synthesis, neural black-box modelling, and an FDN with a dispersion stage added.

The forces at play:

- **We do not have the hardware.** No Twin Reverb, no 6G15, no Reverberocket, no tank on
  a bench. Everything we know comes from datasheets, schematics and published analysis.
  Our [circuit studies](../../documentation/circuits/) list measurable targets like
  "RT60 2.75–4.0 s" and "8 Ω rising at 6 dB/octave" — not impulse responses.
- **The project premise is documentation.** Every audible behaviour must trace to a
  circuit study. An approach we cannot explain is an approach we cannot ship, regardless
  of how good it sounds.
- **It is a guitar plugin.** It has to run several instances on a laptop, alongside
  oversampled tube stages.
- **One codebase, four units.** The models differ mostly in how hard the tank is driven
  and what follows it. We want one tank algorithm with parameters, not four algorithms.
- **Open source.** Contributors need to be able to read, understand and modify the DSP.

## Decision

**Model the tank as a dispersive waveguide: per-spring delay loops whose propagation is
shaped by cascades of stretched allpass sections, with loss filtering in the loop setting
decay time.**

Concretely:

- Each spring is a bidirectional delay loop with reflections modelled at both the input
  and output transducer, since energy makes multiple round trips in a real tank.
- Dispersion comes from a cascade of **stretched first-order** allpass sections (unit
  delays replaced by longer, fractional-length delays) fitted to the closed-form group
  delay in **Parker (2011) eq. (4)**, with the physical targets `F_C` and `T_D` computed
  from **Bilbao & Parker (2009) eqs. (2)–(4)** applied to published caliper measurements
  of real springs. Section counts `M_low = 100` and `M_high = 200`, from Parker.
- A second, wideband loop at −30 dB carries the descending chirp series.
- Decay time (RT60) is set by a low-pass loss filter in the loop, so decay is
  frequency-dependent as it is in hardware, and is tunable to the tank datasheet range.
- Multiple springs run in parallel with deliberately mismatched delay lengths.
- Dispersion collapses above a transition frequency, so the model does not chirp all the
  way to Nyquist.
- The drive and recovery stages (SPEC-002) sit outside the tank model and are what the
  per-amp presets mostly differ in.

The tank model lives in `Source/dsp/` as header-only C++ in `namespace bv`, with no JUCE
dependency, so the offline test suite can exercise it directly.

## Consequences

### Positive

- The chirp is **structural**, not an effect applied to echoes. It comes from the same
  delay loops that produce the decay, which is how hardware works.
- Fittable from the data we actually have, and it has been: decay time comes from the tank
  datasheet, dispersion from two **open-access** papers, drive level from the AB763 study.
  Nothing requires hardware we don't own. Two of the three tank types have their `T_D` and
  `F_C` derived from someone else's published caliper readings rather than guessed.
- One parametric algorithm covers all four units. Tank length, spring count, decay and
  dispersion cutoff are parameters.
- Affordable. The stretched-allpass formulation exists precisely to make this real-time
  viable.
- Explainable. Every block maps to a physical part of the tank, so the code can be
  commented with the circuit property it implements.
- There is published work on automated calibration of exactly this kind of model, so
  parameter fitting has a method to follow.

### Negative / Trade-offs

- Allpass cascades are finicky. Badly tuned, they smear transients or ring. This needs
  real test coverage, which is why SPEC-006 specifies group-delay measurement as a
  first-class test.
- Less physically accurate than finite-difference. We will not get nonlinear spring
  behaviour (the clank of a spring driven to its mechanical limit) for free. If we want
  it, it has to be added deliberately.
- Requires choosing a dispersion curve to fit. That is now done and recorded, and it
  validates against five springs someone else measured — but not against a real
  Accutronics tank, which nobody in the literature has calipered. The Long 2-Spring
  configuration is interpolated for that reason.
- The mechanical "boing" from physically shocking the tank is not an emergent behaviour
  here. If we want it, it is a separate feature.

### Neutral

- Delay-line memory is modest, so the per-instance footprint stays small.
- The structure is familiar to anyone who has worked with waveguide synthesis, which
  helps contributors — and is unfamiliar to anyone who has only worked with FDN reverbs,
  which does not.

## Alternatives Considered

### Option A: Finite-difference / wave digital physical simulation

- **Description:** Discretise the spring's PDE and integrate it.
- **Pros:** Most accurate. Dispersion and nonlinearity emerge from physics.
- **Cons:** CPU-expensive; stability constrains the time step; parameters are physical
  dimensions of a spring we do not have.
- **Why rejected:** We cannot supply the inputs. Parameterising it would mean inventing
  spring dimensions, which is a guess wearing a physics costume — worse for this project
  than an honest fitted model.

### Option B: Modal synthesis

- **Description:** A bank of resonators tuned to the spring's modes.
- **Pros:** Efficient, stable, directly parametric.
- **Cons:** Deriving physically accurate mode parameters for a helical spring is
  documented as difficult; dense tails need many modes; transient chirp behaviour is
  harder to get right than steady-state response.
- **Why rejected:** Same root problem as Option A — the parameters have to come from
  somewhere, and for a helix they are hard to derive. Reconsider if a published mode set
  for guitar-amp tanks turns up.

### Option C: Neural black-box model

- **Description:** Train a network on recordings of a real tank.
- **Pros:** Can match a specific unit very closely.
- **Cons:** Needs the physical units and a recording rig. Not parametric. Inference cost.
- **Why rejected:** Fails the project's documentation premise outright. A model whose
  behaviour cannot be traced to a circuit property cannot be specced, and a repo whose
  central claim is "we researched the circuitry" cannot ship a black box as its engine.

### Option D: FDN or Schroeder reverb with an allpass dispersion stage added

- **Description:** A conventional reverb with a chirp stage inserted.
- **Pros:** Cheap, simple, easy to make pleasant.
- **Why rejected:** Decouples the chirp from the decay structure, which in hardware are
  the same mechanism. The characteristic result is a plate reverb with a flanger on it.
  Kept in mind as a possible low-CPU "eco" mode, which would be a separate spec and would
  have to be labelled as not a model of anything.

## Notes

- Survey and sources:
  [spring-reverb-dsp-approaches](../../documentation/deep-dives/spring-reverb-dsp-approaches.md)
- The chosen curve, its derivation and its coefficients:
  [spring-dispersion-and-allpass-fitting](../../documentation/deep-dives/spring-dispersion-and-allpass-fitting.md)
- Implementation spec: [SPEC-001](../active/SPEC-001-spring-tank-dsp-core.md)
- Drive and recovery stages: [SPEC-002](../active/SPEC-002-tube-drive-and-recovery-stages.md)
- Test strategy: [SPEC-006](../active/SPEC-006-offline-dsp-test-harness.md)
- **This ADR is `proposed`, not `accepted`.** It should move to `accepted` once SPEC-001's
  first dispersion test passes with a fitted curve, because that is the point at which we
  know the approach works with the data we have. If the fit cannot hit the measurable
  targets in the tank study, revisit Option B before writing more DSP.
