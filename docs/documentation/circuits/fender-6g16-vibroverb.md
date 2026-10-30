---
id: CIRCUIT-003
title: 'Fender Vibroverb 6G16'
hardware: 'Fender Vibroverb 6G16 (brown tolex, 2x10)'
years: '1963'
status: draft
updated: 2026-10-29
author: Anthony Coffey
confidence: low
---

# Circuit Study: Fender Vibroverb 6G16 (1963)

> The first Fender amp with reverb built in, and a one-year circuit. It sits between the
> outboard 6G15 unit and the blackface AB763, and it is not simply an early AB763 — the
> mixer is different, and that difference is audible.

## Summary

The 1963 Vibroverb (6G16, brown tolex, 2x10) brought the outboard Reverb Unit's idea
inside the amp. Its reverb differs from the blackface AB763 that replaced it most
clearly in the **mix network**: 6G16 uses a simple resistive mix where AB763 uses a
3.3 MΩ resistor with a 10 pF bright cap in parallel [1][2]. Reports describe the 6G16
mixer as darker and the AB763 as wetter [1]. Supply voltages and dropping resistors
differ as well, and the tube in the reverb position is reported as a 12AX7 in some
6G16 documentation where AB763 uses a 12AT7 driver [3].

**This study is `low` confidence.** Nearly everything specific to 6G16 below comes from
forum discussion rather than a factory schematic, and 1963 was a transition year with
reported prototype variants. Treat the numbers as leads, not specifications.

## Signal Path

```
preamp → [send HPF] → [driver] → [reverb transformer] → tank
                                                          │
dry ──────────────── resistive mix ←── reverb pot ←── recovery
```

The structure matches AB763. The documented differences are in the mixer and the supply.

## Stages

### Mix network — the documented difference

| Property | 6G16 | AB763 | Source |
| --- | --- | --- | --- |
| Mix topology | Simple resistive mix (470 kΩ reported) | 3.3 MΩ with 10 pF in parallel | [1] |
| Reported tonal result | Darker | Wetter | [1] |
| Additional element | — | A 220 kΩ to ground is described as an AB763 addition | [2] |

A 470 kΩ resistive mix is roughly **seven times lower impedance** than AB763's 3.3 MΩ,
and it has no bright cap. Both of those push in the same direction: more wet signal
reaching the mix point, with no treble tilt. That is a plausible mechanism for the
"darker but more present" descriptions, and it is the defining characteristic we should
build the `Vibroverb '63` preset around.

### Supply and tube complement

Forum discussion points to differing B+ voltages, different dropping resistors, and a
12AX7 rather than a 12AT7 in the reverb position [3]. A 12AX7 in the driver slot would
mean a higher-gain, lower-current driver — less able to push the tank hard, which again
points toward a softer, darker reverb than AB763.

A 1963 transition unit with brown tolex is reported to have carried early Vibroverb
reverb circuitry with **an extra tube and a rear-panel dwell control** [4]. If that is
the production 6G16 arrangement rather than a prototype one-off, then 6G16 is much closer
to the 6G15 outboard unit than to AB763, and the preset should reflect that. This is the
single most important open question in this study.

### Tank

Not documented in the sources consulted. Assumed to be an Accutronics Type 4 long-decay
tank consistent with other Fender amps of the period — see
[accutronics-spring-tanks](accutronics-spring-tanks.md). This is an **inference**.

## What gives this circuit its character

1. **A low-impedance, un-brightened mixer.** The reverb arrives darker and more
   integrated with the dry signal than in a blackface amp [1].
2. **Probably a weaker driver.** If the reverb position really is a 12AX7 [3], the tank
   is excited less forcefully, so less drip and splash and more of a soft wash.
3. **Possibly a dwell control.** [4] If so, the 6G16 is the Fender amp that lets you set
   tank drive, which is the one control blackface players always wish they had.

## Measurable targets

| Target | Value | Tolerance | Derived from |
| --- | --- | --- | --- |
| Wet HF tilt relative to AB763 | darker; no HF lift at the mix point | assert wet HF/LF ratio below the AB763 model's | [1] |
| Wet level at the mix point relative to AB763 | higher for the same recovery output | +6 to +17 dB (the 3.3 MΩ / 470 kΩ ratio) | inference from [1] |
| Send high-pass corner | unknown; AB763's 318 Hz used as the placeholder | — | inference |

## Open Questions

- [ ] **Does the production 6G16 have a dwell control and an extra reverb tube?** [4]
      describes a 1963 brown-tolex transition unit that way, without saying whether that
      was the production arrangement or a prototype. This changes the preset's control
      layout. — blocks: SPEC-004 `Vibroverb '63` preset definition
- [ ] **Is the mix resistor really 470 kΩ?** Single anecdotal source [1]. — blocks:
      SPEC-002 acceptance criterion on per-model mixer impedance
- [ ] **Is the reverb-position tube a 12AX7 or a 12AT7?** [3] is a forum thread that does
      not settle it. — blocks: nothing (drive headroom is a model parameter)
- [ ] **Tank model number.** Not documented. Currently an inference from period practice.
- [ ] **Send high-pass value.** Not documented. Currently borrowed from AB763.

**Resolution path for all five:** a 6G16 factory schematic. Fender's published schematic
archive or a reputable schematic library would close most of this study in one pass.
Until then `Vibroverb '63` is described in SPEC-004 as *"voiced after"* the 6G16 rather
than as a model of it.

## Sources

1. [Fender reverb circuit ques.](https://www.thegearpage.net/board/index.php?threads/fender-reverb-circuit-ques.1867181/) — anecdotal
2. [6G16 Reverb vs. AB763 — The Amp Garage](https://ampgarage.com/forum/viewtopic.php?t=11416) — anecdotal
3. [Circuit differences between 6G16 Vibroverb and AB763](https://www.tdpri.com/threads/circuit-differences-between-6g16-vibroverb-and-ab763.1142242/) — anecdotal
4. [Fender Twin](https://en.wikipedia.org/wiki/Fender_Twin) — secondary
5. [AB763 Blackface Amp Model Differences](https://robrobinette.com/AB763_Model_Differences.htm) — secondary (describes a modified AB763 build, not a stock 6G16)

## Confidence

`low` — one documented difference from AB763 (the mixer), sourced anecdotally, and an
unresolved question about whether the circuit even has the same control set.
