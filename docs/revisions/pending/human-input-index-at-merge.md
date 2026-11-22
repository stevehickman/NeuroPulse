## DHF
author: NeurOne Quality
text: **CLAUDE.md Rev {CLAUDE.md from → to}: an edit of the human-input index (`docs/human-input/README.md`) is written in the PR's revision fragment as `## INPUT` hunks and applied when the PR merges, as an edit of the open-items page already is; the CLAUDE.md index row follows.** No controlled document's content changed.
## CLAUDE.md
text: **Rev {CLAUDE.md to} ({date}) — Document Map: an edit of the `docs/human-input/README.md` index goes in the PR's revision fragment as `## INPUT` hunks, written by `--convert-open`, and is applied when the PR merges. No locked decision changed.**
text: **What changed.** The human-input index is a table with one request per row, and almost every PR that raises, sends or answers a request adds or changes a row, most often at the end of the table, which is the end of the file. It now travels as the open-items page has since Rev 86: `bun scripts/assign-revisions.ts --convert-open` turns an ordinary edit of it into `## INPUT` hunks in the fragment and puts the page back, and the `assign-revisions` workflow applies them in place at merge. `check-revision-fragments` rule M now fails a direct edit of either page, and rule N dry-runs both pages' pending hunks. An insertion at the very end of a page also gets a `before: end <n>` anchor, so a row appended to the table still lands when another PR changed the row above it. The request files themselves are edited as before. The Document Map row that lists what a fragment carries names the index.
text: **Why.** Git refuses two branches' changes to the same or adjacent lines, so two PRs that each added a row at the end of the index, or changed neighbouring rows, conflicted, and with `main` requiring a branch to be up to date each merge sent the other back to resolve it by hand. The index was the most frequent remaining source of such conflicts.
text: **What it leaves.** The §1 to §6 and §16 to §23 numbering and every subsection number are unchanged. This fragment's own edit of the index (the note on how a PR changes it) is the first applied that way.
## LOG
text: **{date} — Edits of the human-input index (`docs/human-input/README.md`) are written when a PR merges, not by the PR (CLAUDE.md Rev {CLAUDE.md to}; `docs/revisions/README.md`).** The index is one table with a row per request, and PRs that added a row at its end or changed neighbouring rows conflicted, as the open-items page did before Rev 86. A PR now carries its edit of the index as `## INPUT` hunks in its revision fragment, written by `bun scripts/assign-revisions.ts --convert-open` alongside any `## OPEN` hunks, and the `assign-revisions` workflow applies them in place at merge. `check-revision-fragments` rule M fails a direct edit of either page and rule N dry-runs every pending hunk of both. An insertion at the end of a page is also anchored to the end of the file (`before: end <n>`), so it lands when another PR changed the line above it. In mode `hand` the index is edited directly as before.
## INPUT edit
after: 7860f84d497d51bc 2
before: b66542635d2ad70d 1
text: > **How a PR changes this file (since 2026-11-22).** Not directly. Edit it as usual (a new row, a status cell), then run
text: > `bun scripts/assign-revisions.ts --convert-open`: the edit moves into the PR's revision fragment as `## INPUT` hunks
text: > and this file goes back as the branch found it; the `assign-revisions` workflow applies the hunks in place when the PR
text: > merges. `--expand-open` puts them back to edit again. Each hunk is found by the text of the lines it replaces, so PRs
text: > that change neighbouring rows, or both add a row at the end, no longer conflict; two that change the same row do, as a
text: > failed check on the second (`check-revision-fragments` rule N). The request files themselves are edited as usual.
text: > Procedure: `docs/revisions/README.md`.
text:
