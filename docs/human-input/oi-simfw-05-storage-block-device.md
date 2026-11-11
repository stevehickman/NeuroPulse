# Principal question for review: OI-SIMFW-05, what the twin's storage sits on

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Software Engineering (`OI-SIMFW-05`, `NP-SIM-FW-001` §7, §29)
**To:** The principal (with the owner of `NP-FW-NVRAM-001` for Q2)
**Response needed:** an option for Q1 and an answer to Q2, with a short rationale, name, role and date.
**Why now:** the twin already has one block device, chosen while building the GATT slice (`NP-SIM-FW-001` §27) and never recorded as the choice `OI-SIMFW-05` asks for. `OI-SIMFW-51` (the hub's other storage and records) needs a device per partition, so the choice is about to be made again. Nothing in the repository acts as if this were answered.

## 1. What is known (read on 2026-11-11)

- **The item's own open question is settled by the tree: the hub's power-loss test device can be reused, and is.** `firmware/platform_sim/CMakeLists.txt` links `firmware/hub_control/tests/np_lfs_powerbd.c`, and `np_sim_gatt.c` binds it to the device's own Config-instance geometry (`np_lfs_instance.c`), wipes it, formats and mounts through `np_cfg_store.c`.
- **It is a RAM medium.** The media array is a static buffer in the twin's process. Nothing under `firmware/platform_sim/` opens a file for storage, and no command of the twin or the bridge takes a path.
- **Checked against `REQ-SIMFW-08` as built:** the requirement says the storage is synthetic and the twin never mounts, opens or accepts an image of a real UHDR or SHDR partition. A RAM medium that starts wiped, with no path input, has no way to be given an image. That holds for the one instance that exists (Config). The UHDR and SHDR partitions are not modelled at all.
- **Nothing persists.** Each start of the twin is a new, empty part. A setting written over GATT is gone when the process ends.
- **The design (§3) says "file-backed block device for littlefs".** What was built differs from it.

## 2. What is assumed

That `OI-SIMFW-51` goes ahead in some form, so more than one partition will need a medium.

## 3. Questions

**Q1. What medium do the twin's partitions sit on?**

- **(A) RAM only, as built.** Costs: nothing survives a restart, so a scenario about what the hub does with yesterday's settings or logs has to write them first in the same run. Gains: `REQ-SIMFW-08` holds by construction, since there is no path and no file to point at a real image; the device already exists and has power-loss injection.
- **(B) File-backed, as §3 says.** Costs: a path enters the twin, and with it the question `REQ-SIMFW-08` exists to prevent: something must stop that path from being a copy of a real partition. That needs a rule the twin can enforce (for example, it creates the file itself and refuses one it did not create), and that rule is design work. Gains: state survives a restart, which is closer to the device.
- **(C) RAM, with an explicit save and load of synthetic state the twin itself wrote.** Costs: a format and a check that a loaded state came from the twin; the same enforcement question as B, in a narrower place. Gains: persistence where a scenario asks for it, RAM otherwise.

**Q2. Which partitions does the twin hold, and are UHDR and SHDR among them?** `OI-SIMFW-51` asks the owner of `NP-FW-NVRAM-001` the first half. The second half is the principal's: a twin that holds a UHDR partition holds synthetic content under a TEST key only (`REQ-SIMFW-08`, `CLAUDE.md` §5.1), and whether it should hold one at all has not been said.

## 4. What to send back

The option for Q1, the answer to Q2, a sentence on why, anything that would reopen it, and name, role and date. The answer is recorded in `NP-SIM-FW-001` §7 (`OI-SIMFW-05`) and in `docs/status/completed-decisions.md`.
