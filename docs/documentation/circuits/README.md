# Circuit Studies

Research on the hardware birbverb models. These documents describe **amplifiers and
spring tanks**, never our DSP. Design decisions derived from them live in
[`docs/specs/`](../../specs/).

Start a new one with `/new-circuit`, from
[`docs/templates/circuit-study-template.md`](../../templates/circuit-study-template.md).

## Index

| Study | Hardware | Drives which preset | Confidence |
| --- | --- | --- | --- |
| [accutronics-spring-tanks](accutronics-spring-tanks.md) | Accutronics / Hammond tanks (the shared component) | all | medium |
| [fender-ab763-reverb](fender-ab763-reverb.md) | Fender AB763 blackface — Twin Reverb, Super Reverb, Vibroverb (1964) | `Twin '64`, `Super '63` | medium |
| [fender-6g16-vibroverb](fender-6g16-vibroverb.md) | Fender Vibroverb 6G16 (1963, brown) | `Vibroverb '63` | low |
| [fender-6g15-reverb-unit](fender-6g15-reverb-unit.md) | Fender Reverb Unit 6G15 (outboard) | `Drip Tank` | medium |
| [ampeg-reverberocket](ampeg-reverberocket.md) | Ampeg Reverberocket R-12-R / Echo Jet | `Rocket '62` | low |

## Source discipline

Every claim is tagged in the Sources section of each study:

- **primary** — factory schematic, datasheet, service manual, or a measurement we took
- **secondary** — reputable technical write-up (ampbooks, Rob Robinette, Merlin
  Blencowe, Gerald Weber, SurfGuitar101's circuit FAQ)
- **anecdotal** — forum posts, listings, lore

Anecdotal material never appears in the body of a study as established fact. It goes in
**Open Questions** with the conflict named. The practical reason: several of these
circuits have multiple revisions under one model name, and forum reports routinely
describe a different revision than the one being asked about.

## Where the confidence is low, and why it matters

Two of the five studies are `low` confidence:

- **6G16 Vibroverb** — the reverb mixer and driver differ from AB763 in ways the
  secondary sources disagree about. A 6G16 factory schematic would settle it.
- **Ampeg Reverberocket** — the defining fact (no reverb transformer, one tube driving
  the tank) is well attested, but the exact driver topology across the R-12-R / Rocket 2
  revisions is not.

Low confidence is not a blocker for shipping a preset. It *is* a blocker for claiming
the preset is an emulation of a specific revision. The preset descriptions in
[SPEC-004](../../specs/active/SPEC-004-preset-bank-and-foundation-presets.md) are worded
to match the confidence of the study behind them.

## What we are not researching

Power supply sag, output stage distortion, speaker and cabinet response. birbverb models
the **reverb circuit**, from the send through the tank to the recovery and mix network.
Everything downstream of the mixer belongs to whatever amp sim or amp the player is
already using. See the Non-goals in
[SPEC-001](../../specs/active/SPEC-001-spring-tank-dsp-core.md).
