# Authoring Sprite Sheets

Everything you need to produce birbverb's UI art, and the rules the art has to follow so
the code can composite it without any C++ changes.

The contract is in [SPEC-005](../../specs/active/SPEC-005-sprite-based-pedal-ui.md); the
reasoning is in [ADR-002](../../specs/adrs/ADR-002-sprite-based-ui-rendering.md). This
guide is the practical side: file formats, dimensions, lighting, and what to do about
shadows.

---

## The one rule that matters most

**The knob sprite contains only the part that rotates.**

Everything that stays still when you turn a knob — the recessed well it sits in, its
drop shadow, the painted label, the ring of tick marks — is painted into
`background.png`. The knob atlas contains the knob body and its pointer, on transparency,
and nothing else.

Get this wrong and the shadow rotates with the knob, which looks instantly and
unmistakably fake. It is the single most common mistake in sprite-based plugin UIs.

The same logic applies everywhere:

| Element | Goes in the atlas | Goes in `background.png` |
| --- | --- | --- |
| Knob | Body, cap, knurling, pointer, the highlight that moves with the body | Well, drop shadow, label text, tick marks |
| Switch | The bat or lever in each position | Mounting plate, bezel, position labels |
| Footswitch | The button cap, up and down | Threaded collar, shadow ring, "ENGAGE" text |
| LED | The lens and its glow, at each brightness | Bezel ring |
| Preset screen | — (static) | — (the screen bezel is its own 1-frame atlas so the text can be drawn over it) |

---

## File format

| Property | Value |
| --- | --- |
| Format | PNG-32 (8-bit RGBA) |
| Alpha | **Straight / non-premultiplied.** JUCE expects straight alpha. |
| Colour space | sRGB, no embedded ICC profile other than sRGB |
| Interlacing | Off (no Adam7) |
| Bit depth | 8 bits per channel. 16-bit is wasted here |
| Metadata | Strip it |

Optimise before committing:

```bash
oxipng -o4 --strip safe Assets/sprites/*.png
```

Why straight alpha matters: with premultiplied alpha, every semi-transparent edge pixel
has had its colour multiplied by its own alpha. Composite that as straight alpha and
antialiased edges come out with a dark halo. If knob edges look dirty against the
background, this is why.

---

## Atlas geometry

Every multi-frame asset is a **row-major grid**: frame 0 at top-left, filling left to
right, then wrapping to the next row.

```
┌────┬────┬────┬────┐
│  0 │  1 │  2 │  3 │
├────┼────┼────┼────┤
│  4 │  5 │  6 │  7 │
├────┼────┼────┼────┤
│  8 │  9 │ 10 │ 11 │
└────┴────┴────┴────┘
```

Rules:

- **No padding, no gutters, no margins.** Frame *n* occupies exactly
  `(n % cols) * frameWidth, (n / cols) * frameHeight` to `+frameWidth, +frameHeight`.
- Sheet dimensions must be exactly `cols * frameWidth` by `rows * frameHeight`. A single
  stray pixel of canvas and every frame after the first is misaligned.
- Keep at least **2 px of transparent margin** inside each frame cell around the artwork,
  so antialiased edges are not clipped and a frame cannot bleed into its neighbour.
- The artwork must be **centred on the frame cell's centre**, and for rotary controls the
  rotation axis must be that exact centre. An off-centre axis makes the knob wobble as it
  turns.

---

## The full asset list

All dimensions are **1x**. The `@2x` variant is exactly double in both dimensions — not
approximately, exactly.

