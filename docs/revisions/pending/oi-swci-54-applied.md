date: 2026-11-25
## DHF
author: NeurOne Software Engineering
text: **{NP-SW-CI-001 from → to}: OI-SWCI-54 records the App id as confirmed and the ruleset half of Q2 as in force.**
## NP-SW-CI-001
author: NeurOne Software Engineering
text: **New §6.21.5: the ruleset half of Q2 is in force (read live on 2026-11-25), the App id `5585393` is `RevNumberFixer` (confirmed by the principal), and what is still open. §6.17 and the OI-SWCI-54 row updated. No workflow, script or setting changed.**
## LOG
text: **{date} — The revision App on the `main` ruleset's bypass list is `RevNumberFixer`: App id `5585393` confirmed (principal; `OI-SWCI-54`, `NP-SW-CI-001` §6.21.5).** It was written on 2026-11-11 as not confirmed. The ruleset half of Q2 was found in force on 2026-11-25: `bun scripts/check-main-ruleset.ts --live --require-bypass` printed `OK` from `main` at `fb9137f`, so GitHub requires the 14 manifest contexts, `SHDR Schema Privacy Gate` and `Warranty No-Join Privacy Gate` among them. Who applied it, and when, is not recorded. Still open under `OI-SWCI-54`: the first run of `main-pushes-audit` and of the `main-ruleset-live` job.
