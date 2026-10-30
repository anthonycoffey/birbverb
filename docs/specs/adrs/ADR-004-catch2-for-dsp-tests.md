---
id: ADR-004
title: 'Catch2 for the DSP test suite'
status: proposed
date: 2026-10-29
deciders: ['Anthony Coffey']
supersedes: ''
superseded_by: ''
---

# ADR-004: Catch2 for the DSP test suite

## Status

`proposed`

## Context

birbverb is TDD-driven, and its acceptance criteria are measurements with tolerances
rather than equality checks. The project needs a test framework that supports a tight
RED → GREEN loop and reports *how far off* a measurement was, not merely that it failed.

zoo-tron's precedent is `tools/engine_test.cpp`: a hand-written `main()` that printed
measurements, ANDed a `bool ok` across checks, and returned non-zero on failure. It is
dependency-free and it worked for about a dozen checks. birbverb's six specs list over
fifty acceptance criteria.

Forces:

- The DSP is header-only and JUCE-free by design (ADR-001), so the test target should not
  need JUCE at all.
- A few tests genuinely need JUCE — APVTS state round-trip, `PresetManager`'s file
  handling, reading the sprite manifest.
- Tolerance failures must print the numbers. This is the single most important property.
- Contributors should get a working test run from `cmake` + `ctest` with no extra setup.
- Build time matters; the suite should run on every save.

## Decision

**Catch2 v3 via CMake FetchContent, pinned to an exact tag, with custom matchers for
dB / Hz / ms tolerances, registered with CTest.**

Two test targets:

- `birbverb_tests` — links Catch2 only. Includes `Source/dsp/` headers directly. This is
  where the DSP suite lives, and the fact that it **cannot** link JUCE is a structural
  guarantee that the DSP stays JUCE-free.
- `birbverb_juce_tests` — links Catch2 and JUCE. For state round-trip, preset manager,
  and sprite manifest tests.

Custom matchers are the point of choosing a framework with a matcher system:

```cpp
REQUIRE_THAT(rt60, WithinMs(3400.0, Percent(15.0)));
// on failure: measured 2810 ms, expected 3400 ms +/- 15% (2890..3910 ms)
```

Catch2 is fetched rather than vendored, pinned to a tag rather than a branch, so builds
are reproducible and the dependency is one line to update.

## Consequences

### Positive

- Tolerance failures report the measured value, the expected value and the range, which
  is what makes a DSP suite usable rather than something you learn to ignore.
- `BDD`-style sections and `GENERATE` cover the sample-rate matrix (44.1 / 48 / 96 /
  192 kHz) without copy-pasted test bodies.
- Single-header-equivalent install via FetchContent: contributors run `cmake` and get a
  working suite.
- CTest integration gives `--output-on-failure`, test filtering, and parallel runs for
  free, plus trivial CI wiring.
- The two-target split enforces the JUCE-free DSP boundary at link time rather than by
  convention. A `#include <JuceHeader.h>` in a DSP header breaks the build, which is
  exactly when you want to find out.

### Negative / Trade-offs

- Catch2 v3 is a compiled library, not a single header, so the first configure pulls and
  builds it. Adds roughly 30–60 s to a cold build.
- Catch2's compile times are not the best in class. Doctest is faster. For a suite this
  size the difference is seconds.
- A dependency where there was none. Mitigated by pinning and by FetchContent being
  already in use for JUCE.

### Neutral

- Catch2's own `Approx` is deprecated in favour of matchers, which is fine because the
  custom matchers above are better for this anyway.
- Test binaries are large. Irrelevant; they are not shipped.

## Alternatives Considered

### Option A: Hand-rolled `main()` (the zoo-tron approach)

- **Description:** A `main()` that runs checks and returns non-zero on failure.
- **Pros:** Zero dependencies. Instant compile. Complete control over output.
- **Cons:** No test isolation — one crash takes the whole run down. No filtering, so you
  cannot run one failing test in a loop. Every tolerance message written by hand. No
  fixtures or parameterisation, so the sample-rate matrix becomes copy-paste.
- **Why rejected:** Fine at a dozen checks, unworkable at fifty-plus. The specific thing
  it cannot give is running a single failing test repeatedly, which is most of what a TDD
  loop does.

### Option B: GoogleTest

- **Description:** The other mainstream C++ framework.
- **Pros:** Ubiquitous. Excellent death tests and mocking. Good failure output.
- **Cons:** Heavier dependency. Matchers live in GMock, which is more machinery than this
  needs. Less pleasant for the value-parameterised style the sample-rate matrix wants.
- **Why rejected:** Close call. Catch2's matcher ergonomics and lighter footprint suit a
  header-only numeric codebase better. GMock's mocking is irrelevant here — there is
  nothing to mock in a DSP chain.

### Option C: doctest

- **Description:** A lighter, faster Catch2-alike.
- **Pros:** Dramatically faster compiles. Single header. Nearly the same API.
- **Cons:** Smaller matcher story, which is the feature being bought. Smaller community.
- **Why rejected:** The matchers are the reason for picking a framework at all. Worth
  revisiting if compile times become the bottleneck in the TDD loop — the test bodies
  would mostly port unchanged.

### Option D: JUCE's `UnitTest`

- **Description:** JUCE's built-in test framework.
- **Pros:** Already a dependency. Runs inside a JUCE app.
- **Cons:** Requires linking JUCE, which destroys the structural guarantee that the DSP
  is JUCE-free. Sparse assertion vocabulary. Awkward to run from CTest.
- **Why rejected:** It makes the one architectural boundary this project cares about
  unenforceable.

## Notes

- Test harness spec: [SPEC-006](../active/SPEC-006-offline-dsp-test-harness.md)
- Testing conventions: [testing-dsp](../../documentation/guides/testing-dsp.md)
- Pin the Catch2 tag in `CMakeLists.txt` and update it deliberately, never with a branch
  ref. A test framework that changes under you turns a green suite red for reasons that
  have nothing to do with your code.
