# Assets/

Binary art for the plugin UI. Committed directly to Git and compiled into the binary —
see [ADR-003](../docs/specs/adrs/ADR-003-binary-asset-pipeline.md) for why, including the
rules that keep history from bloating.

```
Assets/
  sprites/
    sprites.json          the manifest: frame geometry + control layout
    background.png        600 x 650
    background@2x.png     1200 x 1300
    knob_large.png        10 x 10 grid of 104 x 104 frames
    ...
    source/               generation inputs — Blender scenes, layered files, PROMPTS.md
```

## Nothing here yet, and the plugin still works

None of the PNG files the manifest references exist. Every control falls back to
procedural drawing, so a fresh clone builds and runs a fully usable plugin
([SPEC-005](../docs/specs/active/SPEC-005-sprite-based-pedal-ui.md) requirement 6).

That is deliberate. It means DSP work never waits on art, and sheets can land one at a
time instead of as one big drop.

## Before you add a sheet

Read [sprite-authoring](../docs/documentation/guides/sprite-authoring.md). It has the
file format rules, the exact dimensions for every asset, the lighting conventions, and
the one rule that matters most:

> **The knob sprite contains only the part that rotates.** The well, the drop shadow and
> the label go in `background.png`. A rotating shadow reads as fake instantly, and it is
> the most common mistake in sprite-based plugin UIs.

Then:

```bash
oxipng -o4 --strip safe Assets/sprites/*.png
cmake --build build --target birbverb_tests -j
ctest --test-dir build --output-on-failure      # the manifest test checks geometry
```

The manifest test reads this folder's real `sprites.json` and checks it against the
sheets, so most geometry mistakes fail the build rather than shipping.

## `source/`

Generation inputs go here, committed alongside the output. A Blender scene, a layered
source file, or `PROMPTS.md` with the exact prompt and model used plus the base images.

Without these, a sheet that needs one knob recoloured has to be recreated from scratch
and will not match the others. For an open-source project that is the difference between
art someone else can maintain and art that is frozen the day its author stops working on
it.

## Suggested order

1. `background.png` — establishes the palette and the lighting direction everything else
   has to match
2. `knob_large.png` — the part the eye tracks
3. `knob_small.png`, `knob_small_alt.png`
4. Switches, footswitch, LED, screen, arrows
