---
id: SPEC-005
title: 'Sprite-based pedal UI'
status: draft
created: 2026-10-29
author: Anthony Coffey
reviewers: []
affected_repos: ['birbverb']
---

## Reviewer Notes

<!-- Leave empty until code review. When requesting changes, reviewer adds feedback here: -->

---

# Feature: Sprite-based pedal UI

## Problem

birbverb should look like a guitar pedal that exists — a painted enclosure with real
knobs on it, photographed or rendered, not a vector drawing of a pedal.

zoo-tron draws its whole face procedurally: gradients, strokes, and `drawRotarySlider`.
That approach has real virtues (no binary assets, resolution-independent, every colour is
a constant you can tweak) and it has one hard ceiling: **procedural drawing cannot
produce the things that make hardware look like hardware.** Brushed aluminium grain,
powder-coat texture, screen-print ink sitting slightly proud of the paint, worn edges,
the specific way a knurled knob catches light, a scuff. Those are photographic
properties. You can approximate them with enough gradient layers, and it will still read
as "CSS pedal".

So: render the art externally, ship it as sprite sheets, and have the UI composite
frames.

## Requirements

### Must have

1. The editor SHALL be fixed at 600 x 650 logical pixels, and the background sprite SHALL
   be authored at exactly that size.
2. All controls SHALL be drawn by compositing frames from sprite atlases, not by
   procedural shape drawing.
3. Each sprite atlas SHALL be a **row-major grid** of equally sized frames, with its
   geometry declared in `Assets/sprites/sprites.json` rather than hard-coded.
4. The system SHALL load a `@2x` variant when the display scale factor is greater than
   1.0, and SHALL fall back to the 1x asset when no `@2x` exists.
5. WHEN a rotary control's value changes, the system SHALL select the frame index
   `round(normalisedValue * (frameCount - 1))`.
6. WHEN a sprite asset is missing or fails to decode, the control SHALL fall back to
   procedural drawing and the plugin SHALL NOT crash or render blank.
7. Sprite assets SHALL be compiled into the binary via JUCE's binary data target, so the
   plugin has no external file dependencies at runtime.
8. The UI SHALL show the current preset name with a modified marker, and SHALL provide
   ◀ ▶ stepping plus a click-to-browse list, per
   [SPEC-004](SPEC-004-preset-bank-and-foundation-presets.md).
9. All ten parameters from [SPEC-003](SPEC-003-parameter-model.md) SHALL be reachable from
   the face.
10. Controls that are **effect controls** rather than modelled circuit behaviour (`boing`,
    `level`) SHALL be visually distinguished from the modelled ones.
11. The editor SHALL NOT allocate or decode images on the audio thread, and SHALL NOT
    block the audio thread for any reason.

### Nice to have

- A wet-level LED whose brightness tracks the recovered wet signal, using the 8-frame LED
  atlas.
- A tank graphic that animates subtly with wet energy.
- Knob rotation interpolated between frames for very slow automation.

### Non-goals

- No resizable or scalable UI in v0.1. A fixed-size sprite UI is the trade being made
  (see [ADR-002](../adrs/ADR-002-sprite-based-ui-rendering.md)).
- No theming or alternate skins in v0.1, though the manifest design does not preclude it.
- No animated footswitch stomp sequence.
- This spec does NOT define the artwork's visual design. It defines the **contract** the
  artwork must satisfy. The art direction lives in
  [sprite-authoring](../../documentation/guides/sprite-authoring.md).

## Design

### Layout at 600 x 650

```
┌──────────────────────────────────────────────────────┐ 0
│                   badge  (180,24) 240x64             │
├──────────────────────────────────────────────────────┤ 100
│  ◀    [ preset screen  170,112  260x48 ]    ▶        │
├──────────────────────────────────────────────────────┤ 176
│                                                      │
│    DWELL          DECAY           TONE               │  large knobs, 104px
│   (84,200)      (248,200)      (412,200)             │
│                                                      │
├──────────────────────────────────────────────────────┤ 332
│   LO CUT      BOING*       MIX        LEVEL*         │  small knobs, 72px
│  (60,356)   (196,356)   (332,356)   (468,356)        │
│                                                      │
├──────────────────────────────────────────────────────┤ 456
│      CIRCUIT                      TANK               │
│     (140,480)                   (396,480)            │  switches, 64x112
├──────────────────────────────────────────────────────┤ 600
│   (led 284,610)        [ footswitch 236,510 ]        │
└──────────────────────────────────────────────────────┘ 650
```

