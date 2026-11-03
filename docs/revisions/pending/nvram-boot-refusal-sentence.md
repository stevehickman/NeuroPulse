## DHF
author: NeurOne Firmware Engineering
text: **NP-FW-NVRAM-001 Rev {NP-FW-NVRAM-001 from → to}: a stale sentence is corrected, and this is the first change to go through the assign-at-merge scheme (`docs/revisions/README.md`).** No requirement changed.
## NP-FW-NVRAM-001
author: NeurOne Firmware Engineering
text: **§3.4.1 (`OI-NVRAM-19`): the sentence that said the runner compiles for ARM only is corrected.** `np_runner_load_tests` (`NP-FW-HUB-001` Rev 28) now runs `np_runner_load()` on the host and asserts that a refused boot returns `NP_HUB_ERR_BOOT_REFUSED` before the lease is claimed. `check-hub-bringup-order.ts` rule 3 still gates the order statically. No requirement changed.
