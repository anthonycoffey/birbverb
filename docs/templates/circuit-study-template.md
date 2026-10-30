---
id: CIRCUIT-000
title: ''
hardware: ''
years: ''
status: draft
updated: YYYY-MM-DD
author: ''
confidence: low
---

# Circuit Study: [Hardware Name]

> Research document. This describes **hardware**, not our code. Every factual claim
> needs a source. Anything unverified belongs in Open Questions, not in the body as if
> it were established.

## Summary

<!-- Two or three sentences: what this unit is, what makes its reverb sound the way it
does, and what we most want to capture from it. -->

## Signal Path

<!-- Block diagram in text form, input to output. -->

```
input → [stage] → [stage] → tank → [stage] → mixer → output
```

## Stages

### [Stage name]

| Property | Value | Source |
| --- | --- | --- |
|  |  |  |

<!-- Repeat per stage: send/driver, tank, recovery, tone, mixer. -->

## Spring Tank

| Property | Value | Source |
| --- | --- | --- |
| Model |  |  |
| Springs |  |  |
| Input impedance |  |  |
| Output impedance |  |  |
| Decay time |  |  |
| Mounting |  |  |

## Controls

| Control | Range / taper | What it actually changes in the circuit | Source |
| --- | --- | --- | --- |
|  |  |  |  |

## What gives this unit its character

<!-- The two or three things that distinguish it from the other units being modelled.
This is the section spec authors will read most. -->

## Measurable targets

<!-- Numbers a test can assert against. These become Acceptance Criteria in specs. -->

| Target | Value | Tolerance | Derived from |
| --- | --- | --- | --- |
|  |  |  |  |

## Open Questions

<!-- Unverified claims, conflicting sources, things needing a schematic or a real unit. -->

- [ ] [Question] — blocks: [which spec, if any]

## Sources

<!-- Numbered, with URLs. Mark each primary (schematic, datasheet, service manual),
secondary (reputable technical write-up), or anecdotal (forum post). -->

1. [Title](url) — primary | secondary | anecdotal

## Confidence

`low` | `medium` | `high` — and one sentence on why.
