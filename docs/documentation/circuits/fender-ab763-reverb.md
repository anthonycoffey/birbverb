---
id: CIRCUIT-002
title: 'Fender AB763 Reverb Circuit'
hardware: 'Fender Twin Reverb, Super Reverb, Vibroverb (AB763 blackface)'
years: '1963–1967'
status: draft
updated: 2026-10-29
author: Anthony Coffey
confidence: medium
---

# Circuit Study: Fender AB763 Reverb Circuit

> The reference circuit. AB763 is the blackface chassis shared across the Twin Reverb,
> Super Reverb, Deluxe Reverb, Vibrolux Reverb and the 1964 Vibroverb, and it is the
> reverb most players mean when they say "Fender reverb".

## Summary

A small-signal triode pair drives a transformer into an 8 Ω spring tank; a 12AX7-type
recovery stage brings the weak return back up, and the wet signal is injected into the
dry path through a very large mix resistor with a small bright cap across it. The
character comes from three things: the 500 pF coupling cap that strips the bass before
the tank, the long-decay two-spring tank, and the extremely high-impedance mix network
that makes the reverb sit *behind* the dry signal rather than beside it.

## Signal Path

```
preamp → 500pF ──┬─→ 12AT7 (both halves in parallel) → reverb transformer → tank (4AB3C1B)
                 │                                                             │
                 │                                                             ↓
dry ─────────────┴──────────────── 3.3M mix resistor ←─ reverb level pot ←─ 12AX7 recovery
                                   (10pF bright cap in parallel)
                                          │
                                          ↓
                                   to the tone stack / PI
```

## Stages

### Send / high-pass into the driver

| Property | Value | Source |
| --- | --- | --- |
| Coupling capacitor into the driver grids | 500 pF | [1][3] |
| Driver grid leak | 1 MΩ | [2] |
| Resulting high-pass corner | ≈ 318 Hz | [2] |
| Response at 82 Hz (low E) | ≈ −12 dB | [1] |

This is the most consequential single component in the circuit. The tank's input
impedance rises with frequency, so without the 500 pF cap the bass would dominate the
tank drive [1]. Fender's own description of the problem, paraphrased: low frequencies
turn to mud when reverb is applied [3].

A **dwell** control is a stock mod here: replace the driver's 1 MΩ grid leak with a 1 MΩ
pot [2]. That is a useful reference point for us, because it means "dwell" and "how hard
the tank is driven" are the same parameter, and the AB763 just doesn't expose it.

### Reverb driver

Both halves of a 12AT7 wired **in parallel** into a small output transformer [1][3]. The
transformer trades voltage for current, because the tank's input transducer needs current
to move the springs [3].

| Property | Value | Source |
| --- | --- | --- |
| Transformer (Fender 125A20B; Hammond 1750A equivalent) primary | 22.8 kΩ | [1] |
| Transformer secondary | 8 Ω | [1] |
| Impedance ratio | 2850:1 | [1] |
| Turns ratio | ≈ 53.4:1 (datasheet 53.11) | [1] |
| Primary inductance | ≈ 57 H | [1] |
| Grid-to-cathode bias | −8.2 V simulated, −8.6 V measured by Fender | [1] |
| Plate-to-cathode | 449.8 V simulated, 431.4 V measured by Fender | [1] |
| DC plate current | 1.86 mA per triode | [1] |
| Effective plate impedance | 43 kΩ per triode, 21.5 kΩ for the pair | [1] |
| Drive needed for nominal tank current at 1 kHz | ≈ 447–477 mV peak at the grids | [1] — see Open Questions |
| Secondary at nominal drive | 225 mV RMS, 28 mA RMS, current lagging voltage by ~90° | [1] |
| Signal at the input jack for full tank drive | ≈ 25 mV peak (−35 dBV), controls at noon | [1] |

That last row is the one that matters musically: **the tank is driven into its nominal
range by a fairly quiet guitar signal.** A hot pickup or a boosted signal overdrives the
driver and the transformer, which is where the splash and compression of a cranked
Fender reverb comes from.

Note the current lagging voltage by nearly 90° [1] — the load really is dominantly
reactive, and a model that treats the tank input as a resistor will get the drive
spectrum wrong.

Driver tube swaps are common and tell us about the gain structure: a 12AU7 in place of
the 12AT7 reduces reverb gain, makes the control easier to dial in, and lowers noise
[2]. A 12U7 is also used to quiet the reverb [2].

### Tank

Accutronics `4AB3C1B` — 8 Ω in, 2.25 kΩ out, long decay (2.75–4.0 s), two springs,
horizontal mount. See [accutronics-spring-tanks](accutronics-spring-tanks.md).

### Recovery