| File | Frames | Grid | Frame | Sheet (1x) | Sheet (@2x) |
| --- | --- | --- | --- | --- | --- |
| `background.png` | 1 | 1 x 1 | 600 x 650 | 600 x 650 | 1200 x 1300 |
| `knob_large.png` | 100 | 10 x 10 | 104 x 104 | 1040 x 1040 | 2080 x 2080 |
| `knob_small.png` | 100 | 10 x 10 | 72 x 72 | 720 x 720 | 1440 x 1440 |
| `knob_small_alt.png` | 100 | 10 x 10 | 72 x 72 | 720 x 720 | 1440 x 1440 |
| `switch_4way.png` | 4 | 4 x 1 | 64 x 112 | 256 x 112 | 512 x 224 |
| `switch_3way.png` | 3 | 3 x 1 | 64 x 112 | 192 x 112 | 384 x 224 |
| `footswitch.png` | 2 | 2 x 1 | 128 x 128 | 256 x 128 | 512 x 256 |
| `led.png` | 8 | 8 x 1 | 32 x 32 | 256 x 32 | 512 x 64 |
| `arrow.png` | 2 | 2 x 1 | 36 x 48 | 72 x 48 | 144 x 96 |
| `screen.png` | 1 | 1 x 1 | 260 x 48 | 260 x 48 | 520 x 96 |

Naming: `name.png` and `name@2x.png`, in `Assets/sprites/`.

### Frame content per asset

**`knob_large.png` / `knob_small.png` / `knob_small_alt.png`** — 100 frames of rotation
across **270°**. Frame 0 is the pointer at −135° from straight up (that is, pointing down
and to the left); frame 99 is +135° (down and to the right). 2.73° per frame. Rotate in
even steps; do not ease.

`knob_small_alt.png` is the same knob in a different cap colour, used for the two
controls that are effect controls rather than modelled circuit behaviour (`boing` and
`level`). Same geometry, same lighting, just a different cap.

**`switch_4way.png`** — a 4-position rotary or slide switch. Frame 0 is position 1
(`AB763`), frame 3 is position 4 (`Ampeg`).

**`switch_3way.png`** — frame 0 is the top position, frame 2 the bottom.

**`footswitch.png`** — frame 0 is up (not engaged), frame 1 is down (engaged). The
difference should be a couple of pixels of travel plus a lighting change, not a big
movement.

**`led.png`** — 8 frames, dark to full. Frame 0 is an unlit lens (still visible, still
has a specular highlight — an unlit LED is not invisible). Frame 7 is fully lit with
bloom. Ramp perceptually, not linearly: the steps between frames 5, 6 and 7 should be
smaller than between 0, 1 and 2.

**`arrow.png`** — frame 0 idle, frame 1 pressed.

---

## Lighting and material

Consistency across sheets matters more than any individual sheet looking good. Pin these
down before rendering anything:

| Property | Value |
| --- | --- |
| Light direction | Upper-left, roughly 135° azimuth, 45° elevation |
| Light type | One soft key light plus a dim fill from the lower right |
| Camera | **Orthographic, straight on.** Perspective makes off-centre knobs look tilted |
| Scene scale | Render every asset in one scene at one scale, so highlights match |

Material notes:

- **Enclosure** — satin powder coat. Slight orange-peel texture, broad soft specular, no
  mirror reflection. Edges very slightly rounded and marginally lighter where the coat
  thins.
- **Knobs** — the usual options are a Fender-style skirted chrome-top, a black knurled
  plastic cap, or a cream/ivory plastic knob with a painted pointer line. Pick one and
  keep it for both sizes.
- **Screen print** — matte ink, sitting a touch proud of the paint. A very subtle bevel on
  the letterforms sells it more than any amount of glow.
- **Hardware** — the footswitch collar and switch bezels are plated metal: bright
  specular, dark body, environment reflection kept dim so it does not fight the paint.

---

## Rendering workflow

Three ways to produce this, in order of how maintainable the result is.

### 1. Blender (recommended)

Model the enclosure and one knob once, then script the rotation and render the grid. This
is the maintainable option: changing a knob colour is a material tweak and a re-render,
and the output is deterministic.

Sketch of the knob sheet script:

```python
# Assets/sprites/source/render_knob_atlas.py
import bpy, math

FRAMES, COLS, SIZE = 100, 10, 104
knob = bpy.data.objects["KnobBody"]
bpy.context.scene.render.resolution_x = SIZE
bpy.context.scene.render.resolution_y = SIZE
bpy.context.scene.render.film_transparent = True

for i in range(FRAMES):
    angle = -135.0 + (270.0 * i / (FRAMES - 1))
    knob.rotation_euler[2] = math.radians(angle)
    bpy.context.scene.render.filepath = f"//frames/knob_{i:03d}.png"
    bpy.ops.render.render(write_still=True)
```

