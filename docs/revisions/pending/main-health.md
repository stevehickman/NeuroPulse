## DHF
author: NeurOne Tooling
text: **{NP-SW-CI-001 from → to}: §6.19.4 merge queue deferred (personal-account repository); new §6.20, main health: a red `main` emails the owner and a required status holds merging until it is green; OI-SWCI-10 closed, OI-SWCI-54 Q5 answered again.**
## NP-SW-CI-001
author: NeurOne Tooling
text: **§6.19.3 notes the merge queue is offered only on organization-owned repositories; new §6.19.4 records the options and the principal's choice to keep `strict: true` for now (OI-SWCI-55 deferred). New §6.20: `main-health.yml` and `scripts/main-health.ts` email the owner on each change of colour on `main` and post the required status `main is green, or this PR is up to date with main` (the up-to-date requirement while `main` is red); `ci/required-statuses.txt`; `check-ci-scope.ts` correspondence F and posted statuses in C; `check-main-ruleset.ts` reads both manifests; `ci-changed-scope.sh` diffs a push to `main` from its last green head; runbook §6.20.3. §6.18.4 marked replaced. OI-SWCI-10 closed; OI-SWCI-54 Q5 answered again (`strict: false` with the hold).**
## LOG
text: **{date} — `OI-SWCI-55` DEFERRED: the merge queue is not available on `stevehickman/NeuroPulse`, a personal-account repository; the repository is not moved to an organization for now (the principal; `NP-SW-CI-001` §6.19.4).** The `merge_group` triggers from #1007 stay in place for a later move.
text: **{date} — `OI-SWCI-10` CLOSED: a red `main` is heard by email to the repository owner, high importance and a fixed subject (`[NEURONE MAIN RED]`), on each change of colour of any workflow on `main` (the principal; `NP-SW-CI-001` §6.20, `.github/workflows/main-health.yml`).** The recipient and sending account are repository secrets, not literals: the repository is public.
text: **{date} — `OI-SWCI-54` Q5 ANSWERED AGAIN: `strict: false`, with the required status `main is green, or this PR is up to date with main` holding merging while `main` is red; replaces option B of 2026-11-21 (the principal; `NP-SW-CI-001` §6.20).** The switch is a status posted by CI, not a token that edits the ruleset, because CI holds no admin token.
## OPEN OI-SWCI-55
after: c2c1b21c14f8bacb 2
before: a831d91b10933865 1
text:     > **UPDATE 2026-11-22 (later) — `OI-SWCI-55` DEFERRED (`NP-SW-CI-001` §6.19.4).** GitHub offers the merge queue only on organization-owned repositories, and `stevehickman/NeuroPulse` is owned by a personal account, so §6.19.3 step 2 cannot be done. The principal chose to keep `strict: true` and not to move the repository for now. The `merge_group` triggers stay; the item stays open until the repository is organization-owned.
text:     >
text:     > **UPDATE 2026-11-22 (third) — `OI-SWCI-10` CLOSED; `OI-SWCI-54` Q5 answered again (`NP-SW-CI-001` §6.20).** A red run on `main` emails the repository owner (`[NEURONE MAIN RED]`, high importance; `[NEURONE MAIN GREEN]` when it recovers), and the required status `main is green, or this PR is up to date with main` holds every pull request that is behind `main` while `main` is red. `strict` is to be `false`. `main`'s diff base is its last green head, so the revision App's docs-only commit no longer reports green over a red `main`. Settings half: §6.20.3 (the sending account's secrets, the ruleset).
text:     >
