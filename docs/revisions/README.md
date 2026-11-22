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

### The decision log entry: `## LOG`

A PR that records a decision does not append to `docs/status/completed-decisions.md`. It adds the entry to its fragment:

```
## LOG
text: **{date} — `OI-X-01` CLOSED: what was decided (who; where it is recorded).** The rest of the entry, on one line.
```

- One `text:` line per entry, written as the log's `- ` item would be, without the `- ` (a leading `- ` is dropped). An
  entry may hold a `|`. `{date}` becomes the date assigned (the fragment's `date:`, else the merge date), and the
  revision tokens above work here too.
- The section takes no `author:` (git records the PR's author on the commit that appends it).
- A fragment may hold **only** a `## LOG` section, for a decision that changes no controlled document. It then bumps
  nothing, not even the DHF.
- `check-revision-fragments` rule K fails a PR that adds a `- ` entry to the log by hand. The log's header (the lines
  that are not entries) may still be edited in the PR.

Why: the log is append-only and every entry goes at its end, so any two PRs in flight both changed its last line and the
second to merge conflicted there.

### CLAUDE.md: `## CLAUDE.md`

CLAUDE.md has no `**Document:**` serial, but its `**Revision:** N (current)`, its row in the `NP-DHF-001` index and its
history (`docs/reference/claude-md-revision-history.md`, newest first) collided the same way. A PR that changes CLAUDE.md
leaves all three alone and adds the history entry to its fragment:

```
## DHF
author: NeurOne Quality
text: **CLAUDE.md Rev {CLAUDE.md from → to}: what changed, for the DHF history.**
## CLAUDE.md
text: **Rev {CLAUDE.md to} ({date}) — §N: the change in one sentence. No other locked decision changed.**
text: **What changed.** One paragraph per `text:` line.
text: **Why.** …
```

- The first `text:` must start `**Rev {CLAUDE.md to} ({date}) — `. Each `text:` is one paragraph and may hold a `|`. No
  `author:`.
- A `## DHF` section is required with it, because CLAUDE.md's DHF index row changes.
- At merge the workflow sets CLAUDE.md's revision to the next number, sets its DHF row and inserts the entry at the top of
  the history's "Current revision" section. `{CLAUDE.md to}` works in any `text:` of the fragment, the LOG included.
- `check-revision-fragments` rule L fails a PR that changes CLAUDE.md with no `## CLAUDE.md` section, or that sets the
  revision line, the DHF row or a `**Rev N (date)` history entry by hand.

### Open items: `## OPEN` (`docs/status/pending-decisions.md`)

The open-items page is edited in place, one item per line, and git refuses two branches' changes to adjacent lines, so
PRs that closed neighbouring items conflicted. A PR leaves the page as it found it and carries its edit in the fragment:

```
# edit docs/status/pending-decisions.md as usual, then:
bun scripts/assign-revisions.ts --convert-open     # moves the edit into the fragment and puts the page back
bun scripts/assign-revisions.ts --expand-open      # the reverse, to edit again (then --convert-open again)
```

`--convert-open` writes one `## OPEN <item>` section per changed run of lines. Nobody writes these by hand:

```
## OPEN OI-PRIV-22
was: 3fa2c19e0b7d4a61 1          (replace the 1 line whose text hashes to this)
text: - [x] **`OI-PRIV-22`** … the line as it should read
## OPEN OI-PRIV-42
after: 9c0d…  1                  (insert after this line; if another PR changed it,
before: 51ab… 2                   before these 2 lines instead)
text: - [ ] **`OI-PRIV-42`** …
```

- A hunk is found by the text of the lines it replaces, never by line number or by its neighbours, so a PR that changes
  the next item does not disturb it. A hunk's lines are widened only when they would not occur exactly once.
- At merge the workflow applies the hunks in place. `check-revision-fragments` rule M fails a PR that edits the page
  directly, and rule N dry-runs every pending fragment's hunks (other PRs' first, this one's last): a hunk written
  against an older page, because another PR changed the same item, fails on the PR, which then merges `main`, runs
  `--expand-open`, redoes the edit and converts again. With `main` requiring a branch to be up to date, N runs again
  whenever another PR's edit lands first. In the merge queue (`NP-SW-CI-001` §6.19) N also runs on the merge-group
  commit, after the fragments of the pull requests queued ahead, provided the queue merges by `MERGE`. With `strict`
  off (`NP-SW-CI-001` §6.20) N does not re-run when another PR lands first: two PRs that edit the same item can both
  merge, the second's hunk then fails in `assign-revisions` on `main`, which emails the owner, and it is fixed by hand.
- A fragment may hold only `## OPEN` sections. The page is not a controlled document, so they bump nothing.

### The human-input index: `## INPUT` (`docs/human-input/README.md`)

The index of human-input requests is a table, one request per row, and nearly every PR that raises, sends or answers a
request adds or changes a row, most often at the end of the table, which ends the file. Two PRs in flight conflicted
there the same way. It travels exactly as the open-items page does, under its own heading:

```
## INPUT OI-PRIV-11
was: 3fa2c19e0b7d4a61 1
text: | `OI-PRIV-11` (2) | `oi-priv-11-share-export.md` | … | ANSWERED 2026-11-16 |
## INPUT OI-PRIV-42
after: 9c0d…  1
before: end 1                    (the end of the file, ahead of its 1 trailing blank line)
text: | `OI-PRIV-42` | `oi-priv-42-….md` | … | DRAFT |
```

- `--convert-open` and `--expand-open` handle both pages: edit either or both, run it once, and each page's edit goes
  under its own heading. Rules M and N hold both. The request files (`docs/human-input/*.md` other than the README) are
  edited as usual; only the index moves.
- `before: end <n>` is written for an insertion at the very end of a page (on either page): if another PR changed the
  line above it, the insertion still lands at the end instead of failing. Two PRs that both append both land, the one
  merged second above the first.

When the PR merges, the `assign-revisions` workflow, on `main`, takes each fragment oldest first and sets each named
document's `**Revision:**` and `**Date:**` (current + 1), its DHF index row, appends its history row (in the linked
`docs/reference/*-revision-history.md` if there is one) appends any `## LOG` entries to the end of
`docs/status/completed-decisions.md`, applies any `## CLAUDE.md` section, applies any `## OPEN` edits to
`docs/status/pending-decisions.md` and any `## INPUT` edits to `docs/human-input/README.md`; it deletes the fragment and commits, authored by the PR's author
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

The mode the repository shipped in, and the one to return to if the workflow cannot run. Authors write the numbers, append their own entries to
`completed-decisions.md`, number CLAUDE.md and edit `pending-decisions.md` and the human-input index by hand, and after a `git merge origin/main` that collides,
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
- **An `## OPEN` or `## INPUT` hunk does not apply on `main`** (rule N passed, but something changed the page between the check and
  the merge): the run stops with nothing changed. Expand that edit by hand from the fragment on `main`, or revert the
  fragment, and run it again.
- **One fragment that cannot be assigned stops the ones behind it** (the numbers are a counter and the order matters): fix
  the cause and run it again.
