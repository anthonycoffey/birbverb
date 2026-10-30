# Sprite work order

What to draw, in what order, and how to check it. Exact file format, lighting conventions
and rendering recipes are in
[sprite-authoring](docs/documentation/guides/sprite-authoring.md) — this file is the
sequence and the acceptance checks.

Everything here is **independent of the DSP**. Every control falls back to procedural
drawing when its sheet is missing, so you can ship one sheet at a time and the plugin
stays usable the whole way. That is a requirement in
[SPEC-005](docs/specs/active/SPEC-005-sprite-based-pedal-ui.md), not a convenience.

---

## The one rule

> **A knob sheet contains only the part that rotates.**

The well it sits in, its drop shadow, and its label all belong in `background.png`. If the
shadow rotates with the knob, the whole face reads as fake instantly, and no amount of
texture detail recovers it.

| Goes in the knob atlas | Goes in `background.png` |
| --- | --- |
| The cap, its skirt, its pointer or indicator line | The recessed well or bezel |
| Highlights and shading **on the cap itself** | The drop shadow the cap casts |
| | The label text and the tick marks |
| | Everything else: enclosure, screws, branding, panel wear |

## Where the files go

`Assets/sprites/`, named exactly as [`sprites.json`](Assets/sprites/sprites.json) says.
Optional retina versions go beside them as `<name>@2x.png` at exactly double every
dimension.

