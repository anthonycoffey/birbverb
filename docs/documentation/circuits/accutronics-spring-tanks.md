---
id: CIRCUIT-001
title: 'Accutronics / Hammond Spring Reverb Tanks'
hardware: 'Accutronics (Hammond) Type 1/4/8/9 reverb tanks'
years: '1960s–present'
status: draft
updated: 2026-10-29
author: Anthony Coffey
confidence: medium
---

# Circuit Study: Accutronics / Hammond Spring Reverb Tanks

> The tank is the one component every unit in our preset bank shares. Model differences
> between the amps come from how hard the tank is driven and what happens after it — but
> the tank itself sets the delay structure, the dispersion, and the decay envelope.

## Summary

Spring reverb was developed by Hammond for organs; Fender licensed the approach for
guitar amps [6]. A tank is two transducers joined by two or more helical springs: an
input transducer (an electromagnet that twists the spring) and an output transducer (a
magnet and coil that picks up the resulting motion) [6]. Delay comes from the travel time
of a wave down the spring, and because that wave is **dispersive** — different frequencies
travel at different speeds — a single impulse arrives at the far end smeared into a chirp.
That chirp, not the delay, is the spring sound.

The dominant chirp **ascends**: group delay rises with frequency up to a transition
frequency `F_C`, so low frequencies arrive first. An earlier version of this study said
descending, which describes a second, wideband series that sits at least 30 dB down. Both
are real; only one carries the sound. See
[spring-dispersion-and-allpass-fitting](../deep-dives/spring-dispersion-and-allpass-fitting.md).

## Part numbering

Accutronics part numbers are seven characters, one spec per position [1][5]:

