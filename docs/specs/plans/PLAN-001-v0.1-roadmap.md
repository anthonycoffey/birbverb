---
id: PLAN-001
title: 'v0.1 roadmap'
status: draft
created: 2026-10-29
author: Anthony Coffey
---

# PLAN-001: v0.1 roadmap

What "done" means for a first release, in what order, and what has to be true before each
phase starts.

## Definition of done for v0.1

A VST3 and Standalone (plus AU on macOS) plugin that:

- Sounds like spring reverb — dispersion, not a filtered delay
- Ships five foundation presets, one per researched circuit, each a full parameter
  snapshot
- Ships fourteen derived presets built on those foundations
- Has a sprite-based pedal UI at 600 x 650
- Has a test suite that measures its own acceptance criteria
- Has a circuit study behind every modelled behaviour, and says so where it doesn't

## Phase 0 — Foundations (no audio)

**Goal:** the project builds, the tests run, the plugin loads and passes audio through.

| Work | Spec |
| --- | --- |
| CMake with JUCE 8 and Catch2, two test targets | SPEC-006 / ADR-004 |
| Measurement library and its own analytical tests | SPEC-006 |
| Stability harness, proven to fail on an unstable filter | SPEC-006 |
| Parameter layout, IDs, state round-trip | SPEC-003 |
| Preset table and manager with the modified marker | SPEC-004 |
| Editor with procedural fallback controls for all ten parameters | SPEC-005 |

**Exit criteria:** `ctest` green. Plugin loads in a host, all ten parameters automatable,
presets switch, audio passes through unchanged. No reverb yet.

This phase is deliberately audio-free. Every later phase's RED step depends on the
measurement library, so building it first is not overhead, it is the critical path.

## Phase 1 — The tank

**Goal:** it sounds like a spring.

| Work | Spec |
| --- | --- |
| **Choose and record the dispersion curve to fit** | SPEC-001 task 1 |
| `DispersiveAllpass`, group-delay tests | SPEC-001 |
| `SpringLine` — delay loop, allpass cascade, loss filter, reflections | SPEC-001 |
| `SpringTank` — multi-spring, tank types | SPEC-001 |
| RT60 tracking, echo spacing, frequency-dependent decay | SPEC-001 |
| Sample-rate independence across 44.1 / 48 / 96 / 192 kHz | SPEC-001 |

**Entry criteria:** Phase 0 exit criteria met, and SPEC-001 at status `ready` — which it
cannot reach until the dispersion curve is chosen. That is the gate.

**Exit criteria:** impulse in produces a descending chirp with measured monotonic group
delay. RT60 tracks the `decay` parameter within ±15%. ADR-001 moves to `accepted`, or the
approach gets revisited.

## Phase 2 — The circuits

**Goal:** four units that are audibly four units.

| Work | Spec |
| --- | --- |
| `SendFilter`, `TankLoad`, `DriveTransformer` | SPEC-002 |
| `TriodeStage`, `PentodeStage` with ADAA and 2x oversampling | SPEC-002 |
| `RecoveryStage`, `MixNetwork` | SPEC-002 |
| `CircuitModel` configurations for AB763, 6G16, 6G15, Ampeg | SPEC-002 |
| The cross-model ordering tests | SPEC-002 |

**Exit criteria:** all four models pass their ordering assertions. Aliasing 60 dB down.
Combined CPU under 6% of one core at 48 kHz.

## Phase 3 — Voicing

**Goal:** the presets are worth shipping.

| Work | Spec |
| --- | --- |
| Review every foundation value against its circuit study | SPEC-004 |
| Listen to and revise all fourteen derived presets | SPEC-004 |
| Resolve the Ampeg revision and naming question | SPEC-004 |
| Whole-bank stability and click-free stepping | SPEC-004 |

This is the first phase whose work is primarily listening rather than measuring. The
derived preset values currently in SPEC-004 were written without an engine to hear, and
they are expected to be wrong.

**Exit criteria:** all 15 presets reviewed by ear. Foundation values traced to studies.

## Phase 4 — The face

**Goal:** it looks like a pedal.

| Work | Spec |
| --- | --- |
| `sprites.json` manifest and its test | SPEC-005 |
| `SpriteAtlas`, `SpriteStore`, 1x/@2x resolution | SPEC-005 |
| Sprite control components | SPEC-005 |
| Author `background.png`, then the knob atlases, then the rest | sprite-authoring |
| `juce_add_binary_data` wiring | ADR-003 |
| Verify at scale 1.0 / 1.5 / 2.0 on Windows and macOS | SPEC-005 |

Phase 4 can run in parallel with Phases 1–3. The procedural fallback means art is never
blocking, and DSP is never waiting on art.

**Exit criteria:** all ten controls sprite-rendered. Assets removed from the build still
yields a working plugin.

## Phase 5 — Release

| Work |
| --- |
| `pluginval` at strictness 10, and `auval` on macOS |
| Test in at least three hosts (Reaper, LUNA, Ableton) |
| README with a screenshot and an audio example |
| LICENSE — done: AGPLv3 |
| CI: build and test on Windows and macOS |
| Move completed specs to `docs/specs/archive/` |
| Tag v0.1.0 |

## What is explicitly not in v0.1

- Resizable UI (ADR-002 trade)
- 6G15 reissue circuit model
- Nonlinear spring behaviour (spring clank)
- Tank shock / "kick the amp" feature
- Stereo tank decorrelation
- Preset import/export, preset categories
- MIDI learn, tempo sync
- Oversampling above 2x

## Open Questions

- [x] **License. AGPLv3.** Confirmed against JUCE's own terms, which read "licensed
      under both the open source AGPLv3 and a commercial JUCE licence" — so it is AGPL,
      not GPL, and the two are not interchangeable. The alternative, JUCE 8's free
      Starter tier (free under $20k annual revenue), forbids releasing under a licence
      requiring source disclosure, which would mean not being open source at all.
- [x] Does the bank need a separate Ampeg Echo Jet preset? **Yes**, and it is in the bank
      as `Echo Jet '63`. The Echo Jet turned out to be a different design, not a small
      Reverberocket: 1963, 6D10 preamp, 2 x 7591A, and a speaker-driven tank on its own
      separated speaker. With no schematic it is voiced on the Reverberocket circuit, and
      says so in its description.
- [ ] Is a low-CPU "eco" mode worth a spec? ADR-001 rejected FDN-with-dispersion as the
      main engine but noted it as a possible cheap mode.

## Notes

The ordering is driven by one fact: **four of the six specs cannot start their RED phase
until the measurement library exists.** That is why Phase 0 ships no audio and is still
the right thing to build first.

The second ordering constraint is softer but real: Phase 1's gate is a research decision,
not a coding one. Choosing the dispersion curve to fit is the single highest-leverage
decision in the project, and the plan puts it where it cannot be skipped.
