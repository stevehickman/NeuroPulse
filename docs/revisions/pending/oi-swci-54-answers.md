date: 2026-11-24
## DHF
author: NeurOne Software Engineering
text: **{NP-SW-CI-001 from → to}: the five main-ruleset questions are answered (OI-SWCI-54): two privacy gates join the required contexts, the revision App's pushes are audited, and the other settings stay as they are.**
## NP-SW-CI-001
author: NeurOne Software Engineering
text: **New §6.21: Q1 B (audit of the App's pushes, `check-main-pushes.ts`), Q2 set P (`SHDR Schema Privacy Gate`, `Warranty No-Join Privacy Gate`), Q3 A, Q4 A, Q5 A; the OI-SWCI-54 row updated. The ruleset half of Q2 is the principal's.**
## LOG
text: **{date} — The five questions of `docs/human-input/oi-swci-54-main-ruleset.md` are answered (principal; `OI-SWCI-54`, `NP-SW-CI-001` §6.21).** **Q1: B.** The revision App keeps its bypass in mode `always`, and an audit checks what it pushed: a commit on `main` with no merged pull request may touch only `docs/` (`scripts/check-main-pushes.ts`, weekly in `build-all.yml`). Detection, not prevention; option C (a pull request instead) was not chosen. **Q2: as recommended, set P.** `SHDR Schema Privacy Gate` and `Warranty No-Join Privacy Gate` join the required contexts with `reporting paths cannot leak a sensitive predicate (§5.1)`, which was required on 2026-11-21. The tree half is done and the ruleset half is the principal's. **Q3: A.** `require_extra_approval_for_unattributed_changes` stays `true`. **Q4: A.** The `copilot_code_review` rule stays. **Q5: A.** `strict_required_status_checks_policy` stays `false`; the merge hold of 2026-11-22 (`main is green, or this PR is up to date with main`) is a separate status and stays. The principal gave the options and no separate reasons, so the reasons recorded are the brief's. Not answered: whether App id `5585393` is `RevNumberFixer`.
