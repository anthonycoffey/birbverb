---
id: SPEC-002
title: 'Tube drive and recovery stages'
status: draft
created: 2026-10-29
author: Anthony Coffey
reviewers: []
affected_repos: ['birbverb']
---

## Reviewer Notes

<!-- Leave empty until code review. When requesting changes, reviewer adds feedback here: -->

---

# Feature: Tube drive and recovery stages

## Problem

The tank is the same part in every amp we model. **What differs is everything around it.**

- A Fender AB763 drives the tank with both halves of a 12AT7 through a step-down
  transformer, after a 500 pF cap has stripped the bass.
- A Fender 6G15 drives it with a **6K6 power tube** — far more current, far more splash.
- An Ampeg Reverberocket drives it with **one triode and no transformer at all** — far
  less current, a darker and softer excitation.

If birbverb models only the tank, every preset sounds the same with different decay
times. This spec is where the four units actually become four units.

Research: [fender-ab763-reverb](../../documentation/circuits/fender-ab763-reverb.md),
[fender-6g15-reverb-unit](../../documentation/circuits/fender-6g15-reverb-unit.md),
[fender-6g16-vibroverb](../../documentation/circuits/fender-6g16-vibroverb.md),
[ampeg-reverberocket](../../documentation/circuits/ampeg-reverberocket.md).

## Requirements

### Must have

1. WHEN a circuit model is selected, the system SHALL configure the send high-pass, driver
   type, transformer coupling, recovery gain and mix network for that model.
2. WHEN the AB763 model is active, the send high-pass SHALL have a corner at 318 Hz ±15%
   and SHALL attenuate 82 Hz by 12 dB ±2 dB.
3. WHEN the 6G15 model is active, the send high-pass corner SHALL be 300 Hz ±15%.
4. WHEN any transformer-coupled model is active, the tank SHALL be presented with a
   **reactive load whose magnitude rises with frequency**, so bass draws more current than
   treble for the same drive voltage.
5. WHEN the Ampeg model is active, the system SHALL model a transformerless driver, and
   the tank drive current SHALL be below 25% of the AB763 model's nominal drive at the
   same input level.
6. WHEN the 6G15 model is active at maximum dwell, total harmonic distortion in the wet
   path SHALL exceed that of the AB763 model at its maximum drive.
7. WHEN `dwell` is increased, the drive stage SHALL move from linear into saturation, and
   the onset of audible compression SHALL occur near the tank's nominal drive current of
   28 mA RMS equivalent.
8. WHEN the recovery stage is driven hard, it SHALL clip asymmetrically, as a triode
   stage does, rather than symmetrically.
9. WHEN the AB763 mix network is active, the wet signal SHALL be injected with a gentle
   high-frequency tilt, modelling the 10 pF cap across the 3.3 MΩ mix resistor.
10. WHEN the 6G16 mix network is active, the wet signal SHALL have no high-frequency tilt
    and SHALL arrive at a higher level than AB763 for the same recovery output.
11. WHEN any nonlinear stage is active, aliasing products SHALL stay at least 60 dB below
    the fundamental for a full-scale 1 kHz input.
12. WHEN the `tone` control is swept, it SHALL affect the **wet path only**.

### Nice to have

- Transformer core saturation modelled separately from tube saturation; they behave
  differently and the 6G15 has both.
- A `circuit` model for the 6G15 reissue, which uses a 12AX7 input, a 6V6 driver and an
  extra buffer — a different sound and a common point of confusion.

### Non-goals (what this does NOT do)

- This spec does NOT model the tank. That is SPEC-001.
- This spec does NOT model the amp's preamp, tone stack, phase inverter, output stage or
  speaker. birbverb is a reverb circuit, not an amp sim. The player's dry signal passes
  through unprocessed except for the mix network.
- This spec does NOT model the 1990s 6G15 reissue in v0.1 (see Nice to have).
- This spec does NOT claim component-accurate recovery stages. The recovery stage plate
  and cathode values are an open question in the AB763 study; the recovery model is fitted
  to gain and clipping behaviour, not to components.

## Design

