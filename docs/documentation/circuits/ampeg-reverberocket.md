---
id: CIRCUIT-005
title: 'Ampeg Reverberocket / Echo Jet'
hardware: 'Ampeg Reverberocket R-12-R, Reverberocket 2, Echo Jet EJ-12'
years: '1961–1966'
status: draft
updated: 2026-10-29
author: Anthony Coffey
confidence: low
---

# Circuit Study: Ampeg Reverberocket / Echo Jet

> **Naming note.** The brief called this one the "Reverbojet / Echo Jet". Ampeg's actual
> model names in this family are the **Reverberocket** (R-12-R, 1961), the
> **Reverberocket 2** (late 1962 / early 1963), and the **Echo Jet** (EJ-12), plus a
> one-off **Jet Reverb** prototype built into a 1961 Jet. There is no Ampeg
> "Reverbojet". This study covers the Reverberocket family, which is the circuit that
> matters: it is the first production guitar amp with built-in reverb, and it beat
> Fender's Vibroverb to market by two years.

## Summary

The defining fact, and the reason this amp sounds nothing like a Fender: **there is no
reverb transformer.** A single tube drives the tank directly [3]. Ampeg used an
octal-base **6SN7** dual triode [2][3]. Without a step-down transformer to trade voltage
for current, the tank is excited far less forcefully than in a Fender, which produces a
softer, darker, more diffuse reverb with much less of the drip and splash the Fender
circuit is known for.

A closely related design built around a **12DW7** splits the two triodes by role: the
low-gain, high-current 12AU7-type half drives the tank and the high-gain 12AX7-type half
recovers the level lost through it [3]. The 6SN7 arrangement is described as the same idea
in an octal envelope, though the sources do not confirm Ampeg's exact wiring — see Open
Questions.

## History (relevant because the circuit moved)

| Date | Event | Source |
| --- | --- | --- |
| April 1961 | Prototype reverb built into an Ampeg Jet by Jess Oliver | [1][4] |
| May 1961 | Factory schematic for the first-edition **R-12-R**; reported identical to the prototype apart from minor component-value changes | [1] |
| July 1961 | Reverberocket debuts at the summer NAMM show | [1] |
| September 1962 | Revised schematic. **The reverb circuit itself changed** — see below | [7] |
| late 1962 / early 1963 | **R-12-R-A** — 7591 output tubes in place of 6V6, 5Y3 rectifier retained | [5][7] |
| later | **R-12-R-B** — 7591 output tubes, solid-state rectification | [7] |

The practical consequence: **"Reverberocket" names at least four different amps.** Any
measurement, demo or schematic needs its revision identified before it can be used. Note
also that Ampeg's plain **Rocket** (R-12) is a *different amp with no reverb*, which is
why the preset is named for the model designation rather than just "Rocket".

### The 1961 and September 1962 reverb circuits differ

This matters to us, because the differences are in the reverb chain and not in the power
amp. Reported by forum members comparing the two factory schematics [7]:

| | 1961 | September 1962 |
| --- | --- | --- |
| V3a plate resistor | 5.6 kΩ | 2.2 kΩ |
| V3a cathode resistor | 1.5 kΩ | 330 Ω |
| Reverb footswitch | on the tank's **output** side | on the tank's **input** side |
| Resistor beside the footswitch | 33 kΩ | 22 kΩ |
| V3b Dimension pot cap | absent | 0.0068 µF |
| V3a snubber cap | 0.01 µF | 0.02 µF |

Both V2 and V3 are **6SN7** in the reverb chain [7].

Read as a circuit: dropping the plate resistor from 5.6 kΩ to 2.2 kΩ while cutting the
cathode resistor from 1.5 kΩ to 330 Ω is a substantial **gain increase** with much less
cathode degeneration. The 1962 revision drives its tank harder and recovers hotter than
the 1961. Moving the footswitch from the tank output to the tank input changes what
happens when reverb is off: bypassing at the input leaves the tank undriven, while
bypassing at the output leaves it ringing.

**This is anecdotal**, from forum accounts rather than from Ampeg documents, and is
recorded here as the best available account rather than as established fact. birbverb
models the **1961** revision, because that is the one with a dated factory schematic
reference [1], and offers the 1962 voicing as a preset approximation rather than as a
second circuit model — the component values above do not justify a second model until a
primary source confirms them.

## Signal Path

