# Spring Reverb DSP Approaches

A survey of the published ways to make a computer sound like a spring, and why birbverb
picks the one it does. The decision itself is recorded in
[ADR-001](../../specs/adrs/ADR-001-spring-reverb-modelling-approach.md); this document is
the reasoning behind it.

## The thing that has to be modelled

A torsional wave travelling down a helical spring is **dispersive**: its propagation
speed depends on frequency. High frequencies arrive first, low frequencies lag. So an
impulse fed into one end comes out the other end as a **descending chirp**, and after
several round trips between the two transducers the chirps overlap into the
unmistakable spring "boing" [1].

This is why generic reverb algorithms cannot do spring reverb. A Schroeder or FDN
reverb is built from non-dispersive delays: every frequency arrives at the same time, so
echoes stay echoes. You can filter them, you can diffuse them, but you cannot make them
chirp. Parker and Bilbao make the point directly — standard methods for modelling analog
audio effects are poorly suited to spring reverberation because of the dispersive
mechanical vibration at its core [4].

Everything below is a different answer to the question: *how do you build a delay whose
delay time depends on frequency?*

## Approach 1 — Dispersive waveguide (cascaded allpass)

Fit the spring's measured dispersion curve with a chain of allpass filters, then run that
chain inside a waveguide (delay-and-feedback) structure. Abel, Berners, Costello and
Smith's 2006 work does exactly this [1].

An allpass filter passes all frequencies at equal gain but delays them by different
amounts. Cascade enough of them and you can approximate any group-delay curve you like,
including a spring's. Put the cascade in a feedback loop with a loss filter and you have a
resonator that chirps.

Välimäki and colleagues developed the idea further under the name **spectral delay
filters** — long cascades of allpass sections [2]. Their key practical trick: if you
band-limit the signal first, you can **"stretch"** the allpass sections by replacing each
unit delay with a longer delay, which gets the same dispersion curve from far fewer
sections [2]. That turns an academically interesting structure into something you can
afford to run several times over on an audio thread.

There is also published work on **automated calibration** of a parametric spring model,
which matters for us: it means the parameter-fitting problem has an established method
rather than being pure trial and error [3].

| | |
| --- | --- |
| **Strengths** | Chirps correctly. Efficient once stretched. Parametric — one model covers short and long tanks. Well documented with a calibration method. |
| **Weaknesses** | Allpass cascades have to be tuned carefully or they ring or smear. Dispersion curve has to come from somewhere (measurement or published fit). |
| **Cost** | Moderate. Scales with the number of allpass sections per spring times the number of springs. |

## Approach 2 — Finite-difference / physical simulation

Discretise the actual partial differential equation for a helical spring and step it
forward in time. Parker and Bilbao's physical-perspective work sits here, alongside wave
digital filter and finite-difference treatments [4][5].

| | |
| --- | --- |
| **Strengths** | The most accurate. Dispersion falls out of the physics rather than being fitted. Nonlinear behaviour and the mechanical "boing" come for free. |
| **Weaknesses** | Expensive. Stability conditions constrain the time step. Parameters are physical (wire diameter, coil radius, material), which is a problem when you have no spring to measure. |
| **Cost** | High. Historically offline or research-grade; feasible in real time only with careful reduction. |

## Approach 3 — Modal synthesis

Decompose the spring into its resonant modes and run a bank of resonators. A modal
architecture is attractive for real-time use because it is computationally efficient and
gives direct parametric control, though obtaining physically accurate mode parameters for
helical springs is difficult [6].

| | |
| --- | --- |
| **Strengths** | Efficient. Parameters map onto audible features one-to-one. Trivially stable. |
| **Weaknesses** | Getting physically correct mode sets for a helix is hard [6]. Many modes needed for a convincing dense tail. Transient/chirp behaviour is harder to get right than steady-state. |
| **Cost** | Moderate, and predictable. |

## Approach 4 — Neural / black box

Train a network on recordings of a real tank. There is a DSP-informed deep-network
approach for plate and spring reverb [7], and a more recent comparison of architectures
specifically for spring reverb modelling [8].

