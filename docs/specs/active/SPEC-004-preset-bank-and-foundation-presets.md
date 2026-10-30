---
id: SPEC-004
title: 'Preset bank and foundation presets'
status: draft
created: 2026-10-29
author: Anthony Coffey
reviewers: []
affected_repos: ['birbverb']
---

## Reviewer Notes

<!-- Leave empty until code review. When requesting changes, reviewer adds feedback here: -->

---

# Feature: Preset bank and foundation presets

## Problem

The point of birbverb's preset bank is not convenience. It is that **each researched amp
is a complete, coherent starting point** — select it and every control lands where that
circuit puts it, so the player is standing on a real foundation before they touch
anything. Then they build on it.

This only works if a preset is a *full* snapshot. A preset that sets four of ten
parameters and leaves the rest wherever they happened to be is not a foundation, it is a
suggestion.

## Requirements

### Must have

1. WHEN a factory preset is selected, the system SHALL set **every** parameter in
   [SPEC-003](SPEC-003-parameter-model.md) to that preset's stored value, including
   `circuit` and `tank`.
2. The bank SHALL contain one **foundation** preset per circuit study in
   [docs/documentation/circuits/](../../documentation/circuits/), and these SHALL be
   listed first.
3. Each foundation preset's parameter values SHALL be the circuit-correct values from its
   circuit study, not a taste judgement, with the exception of `mix` and `level` which
   have no single correct value.
4. WHEN a parameter drifts from the loaded factory preset's stored value, the preset
   display SHALL mark the preset as modified.
5. WHEN the user saves a user preset, it SHALL capture the full parameter state and SHALL
   persist across plugin sessions.
6. WHEN the host saves project state, the current preset selection SHALL round-trip
   alongside the parameter values.
7. Factory presets SHALL be ordered foundations first, then derived presets, and each
   derived preset SHALL record which foundation it was built from.
8. Each preset SHALL carry a one-line description, and foundation preset descriptions
   SHALL reflect the **confidence level** of the circuit study behind them.
9. The ◀ ▶ arrows SHALL step through factory presets only; user presets SHALL be
   reachable from the preset list.

### Nice to have

- A "back to foundation" action that resets to the foundation the current preset derives
  from, keeping `mix` and `level`.
- Preset categories in the browser (Foundations / Surf / Subtle / Extreme).
- Preset import/export as a single file for sharing.

### Non-goals

- No preset morphing or interpolation.
- No cloud or community preset browser.
- No per-DAW preset format (`.vstpreset`, `.aupreset`) in v0.1; user presets are XML in
  the plugin's application-data folder, as in zoo-tron.

## Design

### Storage

Factory presets are a `constexpr`-friendly array in `Source/PresetManager.h`, following
the zoo-tron pattern: a `BirbPreset` struct of denormalised parameter values plus name,
description, and foundation reference. User presets are XML snapshots of the APVTS state
in the plugin's application-data directory, merged into the same list.

The current selection lives in the APVTS state tree (factory index, or −1 plus a name for
a user preset) so it survives save/restore, and a factory preset shows a modified marker
the moment a value drifts from its snapshot.

### Foundation presets

Values below are the **proposed** circuit-correct settings. They are derived from the
circuit studies and must be reviewed against them before this spec reaches `ready`.

| Preset | `circuit` | `tank` | `dwell` | `decay` | `tone` | `locut` | `boing` | `mix` | `level` |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| **Twin '64** | AB763 | Long 2-Spring | 5.0 | 3.4 | 5.5 | 318 | 5.0 | 28 | 0.0 |
| **Super '63** | AB763 | Long 2-Spring | 5.5 | 3.2 | 6.0 | 318 | 5.0 | 32 | 0.0 |
| **Vibroverb '63** | 6G16 | Long 2-Spring | 4.5 | 3.0 | 3.5 | 318 | 4.5 | 38 | 0.0 |
| **Drip Tank** | 6G15 | Long 2-Spring | 7.5 | 3.6 | 6.5 | 300 | 6.5 | 45 | 0.0 |
| **Rocket R-12-R** | Ampeg | Short 2-Spring | 6.0 | 2.0 | 3.0 | 280 | 3.5 | 35 | +1.0 |

Descriptions, worded to the confidence of the study behind each:

