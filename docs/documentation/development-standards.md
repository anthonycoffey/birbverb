# Development Standards

This document defines how we work on birbverb — the development process, testing
philosophy, spec lifecycle, and version control conventions.

---

## Development Process

We follow a **research-first, spec-driven** approach. No DSP block and no UI control
begins without a written spec, and no spec about a vintage circuit begins without a
circuit study behind it.

```
0. Research  → Circuit study in docs/documentation/circuits/. Cite sources. Flag unknowns.
1. Plan      → Understand the problem. Identify what we cannot model and why.
2. Spec      → Write a spec in docs/specs/active/.
3. Approve   → Get spec status to `ready` before writing code.
4. Test      → Write failing tests first (RED phase).
5. Implement → Write the minimum code to make tests pass (GREEN phase).
6. Refactor  → Clean up the implementation while keeping all tests green.
7. Review    → PR opened, spec status -> `review-pending`.
8. Ship      → Merge. Spec status -> `complete`. Move to archive.
```

Specs serve as the contract between intent and implementation. If you are writing code
without a spec, stop and write one first.

### The research rule

Every audible behaviour in birbverb must be one of three things:

1. **Derived** — traceable to a documented property of real hardware, with the circuit
   study cited in the spec.
2. **Inferred** — a reasonable consequence of a documented property, labelled as an
   inference in the spec's Design section.
3. **A deliberate liberty** — something chosen for musicality or CPU cost, stated as
   such in the spec's Non-goals or Constraints.

"It sounds about right" is not one of the three. If a value is a guess, say so in the
spec so the next person knows it is fair game to re-derive.

---

## Test-Driven Development (TDD)

We use the RED → GREEN → REFACTOR cycle. Tests live in `tests/` and run under Catch2
via CTest — see [ADR-004](../specs/adrs/ADR-004-catch2-for-dsp-tests.md).

### RED — Write a failing test

Write a test that describes the desired behaviour, taken from a spec's Acceptance
Criteria. Run it. It must fail before you write any implementation code.

```bash
cmake --build build --target birbverb_tests -j
ctest --test-dir build --output-on-failure
```

### GREEN — Make the test pass

Write the minimum code necessary. Do not over-engineer. Do not add DSP features that no
test covers.

### REFACTOR — Clean up

With tests green, improve the code: extract duplication, clarify naming, simplify.
Run tests after every change.

### What a DSP test looks like

Audio DSP does not test like application code. Use these patterns rather than asserting
on individual sample values:

| What you are verifying | How to test it |
| --- | --- |
| Filter shape | Drive a sine at N frequencies, measure steady-state RMS, assert on the dB slope |
| Decay time (RT60) | Impulse in, measure the time for the envelope to fall 60 dB |
| Dispersion / chirp | Impulse in, measure group delay per band, assert LF arrives later than HF |
| Echo spacing | Impulse in, autocorrelate the tail, assert the first peak sits at the expected delay |
| Nonlinearity | Sine in, FFT out, assert harmonic ratios and that aliasing stays below a floor |
| Stability | Feed worst-case input (DC, full-scale noise, fast parameter sweeps), assert the output stays finite and bounded |
| Parameter smoothing | Step a parameter mid-block, assert no sample-to-sample jump above a threshold |
| Preset integrity | Load every factory preset, assert every parameter lands in range and round-trips |

**Every DSP test must be deterministic.** Seed any noise. Never assert on wall-clock
timing. Never assert on exact floats — use tolerances expressed in dB, Hz, or ms, with
the tolerance justified in a comment.

### Golden-file tests

Rendered-audio comparisons (`tools/render.cpp`) are a diagnostic, not a gate. Do not
commit `.wav` fixtures as pass/fail oracles — resampling and compiler-level float
differences make them flaky across platforms. Assert on *measured properties* of the
render instead.

---

## Spec Lifecycle

```
draft → ready → in-progress → review-pending → complete
                                    ↓
                               deprecated (from any status)
```

| Status | Meaning | Who sets it |
| --- | --- | --- |
| `draft` | Being written; not ready for development | Author |
| `ready` | Approved for development; requirements are complete | Author + reviewer |
| `in-progress` | Development is underway | Developer |
| `review-pending` | Implementation complete; awaiting code review | Developer |
| `complete` | Shipped, verified, and closed | Reviewer |
| `deprecated` | No longer relevant; superseded or cancelled | Author |

**Rules:**

- Never start coding on a spec with status `draft`
- ADR status transitions: `proposed` → `accepted` | `deprecated` | `superseded`
- ADRs are never deleted or archived — they are permanent records

---

## Version Control Conventions

### Branch naming

```
feature/short-description      # New feature
fix/short-description          # Bug fix
chore/short-description        # Non-functional changes (deps, tooling, docs)
refactor/short-description     # Code restructuring without behaviour change
research/short-description     # Circuit study / measurement work, no code
```

### Commit messages

Use the imperative mood. Reference the spec ID when applicable.

```
<type>: <short description> [SPEC-XXX]

<optional body — what and why, not how>
```

**Types:** `feat`, `fix`, `chore`, `refactor`, `docs`, `test`, `style`, `research`,
`assets` (sprite sheets and other binary art).

**Examples:**

```
feat: add dispersive allpass chain to the tank model [SPEC-001]
test: add RT60 measurement harness for the tank [SPEC-006]
research: document AB763 reverb driver component values
assets: add knob_large sprite atlas at 1x and 2x [SPEC-005]
docs: record modelling approach decision [ADR-001]
```

### Branching strategy

Trunk-based development. All branches are cut from `main` and merged back via pull
request. Branches are deleted after merging.

**CRITICAL RULE: NEVER commit directly to `main`.**
All updates, no matter how small or trivial, must be made on a separate branch and
merged via Pull Request. No exceptions.

---

## Code Style

C++20. See `CLAUDE.md` at the project root for the full style guide. Short version:

- **DSP is header-only** under `Source/dsp/`, in `namespace bv`, with **no JUCE
  dependency**. This is what makes the offline test suite possible — tests include the
  DSP headers directly and never link JUCE.
- `Source/ui/` and the two `Plugin*` files may use JUCE freely.
- No allocation, locking, logging, or file I/O on the audio thread.
- `prepare(sampleRate, numChannels)` then `reset()` then `process(...)`. Every DSP class
  follows that shape.
- Parameter IDs are string constants in one place (`Source/ParameterIDs.h`), never
  inline literals.
- Comments explain *why* — especially which circuit property a magic number came from.
  A bare constant in a DSP file is a bug waiting to happen.

---

## Tooling

| Tool | Command | Purpose |
| --- | --- | --- |
| Configure | `cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo` | Fetches JUCE + Catch2 |
| Build plugin | `cmake --build build -j` | VST3 / Standalone (+ AU on macOS) |
| Build tests | `cmake --build build --target birbverb_tests -j` | Offline DSP suite |
| Run tests | `ctest --test-dir build --output-on-failure` | The TDD loop |
| Render audio | `cmake --build build --target bv_render -j` | Offline wav render for listening |
| Validate (AU) | `auval -v aufx Bvrb Ynme` | macOS Audio Unit validation |
