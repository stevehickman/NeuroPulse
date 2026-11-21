# Question for Safety and the principal: OI-PRIV-25 (NP-PRIV-ANALYSIS-004), what a factory reset does to the safety MCU's cardiac-cutoff log

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#953](https://github.com/stevehickman/NeuroPulse/issues/953)
**From:** NeurOne Firmware Engineering (`OI-PRIV-25` (NP-PRIV-ANALYSIS-004) in `docs/status/pending-decisions.md`; firmware privacy review `docs/np_priv_analysis_004.md`, Finding H5)
**To:** Safety (the RISK-25 owner and the Quality Lead, who re-scores under `NP-RM-001` §8.2) and the principal
**Response needed:** an option for Q1; yes or no on Q2; an option for Q3; a short rationale (CLAUDE.md §18); anything that would reopen it; your name, role and the date.

> **ID note.** `docs/status/pending-decisions.md` holds two rows called `OI-PRIV-25`. This brief is the
> **firmware** one (NP-PRIV-ANALYSIS-004, GitHub #953). The other (NP-PRIV-ANALYSIS-005 F5, the warranty
> token in both databases) is unrelated. The duplicate IDs will be renumbered separately.

## 1. The question

The safety MCU keeps a per-user record of outstanding cervical VNS cardiac cutoffs in its own flash
(`NP-SW-FAULTMSG-001` P1, `OI-FAULTMSG-01`, principal decision 2026-09-22). That record is RISK-25 control
**C4**: the 30 s lockout, app confirmation and repeat impedance check (`REQ-CVNS-09`, C3) survive a power
loss (`NP-RISK-002` §4.2, §4.3.2). Persisting the cutoff was right, and nothing here proposes weakening it.

**Nobody decided what a factory reset does to that record.** Today it does nothing. The record outlives the
reset, the resale and the clinic handover, and it keeps the earlier users' tags and the order of their
events.

Plain form: **when a device is factory reset, what must the safety MCU forget, and what must it keep?**

## 2. What is known (re-verified 2026-11-21 against `main` at `49d6c51f`; citations re-checked at `adae2396`)

### 2.1 What the record holds

`firmware/safety_mcu/src/np_nv_state.c` (SW01-M09, IEC 62304 Class C) is an append-only log of 64-bit
records on flash pages 62–63 (`0x0801F000`, 4 KB, reserved as `NVST` in
`firmware/safety_mcu/startup/stm32g071_flash.ld`).

| Field | Content | Source |
|---|---|---|
| `lo` | magic `0xA5`, record type, 16-bit `seq` | `np_nv_state.c:30`, `rec_lo()` at `:196` |
| `hi` | a user tag (SET, CLR, USER) or a count (COMMIT) | `:30` |
| Types | `SET(tag)` cutoff outstanding · `CLR(tag)` acknowledged · `USER(tag)` active user changed · `RESET … COMMIT(n)` snapshot | `:31-34` |

- **Plaintext.** Nothing encrypts or MACs a record. `program_verify()` writes the words as built.
- **No time.** No record carries a timestamp. "Never timed" in `data-architecture-detail.md` §5.1 is
  accurate about time. It is not accurate about **order**.
- **Ordered, with a lifetime counter.** `write_change()` takes `seq = max over both pages + 1`
  (`:393`), and `write_snapshot()` continues from it. Compaction **never resets `seq`**. So `seq` counts
  every record the device has written in its life. At `0xFFFF − 12` the write is refused rather than
  wrapped (`:394-397`).
- **History is bounded, not lifetime.** Compaction writes a snapshot of the current state to the empty page
  and erases the full one (`:420-425`). So the readable event history is the records since the last
  compaction (at most one 256-slot page). The finding's "lifetime" is true of `seq`, not of the event list.
  This refines H5. It does not overturn it.
- **The tag is stable and cross-device.** It is the first four bytes of the app profile's random UUID
  (`np_sw_faultmsg_001.md` §9.2), the same on every helmet that profile uses. The wire header's "a random
  32-bit value" is wrong. That is `OI-PRIV-29` (NP-PRIV-ANALYSIS-004, M2), not this item.

### 2.2 Factory reset never reaches it

- `firmware/factory_reset/` runs on the i.MX RT1062. Its steps R-1 to R-12
  (`include/np_factory_reset.h`) sanitize UHDR, zero SHDR and Config, mint a new salt and warranty token,
  and reboot. **No step addresses the safety MCU**, and nothing in `firmware/factory_reset/` names it.
- The safety MCU writes the log from two places only: the cardiac interlock's SET/CLR
  (`np_safety_main.c:672`) and the active-user command (`np_safety_main.c:706`). **No SPI command clears
  the log or erases pages 62–63.**
