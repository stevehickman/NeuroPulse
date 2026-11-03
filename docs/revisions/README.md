# Revision numbers: assigned when a change merges

Decision: `docs/human-input/np-qms-dc-001-revision-assignment.md` (option A, the principal, 2026-11-02). The mode in
force is `docs/revisions/policy.json`.

## Why

Authors used to hand-assign the next integer to a document's `**Revision:**`, its history row, its `Rev` cell in the
`NP-DHF-001` index and the DHF's own revision. Two sessions working in parallel took the same integer, and the second
to merge had to renumber (125 commits touched the DHF in two weeks).

## Mode `assign-at-merge`

A PR **writes no revision number**. It adds one fragment, `docs/revisions/pending/<slug>.md`:

```
date: 2026-11-02                      (optional; default: the merge date, UTC)
## DHF
author: NeurOne Firmware Engineering
text: **{NP-SW-001 from → to}, {NP-FW-HUB-001 from → to}: what changed, for the DHF history.**
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3: what changed in this document, for its own history.**
## NP-FW-HUB-001
author: NeurOne Firmware Engineering
text: **…**
```

- One `## <serial>` section per controlled document the PR changes, and a `## DHF` section (the DHF is bumped once per
  PR). Each `text:` is one line and holds no `|`.
- `{X from}`, `{X to}` and `{X from → to}` (X is a serial, or `DHF`) become the numbers assigned. They work in a `text:`
  and anywhere in a bumped document, for example a callout `> **Rev {NP-FW-HUB-001 to} (2026-11-02) — …**`.
- The PR does not edit a `**Revision:**` line, a history row, or a `Rev` cell in the DHF index. `check-revision-fragments`
  fails it if it does, or if a controlled document changes with no section naming it.
- A **new** document starts at its first revision by hand, with its DHF index row.

When the PR merges, the `assign-revisions` workflow, on `main`, takes each fragment oldest first and sets each named
document's `**Revision:**` and `**Date:**` (current + 1), its DHF index row, and appends its history row (in the linked
`docs/reference/*-revision-history.md` if there is one); it deletes the fragment and commits, authored by the PR's author
and committed by the App `RevNumberFixer`.

A document with **no history table** (no `docs/reference/<serial>-revision-history.md` and no table in the document) is
numbered from its `**Revision:**` line, and the assigner starts a `## Revision history` section at the end of the
document, with a line saying earlier revisions were not recorded in a table, and the row. Later changes append to it. A
document that has the heading but no row is refused (the run stops with nothing changed): fix the table by hand.

## Bringing a hand-numbered branch across

```
git merge origin/main
bun scripts/assign-revisions.ts --convert      # undoes this branch's hand-assigned numbers and writes the fragment
bun scripts/check-revision-fragments.ts --base origin/main
git add -A && git commit
```

`--convert` turns `NP-X Rev a → b` in the history text into tokens. A callout in a document that names a revision by
hand (`> **Rev 30 (…)**`) it does not touch: replace the number with `{NP-X to}` yourself.

## Mode `hand`

The mode the repository shipped in, and the one to return to if the workflow cannot run. Authors write the numbers, and after a `git merge origin/main` that collides,
`bun scripts/renumber-revisions.ts` moves this branch's numbers past main's.

## Turning it on (once) — done 2026-11-02

1. The GitHub App `RevNumberFixer` is installed on this repository with Contents and Pull requests read and write, and is on
   the `main` ruleset's bypass list.
2. Repository secrets `REVBOT_APP_ID` (the App's numeric ID) and `REVBOT_PRIVATE_KEY` (the whole `.pem`).
3. PRs in flight are converted (above), or merged first.
4. Change `"mode"` in `docs/revisions/policy.json` to `"assign-at-merge"` in its own PR. That PR is the last to be
   hand-numbered, and the workflow runs when it lands to catch up anything pending.

Turning it off is the reverse edit; fragments already on `main` are still assigned.

## What can go wrong

- **The workflow fails** (the App token, a push refused): the fragment stays on `main` and the workflow is red. Fix the
  cause and run it again (Actions → Assign revisions → Run workflow, or any change to the workflow file or the policy);
  nothing is lost. A "Re-run" of the failed run uses the workflow file as it was then, so it cannot pick up a fix. `main` shows new content under the old revision until then.
- **`check-dhf-index` or `check-md-tables` fails after an assignment:** the script stops before committing, so nothing is
  pushed, the fragment stays, and the log shows the failure.
- **A fragment names a document that does not exist:** the run stops with nothing changed.
- **A document's `**Revision:**` is behind its own history** (two hand-numbered changes once set the same line and each added a
  row): the assigner takes the newest history row as the current revision, so the next number is never one the history
  already has, and the `**Revision:**` line jumps to it. Without that the run would stop ("its history already reaches …").
- **One fragment that cannot be assigned stops the ones behind it** (the numbers are a counter and the order matters): fix
  the cause and run it again.
