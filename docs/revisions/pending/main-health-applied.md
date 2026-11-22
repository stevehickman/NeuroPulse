## DHF
author: NeurOne Tooling
text: **{NP-SW-CI-001 from → to}: new §6.20.5, the main-health settings applied (required status added, `strict: false`); `ci/main-ruleset.json` records `strict: false`.**
## NP-SW-CI-001
author: NeurOne Tooling
text: **New §6.20.5: the principal added the required status and unticked *Require branches to be up to date*; `--live` reported only the strict flag, which confirms the posted status is bound to GitHub Actions (§6.20.4 item 5 closed). `ci/main-ruleset.json` now says `strict: false`.**
## LOG
text: **{date} — `OI-SWCI-54` Q5 APPLIED: the `Safety` ruleset requires `main is green, or this PR is up to date with main` and no longer requires a branch to be up to date (`strict: false`); `check-main-ruleset.ts --live` reported only the strict flag before `ci/main-ruleset.json` was updated (the principal; `NP-SW-CI-001` §6.20.5).**
