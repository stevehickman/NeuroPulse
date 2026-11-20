date: 2026-11-20
## DHF
author: NeurOne Software Engineering
text: **{NP-SW-001 from → to}: the backup bundle format and the recording archive format go back to version 1 (the principal, 2026-11-19: no copy of an earlier form of either should exist).**
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3.7 `SWR-MG-05`, `SWR-MG-29`, `SWR-MG-31`: neither format's version rose with the object ID, the per-file key or the epoch (the principal, 2026-11-19).** `FORMAT_VERSION` and the archive's `VERSION` are 1 again; the `older_version` refusal and the reading of an epoch-less bundle as epoch 0 are removed, so a body with no epoch is `malformed` and a file of an earlier form does not open. The rule that a format change after a release bumps the version stands.