| Position | Encodes |
| --- | --- |
| 1 | Type (tank length / spring count) |
| 2 | Input impedance (letter, read from the input column of that type's table) |
| 3 | Output impedance (letter, read from the output column) |
| 4 | Decay time |
| 5 | Connectors (grounded / insulated, input and output) |
| 6 | Locking devices |
| 7 | Mounting plane |

**Type (position 1)** [1][2]:

| Type | Length | Springs |
| --- | --- | --- |
| 1 | Long (~17 in) | 2 |
| 4 | Short (~9 in) | 2 |
| 8 | Short | 3 |
| 9 | Long | 3 (reported elsewhere as 6 springs, counter-wound, coupled in parallel pairs — see Open Questions) |

**Decay (position 4)**, for Type 1 / 4 [1]:

| Code | Decay |
| --- | --- |
| 1 | Short — 1.2 to 2.0 s |
| 2 | Medium — 1.75 to 3.0 s |
| 3 | Long — 2.75 to 4.0 s |

**Impedance (positions 2 and 3)** are measured at 1 kHz, and the letter-to-value map
differs by type [1]. For Type 4: `A` = 8 Ω input / 500 Ω output; `B` = 150 Ω input /
2.25 kΩ output; `E` = 600 Ω input. For Type 8/9 the same letters shift: `A` = 10 Ω input
/ 600 Ω output, `B` = 190 Ω input / 2.575 kΩ output.

Worked example [1]: `4EB2C1B` = Type 4, 600 Ω input, 2250 Ω output, medium decay, input
insulated with output grounded.

## The tank in our Fender presets: 4AB3C1B

Decoding `4AB3C1B`: Type 4, input `A`, output `B`, decay `3`, connectors `C`, locking
`1`, mounting `B`.

| Property | Value | Source |
| --- | --- | --- |
| Input impedance | 8 Ω at 1 kHz (≈ 1.27 mH) | [3] |
| Output impedance | 2.25 kΩ at 1 kHz | [3] |
| Input DC resistance | 0.81 Ω | [3] |
| Nominal drive current | 28.0 mA RMS (39.6 mA peak) | [3] |
| Decay time | 2.75 to 4.0 s | [1][3] |
| Springs | 2, long decay | [3][7] |
| Mounting | Horizontal (the `B` suffix; `4AB3C1C` is the vertical-mount variant) | [4] |

### Why the drive is a current drive, not a voltage drive

The input transducer is essentially a coil: its impedance is almost purely reactive and
**rises with frequency** [3]. Drive it from a voltage source and bass draws far more
current than treble, so the tank is excited hardest exactly where spring reverb sounds
worst. Two consequences, both of which we have to model:

1. Fender's answer was a small 500 pF coupling capacitor into the driver grids, which
   attenuates bass before it ever reaches the transformer [3][6].
2. The useful mental model for the send is **current into the transducer**, not voltage.
   A frequency-dependent load that our drive stage has to work against is the difference
   between "spring reverb" and "a delay with a filter on it".

## What gives the tank its character

1. **Dispersion.** A wave in a helical spring is dispersive. An impulse comes back as a
   chirp that sweeps **upward** to the transition frequency, and successive echoes are
   progressively more smeared. This is the single most identifiable spring artefact.
2. **Multiple springs, deliberately mismatched.** Each spring has a slightly different
   delay, so the echo train is a comb of near-coincident taps rather than one clean
   repeat. Springs of different makeup add multiple delays [6].
3. **Reflections at both ends.** The wave reflects off the input transducer as well as
   the output one, so energy makes multiple round trips with diminishing amplitude [6].
   That is where the decay tail comes from — the tank is a resonator, not a delay line.
4. **A transition frequency `F_C`.** Group delay peaks there and the main chirp series
   does not exist above it. This is why real tanks are dark and why a model that chirps
   all the way to Nyquist sounds like a flanger. `F_C` depends on wire radius and helix
   radius only — **not on spring length** — which is why a maker can change echo spacing
   and transition frequency independently, and did.
5. **Boing.** The tank is a mechanical object with a chassis. Shock it and it rings
   audibly — a real behaviour, not a defect, and worth exposing as a feature.

## Measurable targets

| Target | Value | Tolerance | Derived from |
| --- | --- | --- | --- |
| RT60, long-decay tank | 3.4 s | ±0.6 s (the datasheet range is 2.75–4.0 s) | [1][3] |
| RT60, medium-decay tank | 2.4 s | ±0.6 s | [1] |
| Echo spacing `T_D`, long tank | 52.7 ms | ±15% | derived, [8] eq. (2) on [8]'s own caliper data |
| Single-pass delay, long tank | 26.3 ms | ±15% | `T_D / 2` — `T_D` is a round trip |
| Transition frequency `F_C` | 3.8–4.5 kHz | ±15% | derived, [8] eq. (4) |
| Input impedance magnitude | 8 Ω at 1 kHz, rising ~6 dB/octave | ±20% | [3] |
| Output impedance | 2.25 kΩ at 1 kHz | ±10% | [3] |
| Nominal drive current | 28 mA RMS | ±10% | [3] |

## Open Questions

- [ ] **Spring count on 4AB3C1B.** [3] and [7] both say 2 springs; some retail listings
      and forum posts describe Fender blackface tanks as 3-spring. Type 4 is a two-spring
      type per [1][2], so 2 is the reading we use. Worth confirming against a physical
      tank. — blocks: nothing (SPEC-001 makes spring count a parameter)
- [ ] **Type 9 spring count.** [1] describes it as 3-spring; another source describes 6
      springs counter-wound and coupled in parallel pairs [2]. These may be different
      revisions. — blocks: nothing yet
- [x] **Single-pass delay time.** Closed by derivation: [8] eq. (2) gives echo spacing
      from spring geometry, and [8] published caliper measurements of five real springs
      from two guitar-amp tanks. No longer an inference — but see the next item.
- [x] **Dispersion cutoff frequency.** Closed by derivation: [8] eq. (4). Comes out at
      2.5–4.9 kHz across the measured springs, which independently agrees with [9]'s
      remark that an `F_C` above 5 kHz "is rarely seen in real spring reverberators".
- [ ] **Accutronics' own spring geometry.** The two items above are derived from *other
      manufacturers'* tanks, because nobody in the literature has calipered an
      Accutronics. The Long 2-Spring configuration in SPEC-001 is interpolated for that
      reason. Four micrometer readings — helix length, helix diameter, turns, wire
      diameter — close it with no fitting. — blocks: nothing, but it would move this
      study to `high`
- [ ] **Series or parallel?** Both units measured in [8] put their springs in parallel,
      and birbverb models parallel. If Accutronics puts them in series, echo spacing
      roughly doubles. — blocks: nothing (SPEC-001 makes it a parameter)
- [ ] Letter-to-impedance tables for Type 8 and Type 9 were only partially retrieved [1].

## Sources

1. [Accutronics Products and Specifications](https://www.amplifiedparts.com/tech-articles/accutronics-products-and-specifications) — secondary (reproduces the manufacturer tables)
2. [Spring Reverb Tanks Explained and Compared](https://www.amplifiedparts.com/tech-articles/spring-reverb-tanks-explained-and-compared) — secondary
3. [Fender Parallel 12AT7 Reverb Driver](https://www.ampbooks.com/mobile/classic-circuits/reverb/) — secondary (SPICE analysis with datasheet figures)
4. [Fender Reverb Unit](https://en.wikipedia.org/wiki/Fender_Reverb_Unit) — secondary
5. [Accutronics reverb tank specifications and part numbering system](https://www.guitarrepair.com/accutronics%20spring%20reverb%20specifications.html) — secondary
6. [How Spring Reverb Works](https://robrobinette.com/How_Spring_Reverb_Works.htm) — secondary
7. [Reverb Tank — Accutronics 4AB3C1B, 8 ohm long decay, 2-spring](https://modulusamplification.com/reverb-tank---accutronics-4ab3c1b-8-ohm-long-decay-2-spring-3363-p.asp) — anecdotal (retail listing, used here only to corroborate spring count)
8. S. Bilbao and J. Parker, "Spring Reverberation: A Physical Perspective", Proc. DAFx-09, Como, Italy, Sept. 2009, pp. 416–421 — **primary** (closed forms for echo spacing and transition frequency; calipered geometries of two real tanks in Appendix A)
9. J. Parker, "Efficient Dispersion Generation Structures for Spring Reverb Emulation", EURASIP J. Adv. Signal Process. 2011:646134, CC BY — **primary** (allpass cascade group delay, chirp series levels, typical `F_C` range)

## Confidence

`medium` — the electrical specifications are well sourced from datasheet-derived figures.
The two numbers that matter most to the DSP, echo spacing and transition frequency, are
now **derived from closed forms in primary sources** [8][9] rather than inferred, but
applied to *other manufacturers'* calipered springs, because no published measurement of
an Accutronics tank exists. Four micrometer readings would move this to `high`. Working
through the consequences:
[spring-dispersion-and-allpass-fitting](../deep-dives/spring-dispersion-and-allpass-fitting.md).