```
preamp → [send] → 6SN7 (low-gain/high-current half) ──→ tank   [NO transformer]
                                                          │
dry ──────────────── mixer ←─ 6SN7 (high-gain half, recovery) ←┘
```

## Stages

### Driver — transformerless

| Property | Value | Source |
| --- | --- | --- |
| Driver tube | 6SN7, octal dual triode | [2][3] |
| Reverb transformer | **none** | [3] |
| Role split | Low-gain/high-current half drives the tank, high-gain half recovers | [3] (described for the analogous 12DW7 version) |

A triode driving a ~8 Ω reactive load directly is a severe impedance mismatch. The tank
sees a small fraction of the current a transformer-coupled driver delivers, and the
driver sees a load it cannot develop much voltage swing into. Both effects are part of
the sound, not problems to fix: the model should reproduce the *weak, bass-shy, slightly
compressed* excitation that results.

### Tank

Not documented in the sources consulted. Ampeg's tanks of the period were generally
shorter than Fender's long pans, which is consistent with the shorter, less boingy decay
the amps are described as having — but this is an **inference**, not a sourced fact.

## The Echo Jet is a different animal

Ampeg entered the **outboard** reverb market twice: the **Echo Satellite** in 1961 and the
**Echo Jet** in 1963 [8]. The Echo Jet is unusual in that it also works as a stand-alone
amp, and it is **not** simply a Reverberocket in a smaller box.

| Echo Jet EJ-12 | Value | Source |
| --- | --- | --- |
| Introduced | 1963 | [8] |
| Preamp | 1 x 6D10 (triple triode) | [9] |
| Output | 2 x 7591A, 18 W RMS into 8 Ω | [9] |
| Rectifier | 1 x 5Y3GT | [9] |
| Speaker | 1 x 12" CTS alnico | [9] |
| Controls | Volume, Tone, reverb. **No tremolo** | [8][9] |
| Revisions | EJ-12, EJ-12-A, and EJ-12-D briefly in late 1965 | [8] |

The signature, and the thing birbverb cannot currently reproduce: the Echo Jet's reverb
is **speaker-driven**, and its reverb speaker is **physically separate from the dry
speaker** — up to about 8 feet apart in use — which gives a quasi-stereo image [8]. The
wet path is not just a different filter; it is a different loudspeaker in a different
place.

Two consequences:

1. **A proper Echo Jet circuit model needs a schematic**, which we do not have. Every
   drive constant would be an invention, and a `low`-confidence circuit model whose
   values are all guesses is worse than no model — it would be the one thing in the repo
   that cannot trace to a source.
2. **Its defining spatial behaviour is out of scope for SPEC-001**, which models a mono
   tank. A wet/dry spatial split is listed in SPEC-001's Nice to have as a deliberate
   liberty and would need its own parameter.

So the Echo Jet appears in the bank as a **preset voiced on the Reverberocket circuit**,
with its description saying so. Promoting it to a circuit model is an open question below.

## What gives this circuit its character

1. **No transformer.** [3] The single most important difference from every Fender in the
   bank. Weak tank drive, so the springs never get slammed.
2. **One tube does both jobs.** [3] Drive and recovery share an envelope, so there is
   less gain available overall and the wet signal is quieter relative to dry.
3. **Darker and more diffuse.** A consequence of 1 and 2 rather than a documented
   measurement — our stated inference.
4. **It came first.** 1961 [1], two years before the Vibroverb. Worth saying in the
   preset description.

## Measurable targets

| Target | Value | Tolerance | Derived from |
| --- | --- | --- | --- |
| Tank drive current relative to the AB763 model | substantially lower | assert below 25% of the AB763 model's nominal 28 mA RMS | inference from [3] |
| Wet/dry ratio at maximum reverb | lower than any Fender model in the bank | assert ordering, not an absolute figure | inference from [3] |
| Decay | shorter than the Fender long-pan models | assert ordering | inference |

Every target here is an **ordering assertion** rather than an absolute value. That is
deliberate: with confidence this low, a test that pins an absolute number would be
asserting our guess rather than the hardware's behaviour. Ordering tests still catch the
bug that matters — a Reverberocket preset that drips like a Twin.

## Open Questions

- [ ] **Ampeg's actual 6SN7 wiring.** The drive/recovery role split is documented for the
      12DW7 analogue [3], not confirmed for the 6SN7 Ampeg circuit. — blocks: SPEC-002
      acceptance criterion on the transformerless driver model
