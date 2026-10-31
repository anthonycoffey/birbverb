---
id: SPEC-003
title: 'Parameter model'
status: draft
created: 2026-10-29
author: Anthony Coffey
reviewers: []
affected_repos: ['birbverb']
---

## Reviewer Notes

<!-- Leave empty until code review. When requesting changes, reviewer adds feedback here: -->

---

# Feature: Parameter model

## Problem

birbverb needs a parameter set that is small enough to fit a pedal face, honest about
which controls model hardware and which do not, and complete enough that a preset can
capture an entire amp's reverb voice in one snapshot.

The hardware does not agree on what controls should exist. An AB763 Twin Reverb has
**one** reverb knob. The 6G15 has three. The Ampeg has whatever it has. The design
problem is to expose a superset that can be set to each unit's circuit-correct position,
so a preset is a *starting point* the player can then move off — which is the core idea
of the whole plugin.

## Requirements

### Must have

1. The system SHALL expose exactly the ten parameters in the table below, with those IDs,
   ranges, and defaults.
2. Every parameter SHALL be host-automatable and SHALL appear in the host's parameter
   list with its display name and unit.
3. WHEN a parameter is changed, the change SHALL be smoothed so no audible zipper noise
   or click is produced, including for the two choice parameters.
4. WHEN the plugin state is saved and restored, every parameter value and the current
   preset selection SHALL round-trip exactly.
5. Parameter IDs SHALL be defined in exactly one place, `Source/ParameterIDs.h`, and
   referenced by constant everywhere else.
6. WHEN `bypass` is engaged, the output SHALL be the unmodified dry signal, and the wet
   path SHALL continue to run so that re-engaging does not produce a silent gap.
7. The `circuit` parameter SHALL select the drive/recovery/mix topology per
   [SPEC-002](SPEC-002-tube-drive-and-recovery-stages.md), and `tank` SHALL select the
   tank configuration per [SPEC-001](SPEC-001-spring-tank-dsp-core.md).
8. Each parameter SHALL be documented in this spec as **modelled** (it corresponds to
   something in the hardware) or **an effect control** (it does not).
9. WHEN a plugin instance is created with no state to restore, the parameters SHALL take
   the defaults in the table below, the boot preset SHALL be selected, and the preset
   display SHALL show that preset's name **without** a modified marker.

### Nice to have

- Per-parameter tooltips in the UI drawn from the Notes column below.
- A modifier-click "reset to circuit-correct value for the current circuit model".

### Non-goals

- This spec does NOT define preset contents. That is
  [SPEC-004](SPEC-004-preset-bank-and-foundation-presets.md).
- This spec does NOT define UI layout. That is
  [SPEC-005](SPEC-005-sprite-based-pedal-ui.md).
- No MIDI learn, no parameter modulation, no tempo sync in v0.1.

## Design

### The parameter table

| ID | Display | Type | Range | Default | Modelled? | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| `circuit` | Circuit | choice | `AB763`, `6G16`, `6G15`, `Ampeg` | `AB763` | **modelled** | Selects the whole drive/recovery/mix topology. The one control that is not on any of the real units. |
| `tank` | Tank | choice | `Short 2-Spring`, `Long 2-Spring`, `Long 3-Spring` | `Long 2-Spring` | **modelled** | Accutronics tank type. Changes spring count, delay and default decay. |
| `dwell` | Dwell | float | 0–10, step 0.01 | 5.0 | **modelled** | How hard the driver pushes the tank. A real control on the 6G15; the AB763's standard mod point. **Not** wet level. |
| `decay` | Decay | float | 0.8–4.5 s, step 0.01, skew 0.8 | 3.4 | **modelled** | Tank RT60. A tank property rather than a knob, exposed because selecting between tanks is otherwise the only way to change it. |
| `tone` | Tone | float | 0–10, step 0.01 | 5.5 | **modelled** | Wet-path tone. A real control on the 6G15; absent on all three Fender amps. |
| `locut` | Lo Cut | float | 80–600 Hz, step 1, skew 0.5 | 318 | **modelled** | The send high-pass — the 500 pF coupling cap. 318 Hz is the AB763 value. Turning it down is "what if Fender had used a bigger cap". |
| `boing` | Boing | float | 0–10, step 0.01 | 5.0 | **effect control** | Dispersion depth. At 5.0 it sits at the fitted curve; above that it exaggerates the chirp beyond any real spring. |
| `mix` | Mix | float | 0–100%, step 0.1 | 28.0 | **modelled** | Dry/wet. Its law follows the active circuit's mix network, so it feels different per model (see SPEC-002). |
| `level` | Level | float | −12 to +12 dB, step 0.1 | 0.0 | **effect control** | Output trim. No hardware equivalent; a plugin necessity. |
| `bypass` | Bypass | bool | — | `false` | **effect control** | Footswitch. |

