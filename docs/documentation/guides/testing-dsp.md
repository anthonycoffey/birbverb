# Testing DSP

How to write tests in this repo. The framework decision is
[ADR-004](../../specs/adrs/ADR-004-catch2-for-dsp-tests.md); the harness is
[SPEC-006](../../specs/active/SPEC-006-offline-dsp-test-harness.md).

## The core problem

Audio DSP does not test like application code. You cannot assert that sample 4817 equals
`0.3718`. Change the filter topology, change the compiler, change the optimisation level,
and that number moves while the thing still sounds identical.

So: **assert on measured properties, with tolerances, in the units the spec uses.**

| What you are verifying | How |
| --- | --- |
| Filter shape | Sine at N frequencies, steady-state RMS, assert the dB slope |
| Decay time | Impulse in, Schroeder-integrated RT60 |
| Dispersion | Impulse in, group delay per band, assert LF arrives later than HF |
| Echo spacing | Impulse in, autocorrelate the tail, assert the first peak's lag |
| Nonlinearity | Bin-centred sine in, FFT out, assert harmonic ratios |
| Aliasing | Same, assert inharmonic products are N dB down |
| Stability | Worst-case input, assert finite and bounded |
| Smoothing | Step a parameter mid-block, assert no jump above a threshold |

## The TDD loop

Tests written ahead of their implementation are tagged `[.red]`. The leading dot makes
Catch2 hide them from the default run, so CI stays green on a scaffold while the tests
that describe unbuilt behaviour still exist and still run on demand.

```bash
# 1. RED — write the test, tagged [.red], and watch it fail
./build/birbverb_tests "[red]"

# 2. GREEN — minimum code to pass
cmake --build build --target birbverb_tests -j
./build/birbverb_tests "[red]"

# 3. Drop the leading dot from the tag so it joins the default suite
ctest --test-dir build --output-on-failure
```

**Step 1 is not optional.** A test that has never been seen to fail has not been shown to
test anything — it is entirely possible to write an assertion that passes against silence,
and `test_dispersion.cpp` has a `REQUIRE (rms (out) > 1.0e-6)` in it specifically because
the first version of its stability test did exactly that.

## Tolerances

Use the matchers in `support/Approx.h`. They print the measured value, the expected value
and the resulting window:

```cpp
REQUIRE_THAT (r.seconds,      WithinSeconds (3.4, Percent (15.0)));
REQUIRE_THAT (corner,         WithinHz (318.0, Percent (15.0)));
REQUIRE_THAT (gainDb,         WithinDb (-12.0, 2.0));
REQUIRE_THAT (delay * 1000.0, WithinMs (10.0, 0.1));
```

A failure reads:

```
measured 2.810 s, expected 3.400 s +/- 15.0% (2.890..3.910 s)
```

which tells you how far off you are and in which direction. Never use a bare float
epsilon, and never use `REQUIRE (std::abs (a - b) < 0.01)` — the failure message from that
is useless and you will end up adding print statements instead of reading it.

**Justify the tolerance in a comment.** Where did ±15% come from? In this repo it is
usually the width of a datasheet range. If you cannot say where it came from, you are
asserting a guess.

## Determinism

- `noise()` takes an explicit seed. There is no unseeded overload, on purpose.
- Never assert on wall-clock timing. CPU benchmarks report; they never assert.
- Compare across sample rates in **seconds and hertz**, never samples.
- No fast-math on the test target.

An unseeded or timing-dependent test fails once a fortnight for reasons nobody can
reproduce, and the standard response is to disable it. One flaky test costs more than the
bug it was meant to catch.

## Measurement gotchas

**Put the fundamental on a bin centre.** `sineAtBin (fftSize, bin, fftSize, fs)` gives a
signal that is exactly periodic in the FFT window, so there is no spectral leakage and no
windowing is needed. Measure THD any other way and leakage from the fundamental swamps the
harmonics.

**RT60 can lie.** Schroeder backward integration always appears to decay near the end of
a buffer, because the integration window is shrinking rather than the signal stopping.
`rt60()` guards against a −35 dB crossing in the last 1% and reports
`sufficientDecay = false` instead. **Always check that flag** — ignoring it is how you get
a confident wrong number.

**Give the tail room.** RT60 needs a buffer several times the expected decay. A 3.4 s decay
wants at least 12 s.

**Unwrap phase before differentiating.** `groupDelay()` does this; if you write your own
phase analysis, remember it.

**Autocorrelation needs a threshold.** Without one, aperiodic input returns whatever lag
happened to score highest. `autocorrelationPeak()` returns `nullopt` below the threshold.

## Writing a cross-model ordering test

For the four circuit models, some properties have no trustworthy absolute value — the
Ampeg study is low confidence and most of its numbers are inferences. Asserting an absolute
figure there means asserting our guess.

Assert the **ordering** instead:

```cpp
// The Ampeg has no reverb transformer, so its tank drive must be far below the
// AB763's. SPEC-002 acceptance criterion 4.
REQUIRE (ampegDrive < ab763Drive * 0.25);
```

This still catches the failure that would actually matter — an Ampeg preset that drips like
a Twin — without pretending to precision we do not have. Do not tighten these into absolute
assertions without new research.

## What not to test

**No golden-file wav comparison.** Rendered audio is a listening diagnostic
(`tools/render.cpp`), not a pass/fail oracle. Resampling and compiler-level float
differences make it flaky across platforms, and a flaky gate gets disabled. Assert on
measured properties of the render.

**No timing assertions.** CPU budgets are in the specs and measured by benchmarks that
report rather than fail. A CI machine under load would otherwise fail the suite for no
reason.

## File layout

```
tests/
  support/
    Generate.h    signal generators, all seeded
    Measure.h     RMS, FFT, response, RT60, group delay, THD, autocorrelation
    Approx.h      dB / Hz / ms / s matchers with readable failure messages
    Stability.h   the worst-case-input harness
  test_measure.cpp   verifies the measurement tools against analytical answers
  test_*.cpp         one file per spec area
```

`test_measure.cpp` is the one to read first, and the one to extend whenever you add a
measurement tool. It exists because a measurement tool that is quietly wrong turns every
test using it into a lie that passes.
