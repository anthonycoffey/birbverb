# Handoff — picking this up on macOS

Everything in this repo was written on Windows on a machine with **no CMake**. So the
docs, the specs, the research and the JUCE-free DSP are real and verified; **nothing has
ever been configured, built or linked by CMake, and the JUCE half has never been
compiled at all.**

That is the one thing to know before you start. The first hour on the Mac is a build
shakedown, not feature work.

Read this, then [PLAN-001](docs/specs/plans/PLAN-001-v0.1-roadmap.md) for where the
project is going, and [CLAUDE.md](CLAUDE.md) for the conventions.

---

## 1. First commands

```bash
brew install cmake
```

```bash
xcode-select --install
```

Then, from the repo root — the first configure downloads JUCE 8.0.14 and Catch2 v3.7.1,
so it needs a few minutes and a network connection:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

Build the tests before the plugin. They are the half that is verified, they build in
seconds, and they do not link JUCE:

```bash
cmake --build build --target birbverb_tests -j
```

```bash
ctest --test-dir build --output-on-failure
```

Then the plugin itself — VST3, Standalone, and AU, which only exists on Apple:

```bash
cmake --build build -j
```

```bash
auval -v aufx Bvrb Ynme
```

`COPY_PLUGIN_AFTER_BUILD` is on, so the VST3 and AU land in `~/Library/Audio/Plug-Ins/`
by themselves.

## 2. What `ctest` should print

There are two test entries, and the second one is **supposed** to fail:

| Entry | Expectation |
| --- | --- |
| `implemented` | passes — `test_measure.cpp`, `test_stability.cpp`, `test_dispersion_curve.cpp` |
| `red` | the Catch2 run fails, and CTest reports the *test* as passed because of `WILL_FAIL TRUE` |

`red` runs the tests tagged `[.red]` — written ahead of their implementation. The leading
dot hides them from the default run. When you implement something, drop the dot from its
tag and it joins `implemented`.

```bash
ctest --test-dir build -L red --output-on-failure
```

A test that has never been seen to fail has not been shown to test anything, so that
entry is deliberate rather than neglect.

## 3. Where the trouble will be

In rough order of likelihood. None of this is known-broken; it is all simply never-built.

1. **`tests/support/Approx.h`.** Custom Catch2 matchers deriving from
   `MatcherGenericBase`, with `match(double)` by value and an overridden `describe()`.
   The API usage was written from the Catch2 docs and never compiled. If one thing in the
   test suite fails to build, expect it to be this.
2. **`Source/ui/SpriteStore.h` and `SpriteAtlas.h`.** JUCE `File`, `JSON`, `Image` and
   `Graphics` calls, never compiled. `SpriteStore` already had one JUCE API mistake caught
   by reading the headers (`juce::HashMap` has no pointer-returning `find()`, so it uses
   `std::map` now) — there may be more of that kind.
3. **`Source/PluginEditor.cpp`.** The largest body of uncompiled JUCE: custom
   `LookAndFeel_V4`, five sprite control classes, and the preset bar with its browser and
   save dialog.
4. **Clang versus MSVC.** The JUCE-free half is clean at MSVC `/W4 /permissive-`, but
   `juce_recommended_warning_flags` is enabled and Clang finds a different set of things.
   Expect sign-compare and unused-parameter noise.
5. **`juce_add_binary_data`.** Guarded by `BIRBVERB_HAS_ASSETS`, which is 0 because no
   PNGs are committed. If the guard is wrong, CMake will complain about an empty
   `SOURCES` list. Nothing depends on the assets target existing.
6. **Universal binaries.** `CMakeLists.txt` sets a universal architecture on Apple.
   If that fights your toolchain, build one arch while you shake things out:
   `-DCMAKE_OSX_ARCHITECTURES=arm64`.

Optional targets, off by default:

```bash
cmake -B build -DBIRBVERB_BUILD_TOOLS=ON && cmake --build build --target bv_render -j
```

`bv_render` writes a wav per preset for listening. It is worth running once you have
audio, and pointless before then — see section 5.

## 4. What is actually verified, and how

Since CMake was unavailable, the JUCE-free half was compiled with MSVC 14.51 at
`/W4 /permissive-` and **run** against signals with analytically known answers. That
found and fixed three real bugs, which is the only reason to mention it:

| Bug | What was wrong |
| --- | --- |
| `rt60()` returned nothing for any input | The normalising loop divided by `curve[0]` *inside* the loop, after that element had been overwritten. Everything became `+inf`. Every RT60 assertion in the repo would have silently failed |
| `findCornerHz()` only worked for high-pass shapes | Inverted bisection. Returned 8000 Hz for a 318 Hz low-pass. Now derives direction from the brackets |
| `tools/render.cpp` called a function that does not exist | Used `writeWav(...)`; the real API in `tools/wav.h` is `wav::write(path, interleaved, frames, channels, rate)` |