**Eight of ten are modelled.** The two that are not are labelled in the UI and in this
table, because the project's claim is that behaviour traces to hardware, and a control
that does not has to say so rather than hide among the ones that do.

### The default column is the boot state

The defaults are not neutral round numbers. They are the AB763 circuit-correct position
— the same position as SPEC-004's **Twin '64** foundation row — because the plugin boots
with that preset selected. A player's first launch should land on a researched circuit,
not on a blend of round numbers that is no amp in particular.

That makes the boot position one set of values written out in two tables, and they
drifted: this spec had `tone` 5.0 and `mix` 30.0 where SPEC-004's Twin '64 row had 5.5
and 28.0, both past the modified-marker tolerance. The plugin booted displaying
`Twin '64 *` — the marker lit before the player had touched a control. The marker was
telling the truth; the claim that we had booted on Twin '64 was the lie.

**The decision: this spec's default column moves to the foundation values.** The two
alternatives were both worse:

- *Boot with nothing selected* (display reads `Manual`). It keeps this spec's numbers
  untouched, but it gives up SPEC-004's whole premise — that selecting an amp puts the
  player on a complete, coherent foundation — for the one state a player is guaranteed to
  see. The defaults would also still be AB763-derived numbers (`locut` 318 Hz is the
  AB763 figure, see Notes) that no longer claim to be any amp.
- *Load the boot preset at construction.* The plugin would genuinely boot on Twin '64,
  but the APVTS defaults and the boot state would then disagree, so a host's
  "reset to default" would re-light the marker — the same bug, moved somewhere less
  visible — and the duplication that caused it would survive.

Moving this column keeps **one** set of values for the boot position and makes the
modified marker honest. The two values that moved were the two written as round numbers
rather than read off a circuit study; `mix` is in any case one of the two values SPEC-004
requirement 3 exempts from being circuit-correct, so this spec is not overriding a
measurement, it is adopting a voicing judgement that was already pinned in SPEC-004.

The implementation keeps both tables literal — `Source/dsp/ParameterDefaults.h` here,
`Source/dsp/PresetData.h` there — rather than deriving one from the other, so each stays
checkable against its own spec, and `tests/test_preset_boot.cpp` asserts they agree. If a
listening pass on Twin '64 moves `mix`, that test fails and this column moves with it.
That failure is the review gate, not an inconvenience.

### Why `dwell` and `mix` are separate

This is the distinction the hardware teaches and most plugins lose. On a 6G15, Dwell is
*upstream* of the driver: turning it up makes the tank work harder, which changes the
**character** of the reverb — more splash, more compression, more drip — without
necessarily making it louder. Mix is a blend at the output. A single "reverb amount" knob
collapses the two, and that collapse is why a lot of spring plugins sound static.

### Smoothing

| Parameter | Smoothing |
| --- | --- |
| `dwell`, `tone`, `mix`, `level`, `boing` | 20 ms linear ramp, per-sample |
| `locut` | 50 ms ramp on the filter coefficient |
| `decay` | 100 ms ramp on the loss filter coefficient — fast changes here can destabilise the loop |
| `circuit`, `tank` | Crossfade over 30 ms between old and new configuration; switching cannot click |
| `bypass` | 10 ms crossfade, wet path kept running |

### Implementation