- Two documents forbid an update path from erasing those pages: `np_sw_faultmsg_001.md:293` and
  `np_fw_cvns_001.md:445`. RISK-25 hazard **25-b** scores that erasure S5 × P1, ALARP, on the control
  "no such path exists; the linker reserves the pages", with its test owed by `OI-RISK2-07`.

### 2.3 What the next owner inherits (in-band, no tools needed)

This is the larger half of the finding, and RDP does nothing about it.

- `view_clr()` (`np_nv_state.c:150-155`) clears the confirming user's tag, the unspecified entry and ANY.
  **It never clears another tag.** A new owner's profiles have new random tags, so an earlier owner's
  outstanding `SET(tag)` can never be cleared. A new owner could clear it only by naming that tag.
- `np_nv_cardiac_outstanding()` (`:459`) therefore stays true for the rest of the device's life. Bit 1 of
  the NV report goes to the hub on every SPI reply, and the apps show the **blanket warning** at every
  connection and ask **"is this your profile?"** before every cervical upload (C6; one confirmation covers one upload, `app/ios/NeurOne/Session/SessionProtocolUploader.swift:157-185`).
- The blanket warning is a **principal-approved disclosure** (`data-architecture-detail.md` §5.1, the "Safety-MCU cardiac-cutoff persistence" entry,
  2026-09-22): "anyone connecting to a **shared** device learns that someone on it had a cardiac cutoff".
  The approval was argued for people sharing one device. **It was not argued for a new owner after a
  reset,** to whom it says that a previous owner had a cardiac reflex event and never acknowledged it.
- `CLAUDE.md` §5.1: "an erase … returns the device to factory state". After R-1 to R-12 the safety MCU is
  not in factory state. Factory-fresh is "never-written storage → no user, nothing outstanding"
  (`np_nv_state.c:51`).

### 2.4 Read-out protection (RDP): planned, level not chosen, nothing written down

Searched on 2026-11-21: every file outside `firmware/vendor/` for `RDP`, `read-out protection`,
`readout protection`, `option byte`, `OPTR`, `WRP`, `PCROP` and `SWD`; and the tree for linker, OpenOCD,
STM32CubeProgrammer, option-byte and flashing scripts.

| Where | Finding |
|---|---|
| `firmware/safety_mcu/` | **No RDP or option-byte setup.** The only hit is `FLASH_SR_WRPERR` as an error bit in `platform/np_hal_nv.c:41`. The linker script reserves pages 62–63 and sets no protection. |
| Scripts and manufacturing tooling | **None exists.** No programming script, option-byte file or fixture configuration for the STM32G071 anywhere. |
| `NP-REG-UPG-001` §7.9 step 7 | "Lock the debug port (read-out protection) only after step 6 passes … **its exact setting for the part is not established here.**" |
| `NP-REG-UPG-001` §9, `OI-UPG-08` (iv) | "Debug-port lock: set RDP so no external interface can program the OTP window afterwards." The level (1 or 2) is not decided. |
| `docs/human-input/oi-upg-08-factory-step-scoping.md` item 4 | Lists the exact RDP setting as one of the decisions still needed. |
| `np_fmea_001.md` OI-FMEA-03 | Once said "PCROP active". OI-FMEA-03 closed by putting the key in OTP. No PCROP configuration exists in the tree. |

**So: RDP is required in principle for production (`OI-UPG-08` (iv)), but neither Level 1 nor Level 2 is
specified, and nothing applies it.** Until it is set, pages 62–63 are readable over SWD by anyone with
the board.