Measured state after the fixes: RMS 0.707107 on a unit sine; RT60 exact across 0.8–4.5 s
and 44.1–192 kHz; group delay 10.0000 ms on a 480-sample delay at 48 kHz; corner finder
within 0.2% for both filter shapes at 100 / 318 / 1000 / 4000 Hz; the stability harness
correctly **fails** a divergent filter, a NaN producer, and a coefficient crossing 1.0
mid-sweep.

The dispersion work in `Source/dsp/Dispersion.h` and `Source/dsp/TankConfig.h` was
verified the same way — all of `test_dispersion_curve.cpp`'s assertions were run outside
Catch2 and pass, including the five published spring geometries reproducing their
published `T_D` and `F_C` to within 1%.

**Unverified:** every CMake target, all four Catch2 test files as Catch2 code, and all of
`Source/ui/` and `Source/Plugin*`.

## 5. The state of the audio

**There is no reverb yet.** `Source/dsp/SpringTank.h` is a documented skeleton that
returns silence. The plugin builds, loads, shows its UI, moves all ten parameters,
switches presets and round-trips state — and passes audio through dry.

What changed this session is that the thing blocking the tank is now unblocked.
[SPEC-001](docs/specs/active/SPEC-001-spring-tank-dsp-core.md) is `ready`: the dispersion
curve is chosen, derived and recorded, with every coefficient computed from published
caliper measurements rather than guessed. The write-up is
[spring-dispersion-and-allpass-fitting](docs/documentation/deep-dives/spring-dispersion-and-allpass-fitting.md).

**Read the first two paragraphs of that document before you write any allpass code.** It
corrects something four earlier documents in this repo had backwards: the main chirp
**ascends**. Group delay rises with frequency and peaks at the transition frequency `F_C`,
so low frequencies arrive first. The descending chirp everyone describes is a *secondary*
series, at least 30 dB down. A model that only descends has modelled the artefact and
thrown away the sound.

## 6. Next steps, in order

### Step 0 — make the build work

Nothing below matters until `cmake --build build -j` and `ctest` both behave. Expect to
spend the time; see section 3.

### Step 1 — `DispersiveAllpass`

The first real DSP. One stretched first-order allpass section with a **fractional** stretch
factor.

```
A(z)        = (a + z^-1) / (1 + a*z^-1)
H_single(z) = A(z^k) = (a + z^-k) / (1 + a*z^-k)
H_cascade   = A^M(z^k)
```

`Source/dsp/Dispersion.h` already has every closed form you need, tested:
`stretchFactor()` for `k` from `F_C`, `coeffForPeakDelay()` for `a` from a target chirp
length, `groupDelaySamples()` to check your work, and `coeffForFractionalDelay()`
(Parker eq. 6) for the fractional remainder.

**The fractional part is not optional.** Rounding `k` to an integer makes `F_C` jump with
the sample rate — at 48 kHz, `k = 5` gives 4800 Hz and `k = 6` gives 4000 Hz — which
breaks SPEC-001 requirement 9.

Write `tests/test_dispersive_allpass.cpp` first, tagged `[.red]`, asserting measured group
delay against `dispersion::groupDelaySamples()`. `Measure.h`'s `groupDelay()` already
agrees with that closed form to 0.25%, so this is a tight test, not a loose one.

### Step 2 — `SpringLine`

Delay loop, allpass cascade, loss filter, reflections at both transducers. Parameters come
straight off `TankConfig`: `springs[i].echoSpacingMs()`, `springs[i].transitionHz()`,
`loopGain()`.

Watch the structural tension in SPEC-001 requirement 7: group delay peaks at `F_C`, so
energy there makes *fewer* round trips per second, and a flat loss filter would make `F_C`
decay **slower** than the rest of the band — the opposite of what a real tank does. The
loss filter needs real low-pass slope. The test for that requirement is not a formality.

### Step 3 — `SpringTank`

N `SpringLine`s summed, plus the secondary wideband loop at −30 dB with `a_hf = -0.6`.
Then drop the `.` from the `[.red]` tags in `test_dispersion.cpp` and `test_decay.cpp` and
make them pass.

### Step 4 — listen

```bash
cmake --build build --target bv_render -j && ./build/bv_render
```

A wav per preset. The fourteen derived presets in `Source/dsp/PresetData.h` were written
**without an engine to hear** and are expected to be wrong; SPEC-004 requires listening
before they are final. This is where that starts.

### Step 5 — SPEC-002, the drive and recovery stages