Then tile the frames into a grid:

```bash
magick montage frames/knob_*.png -tile 10x10 -geometry +0+0 -background none knob_large.png
```

`-geometry +0+0` is the important part — it is what gives you zero gutters. Check the
output dimensions are exactly 1040 x 1040 before committing.

### 2. Image generation, then compositing

Generate the enclosure face and a single knob at high resolution, then rotate the knob
programmatically for the frames. A generated knob rotated in software looks right as long
as the knob art is lit flat enough that the highlight does not obviously need to move.
This is the fast path, and the one most likely to produce good-looking art quickly.

The catch: **commit the prompt and the model name** to `Assets/sprites/source/PROMPTS.md`
along with the base images, or the next person cannot regenerate anything. Image-model
output is not reproducible from the prompt alone, so the base images are the real source
and have to be kept.

Rotate and tile:

```bash
for i in $(seq 0 99); do
  angle=$(python -c "print(-135 + 270*$i/99)")
  magick knob_base.png -background none -distort SRT "$angle" -resize 104x104 "frames/knob_$(printf %03d $i).png"
done
magick montage frames/knob_*.png -tile 10x10 -geometry +0+0 -background none knob_large.png
```

Note `-distort SRT` rather than `-rotate`: it rotates about the image centre without
changing the canvas size, which is exactly what the frame grid needs.

### 3. Photograph a real pedal

Best-looking results, worst maintainability, and a rights question depending on whose
pedal it is. If you go this way, photograph on a copy stand with fixed lighting, rotate
the knob by hand through marked positions, and accept that you will not get 100 frames —
author the manifest for however many you do get (24 is plenty if the knob has no
asymmetric highlight).

---

## Verification before committing

The manifest test (`tests/test_sprite_manifest.cpp`) reads the real
`Assets/sprites/sprites.json` and checks it against the sheets, so most geometry mistakes
fail the build rather than shipping. Run it:

```bash
cmake --build build --target birbverb_tests -j && ctest --test-dir build --output-on-failure
```

Then check by eye, because the test cannot see these:

- [ ] Sheet dimensions are **exactly** `cols * frameWidth` by `rows * frameHeight`
- [ ] Frame 0 and frame 99 are the rotation extremes, not duplicates of each other
- [ ] Dragging a knob through its full range shows smooth rotation with no jump or stutter
- [ ] The knob's drop shadow does **not** rotate (if it does, it is in the wrong file)
- [ ] No dark halo at knob edges (if there is, the alpha is premultiplied)
- [ ] `@2x` is exactly double the 1x dimensions
- [ ] Lighting direction matches across all sheets
- [ ] At 1x on a non-HiDPI display, the background is pixel-sharp with no resampling
- [ ] Transparent margin inside each frame cell, nothing touching a cell edge

---

## Updating the manifest

Adding an asset or moving a control is a manifest edit, not a code change:

```json
{
  "atlases": {
    "knob_large": {
      "file": "knob_large.png",
      "cols": 10, "rows": 10,
      "frameWidth": 104, "frameHeight": 104,
      "frames": 100,
      "rotarySweepDegrees": 270
    }
  },
  "controls": {
    "dwell": { "atlas": "knob_large", "x": 84, "y": 200 },
    "decay": { "atlas": "knob_large", "x": 248, "y": 200 }
  }
}
```

Control positions are the **top-left of the frame rectangle** in logical pixels, in the
600 x 650 editor space. The layout diagram in
[SPEC-005](../../specs/active/SPEC-005-sprite-based-pedal-ui.md) has all ten.

---

## Working without art

The plugin runs with no sprite assets at all. Every control falls back to procedural
drawing, so you can build and use birbverb before a single sheet exists, and land sheets
one at a time as they are finished. Nothing in the DSP work is blocked on art.

Practically, that means the sensible order is `background.png` first — it establishes the
palette and the lighting direction everything else has to match — then `knob_large.png`,
then the rest.
