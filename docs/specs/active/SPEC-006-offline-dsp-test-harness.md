---
id: SPEC-006
title: 'Offline DSP test harness'
status: draft
created: 2026-10-29
author: Anthony Coffey
reviewers: []
affected_repos: ['birbverb']
---

## Reviewer Notes

<!-- Leave empty until code review. When requesting changes, reviewer adds feedback here: -->

---

# Feature: Offline DSP test harness

## Problem

Every other spec in this repo has acceptance criteria phrased as measurements: RT60
within ±15%, group delay monotonically decreasing, aliasing 60 dB down, impedance rising
at 6 dB/octave. None of those can be asserted without measurement code.

TDD requires the test to come first. For DSP that means the **measurement tools** have to
exist before the first real test, or every spec stalls at its first RED step. This spec
builds them.

It is deliberately the spec that gets implemented first, even though it ships no audible
behaviour.

## Requirements

### Must have

1. The test target SHALL build and run without linking JUCE, using only the headers under
   `Source/dsp/`.
2. The system SHALL provide a measurement library covering: RMS, peak, dB conversion,
   FFT magnitude spectrum, steady-state frequency response, RT60, group delay per band,
   THD, aliasing floor, and autocorrelation peak detection.
3. The system SHALL provide signal generators: impulse, sine, swept sine, white noise
   with an explicit seed, DC, and silence.
4. Every measurement SHALL be deterministic — identical input produces identical output
   across runs and across build configurations.
5. Tests SHALL run under CTest, and `ctest --output-on-failure` SHALL report which
   assertion failed with the measured value and the expected tolerance.
6. A failing measurement SHALL print the measured value, the expected value, and the
   tolerance. "Assertion failed" alone is useless when the thing being asserted is a
   number with a tolerance.
7. The suite SHALL complete in under 30 seconds on a developer machine, so it can run on
   every save.
8. The system SHALL provide a stability harness that runs a DSP block against DC,
   full-scale noise, and randomised parameter sweeps, asserting finite bounded output.

### Nice to have

- A CSV dump mode so a failing frequency response can be plotted.
- `tools/analyze.cpp` as a standalone binary for ad-hoc measurement of a rendered file.
- Benchmark timings recorded per build so a CPU regression is visible.

### Non-goals

- No audio-file golden-master comparison as a pass/fail gate. Rendered audio is a
  listening diagnostic; float differences across platforms make it a flaky oracle. See
  Development Standards.
- No plugin-host integration tests (pluginval, auval) in this spec. Those are release
  checks, not the TDD loop.
- No UI tests beyond the two sprite-geometry tests specified in SPEC-005.

## Design

### Layout

```
tests/
  CMakeLists.txt
  test_main.cpp              Catch2 main
  support/
    Measure.h                RMS, peak, dB, FFT, response, RT60, group delay, THD, autocorr
    Generate.h               impulse, sine, sweep, seeded noise, DC, silence
    Stability.h              the worst-case input harness
    Approx.h                 dB / Hz / ms tolerance helpers with useful failure messages
  test_measure.cpp           the measurement tools test themselves
  test_dispersion.cpp        SPEC-001
  test_decay.cpp             SPEC-001
  test_tank.cpp              SPEC-001
  test_send_filter.cpp       SPEC-002
  test_tank_load.cpp         SPEC-002
  test_tube_stages.cpp       SPEC-002
  test_mix_network.cpp       SPEC-002
  test_circuit_models.cpp    SPEC-002
  test_parameters.cpp        SPEC-003
  test_parameter_ids.cpp     SPEC-003
  test_state.cpp             SPEC-003
  test_presets.cpp           SPEC-004
  test_sprite_atlas.cpp      SPEC-005
  test_sprite_manifest.cpp   SPEC-005
```

### Measuring the measurement tools

`test_measure.cpp` exists because a measurement tool that is quietly wrong turns every
other test into a lie that passes. Each tool is verified against a signal whose answer is
known analytically:

| Tool | Verified against |
| --- | --- |
| RMS | Unit sine, expect 0.7071 |
| Frequency response | One-pole low-pass at a known cutoff, expect −3 dB at cutoff and −6 dB/octave above |
| RT60 | Synthetic exponential decay with a known time constant |
| Group delay | Pure delay line, expect flat group delay equal to the delay |
| Group delay, dispersive | Single allpass with an analytically known group delay |
| THD | Hard clipper at a known threshold, expect the known harmonic series |
| Autocorrelation peak | Two impulses at a known spacing |
| FFT | Sine at a bin centre, expect a single non-zero bin |

