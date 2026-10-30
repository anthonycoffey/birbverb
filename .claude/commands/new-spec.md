Create a new feature spec from the canonical template.

Steps:

1. Read `docs/templates/feature-template.md`
2. Ask the user for:
   - **Title** — short descriptive name for the feature
   - **ID** — next available SPEC number (e.g., SPEC-001). Check `docs/specs/active/` and `docs/specs/archive/` for existing IDs to avoid collisions.
3. Create a new file at `docs/specs/active/SPEC-{ID}-{kebab-case-title}.md`
4. Fill in the frontmatter:
   - `id`: the SPEC ID provided
   - `title`: the title provided
   - `status`: `draft`
   - `created`: today's date (YYYY-MM-DD)
   - `author`: Anthony Coffey
   - Leave `reviewers` and `affected_repos` as empty arrays
5. Present the new file to the user for editing
6. Remind them: spec must reach status `ready` before development begins
7. For a spec that models vintage hardware, check there is a circuit study behind it in
   `docs/documentation/circuits/`. If there is not, write that first (`/new-circuit`) — the
   spec's acceptance criteria have to come from the study's Measurable targets table, not
   from judgement. Write acceptance criteria as **measurements with tolerances**, and state
   the tolerance's origin.
8. Add a link to the new spec in `docs/SUMMARY.md` under Active
