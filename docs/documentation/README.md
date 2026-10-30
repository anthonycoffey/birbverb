# documentation/

Reference material. Specs and decisions live in [`../specs/`](../specs/); this folder is
everything else.

| Folder | What belongs here | What does not |
| --- | --- | --- |
| [`circuits/`](circuits/) | **Circuit studies** — researched hardware, every claim sourced | Our DSP design, opinions |
| [`agents/`](agents/) | AI agent briefs — repo context for Claude and other tools | Specs |
| [`guides/`](guides/) | Procedural how-to: build, test, author assets and presets | Deep technical analysis |
| [`deep-dives/`](deep-dives/) | Narrow-focus analysis — algorithm comparisons, surveys | Step-by-step instructions |
| [`repos/`](repos/) | Comprehensive module/repo technical reference | |

## Start here

- [Development Standards](development-standards.md) — the process, the TDD cycle, and the
  patterns for testing DSP. Read this before writing code.
- [birbverb Agent Brief](agents/birbverb.md) — stack, entry points, architecture
  contracts, gotchas.
- [Circuit Studies index](circuits/README.md) — the hardware research, and how confident
  we are in each study.

## Guides

- [Building birbverb](guides/building.md)
- [Testing DSP](guides/testing-dsp.md)
- [Authoring Sprite Sheets](guides/sprite-authoring.md)
- [Authoring Presets](guides/preset-authoring.md)

## Deep Dives

- [Spring Reverb DSP Approaches](deep-dives/spring-reverb-dsp-approaches.md) — the survey
  behind ADR-001.
- [Spring Dispersion and Allpass Fitting](deep-dives/spring-dispersion-and-allpass-fitting.md)
  — the curve SPEC-001 fits, where every tank coefficient comes from, and the correction
  that the main chirp *ascends*.

## The distinction that matters

A **circuit study** answers *"what did the hardware actually do?"* It is research, it
cites sources, and it is allowed to say "unverified".

A **spec** answers *"what will our code do?"* It is a contract, and every requirement in
it must be testable.

Keeping them apart means a measurement that turns out to be wrong gets corrected in one
place, and the specs depending on it get re-derived deliberately rather than silently
inheriting the error.