Coordinates are the **top-left of each control's frame rectangle**, in logical pixels.
Controls marked `*` are effect controls and get the visual distinction from requirement
10 — the current proposal is a different knob cap colour, provided as a second knob atlas
rather than a tint, so the distinction is in the art where it belongs.

### Sprite asset contract

All sizes are **1x**; the `@2x` variant is exactly double in both dimensions.

| Asset | Frames | Grid (cols x rows) | Frame size | Sheet size | Purpose |
| --- | --- | --- | --- | --- | --- |
| `background.png` | 1 | 1 x 1 | 600 x 650 | 600 x 650 | Enclosure, paint, screen print, knob wells, static shadows |
| `knob_large.png` | 100 | 10 x 10 | 104 x 104 | 1040 x 1040 | Dwell, Decay, Tone |
| `knob_small.png` | 100 | 10 x 10 | 72 x 72 | 720 x 720 | Lo Cut, Mix |
| `knob_small_alt.png` | 100 | 10 x 10 | 72 x 72 | 720 x 720 | Boing, Level — the effect-control cap colour |
| `switch_4way.png` | 4 | 4 x 1 | 64 x 112 | 256 x 112 | Circuit selector |
| `switch_3way.png` | 3 | 3 x 1 | 64 x 112 | 192 x 112 | Tank selector |
| `footswitch.png` | 2 | 2 x 1 | 128 x 128 | 256 x 128 | Bypass — frame 0 up, frame 1 down |
| `led.png` | 8 | 8 x 1 | 32 x 32 | 256 x 32 | Wet level, frame 0 dark through frame 7 full |
| `arrow.png` | 2 | 2 x 1 | 36 x 48 | 72 x 48 | Preset ◀ ▶, frame 0 idle, frame 1 pressed |
| `screen.png` | 1 | 1 x 1 | 260 x 48 | 260 x 48 | Preset display bezel and glass |

Total at 2x, as PNG-32, is roughly 8–14 MB depending on texture detail — acceptable
inside a plugin binary, and the reason the manifest allows per-asset omission of the
`@2x` variant for anything where 1x upscaling is imperceptible.

**Rotary frame mapping:** 100 frames spanning 270° of rotation. Frame 0 is the minimum
position (pointer at −135° from vertical), frame 99 is the maximum (+135°). 2.73° per
frame.

### The manifest

`Assets/sprites/sprites.json` declares geometry so neither the art nor the layout is
hard-coded in C++:

```json
{
  "version": 1,
  "scale": 1,
  "editor": { "width": 600, "height": 650 },
  "atlases": {
    "knob_large": { "file": "knob_large.png", "cols": 10, "rows": 10,
                    "frameWidth": 104, "frameHeight": 104, "frames": 100,
                    "rotarySweepDegrees": 270 }
  },
  "controls": {
    "dwell": { "atlas": "knob_large", "x": 84, "y": 200 }
  }
}
```

The manifest is parsed once on editor construction. A malformed manifest triggers the
requirement 6 fallback for every control rather than failing to open.

### Components

| Component | File | Responsibility |
| --- | --- | --- |
| `SpriteAtlas` | `Source/ui/SpriteAtlas.h` | One decoded image plus frame geometry; `drawFrame(g, index, bounds)` |
| `SpriteStore` | `Source/ui/SpriteStore.h` | Loads the manifest and all atlases from binary data; resolves 1x vs @2x; owns the fallback flags |
| `SpriteKnob` | `Source/ui/SpriteKnob.h` | `juce::Slider` subclass that paints a frame; procedural fallback |
| `SpriteSwitch` | `Source/ui/SpriteSwitch.h` | N-position switch bound to a choice parameter |
| `SpriteFootswitch` | `Source/ui/SpriteFootswitch.h` | Bypass control |
| `SpriteLED` | `Source/ui/SpriteLED.h` | Level indicator, driven at ~30 fps from published engine state |
| `PresetBar` | `Source/ui/PresetBar.h` | Screen, arrows, browse list, save dialog |
| `BirbLookAndFeel` | `Source/ui/BirbLookAndFeel.h` | Fonts, popup menu colours, and the procedural fallback drawing |

### The fallback matters more than it looks

