# OI-SIMFW-32: a fault restored after a warm reset loses its slot, and nothing clears it but power

**Status:** DRAFT, not sent. **From:** a Claude Code session working for the principal (2026-11-11). **To:** the safety
MCU's owner (not yet named), with Risk. **Owning item:** `OI-SIMFW-32` (`docs/np_sim_fw_001.md` §22). Related:
`OI-SIMFW-33`, `OI-SIMFW-27` (Q8, where a latch lives), GitHub #826, #467.

**This is a proposal, not a decision.** No firmware was changed for it.

## 1. What happens today, and what the documents say

After an independent-watchdog reset with a fault in the latch (status 0x85, slot 0xFD in the simulator run):

| Step | As built | As stated |
|---|---|---|
| First reply after the reset | Status 0x01 (generic `FAULT`), slot 0xFF. `main()` sets only the `FAULT` bit and never reads the latched slot or status into the state | `np_safety_main.c`: "report it on first heartbeat reply", "the preserved latch data (fault slot + specific status bits)" |
| Second loop iteration onward | `np_fault_latch_commit()` writes the generic state over the latch, so the slot and status bits are gone from the latch too | `np_fault_latch.c`: a fault that triggers a reset "will persist until the app explicitly clears it" |
| Clearing | Only a power-on reset. `np_fault_latch_clear()` has no caller. `np_session_sig_reenable()` clears only slot 0xFD, and the restored slot is 0xFF | `NP-SW-001` SW01-M08: "requires explicit app clear". `NP-FMEA-001` FMEA-M08-01: cleared only by `np_fault_clear()` "which requires explicit validated app command via signed session protocol"; "fault state also persisted in backup register" |

The outcome is fail-closed: every channel is withheld until power is removed. What is lost is the diagnosis (which
fault), and what does not exist is the clear path that SW01-M08 and FMEA-M08-01 describe. There is no backup register
either: the headset has no battery (CLAUDE.md §4.5; the same finding as FMEA-M05-06 at Rev 8).

## 2. The questions

**Q1. Should the latched slot and status reach the hub after a warm reset?**

| Option | What it means | Cost |
|---|---|---|
| **A. Report them, and keep them** | At init the state takes the latched slot, with `FAULT` and `CUTOFF` set, and the commit no longer overwrites them. The code then does what its comments say | A small Class C change with a host test. **Two traps, both must be closed in the same change:** a restored slot of 0xFD would be cleared at the next session start by `np_session_sig_reenable()`, because the failure count is zeroed by the reset, which would make a restored fault easier to clear than today; and restored `THERMAL`, `CHARGE`, `CARDIAC`, `WATCHDOG` or `SIG_PENDING` bits would be acted on by modules that did not set them. So only the slot is restored into the state, the restored fault is marked not recoverable at a session start, and the latched status is reported, if at all, without being loaded into the live status |
| **B. Keep as built** | The comments in `np_safety_main.c` and `np_fault_latch.c` are corrected to say a restored fault reads as generic `FAULT`, slot none | The hub and the app can say only "a fault survived a reset". Support cannot tell a signature fault from a clock fault |

**Q2. How is a restored fault cleared?**

| Option | What it means | Cost |
|---|---|---|
| **A. Power-on only (as built)** | The recovery is "unplug the headset", and the documents say so | SW01-M08's description and FMEA-M08-01's control are re-derived on what exists (as FMEA-M05-06 was at Rev 8). The app needs a message that says to unplug. In a clinic, a unit that faults is out of use until someone does |
| **B. A signed clear command** | The control FMEA-M08-01 describes: a new SPI command, verified with the session key, that calls `np_fault_latch_clear()` | New Class C code and a new command to certify. Which faults it may clear must be listed (a clock fault or a key CRC fault must not be clearable). An app path that clears a safety latch is the thing CLAUDE.md §4.2 is wary of |
| **C. Per-slot rules** | Some slots clear at a session start (as 0xFD does), others at power-on only | A table to decide slot by slot; overlaps `OI-SIMFW-33` |

**Recommendation: Q1 A (slot only, with both traps closed), Q2 A.** Reporting the slot costs little and is what the code
says it does. Power-on as the only clear is the behaviour that exists, is fail-closed, and needs no new path into a
Class C latch; the persistent latch the principal chose for `OI-SIMFW-27` (Q8 C) is a better home for anything that must
be cleared by a person. FMEA-M08-01 must be re-derived whichever is chosen, because the control it scores does not
exist.

**Two design inputs also bear on this.** `NP-DT-001` `DI-SAFE-02` ("fault latch requires explicit clear") and
`DI-SAFE-09` ("any safety cutoff latches the fault state; explicit app confirmation + repeat impedance check required
to re-enable") state a latch with an explicit clear for every safety cutoff. No explicit clear exists in the firmware
for any fault except the cardiac cutoff. How those two inputs should read is asked once, as Q9 of
`oi-simfw-27-interruption-policy.md` §8, and the answer here must agree with it.

## 3. What is known

Run in the simulator against the real firmware (`np_sim_safety_tests.c`, the warm-reset scenario): the facts in the
"as built" column. Read from the code, not run: the two traps under Q1 A. Not known: whether the hub or the app does
anything with the slot after a reset; how often an independent-watchdog reset happens on silicon (`OI-SWCI-49`).

## 4. What I need back

An option for Q1 and Q2, a short reason, and your name, role and date. Until then the firmware and its comments stay as
they are, and the scenario keeps recording the behaviour as built.
