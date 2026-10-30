# birbverb

An open-source vintage spring reverb plugin (VST3 / AU / Standalone), built with
[JUCE](https://juce.com) — modelled on the actual circuits, not on a general impression
of them.

Spring reverb is not a room. A wave in a helical spring travels at a frequency-dependent
speed, so an impulse comes back as a **chirp**, and after several round trips between the
transducers those chirps overlap into the sound everyone recognises. No conventional
reverb topology produces that, because conventional reverbs are built from delays where
every frequency arrives at the same time.

The chirp **ascends**: group delay rises with frequency and peaks at a transition
frequency set by the spring's wire and helix radius. There is a second, descending chirp
series across the whole band, but it sits at least 30 dB down. Most descriptions of spring
reverb — including four earlier documents in this repo — have that backwards. The
derivation, with sources, is in
[spring-dispersion-and-allpass-fitting](docs/documentation/deep-dives/spring-dispersion-and-allpass-fitting.md).

birbverb models the spring, and then models what each amp did to it.

## The circuits

Four reverb circuits, researched and documented before any code was written:

| Preset | Circuit | What makes it different |
| --- | --- | --- |
| **Twin '64** / **Super '63** | Fender AB763 blackface | Both halves of a 12AT7 through a step-down transformer, behind a 500 pF cap that strips everything below ~318 Hz. Wet injected through a 3.3 MΩ resistor with a 10 pF bright cap. Bright, splashy, sits behind the dry. |
| **Vibroverb '63** | Fender Vibroverb 6G16 | The first Fender with reverb built in. A simple low-impedance resistive mixer with no bright cap — darker and more present than a blackface. |
| **Drip Tank** | Fender Reverb Unit 6G15 | A **6K6 power tube** drives the tank. Far more current than any amp's driver, plus a real Dwell control. This is the surf sound. |
| **Rocket R-12-R** | Ampeg Reverberocket R-12-R, 1961 | **No reverb transformer at all** — one 6SN7 half drives the springs directly. The first production amp with reverb, two years before the Vibroverb, and it sounds nothing like a Fender: dark, soft, diffuse. |

The research is in [`docs/documentation/circuits/`](docs/documentation/circuits/), with
every component value sourced and every unverified claim marked as such. Two of the five
studies are low confidence, and the presets that rest on them say "voiced after" rather
than claiming to be the circuit. That distinction is a requirement, not a disclaimer.

## Presets as foundations

Selecting a preset sets **every** control — including which circuit and which tank — so
you land on a complete, coherent starting point and build from there. Five foundation
presets, one per researched circuit, plus fourteen variations that each record the
foundation they came from.

## Controls

| Control | What it does |
| --- | --- |
| **Dwell** | How hard the driver pushes the tank. Not wet level — this is the character control, and the thing most spring plugins collapse into a single knob |
| **Decay** | Tank RT60, 0.8 to 4.5 s |
| **Tone** | Wet-path tone |
| **Lo Cut** | The send high-pass — the 500 pF coupling cap. 318 Hz is the stock Fender value |
| **Boing** | Dispersion depth. An effect control, not a modelled property, and labelled as one |
| **Mix** | Dry/wet, with a law that follows the active circuit's mix network |
| **Level** | Output trim, ±12 dB |
| **Circuit** | AB763 / 6G16 / 6G15 / Ampeg |
| **Tank** | Short 2-spring / Long 2-spring / Long 3-spring |

## Status

**Early. Phase 0 of [PLAN-001](docs/specs/plans/PLAN-001-v0.1-roadmap.md).**

What works: the plugin builds and loads, all ten parameters are automatable, presets
switch, state round-trips, and the measurement test suite runs.

What does not: **there is no reverb yet.** `SpringTank` is a documented skeleton and the
plugin currently passes audio through. The tests that describe the unbuilt behaviour exist
and are tagged `[.red]`.

That is on purpose. Phase 0 ships no audio because four of the six specs cannot start
their RED phase until the measurement library exists, which makes it the critical path
rather than overhead.

What is now unblocked: [SPEC-001](docs/specs/active/SPEC-001-spring-tank-dsp-core.md) is
`ready`. The dispersion curve the whole model rests on is chosen, derived and recorded,
with every tank coefficient computed from published caliper measurements of real springs
rather than guessed.

Two documents to start from:

- **[HANDOFF.md](HANDOFF.md)** — what is verified and what has never been built, the
  first commands to run, where the build will break, and the next steps in order.
- **[SPRITES.md](SPRITES.md)** — the UI art work order. Independent of the DSP, because
  every control falls back to procedural drawing.

## How this project works

Research first, then a spec, then a failing test, then code:

```
0. Research  → circuit study in docs/documentation/circuits/, sources cited
1. Spec      → docs/specs/active/, acceptance criteria as measurements
2. Approve   → spec reaches status: ready
3. RED       → write the failing test
4. GREEN     → minimum code to pass
5. REFACTOR  → clean up, tests stay green
```

Every audible behaviour has to be **derived** from documented hardware, **inferred** from
it and labelled as an inference, or a **deliberate liberty** stated as such. "It sounds
about right" is not one of the three.

- [`docs/README.md`](docs/README.md) — how the docs are organised
- [`docs/documentation/development-standards.md`](docs/documentation/development-standards.md) — the process, and how to test DSP
- [`docs/specs/`](docs/specs/) — specs, ADRs, and the roadmap
- [`CLAUDE.md`](CLAUDE.md) — context for AI agents working in this repo

## Build

Requires CMake ≥ 3.22 and a C++20 toolchain. JUCE and Catch2 are fetched automatically.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
```

Artifacts land in `build/birbverb_artefacts/`. With `COPY_PLUGIN_AFTER_BUILD`, the VST3
(and AU on macOS) are also installed to your user plugin folders.

## Test

```sh
ctest --test-dir build --output-on-failure
```

That runs the implemented suite. The tests written ahead of their implementation are
hidden from the default run and can be checked explicitly:

```sh
ctest --test-dir build -L red --output-on-failure
```

## UI

A fixed 600 × 650 pedal face composited from sprite atlases rather than drawn
procedurally, because powder-coat texture and the way a knurled knob catches light are
photographic properties, not gradient stacks
([ADR-002](docs/specs/adrs/ADR-002-sprite-based-ui-rendering.md)).

No art is committed yet, and every control falls back to procedural drawing until it is,
so a fresh clone builds a fully usable plugin. Authoring instructions, including exact
dimensions for every sheet, are in
[sprite-authoring](docs/documentation/guides/sprite-authoring.md).

## License

**GNU Affero General Public License v3.0 or later.** Full text in [LICENSE](LICENSE).

Not GPLv3, which is what an earlier note here guessed. JUCE states its own terms as
"licensed under both the open source AGPLv3 and a commercial JUCE licence", so AGPLv3 is
the open-source option JUCE itself offers and the one a plugin linking it inherits. AGPL
and GPL are not interchangeable, so the distinction matters.

The alternative was JUCE 8's free **Starter** commercial tier (no cost up to $20,000
annual revenue), but its terms prohibit releasing the result under a licence that
requires source disclosure — which rules out being open source at all. birbverb is an
open-source project whose entire premise is that you can read why it sounds the way it
does, so that was never a real option.

What this means in practice:

- You can use, study, modify and redistribute birbverb freely.
- If you distribute a modified version, you must publish your source under AGPLv3 too.
- The network clause that distinguishes AGPL from GPL concerns software users interact
  with over a network. A desktop audio plugin does not, so for almost everyone AGPLv3
  behaves like GPLv3 here.
- If you want to ship a closed-source product built on this code, you need a commercial
  JUCE licence *and* permission from every birbverb copyright holder. Ask first.
