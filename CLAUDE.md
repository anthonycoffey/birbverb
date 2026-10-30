# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in
this repository.

## Read first

birbverb is a **document-driven** project. Before starting any non-trivial task:

- **Agent brief** (start here): `docs/documentation/agents/birbverb.md` — tech stack,
  entry points, common tasks, gotchas
- **Docs structure**: `docs/README.md` — how `docs/` is organised and the spec lifecycle
- **Development standards**: `docs/documentation/development-standards.md` — the
  research-first workflow, the TDD cycle, how to test DSP, version control conventions
- **Roadmap**: `docs/specs/plans/PLAN-001-v0.1-roadmap.md` — what phase we are in and
  what the current phase's exit criteria are

## The three rules that matter most

### 1. Research before spec, spec before test, test before code

```
0. Research  → circuit study in docs/documentation/circuits/, every claim sourced
1. Spec      → docs/specs/active/, acceptance criteria written as measurements
2. Approve   → spec reaches status: ready
3. RED       → write the failing test
4. GREEN     → minimum code to pass
5. REFACTOR  → clean up, tests stay green
```

If you are writing DSP and cannot name the spec it satisfies, stop and write the spec.
If you are writing a spec about a vintage circuit and cannot name the circuit study
behind it, stop and write the study.

### 2. Every constant carries its source

Every audible behaviour must be one of:

- **Derived** — traceable to a documented hardware property, with the circuit study cited
- **Inferred** — a reasonable consequence of one, labelled `INFERRED` in the code
- **A deliberate liberty** — labelled `FITTED` or stated in the spec's Non-goals

Look at `Source/dsp/CircuitModel.h` for the convention. A bare numeric constant in a DSP
file is a review failure, because the project's entire claim is that behaviour traces to
hardware, and a constant without provenance quietly breaks it.

### 3. `Source/dsp/` must never include JUCE

The DSP is header-only, in `namespace bv`, with no JUCE dependency. The test target links
Catch2 and `bv_dsp` and **not** JUCE, so a stray `#include <JuceHeader.h>` in a DSP header
breaks the test build. That is the point — it is a link-time guarantee rather than a
convention. See `docs/specs/adrs/ADR-004-catch2-for-dsp-tests.md`.

`Source/ui/` and the two `Plugin*` files may use JUCE freely.

## Commands

```bash
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo   # fetches JUCE + Catch2
cmake --build build -j                             # VST3 / Standalone (+ AU on macOS)
cmake --build build --target birbverb_tests -j     # offline DSP suite
ctest --test-dir build --output-on-failure         # the implemented tests
ctest --test-dir build -L red --output-on-failure  # tests written ahead of their code
cmake --build build --target bv_render -j          # offline wav render for listening
```

On macOS, `auval -v aufx Bvrb Ynme` validates the Audio Unit.

## Current state

**Phase 0 of PLAN-001.** The plugin builds and loads, all ten parameters work, presets
switch, state round-trips, and the measurement suite runs.

**There is no reverb yet.** `Source/dsp/SpringTank.h` is a documented skeleton that
outputs silence, so the plugin passes audio through. Tests describing the unbuilt
behaviour are tagged `[.red]` and hidden from the default run.

**SPEC-001 is now `ready`.** The dispersion curve is chosen and recorded: Parker (2011)
eq. (4) for the stretched allpass cascade, with transition frequency and echo spacing
derived from Bilbao & Parker (2009) eqs. (2)-(4). Both papers are free to read, which is
why they were chosen over the paywalled originals. The closed forms are in
`Source/dsp/Dispersion.h`, every tank coefficient is computed from published caliper
measurements in `Source/dsp/TankConfig.h`, and `tests/test_dispersion_curve.cpp` asserts
all of it against the papers' own numbers. Next up is `DispersiveAllpass`, then
`SpringLine`, then the tank.

## Layout