| | |
| --- | --- |
| **Strengths** | Can match a specific physical unit extremely closely. |
| **Weaknesses** | Needs the physical unit and a recording rig — we have neither for four vintage amps. Not parametric: one trained model is one tank at one setting. Inference cost. And fatally for this project, a black box **cannot be documented**, which breaks the entire premise of the repo. |
| **Cost** | High, and opaque. |

## Approach 5 — FDN / Schroeder with a dispersion stage bolted on

Use a conventional reverb and insert an allpass chain somewhere in the loop to fake the
chirp.

| | |
| --- | --- |
| **Strengths** | Cheap. Familiar. Easy to make "nice". |
| **Weaknesses** | The chirp and the decay structure become independent, which they are not in a real tank — in hardware both come from the same springs. Tends to sound like a plate with a flanger. |
| **Cost** | Low. |

## What birbverb needs from an approach

Ranked, because these conflict:

1. **It must chirp**, with the chirp coupled to the delay structure rather than bolted on.
2. **It must be parametric across the four units.** The bank spans a transformerless
   Ampeg driver and a power-tube-driven Fender unit, and the differences live in the
   drive and the tank length, not in five separate algorithms.
3. **It must be documentable.** Every audible behaviour has to trace back to a circuit
   study. This is a hard requirement of the project, not a preference.
4. **It must be affordable** — multiple instances on a guitar bus, no oversampling budget
   left over after the tube stages.
5. **It must be calibratable from published figures**, because we do not have the
   hardware. Our measurable targets are datasheet decay times and impedance curves, not
   impulse responses we recorded.

Requirement 5 is the one that decides it. Finite-difference needs physical spring
dimensions we do not have; neural needs recordings we cannot make; modal needs mode sets
that are documented as hard to derive for helices [6]. The allpass-waveguide approach is
the only one of the five that can be fitted to *the numbers we actually have* — decay
time from a tank datasheet, dispersion from a published curve, drive level from a SPICE
analysis of the driver stage.

**Decision: dispersive waveguide with stretched allpass cascades.** Recorded in
[ADR-001](../../specs/adrs/ADR-001-spring-reverb-modelling-approach.md), specified in
[SPEC-001](../../specs/active/SPEC-001-spring-tank-dsp-core.md).

## Sources

1. Abel, Berners, Costello, Smith — *Spring Reverb Emulation Using Dispersive Allpass Filters in a Waveguide Structure* (2006). Summarised in [Publication V, Aalto](https://aaltodoc.aalto.fi/server/api/core/bitstreams/3724cce5-7f45-47d3-ba52-e97104129b47/content)
2. Välimäki et al. — *Parametric Spring Reverberation Effect* / spectral delay filters. [ResearchGate](https://www.researchgate.net/publication/230561075_Parametric_Spring_Reverberation_Effect)
3. *Automated Calibration of a Parametric Spring Reverb Model*. [ResearchGate](https://www.researchgate.net/publication/230561094_Automated_Calibration_of_a_Parametric_Spring_Reverb_Model)
4. Parker, Bilbao — *Spring Reverberation: A Physical Perspective*. [Academia.edu](https://www.academia.edu/251885/Spring_Reverberation_A_Physical_Perspective)
5. *Numerical Simulation of Spring Reverberation*. [ResearchGate](https://www.researchgate.net/publication/262731418_NUMERICAL_SIMULATION_OF_SPRING_REVERBERATION)
6. Modal spring reverb architecture discussion — efficiency and parametric control versus parameter-derivation difficulty. Author unconfirmed; see the note in Open Questions below.
7. *Modeling plate and spring reverberation using a DSP-informed deep neural network*. [arXiv:1910.10105](https://arxiv.org/pdf/1910.10105)
8. *Evaluating Neural Network Architectures for Spring Reverb Modelling*. [arXiv:2409.04953](https://arxiv.org/pdf/2409.04953)

## Open Questions

- [ ] Source [6] was retrieved without a confirmed author list, so it is cited as an
      unattributed claim. Before quoting it in a spec, identify the paper — likely in the
      DAFx or IEEE TASLP proceedings.
- [ ] The specific dispersion curve to fit. [1] and [2] both publish fits; we need to pick
      one and record the coefficients in SPEC-001 rather than deriving them ad hoc.
- [ ] Whether the stretched-allpass trick from [2] interacts badly with our tube-stage
      oversampling. Needs a measurement once both exist.
