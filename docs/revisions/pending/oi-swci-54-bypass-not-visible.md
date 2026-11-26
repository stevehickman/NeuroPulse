date: 2026-11-26
## DHF
author: NeurOne Software Engineering
text: **{NP-SW-CI-001 from → to}: OI-SWCI-54 records that main-ruleset-live cannot see the bypass list with the job's token, and repairs a stray edit in §6.21.4.**
## NP-SW-CI-001
author: NeurOne Software Engineering
text: **§6.21.4, §6.21.5 and the OI-SWCI-54 row: the log of main-ruleset-live says the bypass list is not visible to the job's token, so the weekly job does not compare the bypass entries; --require-bypass is not added; a token that can read them is open and undecided. A line in §6.21.4 that the previous revision overwrote is restored. No workflow, script or setting changed.**
## LOG
text: {date}: {NP-SW-CI-001 from → to}: OI-SWCI-54: the log of main-ruleset-live says the bypass list is not visible to the job's token, so the weekly job does not compare it and --require-bypass is not added; a token that can read it is open (the principal, 2026-11-26).
