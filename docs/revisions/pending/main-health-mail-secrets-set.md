## DHF
author: NeurOne Tooling
text: **{NP-SW-CI-001 from → to}: §6.20.5 steps 2 and 3 done (the mail secrets and the mail rule, the principal); §6.21.5 step 5: the first `main-pushes-audit` run crashed on `previous_filename: null`, and `check-main-pushes.ts` is fixed.**
## NP-SW-CI-001
author: NeurOne Tooling
text: **§6.20.5: step 2 (the sending account's secrets) and step 3 (a mail rule in Apple Mail and on the iPhone) are done, reported by the principal on 2026-11-25; step 6 stays open. §6.21.5 step 5: the first `main-pushes-audit` run stopped on a `TypeError` in `check-main-pushes.ts` (`jq` writes `previous_filename: null`); the script treats null as absent, with two new self-test cases; the audit's result is the next run after this merges.**
