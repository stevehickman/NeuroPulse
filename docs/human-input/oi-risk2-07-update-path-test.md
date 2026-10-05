# Owner request: OI-RISK2-07 (verification owed by the first safety-MCU update path)

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Quality (`NP-RISK-002` §4.3.3 hazard 25-b, §6 `OI-RISK2-07`)
**To:** Quality and Firmware owners (the item's owner column reads "Quality + FW"). No names are assigned.
**Response needed:** the confirmations and choices in section 4, plus names and a date.
**This is not a question about a present defect.** Nothing is wrong today. The item exists so the obligation does not get lost.

## 1. What is owed

> Any change that adds a safety-MCU firmware update path must carry a test that NV pages 62–63 (the
> persisted per-user cardiac cutoff) survive the update.

Hazard 25-b: *a future update path erases pages 62–63 and silently clears every outstanding cutoff.*
Rated S5 × P1 = ALARP. The cutoff is the cardiac lockout for cervical VNS. If an update erased it, a
person who had a cardiac event could be stimulated again with no app confirmation and no repeat
impedance check, which is exactly the failure Rev 6 closed.

The item moved here from `OI-RISK2-05` when that closed on 2026-09-25, so it was not dropped.
**Blocking:** the first safety-MCU update path.

## 2. What exists today

| Item | State |
|------|-------|
| Update path for the safety MCU | **None.** The item says so, and I found no code or document that builds one. |
| Reserved region | `NVST` at `0x0801F000`, 4 KB, flash pages 62–63, in `firmware/safety_mcu/startup/stm32g071_flash.ld` and `NP_NV_FIRST_PAGE` in `np_safety_config.h` |
| Design rule | `NP-FW-CVNS-001` §5.4.1: "a future safety-MCU update path must never erase them" |
| Related policy | `NP-REG-UPG-001` §7.7: "the safety MCU is never updated by automated OTA"; a key rotation is a **user-confirmed safety-image update**. Custody and key list are `OI-UPG-08`. |
| Existing test | `firmware/safety_mcu/tests/np_nv_state_tests.c` tests the log, not an update. |

**The control is a linker reservation plus a rule in a document.** Neither stops a future tool from
erasing the pages. A flasher that does a full-chip or mass erase would clear them regardless of the
linker script. That is why a test is owed, and it is why the test has to run against the **actual
update tool**, not only the image.

## 3. What a sufficient test would show

This is a proposal for you to correct, not a requirement. Nothing requires it until Quality adopts it
(CLAUDE.md §18).

1. Record a cutoff for a test user in pages 62–63.
2. Run the update through **the real path**, including its failure and interrupted cases.
3. Reboot and confirm the user is still withheld from `NP_SAFETY_EN_CVNS` until the full re-enable
   sequence completes. Confirm unattributable and other users' entries survive too.
4. Repeat with the update **interrupted** (power loss mid-write), and with a **rollback** if one exists.
5. Confirm the failure direction: if the pages cannot be preserved, the update **refuses**, and does
   not proceed and clear them.

## 4. What we need back

1. **Confirm the obligation stands** as worded, or say what changes it. In particular: is
   "survive the update" enough, or should it also say "survive a **downgrade**" and "survive a key
   rotation"?
2. **Where does it bind?** Name the change that triggers it: a bootloader, an SWD/factory programming
   step, a field update, or the user-confirmed key-rotation update in `NP-REG-UPG-001` §7.7. Is the
   factory programming step (which today has to write the tier record) a "path" for this purpose?
3. **Who carries the test?** The item says "whichever change adds the path". Confirm that the author
   of that change owns it, and that review cannot approve the change without it.
4. **Is a gate wanted now?** For example, a check that fails CI if the linker script's `NVST` region
   moves or shrinks. This would guard the reservation, though not the tool. Yes or no. It is new work.
5. **Owners:** one name each for Quality and Firmware, with the date.

## 5. What this does not decide

- It does not close hazard 25-b or change its rating. P1 stays a target (`NP-RISK-002` §4.3.1).
- It does not design an update path. Whether one is built, and by what route, is `OI-UPG-08` and
  `NP-REG-UPG-001`'s call.
- Without an update path to test, **no one can answer item 3 of section 3 yet.** If you would rather
  hold this request until a path is proposed, say so. The item already says it waits on the first one.

## References
`docs/np_risk_002.md` §4.3.3 (25-b), §6 `OI-RISK2-07` · `docs/np_fw_cvns_001.md` §5.4.1 ·
`docs/np_reg_upg_001.md` §7.7, `OI-UPG-08` · `firmware/safety_mcu/startup/stm32g071_flash.ld` ·
`firmware/safety_mcu/include/np_safety_config.h` · `firmware/safety_mcu/tests/np_nv_state_tests.c`