### Tolerance helpers

Tolerances are expressed in the units the spec uses, not as raw float epsilons:

```cpp
REQUIRE_THAT(measuredRt60, WithinMs(3400.0, 15.0_percent));
REQUIRE_THAT(measuredCorner, WithinHz(318.0, 15.0_percent));
REQUIRE_THAT(measuredGain, WithinDb(-12.0, 2.0));
```

A failure prints `measured 2810 ms, expected 3400 ms +/- 15% (2890..3910 ms)` — which
tells you how far off you are and in which direction. That is the difference between a
test suite you use and one you disable.

### Determinism

- Noise generators take an explicit seed; there is no unseeded overload.
- No test depends on wall-clock time. The CPU benchmarks report but never assert.
- `-ffast-math` is **not** enabled for the test target, so float behaviour is predictable.
- Tests that must compare across sample rates do so in seconds and hertz, never samples.

## Edge cases

- [ ] RT60 on a signal that never decays 60 dB within the analysis window — must report
      "insufficient decay" rather than returning a plausible wrong number
- [ ] Group delay where phase wraps — unwrap before differentiating
- [ ] FFT of a signal shorter than the window — zero-pad explicitly, do not read past the
      end
- [ ] THD where the fundamental is not at a bin centre — use a window and a peak
      interpolation, or choose test frequencies at bin centres (preferred)
- [ ] Autocorrelation on a signal with no periodicity — must report no peak found
- [ ] Measurement on all-zero input — must not divide by zero

## Acceptance criteria

1. `cmake --build build --target birbverb_tests` succeeds with **no JUCE in the link
   line**. Verified by inspecting the link command in CI.
2. Every tool in `Measure.h` passes its analytical verification in `test_measure.cpp`.
3. Two consecutive full runs produce byte-identical output.
4. A deliberately broken assertion prints the measured value, expected value and
   tolerance range.
5. Full suite completes in under 30 s.
6. `Stability.h` run against a trivially unstable filter (a one-pole with a pole outside
   the unit circle) **fails**, proving the harness detects instability rather than
   passing everything.
7. RT60 on a synthetic decay with a 1.0 s time constant measures 1.0 s ±2%.
8. Group delay on a 480-sample delay line at 48 kHz measures 10 ms ±0.1 ms, flat across
   the band.

Criterion 6 is the one that matters most. A stability harness that has never been seen to
fail is not known to work.

## Constraints

- Catch2 v3 via CMake FetchContent, pinned to an exact tag — see
  [ADR-004](../adrs/ADR-004-catch2-for-dsp-tests.md).
- The test target must not depend on `Source/PluginProcessor.*` or anything JUCE. Tests
  that genuinely need JUCE types (state round-trip, preset manager, sprite manifest) go in
  a second target, `birbverb_juce_tests`, which may link JUCE.
- FFT in the test support code may be a simple radix-2 implementation; this is test code
  and clarity beats speed.

## Tasks

- [ ] CMake: Catch2 FetchContent, `birbverb_tests` target, CTest registration
- [ ] `support/Generate.h` — generators, all seeded
- [ ] `support/Approx.h` — dB / Hz / ms matchers with useful messages
- [ ] `support/Measure.h` — RMS, peak, dB, FFT
- [ ] `test_measure.cpp` RED/GREEN for each of the above
- [ ] `support/Measure.h` — frequency response, RT60, group delay, THD, autocorrelation
- [ ] `test_measure.cpp` RED/GREEN for each, against the analytical table
- [ ] `support/Stability.h` plus the deliberately-unstable-filter test (criterion 6)
- [ ] Second target `birbverb_juce_tests` for the JUCE-dependent tests
- [ ] CI: run both targets, fail the build on any failure
- [ ] Document the measurement patterns in
      [testing-dsp](../../documentation/guides/testing-dsp.md)

## Notes

- **Implement this spec first.** Every other spec's RED phase depends on it.
- Test framework decision: [ADR-004](../adrs/ADR-004-catch2-for-dsp-tests.md)
- Testing conventions: [testing-dsp](../../documentation/guides/testing-dsp.md)
- zoo-tron's `tools/engine_test.cpp` is the ancestor of this: a hand-rolled `main()` that
  printed measurements and returned non-zero on failure. It worked, and it did not scale
  past about a dozen checks, which is why this one is a real framework.