```
Source/
  ParameterIDs.h        the ONLY place a parameter ID string may appear
  PluginProcessor.*     parameter layout, state, block processing
  PluginEditor.*        the pedal face, sprite controls, procedural fallback
  PresetManager.h       factory + user presets, modified marker
  dsp/                  header-only, namespace bv, NO JUCE
    CircuitModel.h      per-amp configuration, every value sourced
    Dispersion.h        the dispersion closed forms; pure functions, no state
    TankConfig.h        tank types, as spring GEOMETRY — delays are derived
    SpringTank.h        SKELETON — SPEC-001
    ReverbEngine.h      dry path, mix, level, bypass (real); wet path (stubbed)
    EngineParams.h      denormalised params handed to the engine each block
    PresetData.h        the JUCE-free factory preset table
  ui/
    SpriteAtlas.h       one atlas: grid geometry + frame drawing
    SpriteStore.h       manifest loading, 1x/@2x, per-atlas fallback
tests/
  support/              Measure.h, Generate.h, Approx.h, Stability.h
  test_*.cpp            one file per spec area
                        test_dispersion_curve.cpp is NOT red — it tests pure math
Assets/sprites/         sprites.json + the art (none committed yet)
tools/                  render.cpp, wav.h
docs/                   the central hub — see docs/README.md
```

## TDD loop for a RED test

Tests written ahead of their implementation are tagged `[.red]` — the leading dot hides
them from the default Catch2 run, so CI stays green on a scaffold while the tests still
exist and still run on demand.

```bash
./build/birbverb_tests "[red]"      # watch it fail
# implement
# drop the leading dot from the tag so it joins the default suite
ctest --test-dir build --output-on-failure
```

A test that has never been seen to fail has not been shown to test anything.

## Code style

- C++20.
- `prepare(sampleRate, ...)` then `reset()` then `process(...)`. Every DSP class has that
  shape.
- No allocation, locking, logging, or file I/O on the audio thread. Allocate in
  `prepare()`.
- Comments explain *why*, especially which circuit property a number came from.
- Parameter IDs come from `bv::ids::`, never an inline literal.
- Match the surrounding code's density and idiom.

## Version control

- **Never commit directly to `main`.** Branch, then PR. The initial import went
  straight to `main` by explicit decision; that was the one exception.
- Branches: `feature/`, `fix/`, `chore/`, `refactor/`, `research/`
- Commits: `<type>: <description> [SPEC-XXX]`, imperative mood
- Types: `feat`, `fix`, `chore`, `refactor`, `docs`, `test`, `style`, `research`, `assets`
- A PR that changes sprite art needs a screenshot — binary diffs are unreviewable

## Slash commands

| Command | Creates | Destination |
| --- | --- | --- |
| `/new-spec` | Feature spec | `docs/specs/active/` |
| `/new-bug` | Bug report | `docs/specs/active/` |
| `/new-adr` | Architecture Decision Record | `docs/specs/adrs/` |
| `/new-agent-brief` | AI agent brief | `docs/documentation/agents/` |
| `/new-circuit` | Circuit study | `docs/documentation/circuits/` |

## Gotchas

- **`decay` automated fast can destabilise the tank loop.** SPEC-003 specifies 100 ms
  smoothing on the loss filter coefficient for that reason, not 20 ms like the rest.
- **The main chirp ASCENDS.** Group delay rises with frequency and peaks at the transition
  frequency `F_C`, so low frequencies arrive first. The descending chirp is a secondary
  wideband series at least 30 dB down. Several documents said "descending" before the
  dispersion study corrected them; if you find another, fix it.
- **The allpass stretch factor is fractional.** Rounding it to an integer makes `F_C` jump
  with the sample rate — at 48 kHz, `k = 5` is 4800 Hz and `k = 6` is 4000 Hz — and breaks
  SPEC-001 requirement 9. Use `dispersion::coeffForFractionalDelay` for the remainder.
- **The tank load is reactive, not resistive.** Its impedance rises ~6 dB/octave, so bass
  draws more current. Modelling it as a resistor gets the drive spectrum wrong and is the
  easiest way to make this sound like a filtered delay.
- **Sprite assets: the knob sheet contains only what rotates.** Wells, shadows and labels
  belong in `background.png`. A rotating shadow reads as fake instantly.
- **Preset values for the fourteen derived presets were written without an engine to hear.**
  They are a draft and are expected to be wrong. SPEC-004 requires listening before they
  are final.
- **`isDirty()` needs per-parameter tolerances.** A value that round-trips through
  normalisation is not bit-identical and must not read as modified.
- **The license is AGPLv3, not GPLv3.** JUCE's open-source option is AGPLv3, and the two
  are not interchangeable. Do not write "GPL" in new files or docs.
