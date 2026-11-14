date: 2026-11-13
## DHF
author: NeurOne Software Engineering
text: **{NP-SIM-FW-001 from → to}: the simulator stores the hub's Config, UHDR and SHDR partitions in unlinked owner-only files and the hub's own logger and persistence write to them; development tool only, no device document changed.**
## NP-SIM-FW-001
author: NeurOne Software Engineering
text: **§5, §7, §26, §27, §28, §31, new §33: `REQ-SIMFW-25` and its five parts; `OI-SIMFW-51` partly closed (Config, UHDR and SHDR are littlefs instances in the device's geometry over unlinked, 0600, sparse files; the session logger, log backend, session count and module map write through the device's own code; `np_lfs_log_instance.c`, `np_hexmap_nvram.c`, `np_map3_record.c` and `np_hexmap_inventory.c` are linked); `OI-SIMFW-05` records what of `REQ-SIMFW-21` is built (parts a, b, d for the three partitions) and what is not; `OI-SIMFW-79` raised (the log file layer is the twin's own because the device has none, and `scripts/check-lfs-caller-rules.ts` was extended for it, which the principal has not seen); `UC-182` registered.**