| Preset | Description |
| --- | --- |
| Twin '64 | Blackface AB763 reverb. Bright, splashy, and sitting behind the dry signal where Fender put it. |
| Super '63 | The same AB763 circuit, voiced a touch wetter and brighter. |
| Vibroverb '63 | Voiced after the 1963 brown Vibroverb — darker and more present than a blackface, from a lower-impedance mixer with no bright cap. |
| Drip Tank | The outboard 6G15 unit. A power tube drives the tank, so dwell goes from clean to crashing. This is the surf sound. |
| Rocket R-12-R | Voiced after the first-edition R-12-R Reverberocket of May 1961, the first production amp with reverb. Named for the model designation because Ampeg's plain Rocket (R-12) is a different, reverb-less amp. No reverb transformer, so the springs are driven gently: dark, soft, diffuse. |

Note the wording. `Twin '64` and `Drip Tank` rest on `medium`-confidence studies and are
described as the circuit. `Vibroverb '63` and `Rocket R-12-R` rest on `low`-confidence
studies and are described as *voiced after* it. That distinction is a requirement
(requirement 8), not a stylistic choice — the repo's premise is that we say what we know.

### Derived presets

Each builds on a foundation. The `from` column is stored with the preset so a
"back to foundation" action is possible.

| Preset | From | What changes | Description |
| --- | --- | --- | --- |
| Surf Crash | Drip Tank | dwell 9.5, decay 4.2, tone 7.5, mix 60, boing 8.0 | Dwell cranked until the tank splatters. |
| Wet Twin | Twin '64 | dwell 6.5, decay 3.8, mix 55 | A blackface with the reverb knob past where it's polite. |
| Dry Kiss | Twin '64 | dwell 3.5, mix 12 | Just enough air to tell it's there. |
| Dark Wash | Vibroverb '63 | tone 2.0, decay 3.6, locut 200, mix 45 | Lo cut opened up and the top rolled off. Thick. |
| Clang | Super '63 | boing 9.5, decay 2.2, tone 8.0, mix 40 | Dispersion pushed past any real spring. Metallic, chirping. |
| Slapback Spring | Twin '64 | tank Short 2-Spring, decay 0.9, dwell 6.0 | Short tank, fast decay. Nearly a slapback. |
| Cavern | Twin '64 | tank Long 3-Spring, decay 4.5, tone 4.5, mix 55 | Three springs at full decay. |
| Pedal Steel | Twin '64 | locut 420, tone 7.0, decay 3.0, dwell 4.5, mix 48 | Bass cut hard so chords stay clear under the wash. |
| Bloom | Rocket R-12-R | tank Long 3-Spring, decay 4.0, dwell 7.0, tone 3.5, boing 2.5, mix 50 | The Ampeg's soft drive into a long tank. Slow swell, no drip. |
| Hard Pick | Drip Tank | decay 2.6, dwell 8.5, locut 380, mix 25 | Saturating driver, short tail, cut low. Reacts to attack. |
| Rocket '62 | Rocket R-12-R | dwell 7.5, decay 2.1, tone 3.5, boing 4.0, mix 40 | The September 1962 Reverberocket revision, approximated: more gain into the tank, hotter return. |
| Echo Jet '63 | Rocket R-12-R | dwell 5.0, decay 1.8, tone 4.0, locut 300, boing 3.0, mix 55, level +1.5 | Voiced after the Ampeg Echo Jet. Softer and wetter than the Reverberocket. |
| Brown Verb | Vibroverb '63 | dwell 5.5, decay 2.6, tone 4.0, locut 250, mix 42 | Mid-forward and close. The brownface voicing without the wash. |
| Big Stage | Super '63 | tank Long 3-Spring, dwell 6.0, decay 4.0, tone 6.5, boing 5.5, mix 50 | Three springs, long decay, plenty of wet. A 4x10 in a room. |

Nineteen presets: five foundations, fourteen derived.

### The two Ampeg additions carry caveats in their descriptions

`Rocket '62` approximates a revision whose component values are **anecdotal** — forum
members comparing the 1961 and September 1962 factory schematics report V3a's plate
resistor dropping 5.6 kΩ → 2.2 kΩ and its cathode resistor 1.5 kΩ → 330 Ω. That is a
large gain increase, and if a primary source confirms it the preset should be promoted to
a second circuit model. Until then it is more dwell on the 1961 circuit, and says so.

