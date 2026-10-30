# Authoring Presets

How to add or revise a preset, and the rules that keep the bank meaning something. The
contract is [SPEC-004](../../specs/active/SPEC-004-preset-bank-and-foundation-presets.md).

## The idea

A preset in birbverb is not a convenience shortcut. It is a **foundation**: select it and
every control lands where that circuit puts it, so the player is standing on a complete,
coherent starting point before they touch anything. Then they build on it.

Two rules follow from that, and they are the whole reason this guide exists.

### Rule 1 — a preset sets every parameter

All ten, including `circuit` and `tank`. A preset that sets four parameters and leaves the
rest wherever they happened to be is not a foundation, it is a suggestion, and the player
can no longer tell what they are hearing.

This is the thing that gets forgotten when a parameter is added. The checklist in the
[agent brief](../agents/birbverb.md#add-a-parameter) has it as step 5 for that reason.

### Rule 2 — the description matches the confidence of the research

Foundation presets rest on circuit studies, and those studies have a `confidence` field.

| Study confidence | How the preset is described |
| --- | --- |
| `medium` or better | As the circuit — *"Blackface AB763 reverb."* |
| `low` | As **voiced after** it — *"Voiced after the 1963 brown Vibroverb."* |

`Twin '64` and `Drip Tank` rest on medium-confidence studies. `Vibroverb '63` and
`Rocket '61` rest on low-confidence ones, and they say so.

This is a requirement (SPEC-004 requirement 8), not a stylistic preference. The repo's
claim is that behaviour traces to documented hardware; a preset that overstates what we
know breaks that claim in the most visible place in the product.

## Where presets live

`Source/dsp/PresetData.h` — a `constexpr std::array` of `bv::Preset`, JUCE-free so the
test suite can read it. `PresetManager` wraps it with the APVTS and file handling.

```cpp
{ "Twin '64", "Blackface AB763 reverb. Bright, splashy, sitting behind the dry.",
  -1, CircuitModel::ab763, TankType::long2Spring,  5.0f, 3.4, 5.5f, 318.0, 5.0f, 28.0f, 0.0f },
//     ^ derivedFrom                                dwell decay tone locut boing  mix  level
```

`derivedFrom` is `-1` for a foundation, otherwise the index of the foundation it was built
from. That is what makes "back to foundation" possible and what the preset browser uses to
group the list.

## Adding a foundation preset

A new foundation means a new circuit, so the order is:

1. **Research it.** `/new-circuit`, every value sourced, Measurable targets filled in.
2. **Add the circuit model.** `CircuitModel` enum plus a `circuitConfigFor` case in
   `Source/dsp/CircuitModel.h`, every constant carrying its source or `INFERRED`.
3. **Extend the `circuit` parameter's choice list** in `createParameterLayout()`, and the
   switch labels in the editor. This changes a choice parameter's range, so consider
   whether old sessions restore sensibly.
4. **Add the preset** at the end of the foundations block, before the derived ones.
5. **Set the values from the study**, not from taste. `mix` and `level` are the exceptions
   — they have no circuit-correct value.
6. **Write the description** at the study's confidence level.
7. **Update** SPEC-004's foundation table and `docs/documentation/circuits/README.md`.

## Adding a derived preset

1. Pick the foundation. Set `derivedFrom` to its index.
2. Start from the foundation's values and change only what the preset is about. A derived
   preset that shares nothing with its foundation should be a foundation, or should be
   derived from a different one.
3. Keep the name to **16 characters or fewer** — it has to fit the preset display.
4. One line of description, saying what it is for rather than what it changed. "Dwell
   cranked until the tank splatters" beats "dwell 9.5, decay 4.2".

## Listening

The fourteen derived presets currently in `PresetData.h` were written **without an engine to
hear**, because the engine does not exist yet. They are a first draft and they are
expected to be wrong.

SPEC-004's task list requires listening to each one before the values are final, and that
task is not optional. The procedure:

```bash
cmake --build build --target bv_render -j
./build/bv_render out/
```

That renders every factory preset through a plucked-string test signal. Spring reverb is
judged almost entirely on transients, which is why the test signal has a hard attack and a
harmonic series rather than being a sine.

Then listen in a host, with a guitar, through an amp or an amp sim. A reverb preset that
sounds good on a rendered pluck and bad under a real performance is a bad preset, and
there is no measurement that will tell you which you have.

## Tests

`tests/test_presets.cpp` asserts the whole bank:

- Every preset sets all ten parameters, each within its declared range
- Every foundation's `circuit` matches the study it is named for
- Every derived preset's `derivedFrom` names an existing foundation
- Every preset has a non-empty name and description (an empty description is exactly the
  kind of thing that ships)
- Every preset produces finite, bounded output for 10 s of full-scale noise
- Stepping through the whole bank while audio runs produces no click

Run them after any change to `PresetData.h`:

```bash
ctest --test-dir build --output-on-failure
```

## The modified marker

`PresetManager::isDirty()` compares the current parameter values against the loaded
preset's snapshot and shows a `*` when they differ. It uses **per-parameter tolerances**,
because a value that round-trips through APVTS normalisation is not bit-identical and must
not read as modified.

If you add a parameter, add it to `isDirty()` with a tolerance proportional to its range —
0.02 on a 0–10 control, 1.0 on a frequency in Hz.

## User presets

Saved as XML snapshots of the APVTS state in the plugin's application-data folder, merged
into the same list as the factory bank. A corrupt or truncated file is skipped rather than
being fatal. There is no `.vstpreset` / `.aupreset` support in v0.1.
