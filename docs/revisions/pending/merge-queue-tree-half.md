## DHF
author: NeurOne Tooling
text: **{NP-SW-CI-001 from → to}: new §6.19, the tree half of a merge queue (`merge_group` triggers, merge-group diff base, guards); OI-SWCI-55 raised.**
## NP-SW-CI-001
author: NeurOne Tooling
text: **New §6.19: the four workflows holding required contexts trigger on `merge_group`; `ci-changed-scope.sh` diffs a merge group against its `base_sha` and never a last-green base; the revision-fragment and wavelength-register guards run in the queue (merge method `MERGE`); correspondence C requires the trigger; `check-main-ruleset.ts` R7 and L5 cover a `merge_queue` rule; the settings runbook (§6.19.3) and Q6 on `strict`. OI-SWCI-55 raised; no setting changed.**
## OPEN OI-SWCI-55
after: eeb4e2f3ecd7797f 2
before: a831d91b10933865 1
text:     > **UPDATE 2026-11-22 — `OI-SWCI-55` RAISED (`NP-SW-CI-001` §6.19).** The tree half of a merge queue is in: the four workflows that hold required contexts run on `merge_group`, diff against the group's `base_sha` and never a last-green base, and the revision-fragment and wavelength-register guards run in the queue too. `check-ci-scope.ts` (correspondence C) and `check-main-ruleset.ts` (R7, L5) keep it so. No setting was changed: switching the queue on (merge method `MERGE`), Q6 (whether `strict` stays on) and recording the queue's parameters are §6.19.3 steps 2 to 5.
text:     >