`Echo Jet '63` is voiced on the Reverberocket circuit because the Echo Jet is a genuinely
different design — 1963, 6D10 preamp, 2 x 7591A, **speaker-driven** reverb on a physically
separated reverb speaker — and we have no schematic for it. Its quasi-stereo wet/dry
separation is **not modelled**, and the preset description says that too. Building an
`ampegEchoJet` circuit model out of inferred drive constants would put the one
untraceable thing in the repo at the centre of a preset, which is worse than a preset
that admits what it is.

## Edge cases

- [ ] User preset with a name that collides with a factory preset name
- [ ] User preset file that is corrupt or truncated — must be skipped, not fatal
- [ ] User preset saved by a future version with extra parameters — load what is known
- [ ] Preset selected while `bypass` is engaged — values apply, no audible artefact on
      unbypass
- [ ] Preset stepped rapidly with the arrows — `circuit`/`tank` crossfades must not stack
- [ ] Application-data directory not writable — save fails gracefully with a message
- [ ] Factory preset loaded, then one parameter nudged and nudged back — the modified
      marker must clear (comparison needs a tolerance, per parameter)

## Acceptance criteria

1. Loading each of the 19 factory presets sets all ten parameters to the stored values.
   Verified by `tests/test_presets.cpp` iterating the whole bank.
2. Every factory preset's stored values are within their parameter's declared range.
3. Every foundation preset's `circuit` matches the circuit study it is named for.
4. Every derived preset's `from` field names an existing foundation preset.
5. Save, modify, reload: a user preset round-trips all ten parameters exactly.
6. Loading a factory preset then changing one parameter sets the modified marker; reverting
   it clears the marker.
7. Project state save/restore preserves the preset index or user preset name.
8. All 19 presets produce finite, bounded output for 10 s of full-scale noise.
9. Stepping through all 19 presets while audio runs produces no sample-to-sample jump
   greater than 0.1.
10. Every preset has a non-empty name and description. Verified by test, because an empty
    description is the kind of thing that ships.

## Constraints

- `PresetManager.h` is header-only but may use JUCE (it handles files and APVTS). The
  preset *data* must also be reachable from the DSP tests, so the parameter values live in
  a JUCE-free table that `PresetManager` wraps.
- Preset names must be ≤ 16 characters to fit the UI's preset display
  ([SPEC-005](SPEC-005-sprite-based-pedal-ui.md)).

## Tasks

- [ ] Review every foundation value against its circuit study; correct the table above
      before this spec goes `ready`
- [x] Resolve: does the bank need a separate Ampeg Echo Jet preset? **Yes** — shipped as
      `Echo Jet '63`, voiced on the Reverberocket circuit, because the Echo Jet is a
      different design and we have no schematic for it
- [x] Resolve the Ampeg naming against which revision SPEC-002 actually models. **The
      first-edition R-12-R, May 1961.** Foundation renamed `Rocket R-12-R` so it cannot be
      confused with Ampeg's reverb-less Rocket R-12; the 1962 revision ships as the
      derived `Rocket '62`
- [ ] Promote `Rocket '62` to a circuit model if a primary source confirms the September
      1962 component values
- [ ] RED: `tests/test_presets.cpp` — the whole-bank assertions (criteria 1–4, 10)
- [ ] GREEN: the JUCE-free preset table
- [ ] GREEN: `PresetManager` — load, save, step, modified-marker, user preset merge
- [ ] RED then GREEN: user preset round-trip and corrupt-file handling
- [ ] RED then GREEN: stability and click-free stepping across the bank
- [ ] Write the 14 derived presets and listen to each one before committing values

## Notes

- Parameter definitions: [SPEC-003](SPEC-003-parameter-model.md)
- Per-amp circuit behaviour: [SPEC-002](SPEC-002-tube-drive-and-recovery-stages.md)
- Preset display and browser UI: [SPEC-005](SPEC-005-sprite-based-pedal-ui.md)
- The derived preset values in the table are a **starting draft written without listening
  to the engine**, because the engine does not exist yet. They will be wrong. The task
  list says to listen before committing them, and that task is not optional.
