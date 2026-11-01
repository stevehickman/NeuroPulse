# Principal and product sign-off for review: NP-ACC-PRIORITY-001, the accessory priority set

**Status:** DRAFT, ready to send; not yet sent (no addressee named). Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Systems Engineering (`docs/np_acc_priority_001.md` Rev 5, GitHub #344; the row in `docs/status/pending-decisions.md` closes on this sign-off)
**To:** The principal and product (the document's Approved By line reads "pending principal + product review"). This item has no open-item ID of its own.
**Response needed:** a sign-off, a sign-off with changes, or a rejection of **section 3**, plus an answer on the one flagged placement; name, role and date.
**Why now:** nothing is blocked. The one output that does not wait on the ranking (section 7, the mastoid-pad anchor boss) is time-limited by shell tooling's first cut.

## 1. The question

Do you approve the **criterion** in `NP-ACC-PRIORITY-001` §3, by which accessories and companion software are ranked? The ranking in §5 is an output of §3 and is
correctable one row at a time. Sign-off on §3 makes §5 maintain itself. Sign-off on §5 alone leaves the next accessory to reopen the list, which is what GitHub #344 was raised to end.

## 2. What is known

- §3 states four tests applied **lexicographically, not weighted**: C1 access, C2 binding relief, C3 continuity (split into blocking and degrading), C4 envelope, C5 experience, then reach, irreversibility of omission and engineering cost to ship.
- **No cost, COGS, BOM, margin or retail figure is an input** (`CLAUDE.md` §2.1: every figure is a floor; `OI-COST-10`).
- **Dependencies are recorded in a Gate column, never ranked**, so the set does not re-rank when a trial slips. §6 gives the triggers for re-running §5 and says a calendar is not one.
- §4 tests the criterion against its precedent (`NP-THERM-COOL-001` D-3, chiller above ice pack) and reproduces both its direction and its "low".
- The set holds 19 items in 18 ranks (rank 18, the Apple Watch app, is retired in place as of Rev 5). Rank 1 is the 40 Hz mastoid vibrotactile pad and rank 2 the S3 prescription clip.
- **One placement is flagged as judgement, not derivation: rank 16=, the EC lens, C4 versus C5.** The other flagged placement, rank 2, was settled on 2026-09-14 (`OI-ACC-01`).
- **§7, the one output that does not wait:** provision the mastoid-pad anchor boss at first tooling cut whichever way HOPE Phase 3 (#30) reads out. It changes no tooling specification.

**Not known:** whether product agrees that "value delivered" is the right axis, rather than, for example, expected demand. §3 rules demand and revenue out as inputs.

## 3. Questions

**Q1. Sign off §3 (the criterion)?** Options: yes; yes with changes (state them); no (state which test is wrong).
**Q2. Rank 16=, the EC lens: C4 (envelope) or C5 (experience)?** The document flags this placement as judgement rather than derivation; the sender offers no reading here.
**Q3. Confirm §7?** Provision the mastoid-pad anchor boss regardless of HOPE Phase 3.
**Q4. Who owns re-running §5** when a §6 trigger fires?

## 4. What the sender needs back

1. The answer to each question.
2. For any "no" or change, what fails under the current criterion (CLAUDE.md §18).
3. Name, role and date. The sender sets the document's Status and Approved By lines only after the answer is recorded in `docs/status/completed-decisions.md`.