One half of a 12AX7 / 7025 [1][3]. The return from the tank is weak, and this stage
restores it before the reverb level pot [3].

### Mix network

The dry signal is tapped from the **input** side of a 3.3 MΩ reverb mix resistor and the
wet signal is injected on the **other** side, with a 10 pF capacitor in parallel with the
mix resistor acting as a bright cap [3].

| Property | Value | Source |
| --- | --- | --- |
| Mix resistor | 3.3 MΩ | [3] |
| Bright cap across the mix resistor | 10 pF | [3] |
| Reverb attenuation resistor (AB763) | 1.5 MΩ, where other revisions use 470 kΩ | [2] |

The 1.5 MΩ attenuation resistor is what gives AB763 its particular reverb-pot taper —
[2] describes raising the resistor as reducing the wet signal and slowing the pot's
response. A model that wants an authentic *feel* on the Mix control needs this, not a
linear blend.

## Controls

| Control | Range / taper | What it actually changes | Source |
| --- | --- | --- | --- |
| Reverb | 1 MΩ pot after the recovery stage | How much recovered wet signal reaches the mix resistor. Not a dry/wet balance — the dry path is untouched. | [3] |
| (Dwell) | not fitted; 1 MΩ pot in the driver grid leak position when modded | How hard the driver pushes the tank | [2] |

**The AB763 has no dwell and no reverb tone control.** One knob. Everything else about
how the reverb sits is fixed by the component values above. That is worth preserving in
the preset: `Twin '64` should *land* on a convincing tone with Dwell and Tone at their
circuit-correct positions, and let the player move off it from there.

## What gives this circuit its character

1. **Bass is gone before the tank.** 318 Hz high-pass, −12 dB at low E [1][2]. The wet
   signal is a treble effect sitting on top of a full-range dry signal.
2. **Reverb sits behind the dry, not alongside it.** A 3.3 MΩ mix resistor with a 10 pF
   bright cap [3] is a very gentle, very high-impedance injection with a treble tilt.
3. **The driver clips before the amp does.** Nominal tank drive from ~25 mV at the jack
   [1] means normal playing already works the driver hard, and hard picking splashes.
4. **Long two-spring decay.** 2.75–4.0 s [1], two springs, so the echo train is sparse
   and distinctly "boingy" rather than diffuse.

## Measurable targets

| Target | Value | Tolerance | Derived from |
| --- | --- | --- | --- |
| Send high-pass corner | 318 Hz | ±15% | [2] |
| Send response at 82 Hz | −12 dB | ±2 dB | [1] |
| Mix-network wet tilt | gentle HF lift from the 10 pF across 3.3 MΩ | assert wet HF > wet LF after mixing | [3] |
| Tank RT60 | 3.4 s | ±0.6 s | [1] |
| Driver onset of audible compression | tank current > 28 mA RMS | ±10% | [1] |

## Open Questions

- [ ] **447 mV vs 477 mV peak at the driver grids.** [1] gives both figures on the same
      page — 447 mV in the prose and 477 mV in the simulation. Use 460 mV ±5% until
      resolved. — blocks: nothing (the drive calibration is a single constant)
- [ ] **Recovery stage plate load and cathode values.** Not given by any source
      consulted. Needed for an accurate recovery-stage frequency response and clipping
      threshold. — blocks: SPEC-002 acceptance criterion on recovery gain
- [ ] **Whether the Twin Reverb and Super Reverb reverb circuits differ at all.** Both
      are AB763; the differences between the models are in the power and preamp sections,
      not documented as reverb differences. Assumed identical. — blocks: whether
      `Twin '64` and `Super '63` can share a circuit model and differ only in preset
      values (SPEC-004 currently assumes yes)
- [ ] Secondary inductance of 20 mH in [1] is explicitly described as arbitrarily chosen,
      so transformer resonance behaviour from that source is not trustworthy.

## Sources

1. [Fender Parallel 12AT7 Reverb Driver](https://www.ampbooks.com/mobile/classic-circuits/reverb/) — secondary (SPICE analysis using Andre Adrian's large-signal 12AT7 model)
2. [AB763 Fender Amp Modifications Guide](https://www.scribd.com/document/739368105/AB763-Mods) — secondary
3. [How Spring Reverb Works](https://robrobinette.com/How_Spring_Reverb_Works.htm) — secondary
4. [AB763 Blackface Amp Model Differences](https://robrobinette.com/AB763_Model_Differences.htm) — secondary
5. [Fender Super Reverb](https://en.wikipedia.org/wiki/Fender_Super_Reverb) — secondary
6. [Fender Twin](https://en.wikipedia.org/wiki/Fender_Twin) — secondary

## Confidence

`medium` — the driver stage is very well characterised, the tank is documented, and the
mix network values are consistent across sources. The recovery stage is the gap.
