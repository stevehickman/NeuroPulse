# Hub Control Program — Firmware Specification

**Project:** NeurOne
**Document:** NP-FW-HUB-001
**Revision:** 10
**Date:** 2026-09-25
**Status:** **DRAFT — pending approval** (`OI-FWHUB-06`). Issued as a design output under `21 CFR §820.30(d)`, which expects design outputs to be reviewed and approved before release; `Approved By` is blank, so this record does not claim a completed review (Rev 7 — it read RELEASED until then). **Written against the firmware that exists**, not ahead of it — see the banner below for what that means and what it does not.
**Effective Date:** 2026-09-23
**Author:** NeurOne Firmware Engineering
**Approved By:** — (pending design review; `FWHUB-DRC-01…12` in §10.3 are the review checklist)
**References:** CLAUDE.md §1 (product tiers), §3 (modality roster and its hard limits), §4.1 (processor stack), §4.2 (safety architecture), §4.6 (operating modes), §5 (UHDR/SHDR), §17 (firmware carries no locale key); `NP-SW-001` Rev 5 §3.2 (SW-02 Class B rationale), §5.2 (SW-02 module inventory), §9.4 (SOUP); `NP-HW-HUB-001` Rev 4 §7.2–§7.2.2 (`PBM_CRANIAL_EN#`, per-cluster gates, `HUB-REQ-C05`); `NP-FW-EMMC-001` Rev 2 §4 (partitions), §9 (Config/Calibration), §12 (session data classification); `NP-FW-EMMC-002` Rev 3 §C (UHDR key + mount), §F (Mode F); `NP-FW-CVNS-001` Rev 1 §6 (cardiac interlock); `NP-FW-NVRAM-001` Rev 2 §4 (power-loss atomicity), §9 (Class B argument); `NP-SOUP-LFS-001` Rev 1 (LittleFS SOUP record + hazard analysis); `NP-HEX-ZM-001` Rev 3 §7 (`OI-HUB-SOCKET-01`); `NP-HW-HEXTILE-001` §8.4 (D-8), §9.3 (`OI-HEXTILE-09`, the concurrent-power governor); `NP-SES-PWR-001` (the protocol power audit, `OI-SESPWR-03`); `NP-HW-HUB-001` Rev 4 §9.2 (socket-indexed HAL signatures), §9.3 (`np_hub_cluster_read_frame`); `NP-NPPS-REF-001` (`.npps` grammar, the compiler's input); `NP-MOD-ID-001` Rev 1; `NP-PRIV-REM-001` Rev 3 STEP-33 (adaptation events); `NP-CONV-001` Rev 6 §4, §5, §6, §8; `firmware/hub_control/`; `app/web/src/lib/hubCompiler.ts`
**Related Issues:** #339 (`OI-DOC-01`), #69 (original implementation), #179, #298
**Gate:** G2-14 (SW-02 design outputs documented) — this document is the missing half of that gate.
**IEC 62304 Class:** **SW-02 Class B.** Argued in §9. The Class C item is the separate safety MCU program (`firmware/safety_mcu/`, `NP-SW-001` SW-01); nothing in this document is Class C and no change here may add a bit to a Class C wire format without SW-01 review.
**Supersedes:** None — new document. It does **not** supersede `NP-FW-REQ-001` (already SUPERSEDED) and does not absorb `NP-FW-CVNS-001`, `NP-FW-EMMC-001/002` or `NP-FW-NVRAM-001`, which remain the governing documents for what they specify.
**Parent Document:** `NP-SW-001`

---

> **Rev 9 (2026-09-25) — the hub builds the three controls `OI-FMEA-09` names (GitHub #386).**
> `NP-FMEA-001` FMEA-M03-02 scores its residual against a hub check of delivered current against
> commanded current. The safety MCU cannot make that check, because its charge monitor integrates
> commanded current only. That check had never been built.
> - **Commanded-dose record** (§6.1, `REQ-FWHUB-39`): every dispatched command is written to UHDR
>   with its signed parameters and whether the registry accepted it. Until now the device log held
>   no protocol parameters at all, so the commanded dose could not be rebuilt from it.
> - **Commanded-versus-delivered cross-check** (§8.3.1, `REQ-FWHUB-41`): `src/np_stim_xcheck.c`
>   compares the BES/tACS and tDCS delivered-current read-back with the commanded current. It
>   raises one SHDR divergence flag per channel per session on a sustained excess, and it holds the
>   previous level through a tDCS ramp-down.
> - **Dirty-session marker** (§6.1, `REQ-FWHUB-40`): session start writes an SHDR `SESSION_OPEN`
>   record and syncs it, together with the UHDR start record, before anything is dispatched. An OPEN
>   with no END is a session that ended uncleanly.
>
> **Not closed by this revision.** The divergence thresholds are **unvalidated placeholders**; no
> threshold has been derived. The delivered-current read (`OI-STIM-06`) is a HAL stub with no
> hardware behind it. Cervical and auricular VNS have no delivered-current read to check. Those
> remain `OI-FMEA-09`, so FMEA-M03-02's residual stays conditional. New host target
> `np_stim_xcheck_tests`; `np_mod_stim_tests` and `np_log_backend_tests` gain cases.

> **Rev 8 (2026-09-25) — `OI-FWHUB-16` closed: an accessory attached after boot is registered
> again.** `task_module_detect` re-probed slots 7–18 through a rescan that refused every slot ≥ 5, and
> it discarded the return value. So goggles, the auricular VNS clip, the intranasal probe, cervical VNS
> and every T2 unit were seen only by the boot scan. The rescan is now `np_mod_reg_rescan_slot()`. It
> accepts slots 5–18, refuses the retired zone slots 0–4, and re-initialises a slot **only when its
> presence or type changes**. Widening the bound alone would have re-run `init()` twice a second on
> every attached module, writing an SHDR auth record each time from the intranasal and cervical VNS
> drivers (§3.2, D-30). The idle check now also covers LOADING, VERIFYING, PAUSED and STOPPING, and is
> re-read before every slot (§2.2, D-31). New `np_module_registry_tests` (Class B 34 → 35) fails
> 120 checks against the old registry. **What gets worse:** detection now actually probes, so
> `REQ-FWHUB-03`'s rule — no probe during a session — is live for the first time. It holds only by a
> state check, not by exclusion (`OI-FWHUB-17`).

> **Rev 7 (2026-09-25) — the wire format is diffed, the scan stops writing made-up SHDR records,
> the PBM stub takes a socket index, and `OI-FWHUB-04` is recorded closed (#384).** None of this
> changes whether transcranial PBM runs: it still does not, and `OI-FWHUB-09` still carries that
> (blocked on #335).
> - **`OI-FWHUB-03`:** new §4.6 tabulates every modality's code, parameter struct, byte count and
>   target. `scripts/check-hub-wire-format.ts` checks it, §4.1/§4.3/§4.5, `np_hub_config.h`,
>   `np_hub_types.h` and **the compiler's actual output**, decoded at the firmware's struct offsets,
>   against each other. It is falsified on 15 single perturbations spread across the three corners
>   (`REQ-FWHUB-28` is now met).
> - **`OI-FWHUB-04`:** closed. Rev 5 (#425) already made every path append and **then** sync
>   through `uhdr_drain()`/`shdr_drain()`, but left the item open in §13; Rev 7 records it.
> - **`OI-FWHUB-05`:** `np_mod_reg_scan()` no longer writes a "zone auth" record per retired zone slot.
>   Those records were classified from the ZONE_ID ladder that Rev 3 hardware does not carry, and the
>   stub reported five "pass" results on every boot. Nothing on the lattice is authenticated, so
>   there is no socket equivalent to write (§3.2).
> - **`OI-FWHUB-12`:** the PBM library's HAL stub is addressed over the 128-socket domain, so a smart
>   tile at socket 77 now completes its driver startup instead of failing `NP_PBM_ERR_I2C_WRITE`.
>
> `OI-FWHUB-06` is half done: the Status line above now says DRAFT, and the approval itself is still
> the Principal's. `OI-FWHUB-10` and `-11` stay open with the dependencies §13 names.
>
> **Rev 6 (2026-09-24) — `OI-FWHUB-15` closed: adaptation events queued at session end reach their
> session's file.** `np_log_session_end()` now drains the adaptation ring before it writes the
> session-end record and closes the file. Until now the runner's following `np_log_flush()` drained
> the ring into a closed file, and the events were lost without a trace. `np_log_backend_tests` gains
> 1 case, and removing the drain fails it (§6.5, §13).

> **Rev 5 (2026-09-24) — the session logger appends before it syncs, and keeps a UHDR session file
> in the order records were logged (§6.1, §6.5).** Two defects in `np_session_log.c` predate
> `OI-LFS-11` and were found while working on it. First, a full 4 KiB buffer was synced *before* it
> was appended, so the sync covered none of the bytes it followed. Second, EEG sample blocks went
> straight to the HAL ahead of records still buffered. Fixing the second exposed a third:
> `np_log_flush()` synced UHDR only when the buffer held something, so EEG data could wait past the
> 30 s bound. All three are fixed, and `np_log_backend_tests` gains 4 cases, falsified five ways.
> Raised: `OI-FWHUB-14`, because the EEG path's documented ISR context cannot share the logger's
> state, and `OI-FWHUB-15`, because adaptation events still queued at session end are lost.

> **Rev 3 (2026-09-23) — BES/tACS gets its own fail-closed geometry gate, and a latent defect in the
> area hand-off is fixed.** `NP-FW-MMSOCK-001` P-5 (principal, 2026-09-23) took Class C change C-1:
> `NP_SESSION_STATUS_GEOM_REQ_BES` (bit 4) and a third arm of `np_charge_monitor_geom_gate()`, so the
> safety MCU no longer applies its 25 cm² fallback to BES/tACS on trust (§5.4, §7.4, `REQ-FWHUB-36`).
> **Pending SW-01 review.** Writing it found that the runner sent the area frame **only** when a
> session held HD-tDCS or tDCS, so the fixed BES, VNS (0.5 cm²) and cervical-VNS (2 cm²) areas were
> computed and dropped in every other session and the MCU enforced 25 cm² instead — **50× looser than
> designed on the auricular clip** (`RISK-FWHUB-15`). The scan is now `src/np_chan_decl.c`, extracted
> from the ARM-only runner so it is host-tested (`np_chan_decl_tests`, Class B 31 → 32, total 39 → 40),
> and it sends every area it computes (`REQ-FWHUB-37`). No wire-format change: the BES pad area stays
> the fixed device constant `OI-CHARGE-07` made it, because T1 tES stays on pads (`NP-FW-MMSOCK-001` P-1).
>
> **Rev 2 (2026-09-23) — `OI-FWHUB-01` is closed: the socket-indexed dispatch registry exists, and
> transcranial PBM still does not run, now for exactly one named reason.**
>
> `src/np_socket_dispatch.c` (§3.4) is the missing half of `OI-HUB-SOCKET-01`. `dispatch_command()`
> now routes a `NP_PROTO_TARGET_SOCKET_MASK` command to it, and it checks placement against the live
> inventory, drives **every named socket or none**, owns the one `NP_SAFETY_EN_PBM_CRANIAL` bit, and
> keeps per-socket auto-stops. **What it will not do is admit a drive command today.** Opening the path
> made a gap that had been latent live: `NP-HW-HEXTILE-001` §9.3 requires a concurrent-power governor
> in the session runner, and `scripts/check-pbm-power.ts` finds **20 of 23** predefined transcranial
> protocols over the 40 W emitter budget, all of which compile clean. That governor cannot be written
> yet (`OI-HEXTILE-09`, blocked on `OI-SESPWR-03`), so `np_pbm_power_admit()` ships **refusing every
> load** — complete, and closed by default, like the `StudyDescriptorVerifier` of CLAUDE.md §6.3.
> Replacing that one function body is `OI-FWHUB-09`, which inherits the blocking status. Three other
> items the new path does not yet discharge are raised rather than absorbed: socket-path telemetry and
> dose metering (`OI-FWHUB-10`), the Class B per-cluster gate command `HUB-REQ-C05` asks of this
> processor (`OI-FWHUB-11`), and the five-slot bound in the PBM library's I²C stub (`OI-FWHUB-12`).
>
> **⚠ READ FIRST — this document was written last, and three things it found say so.**
>
> NP-FW-HUB-001 has been cited as the governing specification of `firmware/hub_control/` since
> 2026-05-16. It was registered in `NP-DHF-001` §5.4, named at the top of `firmware/CMakeLists.txt`
> and `app/web/src/lib/hubCompiler.ts`, and referenced from 48 files. **It had never been written.**
> The code was its own only description, and `hubCompiler.ts` — which must emit a wire format the hub
> accepts — had nothing to compile against but the C it was trying to match. That is `OI-DOC-01`,
> raised 2026-08-05, and this document closes it.
>
> **What this document is.** A design output describing `firmware/hub_control/` **as it is on `main`
> at 2026-09-13**, issued as a new record at Rev 1. Nothing is back-dated: the register's Rev 1 dated
> 2026-05-16 recorded a document that did not exist, and the honest repair is a Rev 1 dated today, not
> a fabricated history. Where the code embodies a decision, this document states the decision and its
> reason. **Where the code is incomplete, this document says so in the same voice** — a specification
> written after the fact earns its place by being falsifiable against the tree, and the fastest way to
> make it worthless would be to describe intent as though it were behaviour.
>
> **Four things did not survive being written down.**
>
> 1. **Cranial PBM — modality ① of CLAUDE.md §3, the product's primary optical modality — has no
>    dispatchable path in the shipped firmware at all.** Two independently-documented facts compose
>    into one nobody had stated. (a) `np_mod_pbm_*` is reachable **only** through slots 0–4
>    (`k_slot_probes`, `np_module_registry.c`), and those five slots are the retired zone-module
>    slots, which `np_protocol_verify_and_parse()` **rejects** — `slot_id < NP_HUB_SLOT_FIRST_VALID`
>    fails the blob. (b) The replacement addressing, `NP_PROTO_TARGET_SOCKET_MASK`, parses and
>    resolves correctly through `np_module_map`, and is then **dropped** by `dispatch_command()`,
>    which handles no target kind but `NP_PROTO_TARGET_SLOT`. Each half is a deliberate fail-closed
>    choice and each is individually recorded (the parser rejection in `np_hub_config.h`; the drop as
>    `OI-HUB-SOCKET-01`). **Their conjunction is not recorded anywhere, and it is that a transcranial
>    PBM protocol cannot execute.** Intranasal PBM (slot 9) is unaffected and works. §3.3, §5.6,
>    `OI-FWHUB-01` — the only **blocking** item this document raises. **Rev 2: the dispatch half is
>    resolved (§3.4); the blocking status moves to the power governor, `OI-FWHUB-09`.**
> 2. **Three source files cite a revision of this document that has never existed.**
>    `modules/np_mod_t2_stubs.c`, `src/np_adaptation_log.c` and `include/np_adaptation_log.h` head
>    their banners *"NP-FW-HUB-001 Rev 2"*. There was no Rev 1 to be a second revision of. The
>    citations are re-pointed in this change to Rev 1 with anchors that resolve (§8.9 and §6.4
>    respectively); nothing about the code changed. Recorded rather than fixed silently, because a
>    revision citation invented against an absent document is exactly the failure `OI-DOC-01` is
>    about, in miniature.
> 3. **`NP_HUB_PROTO_VERSION` is at 3 and the register said Rev 1 described it.** The wire format has
>    been revised twice since the register entry was written — v2 replaced the five-bit `slot_mask`
>    with `slot_id` plus a target block (80 sockets do not fit in five bits), v3 grew
>    `np_mod_tdcs_params_t` by the declared electrode area that `OI-CHARGE-04` requires. A register
>    entry naming a document that cannot be read cannot go stale visibly, which is the property that
>    makes the absence expensive rather than merely untidy. §4.5 is now the versioned record.
>
> 4. **Writing the bring-up table found a live ordering defect, and fixing it is part of this
>    change.** `np_mod_reg_scan()` was called with the SHDR zone-auth callback *before*
>    `np_log_init()`, so each record was stamped with `s_device_session_count` while it was still
>    **0** — the count is read later — and was then **discarded outright** when `np_log_init()` set
>    `s_shdr_pos = 0U`. Boot-time module authentication never reached SHDR, for two independent
>    reasons, under a comment three lines away asserting the log files open *"before the session
>    logger writes any record"*. Not a hazard — SHDR is device-condition fleet telemetry and nothing
>    on the device reads it back — and near-inert today because the zone slots those records describe
>    are themselves retired, but it would have bitten the moment the callback follows the socket
>    lattice. The logger now comes up before the scan (§2.1). The same pass found the banner
>    declaring *"four tasks"* where the code creates five — `task_protocol_rx` was omitted, and the
>    document register had inherited the undercount verbatim (§2.2).
>
>    **Both are now gated rather than merely fixed.** `np_hub_control_app_main()` is ARM-cross-only,
>    so no unit test can reach it and nothing had ever read its call order — which is how the defect
>    survived. `scripts/check-hub-bringup-order.ts` asserts the four ordering constraints and the
>    task count against the function itself, and was **falsified against the pre-fix commit**, not
>    only against fixtures: it reports exactly these two violations on it (`NP-CONV-001` §8).
>    `OI-FWHUB-07` and `OI-FWHUB-08`, both closed.
>
> **What is NOT claimed.** This document does not verify the firmware, does not close any `OI-*` item
> belonging to another document, and adds no requirement the code does not already meet — every
> `REQ-FWHUB-*` in §10 is traceable to a named file and line-level construct, and §10.2 marks the four
> that the code **does not** currently satisfy. A requirement invented here and not implemented would
> be a second `OI-DOC-01` pointing the other way.

---

## 1. Scope

**In scope:** the hub control program — the SW-02 FreeRTOS application that runs on the i.MX RT1062
main processor. Its task structure and bring-up order; the app↔hub transport framing; the signed
session-descriptor wire format and everything verification rejects; the module registry; the session
runner's execution model; the UHDR/SHDR session logger and its durability model; the safety-MCU SPI
interface and the cervical-VNS re-enable manager that rides on it; and the nine module drivers.

**Out of scope, and governed elsewhere:**

| Thing | Governing document |
|---|---|
| The safety MCU program itself (Class C) | `NP-SW-001` SW-01, `firmware/safety_mcu/` |
| eMMC partitioning, LittleFS instances, at-rest encryption, key custody | `NP-FW-EMMC-001`, `NP-FW-EMMC-002` |
| The socket lattice, zone membership, `np_module_map`'s inventory blob | `NP-HEX-ZM-001`, `NP-FW-NVRAM-001` |
| Cervical VNS safety library and its cardiac interlock | `NP-FW-CVNS-001` |
| The `.npps` source language the app compiles from | `NP-NPPS-REF-001` |
| Bootloader, OTA, signature-key provisioning | `NP-FW-OTA-001`, `firmware/bootloader/` |
| Whether a given protocol fits the optical power envelope | `NP-SES-PWR-001` |

**Boundary rule this document must not cross.** The hub is Class B and the safety MCU is Class C.
Every stimulation enable line is physically owned by the safety MCU (CLAUDE.md §4.2). Nothing
specified here may be argued to *substitute* for that ownership, and nothing here is a safety
control in its own right — the hub *requests*, the MCU *grants*. §9 turns that into the Class B
argument.

**Localization.** The hub renders no text. It carries no locale key and includes no locale file, per
CLAUDE.md §17 — the device speaks in LEDs, tones and numeric status, and the app does the wording. A
locale reference appearing under `firmware/hub_control/` means that boundary moved, which is a
decision and not a detail.

---

## 2. Program structure

### 2.1 Entry point and bring-up order

`np_hub_control_app_main()` (`src/np_hub_control_main.c`) is called by the post-bootloader
application startup code **after** clocks, eMMC mount and USB-C PD negotiation are complete. It is
not an `main()`; it never returns.

Bring-up order is load-bearing, and `REQ-FWHUB-01` binds the four constraints marked **binding**
below. This is the order the code runs.

| # | Call | Note |
|---|---|---|
| 1 | `np_safety_spi_init()` | configures SPI3 **and drives `GAIN_SEL[0..4]` LOW** before any probe (`OI-PBM-HW-01` sequencing) — **binding** |
| 2 | `np_cvns_reenable_init()` | the re-enable manager starts `IDLE` |
| 3 | `np_transport_init()` | creates the consumer wait primitive before any task can feed it |
| 4 | `np_log_backend_init()` | opens both partition log files — **binding** |
| 5 | `np_log_init(device_session_count)` | reads the SHDR device session count that stamps SHDR records, **and zeroes both log buffers** — **binding** |
| 6 | `np_mod_reg_init()` | zeroes the registry — **binding** |
| 7 | `np_mod_reg_scan()` | populates it; the intranasal and cervical VNS drivers write their SHDR auth records from `init()` (Rev 7: the scan's own per-zone-slot callback is removed, `OI-FWHUB-05`) |
| 8 | `xEventGroupCreate()` then `np_runner_init()` | the runner requires the event group to exist |
| 9 | five `xTaskCreate()` calls, then `vTaskStartScheduler()` | which does not return |

**Step 1 before step 7** is not stylistic. The PBM gain-select lines float at reset; probing a zone
slot with `GAIN_SEL` undriven reads an indeterminate transimpedance gain, and the detect result is
then a function of board leakage rather than of what is plugged in.

**Step 5 before step 7 is the constraint this section was written to find.** Until 2026-09-14 the
scan ran *first*, and its records were lost twice over: `np_log_shdr_zone_auth()` stamped each one
with `s_device_session_count` while it was still **0**, because the count is read at step 5, and then
`np_log_init()` set `s_shdr_pos = 0U` and discarded the buffer they had been written into. Boot-time
module authentication reached SHDR not at all — under a source comment, three lines below the scan,
asserting that the log files open *"before the session logger writes any record"*. `OI-FWHUB-07`,
fixed in the same change that issued this document. *(Rev 7: the zone-slot records themselves are
gone, `OI-FWHUB-05`, §3.2. The constraint still binds, because the accessory auth records written
during the scan need the same count and the same buffer.)*

**The order is gated, because nothing could test it.** `np_hub_control_app_main()` is ARM-cross-only
— `firmware/hub_control/src/` is in `HUB_SOURCES`, which compiles under the arm-none-eabi toolchain
and in no host test — so no unit test can reach this sequence, and that is precisely why the defect
survived from 2026-05-16. `scripts/check-hub-bringup-order.ts` parses the function and asserts all
four constraints plus §2.2's task count. Per `NP-CONV-001` §8 it was falsified before it was trusted,
and against the **pre-fix commit** rather than only against fixtures: it reports exactly the two
violations this change repairs.

### 2.2 The four tasks

**There are five, not four.** The source banner and the document register both said "four tasks"
from 2026-05-16 to 2026-09-14; `task_protocol_rx` is the fifth, and it is the one that blocks on the
transport. Corrected in all three places, and the banner's declared count is now **counted against
`xTaskCreate()` by `scripts/check-hub-bringup-order.ts`** rather than read — a miscount in the one
comment a reader starts from is cheap to make and invisible to every other check. `OI-FWHUB-08`.

| Task | Priority | Stack (words) | Duty |
|---|---|---|---|
| `task_safety_heartbeat` | 4 (`NP_HUB_TASK_PRIO_HEARTBEAT`) | 256 | one SPI heartbeat every `NP_SAFETY_HEARTBEAT_MS` (200 ms); mirrors the cVNS re-enable bit; hands the granted mask, MCU status and MCU impedance report to `np_mod_cvns`; posts `NP_EV_SAFETY_FAULT` on a fault reply |
| `task_hub_control` | 3 (`..._PRIO_CONTROL`) | 1024 | waits on `NP_EV_SESSION_START`, then `np_runner_run()` until the session completes or aborts |
| `task_protocol_rx` | 2 (`..._PRIO_CONTROL - 1`) | 1024 | blocks in `np_hal_proto_queue_receive()` on `portMAX_DELAY`; on a blob calls `np_runner_load()` (which verifies the signature and posts `NP_EV_SESSION_START`), then **zeroes the receive buffer** so no plaintext protocol is left in RAM |
| `task_telemetry` | 2 | 512 | `np_log_flush()` on the `NP_LOG_UHDR_FLUSH_MS` / `NP_LOG_SHDR_FLUSH_MS` intervals, so the runner never blocks on eMMC |
| `task_module_detect` | 1 | 512 | every `NP_DETECT_ACCESSORY_POLL_MS` (500 ms), re-probes accessory slots 7–18 with `np_mod_reg_rescan_slot()` (§3.2) while the runner is `IDLE`, `COMPLETE` or `FAULT`. The state is re-read before each slot, and the pass stops as soon as a session is in flight. **(Rev 8.** It used to skip only `RUNNING`, and every rescan it issued was refused: `OI-FWHUB-16`.) |

Receiving and running are **separate tasks on purpose**: `np_runner_run()` blocks for the whole
session, so a single task doing both could not accept the next protocol until the current one
finished, and the transport's single-slot mailbox (§2.3) would then backpressure for the session's
duration rather than for the handoff.

`REQ-FWHUB-02`: the heartbeat task holds the **highest** priority in the program and must never
block for longer than one heartbeat period. Its deadline is not a performance target — missing it
for `NP_SAFETY_WATCHDOG_MS` (1500 ms) is what makes the safety MCU cut every stimulation channel.
The priority ordering below it is a consequence: telemetry and detection exist to be preempted.

`REQ-FWHUB-03`: `task_module_detect` must not run a probe concurrently with a session. Probing
drives `GAIN_SEL` and reads impedance on lines a running session owns. **Rev 8: this is not fully
met** (§10.2). The task is the lowest priority, so a session can start between its state check and
a probe. Re-reading the state before each slot narrows the window to one `detect()` call; it does
not close it. Until Rev 8 the rule held only because every accessory rescan was refused.
`OI-FWHUB-17`.

### 2.3 Transport framing

Both transports — BLE 5.3 LE GATT characteristic writes and USB-C CDC-ACM bulk — deliver the same
framing, and `np_transport.c` is the single reassembler (`REQ-FWHUB-04`):

```
┌──────────────────────────┬────────────────────────────────────┐
│ length prefix (4 B, LE)  │ protocol blob (`length` bytes)      │
└──────────────────────────┴────────────────────────────────────┘
```

`length` is the exact byte count of the signed blob that follows and must lie in
`[1, NP_HUB_PROTO_BLOB_MAX]`. A malformed prefix discards the partial frame and restarts
reassembly; it never advances a partial parse.

**One blob at a time, by construction.** A single-slot mailbox holds one completed blob. While it is
occupied, `np_transport_feed()` refuses new bytes with `NP_HUB_ERR_SESSION_ACTIVE`. This is the
backpressure mechanism for a client that pipelines protocols, and it is preferred to a queue for a
reason worth stating: a queue of session descriptors is a queue of things that could each start
stimulation, and the depth of that queue would become a safety parameter.

`np_transport_feed()` runs in **task** context — the BLE host-stack callback task or the CDC RX task
— never a raw ISR, because the mailbox handoff is guarded by a task-level critical section. Both
common stack families already defer received data to a task, so this holds; a port that does not
must add the deferral rather than relax the guard (`REQ-FWHUB-05`).

### 2.4 Fault philosophy

**The hub is not the last line of defence, and must never be written as though it were.**

The safety MCU independently cuts all stimulation when the 200 ms heartbeat stops for
`NP_SAFETY_WATCHDOG_MS`, in under 50 ms (CLAUDE.md §4.2). Therefore the correct hub response to an
unrecoverable internal fault — a FreeRTOS stack-overflow hook, a malloc-failed hook, an assertion,
a platform trap — is to **stop heartbeating and halt**, not to attempt recovery, and not to keep
running degraded. `src/np_hub_freertos_hooks.c` and `firmware/platform/include/np_platform_trap.h`
implement exactly that, and `REQ-FWHUB-06` binds it: **no hub fault path may contrive to keep the
heartbeat alive.** A hub that keeps beating while its own state is unsound is a hub that keeps
stimulation enabled while its own state is unsound.

Recoverable faults are handled without halting: a module `control()` returning non-`NP_HUB_OK`
records `NP_ABORT_MOD_FAULT` and an SHDR fault record, and the **session continues** — the safety
MCU owns the hard cutoff, and aborting a whole session on one module's transient would trade a
recoverable outcome for a worse one.

---

## 3. Module registry

### 3.1 The slot table

The registry (`src/np_module_registry.c`) is a flat array of `np_mod_entry_t`, one per slot,
`NP_HUB_SLOT_MAX` = 19 entries. Each present entry carries the module type and four function
pointers: `init`, `control`, `telemetry`, `shutdown`.

| Slot | Name | Status | Driver |
|---|---|---|---|
| 0–4 | `NP_HUB_SLOT_ZONE_0..4` | **RETIRED** — see §3.3 | `np_mod_pbm` |
| 5 | `EEG` | fixed hardware, always present | `np_mod_eeg` (§8.2) |
| 6 | `AUDIO` | fixed hardware | `np_mod_audio` (§8.5) |
| 7 | `VISUAL` | Hall + IR detected | `np_mod_visual` (§8.6) |
| 8 | `VNS_HRV` | accessory-port impedance | `np_mod_vns` (§8.4) |
| 9 | `INTRANASAL` | optical code + pogo | `np_mod_intranasal` (§8.7) |
| 10 | `CVNS` | accessory port (T2) | `np_mod_cvns` (§8.8) |
| 11–16 | `QEEG`, `TMS`, `PBM_1170NM`, `CLIN_TACS`, `HD_TDCS`, `VIBROTACTILE` | **stubs** | `np_mod_t2_stubs` (§8.9) |
| 17 | `BES_TACS` | shares the stim driver | `np_mod_stim` (§8.3) |
| 18 | `TDCS` | shares the stim driver | `np_mod_stim` (§8.3) |

**Why slots 0–4 are retained though retired.** Deleting them would shift every slot number above
them, and a slot number is not just an array index: it is the bit position of the corresponding
`NP_SAFETY_EN_*` enable, and enable-bit position **is** the safety MCU's charge-monitor channel index
into `current_ua[]` and `s_charge_nc[]`. That index is Class C. A renumbering on the Class B side
would silently re-aim a Class C charge accumulator. `REQ-FWHUB-07`: **slot numbers are append-only
and must never be reused or compacted.** The same rule governs enable bits 1–4, which are reserved
and not reused, and which the safety MCU strips via `NP_SAFETY_EN_ALL_MASK` as defence in depth
against a future authoring error.

**Why `BES_TACS` and `TDCS` are two slots on one driver.** They share the stimulation DAC and the
driver file, but the safety MCU gates them on **separate** enable bits and they carry different
parameter structs. One slot would have made the gate coarser than the hardware.

### 3.2 Probe, scan and rescan

`np_mod_reg_scan()` walks `k_slot_probes[]`, calls each slot's `detect()`, and on
`NP_HUB_OK` installs the four control pointers and calls `init()`. A slot may resolve to more than
one type — the PBM entry serves both `NP_MOD_PBM_BASE` and `NP_MOD_PBM_SMART` — so `detect()` returns
the type it actually found and the registry stores it.

**The scan writes no SHDR record of its own** (Rev 7, `OI-FWHUB-05`). Until Rev 7 every zone-slot
probe emitted a "zone auth" record through an `np_mod_reg_shdr_cb_t` callback. Those slots are
retired, and on Rev 3 hardware their probe reads a ZONE_ID resistor ladder that does not exist
(`np_pbm_hal_adc_read_zone_id()`, *"retained only to keep the host-test build linking"*). The stub
classified all five as present, so every boot sent SHDR five authentication **passes** for modules
nobody plugged in. The callback and its parameter are removed rather than left pointing at nothing.

**Nothing follows the socket lattice instead, because nothing on the lattice is authenticated.**
A tile is identified by its UID in the `np_module_map` inventory, not by a pass/fail challenge, so
there is no result to log. A per-socket inventory record in SHDR would be a **new SHDR field**, and
that is a CLAUDE.md §5.1 classification question (which socket a tile sits in can show how the
headset is fitted), not a logging detail. It belongs with the other socket-path telemetry in
`OI-FWHUB-10`. Accessories that do authenticate, the intranasal probe and the cervical VNS cuff,
write `np_log_shdr_zone_auth()` from their own `init()`.

`np_mod_reg_rescan_slot()` re-probes one slot without a full rescan. `task_module_detect` calls it
for slots 7–18 while idle (§2.2). **(Rev 8, `OI-FWHUB-16`.** It was `np_mod_reg_rescan_zone()`, which
refused every slot ≥ `NP_HUB_ZONE_SLOT_COUNT`. The detect task discarded the refusal, so no
accessory attached after boot was ever registered.) The contract:

| Detect result vs. registry entry | Action | Returns |
|---|---|---|
| slot 0–4, or ≥ `NP_HUB_SLOT_MAX` | nothing — `detect()` is not called | `INVALID_ARG` |
| unchanged, absent | nothing | `NOT_PRESENT` |
| unchanged, present | nothing beyond `detect()` | `OK`, or `MOD_INIT` if its `init()` had failed |
| newly present | register; call `init()` | `OK` / `MOD_INIT` |
| removed | `shutdown()` if it was initialised; deregister | `NOT_PRESENT` |
| type changed | `shutdown()` the old occupant; `init()` the new one | `OK` / `MOD_INIT` |

**Why only on a change (D-30).** A rescan that re-ran `init()` every poll would write an SHDR auth
record twice a second for as long as the intranasal probe or cervical VNS unit stayed attached, since
both drivers write one from `init()` (§8.7, §8.8). It would also re-run ADS1299 self-calibration and
zero both stimulation channels' state (`np_mod_stim_init()` clears BES and tDCS together). **Only
`detect()` repeats** on an unchanged slot. A failed `init()` is **not retried** until the module is
removed and re-seated, which bounds the auth records a faulty accessory can write to one per
insertion. The entry fails closed meanwhile, because `np_mod_reg_get()` requires `initialized`.

**The fixed slots are harmless to rescan.** EEG, audio, BES/tACS and tDCS always report present, so
a rescan of any of them calls only `detect()`. The loop still starts at `VISUAL`, so EEG (an ADS1299
ID read) and audio are not polled. `BES_TACS` and `TDCS` fall inside the loop and are no-ops.

**The retired zone slots are refused.** The parser rejects them as targets, so a re-probe could only
re-initialise a driver nothing can address. The boot scan still probes them; since Rev 7 it writes
no SHDR record for them (`OI-FWHUB-05`). `np_hub_zone_insert_cb()` used to be the only caller in 0–4. It
is now a deliberate no-op: `np_za_init()` has no caller in the hub, so nothing registers it
(`OI-FWHUB-18`).

**Write order on a change.** `initialized` is cleared before any other field and set only after
`init()` succeeds, so a reader in `task_hub_control` gets `NULL`, not a half-written entry. That is
an ordering argument, not a lock, and it is part of `OI-FWHUB-17`.

Pinned by `np_module_registry_tests`, which links the production registry against per-slot driver
doubles: hot-plug on every accessory slot, 100 unchanged polls → one `init()`, removal → one
`shutdown()`, type change, failed-init no-retry, fixed-slot no-op, retired-slot refusal. Built with
`-DRESCAN_FN=np_mod_reg_rescan_zone` against the Rev 7 registry, it fails 120 checks.

### 3.3 The two registries that have never been joined

**This is the root of `OI-FWHUB-01` and it is worth stating structurally rather than as a symptom.**

There are two module inventories in this program and they do not know about each other:

| | `np_module_registry` (§3) | `np_module_map` (`NP-FW-NVRAM-001`) |
|---|---|---|
| Indexed by | **slot**, 0–18, fixed at compile time | **socket**, a UID auto-inventory over the 80-socket lattice |
| Holds | four function pointers per slot | `(socket:element)` addresses, element types, calibration |
| Populated by | `detect()` probes at boot | UID enumeration, persisted as the `"NPMP"` blob |
| Reachable from a command | yes — `dispatch_command()` calls `control()` | **no — nothing dispatches through it** |

The socket lattice replaced the five zone-module FPC slots as the cranial addressing scheme. The
**wire format** followed it (§4.3: `NP_PROTO_TARGET_SOCKET_MASK`), the **parser** followed it
(`np_protocol_socket_expand()` emits exactly the list `np_group_query_t` wants), and the **map**
followed it. The **dispatch path did not.** `dispatch_command()` still resolves a command through
`np_mod_reg_get(cmd->slot_id)`, and there are no socket-indexed entries to resolve.

So the PBM driver sits in slots the parser refuses, and the addressing the parser accepts reaches no
driver. Both halves fail closed, correctly and deliberately — the alternative to the drop would be
delivering an eleven-socket frontal-left command to whatever occupies slot 0, and a wrong-site dose
is not recoverable where a missed one is. What is missing is not a guard; it is the socket-indexed
dispatch registry itself. §5.6 states the consequence; `OI-FWHUB-01` owns it.

**Rev 2: joined — §3.4.** The table above still describes the two inventories; what changed is that
the right-hand column is now reachable from a command, through a third structure that sits beside
`np_module_registry` rather than inside it.

### 3.4 The socket dispatch registry (Rev 2, `OI-FWHUB-01`)

`src/np_socket_dispatch.c` holds one record per socket in the full 7-bit domain
(`NP_HEXMAP_MAX_SOCKETS`, 128) — active, module type, the params it was driven with, and its
auto-stop time. It has no FreeRTOS dependency and is host-tested by `np_socket_dispatch_tests`.
`dispatch_command()` sends every socket-addressed command here and every slot-addressed command to
`np_module_registry`; **neither falls through to the other** (`REQ-FWHUB-31`).

**Admission — every step all-or-nothing, in this order:**

| # | Gate | Refusal | Why it is all-or-nothing |
|---|---|---|---|
| 1 | `mod_type` is `NP_MOD_PBM_BASE` or `NP_MOD_PBM_SMART` | `INVALID_ARG` | nothing else occupies a lattice socket as a *driven* emitter; tES and EEG sockets are not dispatched here (`REQ-FWHUB-33`) |
| 2 | the mask selects ≥ 1 socket | `INVALID_ARG` | — |
| 3 | **a stop (`params_len == 0`) is admitted here, unconditionally** | never | stopping is always safe; it is never gated on placement or power |
| 4 | `params_len` is exactly the struct its `mod_type` defines | `INVALID_ARG` | a short block is read past its end; a long one means producer and firmware disagree about the struct |
| 5 | the params light ≥ 1 emitter, and a smart `ch_mask` names no channel above bit 2 | `INVALID_ARG` | a drive that lights nothing would still enter UHDR as a delivered modality |
| 6 | **placement:** every socket holds every emitter the params light (`np_module_map_check_placement`, one requirement per socket × emitter) | `NOT_PRESENT` | driving the capable subset would dose fewer sites than the protocol named while UHDR recorded the protocol — the same rule `np_pbm_session_desc_expand()` states: *a missed dose is recoverable, a partially-honoured session is not* |
| 7 | **power:** `np_pbm_power_admit(cmd, current)` | `POWER_BUDGET` | §5.6 — the production definition refuses everything |
| 8 | drive each socket in ascending order; on any driver fault, stop every socket this command touched | `MOD_FAULT` | the rollback is what makes step 8 all-or-nothing too (`REQ-FWHUB-32`) |

A socket already driven by the *other* module type is stopped before it is re-driven — base and smart
tiles are reached through different hardware paths, and reprogramming one does not quiesce the other.
A socket re-driven with the same type is simply reprogrammed, and its stop time overwritten, which is
the slot path's semantics.

**The cranial enable is owned here, and is reference-counted by active sockets** (`REQ-FWHUB-34`).
`NP_SAFETY_EN_PBM_CRANIAL` is one bit for the whole lattice (`NP-HW-HUB-001` §7.2). It is requested
only after every socket of a command is configured, and released only when **no** socket remains
active — so one command's stop cannot cut the gate under another command's running sockets, which
would leave UHDR recording as delivered a dose the safety MCU had cut. **The exception is a stop that
fails:** firmware has then lost control of an emitter, so every socket is stopped and the bit released
at once, and the safety MCU's whole-lattice cut is what guarantees the emitter goes dark. (The slot
path's `np_mod_pbm_control()` drops the bit on every stop; that was only ever safe because the slot
path is unreachable.)

**The driver seam** is two functions added to `modules/np_mod_pbm.c` (registry idiom, declared in
`np_socket_dispatch.h`): `np_mod_pbm_socket_drive()` and `np_mod_pbm_socket_stop()`. Smart (T1-C)
tiles use the same `np_pbm_drive` register sequence as the slot path with the socket index as the
address, which the HAL tunnels through the socket's cluster controller (`NP-HW-HUB-001` §9.2). Base
(T1-A) tiles have no on-module MCU; their PWM comes from the cluster controller through a new platform
seam, `np_mod_pbm_hal_socket_pwm_set()` (`OI-PBM-HAL-04`; SW-02 census 97 → 98). Neither touches the
safety enable.

**What this registry does not do yet** — each raised, none silently absorbed: per-socket telemetry and
dose metering (`OI-FWHUB-10`), commanding the per-cluster Class B gate (`OI-FWHUB-11`), and anything
on hardware at all, since every seam beneath it is a platform trap.

---

## 4. Session descriptor wire format

This section is the specification `app/web/src/lib/hubCompiler.ts` compiles against. Where the two
disagree, **this section and `np_protocol.c` are the same artifact seen twice**, and the compiler is
wrong (`REQ-FWHUB-08`).

### 4.1 Blob layout

```
┌────────────────────┬──────────────────────────────┬──────────────────────┐
│ np_proto_header_t  │ command body (cmd_count cmds)│ Ed25519 sig (64 B)   │
└────────────────────┴──────────────────────────────┴──────────────────────┘
                     └──────── signed region ───────┘
  signature covers buf[0 .. len - NP_HUB_PROTO_SIG_LEN - 1]
```

Each command in the body is:

```
[np_proto_cmd_hdr_t] [target_len bytes] [params_len bytes]
```

Constants (`np_hub_config.h`): magic `NP_HUB_PROTO_MAGIC` = `0x4E504850` (`"NPHP"`); version
`NP_HUB_PROTO_VERSION` = 3; UUID 16 B; serial 32 B ASCII; signature 64 B; at most
`NP_HUB_PROTO_CMD_MAX` = 64 commands; at most `NP_HUB_PROTO_PARAMS_MAX` = 64 params bytes per
command; target block at most `NP_HUB_PROTO_TARGET_MAX` = 16 B.

### 4.2 Header

The header carries the magic, the version, the 16-byte session UUID (which is the UHDR key for this
session's records), the 32-byte device serial, `compiled_at_unix`, and `cmd_count`.

**The serial is a replay guard, not an identifier.** A descriptor signed for one device is refused
by every other (`NP_HUB_ERR_WRONG_DEVICE`). Without it, a validly-signed protocol captured from one
headset would run on any headset.

### 4.3 The target block

`target_kind` (`np_proto_target_kind_t`) selects how the variable-length target block is read.

| Kind | `target_len` | `slot_id` | Meaning |
|---|---|---|---|
| `NP_PROTO_TARGET_SLOT` | 0 | a valid slot in `[5, 18]` | dispatch to that fixed slot |
| `NP_PROTO_TARGET_SOCKET_MASK` | `NP_HUB_SOCKET_MASK_BYTES` = 16 | `NP_HUB_SLOT_NONE` (`0xFF`) | a 128-bit socket bitmap |

Three properties of the socket bitmap are decisions, not encodings, and §4.3 is where they are of
record:

1. **A bitmap, not an address list, because zone membership is inclusive.** A socket may belong to
   more than one zone — the ten midline sockets are in *both* hemisphere zones of their lobe — so a
   protocol naming "Frontal Left" and "Frontal Right" names sockets 1, 5 and 13 twice. In a list
   that is two entries and **two drives of the same emitter: double J/cm² on a real PBM protocol**.
   In a bitmap the duplicate is not expressible. The dedup guarantee stops being something every
   producer must remember.
2. **Sized to 128 bits, not 80.** 128 is the full 7-bit socket domain (`NP_HEXMAP_MAX_SOCKETS`). The
   wire format must not need a revision when a later shell wires more of the domain it already
   addresses.
3. **Bit position is index space, not a socket number.** Bit 0 selects socket 1. Socket numbers are
   1-based project-wide (`docs/np_hex_zm_001.md` §3.3); this bitmap is the exemption, and the
   exemption is what makes the sizing work — mapping sockets 1…128 onto bits 1…128 needs a 129th bit,
   i.e. a 17th byte, or a lattice capped at 127. Producers convert once
   (`hubCompiler.socketBitmap`, `NPSocketMask`); consumers add the base back before a socket number
   is displayed, logged or transmitted. **Nothing in `np_protocol.c` performs that conversion**, by
   design — one conversion site, at the edge.

`slot_id` is a plain index and not v1's `slot_mask` bitmask. The mask could not name slot 8 or above
at all, which made the VNS clip, the intranasal probe, cervical VNS and every T2 unit unreachable;
and its zero value invited the `0x01` catch-all that sent every modality to the PBM driver in slot 0.
Each remaining slot is a distinct device with its own parameter struct, so a command has exactly one
slot or none, and a plain index says that.

### 4.4 What verification rejects

`np_protocol_verify_and_parse()` verifies the Ed25519 signature **before parsing any command**, using
the same RFC 8032 verifier as the bootloader (`np_signature.c`), then rejects a blob for any of:

| Check | Error | Why it is a check and not a tolerance |
|---|---|---|
| magic / version | `BAD_MAGIC` / `BAD_VERSION` | §4.5 |
| signature | `BAD_SIGNATURE` | the only thing that makes a descriptor trustworthy |
| serial ≠ device serial | `WRONG_DEVICE` | replay across devices |
| `cmd_count` > 64 | `CMD_TOO_MANY` | fixed `.bss` descriptor |
| `params_len` > 64 | `PARAMS_TOO_LONG` | fixed per-command storage |
| unknown `target_kind`, or `target_len` ≠ that kind's exact length | `INVALID_ARG` | a variable-length field whose length is not pinned is a parser gadget |
| `SLOT` target with `slot_id` outside `[5, 18]` | `INVALID_ARG` | **a v1 zone target decodes to slot 0–4; this is what stops it reaching a driver** |
| non-`SLOT` target with `slot_id` ≠ `0xFF` | `INVALID_ARG` | a producer confusion, caught at the edge |
| `start_ms + duration_ms` wraps `uint32` | `INVALID_ARG` | a wrapped auto-stop deadline lands on the runner's "no stop pending" sentinel 0 — the stop is then **never issued** |
| command body not **exactly** consumed by `cmd_count` commands | `INVALID_ARG` | trailing bytes mean `cmd_count` under-counts, silently dropping the tail — **and the tail of an interval protocol is its STOP commands** |

The last two rows are the ones to keep. Both are cases where a lenient parser produces a session that
starts stimulation it never stops, and in both the lenient reading looks like robustness.

`REQ-FWHUB-09`: **any failure leaves `*desc_out` undefined and the caller must not use it.** The
parser does not partially populate.

### 4.5 Versioning

| Version | Change | Why bumped rather than absorbed |
|---|---|---|
| 1 | original; `slot_mask` byte addressing | — |
| 2 | `slot_id` + variable target block; socket-mask targeting | 80 sockets do not fit in five bits, and are not slots |
| **3** | `np_mod_tdcs_params_t` grew `electrode_area_mcm2` (6 → 8 B) — `OI-CHARGE-04` | a v2 descriptor's tDCS block is a *different length* for the same modality code. Left at v2, the only symptom would be `NP_HUB_ERR_INVALID_ARG` out of the stim handler at dispatch, which reads as a **corrupt** descriptor. `NP_HUB_ERR_BAD_VERSION` at header verification says what actually happened. |

`REQ-FWHUB-10`: **any change to a per-modality parameter struct's length bumps
`NP_HUB_PROTO_VERSION`.** The rule is length, not semantics — a struct that changes meaning at the
same length is caught by the signature only if the app also changed, which is not a guarantee.

### 4.6 Per-modality parameter blocks *(Rev 7, `OI-FWHUB-03`)*

A command's `params_len` is **exactly** its modality's packed struct size, and the drivers check it
exactly: a longer block is refused, and a shorter one would leave fields unread. `params_len` 0 is
the stop command. Each modality has exactly one target form.

| Code | `mod_type` | Parameter struct (`np_hub_types.h`) | Bytes | Target |
|---|---|---|---|---|
| `0x01` | `NP_MOD_PBM_BASE` | `np_mod_pbm_base_params_t` | 4 | socket mask |
| `0x02` | `NP_MOD_PBM_SMART` | `np_mod_pbm_smart_params_t` | 6 | socket mask |
| `0x03` | `NP_MOD_INTRANASAL` | `np_mod_intranasal_params_t` | 5 | `NP_HUB_SLOT_INTRANASAL` (9) |
| `0x04` | `NP_MOD_EEG` | `np_mod_eeg_params_t` | 5 | `NP_HUB_SLOT_EEG` (5) |
| `0x05` | `NP_MOD_BES_TACS` | `np_mod_bes_tacs_params_t` | 7 | `NP_HUB_SLOT_BES_TACS` (17) |
| `0x06` | `NP_MOD_TDCS` | `np_mod_tdcs_params_t` | 8 | `NP_HUB_SLOT_TDCS` (18) |
| `0x07` | `NP_MOD_VNS_HRV` | `np_mod_vns_hrv_params_t` | 9 | `NP_HUB_SLOT_VNS_HRV` (8) |
| `0x08` | `NP_MOD_AUDIO` | `np_mod_audio_params_t` | 8 | `NP_HUB_SLOT_AUDIO` (6) |
| `0x09` | `NP_MOD_VISUAL` | `np_mod_visual_params_t` | 9 | `NP_HUB_SLOT_VISUAL` (7) |
| `0x0A` | `NP_MOD_CVNS` | `np_mod_cvns_params_t` | 10 | `NP_HUB_SLOT_CVNS` (10) |
| `0x0B` | `NP_MOD_QEEG_21CH` | `np_mod_qeeg_21ch_params_t` | 8 | `NP_HUB_SLOT_QEEG` (11) |
| `0x0C` | `NP_MOD_TMS` | `np_mod_tms_params_t` | 10 | `NP_HUB_SLOT_TMS` (12) |
| `0x0D` | `NP_MOD_PBM_1170NM` | `np_mod_pbm_1170nm_params_t` | 5 | `NP_HUB_SLOT_PBM_1170NM` (13) |
| `0x0E` | `NP_MOD_CLIN_TACS` | `np_mod_clin_tacs_params_t` | 8 | `NP_HUB_SLOT_CLIN_TACS` (14) |
| `0x0F` | `NP_MOD_HD_TDCS` | `np_mod_hd_tdcs_params_t` | 6 | `NP_HUB_SLOT_HD_TDCS` (15) |
| `0x10` | `NP_MOD_VIBROTACTILE` | `np_mod_vibrotactile_params_t` | 4 | `NP_HUB_SLOT_VIBROTACTILE` (16) |

**This table, `np_hub_types.h` and `hubCompiler.ts` are diffed, not read.** `scripts/check-hub-wire-format.ts`
checks three sources against each other. The first is this section's constants (§4.1), target kinds (§4.3),
current version (§4.5) and this table. The second is the firmware: the `np_hub_config.h` defines, the
`np_hub_types.h` enums, and every packed struct's size and field offsets computed from its declaration.
The third is **the compiler's output**: the check runs `compileProtocol()` for each modality and decodes
the blob using the firmware's field offsets, so what is compared is what the compiler actually writes.
Its self-test perturbs one corner at a time and must fail on each (`NP-CONV-001` §8). **A change to
any of the three without the other two fails CI.** `REQ-FWHUB-10` still applies: a length change in
this table bumps §4.5's version.

---

## 5. Session runner

### 5.1 States

`np_session_state_t`: `IDLE` → `LOADING` → `VERIFYING` → `RUNNING` → `STOPPING` →
`COMPLETE` | `FAULT`, plus `PAUSED`.

These are **enum values and never bit flags.** `np_safety_session_status_bits()` exists because
writing a state enum into the heartbeat's `session_status` byte was a live bug: `NP_SESSION_RUNNING`
(= 3) asserts `ACTIVE|CVNS_REENABLE`, and `NP_SESSION_PAUSED` (= 4) asserts `GEOM_REQUIRED`.
`REQ-FWHUB-11`: **the `session_status` byte is only ever built by
`np_safety_session_status_bits()`.**

That mapping carries one non-obvious rule. `ACTIVE` is asserted for `RUNNING`, `PAUSED` **and**
`STOPPING`. The safety MCU resets per-session state — session signature, charge accumulator,
impedance checks — on the `ACTIVE` 0→1 transition. If `PAUSED` dropped `ACTIVE`, a pause/resume cycle
would **zero the charge accumulator mid-session**, which is charge-limit circumvention by an
ordinary user action. `STOPPING` stays active through ramp-down.

### 5.2 Load

`np_runner_load()` refuses while `RUNNING` or `STOPPING` (`NP_HUB_ERR_SESSION_ACTIVE`), then parses,
sorts by `start_ms` (`np_protocol_sort_cmds()`), and posts `NP_EV_SESSION_START`. The descriptor
lives in the statically-allocated `np_runner_ctx_t` in `.bss` — which the application linker script
places in the OCRAM2 staging reservation. **There is no external SDRAM on this device**
(`OI-SWCI-46`), and three comments in this codebase once described a placement the compiler was never
asked for.

### 5.3 Run loop

```
reset cVNS re-enable manager   ← no cutoff/confirm state may cross a session boundary
state := RUNNING; start_tick := now
write UHDR session-start + SHDR SESSION_OPEN, both synced   ← §6.1, Rev 9
reset the commanded-vs-delivered cross-check                ← §8.3.1, Rev 9
send per-channel electrode geometry to the safety MCU   ← §5.4
loop:
    if NP_EV_SESSION_ABORT | NP_EV_SAFETY_FAULT  → break
    dispatch every command with start_ms ≤ elapsed_ms   ← slot → registry; socket → §3.4
        and write each to UHDR as a commanded-dose record (accepted or refused)   ← Rev 9
    process_stops(elapsed_ms); np_sock_disp_process_stops(elapsed_ms)
    every NP_RUNNER_TELEM_INTERVAL_MS: telemetry() each present slot → np_log_telemetry()
        → np_stim_xcheck_observe(); on a new divergence, one SHDR flag   ← §8.3.1, Rev 9
    if a cVNS command is active: np_mod_cvns_tick(hal_now_ms, hal_now_unix)
    sleep ms_until_next_event(), clamped to ≥ NP_RUNNER_TICK_MS (5 ms)
                                 and to ≤ NP_CVNS_STIM_TICK_MS while cVNS is active
```

Each dispatched command registers an auto-stop at `start_ms + duration_ms` in `stop_at_ms[slot]`; a
later command for the same slot **overwrites** the stop time rather than stacking. `process_stops()`
calls `control(slot, NULL, 0)` — the stop convention — and de-requests that slot's safety enable bit.

Two details that are decisions:

- **`stop_at_ms[slot] == 0` is the "no stop pending" sentinel**, which is why §4.4 rejects a wrapping
  `start_ms + duration_ms` at the parser. The guard is at the edge because by the time the runner
  holds the value, 0 is indistinguishable from "none".
- **The cVNS tick uses the CVNS HAL clocks, not the runner's session-relative clock.** The
  Pan-Tompkins R-R timing depends on being in the same free-running millisecond domain the PPG ISR
  stamps samples with. Two clocks that merely agree in rate are not the same clock.

### 5.4 Electrode geometry hand-off (`OI-CHARGE-02` / `-04`)

Before **any** enable is requested, the runner scans the already-verified descriptor for the two
modalities whose electrode geometry the safety MCU cannot infer, and sends per-channel electrode
**area** via `np_safety_spi_send_channel_limits()`.

**Area only — never a pre-computed limit.** Both density constants (150 mC/cm² per session, 40 µC/cm² per phase) stay resident on the
Class C safety MCU, which derives the limit itself. Sending a limit would move a Class C constant
onto the Class B side.

| Channel | Source of the area | Behaviour |
|---|---|---|
| `NP_SAFETY_CH_CLIN_STIM` (13) | implied by the HD-tDCS montage code — ring / bilateral 4×1 use 3.5 mm electrodes | fixed `NP_HD_SMALL_ELECTRODE_AREA_MCM2` = 96 milli-cm², **floored** so the derived limits can only err low |
| `NP_SAFETY_CH_TDCS` (6) | declared per protocol in the signed descriptor (`electrode_area_mcm2`) | **the smallest declared area wins** across several tDCS commands |
| `NP_SAFETY_CH_BES_TACS` (5) — **Rev 3** | the fixed pad constant `NP_BES_ELECTRODE_AREA_MCM2` (25 cm², PROVISIONAL, `OI-CHARGE-07`) | **gated** on its own bit (`OI-MMSOCK-02`): a pad is a fixed product part, so firmware may hold its area — but the MCU is no longer left to assume it |
| `NP_SAFETY_CH_VNS_HRV` (7), `NP_SAFETY_CH_CVNS` (10) | fixed constants, 0.5 cm² and 2 cm² (`OI-CHARGE-07`) | not gated; **sent in every session that commands them** (Rev 3 — see below) |

**The scan is `np_chan_decl_build()` (Rev 3).** It was inline in `np_runner_run()`, which is ARM-cross-only
and reached by no host test, and it arms Class C gates — so it was extracted to `src/np_chan_decl.c` and
is tested by `np_chan_decl_tests`. The runner arms the gates it returns, then sends the area frame, then
the waveform frame, in that order.

**Every area it computes is sent (Rev 3, `REQ-FWHUB-37`).** Until Rev 3 the area frame went out only
when the session held an HD-tDCS or tDCS command. The fixed BES, VNS and cervical-VNS areas were computed
into the frame and then **dropped in every other session**, and the safety MCU enforced its 25 cm²
fallback: 50× looser than designed on the 0.5 cm² auricular clip, 12.5× on the cervical collar. At rated
VNS parameters the per-phase charge is ~3 % of even the intended ceiling, so no dose exceeded a limit —
but the Class C argument that divides by 0.5 cm² (`OI-CHARGE-07`, `NP-HW-VNSCLIP-001` §5.2) was not what
ran. `RISK-FWHUB-15`. Falsified: restoring the old send rule fails the VNS-only and cervical-VNS-only cases.

T1 tDCS pad area is not implied by anything else the descriptor carries — `electrode_pair` names
10-20 sites, not pad sizes — so it travels in the signed descriptor, and the app pre-flight and this
enforcer divide by the same declared number.

**The fail-closed gates are the point.** A tDCS command declaring **no** area leaves `area_mcm2` at 0
("keep default" to the MCU) *while still arming the tDCS geometry gate*, so the MCU never grants
`TDCS` at all. Silently running such a protocol against the 25 cm² fallback is the fail-**open**
behaviour `OI-CHARGE-03` rejected. The **three** gates are deliberately **separate flags**
(`set_geom_required` for `CLIN_STIM`, `set_geom_required_tdcs` for `TDCS`, and — Rev 3 —
`set_geom_required_bes` for `BES_TACS`) because the gates are per-channel: declaring tDCS geometry must
not gate off clinical tACS, which shares the `CLIN_STIM` enable bit and declares no geometry, and a tDCS
area must not open the BES gate.

### 5.5 Shutdown

On completion or abort: `state := STOPPING` → `np_safety_spi_disable_all()` → `control(slot, NULL, 0)`
on **every** slot (modules ramp down internally) → `np_sock_disp_stop_all()` on every active socket → wait `NP_RUNNER_SHUTDOWN_MS` (5 s) → flush all
stops → finalise the UHDR and SHDR records → `np_log_session_end()` → `np_log_flush()` →
`COMPLETE` or `FAULT`.

`np_safety_spi_disable_all()` comes **first**, before the module stops. The enable lines are the
thing that matters; the graceful ramp is a comfort and dose consideration layered on top of a cut
that has already been requested.

`REQ-FWHUB-12`: `abort_reason` is authoritative on `s_ctx`, not on the record. The session-end block
overwrites `shdr.abort_reason` from `s_ctx.abort_reason` unconditionally, so a driver writing the
record field directly has its value erased before it is ever logged.

### 5.6 What the runner cannot dispatch — and the power gate (Rev 2)

**Rev 1 recorded here that `dispatch_command()` handled `NP_PROTO_TARGET_SLOT` and nothing else**, so
that no transcranial PBM command of either addressing form could reach an emitter (`OI-FWHUB-01`).
Rev 2 routes socket-addressed commands to the registry of §3.4. What remains true, and is the reason
this section keeps its number:

**A refused command is logged and kept out of UHDR.** Any command the socket registry refuses — at
placement, at power, or on a driver fault — is logged as an SHDR fault **against
`NP_HUB_SLOT_NONE`** with the refusing gate's status as the fault code, sets `NP_ABORT_MOD_FAULT`, and
does **not** set its bit in `uhdr.mods_active_mask`. UHDR is the patient's dose record; a command that
was refused must not appear in it as delivered. A socket **stop** that succeeds is dispatched but
delivers nothing, so it does not set the bit either. A target kind the parser does not know is
unreachable (§4.4) and is refused the same way.

**Every transcranial drive command is refused today, at the power gate, and that is deliberate.**
`NP-HW-HEXTILE-001` §9.3: *"A global concurrent-power governor is required in firmware … The compiler
and the session runner both need a power-budget check against the negotiated USB-C PD contract."*
While no socket command could reach an emitter, that requirement was latent. §3.4 removes the barrier
that made it latent, and the protocol library would not survive the change: `NP-SES-PWR-001` found
17 of 20 transcranial protocols over the 40 W emitter budget by 1.25×–40×, and
`scripts/check-pbm-power.ts`, re-run on the live library at this revision, reads **20 of 23 over, 2
within, 1 indeterminate** — every one of which compiles clean.

The governor cannot be written yet, and a plausible one would be worse than none:

- it must be denominated in **watts against the negotiated PD contract**, not a tile count — per-tile
  draw spans 1.3–25.0 W, so the concurrent ceiling spans 1–32 tiles (`OI-HEXTILE-09`,
  `NP-PWR-BUDGET-001` D-4) — and there is no PD-contract seam on this processor yet;
- its input is **undefined** for `frequency: 0Hz` combined with `duty_cycle:`, a 4× swing across a
  fifth of the library (`OI-SESPWR-03`, which blocks `OI-HEXTILE-09`);
- its per-tile watts derive from emitters that are **not selected** (`OI-HEXTILE-02`), so every
  coefficient would be a design target presented as a limit.

So `src/np_pbm_power_gov.c` defines `np_pbm_power_admit()` to **refuse every load**, and
`np_socket_dispatch_tests` asserts that the definition which ships does so (`REQ-FWHUB-35`). The
dispatch path is complete and closed by default — the shape of the `StudyDescriptorVerifier` that
ships refusing everything (CLAUDE.md §6.3), so the path cannot be opened without supplying the check
it exists to hold. **Consequence, stated plainly:** a transcranial PBM protocol still does not execute
on this firmware; the reason is now one function body rather than a missing registry. `OI-FWHUB-09`,
blocking. Stops never reach the governor.

---

## 6. Session log

### 6.1 Buffers and record tags

Two 4 KiB static buffers (`s_uhdr_buf`, `s_shdr_buf`), each record prefixed by a one-byte tag:

| UHDR tags | | SHDR tags | |
|---|---|---|---|
| `0x10` session start | `0x15` stim | `0x80` session end | `0x83` zone auth |
| `0x11` session end | `0x16` visual | `0x81` PBM health | `0x84` NTC peak |
| `0x12` EEG band | `0x17` EEG impedance | `0x82` fault | `0x85` EEG calibration |
| `0x13` PBM dose | `0x18` adaptation event | | `0x86` session open *(Rev 9)* |
| `0x14` VNS/HRV | `0x19` command *(Rev 9)* | | |

**`0x19` command — the commanded dose (Rev 9, `OI-FMEA-09`).** The runner writes one record per
dispatched command: `session_ms` (4), `mod_type`, `target_kind`, `slot_id`, `accepted` (1 each),
`params_len` (2), the signed `params`, and for a socket command its socket mask. `accepted` is
false when the registry refused the command, so a refused drive is on the record as refused and
never as delivered. The module caps are fixed firmware constants, so this record and the firmware
version reproduce the commanded current the safety MCU integrated. UHDR only: it is the treatment
the person received ("protocol parameters used", `data-architecture-detail.md` §5.1).

**`0x86` session open — the dirty-session marker (Rev 9, `OI-FMEA-09`).** `np_log_session_start()`
writes `SESSION_OPEN` with the device session count. It then appends and syncs both it and the UHDR
start record before it returns, so before the runner dispatches anything. A clean end writes `0x80`
for the same count. **An OPEN with no matching END is a session that did not end cleanly**, whether
from power loss, a hard fault or a watchdog reset. A UHDR session file with a start record and no end
record says the same. Without the marker, a truncated log cannot tell "the record was never written"
from "nothing happened", and a reader then has to infer state from an absence (CLAUDE.md §5.1).
The record carries the count only. Its classification is recorded in `data-architecture-detail.md`
§5.1.

The high bit separates the two spaces, which is not decorative: a tag byte misrouted between
partitions is then a visibly invalid tag rather than a plausible one.

EEG sample blocks **bypass** the buffer and go straight to the HAL — 500 Hz × 8 ch × 3 B = 12 000 B/s
would otherwise be copied through a 4 KiB intermediate for no benefit. **They skip the copy, not the
queue** (Rev 5). `np_log_eeg_sample_block()` first drains the adaptation ring, then hands `s_uhdr_buf`
down with an append and no sync. Every UHDR record logged before a block therefore precedes it in the
session file (§6.5).

### 6.2 The routing rule

Routing follows `NP-FW-EMMC-001` Rev 2 §12, and the rule this section enforces is CLAUDE.md §5's:

- **UHDR** — session UUID and timestamps, PBM dose, HRV, coherence, impedance, EEG band power, EEG
  waveforms, eye state, the protocol parameters actually used, adaptation events.
- **SHDR** — device-condition only. **No HR values, no EEG amplitudes, and no timestamps** — only the
  unsigned device session count.

`REQ-FWHUB-13`: **a field on neither list is UHDR** until a positive demonstration that it carries no
user biology is recorded in `docs/reference/data-architecture-detail.md` §5.1. The hub does not get
to decide a new field's class locally.

### 6.3 The unconditional timestamp suppression

`np_log_shdr_fault()` **discards `session_ms` for every caller and every fault type.** Callers pass
0 as belt-and-braces; the discard is the mechanism, and it is unconditional.

This is CLAUDE.md §5.1's second general rule made concrete, and the reason is a defect this programme
already paid for: zeroing `tick_ms` only for `NP_SAFETY_STATUS_CARDIAC` made `count > 0 && tick_ms ==
0` a **self-interpreting one-bit cardiac oracle**. A redaction applied conditionally on a sensitive
predicate leaks that predicate. `REQ-FWHUB-14`: **no SHDR suppression in this program may be made
conditional on the event being suppressed.** `scripts/check-redaction-shape.ts` enforces the shape at
the fault-latch marshaller; this section is the rule the script is derived from.

The cVNS lifecycle codes (`NP_CVNS_SHDR_EV_*`, `0xC1`–`0xC7`) are flags only — no HR, no RR, no kΩ.
So is `NP_STIM_SHDR_EV_DELIVERY_DIVERGENCE` (`0xD1`, Rev 9, §8.3.1): which slot diverged, never by
how much.
Raw per-electrode impedance is UHDR (patient tissue) and is **never** written to SHDR; only a
divergence flag may be (`OI-CVNS-HUB-11`).

### 6.4 Adaptation event log

*(This is the anchor `np_adaptation_log.c/.h` should cite; they currently cite a non-existent Rev 2 —
see the banner, finding 2.)*

`np_adaptation_log` is a per-session ring buffer of `np_adaptation_event_t`, one record per
closed-loop parameter change, drained to **UHDR only** by `np_adapt_log_flush()`. `NP-PRIV-REM-001`
STEP-33. There is no SHDR routing and none may be added: the *fact* that a parameter was adapted, and
when, is a statement about the person, not the device.

Two properties are binding. **Values are scaled integers (×100) or typed enums** — never a raw EEG or
HRV float, so the record carries the classification (`NP_ADAPT_TRIGGER_EEG_ALPHA_LOW`) rather than the
measurement. And **enum values are frozen once written to UHDR**: new triggers are additive only and
existing values must never be renumbered, because UHDR is read back by an app that may be older or
newer than the firmware that wrote it (`REQ-FWHUB-15`).

`np_adapt_log_reset()` clears the buffer at session start. A full buffer drops the event and
increments an SHDR fault count — dropping the record is preferred to blocking the runner.

### 6.5 Durability model

`np_log_backend.c` backs the four HAL entry points (`OI-LOG-01…04`) with a block-coalescing append
writer over the lower LittleFS-file HAL (`OI-LOG-05…07`).

- Appends coalesce into `NP_LOG_STAGE_BYTES` (512 B) program-block-sized writes, to limit eMMC write
  amplification and wear.
- Between flushes only whole blocks are written; a flush commits the exact buffered tail **without
  padding** and issues a block-device sync.
- The log file is append-mode, so **a flush never rewrites a previously committed byte.**
- Therefore: **at most the records appended since the last flush are lost on power loss** — bounded by
  `NP_LOG_UHDR_FLUSH_MS` (30 s) and `NP_LOG_SHDR_FLUSH_MS` (5 s).

**This layer writes plaintext and must continue to.** Encryption is transparent at the mounted
block-device layer — UHDR is mounted AES-256-XTS under the user biometric-derived UKMD by
`np_uhdr_key_unlock()`; SHDR under the HKDF manufacturing key at boot. `REQ-FWHUB-16`: **the log
backend never touches key material.** Confining the UHDR key to `np_uhdr_key` is a privacy invariant,
not a layering preference — NeurOne does not hold that key (CLAUDE.md §5.1), and every module that
could see it is a module that would have to be argued about.

**The durability claim above rests on LittleFS, and that dependency is weaker than it reads.**
`OI-NVRAM-12` raised it as the only Class B SOUP item neither vendored, version-pinned nor
anomaly-evaluated. `NP-SOUP-LFS-001` Rev 1, issued in the same change as this document, brings it
under SOUP management and finds something stronger: **LittleFS is not in the tree at all** — the only
`lfs_` tokens in `firmware/` are the two comments in this header naming the `OI-LOG-06/07` seams. The
durability model above is therefore **specified but unverified**, and `OI-LFS-01` (pin and vendor) is
**still blocking any reliance on it**. That item is not closed by this document and must not be read
as closed.

> **Update 2026-09-14 — `OI-LFS-01` is closed, and the durability model is no better off.**
> `NP-SOUP-LFS-001` Rev 2 pins littlefs at `v2.11.3` and vendors it, so the "not in the tree"
> half above is resolved: the component exists, with per-file SHA-256 and a recorded, tested
> configuration. **The blocking status moved rather than lifted.** What §6.5 needs is a NeurOne
> power-loss injection test against claims `L-1…L-4`, falsified in both directions — that is
> `OI-LFS-02`, which now carries `OI-LFS-01`'s BLOCKING status and is open. The model is still
> **specified and unverified**, and a populated `firmware/vendor/littlefs/` is not evidence for
> it (`NP-SOUP-LFS-001` §7.5).
>
> Two things from Rev 2 bear on this section specifically. **`OI-LFS-05`:** `EMMC-FS-01` states
> instance parameters for the **Config** partition only, and `L-1`/`L-2` — the two claims §6.5
> rests on — are about the **log** partitions, whose parameters no document states. **`OI-LFS-06`:**
> the pinned tag was chosen for an upstream fix to corruption caused by two open write handles on
> one file, which makes "one writer per log file" a requirement the `OI-LOG-05..07` glue has to
> keep rather than something that happens to be true.

> **Update 2026-09-14 (second) — `OI-LFS-02` is closed, and §6.5 is unblocked *to a stated depth*.**
> `NP-SOUP-LFS-001` Rev 3 performs the §7.1.2 anomaly evaluation (§11) and runs the power-loss
> injection test this section was waiting for (§12): `np_lfs_powerloss_tests` sweeps a cut across
> every medium-touching op of a flushed-append sequence under three tear models — **90 interrupted
> runs for `L-1`/`L-2`, 0 violations** — and `L-1` is checked in a **sharper** form than §6.5
> writes it: the recovered length must be an exact flush boundary, because *"a flush commits the
> exact buffered tail"* forbids a partial batch becoming visible. It was observed committing
> exactly one batch and not the next.
>
> **Three qualifications, and §6.5 must not be read past them.** (1) The test interrupts the
> `struct lfs_config` **contract**, not the eMMC behind the XTS layer — `OI-LFS-07`, and it needs
> hardware. (2) It mounts the **Config** instance, because `OI-LFS-05` is still open and the log
> partitions still have no stated parameters; `L-1` and `L-2` are claims about those. (3) A
> post-power-loss **hang** is invisible to a sweep (upstream #1211), and on this device it is
> fail-safe only because the SPI heartbeat stops. So §6.5's durability model is **verified in
> shape and unverified on its own medium**.

> **Update 2026-09-24 — `OI-LFS-05` and `OI-LFS-06` are closed (`NP-SOUP-LFS-001` Rev 4 §13).**
> Qualification (2) above is lifted: the log partitions now have parameters — `EMMC-FS-01`'s own
> UHDR and SHDR columns, which the Rev 2 note above wrongly said did not exist, with one deviation
> (`read_size`/`prog_size` 512, the XTS data unit, `ECR-EMMC-002`) — applied and validated by
> `np_lfs_log_instance`, and `L-1`/`L-2` are swept on the **real** 1,767,168- and 131,072-block
> geometry: 132 interrupted runs, 0 violations. Qualifications (1) and (3) stand. **Two new ones
> bear on §6.5:** the specified UHDR `lookahead_size` means one whole-filesystem allocator
> traversal per 16 MiB written, during which the writer waits — counted on the host (≈ 0.5 reads
> per block in use), timed only on hardware (`OI-LFS-07`); and §6.5's single append-mode log file
> per partition cannot exceed littlefs's 2 GiB `file_max`, under a third of UHDR (`OI-LFS-11`,
> for `OI-LOG-05`). "One writer per log file" is now enforced for the Config instance by a
> registry and a CI gate; the log glue joins that gate's caller list when it is written.

> **Update 2026-09-24 — `OI-LFS-11` closed (`NP-SOUP-LFS-001` Rev 7 §13.10).** §6.5's *"the log
> file"* is, for UHDR, **one file per session** — `/uhdr/sessions/<count>`, as `NP-FW-EMMC-001`
> `EMMC-UHDR-12`/`-13` specify — opened by `np_log_session_start()` and closed by
> `np_log_session_end()`, created exclusively and never reopened. The durability model above holds
> per file: a flush commits the exact tail of the session file it was written to. SHDR keeps one
> file. UHDR is no longer opened at bring-up (it is not mounted until the user unlocks it), and
> nothing is written to UHDR outside a session. The device session count increments at session
> start but is not persisted (`OI-LFS-12`); the logger steps past a count whose file exists.
> *(`OI-LFS-12` closed the same day, `NP-SOUP-LFS-001` §13.13: the count is persisted in the Config
> partition by `np_session_count`, committed before each session's file is created and loaded at
> bring-up; `np_hal_get_device_session_count()` is retired.)*

> **Update 2026-09-24 (Rev 5) — the logger now feeds the backend in the order this model assumes.**
> "At most the records appended since the last flush are lost" describes the backend. The logger
> above it was breaking that assumption in three ways. Each is fixed in `np_session_log.c`, and each
> has a case in `np_log_backend_tests` that fails when its fix is reverted:
>
> 1. **Sync before append on overflow.** When a 4 KiB buffer filled, `uhdr_write()` and
>    `shdr_write()` synced first and appended the buffer afterwards. The sync therefore covered none
>    of the ~4 KiB it was there for, and those bytes waited for the next flush: up to 30 s for UHDR
>    and 5 s for SHDR. Each partition now has one helper, `uhdr_drain()` or `shdr_drain()`, which
>    appends and then syncs. That helper is the only way a buffer becomes durable; overflow, session
>    boundaries and `np_log_flush()` all use it. Records are serialized one field at a time, so an
>    overflow can fall inside a record. The sync covers the part already handed down, and the rest
>    follows in order.
> 2. **EEG blocks overtook buffered records.** `np_log_eeg_sample_block()` appended straight to the
>    HAL. Records still in `s_uhdr_buf` therefore landed *after* an EEG block logged later than
>    them. So did adaptation events, which reach `s_uhdr_buf` only when the ring is drained. The EEG
>    path now drains the ring and hands the buffer down before writing its header. It appends only:
>    order needs no sync, and syncing would cost one per block. The samples still skip the copy. A
>    UHDR session file is now in the order its records were logged.
> 3. **The periodic flush skipped EEG data** (found by the test for (2)). `np_log_flush()` synced
>    UHDR only when `s_uhdr_buf` was non-empty. EEG bytes sit in the backend's 512 B staging, where
>    that check cannot see them. With (2) in place the buffer is empty after every block, so the
>    30 s bound would never have held for EEG. UHDR now syncs on every `np_log_flush()`. SHDR has no
>    bypass path and keeps its conditional.
>
> The host model gains `np_log_test_synced_len()`, which reports how much of a file the last sync
> covered. **Not changed:** the backend, the record formats and the flush cadence. **Two things this
> does not settle.** First, the ordering guarantee holds only when every logger call runs in one
> context. §8.2 says `np_log_eeg_sample_block()` runs in the DMA ISR. From there it would race the
> task-side logger on `s_uhdr_buf`, the adaptation ring and the backend staging; the old direct
> append already raced on the staging. No caller exists yet, and the function is absent from the
> linked `np_application.elf`, so this is `OI-FWHUB-14`, not a live defect. Second, adaptation events
> still in the ring when a session ends are dropped. `np_session_runner` calls
> `np_log_session_end()`, which closes the session file, before `np_log_flush()` drains the ring
> (`OI-FWHUB-15`).

> **Update 2026-09-24 (Rev 6) — `OI-FWHUB-15` closed.** `np_log_session_end()` now drains the
> adaptation ring first, while the session's file is still open. Events queued at session end
> therefore land in that session's file, ahead of the session-end record, which ends each file. The
> runner's `np_log_flush()` that follows finds the ring empty. A test asserts both properties, and
> both fail when the drain is removed. It also checks that nothing from the ended session reaches
> the next session's file.

---

### 6.6 Per-session recording limit — proposed IFU text *(Rev 4)*

Since `OI-LFS-11` (`NP-SOUP-LFS-001` Rev 7 §13.10) each session's UHDR record is **one file**,
`/uhdr/sessions/<count>`, and littlefs caps one file at `file_max` = 2,147,483,647 B
(`NP-FW-EMMC-001` Rev 3 `EMMC-FS-01`). What that means for the person using the device:

| | Value | Derivation |
|---|---|---|
| Per-session file cap | 2,147,483,647 B (2 GiB − 1) | `NP_LOG_SEGMENT_MAX_BYTES` = `file_max` |
| Dominant data rate | ≈ 12,000 B/s | 8 EEG channels × 500 Hz × 3 B (`NP_EEG_CHANNELS`, `NP_EEG_SAMPLE_BYTES`); every other UHDR record is small beside it |
| Continuous recording per session | **≈ 49 hours** with EEG recording | 2,147,483,647 / 12,000 ≈ 178,900 s ≈ 49.7 h — stated as *"about 49 hours"* so the figure never over-promises |
| What happens at the limit | **recording stops for the rest of that session; the session itself is not stopped** | `np_log_*()` return nothing and `np_runner_run()` does not check them — the limit ends the record, not the stimulation, which is still governed by the safety MCU and the protocol's own duration limits |
| Effect on other sessions | none — the next session starts a new file; earlier sessions are untouched | per-session files, created exclusively (§6.5 update) |

**Proposed IFU text** — for the instructions for use and the in-app help when those documents
exist (`NP-QMS-DC-001` lists device labelling and IFU as *TBD*, so there is no IFU to edit yet;
`OI-FWHUB-13` carries this text to it). Plain language, second person, no internal identifiers:

> **How long a session can record.** Your NeurOne saves each session's recording — brain-activity
> (EEG) data, heart-rate data and session events — in its own file on the device. A single session
> can record for about 49 hours. If one session runs longer than that, the device stops recording
> for the rest of that session. The session itself is not stopped, and your earlier sessions and
> your next session are not affected. Most sessions are far shorter than this limit.
>
> This is a limit on one session. The device's total storage is separate: the app tells you when
> the device's storage for your own data is nearly full.

The last sentence points at the existing `EMMC-WE-02` notification (*"UHDR storage approaching
capacity"*), so the two limits are not confused. **Not added:** an in-app warning at protocol
authoring. Every protocol longer than 2 hours already draws `VALIDATE_MSG_GENERAL_DURATION_2`,
far below this limit, and a new locale key needs all eleven locales (CLAUDE.md §17). If a protocol
ever legitimately runs near 49 hours, that key becomes worth adding, and this section is where its
figure comes from.

## 7. Safety MCU interface

The hub is always SPI master; the STM32G071 is slave. Two frame types.

### 7.1 Extended heartbeat — 38 bytes, every 200 ms

Hub sends `np_safety_tx_ext_frame_t`: magic (`0xBE`, `0xA7`), enable mask, `session_status` bits,
`channel_count`, checksum, `current_ua[14]`, extended checksum. The MCU simultaneously returns its
8-byte reply; the hub reads the first 8 bytes and discards the remaining 30 slave zero bytes.

`current_ua[]` carries **commanded** current magnitudes from the session descriptor — **not**
ADC-measured values. This matters for classification: a commanded magnitude is a device-configuration
fact (SHDR class); a measured one would be a statement about tissue.

Reply status bits (`NP_SAFETY_STATUS_*`): `FAULT`, `WATCHDOG`, `CUTOFF`, `IMPEDANCE`, `THERMAL`,
`CHARGE`, `CARDIAC`, `SIG_PENDING`.

### 7.2 Requested vs granted masks

`REQ-FWHUB-17`: **the heartbeat transmits the *requested* mask, never the granted echo.**
Transmitting the granted mask would mean a newly-requested enable never reaches the safety MCU — the
hub would ask only for what it already has. `np_safety_spi_get_granted_mask()` is read-only state
for the hub's own decisions.

The relationship is one-directional and is the whole safety architecture in one line: **the hub
requests; the MCU grants; the MCU owns the GPIO.** A granted bit absent from the request is
impossible; a requested bit absent from the grant is routine and must be handled as "not enabled".

### 7.3 Session signature — 102 bytes, once per session

Ordering is a requirement (`REQ-FWHUB-18`), because each step establishes the precondition of the next:

1. heartbeat with `session_state = RUNNING` → MCU observes `ACTIVE` 0→1 and sets `SIG_PENDING`
2. `np_safety_spi_send_session_sig(hash, sig)` — 32-byte SHA-256 of the signed descriptor portion,
   64-byte Ed25519 signature. The MCU's reply *during this transfer is discarded.*
3. the **next heartbeat reply** is the definitive result: `SIG_PENDING` cleared = verified;
   still set = **rejected → abort the session and request no enables**.

`np_safety_spi_request_enable()` with a non-zero mask must not be called before step 3 succeeds.
The MCU verifying the same signature the hub verified is not redundancy for its own sake: it is what
makes "the headset rejects unsigned or corrupted protocols" (CLAUDE.md §4.2) a Class C claim rather
than a Class B one.

### 7.4 Geometry gates

While `set_geom_required` / `set_geom_required_tdcs` / `set_geom_required_bes` is set, every heartbeat
carries the corresponding `session_status` bit (2, 3, 4) and the MCU keeps that channel **out of
`granted_mask`** until it has applied a valid electrode-area command. A lost or delayed area command
therefore fails **closed**. All three are cleared on session end/abort and by
`np_safety_spi_disable_all()`. Bits 5–7 of `session_status` remain unused. See §5.4.

### 7a. Cervical VNS re-enable manager

`np_cvns_reenable.c` decides when the hub may assert `NP_SESSION_STATUS_CVNS_REENABLE` after the
safety MCU's cardiac interlock has cut cervical VNS. It is **pure C with no FreeRTOS dependency**,
which is why it is host-testable (`np_cvns_reenable_tests.c`, 548 lines).

**All three gates, in order, or the bit is not asserted:**

1. `NP_CVNS_REENABLE_LOCKOUT_MS` (30 s) since the hub **observed** the cutoff — the first heartbeat
   reply with `CARDIAC` set. Because the hub's observation lags the MCU's cutoff by at most one
   heartbeat period (200 ms), **the hub's window always fully contains the MCU's own
   `NP_CARDIAC_LOCKOUT_MS` window**; the MCU can never see the bit while its lockout is active. The
   containment is the argument — not the equality of the two constants.
2. Explicit operator/user confirmation via the app. Confirmations are **single-use**, accepted only
   *after* the lockout has elapsed — a stale pre-lockout tap is **rejected, never queued** — and are
   consumed by step 3.
3. A **new** hub-side impedance check, started by the confirm, that **passed**. Fail, garbage or
   timeout (`NP_CVNS_REENABLE_IMP_TIMEOUT_MS`, 5 s) all return to `AWAIT_CONFIRM`.

```
IDLE ──CARDIAC rising edge──▶ LOCKOUT ──30s──▶ AWAIT_CONFIRM
  ▲                                                  │ confirm()
  │                                                  ▼
  │◀─CARDIAC cleared / session not running──   IMPEDANCE
  │                     pass │   fail/timeout └──▶ AWAIT_CONFIRM
  │                          ▼
  └──CARDIAC cleared──── ASSERTING ──2s, CARDIAC still set──▶ AWAIT_CONFIRM
```

`ASSERTING` is the **single** bit-emitting state and every anomaly exits it. The assertion is
bounded: `NP_CVNS_REENABLE_ASSERT_TIMEOUT_MS` (2 s, ten heartbeats) — if the MCU has not cleared
`CARDIAC` by then, the hub deasserts and requires a fresh confirmation **and** a fresh impedance pass.

`REQ-FWHUB-19`: `np_cvns_reenable_bit_active()` is the **only** legitimate source of the bit. The
heartbeat task mirrors it into `np_safety_spi_set_cvns_reenable()` once per beat, and
`np_runner_run()` hard-resets the manager at session start so no cutoff, confirmation or assertion
state crosses a session boundary.

**Recoverable vs not.** A heartbeat with `CARDIAC` and **none** of
`NP_CVNS_NONRECOVERABLE_FAULTS` (`FAULT | WATCHDOG | THERMAL | CHARGE`) is a *recoverable* cardiac
cutoff: the hub holds the session and runs this flow instead of aborting. `CUTOFF` and `IMPEDANCE` are
expected companions of a cardiac event and do not force an abort by themselves.

**Safety backstop.** The Class C safety MCU independently ignores the re-enable bit while its own
lockout is active (`np_cardiac_interlock_reenable()` denies during `s_lockout_active`). All of §7a is
therefore a *usability and dose* mechanism layered over a Class C denial, which is why it can be
Class B.

---

## 8. Module drivers

Every driver implements the same five functions — `detect`, `init`, `control`, `telemetry`,
`shutdown` — and exposes **no per-module header**: the registry is the single wiring point (the
"registry idiom"). `control(slot, NULL, 0)` is the universal stop.

Each driver's hardware seam is a named `OI-*` HAL stub. Those stubs are the hardware-bring-up backlog
and are listed per driver in the source banners; they are **not** repeated here, because a list
duplicated between a spec and a header is a list that will disagree.

### 8.1 PBM zone modules — `np_mod_pbm.c`

Base (660 nm + 808 nm, direct PWM) and smart (+1064 nm, on-module MCU over I²C) through one function
table; the registry entry's type discriminates. Delegates detection to `np_pbm_detect.c`, smart-module
I²C driving to `np_pbm_drive.c`, dose metering to `np_pbm_dose.c`.

Per-slot state carries `ntc_peak_c` and `throttle_count`. The 42 °C IEC 60601 limit and the 62 °C
junction throttle are enforced in hardware per zone (CLAUDE.md §4.2); this driver records that they
fired, it does not implement them.

**The slot entry points are reachable only through slots 0–4, which the parser rejects (§3.3).** Rev 2
adds `np_mod_pbm_socket_drive()` / `np_mod_pbm_socket_stop()`, the socket-indexed entry points the
dispatch registry calls (§3.4). They never touch the safety enable, and they keep their own per-socket
module type and smart-driver state, sized to the 128-socket domain.

### 8.2 EEG — `np_mod_eeg.c`

ADS1299, 8 channels, 500 Hz, 24-bit. **Fixed hardware: `detect()` always returns OK.** Waveform data
is DMA-driven and written directly to UHDR from the DMA ISR via `np_log_eeg_sample_block()`; this
driver handles configuration, impedance reads and band-power computation for adaptive feedback.
**That calling context is not safe as built** (`OI-FWHUB-14`). The logger state the function touches
is unsynchronized and assumes a single context (§6.5). No call site exists yet.

Self-calibration at session start routes the ADS1299 internal reference to all channels for one
epoch; gain/offset coefficients are stored in the Config partition (SHDR class) per
`NP-FW-EMMC-001` Rev 2 §9.

The Oz channel is the photoparoxysmal detector that halts the goggles (§8.6).

### 8.3 Stimulation — `np_mod_stim.c` (BES/tACS + tDCS)

One driver, two slots (§3.1), because both use the same stimulation DAC. Firmware-layer caps:
BES/tACS ≤ 1000 µA, tDCS ≤ 2000 µA, minimum 30 s ramp.

`REQ-FWHUB-20`: **these are secondary caps, and must always be stated as such.** The binding limit is
the safety MCU's hardware commanded-charge ceilings (150 mC/cm² per session for DC, 40 µC/cm² per phase for charge-balanced), which the app cannot override. A firmware
cap that is described as *the* limit invites someone to relax it.

#### 8.3.1 Commanded-versus-delivered cross-check — `np_stim_xcheck.c` *(Rev 9, `OI-FMEA-09`)*

**Why it exists.** The safety MCU's charge monitor integrates the **commanded** current the hub
publishes. That is a signature-independence choice (`NP-SW-001` SW01-M03), and it means the monitor
cannot see a driver delivering more than it was told to. That hazard is `NP-FMEA-001` FMEA-M03-02,
and this is its control.

**What it compares.** The driver calls `np_stim_xcheck_commanded()` beside every
`np_safety_spi_set_channel_current()`, with the same capped value. The check therefore holds
delivery to the number the MCU integrates, not the authored one: a BES command authored at 5 mA is
held to 1 mA. At each telemetry snapshot the runner passes the record to `np_stim_xcheck_observe()`.
An **excess** is `|delivered| > bound + max(bound × TOL_PCT %, FLOOR_UA)`. A non-finite reading
counts as an excess, because it cannot show delivery is in bounds. `NP_STIM_XCHECK_CONSECUTIVE`
excesses in a row raise **one** SHDR flag for that channel for the session. An in-bound snapshot
resets the run.

**The bound through a ramp.** tDCS reaches a lower target by ramping, over `ramp_s`, or over 30 s on
a stop. While that ramp runs, the previous level stays the bound, so a ramp-down is not flagged as
over-delivery against the new target. The hold is anchored at the first snapshot after the command,
which can lengthen it by up to one telemetry interval and never shortens it. BES/tACS steps, so it
has no hold. An increase is the bound at once. **The envelope belongs to the driver, not the
session.** A 30 s tDCS stop ramp outlasts the runner's 5 s shutdown wait (§5.5). Session start
(`np_stim_xcheck_reset()`) therefore clears only the latch and the debounce run. It keeps the
commanded level and re-anchors a running hold to the new session's clock, so the previous session's
ramp is not flagged in the next one.

**Only the excess direction.** Under-delivery reduces dose. It is not the FMEA-M03-02 hazard, and
flagging it would spend the flag on electrode contact, which the impedance checks already cover.

**Coverage, stated narrowly.** Only BES/tACS and T1 tDCS have a delivered-current read
(`np_mod_stim_hal_read_current()`, `OI-STIM-06`, which for BES returns the peak magnitude, the same
quantity as `amplitude_ua`). Cervical VNS puts its **commanded** current in `current_ua`, so a check
there would compare a number with itself, and it is excluded by name. Auricular VNS reports no
current at all. T2 clinical stimulation waits for `NP-HW-TACSDRV-001` A16.2's sense path.

**What this does not settle.** `NP_STIM_XCHECK_TOL_PCT`, `_FLOOR_UA` and `_CONSECUTIVE` are
**unvalidated placeholders**, marked as such in `np_hub_config.h`, and they are not requirements
(`NP-CONV-001` §7.1). A derived threshold needs the sense path's measured accuracy and the charge
headroom the ceiling can spend before the flag fires. The flag is also **only a flag**: it does not
stop the channel. The spec calls for no more, and whether a divergence should also cut the channel
is a Safety decision. Both remain `OI-FMEA-09`.

### 8.4 VNS + HRV — `np_mod_vns.c`

Auricular clip on slot 8, detected by accessory-port impedance. Delegates HRV biofeedback to
`firmware/hrv_biofeedback/` (`np_hrv_session`), which runs as its own FreeRTOS thread; this driver is
the start/stop interface.

**Contact confirmation precedes the enable request**: impedance must fall in
`[VNS_CONTACT_MIN_OHM, VNS_CONTACT_MAX_OHM]` = [500 Ω, 5000 Ω] before `np_mod_vns_control()` requests
`NP_SAFETY_EN_VNS_HRV`. The safety MCU independently holds the enable if contacts are not confirmed
(CLAUDE.md §4.2) — the hub check exists to avoid asking, not to substitute for the MCU's refusal.

The HRV session uses `np_mod_vns_hal_now_ms()`, the same free-running millisecond clock the session
start/stop/tick timing uses — see the §5.3 note on clock domains.

### 8.5 Audio — `np_mod_audio.c`

Over-ear planar magnetic (binaural beats, isochronic tones, pink/brown noise) and mastoid bone
conduction, both from the hub I²S codec via SAI.

**Not safety-MCU gated** — `NP_SAFETY_EN_AUDIO` is 0, deliberately, and `slot_to_safety_bit()`
returns 0 for it so no enable is requested. Audio delivers no current, no light and no field.

EEG-adaptive mode retunes the binaural carrier every `NP_AUDIO_ADAPT_INTERVAL_MS` to track the
dominant EEG band, which the EEG driver posts via `np_mod_audio_set_eeg_band()`. The band→beat table
(`k_band_beat_mhz`) maps delta/theta/alpha/beta/gamma to 2/6/10/20/40 Hz.

### 8.6 Visual — `np_mod_visual.c`

108 micro-LEDs per lens, 6 zones per eye. **Four independent interlock layers**, which is one more
than CLAUDE.md §4.2's table names for this modality and is stated here as the code has it:

1. IR proximity — eye-open detection before enable
2. Hall sensor — goggle lift → instant LED cutoff, **GPIO interrupt, not polled**
3. hardware current limit at the IEC 62471 MPE ceiling, enforced by the safety MCU
4. ADS1299 Oz photoparoxysmal detection → `np_mod_visual_ppx_halt()`, < 200 ms

Layer 2 being an interrupt rather than a poll is the requirement (`REQ-FWHUB-21`): at
`NP_DETECT_VISUAL_POLL_MS` = 200 ms, a polled Hall would allow up to a full poll period of emission
into a lifted goggle.

**Mode F (NIR retinal walk) is gated at compile time**: `NP_MODE_F_REGULATORY_CLEARED` is **0** and
must stay 0 until the `RISK-03` Q-13 regulatory opinion letter is received
(`NP-REG-PBM1064-001` Q-13, `NP-FW-EMMC-002` Rev 2 §F). `REQ-FWHUB-22`: a compile-time flag, not a
runtime setting — a runtime gate on an uncleared regulatory question is a gate someone can turn on.

### 8.7 Intranasal — `np_mod_intranasal.c`

Bilateral Y-probe, 660 nm + 808–830 nm per side. Authentication is an **optical/resistive pogo-pin
sleeve code — no NFC, no RF, no EMF at the scalp**, consistent with CLAUDE.md §1's wired-first
principle. Duty ceiling `INS_DUTY_MAX` = 25 %, the same ceiling as the cranial tiles.

Detection requires **both** `auth_check()` and `pd_contact()`. This is the only authenticated
consumable path in the firmware (CLAUDE.md §2.3 — hygiene sleeves).

### 8.8 Cervical VNS (T2) — `np_mod_cvns.c`

The only glue between the hub module framework and the standalone cervical VNS safety library
(`firmware/cervical_vns/`). It owns the three library contexts and translates a signed
`np_mod_cvns_params_t` into a validated `np_cvns_session_config_t`.

It bridges **two** safety channels and handles both:

- The unified heartbeat enable (`NP_SAFETY_EN_CVNS`) is requested **only once** the library has
  cleared its own pre-stimulation gate — impedance → cardiac baseline → MCU grant — and is actually
  delivering. It is dropped the instant the library faults, the session ends, or the runner stops the
  command.
- A cardiac cutoff surfaces two independent ways: the library's own PPG/Pan-Tompkins pipeline fires
  the fault callback, **and** `NP_SAFETY_STATUS_CARDIAC` arrives in the heartbeat reply and is
  consumed by the hub-global re-enable manager (§7a). The second path is not this driver's concern.

**Two configuration facts are safety-critical and cannot be waived by a descriptor** (`REQ-FWHUB-23`):
the PPG sample rate is pinned to `NP_CVNS_PPG_SAMPLE_RATE_HZ` and **compile-time asserted**, because
the Pan-Tompkins timing constants depend on it; and the **cardiac baseline is mandatory** — a
descriptor asking to skip it (`baseline_req == 0`) is ignored, and the driver never requests the
unified enable before the library reports an active stimulation stage.

Cross-validation (`OI-CVNS-HUB-11`): the heartbeat task hands the MCU's per-electrode impedance report
to this driver, which compares it against the hub's own measurement. Divergence beyond
`NP_CVNS_IMPEDANCE_CROSSVAL_KOHM` emits `NP_CVNS_SHDR_EV_IMP_CROSSVAL` — **a flag, no kΩ values**
(§6.3).

### 8.9 T2 and accessory stubs — `np_mod_t2_stubs.c`

*(This is the anchor `np_mod_t2_stubs.c` should cite; it currently cites a non-existent Rev 2 — see
the banner, finding 2.)*

Slots 11–16 (`QEEG`, `TMS`, `PBM_1170NM`, `CLIN_TACS`, `HD_TDCS`, `VIBROTACTILE`) are placeholder
implementations whose `detect()` returns `NP_HUB_ERR_NOT_PRESENT` and whose `control()` returns the
same. **Nothing decodes their parameter structs today.**

This matters for two live open items elsewhere and is why the stub state is worth specifying rather
than dismissing: `OI-TACS-02`'s 21-channel `np_mod_clin_tacs_params_t` and `OI-TCAP-01/02`'s
mutually-unsatisfiable current/density/charge constants are both *latent* precisely because these are
stubs. `REQ-FWHUB-24`: **any change from stub to real driver re-opens those items before it ships**,
not after.

---

## 9. IEC 62304 classification

**SW-02 Class B.** The hub control program cannot by itself cause an unacceptable risk, because it
cannot enable stimulation.

The argument, in the four steps that carry it:

1. **No electrical path.** Every stimulation enable line is a GPIO physically owned by the STM32G071
   safety MCU (CLAUDE.md §4.2, `NP-HW-HUB-001` Rev 4 §7.2). The hub's only influence is the
   *requested* mask in a 38-byte SPI frame. A hub that requests everything gets what the MCU grants.
2. **No stored value widens a grant.** Nothing the hub persists — calibration, the module map, the
   log — is read by the safety MCU. Its limits are resident constants (both charge-density ceilings, HR-change
   cutoff, lockout duration) and the hub supplies *inputs* to them (electrode **area**, §5.4), never
   the limits themselves.
3. **Every failure path fails closed.** The watchdog cuts on heartbeat loss in < 50 ms; §2.4 forbids
   a fault path that keeps beating; unknown target kinds drop; retired slots reject; missing geometry
   gates the channel off; the cVNS re-enable machine exits its single asserting state on every
   anomaly.
4. **The Class C claims are made on the Class C side.** Signature verification is performed by the
   MCU (§7.3) as well as by the hub; the cardiac lockout is denied by the MCU independently of §7a.

**Residual, and it is the honest one.** The hub commands the 18 per-cluster PBM gates
(`HUB-REQ-C05`, `NP-HW-HUB-001` Rev 4 §7.2.2) — they are Class B and are commanded by *this*
processor, not by the safety MCU and not by the cluster controller. The accepted consequence is
all-or-nothing cranial PBM: the MCU's `NP_SAFETY_EN_PBM_CRANIAL` is one bit for the whole lattice. The
corresponding per-tile *drive magnitude* question is `OI-NVRAM-10` — whether a Class C bound on
per-tile emitter drive current exists independently of the module map's ranges — and if it does not,
a wrong range is bounded only by the 62 °C thermal cutoff, **a thermal limit standing in for an
optical one.** This classification must be re-derived before any tile variant with differing ranges
ships. It is carried, not closed, by this document.

**Test evidence.** `firmware/hub_control/tests/` — 4,412 lines of host tests across eight files covering the module map,
cVNS module, protocol parser, cVNS re-enable machine, stim module, PBM calibration bridge, transport
and log backend. `wc -l firmware/hub_control/tests/*.c` is the source of truth for that figure, not
this line.

---

## 10. Requirements

### 10.1 Binding requirements, all met by the code as of 2026-09-23 (Rev 2 adds 31–35)

| ID | Requirement | Where |
|---|---|---|
| `REQ-FWHUB-01` | Bring-up order per §2.1 — all four constraints, including `np_log_init()` before `np_mod_reg_scan()` | `np_hub_control_main.c`; gated by `scripts/check-hub-bringup-order.ts` |
| `REQ-FWHUB-02` | Heartbeat task holds highest priority and never blocks beyond one heartbeat period | §2.2 |
| `REQ-FWHUB-03` | Module detection does not probe while a session is running | §2.2 |
| `REQ-FWHUB-04` | One reassembler, one framing, both transports | `np_transport.c` |
| `REQ-FWHUB-05` | `np_transport_feed()` is called from task context, never a raw ISR | `np_transport.h` |
| `REQ-FWHUB-06` | No hub fault path keeps the heartbeat alive | `np_hub_freertos_hooks.c`, `np_platform_trap.h` |
| `REQ-FWHUB-07` | Slot numbers and enable-bit positions are append-only; never reused or compacted | §3.1 |
| `REQ-FWHUB-08` | `hubCompiler.ts` emits exactly §4; on disagreement the compiler is wrong | `hubCompiler.ts` |
| `REQ-FWHUB-09` | A failed parse leaves the descriptor undefined; no partial population | `np_protocol.c` |
| `REQ-FWHUB-10` | A parameter-struct length change bumps `NP_HUB_PROTO_VERSION` | §4.5 |
| `REQ-FWHUB-11` | `session_status` is built only by `np_safety_session_status_bits()` | `np_safety_spi.h` |
| `REQ-FWHUB-12` | `abort_reason` is authoritative on the runner context, not the record | `np_session_runner.c` |
| `REQ-FWHUB-13` | A field on neither classification list is UHDR | §6.2 |
| `REQ-FWHUB-14` | No SHDR suppression is conditional on the event being suppressed | §6.3 |
| `REQ-FWHUB-15` | Adaptation trigger enum values are frozen once written to UHDR; additive only | §6.4 |
| `REQ-FWHUB-16` | The log backend never touches key material | §6.5 |
| `REQ-FWHUB-17` | The heartbeat transmits the requested mask, never the granted echo | §7.2 |
| `REQ-FWHUB-18` | Session-signature ordering per §7.3; no non-zero enable before `SIG_PENDING` clears | `np_safety_spi.c` |
| `REQ-FWHUB-19` | `np_cvns_reenable_bit_active()` is the only source of the re-enable bit | §7a |
| `REQ-FWHUB-20` | Firmware stimulation caps are secondary to the MCU charge-density limit and are stated as such | §8.3 |
| `REQ-FWHUB-21` | Goggle Hall cutoff is interrupt-driven, not polled | §8.6 |
| `REQ-FWHUB-22` | Mode F is compile-time gated at `NP_MODE_F_REGULATORY_CLEARED` = 0 | §8.6 |
| `REQ-FWHUB-23` | cVNS PPG rate is compile-time asserted; cardiac baseline is not descriptor-waivable | §8.8 |
| `REQ-FWHUB-24` | Promoting a T2 stub to a real driver re-opens `OI-TACS-02` and `OI-TCAP-01/02` first | §8.9 |
| `REQ-FWHUB-29` | Every SHDR record a boot emits is durably buffered and carries the true device session count | §2.1; gated |
| `REQ-FWHUB-30` | The file banner's declared task count equals the number of `xTaskCreate()` calls | §2.2; gated — counted, not read |
| `REQ-FWHUB-31` | A socket-addressed command is dispatched only through the socket registry and a slot-addressed one only through the slot registry; neither falls back to the other | §3.4, `np_session_runner.c` `dispatch_command()`; `np_socket_dispatch_tests` |
| `REQ-FWHUB-32` | A socket-addressed drive command drives every named socket or none — placement, power and driver faults all leave no socket of that command driven | §3.4; `np_socket_dispatch_tests` (placement, rollback) |
| `REQ-FWHUB-33` | Only `NP_MOD_PBM_BASE` / `NP_MOD_PBM_SMART` are socket-addressable | §3.4 gate 1; `np_socket_dispatch_tests` |
| `REQ-FWHUB-34` | A socket stop is always admitted; `NP_SAFETY_EN_PBM_CRANIAL` is requested only after a command's sockets are all configured and released only when none is active — or at once, with every socket stopped, when a stop fails | §3.4; `np_socket_dispatch_tests` |
| `REQ-FWHUB-35` | No socket drive command is admitted without `np_pbm_power_admit()`, and the definition that ships refuses every load until the `OI-HEXTILE-09` governor exists | §5.6; `np_socket_dispatch_tests` links the production definition and asserts it |
| `REQ-FWHUB-36` | **(Rev 3)** A session containing a BES/tACS command arms `NP_SESSION_STATUS_GEOM_REQ_BES`, so the safety MCU grants BES/tACS only after an area for that channel has been applied. *Fails without it:* BES/tACS is enforced against the 25 cm² fallback on trust — 24× permissive on a T1-B lattice electrode (`NP-FW-MMSOCK-001` §3.6.1). *Traced to:* DI-SAFE-01a; `OI-MMSOCK-02` | §5.4, §7.4; `np_chan_decl_tests`, `np_charge_monitor_tests`, `np_safety_spi_proto_tests` |
| `REQ-FWHUB-38` | **(Rev 8)** While no session is in flight, every accessory slot (7–18) is re-probed, and a slot is re-initialised only when its presence or type changes. *Fails without it:* an accessory attached after boot is never usable (CLAUDE.md §1's field-upgradeable modules; `OI-FWHUB-16`). Re-initialising unchanged slots instead writes an SHDR auth record twice a second per attached intranasal or cervical VNS unit (§5 SHDR, `NP-FW-EMMC-002` §G wear) | §3.2; `np_module_registry_tests` |
| `REQ-FWHUB-37` | **(Rev 3)** Every electrode area the runner computes is sent to the safety MCU. *Fails without it:* the MCU enforces its 25 cm² fallback in place of the fixed VNS / cervical-VNS / BES areas — 50× looser than designed on the auricular clip. *Traced to:* DI-SAFE-01a; `OI-CHARGE-07` | §5.4; `np_chan_decl_tests` (falsified against the old send rule) |

| `REQ-FWHUB-39` | **(Rev 9)** Every dispatched command is written to UHDR as a commanded-dose record (§6.1 `0x19`), refused commands marked refused. *Fails without it:* the device log cannot reconstruct what was commanded, so FMEA-M03-02's cross-check has no record to be audited against (`OI-FMEA-09` (a)) | `np_session_runner.c`, `np_log_command()`; `np_log_backend_tests` (layout, UHDR-only) |
| `REQ-FWHUB-40` | **(Rev 9)** Session start makes an SHDR `SESSION_OPEN` marker and the UHDR start record durable before any command is dispatched. *Fails without it:* an unclean session end cannot be told from a clean one in a truncated log, and a reader infers state from an absence (CLAUDE.md §5.1; `OI-FMEA-09` (c)) | `np_log_session_start()`; `np_log_backend_tests` (synced before return, OPEN/END pairing) |
| `REQ-FWHUB-41` | **(Rev 9)** Delivered BES/tACS or tDCS current exceeding commanded raises an SHDR divergence flag: once per channel per session, a flag with no magnitude or timestamp, and not raised by a ramp-down the driver is still running. *Fails without it:* FMEA-M03-02's residual score rests on a control that does not exist (`NP-FMEA-001` §3.3). The thresholds are placeholders and are not part of this requirement (§8.3.1) | `np_stim_xcheck.c`; `np_stim_xcheck_tests`, `np_mod_stim_tests` |
| `REQ-FWHUB-28` | **(Rev 7)** The wire format has a mechanical agreement check against `hubCompiler.ts`, falsified in both directions per `NP-CONV-001` §8. *Fails without it:* the two implementations agree only by inspection, and a length or offset drift surfaces on a device as `INVALID_ARG` | §4.6; `scripts/check-hub-wire-format.ts` (15 perturbation fixtures) |

### 10.2 Requirements the code does NOT currently meet

Stated separately and deliberately. A specification written after the code that quietly matched the
code everywhere would be describing, not specifying.

| ID | Requirement | Gap | Item |
|---|---|---|---|
| `REQ-FWHUB-25` | A verified socket-addressed command reaches the addressed emitters | **Rev 2: the path exists (§3.4, `REQ-FWHUB-31…34`) and every drive is refused at the power gate (§5.6).** Was: `dispatch_command()` dropped every non-`SLOT` target kind (`OI-FWHUB-01`, closed) | **`OI-FWHUB-09`** (blocking) |
| `REQ-FWHUB-26` | Every modality in CLAUDE.md §3's T1 roster has a dispatchable path | as `REQ-FWHUB-25`: transcranial PBM is dispatchable and not admitted | **`OI-FWHUB-09`** |
| `REQ-FWHUB-27` | Every source file's `Document:` banner cites a revision of this document that exists | three files cite Rev 2 | `OI-FWHUB-02` (fixed in this change) |
| `REQ-FWHUB-03` | Module detection does not probe while a session is running | **Rev 8:** held by a state check re-read per slot, not by exclusion — a session starting between the check and the probe is not prevented. Vacuously met before Rev 8, because every accessory rescan was refused | **`OI-FWHUB-17`** |
| ~~`REQ-FWHUB-28`~~ | *Moved to §10.1 in Rev 7 — met.* | — | `OI-FWHUB-03` (closed) |

### 10.3 Design review checklist

`FWHUB-DRC-01` bring-up order asserted by a test, not by reading · `FWHUB-DRC-02` heartbeat worst-case
latency measured under full session load · `FWHUB-DRC-03` transport backpressure exercised with a
pipelining client · `FWHUB-DRC-04` every §4.4 rejection has a negative test ·
`FWHUB-DRC-05` wrapping `start_ms + duration_ms` rejected, proven by injection ·
`FWHUB-DRC-06` `cmd_count` under-count with trailing STOP commands rejected, proven by injection ·
`FWHUB-DRC-07` pause/resume does not reset the MCU charge accumulator ·
`FWHUB-DRC-08` geometry gate fails closed for a tDCS command declaring no area ·
`FWHUB-DRC-09` `SIG_PENDING` still set after send → session aborts and no enable is requested ·
`FWHUB-DRC-10` a pre-lockout cVNS confirmation is rejected and not queued ·
`FWHUB-DRC-11` no SHDR record in the tree carries a timestamp — grep, not review ·
`FWHUB-DRC-12` `NP_MODE_F_REGULATORY_CLEARED` is 0 in every build configuration ·
`FWHUB-DRC-13` the bring-up gate is run by CI and its self-test with it — a gate nothing runs is `OI-DOC-01`'s shape in miniature.

---

## 11. Decisions

| # | Decision | Rationale in |
|---|---|---|
| D-1 | Four tasks, heartbeat highest, detection lowest and session-suspended | §2.2 |
| D-2 | A hub fault stops the heartbeat rather than attempting recovery | §2.4 |
| D-3 | A module `control()` failure does not abort the session | §2.4 |
| D-4 | Single-slot transport mailbox, not a queue | §2.3 |
| D-5 | Slot numbers and enable-bit positions are append-only | §3.1 |
| D-6 | BES/tACS and tDCS are two slots on one driver | §3.1 |
| D-7 | Socket targeting is a 128-bit bitmap, index space, not an address list | §4.3 |
| D-8 | A parser leniency that could produce an unstopped session is rejected | §4.4 |
| D-9 | A parameter-struct length change bumps the protocol version | §4.5 |
| D-10 | `ACTIVE` covers `PAUSED` so a pause cannot zero the charge accumulator | §5.1 |
| D-11 | Electrode **area** crosses to the MCU; the density constant never leaves it | §5.4 |
| D-12 | Two separate geometry gates, not one widened gate | §5.4 |
| D-13 | Enables are dropped before the graceful module ramp, not after | §5.5 |
| D-14 | A dropped command does not appear in `mods_active_mask` | §5.6 |
| D-15 | SHDR timestamp suppression is unconditional | §6.3 |
| D-16 | Adaptation events are UHDR-only, scaled integers, frozen enum | §6.4 |
| D-17 | The log backend writes plaintext; encryption is at the mount | §6.5 |
| D-18 | The heartbeat transmits the requested mask | §7.2 |
| D-19 | The MCU verifies the session signature independently of the hub | §7.3 |
| D-20 | cVNS re-enable has one bit-emitting state and a bounded assertion | §7a |
| D-21 | Audio is deliberately not safety-MCU gated | §8.5 |
| D-22 | Mode F is compile-time gated, not runtime | §8.6 |
| D-23 | This document is issued at Rev 1 dated today, not back-dated to 2026-05-16 | banner |
| D-24 | Socket dispatch is a separate registry beside the slot registry, with no fallback between them | §3.4 |
| D-25 | A socket drive command is all-or-nothing across placement, power and driver faults | §3.4 |
| D-26 | The dispatcher owns `NP_SAFETY_EN_PBM_CRANIAL` and releases it only when no socket is active, except on a failed stop | §3.4 |
| D-27 | The power governor ships refusing every load rather than as a plausible approximation | §5.6 |
| D-28 | **(Rev 3)** BES/tACS is gated on its own bit, with its area the fixed pad constant rather than an authored field — no wire-format change while T1 tES stays on pads (`NP-FW-MMSOCK-001` P-1) | §5.4 |
| D-29 | **(Rev 3)** The runner's geometry scan is a pure, host-tested function rather than inline in an ARM-only loop | §5.4 |
| D-30 | **(Rev 8)** The accessory rescan acts only on a presence or type change, and does not retry a failed `init()` until the module is re-seated. Retired zone slots 0–4 are refused, not re-probed | §3.2 |
| D-31 | **(Rev 8)** Detection runs only in `IDLE`, `COMPLETE` and `FAULT`, and re-reads the state before each slot — not only outside `RUNNING` | §2.2 |

---

## 12. Risk register

| ID | Hazard | Sev | Mitigation | Residual |
|---|---|---|---|---|
| `RISK-FWHUB-01` | A socket-addressed command is dispatched through the slot path and delivers stimulation to a site the protocol never named | **High** | **Rev 2:** `dispatch_command()` routes by target kind to two registries with no fallback (`REQ-FWHUB-31`); the parser independently rejects retired slots; `np_socket_dispatch_tests` asserts the socket registry refuses a slot-kind command | **None on wrong-site.** The residual is still no dose at all — now at the power gate (`OI-FWHUB-09`) |
| `RISK-FWHUB-12` | An admitted lattice load exceeds the negotiated PD contract and browns out the rail | **High** | governor ships closed (§5.6, `REQ-FWHUB-35`) — **no drive is admitted**, so the hazard is not reachable on this firmware | **Open until `OI-FWHUB-09`** — becomes reachable the moment that function body admits anything |
| `RISK-FWHUB-13` | One command's stop cuts the lattice gate under another command's running sockets, and UHDR records their undelivered dose | Medium | cranial bit reference-counted by active sockets (`REQ-FWHUB-34`) | Accepted |
| `RISK-FWHUB-14` | A command reaches only the capable subset of its sockets and UHDR records the full protocol | Medium | placement checked for every socket before any is driven; driver faults roll back (`REQ-FWHUB-32`) | Accepted |
| `RISK-FWHUB-02` | Hub hangs with stimulation enabled | High | MCU watchdog cuts in < 50 ms on heartbeat loss; §2.4 forbids a beating fault path | Accepted (Class C backstop) |
| `RISK-FWHUB-03` | A protocol runs with no stop, from a wrapped deadline or an under-counted `cmd_count` | High | both rejected at parse (§4.4); `FWHUB-DRC-05/06` | Accepted with the checks in place |
| `RISK-FWHUB-04` | Charge limit circumvented by pause/resume | High | `ACTIVE` covers `PAUSED` (§5.1) | Accepted |
| `RISK-FWHUB-05` | tDCS runs against the 25 cm² default with small electrodes | High | geometry gate arms even when no area is declared, so the MCU never grants (§5.4) | Accepted |
| `RISK-FWHUB-06` | A cardiac re-enable happens before the MCU lockout expires | High | hub window strictly contains the MCU window; MCU denies independently (§7a) | Accepted |
| `RISK-FWHUB-07` | An SHDR record discloses user biology by redaction *shape* | Medium | unconditional suppression (§6.3); `check-redaction-shape.ts`; `FWHUB-DRC-11` | Accepted |
| `RISK-FWHUB-08` | Log records lost on power loss | Low | bounded to one flush interval (§6.5) — **the bound is now exercised against the `lfs_config` contract and still not against the medium.** `NP-SOUP-LFS-001` Rev 2 pins and vendors littlefs `v2.11.3` (closing `OI-LFS-01`), which resolves "not in the tree" and resolves nothing about the bound; `OI-LFS-02` now blocks reliance on it. The log partitions' own instance parameters are additionally unstated — `OI-LFS-05` | **Open until `OI-LFS-02`** **Update 2026-09-14: `OI-LFS-02` closed** — `NP-SOUP-LFS-001` §12, 90 interrupted runs on the flushed-append path, 0 violations, falsified in both directions. What remains open is narrower and is stated as such: the eMMC behind the XTS layer (`OI-LFS-07`), the log partitions’ own instance parameters (`OI-LFS-05`), and a post-power-loss hang, which a sweep cannot see. |
| `RISK-FWHUB-09` | Emission into a lifted goggle | High | Hall cutoff is a GPIO interrupt, plus three independent layers (§8.6) | Accepted |
| `RISK-FWHUB-11` | Boot-time module authentication is not evidenced in fleet telemetry | Low | **was unmitigated — the records were discarded.** Fixed 2026-09-14 (§2.1) and held by `scripts/check-hub-bringup-order.ts`, falsified against the pre-fix commit | Accepted; records-integrity only, no emission path |
| `RISK-FWHUB-10` | Per-tile PBM drive magnitude bounded only by a thermal cutoff | Medium | carried, not closed — `OI-NVRAM-10`; re-derive §9 before a differing tile variant ships. **2026-09-24: control decided** — a hardware peak-current reference and gate-duty limit per tile channel (`NP-HW-HEXTILE-001` D-9, `REQ-TDRV-01`/`-02`, `NP-SOUP-LFS-001` §13.14). This row closes when `OI-HEXTILE-24` verifies them on hardware | **Open** (control decided, not built) |
| `RISK-FWHUB-15` | **(Rev 3)** A fixed electrode area (VNS 0.5 cm², cervical VNS 2 cm², BES 25 cm²) is computed and not sent, so the Class C per-phase ceiling is enforced against the 25 cm² fallback | Medium — **was live** in every session without HD-tDCS or tDCS; no rated dose reached even the intended ceiling (VNS ~3 %) | **Fixed in Rev 3**: every computed area is sent (`REQ-FWHUB-37`), held by `np_chan_decl_tests`, falsified against the old rule | Accepted |
| `RISK-FWHUB-16` | **(Rev 3)** BES/tACS reaches an electrode smaller than 25 cm² and is enforced against the fallback | High | **not reachable** — tES is not socket-addressable (`REQ-FWHUB-33`) | own geometry gate, fail-closed (`REQ-FWHUB-36`); a lattice electrode's area arrives with a tES socket target (`NP-FW-MMSOCK-001` P-2) | Accepted pending SW-01 review |

---

## 13. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| ~~**`OI-FWHUB-01`**~~ | ✅ **CLOSED 2026-09-23 (Rev 2).** *Was:* transcranial PBM had no dispatchable path — `np_mod_pbm_*` reachable only through the rejected slots 0–4, socket commands dropped by `dispatch_command()`. **Resolved by the socket dispatch registry (§3.4)**, the firmware half of `OI-HUB-SOCKET-01`, with `np_socket_dispatch_tests` (Class B 30 → 31). **The blocking status moves rather than lifts:** every drive is refused at the power gate until `OI-FWHUB-09` | — (closed) | — |
| **`OI-FWHUB-09`** | **The concurrent-power governor `np_pbm_power_admit()` refuses every load.** Deliberately (§5.6). Replace its body with the watts-against-PD-contract governor `NP-HW-HEXTILE-001` §9.3 requires. Needs: `OI-HEXTILE-09` designed; `OI-SESPWR-03` (`0Hz` + duty) defined; a PD-contract seam on SW-02; per-tile watts from selected emitters (`OI-HEXTILE-02`). The compiler-side half of the same check is `OI-HEXTILE-09`'s, not this document's. When it lands, `REQ-FWHUB-35`'s second clause is rewritten, not deleted, and `RISK-FWHUB-12` is re-scored | FW + EE Lead | **BLOCKING — any T1 transcranial PBM session; `REQ-FWHUB-25`, `-26`** |
| **`OI-FWHUB-10`** | **Socket-path telemetry and dose metering do not exist.** The runner's telemetry loop and `np_telem_pbm_t` are slot-indexed (five zone entries); no per-socket NTC, PD1/PD2 or J/cm² reaches SHDR or UHDR, and the slot path's pre-drive NTC over-temperature check has no socket equivalent. Needs `np_hub_cluster_read_frame()` (`NP-HW-HUB-001` §9.3) and a socket-indexed telemetry record. The 42 °C / 62 °C hardware limits are unaffected — they are enforced below this processor. **Blocks CLAUDE.md §3's dual-PD dose-metering claim for the lattice, not safety.** *Rev 7:* also owns whether a per-socket **inventory** record reaches SHDR (moved here from `OI-FWHUB-05`) — a new SHDR field, so classified under CLAUDE.md §5.1 first. The PBM HAL stub now serves PD reads over the socket domain (`OI-FWHUB-12`), so the accessor this item needs no longer stops at socket 4 | FW | Before `OI-FWHUB-09` admits a load |
| **`OI-FWHUB-11`** | **`HUB-REQ-C05` is not implemented.** `NP-HW-HUB-001` §7.2.2 requires the per-cluster Class B `SAFE_EN[n]` gate to be commanded by this processor, not by the cluster controller it gates. The dispatcher does not command it: that needs the `socket_id → (cluster, channel)` table (`OI-HUB-C10`, not yet generated) and a settled gate polarity (`OI-RISK4-01`). Availability only — the Class C cranial bit is in series | FW + EE Lead | Hardware bring-up |
| **`OI-FWHUB-13`** | **The per-session recording limit must reach the user documentation.** §6.6 states it (about 49 hours per session; recording stops, the session does not) and gives proposed IFU text, but no IFU or in-app help document exists yet (`NP-QMS-DC-001`: labelling and IFU *TBD*). When that document is created, §6.6's text goes into it, and the figure is re-derived if `file_max` or the EEG data rate changes | FW + Regulatory | IFU authoring |
| ~~**`OI-FWHUB-12`**~~ | ✅ **CLOSED 2026-09-25 (Rev 7).** *Was:* `firmware/pbm/src/np_pbm_hal.c` bounded its I²C shadow and PD reads to `slot < 5`, so a smart tile above socket 4 failed its driver startup (`NP_PBM_ERR_I2C_WRITE`) and metered no dose. The stub now takes a socket index over `NP_PBM_SOCKET_DOMAIN` (128, derived from `NP_PBM_SOCKET_MASK_BYTES`). `np_pbm_hal.h` records that the tunnelled HAL replacing it keeps the socket index (`NP-HW-HUB-001` §9.2). `np_pbm_session_desc_tests` drives socket 77 end to end and refuses 128, and fails four checks against the pre-fix stub. `np_module_map_tests` pins `NP_PBM_SOCKET_DOMAIN == NP_HEXMAP_MAX_SOCKETS` | — (closed) | — |
| **`OI-FWHUB-14`** | **The EEG sample path's documented calling context is incompatible with the logger.** §8.2 and `np_mod_eeg.c` say `np_log_eeg_sample_block()` is called from the DMA ISR. That function now drains the adaptation ring and `s_uhdr_buf` (§6.5, Rev 5), and it has always appended to the backend's staging. All three are shared with the task-side logger and none is synchronized. From an ISR, a block landing during a task's `uhdr_write()` corrupts the record under construction. No caller exists, and the function is not in the linked image. Decide the hand-off: an ISR-to-task queue that calls the logger from the session runner's context, or a lock that the logger's hot path can afford | FW | **Any caller of `np_log_eeg_sample_block()` (EEG waveform logging)** |
| ~~**`OI-FWHUB-15`**~~ | ✅ **CLOSED 2026-09-24 (Rev 6)** — `np_log_session_end()` drains the adaptation ring before the session-end record (§6.5); pinned by `np_log_backend_tests`. *Was:* **Adaptation events still queued when a session ends are lost.** `np_session_runner` calls `np_log_session_end()` and then `np_log_flush()`. The session-end call closes the session's UHDR file (`OI-LFS-11`). The flush then drains the adaptation ring into `s_uhdr_buf`, and its append is refused with `NP_HUB_ERR_NO_SESSION`. The next session start discards that buffer before it opens its own file. The events reach no file, and nothing reports the loss. Before `OI-LFS-11` they landed after the session-end record in the single UHDR file: out of order, but kept. Likely fix: drain the ring at the top of `np_log_session_end()`, so the events precede the session-end record | — (closed) | — |
| ~~**`OI-FWHUB-16`**~~ | ✅ **CLOSED 2026-09-24 (Rev 8)** — `np_mod_reg_rescan_slot()` accepts slots 5–18 and re-initialises only on a change (§3.2, D-30); pinned by `np_module_registry_tests`, which fails 120 checks against the old registry. *Was:* **no accessory attached after boot was ever registered.** `task_module_detect` looped slots 7–18 through `np_mod_reg_rescan_zone()`, which returned `INVALID_ARG` for any slot ≥ 5, and discarded the return. Only the boot scan saw goggles, the VNS clip, the intranasal probe, cervical VNS or a T2 unit. Nothing caught it because the registry had no host test and the caller is ARM-only | — (closed) | — |
| **`OI-FWHUB-17`** | **Detection is kept off a session by a state check, not by exclusion (`REQ-FWHUB-03`).** `task_module_detect` is the lowest priority. A session can start between its state read and a `detect()`, and the registry entry it rewrites is read by `task_hub_control` without a lock. Rev 8 re-reads the state per slot and orders the entry writes so a reader sees `NULL` rather than a torn entry, which narrows the window to one probe. Close it with a lock the runner holds from `np_runner_load()` to session end, which the detect task takes around each rescan. Dormant until Rev 8, because every accessory rescan was refused | FW | **Hardware bring-up** — before any accessory `detect()` drives a line a session owns |
| **`OI-FWHUB-18`** | **The retired zone-announce hooks are dead code.** `np_hub_zone_insert_cb()` / `np_hub_zone_remove_cb()` have no caller (`np_za_init()` is never called by the hub) and no declaration. Rev 8 made the insert hook a documented no-op instead of letting it call a rescan that now refuses slots 0–4. Delete both with the `firmware/zone_announce/` ZONE_ID path, or wire them to the socket lattice if that path returns | FW | — |
| ~~`OI-FWHUB-02`~~ | ✅ **CLOSED 2026-09-13 in the same change.** Three files cited `NP-FW-HUB-001 Rev 2` against a document with no Rev 1. Re-pointed to Rev 1 §8.9 and §6.4 | FW | — |
| ~~**`OI-FWHUB-03`**~~ | ✅ **CLOSED 2026-09-25 (Rev 7).** *Was:* no mechanical agreement check between §4 and `hubCompiler.ts`. **§4.6** now tabulates the per-modality blocks. **`scripts/check-hub-wire-format.ts`** diffs §4's constants and table, the `np_hub_config.h`/`np_hub_types.h` defines, enums and packed-struct layouts, and **the compiler's own output**, which it runs for all 16 modality encodings and decodes at the firmware's field offsets. Falsified on 15 single-corner perturbations (a field reordered, a struct grown or unpacked, a table byte count, a compiler buffer one byte long, a stale compiler version, and so on). The self-test passes an unperturbed copy first. Wired as `tooling-ci.yml:hub-wire-format` (`REQ-FWHUB-28`) | — (closed) | — |
| ~~**`OI-FWHUB-04`**~~ | ✅ **CLOSED 2026-09-25 (Rev 7) — reordered, by Rev 5.** *Was:* on overflow, `uhdr_write()`/`shdr_write()` flushed *before* appending the full buffer, so the sync covered none of it. **Fixed in Rev 5 (#425):** every path now appends and then syncs through `uhdr_drain()`/`shdr_drain()`, and `np_log_backend_tests` covers it (§6.5). Rev 5 left this row open, so Rev 7 records the closure. No further code change | — (closed) | — |
| ~~**`OI-FWHUB-07`**~~ | ✅ **CLOSED 2026-09-14.** Boot-time SHDR zone-auth records were written before the logger was initialised — stamped with session count 0, then discarded by `np_log_init()`'s `s_shdr_pos = 0U`. `np_log_backend_init()` and `np_log_init()` now precede `np_mod_reg_scan()`, which keeps `np_safety_spi_init()`'s `GAIN_SEL` precedence intact, and the misleading comment is corrected. Held by `scripts/check-hub-bringup-order.ts`, **falsified against the pre-fix commit**, not only fixtures | — (closed) | — |
| ~~**`OI-FWHUB-08`**~~ | ✅ **CLOSED 2026-09-14.** The banner and the register said "four tasks"; the code creates five. All three corrected, and the banner's count is now **counted against `xTaskCreate()`** by the same gate rather than read | — (closed) | — |
| ~~**`OI-FWHUB-05`**~~ | ✅ **CLOSED 2026-09-25 (Rev 7).** *Was:* the scan's SHDR auth callback fired per zone slot. **Answer: no records are expected for slots 0–4, and there is nothing to follow on the lattice** (§3.2). The retired slots' records were classified from a ladder Rev 3 hardware lacks (every boot logged five passes). Tiles are inventoried by UID, not authenticated. The callback and its parameter are removed from `np_mod_reg_scan()` / `np_mod_reg_rescan_zone()`. A per-socket SHDR inventory record, if wanted, is a new SHDR field and goes to `OI-FWHUB-10` | — (closed) | — |
| **`OI-FWHUB-06`** | **This document has no approver.** Issued as a design output with `Approved By` blank pending review against §10.3. `21 CFR §820.30(d)` expects design outputs to be reviewed and approved before release. **Rev 7: the labelling half is done.** The header Status, `NP-DHF-001` and the document register now say **DRAFT — pending approval** instead of RELEASED. **Open: the review and the approval**, which only the Principal can give | Principal | Design-control completeness |

---

## 14. Deliverable summary

**What the program is.** The SW-02 FreeRTOS application on the i.MX RT1062: four tasks, one
reassembling transport, one signed-descriptor parser, one slot registry, one session runner, one
UHDR/SHDR logger, one SPI channel to a Class C safety MCU that owns every enable line, and nine
module drivers behind a five-function interface.

**What decides everything.** The hub *requests*; the safety MCU *grants*. Every design choice worth
writing down is downstream of that: why the heartbeat carries the requested mask and not the granted
echo, why electrode area crosses the boundary and the density constant does not, why a hub fault
stops the heartbeat instead of recovering, and why this program can be Class B at all.

**Two rules the code follows that a reader would not guess.** Slot numbers are append-only because a
slot number *is* a Class C charge-monitor channel index. And SHDR timestamp suppression is
unconditional because a conditional redaction leaks its own predicate — the programme has already
paid once for learning that.

**The gap this document exists to make visible.** The socket lattice replaced the zone slots
everywhere except in dispatch. The parser followed, the wire format followed, the module map
followed; `dispatch_command()` did not. The two halves each fail closed, correctly — and their
composition is that **no transcranial PBM protocol can execute on the shipped firmware.** Nothing in
the repository said so before this document, because the document that would have said it did not
exist. That is the shape of the cost `OI-DOC-01` was tracking, and it is why option (1) — author the
specification — was the right resolution rather than retiring the number.

**What writing it down was worth, concretely.** Two live defects in `np_hub_control_app_main()` — a
bring-up ordering that silently discarded every boot-time SHDR zone-auth record, and a banner that
undercounted the task set — neither of which any test could have found, because that function is
ARM-cross-only and nothing had ever read it. Both are fixed here, and both are now held by
`scripts/check-hub-bringup-order.ts`, falsified against the pre-fix commit rather than against
fixtures alone.

**What Rev 2 changed.** The registry the gap named now exists (§3.4): socket commands are dispatched,
all-or-nothing, through their own registry, which owns the cranial enable. **And transcranial PBM still
does not run** — because the same change that removed the barrier exposed the power governor
`NP-HW-HEXTILE-001` §9.3 had always required, which cannot yet be written, and which therefore ships
refusing everything (§5.6, `OI-FWHUB-09`). A gap that had two halves, each documented and neither
connected, is now one function body with its reason written above it.

**What Rev 7 changed.** The wire format's two implementations are now diffed rather than trusted
(§4.6, `scripts/check-hub-wire-format.ts`). The boot scan no longer sends SHDR five authentication passes for zone
modules that cannot exist. And the PBM stub can address a smart tile above socket 4. None of this
lets transcranial PBM run.

**What is deliberately not here.** No verification, no approval, and no requirement the code does not
meet except the four in §10.2 that are marked unmet and carried as open items — chiefly
`OI-FWHUB-01`, which is a capability absence and genuine design work, not a defect this change could
have absorbed.

---

## 15. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 10 | 2026-09-25 | NeurOne Firmware Engineering | **Signal name corrected; nothing else changes (GitHub #437).** The References line cited `NP-HW-HUB-001` §7.2's active-low Class C cranial enable as `PBM_CRANIAL_EN`, without the `#` that `NP-CONV-001` §1.1 requires. It now reads `PBM_CRANIAL_EN#`. `NP-RISK-004` §2.2 cites the dropped `#` as evidence that two names differing only in `#` get confused, which is why its proposed name for the inverting-buffer output is `PBM_CRANIAL_PERMIT`. No requirement, figure or firmware changes. |
| 9 | 2026-09-25 | NeurOne Firmware Engineering | **GitHub #386, `OI-FMEA-09` (a)–(c) built.** (a) UHDR `0x19` commanded-dose record per dispatched command (`REQ-FWHUB-39`). (b) **New §8.3.1**, `src/np_stim_xcheck.c`: the commanded-versus-delivered cross-check for BES/tACS and tDCS, with one SHDR flag `0xD1` per channel per session and the tDCS ramp-down held (`REQ-FWHUB-41`). (c) SHDR `0x86` `SESSION_OPEN`, synced with the UHDR start record before dispatch (`REQ-FWHUB-40`). New host target `np_stim_xcheck_tests` (Class B 35 → 36); `np_mod_stim_tests` +3, `np_log_backend_tests` +10 and two overflow cases rebased on the now-empty start buffer. Mutation-tested, nine single faults, each caught. **Not closed:** placeholder thresholds, the `OI-STIM-06` read is a stub, VNS/cVNS/T2 uncovered, flag-only (no cutoff). All four stay on `OI-FMEA-09` |
| 8 | 2026-09-25 | NeurOne Firmware Engineering | **Closes `OI-FWHUB-16`: accessories attached after boot are registered again (§2.2, §3.2, §10, §11, §13).** `task_module_detect` re-probed slots 7–18 through `np_mod_reg_rescan_zone()`, which refused any slot ≥ `NP_HUB_ZONE_SLOT_COUNT` (5), and it discarded the `INVALID_ARG`. Only the boot scan ever registered goggles, the auricular VNS clip, the intranasal probe, cervical VNS or a T2 unit. **Renamed `np_mod_reg_rescan_slot()`**, since it is no longer zone-specific. It accepts `NP_HUB_SLOT_FIRST_VALID`..`NP_HUB_SLOT_MAX − 1`, refuses the retired zone slots 0–4 without probing, and re-initialises only on a presence or type change (D-30). A failed `init()` is not retried until the module is re-seated. Widening the bound alone would have flooded SHDR: the intranasal and cervical VNS `init()` each write an auth record, and the poll runs every 500 ms. It would also have re-run ADS1299 self-calibration and zeroed both stimulation channels every poll. `task_module_detect` now probes only in `IDLE`/`COMPLETE`/`FAULT`, re-reads the state before each slot (D-31), and asserts it never receives `INVALID_ARG` again. `np_hub_zone_insert_cb()` is a documented no-op (`OI-FWHUB-18`). **New host target `np_module_registry_tests`** (Class B 34 → 35, repo total 44 → 45, re-derived with `ctest -N`): the production registry against per-slot driver doubles. **Falsified:** built against the Rev 7 registry with `-DRESCAN_FN=np_mod_reg_rescan_zone`, it fails 120 checks. `np_hub_control_main.c` and `np_module_registry.c` cross-compile for Cortex-M7 with `-Werror`. `REQ-FWHUB-38` added. **Named downside:** `REQ-FWHUB-03` held vacuously while every rescan was refused. It is now live and held only by a state check, so it moves to §10.2 as not fully met (`OI-FWHUB-17`, blocking hardware bring-up). `np_mod_reg_rescan_slot()` takes the slot only, matching Rev 7's removal of the SHDR callback from `np_mod_reg_scan()`. |
| 7 | 2026-09-25 | NeurOne Firmware Engineering | **Issue #384, non-blocking half: closes `OI-FWHUB-03`, `-05` and `-12`; records `-04` closed (fixed by Rev 5, #425); does the labelling half of `-06`.** **New §4.6**: per-modality code, parameter struct, byte count and target. **`scripts/check-hub-wire-format.ts`** diffs §4, `np_hub_config.h`/`np_hub_types.h` (defines, enums, packed-struct sizes and offsets) and `hubCompiler.ts`'s actual output, decoded at the firmware's offsets for all 16 encodings. Falsified on 15 single-corner perturbations and wired as `tooling-ci.yml:hub-wire-format`, so `REQ-FWHUB-28` moves to §10.1. **`np_module_registry`**: the per-zone-slot SHDR auth callback and its parameter are removed, because the records were made up from a retired ladder (§3.2, `-05`); §2.1 row 7 and the bring-up gate's rationale follow. **`firmware/pbm`**: the HAL stub is addressed over `NP_PBM_SOCKET_DOMAIN` (128), and `np_pbm_session_desc_tests` drives socket 77 (`-12`). **Status line → DRAFT — pending approval** (`-06`; approval still open). `OI-FWHUB-10` gains the per-socket SHDR inventory question. No requirement weakened; `REQ-FWHUB-25/26` remain unmet against `OI-FWHUB-09` (#335). Rev 6 → 7. |
| 6 | 2026-09-24 | NeurOne Firmware Engineering | **Closes `OI-FWHUB-15` (§6.5, §13).** `np_log_session_end()` now calls `np_adapt_log_flush()` before it writes the session-end record and closes the UHDR file. Adaptation events still queued when a session ends therefore land in that session's file, ahead of its session-end record. Before, the runner's following `np_log_flush()` drained them into a closed file, the append was refused (`NP_HUB_ERR_NO_SESSION`), and the next session start dropped the buffer, with nothing reporting the loss. `np_log_backend_tests` gains 1 case, run in the runner's order (end, then flush). Removing the drain fails its two positive checks. A third check, that nothing reaches the next session's file, guards against a fix that moves the events there. Host suite: 43/44; `np_lfs_log_instance_tests` fails on unmodified `main` too. The ARM cross-build is clean. No classification, record format or wire format changed; no new test target. |
| 5 | 2026-09-24 | NeurOne Firmware Engineering | **The session logger appends before it syncs and keeps log order (§6.1, §6.5, §8.2, §13).** Two defects in `np_session_log.c` predate `OI-LFS-11` and were found during it (NP-SOUP-LFS-001 Rev 7 §13.10). (1) A full 4 KiB UHDR or SHDR buffer was synced and then appended, so the sync covered none of it. Both partitions now go through `uhdr_drain()`/`shdr_drain()`, which append and then sync. (2) EEG sample blocks were appended straight to the HAL ahead of records in `s_uhdr_buf` and the adaptation ring. They now drain both first, append only, and keep the zero-copy path for the samples. (3) Found by (2)'s test: `np_log_flush()` skipped the UHDR sync whenever the buffer was empty, so EEG data could miss the 30 s bound. It now always syncs UHDR. `np_log_backend_tests` gains 4 cases and the host hook `np_log_test_synced_len()`. Reverting each fix fails the case named for it (five mutants), and the pre-fix file fails 5 checks (`NP-CONV-001` §8). Host suite and ARM cross-build pass; the one other ctest failure, `np_lfs_log_instance_tests`, fails identically on the unmodified base (3 of 3 under ctest; it passes most standalone runs, and one crashed with a bus error) and links none of these files. Raised `OI-FWHUB-14` (EEG ISR context vs a single-context logger) and `OI-FWHUB-15` (adaptation events queued at session end are lost). No classification, record format or wire format changed; no new test target. |
| 4 | 2026-09-24 | NeurOne Firmware Engineering | **§6.6 added — the per-session recording limit, with proposed IFU text.** `OI-LFS-11` (`NP-SOUP-LFS-001` Rev 7 §13.10) made each session's UHDR record one file, and littlefs caps a file at 2 GiB − 1: about **49 hours** of recording with EEG at ≈12 kB/s. At the limit recording stops for the rest of that session; the session is not stopped, and other sessions are unaffected. §6.6 derives the figure, states the behaviour from the code, and gives plain-language IFU text. No IFU exists yet, so `OI-FWHUB-13` carries the text to it. §6.5 gains the `OI-LFS-11` update note. Rev 3 → 4. |
| 3 | 2026-09-23 | NeurOne Firmware Engineering | **BES/tACS geometry gate (`NP-FW-MMSOCK-001` P-5, C-1) and the area-hand-off defect.** New `NP_SESSION_STATUS_GEOM_REQ_BES` (bit 4, previously unused) and a third arm of the safety MCU's geometry gate — **Class C, pending SW-01 review**; no enable-word bit and no frame byte moves. The hub arms it for every BES/tACS session and sends the fixed pad area (D-28; `REQ-FWHUB-36`). **Found while writing it:** the area frame was sent only in sessions holding HD-tDCS or tDCS, so VNS, cervical-VNS and BES areas were silently dropped elsewhere and the MCU enforced 25 cm² — 50× loose on the auricular clip (`RISK-FWHUB-15`, fixed; `REQ-FWHUB-37`). The scan moved to `src/np_chan_decl.c` (D-29) with `np_chan_decl_tests` (Class B 31 → 32, total 39 → 40); every mutation of the send rule, the BES arming and the two Class C gate lines is caught. §5.4, §7.4, §10.1, §11, §12 updated. Cross-compiled for both processors on arm-none-eabi-gcc 13.2.1. |
| 2 | 2026-09-23 | NeurOne Firmware Engineering | **Closes `OI-FWHUB-01` — the socket dispatch registry — and moves its blocking status to the power governor rather than lifting it.** New §3.4: `src/np_socket_dispatch.c`, a 128-entry socket-indexed registry beside the slot registry, with no fallback between the two (`REQ-FWHUB-31`); admission is all-or-nothing across mod-type, params, placement against the live `np_module_map` inventory, power, and driver faults with rollback (`REQ-FWHUB-32`, `-33`); the registry owns `NP_SAFETY_EN_PBM_CRANIAL`, requesting it after a command's sockets are configured and releasing it only when none is active, or at once when a stop fails (`REQ-FWHUB-34`). Driver seam: `np_mod_pbm_socket_drive()` / `_stop()` in `np_mod_pbm.c`, and one new platform seam `np_mod_pbm_hal_socket_pwm_set()` (SW-02 census 97 → 98). **§5.6 rewritten: opening the path makes `NP-HW-HEXTILE-001` §9.3's governor requirement live** — `scripts/check-pbm-power.ts` reads 20 of 23 predefined transcranial protocols over the 40 W budget — and that governor cannot be written (`OI-HEXTILE-09`, `OI-SESPWR-03`, `OI-HEXTILE-02`), so `np_pbm_power_admit()` ships **refusing every load** (`REQ-FWHUB-35`, D-27). **Transcranial PBM therefore still does not execute**; `OI-FWHUB-09` (blocking) replaces `OI-FWHUB-01` as the reason, and `REQ-FWHUB-25/26` stay in §10.2 against it. New `NP_HUB_ERR_POWER_BUDGET` (−18). `np_socket_dispatch_tests` (Class B 30 → 31, total 38 → 39): 14 cases linking the real module map and socket expansion, and the production governor renamed so what ships is asserted closed; seven mutations of the dispatcher and governor each caught (three survived the first draft of the suite and each gained a case). Raised: `OI-FWHUB-10` (socket telemetry and dose metering), `-11` (`HUB-REQ-C05` cluster gate not commanded), `-12` (the PBM I²C stub's five-slot bound). `OI-FWHUB-05` unblocked. Risks `RISK-FWHUB-12…14` added; `RISK-FWHUB-01` re-described. Decisions D-24…D-27. |
| 1 | 2026-09-13 | NeurOne Firmware Engineering | **Initial release — closes `OI-DOC-01` (Issue #339) by authoring the specification that had been cited as governing since 2026-05-16 without existing.** Written against `firmware/hub_control/` as on `main`, back-dating nothing: 26 requirements met by the code (§10.1), 4 explicitly **not** met and carried as open items (§10.2), 23 decisions, 11 risk rows, 13 design-review checks, 8 open items of which 4 close here. **Four findings that did not survive being written down:** (i) transcranial PBM — CLAUDE.md §3 modality ① — **has no dispatchable path at all**, because `np_mod_pbm_*` sits only in the five retired zone slots the parser rejects *and* socket-addressed commands are dropped by `dispatch_command()`; each half was individually documented and fail-closed, their conjunction was not (`OI-FWHUB-01`, blocking); (ii) three source files cited a `Rev 2` of a document that had no `Rev 1` — re-pointed to §8.9 and §6.4 in this change (`OI-FWHUB-02`, closed); (iii) the wire format has been revised twice (`slot_mask` → `slot_id` + target block; `electrode_area_mcm2`) while the register still described `Rev 1`, because **a register entry naming an unreadable document cannot go visibly stale**; and **(iv) writing the bring-up table found two live defects in `np_hub_control_app_main()`, and both are FIXED in this change** — `np_mod_reg_scan()` ran before `np_log_init()`, so every boot-time SHDR zone-auth record was stamped with a session count of 0 and then discarded when the logger zeroed its buffer, meaning module authentication reached SHDR not at all, under a source comment three lines away asserting the opposite (`OI-FWHUB-07`); and the file banner and the document register both said "four tasks" where the code creates five, `task_protocol_rx` having been omitted (`OI-FWHUB-08`). Also records that §4 is the specification `hubCompiler.ts` compiles against and that no mechanical check enforces their agreement (`OI-FWHUB-03`), contrary to `NP-CONV-001` §8. **Both fixes are gated, not merely applied:** `np_hub_control_app_main()` is ARM-cross-only and reachable by no host test — which is how a defect dating to 2026-05-16 survived — so `scripts/check-hub-bringup-order.ts` asserts the four ordering constraints and the task count against the function itself, and was falsified **against the pre-fix commit**, where it reports exactly those two violations (`NP-CONV-001` §8). Beyond those two fixes and three corrected `Document:` banners, no code behaviour changed. **§2.1, §2.2, §10, §12 and §13 were amended within this same unmerged change to describe the corrected code rather than the code as first found; Rev 1 is issued once, describing what merges.** |
