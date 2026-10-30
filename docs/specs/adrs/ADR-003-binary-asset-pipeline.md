---
id: ADR-003
title: 'Binary asset pipeline'
status: proposed
date: 2026-10-29
deciders: ['Anthony Coffey']
supersedes: ''
superseded_by: ''
---

# ADR-003: Binary asset pipeline

## Status

`proposed`

## Context

[ADR-002](ADR-002-sprite-based-ui-rendering.md) commits birbverb to sprite art, which
means roughly 8–14 MB of PNG in an otherwise text-only repository. Binary files in Git
are permanent: every version of every sheet stays in history, they do not delta-compress,
and they cannot be reviewed in a diff.

For an open-source plugin there is a second problem. Contributors need to be able to
clone, build, and see the UI. If art lives outside the repo, a fresh clone builds a
plugin that looks broken, and the first thing every contributor files is a bug about it.

Forces:

- Art will be iterated on. Early sheets will be replaced several times.
- A plugin should have no external runtime file dependencies.
- Contributors must get a working UI from a plain `git clone`.
- Git LFS adds a setup step that silently breaks clones for anyone who skips it.
- Generated art is unmaintainable without its generation inputs.

## Decision

**Commit the sprite sheets directly to Git under `Assets/sprites/`, compile them into the
binary with `juce_add_binary_data`, and commit the generation inputs alongside them. No
Git LFS.**

Specifics:

1. **Plain Git, no LFS.** At this scale the history cost is acceptable, and LFS's failure
   mode — a clone that silently produces pointer files instead of images — is worse than a
   large repository for an open-source project.
2. **`juce_add_binary_data`** compiles the sheets into a `BirbAssets` target that the
   plugin links. No runtime file loading, no install-path questions.
3. **Generation inputs are committed** next to the output, under
   `Assets/sprites/source/`: the Blender scene, the layered source file, or the exact
   prompt and model used. Without these, a sheet that needs one knob recoloured has to be
   recreated from scratch and will not match.
4. **The manifest is the interface.** `Assets/sprites/sprites.json` is plain text, diffs
   normally, and is covered by a test that reads the real file. Geometry changes are
   therefore reviewable even when the art is not.
5. **Rules to keep history from bloating:**
   - Replace sheets in place with the same filename; do not accumulate `knob_v2.png`
   - PNG-32, straight alpha, no interlacing, `oxipng -o4` or equivalent before commit
   - Omit a `@2x` variant where 1x upscaling is imperceptible
   - Do not commit work-in-progress renders; land a sheet when it is the one being used
   - One commit per sheet, prefixed `assets:`, so a bad sheet is one revert

## Consequences

### Positive

- `git clone && cmake --build` produces a plugin that looks right. No setup step, no LFS
  install, no asset download.
- No runtime file dependencies, so no install-path or missing-asset failures in the field.
- Generation inputs in the repo mean art stays maintainable by someone who is not the
  person who made it — which for an open-source project is the point.
- The manifest keeps the reviewable part of the UI reviewable.

### Negative / Trade-offs

- **Repository size grows permanently.** Every superseded sheet stays in history. A few
  iterations of the full set could add 50–100 MB over the project's life. This is the real
  cost, and the mitigation rules above are what keep it bounded rather than eliminating
  it.
- Binary diffs are opaque. A sheet change has to be reviewed by looking at the rendered
  UI, which means a screenshot in the PR is effectively mandatory.
- Clone time for contributors on slow connections is worse than a text-only repo.
- Build times increase slightly, and a changed sheet triggers a relink.

### Neutral

- Plugin binary size grows by the asset size. Normal for the category.
- `juce_add_binary_data` generates a large C++ source file, which some tools find
  noteworthy. Harmless.

## Alternatives Considered

### Option A: Git LFS

- **Description:** Store sheets as LFS pointers.
- **Pros:** Keeps the main repository small. The standard answer for binary assets.
- **Cons:** Requires `git lfs install` before cloning. Skip it and you get text pointer
  files where images should be, the build succeeds, and the UI is subtly broken with no
  useful error. GitHub LFS bandwidth is quota'd on free accounts, so a popular repo can
  stop serving its own assets.
- **Why rejected:** The silent-failure mode is unacceptable for a project that wants
  drive-by contributors. Reconsider if the repository passes ~250 MB.

### Option B: Assets in a separate repository or release artifact

- **Description:** Art lives elsewhere; the build fetches it.
- **Pros:** Main repository stays clean and text-only.
- **Cons:** A plain clone does not build a correct-looking plugin. Needs network at build
  time. Versions can drift between the two repos.
- **Why rejected:** Breaks the contributor experience, which is the thing this ADR is
  mostly protecting.

### Option C: Load assets from disk at runtime

- **Description:** Ship PNGs beside the plugin binary and load them on editor open.
- **Pros:** Art is swappable without a rebuild, which would make user skins trivial.
- **Cons:** Plugins live in system directories where write access and relative paths are
  both awkward. Introduces a whole class of "works on my machine" failures.
- **Why rejected:** Not worth the deployment risk for v0.1. If user skinning becomes a
  goal, revisit — the manifest design already allows for it.

### Option D: Generate sprite sheets at build time

- **Description:** Commit only the generation inputs; render sheets during the build.
- **Pros:** Repository stays text-only. Art is fully reproducible.
- **Cons:** Every contributor needs the rendering toolchain — Blender, or an image model
  with an API key. Build times go from seconds to minutes. Image-model output is not
  deterministic, so two builds could differ.
- **Why rejected:** The toolchain requirement is a much higher barrier than a large clone.
  Attractive in principle and worth revisiting if the art ends up fully
  Blender-scripted, where it would be deterministic.

## Notes

- Rendering approach: [ADR-002](ADR-002-sprite-based-ui-rendering.md)
- Implementation spec: [SPEC-005](../active/SPEC-005-sprite-based-pedal-ui.md)
- Authoring instructions, including the optimisation step:
  [sprite-authoring](../../documentation/guides/sprite-authoring.md)
- **Revisit trigger:** repository exceeding 250 MB, or LFS bandwidth becoming cheaper than
  clone time. Record the change as a new ADR superseding this one.
