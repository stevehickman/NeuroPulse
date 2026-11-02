# Revision numbers assigned at merge, not by hand — decision request

**Status:** PARTLY ANSWERED 2026-11-02 (§7); option not yet chosen, question 1 open. **From:** a Claude Code session working for the principal. **To:** the principal
(owner of `NP-QMS-DC-001`; the Quality Lead role is interim the CEO). No open-item ID exists for this; it is a
process question, and one is raised if the answer is "build it".

## 1. The question

Parallel sessions each hand-assign the next integer to a document's `**Revision:**`, its history row, its `Rev`
cell in the `NP-DHF-001` index and the DHF's own revision. Two sessions take the same integer and the second to
merge has to renumber. **Should a revision number be assigned when the change merges, by a script, instead of
by the author?** Pick one:

| Option | What changes | Cost |
|---|---|---|
| **A. Assign at merge** (this file's proposal, §3) | A PR no longer writes any revision number. A fragment file states the change; a workflow on `main` writes the numbers. | A workflow that may push to `main`, a new gate, a change to `NP-QMS-DC-001` §8.2 and `NP-CONV-001`. |
| **B. Reserve from a counter** | A script takes the next number from a counter branch, using the failed push as the lock. | Numbers can go out of order and leave gaps (§4). Needs a branch outside `claude/<topic>`. |
| **C. Stay as is** | Authors keep numbering by hand. `scripts/renumber-revisions.ts` repairs a collision after `git merge origin/main`. | None. Each collision still costs a merge, a push and a CI cycle. |

**Recommendation: A**, if the three conditions in §5 hold; otherwise C. B is not recommended.

## 2. What is known

- The DHF index (`docs/np_dhf_001.md`) was touched by **125 commits** between 2026-10-15 and 2026-11-01, `NP-SW-001`
  by 97, its history file by 50. Every PR that bumps any controlled document edits the DHF index row for it, so
  the DHF is the one file every such PR touches.
- PR #755 hit three merge conflicts in one afternoon, each because another PR merged first and took the same
  DHF or `NP-SW-001` number. A duplicate DHF revision had already happened once (the correction at the head of
  `docs/reference/np-dhf-001-revision-history.md`, 2026-08-12, Rev 29).
- The gates that read these numbers are `scripts/check-dhf-index.ts` rules A (each controlled document has an
  index row), B (the row's `Rev` equals the file's), C (the DHF's history has a row for its current revision).
  No gate requires numbers to be contiguous. I did not check whether any other script reads them.
- Hand-numbering is not only an inconvenience. When a session repairs a collision by hand it can damage a
  neighbour's row: the hand merge of #755 rewrote the cloud-store row 226 (`#752`) to the wrong
  `NP-SW-001` revision, found only when `scripts/renumber-revisions.ts` was replayed against it.

## 3. Proposal A in detail

**A PR states a change, not a number.** It adds one file, `docs/revisions/pending/<branch-slug>.md`:

```
bumps: NP-SW-001, NP-FW-HUB-001
author: NeurOne Firmware Engineering
summary: **{NP-FW-HUB-001 from → to}, {NP-SW-001 from → to}: the run loop wakes when a stop is posted.** …
```

`{NP-X-001 from → to}` is replaced by `NP-X-001 Rev 29 → 30`. The summary is written once and the script copies
it to the document's history table and the DHF history. The PR does not edit any `**Revision:**` line, any
index `Rev` cell, or any history row. It still edits the documents' content.

**A workflow on pushes to `main` assigns the numbers.** `scripts/assign-revisions.ts`, one run at a time
(`concurrency` group, no cancelling), for each pending fragment in merge order:

1. each document in `bumps` gets its current revision + 1; the DHF gets its own + 1 (one DHF bump per fragment,
   as now);
2. it sets those `**Revision:**` and `**Date:**` lines, the index rows, and appends the history rows;
3. it deletes the fragment and commits as a named bot, message `Assign revisions for #<PR>`.

It runs `check-dhf-index.ts` before committing and does not commit if that fails. A failed run leaves the
fragment in place and the workflow red.

**Gates:** a PR fails if it edits an assigned field (a `**Revision:**` line, an index `Rev` cell, a history
row) without being the bot; a PR that changes a controlled document without naming it in `bumps` fails (this
rule does not exist today for hand numbering either; whether to add it is §5 question 4). `check-dhf-index.ts`
is unchanged and runs on `main`.

**Window:** between the merge and the bot's commit (about a minute) `main` holds new content under the old
revision, with a fragment that explains it. Nothing reads `main` in that window except CI on other PRs, which
merge `main` and see a pending fragment.

**What goes away:** the textual conflicts on the `Revision` line, the DHF index row and the history files,
because no PR touches them, and the duplicate numbers.

**Estimated build:** the script and its self-test, the workflow, the gate, text changes in `NP-QMS-DC-001` §8.2
and `NP-CONV-001`, and a migration for PRs in flight.

## 4. Why not B

A counter branch gives each session a unique number, and two sessions cannot take the same one. It does not
keep numbers in merge order: if PR A reserves 230 and PR B reserves 231 and B merges first, `main` goes to 231
and then to 230, and the document's revision goes backwards. Abandoned sessions leave gaps. It also needs
sessions to push to a branch outside `claude/<topic>`, which they are told not to do without permission.

## 5. What I need back

1. **Can a GitHub Actions workflow push to `main`** on this repository (branch protection, required reviews,
   whether the Actions token has `contents: write`)? No workflow here has that permission today. I cannot see
   the repository settings. If the answer is no, the answer to §1 is C.
2. **Is a revision number assigned by a script at merge acceptable as document control?** In the current practice
   the author writes the number and the reviewer sees it in the diff. Under A the reviewer approves a change and
   the number is assigned after. This is arguably closer to assigning a number on release, but whether it meets the
   QMS and the regulator's expectations for document control is a Quality ruling I cannot make.
3. **Who is named as author of the bot's commit and of the history row?** The row's author column is a function
   (`NeurOne Firmware Engineering`) set by the fragment; the commit author would be the bot.
4. **Should a PR that changes a controlled document without bumping it fail?** Today nothing enforces it.
5. **In-flight PRs** at the time of the change: finish by hand, or convert.

Please send back: the option (A, B or C), the answers to 1 to 5, a short reason, and name, role and date. If A,
the change to `NP-QMS-DC-001` §8.2 is made in the same PR as the tool and logged in
`docs/status/completed-decisions.md`.

## 6. What does not change

Revisions stay integers, one per document per PR. The history tables, the DHF index and `check-dhf-index.ts`
rules A to H keep their meaning. Nothing in this file is a decision: until an answer is recorded, authors keep
numbering by hand and use `scripts/renumber-revisions.ts` after a collision.

## 7. Answers received

Recorded 2026-11-02 from the principal, in a session message. They are not yet a decision: the option is not
named (§1) and question 1 is open, so nothing in the repo acts on them.

| # | Answer |
|---|---|
| 1 | Not known. The principal asked how to check; the steps given were GitHub Settings → Branches / Rules (is `main` protected or under a ruleset, does it restrict who can push, is there a bypass list for the GitHub Actions app) and Settings → Actions → General → Workflow permissions (read and write). |
| 2 | **Yes.** A revision number assigned by a script at merge is acceptable as document control. (Given by the principal; a Quality Lead countersign is not recorded.) |
| 3 | The bot's commit and the history row carry the same author as the rest of the change (the PR's author), not a separate bot identity. |
| 4 | **Yes.** A PR that changes a controlled document without naming it in `bumps` fails. |
| 5 | In-flight PRs are brought across by running the tool on them and updating their numbers, not finished by hand. |
