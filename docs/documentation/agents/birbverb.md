---
service: 'birbverb'
repo: 'birbverb'
updated: 2026-10-29
---

# Agent Brief: birbverb

> Context for AI agents working in this repo. Keep it updated as the codebase evolves.

## Purpose

An open-source vintage spring reverb plugin (VST3 / AU / Standalone) that models four
specific reverb circuits — Fender AB763 blackface, Fender Vibroverb 6G16, the Fender
6G15 outboard Reverb Unit, and the Ampeg Reverberocket — rather than approximating "spring
reverb" generically.

The project's distinguishing claim is that **every audible behaviour traces to documented
hardware**. That claim is the reason for the `docs/documentation/circuits/` folder, the
source comments on every DSP constant, and the preset descriptions that say "voiced after"
when the research behind them is thin. An agent that ignores this convention produces code
that technically works and defeats the point of the repo.

## Tech Stack

| Layer | Technology |
| --- | --- |
| Language | C++20 |
| Framework | JUCE 8.0.14 (via CMake FetchContent) |
| Build | CMake ≥ 3.22 |
| Tests | Catch2 v3.7.1 (via FetchContent), run under CTest |
| DSP | Header-only, `namespace bv`, **no JUCE dependency** |
| UI | JUCE `Graphics` compositing sprite atlases, with procedural fallback |
| Formats | VST3 + Standalone everywhere; AU added on Apple |
| Assets | PNG sprite sheets, committed to Git, compiled in via `juce_add_binary_data` |

## Key Dependencies

- **JUCE** — plugin wrapper, APVTS, UI, audio formats. Fetched at configure time.
- **Catch2 v3** — the test framework. Chosen for its matcher system, because DSP
  assertions are numbers with tolerances and the failure message has to print them. See
  [ADR-004](../../specs/adrs/ADR-004-catch2-for-dsp-tests.md).

Nothing else. No DSP library dependency — the tank model is ours.

## Entry Points

- `Source/PluginProcessor.cpp` — `createParameterLayout()` is the SPEC-003 contract;
  `processBlock` gathers params and calls the engine
- `Source/dsp/ReverbEngine.h` — everything between plugin input and output
- `Source/dsp/SpringTank.h` — **the skeleton**. SPEC-001. This is the main unbuilt thing
- `Source/dsp/CircuitModel.h` — the researched per-amp values, every one with its source
- `Source/dsp/PresetData.h` — the factory preset table, JUCE-free
- `Source/PluginEditor.cpp` — the pedal face and the procedural fallback drawing
- `tests/support/Measure.h` — the measurement library every DSP test rests on
- `Assets/sprites/sprites.json` — sprite geometry and control layout

## Architecture Contracts

### The JUCE boundary

`Source/dsp/` is header-only, `namespace bv`, and **must not include JUCE**. The
`birbverb_tests` target links Catch2 and `bv_dsp` but not JUCE, so a stray
`#include <JuceHeader.h>` in a DSP header breaks the test build. This is enforcement by
link line rather than by convention, and it is deliberate.

`Source/ui/`, `PluginProcessor.*`, `PluginEditor.*` and `PresetManager.h` may use JUCE.

### Parameter IDs

`Source/ParameterIDs.h` is the only place a parameter ID string may appear. A typo'd
inline literal produces a parameter that silently does nothing, in a host, in someone
else's session.

### Params in, audio out

The processor turns APVTS values into a `bv::EngineParams` struct each block and hands it
to the engine. The engine never knows APVTS exists, which is what makes it testable
without a host.

### Sprites degrade

Missing, corrupt, or geometrically inconsistent art falls back to procedural drawing, per
atlas rather than globally. This is what lets DSP and art proceed in parallel — the plugin
is fully usable with zero sheets committed, which is its current state.

## Environment Variables

None. The plugin has no runtime configuration and no external services.

## Common Tasks