- `Source/ParameterIDs.h` — the single source of truth for IDs, in `namespace bv::ids`.
- `Source/dsp/ParameterDefaults.h` — the default column above and the boot preset index,
  JUCE-free so the tests can read them without a host.
- `Source/PluginProcessor.cpp` — `createParameterLayout()` builds the APVTS layout from
  the table above, taking every default from `ParameterDefaults.h` rather than a literal,
  so a default exists in exactly one place.
- `Source/dsp/EngineParams.h` — a plain struct of denormalised values, with no JUCE
  dependency, that the processor fills each block and hands to the engine. This keeps the
  DSP testable without a host.

## Edge cases

- [ ] Host automates `circuit` rapidly — crossfade must not stack or leak voices
- [ ] Host sends a parameter value outside the declared range — must clamp, not assert
- [ ] State restored from a future version with unknown parameters — must not crash, must
      keep the parameters it recognises
- [ ] State restored from a version with *fewer* parameters — missing parameters take
      their defaults
- [ ] `bypass` engaged at load time — must not produce a burst on first unbypass
- [ ] `decay` automated from 4.5 s to 0.8 s in one block — must stay stable
- [ ] Host "reset to default" on a fresh instance — must land on the boot preset with the
      modified marker clear, not merely close to it

## Acceptance criteria

1. All ten parameters are present with the exact IDs, ranges and defaults above. Verified
   by `tests/test_parameters.cpp` against the table, which is read as the contract.
2. Every parameter round-trips through save/restore with no loss. Verified by
   `tests/test_state.cpp`.
3. Stepping each continuous parameter from minimum to maximum in a single block produces
   no sample-to-sample jump greater than 0.1 in the output.
4. Switching `circuit` and `tank` mid-signal produces no sample-to-sample jump greater
   than 0.1.
5. `bypass` engaged: output is bit-identical to input.
6. `bypass` disengaged after 10 s of bypass: wet signal is present within 20 ms, no
   silent gap.
7. Restoring a state blob with an unrecognised parameter ID does not crash and preserves
   all recognised values.
8. `grep` for a quoted parameter ID string anywhere outside `ParameterIDs.h` returns
   nothing. Enforced by `tests/test_parameter_ids.cpp`.
9. The defaults above equal the boot preset's stored values within the modified-marker
   tolerances, so a freshly created instance's preset display reads exactly `Twin '64`.
   Verified by `tests/test_preset_boot.cpp`, which asserts the display string rather than
   the values alone.

## Constraints

- `EngineParams` must stay JUCE-free so the DSP tests can construct it.
- Parameter count is capped at ten for v0.1. A pedal face has room for seven knobs and
  two switches; past that the UI stops looking like a pedal, which is a stated goal.

## Tasks

- [x] RED then GREEN: `tests/test_preset_boot.cpp` — the boot display string (criterion 9)
- [ ] RED: `tests/test_parameters.cpp` — assert the table (IDs, ranges, defaults)
- [ ] GREEN: `Source/ParameterIDs.h`, `createParameterLayout()`
- [ ] RED: `tests/test_parameter_ids.cpp` — no inline ID literals outside the header
- [ ] RED: `tests/test_state.cpp` — save/restore round-trip, forward and backward
      compatibility
- [ ] GREEN: `getStateInformation` / `setStateInformation`
- [ ] GREEN: `Source/dsp/EngineParams.h`
- [ ] RED then GREEN: smoothing tests, including `circuit`/`tank` crossfade
- [ ] RED then GREEN: bypass tests
- [ ] Document each parameter's circuit-correct value per model, for the modifier-click
      reset in Nice to have

## Notes

- Circuit research behind the modelled parameters:
  [docs/documentation/circuits/](../../documentation/circuits/)
- The default column and SPEC-004's Twin '64 foundation row are the same position. Moving
  either one means moving both; `tests/test_preset_boot.cpp` will say so.
- The `locut` default of 318 Hz is the AB763 figure from
  [fender-ab763-reverb](../../documentation/circuits/fender-ab763-reverb.md). The 6G15
  preset should set it to 300 Hz.
