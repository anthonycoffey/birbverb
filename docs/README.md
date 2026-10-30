# docs/

This folder is the single source of truth for all documentation, circuit research,
specifications, and architectural decisions for **birbverb**.

birbverb is a document-driven project. The rule is simple: **a circuit study comes
before a spec, a spec comes before a test, and a test comes before an implementation.**
If you are writing DSP code and cannot point at the spec it satisfies, stop.

## Folder Rules

| Folder                        | Purpose                                                                 | Never put here              |
| ----------------------------- | ----------------------------------------------------------------------- | --------------------------- |
| `templates/`                  | Canonical document templates. Do not modify without review.             | Project-specific content    |
| `specs/plans/`                | Multi-phase roadmaps and release plans                                  | Completed work              |
| `specs/active/`               | Current feature and bug specs (draft → complete)                        | Archived work               |
| `specs/adrs/`                 | Architecture Decision Records — permanent, never archived               | Anything other than ADRs    |
| `specs/archive/`              | Completed or deprecated specs                                           | Active work                 |
| `documentation/circuits/`     | **Circuit studies** — the researched hardware each model is derived from | Opinions, DSP design        |
| `documentation/agents/`       | AI agent briefs — repo context for Claude and other AI tools            | Specs                       |
| `documentation/guides/`       | Procedural how-to documentation (build, test, author assets)            | Deep technical analysis     |
| `documentation/deep-dives/`   | Narrow-focus technical analysis (DSP algorithm comparisons, etc.)        | Step-by-step guides         |
| `documentation/repos/`        | Comprehensive repo/module technical reference                           |                             |
| `archive/`                    | General archive for deprecated non-spec documents                       | Active documents            |

### Why `circuits/` is separate from `specs/`

A circuit study answers *"what did the hardware actually do?"* — it is research, it
cites sources, and it is allowed to say "unverified". A spec answers *"what will our
code do?"* — it is a contract, and every requirement in it must be testable.

Keeping them apart means a measurement that turns out to be wrong gets corrected in
one place (the study) and the specs that depend on it can be re-derived deliberately.

## Spec Lifecycle

```
draft → ready → in-progress → review-pending → complete
                                    ↓
                               deprecated (from any status)
```

- **draft** — being written, not ready for implementation
- **ready** — spec is complete and approved for development
- **in-progress** — development is underway
- **review-pending** — implementation is complete, awaiting review
- **complete** — shipped and verified
- **deprecated** — no longer relevant or superseded

ADRs are permanent records. They are never archived or deleted.

## DDD + TDD Workflow

```
0. Research  → Write / update a circuit study in docs/documentation/circuits/
1. Plan      → Understand the problem space, identify unknowns
2. Spec      → Write a spec in docs/specs/active/ (status: draft)
3. Review    → Get spec to status: ready before writing code
4. Test      → Write failing tests first (RED)
5. Implement → Make tests pass (GREEN)
6. Refactor  → Clean up while keeping tests green
7. Archive   → Move completed spec to docs/specs/archive/
```

Step 0 is the one that is specific to this project. Every audible behaviour in
birbverb should trace back to a documented property of real hardware, or be
explicitly labelled as a liberty we took on purpose.

## Slash Commands

Use these Claude Code commands to create new documents from canonical templates:

| Command            | Creates                      | Destination                        |
| ------------------ | ---------------------------- | ---------------------------------- |
| `/new-spec`        | Feature spec                 | `docs/specs/active/`               |
| `/new-bug`         | Bug report                   | `docs/specs/active/`               |
| `/new-adr`         | Architecture Decision Record | `docs/specs/adrs/`                 |
| `/new-agent-brief` | AI agent brief               | `docs/documentation/agents/`       |
| `/new-circuit`     | Circuit study                | `docs/documentation/circuits/`     |

## Common Prompts for Claude

**Start a new feature:**

> "Read docs/specs/active/SPEC-XXX-title.md and implement it. Follow the tasks
> checklist and update the spec status to in-progress."

**Research before specifying:**

> "Use /new-circuit to start a study of the [amp model] reverb circuit. Cite every
> component value and flag anything you could not verify."

**Review a spec before coding:**

> "Review docs/specs/active/SPEC-XXX-title.md. Identify any gaps in the requirements
> or design before I approve it."
