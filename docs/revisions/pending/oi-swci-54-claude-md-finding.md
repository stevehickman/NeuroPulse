date: 2026-11-26
## DHF
author: NeurOne Software Engineering
text: **{NP-SW-CI-001 from → to}: OI-SWCI-54 records what the audit found on its first real read (3 revision-App commits touch CLAUDE.md) and the decision to allow CLAUDE.md beside docs/.**
## NP-SW-CI-001
author: NeurOne Software Engineering
text: **§6.21.3, §6.21.5, new §6.21.6 and the OI-SWCI-54 row: the audit's first real read found 3 commits by the revision App that touch CLAUDE.md; the principal decided to allow CLAUDE.md beside docs/. The allowance is `allowed_paths` on the App's bypass in ci/main-ruleset.json, and the offline mode fails a pull request where it differs from the paths assign-revisions.yml stages.**
## LOG
text: {date}: {NP-SW-CI-001 from → to}: OI-SWCI-54: the audit of the revision App's pushes allows CLAUDE.md beside docs/ (the principal, 2026-11-26). The allowance is `allowed_paths` in ci/main-ruleset.json, and a pull request fails where it differs from what assign-revisions.yml stages (NP-SW-CI-001 §6.21.6).