What makes the four amps four amps rather than one reverb with four decay times. The spec
is written and the research is done; it is `draft` rather than `ready` only because the
tank has to work first.

### In parallel, any time — the sprites

The art is independent of all of the above, because every control falls back to procedural
drawing. See [SPRITES.md](SPRITES.md) for the work order and
[sprite-authoring](docs/documentation/guides/sprite-authoring.md) for exact dimensions and
file format.

## 7. Decisions settled this session

| Question | Answer |
| --- | --- |
| **Licence** | **AGPLv3**, in [LICENSE](LICENSE). *Not* GPLv3 — JUCE states its own terms as "licensed under both the open source AGPLv3 and a commercial JUCE licence", and the two are not interchangeable. JUCE 8's free Starter tier was the alternative, but its terms forbid releasing under a licence requiring source disclosure, i.e. forbid being open source |
| **Dispersion curve** | Parker (2011) eq. (4) for the cascade, physical targets from Bilbao & Parker (2009) eqs. (2)–(4). Both **open access**, which is why they beat the paywalled originals: a constant sourced to a paper nobody can open is a constant on trust |
| **Ampeg revision** | The first-edition **R-12-R, May 1961** — the one revision with a dated factory schematic. Preset renamed `Rocket '61` → `Rocket R-12-R`, because Ampeg's plain **Rocket (R-12) is a different amp with no reverb** and the old name read ambiguously |
| **Echo Jet** | Gets its own preset, `Echo Jet '63`. It turned out to be a genuinely different design — 1963, 6D10 preamp, 2 x 7591A, speaker-driven tank on its own physically separated speaker — not a small Reverberocket. With no schematic it is *voiced on* the Reverberocket circuit and says so, rather than getting a circuit model built out of invented constants |
| **1962 Reverberocket** | Ships as the derived preset `Rocket '62`. The revision is real and documented, but only anecdotally (forum comparison of two factory schematics: V3a plate 5.6 kΩ → 2.2 kΩ, cathode 1.5 kΩ → 330 Ω). Promote it to a second circuit model if a primary source confirms those values |

## 8. Still open

- **Nobody has calipered an Accutronics tank.** The Long 2-Spring configuration — the
  Fender blackface tank, the most important one in the bank — is *interpolated* from
  other manufacturers' measured springs. Four micrometer readings on a real `4AB3C1B`
  (helix length, helix diameter, turns, wire diameter) close it exactly, with no fitting,
  via Bilbao & Parker eqs. (2)–(4). Cheapest research win available.
- **Series or parallel springs?** Both measured units in the literature are parallel, and
  birbverb models parallel. Series would roughly double echo spacing.
- **A 6G16 Vibroverb factory schematic**, and the 6G15's Tone component values — the
  StewMac kit manual is the best unexplored lead.
- **An Ampeg Reverberocket schematic.** Highest-value next step for that study, which is
  `low` confidence and the weakest research in the repo.
- **A low-CPU "eco" mode.** ADR-001 rejected FDN-with-dispersion as the engine but noted
  it as a possible cheap mode. Needs a decision, then a spec. Related: Parker's
  chirp-straightening cuts the allpass cost to about a third and is documented in the
  dispersion study as the first optimisation to reach for.
- **CPU.** `M_low = 100` per spring plus `M_high = 200` means a 3-spring tank runs **500
  allpass sections per sample**. SPEC-001 budgets 2% of one core at 48 kHz. That budget
  is the binding constraint on the whole design, and it has never been measured.

## 9. Repository conventions worth knowing before the first commit

- **Branch, then PR.** The initial import went straight to `main` by explicit decision;
  from here on, `feature/`, `fix/`, `chore/`, `refactor/`, `research/`.
- **Commits:** `<type>: <description> [SPEC-XXX]`, imperative mood. Types: `feat`, `fix`,
  `chore`, `refactor`, `docs`, `test`, `style`, `research`, `assets`.
- **`Source/dsp/` must never include JUCE.** The test target links `bv_dsp` and Catch2 and
  *not* JUCE, so a stray `#include <JuceHeader.h>` in a DSP header breaks the test build.
  That is the point — a link-time guarantee rather than a convention.
- **Every constant carries its source**, or the word `INFERRED` or `FITTED`. A bare
  numeric constant in a DSP file is a review failure, because the project's whole claim is
  that behaviour traces to hardware and a constant without provenance quietly breaks it.
  `Source/dsp/TankConfig.h` is the pattern to copy: it stores spring *geometry* from a
  published table and derives every delay, so there is nothing to transcribe wrong.
- **Research before spec, spec before test, test before code.** If you are writing DSP and
  cannot name the spec it satisfies, write the spec. If you are writing a spec about
  vintage hardware and cannot name the circuit study behind it, write the study.
