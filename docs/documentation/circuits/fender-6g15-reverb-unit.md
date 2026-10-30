---
id: CIRCUIT-004
title: 'Fender Reverb Unit 6G15'
hardware: 'Fender Reverb Unit, 6G15 (outboard tube spring reverb)'
years: '1961–1966'
status: draft
updated: 2026-10-29
author: Anthony Coffey
confidence: medium
---

# Circuit Study: Fender Reverb Unit 6G15 (outboard)

> Not one of the amps in the original brief, but it belongs in the bank: it is the
> extreme end of the same design language. A **power tube** drives the tank, and it has
> the Dwell control the amps don't. Everything the amps hint at, this unit does loudly.

## Summary

The outboard 6G15 has three controls — **Dwell, Mix and Tone** — and a three-tube
complement: a 12AT7 preamp, a **6K6 power tube** as the reverb driver, and a 12AX7
recovery stage [1]. Dwell sets how much signal reaches the drive circuit [2]. The drive
circuit itself is the second half of the 12AT7 into a high-pass that rolls off content
below about 300 Hz, then the 6K6, then the reverb transformer [2]. Recovery is one half
of the 12AX7, and Mix is a 250 kΩ **linear-taper** pot blending the dry path with the
recovery output [2].

Using a power tube rather than a small-signal triode to drive the tank is the whole
story. It can slam the tank far harder than an AB763 driver, which is where the splatter,
drip and bloom of the surf sound come from.

## Signal Path

```
input → 12AT7 (first half, preamp) → Dwell pot → 12AT7 (second half) → HPF (~300 Hz)
      → 6K6GT power tube → reverb transformer → tank
                                                 │
                                                 ↓
dry ──────────────── Mix pot (250k linear) ←─ 12AX7 recovery → Tone → output
```

## Stages

### Preamp and Dwell

| Property | Value | Source |
| --- | --- | --- |
| Preamp tube | 12AT7, first half | [1] |
| Dwell control | Sets how much signal reaches the drive circuit | [2] |

Dwell is upstream of the driver, which makes it a genuine **tank drive** control: turning
it up does not make the reverb louder in the mix, it makes the tank work harder. That
distinction matters for our parameter model — see
[SPEC-003](../../specs/active/SPEC-003-parameter-model.md).

### Driver

| Property | Value | Source |
| --- | --- | --- |
| Driver stage | 12AT7 second half → HPF → 6K6GT power tube → reverb transformer | [1][2] |
| High-pass corner before the driver | ≈ 300 Hz | [2] |
| Driver tube type | 6K6GT (power pentode) | [1] |

The ~300 Hz figure [2] is close enough to AB763's 318 Hz [see
[fender-ab763-reverb](fender-ab763-reverb.md)] that we can treat "high-pass the send
around 300–320 Hz" as a family trait rather than a per-model value.

On transformer selection, a builder note says a **lower primary impedance** with a 6K6 or
6V6 driver gives less distortion and somewhat better harmonic content [3] — which tells us
the stock arrangement is on the distorting side of that trade, deliberately or not.
A separate thread notes the 6G15 intends a different transformer primary than the
standard 12AT7 driver transformer [4].

### Recovery and Mix

| Property | Value | Source |
| --- | --- | --- |
| Recovery tube | 12AX7, one half | [2] |
| Mix pot | 250 kΩ, **linear** taper, resistive blend of dry and recovery output | [2] |

A linear-taper 250 kΩ resistive blend is a very different control from AB763's 1 MΩ pot
feeding a 3.3 MΩ mix resistor. It is a real dry/wet **balance**, and it is linear, so it
has the characteristic "nothing until about 11 o'clock, then all at once" feel players
describe. Worth modelling as a taper, not as a linear mix.

### Tone

A tone control sits in the wet path. Component values were not found in the sources
consulted — see Open Questions.

## Reissue warning

The 1990s-era reissue is **not** a faithful copy: it uses a 12AX7 in the input stage, a
6V6 driver, and an extra buffer stage [1]. If a measurement or a demo is sourced from a
reissue it does not describe this circuit. Our model targets the original 6G15.

## What gives this circuit its character

1. **A power tube drives the tank.** 6K6GT [1], not a 12AT7. Vastly more available
   current, so the tank can be driven into nonlinearity and the transformer into
   saturation. This is the "drip".
2. **Dwell is a real control.** The player can set tank drive independently of wet level
   [2], so the unit spans clean-and-dark to crashing-and-splattery.
3. **A linear Mix taper.** 250 kΩ linear [2] — the mix control has a distinctive,
   slightly awkward law that is part of how the unit plays.
4. **A wet-path Tone control.** The amps have no reverb tone control at all.

## Measurable targets

| Target | Value | Tolerance | Derived from |
| --- | --- | --- | --- |
| Send high-pass corner | 300 Hz | ±15% | [2] |
| Dwell range | clean tank drive through heavy driver/transformer saturation | assert THD at max dwell exceeds THD at max AB763 drive | [1][3] |
| Mix law | linear in resistance, not in perceived level | assert the −6 dB point sits above 50% rotation | [2] |

## Open Questions

- [ ] **Tone control component values.** Not found. Needed for the wet-path tone filter
      shape and range. — blocks: SPEC-002 acceptance criterion on the Tone control
- [ ] **Dwell pot value and taper.** Not found. — blocks: nothing (range is a model
      parameter)
- [ ] **Tank model.** The unit uses a long pan, but no part number was found in the
      sources consulted. [5] refers to a "long-pan spring tank" for the AB763 analysis;
      whether 6G15 uses the same type is unconfirmed. — blocks: nothing
- [ ] **Reverb transformer primary impedance.** [4] says the 6G15 intends a different
      primary than the standard 12AT7 driver transformer but does not give the value.
      — blocks: SPEC-002 acceptance criterion on driver saturation threshold

## Sources

1. [Fender Reverb Unit](https://en.wikipedia.org/wiki/Fender_Reverb_Unit) — secondary
2. [SurfGuitar101 Reverb FAQ](https://wiki.surfguitar101.com/ReverbFaq) — secondary
3. [6G15 build — Need help selecting transformers](https://www.tdpri.com/threads/6g15-build-need-help-selecting-transformers.231474/) — anecdotal
4. [Has anyone used a reverb driver as an output transformer?](https://www.tdpri.com/threads/has-anyone-used-a-reverb-driver-as-an-output-transformer.1148497/) — anecdotal
5. [Fender Parallel 12AT7 Reverb Driver](https://www.ampbooks.com/mobile/classic-circuits/reverb/) — secondary
6. [How Spring Reverb Works](https://robrobinette.com/How_Spring_Reverb_Works.htm) — secondary
7. [Fender '64 Reverb Unit kit assembly instructions](https://www.stewmac.com/globalassets/video-and-ideas/online-resources/building-instruments/64-reverb-unit-kit-instructions/download-64-reverb-unit-kit-instructions) — primary-adjacent (a kit manual for a reproduction; component values in it should be checked against a factory schematic before use)

## Confidence

`medium` — topology, tube complement and control functions are consistently documented
across two independent secondary sources. Component values for the Tone and Dwell
controls are the gap. [7] is the most promising unexplored lead for those values.