Four circuit models behind one interface, selected by the `circuit` parameter
([SPEC-003](SPEC-003-parameter-model.md)).

```
dry ──────────────────────────────────────┬─────────────→ mix network ──→ out
                                          │                   ↑
send → [HPF] → [driver] → [coupling] → tank → [recovery] → [tone] ──┘
       318Hz    12AT7      transformer        12AX7
       /300Hz    /6K6       /direct
                 /6SN7
```

### Components

| Component | File | Responsibility |
| --- | --- | --- |
| `SendFilter` | `Source/dsp/SendFilter.h` | The 500 pF / 1 MΩ high-pass before the driver; corner is a parameter |
| `TriodeStage` | `Source/dsp/TriodeStage.h` | Asymmetric triode transfer with grid conduction; antiderivative anti-aliased |
| `PentodeStage` | `Source/dsp/PentodeStage.h` | 6K6-style power stage for the 6G15 driver |
| `TankLoad` | `Source/dsp/TankLoad.h` | Frequency-dependent reactive load: 8 Ω at 1 kHz rising ~6 dB/octave, 0.81 Ω DCR |
| `DriveTransformer` | `Source/dsp/DriveTransformer.h` | Step-down coupling with optional core saturation; 22.8 kΩ : 8 Ω |
| `RecoveryStage` | `Source/dsp/RecoveryStage.h` | Triode recovery gain with asymmetric clipping |
| `MixNetwork` | `Source/dsp/MixNetwork.h` | Per-model wet injection: AB763 (3.3 MΩ + 10 pF), 6G16 (resistive), 6G15 (250 kΩ linear pot) |
| `CircuitModel` | `Source/dsp/CircuitModel.h` | Named presets of the above; one per researched unit |

### Circuit model table

Values from the circuit studies. Values marked *(inferred)* or *(fitted)* are not sourced.

| | AB763 | 6G16 | 6G15 | Ampeg |
| --- | --- | --- | --- | --- |
| Send HPF | 318 Hz | 318 Hz *(inferred — borrowed from AB763)* | 300 Hz | *(inferred)* |
| Driver | 12AT7, both halves parallel | 12AX7? *(unresolved)* | 6K6GT power pentode | 6SN7, one half |
| Transformer | 22.8 kΩ : 8 Ω, 2850:1 | assumed as AB763 *(inferred)* | different primary, value unknown | **none** |
| Drive for nominal tank current | ~460 mV peak at grids | *(inferred)* | *(unknown)* | n/a |
| Recovery | 12AX7 half | 12AX7 half *(inferred)* | 12AX7 half | 6SN7 other half |
| Mix network | 3.3 MΩ + 10 pF, 1.5 MΩ attenuation | ~470 kΩ resistive | 250 kΩ linear pot | *(inferred)* |
| Dwell control | not fitted | possibly rear-panel | yes | not documented |
| Wet tone control | none | none | yes | not documented |
| Character | bright, splashy, sits behind the dry | darker, more present | heavy drip, saturating | weak, dark, diffuse |

### The send filter is a first-order high-pass, and the sources agree

The two sourced figures for the AB763 send — a 318 Hz corner and −12 dB at 82 Hz — are
mutually consistent with a **single-pole** high-pass. Measured with the test harness, a
TPT one-pole high-pass at 318 Hz gives −12.09 dB at 82 Hz.

That matters for two reasons. It is a cross-check that the two independently sourced
numbers describe the same circuit behaviour rather than conflicting, and it means
requirement 2's pair of criteria can be satisfied by a first-order filter without any
fitting. If a later implementation needs a steeper slope to hit one of them, that is a
signal that one of the two source figures is wrong.

### Anti-aliasing

Nonlinear stages run inside 2x oversampling, with antiderivative anti-aliasing (ADAA) on
the tube transfer functions. Requirement 11's 60 dB figure is the target this must hit;
if 2x cannot, raise to 4x for the pentode stage only rather than globally.

## Edge cases

