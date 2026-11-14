date: 2026-11-13
## DHF
author: NeurOne Software Engineering
text: **{NP-SW-CLOUDSTORE-001 from → to}, {NP-SW-001 from → to}: account check run through the real S3 adapter on a stand-in; finding that bucket routes read "not sufficient" at space; Oracle pages re-read (#566).**
## NP-SW-CLOUDSTORE-001
author: NeurOne Software Engineering
text: **New §7.11: the account check over the real S3 adapter at an Oracle-style endpoint (3 tests, tag SWR-MG-12), the unknown-free-space finding and its resolution (the principal, 2026-11-13, option (a): unknown space passes with `spaceConfirmed: false`; new key `MIGRATION_PROBE_SPACE_UNCONFIRMED`), the re-run-before-migration gap (dormant), Oracle's pages re-read on 2026-11-13 and what a migration route through Oracle needs.**
## NP-SW-001
author: NeurOne Software Engineering
text: **§6.3.7 SWR-MG-12: a storage account that cannot report its free space does not fail the account check; the result says the room was not confirmed (the principal, 2026-11-13).**
