---
id: ADR-002
title: 'Sprite-based UI rendering instead of procedural drawing'
status: proposed
date: 2026-10-29
deciders: ['Anthony Coffey']
supersedes: ''
superseded_by: ''
---

# ADR-002: Sprite-based UI rendering instead of procedural drawing

## Status

`proposed`

## Context

zoo-tron's UI is drawn entirely in code: `drawRotarySlider` overrides, colour constants,
`ColourGradient` fills, stroked paths. It works, it is maintainable, it is
resolution-independent, and every colour in it is a named constant you can nudge.

birbverb wants to look like a pedal that exists. The gap between "a vector drawing of a
pedal" and "a photograph of a pedal" is made of things procedural drawing is bad at:

- powder-coat and brushed-metal texture
- screen-print ink with a slight physical relief
- the way a knurled aluminium knob catches a light source as it rotates
- wear at the enclosure edges, scuffs, fingerprints
- a real drop shadow under a real knob

You can approach some of these with enough stacked gradients and noise, at which point
the paint code is longer than the DSP and still reads as synthetic. Meanwhile, rendering
a knob once in a 3D tool or an image generator and exporting 100 rotation frames gives
you all of it for free, because the lighting and material simulation already happened.

Forces:

- Art is produced outside the codebase and handed over as image files.
- The DSP work must not be blocked waiting on art.
- Plugins should have no external runtime file dependencies.
- HiDPI displays are the norm; 1x-only art looks soft.
- Binary assets in Git are a real cost and are irreversible once committed.

## Decision

**Composite control graphics from sprite atlases — row-major grids of equally sized
frames — with geometry declared in a JSON manifest, assets compiled into the binary, and
a procedural fallback for every control.**

Specifics:

- Atlases are **grids**, not vertical filmstrips. A 100-frame knob as a filmstrip is
  104 x 10400 px, which exceeds common maximum texture dimensions and is awkward in every
  image editor. As a 10 x 10 grid it is 1040 x 1040.
- Rotary controls use 100 frames across 270°, which is 2.73° per frame — below the
  threshold where stepping is visible during a knob drag.
- `Assets/sprites/sprites.json` declares frame geometry and control positions, so adding
  or moving a control is a manifest edit plus art, not a C++ layout change.
- `@2x` variants are authored at exactly double, and selected when the display scale
  exceeds 1.0.
- **Every control has a procedural fallback.** Missing, corrupt, or geometrically
  inconsistent art degrades to a drawn control rather than a blank panel.
- The editor is fixed size at 600 x 650.

## Consequences

### Positive

- The UI can look genuinely photographic, which is the actual goal.
- Art iterates without touching C++. A new knob colour is a file swap.
- The manifest makes layout data rather than code, which also makes it testable — a test
  reads the real manifest and fails when art and layout drift apart.
- The procedural fallback decouples the two workstreams completely: the plugin is fully
  usable with zero art committed, so DSP development never waits on a sprite sheet, and
  sheets can land one at a time.
- Grid atlases stay within texture size limits and open in any image editor.

### Negative / Trade-offs

- **Fixed window size.** Sprite art cannot scale arbitrarily without softening. This is
  the real cost of the decision, and some users will want a resizable window.
- **Binary assets in the repository**, with the attendant size and diff-opacity problems.
  Addressed separately in [ADR-003](ADR-003-binary-asset-pipeline.md).
- Two code paths per control (sprite and fallback) means more to maintain and more to
  test. Mitigated by the fallback being deliberately plain — it is a development aid, not
  a second skin.
- Changing the layout now requires editing the background art too, since knob wells and
  static shadows are painted into it.
- Art provenance has to be managed. If a sheet is generated, the generation inputs need
  to live in the repo or the art is unmaintainable the first time it needs a change.

### Neutral

- Plugin binary grows by roughly 8–14 MB. Unremarkable for an audio plugin.
- Memory: atlases are decoded once per editor instance. A 2x knob atlas is about 17 MB
  decoded as ARGB, so the store must hold one copy rather than one per control.

## Alternatives Considered

### Option A: Keep procedural drawing (the zoo-tron approach)

- **Description:** Draw everything with JUCE `Graphics` calls.
- **Pros:** No binary assets. Resolution-independent, so a resizable window is free.
  Every colour is a tweakable constant. No art pipeline.
- **Cons:** Cannot produce photographic material and texture, which is the stated goal.
- **Why rejected:** It fails the one requirement that motivated the change. Kept as the
  fallback path, which is a genuinely useful second life for it.

### Option B: SVG assets rendered at runtime

- **Description:** Ship vector art, rasterise per frame.
- **Pros:** Scalable, so a resizable window stays sharp. Text assets diff in Git.
- **Cons:** Vector art has the same ceiling as procedural drawing for texture and
  material. Runtime rasterisation of a complex knob on every repaint is expensive.
- **Why rejected:** Solves the scaling problem, not the realism problem. If the UI were
  flat or illustrative rather than photographic this would be the right answer.

### Option C: Vertical filmstrips (the KnobMan convention)

- **Description:** Frames stacked in one column, the long-standing plugin-UI convention.
- **Pros:** Tooling exists. Frame indexing is trivial.
- **Cons:** 100 frames at 104 px is 10400 px tall, past common maximum texture
  dimensions and unwieldy to edit.
- **Why rejected:** Grid atlases get the same result without the dimension problem. The
  manifest means the convention is a data detail, so this could change later without a
  code change.

### Option D: Single composited background with procedural controls on top

- **Description:** One photographic background sprite, controls still drawn in code.
- **Pros:** Most of the texture benefit for one image. No frame pipeline at all.
- **Cons:** The knobs are the part the eye tracks. Photographic enclosure plus drawn
  knobs looks worse than either consistently.
- **Why rejected:** Half-measure. Worth revisiting only if authoring 100-frame knob
  sheets turns out to be impractical.

## Notes

- Implementation spec: [SPEC-005](../active/SPEC-005-sprite-based-pedal-ui.md)
- Asset storage: [ADR-003](ADR-003-binary-asset-pipeline.md)
- Authoring instructions: [sprite-authoring](../../documentation/guides/sprite-authoring.md)
- **Revisit trigger:** if users ask for a resizable window often enough to matter,
  re-evaluate Option B for the controls while keeping a sprite background. The manifest
  makes that a smaller change than it would otherwise be.