- [x] **Which revision are we modelling?** **Decided: the first-edition R-12-R, May
      1961**, because it is the one revision with a dated factory schematic reference [1].
      The preset is named `Rocket R-12-R` for the model designation, since Ampeg's plain
      Rocket (R-12) is a different, reverb-less amp and "Rocket '61" read ambiguously.
      The September 1962 reverb revision is offered as the derived preset `Rocket '62`,
      described as an approximation, because its component values are anecdotal [7].
- [ ] **Confirm the 1962 revision from a primary source.** If the plate/cathode values in
      the table above are confirmed, `Rocket '62` earns promotion from a preset to a
      second circuit model. — blocks: nothing
- [ ] **Tank model and length.** Undocumented. — blocks: nothing (a model parameter)
- [x] **Echo Jet (EJ-12) reverb circuit.** **Answered, and the answer is that it is a
      different design**: 1963, 6D10 preamp, 2 x 7591A, and a speaker-driven reverb on its
      own physically separated speaker [8][9]. It is therefore NOT a Reverberocket in a
      smaller box, and it ships as a preset voiced on the Reverberocket circuit rather
      than as its own circuit model.
- [ ] **An Echo Jet schematic**, which would let the speaker-driven send become a real
      circuit model. Its quasi-stereo wet/dry separation would still need a new
      parameter — a liberty, per SPEC-001's Nice to have. — blocks: an `ampegEchoJet`
      circuit model
- [ ] **Echo Satellite (1961).** Ampeg's first outboard reverb [8]. Nothing retrieved
      beyond its existence. — blocks: nothing
- [ ] **A schematic.** [6] is a forum thread reported to link a Reverberocket schematic.
      Unretrieved. This is the highest-value next step for this study.

## Sources

1. [Ampeg's Jet Reverb Prototype](https://www.vintageguitar.com/27186/ampegs-jet-reverb-prototype/) — secondary
2. [The Ampeg Reverberocket is a Reverb-o-File's Dream](https://www.guitarplayer.com/gear/the-ampeg-reverberocket-is-a-reverb-o-files-dream) — secondary
3. [How to build your own spring reverb unit](https://guitar.com/guides/diy-workshop/build-tube-spring-reverb-unit-amplifier/) — secondary
4. [1961 Ampeg Jet Reverb Jess Oliver Prototype](https://reverb.com/item/3328430-1961-ampeg-jet-reverb-jess-oliver-prototype-for-reverberocket-1st-amp-with-reverb) — anecdotal (listing)
5. [Ampeg Reverberocket 1962](https://www.tdpri.com/threads/ampeg-reverberocket-1962.1037472/page-2) — anecdotal
6. [Oddball Ampeg Reverberocket schematic needed](https://music-electronics-forum.com/forum/amplification/guitar-amps/vintage-amps/48315-oddball-ampeg-reverberocket-schematic-needed) — anecdotal (reported to link a schematic; not yet retrieved)
7. [Ampeg Reverberocket R-12-R DIY project](https://www.tdpri.com/threads/ampeg-reverberocket-r-12-r-diy-project.1102984/) — anecdotal (forum members comparing the 1961 and September 1962 factory schematics; source of the revision table and the R-12-R / -A / -B designations)
8. [GGJaguar's Guitarium: 1965 Ampeg EchoJet](https://ggjaguar.com/ej-12.htm) — secondary (Echo Satellite 1961 / Echo Jet 1963 dates, speaker-driven reverb and the separated reverb speaker, EJ-12 revision list)
9. [Echo-Jet EJ-12A, Radiomuseum](https://www.radiomuseum.org/r/ampeg_echo_jet_ej_12aej12.html) — secondary (tube complement, power rating, speaker). Its "build 1960 ??" date is marked uncertain and conflicts with [8]'s 1963; [8] is preferred

## Confidence

`low` — the one structural fact that drives the model (no transformer, single tube) is
attested by a secondary source and is consistent with how the amp is described. Almost
everything else is inference, and the revision table is anecdotal. The measurable targets
are written as ordering assertions for exactly this reason.

Two things did improve on this pass: the revision we model is now **decided and named**
(first-edition R-12-R, May 1961), and the Echo Jet question is **answered** — it is a
separate design with a speaker-driven reverb, not a small Reverberocket. Neither raises
the confidence, because both rest on secondary and anecdotal sources; what they remove is
ambiguity about what we are claiming.