- [ ] Circuit model switched during playback — no click, no stale filter state
- [ ] `dwell` at zero — wet path silent, not a quiet mess of noise
- [ ] Extremely hot input (+12 dBFS) — stages clip gracefully, nothing blows up
- [ ] DC offset in the input — must not bias the tube stages into permanent asymmetry
- [ ] Ampeg model with `dwell` at maximum — must still be quieter than AB763 at maximum,
      since that ordering is the point of the model
- [ ] Sample rates where 2x oversampling exceeds 192 kHz internal — cap the oversampling
      factor rather than the sample rate

## Acceptance criteria

1. AB763 send high-pass: −3 dB at 318 Hz ±15%, −12 dB at 82 Hz ±2 dB.
2. 6G15 send high-pass: −3 dB at 300 Hz ±15%.
3. `TankLoad` impedance magnitude: 8 Ω ±20% at 1 kHz, rising within ±1 dB of 6 dB/octave
   across 200 Hz–5 kHz, approaching 0.81 Ω ±20% at DC.
4. Ampeg model tank drive current at a reference input level is below 25% of the AB763
   model's at the same level.
5. 6G15 model wet-path THD at maximum dwell exceeds AB763 model wet-path THD at maximum
   drive, measured with a 1 kHz full-scale sine.
6. `TriodeStage` clipping is asymmetric: positive and negative peak excursions differ by
   at least 10% at 6 dB above the linear region.
7. AB763 mix network wet output has higher energy above 3 kHz than below 500 Hz, relative
   to the 6G16 mix network's ratio.
8. 6G16 mix network wet level exceeds AB763's by 6–17 dB for identical recovery output.
9. Aliasing: 1 kHz full-scale sine through the full drive chain, all inharmonic products
   at least 60 dB below the fundamental.
10. `tone` sweep across its full range changes the wet path spectrum and leaves the dry
    path bit-identical.
11. All four circuit models: 60 s of full-scale noise, output finite, peak below
    +12 dBFS.
12. CPU: full drive + recovery chain under 4% of one core at 48 kHz, 2x oversampled.

## Constraints

- Header-only, no JUCE, `namespace bv`.
- No allocation on the audio thread; oversampling buffers allocated in `prepare()`.
- Every constant in `CircuitModel.h` carries a comment naming the circuit study and the
  source number it came from, or the word `INFERRED` / `FITTED`. This is enforced by
  review, not by a test, and it is the single most important convention in the codebase.
- Combined CPU budget with SPEC-001: under 6% of one core at 48 kHz.

## Tasks

- [ ] Resolve the 6G16 driver tube question, or record it as inferred
- [ ] Attempt to source the 6G15 Tone control values from the StewMac kit manual
- [ ] Attempt to retrieve an Ampeg Reverberocket schematic
- [ ] RED: `tests/test_send_filter.cpp` — magnitude response assertions
- [ ] GREEN: `SendFilter`
- [ ] RED: `tests/test_tank_load.cpp` — impedance magnitude versus frequency
- [ ] GREEN: `TankLoad`, `DriveTransformer`
- [ ] RED: `tests/test_tube_stages.cpp` — asymmetry, aliasing floor, compression onset
- [ ] GREEN: `TriodeStage`, `PentodeStage`, `RecoveryStage` with ADAA + 2x oversampling
- [ ] RED: `tests/test_mix_network.cpp` — per-model tilt and level ordering
- [ ] GREEN: `MixNetwork`
- [ ] RED: `tests/test_circuit_models.cpp` — the cross-model ordering assertions (4, 5, 8)
- [ ] GREEN: `CircuitModel` configurations for AB763, 6G16, 6G15, Ampeg
- [ ] REFACTOR: audit every constant for its source comment
- [ ] CPU measurement against the budget

## Notes

- The cross-model **ordering** assertions (criteria 4, 5, 8) are the most valuable tests
  in this spec. Absolute values depend on research we do not have, but the orderings are
  well attested and they catch the failure that would actually matter: an Ampeg preset
  that drips like a Twin, or a 6G16 that is brighter than a blackface.
- The Ampeg column has the most inferred entries. That is honest, and the preset
  description in [SPEC-004](SPEC-004-preset-bank-and-foundation-presets.md) says so.
