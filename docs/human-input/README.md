# Human-input requests

Documents here exist to **get an answer from a person**: a clinical ruling, a regulatory view, a
principal decision, a supplier confirmation. Each one poses a question an open item is waiting on,
gives the reviewer what they need to answer it, and says what to send back.

**They are not controlled documents and not design inputs.** They carry no `**Document:**` serial
and are not in `NP-DHF-001`. An answer takes effect only when it is recorded in the owning file and
logged in `docs/status/completed-decisions.md`. Never quote a draft here as a decision.

## Rules

1. **One file per open item**, named `<open-item-id-lowercase>-<short-topic>.md`.
2. **Start with the status line** (`DRAFT`, `SENT <date> to <name>`, `ANSWERED <date>`), the sender and the
   addressee. An unnamed addressee is stated as such.
3. **Ask a question the reviewer can answer yes or no or by picking an option**, then give the
   options and what each one costs. Say what is already known and what is assumed.
4. **Do not assume the answer.** If the file asks for a ruling, nothing in the repo may act as if it
   has been given.
5. **State what you need back**: the ruling, a short rationale (CLAUDE.md §18 wants a derivation),
   conditions that would reopen it, and the reviewer's name, role and date.
6. **Never include UHDR content or personal data** (CLAUDE.md §5).
7. **When answered**, record the answer in the owning file and in the decisions log, set the status
   line to `ANSWERED`, and leave the file in place as the trail.

## Index

| File | Open item | Asks | Status |
|------|-----------|------|--------|
| `oi-risk2-06-clinical-question.md` | `OI-RISK2-06` | Should a cervical cardiac cutoff also withhold auricular VNS? | DRAFT, not sent |
| `oi-risk2-09-cvns-detection-adequacy.md` | `OI-RISK2-09` | Is the `REQ-CVNS-09` cardiac detection requirement clinically adequate? Five sub-questions (a)–(e). | DRAFT, not sent |
| `oi-risk2-04-counsel-engagement.md` | `OI-RISK2-04` | Commission outside regulatory counsel on the PBM irradiance ceilings (RISK-03)? Four open choices. | DRAFT, not sent |
| `oi-risk2-03-second-reader.md` | `OI-RISK2-03` | Second reader: confirm or challenge the five RETIRED risk dispositions (the item says nine; the table has five). | DRAFT, not sent |
| `oi-risk2-07-update-path-test.md` | `OI-RISK2-07` | Confirm the owed NV-pages-survive-update test, where it binds, and who carries it. Waits on the first safety-MCU update path. | DRAFT, not sent |