What each level would do for this item (STM32G0 family behaviour; the exact behaviour of the part is
`OI-UPG-08`'s to confirm on silicon):
- **Level 1:** debug access cannot read flash. Going back to Level 0 mass-erases the flash, the log
  included. It stops a casual SWD read. Fault-injection attacks on Level 1 have been published for several
  STM32 families, and whether any applies to the G071 is not established here.
- **Level 2:** the debug port is disabled for good, with no regression. It is stronger, and it also ends
  SWD failure analysis on returned units.

## 3. Questions

**Q1. What does a factory reset do to the safety MCU's cutoff log?**

- **A. An authenticated NV purge as a reset step that keeps every outstanding cutoff, collapsed to
  unattributable.** *(Recommended.)* A new SPI command, issued by the reset flow between R-3 and R-10
  and accepted only when no session is active and nothing is granted. The safety MCU rewrites pages 62–63
  as one committed snapshot:
  - current user = unspecified;
  - no per-user tags;
  - **ANY set if, and only if, anything was outstanding** (a tag, unspecified, or ANY);
  - `seq` restarted.

  The tags, the event order and the lifetime count are gone. A cutoff that was outstanding is **kept**,
  and it blocks everyone until someone completes `REQ-CVNS-09` (lockout, app confirmation, repeat
  impedance), which clears ANY.
  - *What the next owner is shown:* when nothing was outstanding, nothing, because the device is factory
    fresh. When something was outstanding, the existing blanket warning until a completed re-enable, worded
    as "a cardiac cutoff was outstanding when this device was reset". The person who starts the reset should
    be told first: "a cardiac cutoff is outstanding; acknowledge it before resetting, or the next user will
    be told one was". That choice belongs to them.
  - *RISK-25:*
    - **C3, C4 and C5 keep their effect.** The purge can never shrink the set of people blocked. For every
      pre-state and every user, `blocked_after(u) ≥ blocked_before(u)`, because ANY blocks everyone. The
      only change is that a cutoff outstanding at reset widens from one person to everyone. That is the
      fail-closed direction.
    - **Hazard 25-b changes shape.** A sanctioned path that rewrites pages 62–63 now exists. Its control
      becomes "no path may clear an outstanding cutoff; the purge collapses it", verified by a property test
      over every pre-state, and `OI-RISK2-07`'s test widens from "an update preserves the pages" to "no
      path clears an outstanding cutoff". The Quality Lead re-scores 25-b. On this design the expected
      result is no change to S5 × P1.
    - **A purge torn by power loss** falls under the existing decoding: an uncommitted snapshot merges
      additively and sets ANY (`np_nv_state.c:49-53`). That fails closed. Restarting `seq` is safe because
      both pages briefly hold the **same** committed state, so their order does not matter. The old page
      is erased only after the new snapshot is committed.
    - **A new availability hazard.** A Class B fault, or a compromised hub, that issues the purge turns a
      per-user block into a device-wide one and forgets the active user. That is S1, with no patient harm,
      but it is a nuisance withhold, so it bears on 25-c and 25-e (`OI-CVNS-13`). The command should be
      bound to the reset (accepted only while the R-3 marker is set, or carrying the reset command's
      signature), not merely gated on idle.
    - **C6 improves.** A resold device stops showing a stale blanket warning for ever. Today that permanent
      warning trains each later owner to click past C6, the control 25-c's P2 rests on.
  - *Cost:* a Class C change to SW01-M09 and the SPI command set (`NP-FW-CVNS-001` §5.5); a hub step in
    `firmware/factory_reset/` that must be idempotent and resumable like R-5 to R-10; an IEC 62304 Class C
    change record; the property test; app strings (CLAUDE.md §17). About 2–4 days of firmware work, plus
    the re-score.

- **A′. The same purge, but dropping outstanding cutoffs instead of collapsing them.** *(Listed so it can be
  rejected.)* The device really is factory fresh. **But a factory reset becomes a bypass of C3 and C4:** a
  blocked person resets the device and has cervical VNS again with no lockout, confirmation or impedance
  check. That is hazard 25-b made live by a user action (S5), and it puts a hub-commanded release of a
  Class C hold in place, against CLAUDE.md §4.2. It would be acceptable only if the purge refused to run
  while anything was outstanding, and that leaves a device the next owner can never reset.

- **B. Keep the cutoff, but store only the current view (compact at every write), and no history.** Every
  write lands as a full snapshot on the other page, then the old page is erased, with `seq` restarted per
  snapshot so it stops counting the device's life.
  - *What it fixes:* the event order inside a page and the lifetime counter, during ordinary multi-user use
    (a clinic device) as well as after a reset.
  - *What it does not fix:* **after a reset, the earlier owner's tags and the device-wide warning stay
    exactly as they are today** (§2.3). B does not answer the handover question on its own.
  - *RISK-25:* C4's semantics are unchanged. **Wear becomes the cost.** Every user switch, cutoff and
    acknowledgement erases a page, so with two pages each page is erased every second write. The STM32G0
    family datasheets give a minimum flash endurance of 10 000 cycles. That figure is not held in this repo
    and must be confirmed for the part, but at that figure the store lasts about 20 000 writes. A clinic
    device switching users 20 times a day reaches that in under three years, after which writes fail, a
    triple failure raises `NP_FAULT_SLOT_NVSTATE`, and the safety MCU faults. That is fail-safe, but it
    ends the device. Each write also opens more erase and program windows for a power-loss tear: 25-a's
    frequency rises (still S1, still fail-safe).
  - *Cost:* a Class C change to SW01-M09; an endurance analysis that does not exist today; re-running the 12
    fault-injection tests and adding wear tests.

- **C. Accept and document.** No firmware change. `data-architecture-detail.md` §5.1 and
  `NP-SW-FAULTMSG-001` §9 record that the log survives a factory reset and holds tags and order; the IFU
  and the reset screen say so; the protection is RDP alone.
  - *RISK-25:* no change to C1–C9 as written. **But the residual is not zero for safety either:** after a
    handover, the blanket warning and the "is this your profile?" prompt fire at every connection and before
    every cervical upload for the life of the device, and nobody can clear them (§2.3). A warning that is
    always on is a warning that is clicked past, and 25-c's P2 rests on that prompt being rare and meant.
    The Quality Lead would need to weigh that against 25-c.
  - *Privacy:* the §5.1 statement that an erase returns factory state becomes false for this store, and the
    2026-09-22 disclosure is extended, without being decided, from sharers to later owners. Accepting it is
    the principal's to do, because it lowers a protection (§5.1, "protection only goes up").

**Q2. Must production set RDP on the safety MCU, whatever Q1 decides?**

Yes or no, with the level left to `OI-UPG-08` (iv). *Recommended: yes, and this item cannot close until
`OI-UPG-08` (iv) names the level.* RDP is already required for the tier-identity OTP window. This item
adds a second reason: under every option, the tags and any outstanding cutoff are on pages 62–63 between
resets. Level 1 is enough for this item if Level 2's loss of failure analysis is unacceptable. Level 2 is
stronger.

**Q3. Should B be done as well as A?**

- **A. No, not now.** *(Recommended.)* With RDP set and A in place, the page history is reachable only by
  fault injection, during one owner's tenure, and only on a device that person controls. B's wear cost is
  real and unanalysed. Reopen if RDP cannot be set, or if Level 1 is shown to fall to a practical attack on
  the G071.
- **B. Yes.** For multi-user clinic devices, where several people share one device for years and a stolen
  device would show which tags had cutoffs in what order. Cost: as Q1 B, plus an endurance analysis
  first.

## 4. Recommendation

**Q1: A. Q2: yes. Q3: A.**

- A is the only option that returns the safety MCU to factory state **and** keeps every outstanding cutoff.
  It does that by moving the hold toward fail-closed (one person to everyone), never away from it.
- A′ is a bypass of RISK-25 C3 and C4 and must not be built.
- B leaves the handover problem untouched and spends flash endurance nobody has measured.
- C makes a later owner live with a permanent, uncleared warning about someone else's cardiac event. That
  is a disclosure nobody approved, and a slow erosion of C6.
- RDP is owed anyway (`OI-UPG-08` (iv)), and it covers what A cannot: the tags held between resets.

## 5. What each ruling changes

Nothing below is done until the ruling is recorded.

| Ruling | Changes |
|---|---|
| Q1 A | `NP-FW-CVNS-001` §5.4.1 and §5.5 (the purge command and its preconditions); `np_nv_state.c` (a purge that writes the collapsed snapshot); `np_safety_main.c` (dispatch, idle and reset binding); `firmware/factory_reset/` (a step between R-3 and R-10, resumable, plus the reset-screen notice); `NP-FW-EMMC-002` §B.3; `NP-SW-FAULTMSG-001` §9; `NP-RISK-002` §4.3.3 25-b's control text and re-score; `OI-RISK2-07`'s test widened; `data-architecture-detail.md` §5.1 (the cardiac-cutoff persistence entry) (the warning after a reset, and "ordered, not timed"); app strings in `locales/*.json`; tests tagged to a new requirement for the purge (written to §18: what fails is a later owner inheriting tags and a hold they cannot clear; it traces to CLAUDE.md §5.1 "an erase returns factory state" and to RISK-25 C4) |
| Q1 B | SW01-M09 write path and `seq`; an endurance analysis; `NP-RISK-002` 25-a frequency note; the 12 fault-injection tests re-run, wear tests added |
| Q1 C | `data-architecture-detail.md` §5.1 (the cardiac-cutoff persistence entry) and `NP-SW-FAULTMSG-001` §9 record the residual; IFU and reset-screen text; `NP-RISK-002` records the C6 effect for the 25-c re-score; the principal's acceptance in `docs/status/completed-decisions.md` |
| Q2 yes | `OI-UPG-08` (iv) gains this item as a second reason; `NP-REG-UPG-001` §7.9 step 7 names pages 62–63 |

## 6. What this brief did not do

- It changed no firmware. Cutoff persistence (`OI-FAULTMSG-01` P1) is untouched.
- It did not decide the RDP level. That is `OI-UPG-08` (iv).
- It did not take up the tag's stability across helmets, or writing another person's tag over BLE. Those
  are `OI-PRIV-29` (NP-PRIV-ANALYSIS-004, M2). If `OI-PRIV-29` makes the tag per (profile, helmet), the
  tags in this log stop linking one person across devices, but they still survive a reset.
- It did not check whether the hub's own cervical fault records (`np_cvns_fault_summary.c`, GATT 0x0014)
  survive a factory reset. They are on the hub, which R-5 to R-7 do reach. That is outside this item.
