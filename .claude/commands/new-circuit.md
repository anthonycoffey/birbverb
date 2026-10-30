Create a new circuit study from the canonical template.

Steps:

1. Read `docs/templates/circuit-study-template.md`
2. Ask the user for:
   - **Hardware** — the amp or unit being studied (e.g., "Fender Vibroverb 6G16")
   - **Years** — production years of the circuit revision
   - **ID** — next available CIRCUIT number. Check `docs/documentation/circuits/`.
3. Create a new file at `docs/documentation/circuits/{kebab-case-hardware}.md`
4. Fill in the frontmatter:
   - `id`, `title`, `hardware`, `years`
   - `status`: `draft`
   - `updated`: today's date (YYYY-MM-DD)
   - `author`: Anthony Coffey
   - `confidence`: `low`
5. Research the circuit before filling the body. Rules for this document type:
   - Every component value, impedance, and time constant needs a cited source
   - Mark each source `primary` (schematic, datasheet, service manual), `secondary`
     (reputable technical write-up), or `anecdotal` (forum post)
   - Never promote an anecdotal claim into the body as established fact — put it in
     Open Questions with the conflict named
   - Fill the **Measurable targets** table. If a property cannot be expressed as a
     number a test could assert, it does not belong in a spec yet.
6. Add a link to the new study in `docs/SUMMARY.md` under Circuit Studies
7. Present the new file to the user for review

Reminder: circuit studies describe hardware, never our DSP. Design decisions derived
from a study go in a spec or an ADR, not here.