### Build and test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure
```

### Add a DSP block

1. Confirm there is a spec. If not, write one (`/new-spec`) and get it to `ready`.
2. Write the failing test in `tests/`, tagged `[.red]`, asserting the spec's acceptance
   criteria with the matchers from `support/Approx.h`.
3. Run `./build/birbverb_tests "[red]"` and watch it fail. Do not skip this.
4. Add the header under `Source/dsp/`, `namespace bv`, no JUCE.
5. Make the test pass. Drop the leading dot from the tag.
6. Add every constant's source comment, or `INFERRED` / `FITTED`.

### Add a parameter

1. Update the table in `docs/specs/active/SPEC-003-parameter-model.md` — it is the
   contract, and `tests/test_parameters.cpp` asserts it.
2. Add the ID to `Source/ParameterIDs.h`.
3. Add it to `createParameterLayout()` with its range, default and skew.
4. Add it to `bv::EngineParams` and to `gatherParams()`.
5. Add it to every preset in `Source/dsp/PresetData.h` — a preset is a FULL snapshot.
6. Add it to `PresetManager::load()` and `isDirty()`, with a sensible tolerance.
7. Add a control to the editor and an entry to `Assets/sprites/sprites.json`.

Step 5 is the one that gets forgotten, and forgetting it turns presets from foundations
into suggestions.

### Research a circuit

Use `/new-circuit`. Cite every value. Mark sources primary / secondary / anecdotal. Put
unverified claims in Open Questions, never in the body. Fill the Measurable targets table
— if a property cannot be expressed as a number a test could assert, it is not ready for a
spec.

### Add sprite art

Read `docs/documentation/guides/sprite-authoring.md` first. Exact dimensions matter: a
sheet one pixel off misaligns every frame after the first.

## Known Gotchas

- **`SpringTank` is a skeleton.** It returns silence. The plugin passes audio through.
  This is expected until PLAN-001 Phase 1.
- **SPEC-001 cannot start until its first task is done** — choosing and recording the
  dispersion curve to fit. Writing allpass code before that decision is how the project
  ends up with a cascade nobody can explain.
- **The tank load is reactive.** Impedance rises ~6 dB/octave, so bass draws more current.
  Modelling it as a resistor gets the drive spectrum wrong, and it is the single easiest
  way to make this sound like a filtered delay instead of a spring.
- **`decay` needs slower smoothing than everything else** (100 ms, not 20 ms). Fast
  changes to the in-loop loss filter can destabilise the tank.
- **The fourteen derived presets were written without an engine to hear.** They are a first
  draft and expected to be wrong.
- **`isDirty()` needs per-parameter tolerances**, because a value round-tripped through
  normalisation is not bit-identical.
- **Two circuit studies are low confidence** (6G16, Ampeg). Their presets are worded as
  "voiced after", and their tests assert orderings rather than absolute values. Do not
  tighten those into absolute assertions without new research.
- **The license is AGPLv3, not GPLv3.** JUCE's open-source option is AGPLv3 and the two
  are not interchangeable. Do not write "GPL" in new files or docs.
- **No AU on Windows.** `CMakeLists.txt` adds AU only on Apple; asking for it elsewhere
  fails the configure step.

## Related Docs

- [docs/README.md](../../README.md) — docs structure and spec lifecycle
- [Development Standards](../development-standards.md) — process, TDD, DSP test patterns
- [PLAN-001](../../specs/plans/PLAN-001-v0.1-roadmap.md) — the roadmap and current phase
- [Circuit studies](../circuits/) — the hardware research
- [ADR-001](../../specs/adrs/ADR-001-spring-reverb-modelling-approach.md) — why a
  dispersive waveguide
- [ADR-002](../../specs/adrs/ADR-002-sprite-based-ui-rendering.md) — why sprites
- [ADR-004](../../specs/adrs/ADR-004-catch2-for-dsp-tests.md) — why Catch2