**CMake globs this directory at configure time**, so after you add the first PNG you have
to re-run configure or the build will not see it:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build -j
```

That is the single most likely reason a sheet you just drew does not appear.

## Frame layout

Multi-frame sheets are a **row-major grid**, left to right then top to bottom, with **no
gutters and no padding** — frame *n* is at `(n % cols, n / cols)` times the frame size,
and the sheet's pixel dimensions must equal `cols × frameWidth` by `rows × frameHeight`
exactly. One pixel off and `SpriteAtlas::isValid()` rejects the whole atlas and that
control silently falls back.

Grids rather than the conventional vertical filmstrip because 100 frames of a 104 px knob
as a strip is 104 × 10400 px, past common maximum texture dimensions. Reasoning in
[ADR-002](docs/specs/adrs/ADR-002-sprite-based-ui-rendering.md).

For the 100-frame knobs: **frame 0 is the minimum value, frame 99 the maximum**, sweeping
**270°** — so −135° at frame 0, 0° at frame 49/50, +135° at frame 99. Keep the rotation
axis at the exact centre of the frame cell, or the knob wobbles as it turns.

---

## Order of work

Each step is shippable on its own. Do them in this order — it is roughly descending
visual payoff, and the background first gives you the palette everything else matches.

### 1. `background.png` — 600 × 650, one frame

The whole panel: enclosure, wells, labels, tick marks, branding, screws, and every drop
shadow. One frame, no grid. Biggest single change to how the plugin looks, and the
simplest file to make.

Controls sit at these positions. `x`/`y` are the **top-left of the frame rectangle** in
the 600 × 650 editor space, so a well for `dwell` is centred at (84 + 52, 200 + 52).

| Control | Atlas | x | y | Frame size |
| --- | --- | --- | --- | --- |
| `dwell` | `knob_large` | 84 | 200 | 104 × 104 |
| `decay` | `knob_large` | 248 | 200 | 104 × 104 |
| `tone` | `knob_large` | 412 | 200 | 104 × 104 |
| `locut` | `knob_small` | 60 | 356 | 72 × 72 |
| `boing` | `knob_small_alt` | 196 | 356 | 72 × 72 |
| `mix` | `knob_small` | 332 | 356 | 72 × 72 |
| `level` | `knob_small_alt` | 468 | 356 | 72 × 72 |
| `circuit` | `switch_4way` | 140 | 470 | 64 × 112 |
| `tank` | `switch_3way` | 396 | 470 | 64 × 112 |
| `bypass` | `footswitch` | 236 | 500 | 128 × 128 |

Leave room at the top for the preset bar, which is drawn by JUCE over the background.

Two labelling notes that are not cosmetic:

- **`boing` must be labelled as an effect, not a circuit control.** It exaggerates
  dispersion past any real spring. SPEC-001 requires it to be visibly distinguished from
  the modelled controls, because the project's whole claim is that behaviour traces to
  hardware and a knob that does not must say so. That is why it has its own atlas,
  `knob_small_alt` — give it a different cap colour from `locut` and `mix`.
- **`level` also uses `knob_small_alt`**, and it is also not a modelled circuit property.
  Same reasoning.

### 2. `knob_large.png` — 1040 × 1040, 10 × 10 grid of 104 px frames

The three main knobs: `dwell`, `decay`, `tone`. Second biggest payoff after the
background, and the one place a hand-drawn pointer beats a procedural one most visibly.

### 3. `knob_small.png` — 720 × 720, 10 × 10 grid of 72 px frames

`locut` and `mix`. Same knob design as `knob_large`, scaled — render it from the same
source at the smaller size rather than downscaling the big sheet, or the highlight
detail turns to mush.

### 4. `knob_small_alt.png` — 720 × 720, 10 × 10 grid of 72 px frames

`boing` and `level`. Same geometry as `knob_small`, visibly different cap — a different
colour is enough. See the labelling note in step 1 for why these two are set apart.

### 5. `switch_4way.png` — 256 × 112, 4 × 1 of 64 × 112

The `circuit` selector. One frame per position, in this order:

| Frame | Circuit |
| --- | --- |
| 0 | Fender AB763 (blackface) |
| 1 | Fender 6G16 (brown Vibroverb) |
| 2 | Fender 6G15 (outboard Reverb Unit) |
| 3 | Ampeg Reverberocket R-12-R |

### 6. `switch_3way.png` — 192 × 112, 3 × 1 of 64 × 112

The `tank` selector:

| Frame | Tank |
| --- | --- |
| 0 | Short 2-Spring |
| 1 | Long 2-Spring |
| 2 | Long 3-Spring |

### 7. `footswitch.png` — 256 × 128, 2 × 1 of 128 px frames

Frame 0 is up/bypassed, frame 1 is down/engaged. Only the moving parts of the switch;
the plate it is mounted on belongs in the background.

### 8. `led.png` — 256 × 32, 8 × 1 of 32 px frames

Frame 0 is dark, frame 7 is fully lit, with six steps between. It is driven by the wet
level the processor publishes, so the gradient wants to be perceptually even rather than
linear in brightness.

### 9. `arrow.png` — 72 × 48, 2 × 1 of 36 × 48

Preset step arrows. Frame 0 normal, frame 1 pressed. Draw the left-pointing one; the
editor mirrors it for the right.

### 10. `screen.png` — 260 × 48, one frame

The preset-name display background. The text is drawn by JUCE on top, so this is the
bezel, glass, and any screen tint — keep the centre quiet enough to read white text over.

---

## Before you commit a sheet

1. **Dimensions are exact.** `cols × frameWidth` by `rows × frameHeight`, to the pixel.

   ```bash
   magick identify Assets/sprites/*.png
   ```

2. **Alpha is straight, not premultiplied.** Premultiplied alpha gives dark halos around
   every knob edge, and it is the kind of thing you do not notice until the background is
   dark. The authoring guide has the export settings per tool.

3. **It is PNG-32, sRGB, non-interlaced.** Then optimise losslessly:

   ```bash
   oxipng -o4 --strip safe Assets/sprites/*.png
   ```

4. **Re-run configure, then look at it.** The glob is configure-time:

   ```bash
   cmake -B build && cmake --build build -j
   ```

5. **Sweep every knob through its whole range in the plugin.** The failure mode you are
   looking for is a pointer that drifts off-centre as it rotates, which means the rotation
   axis was not at the frame-cell centre. It is invisible in a static frame and obvious in
   motion.

6. **Check the fallback still works** by renaming your sheet away and reloading. A broken
   atlas should fall back to procedural drawing for that one control and leave everything
   else alone — never crash, never draw a garbage rectangle.

## Committing

```
assets: add background and large knob sheets [SPEC-005]
```

Binary diffs are unreviewable, so **a PR that changes sprite art needs a screenshot.**
That is a repo convention, in [CLAUDE.md](CLAUDE.md).

Commit your generation inputs too — the Blender file, the layered source, whatever
produced the sheets. [ADR-003](docs/specs/adrs/ADR-003-binary-asset-pipeline.md) records
why: PNGs go into plain Git with no LFS, because a silent LFS pointer-file failure is
worse than a large clone, and the limit to revisit at is 250 MB.

## If you would rather not draw

The procedural fallback is deliberately decent, not a placeholder of last resort — the
plugin is shippable without a single PNG. `Assets/sprites/source/PROMPTS.md` has prompts
for generating the sheets instead, and the dimension tables above are what any generated
result has to be resized to exactly.