Requirement 6 is not defensive programming for its own sake. It is what lets the DSP and
the UI be built in parallel: the plugin is usable with zero art in the repo, so SPEC-001
through SPEC-004 are not blocked waiting on sprite sheets, and sheets can land one at a
time. Every control ships procedural-first and gets its sprite when the art exists.

## Edge cases

- [ ] Display scale 1.25 / 1.5 / 2.0 / 3.0 — pick @2x above 1.0, never upscale past 2x
      without a visible-quality check
- [ ] Display scale changed while the editor is open (monitor switch)
- [ ] Atlas present but with wrong dimensions for its manifest entry — reject that atlas
      and fall back for its controls only, do not fall back globally
- [ ] Manifest declares a control whose atlas is missing
- [ ] Manifest declares frames that do not fit the sheet (`cols * rows < frames`)
- [ ] Editor opened and closed repeatedly — atlases must not be re-decoded each time
- [ ] Parameter at exactly 1.0 — frame index must be `frames - 1`, not out of range
- [ ] Very large host-forced window scaling

## Acceptance criteria

1. `SpriteAtlas::frameIndexFor(0.0)` is 0 and `frameIndexFor(1.0)` is `frames - 1`, with
   no out-of-range index for any value in [0, 1]. Verified by
   `tests/test_sprite_atlas.cpp`.
2. The manifest in `Assets/sprites/sprites.json` parses, and every control it declares
   resolves to an atlas whose grid geometry matches its declared frame count and sheet
   size. Verified by `tests/test_sprite_manifest.cpp`, which reads the real manifest file
   so the test fails when art and manifest drift apart.
3. With every sprite asset removed, the plugin opens its editor, all ten parameters are
   controllable, and no assertion or crash occurs.
4. With a deliberately corrupt atlas, only the controls bound to that atlas fall back.
5. Editor is 600 x 650 and every control's frame rectangle from the layout above lies
   fully inside those bounds with no overlap between controls.
6. `@2x` is selected at scale factor 2.0 and 1x at 1.0.
7. Opening and closing the editor 50 times does not increase decoded-image count.
8. No image decode or allocation occurs on the audio thread. Verified by inspection and
   by an allocation-counting instrument in the processor's `processBlock` under test.
9. Every parameter in SPEC-003 has a control on the face.
10. `boing` and `level` are visually distinguished from the modelled controls.

## Constraints

- JUCE 8 `juce::Image` compositing. No OpenGL requirement.
- Binary assets inflate the repository. [ADR-003](../adrs/ADR-003-binary-asset-pipeline.md)
  records how they are stored and why Git LFS is or is not used.
- Fixed window size. Accepted trade; see ADR-002.
- Sprite sheets are **generated art**, so the generation inputs (prompts, Blender scene,
  or layered source) belong in the repo next to the output, or the art is unmaintainable
  the first time a knob needs to change colour.

## Tasks

- [ ] Write `Assets/sprites/sprites.json` with the geometry and layout above
- [ ] RED: `tests/test_sprite_atlas.cpp` — frame index mapping, bounds
- [ ] RED: `tests/test_sprite_manifest.cpp` — manifest parses and is internally consistent
- [ ] GREEN: `SpriteAtlas`, `SpriteStore` with 1x/@2x resolution and per-atlas fallback
- [ ] GREEN: `BirbLookAndFeel` procedural fallback drawing for every control type
- [ ] GREEN: `SpriteKnob`, `SpriteSwitch`, `SpriteFootswitch`, `SpriteLED`
- [ ] GREEN: `PresetBar` with screen, arrows, browse, save
- [ ] GREEN: `PluginEditor` laying out all ten controls at the coordinates above
- [ ] Author the art per [sprite-authoring](../../documentation/guides/sprite-authoring.md)
- [ ] Wire `juce_add_binary_data` once the first atlas exists
- [ ] Verify at scale 1.0, 1.5 and 2.0 on Windows and macOS
- [ ] Screenshot for the README

## Notes

- Rendering approach decision: [ADR-002](../adrs/ADR-002-sprite-based-ui-rendering.md)
- Asset storage decision: [ADR-003](../adrs/ADR-003-binary-asset-pipeline.md)
- Art direction and authoring instructions:
  [sprite-authoring](../../documentation/guides/sprite-authoring.md)
- The 600 x 650 size comes from zoo-tron, which has the right proportions for this.
  Keeping it identical means the background sprite maps 1:1 to the window with no scaling
  at 1x, which is the only way painted texture stays crisp.
