# Sprite generation inputs

Committed per [ADR-003](../../../docs/specs/adrs/ADR-003-binary-asset-pipeline.md): without
these, a sheet that needs one knob recoloured has to be recreated from scratch and will not
match the others.

Record, per sheet:

- **Tool** — Blender (with the .blend committed here), an image model (with the exact prompt
  and model name, plus the base images, since model output is not reproducible from the
  prompt alone), or a photograph (with the setup).
- **Inputs** — the scene file, layered source, or base images.
- **Post-processing** — the exact `magick` / `oxipng` commands used.

## background.png

- Tool:
- Inputs:
- Post:

## knob_large.png

- Tool:
- Inputs:
- Post:
