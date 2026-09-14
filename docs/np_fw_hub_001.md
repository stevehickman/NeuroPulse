# Hub Control Program — Firmware Specification

**Project:** NeurOne
**Document:** NP-FW-HUB-001
**Revision:** 1
**Date:** 2026-09-13
**Status:** RELEASED as a design output under `21 CFR §820.30(d)`. **Written against the firmware that exists**, not ahead of it — see the banner below for what that means and what it does not.
**Effective Date:** 2026-09-13
**Author:** NeurOne Firmware Engineering
**Approved By:** — (pending design review; `FWHUB-DRC-01…12` in §10.3 are the review checklist)
**References:** CLAUDE.md §1 (product tiers), §3 (modality roster and its hard limits), §4.1 (processor stack), §4.2 (safety architecture), §4.6 (operating modes), §5 (UHDR/SHDR), §17 (firmware carries no locale key); `NP-SW-001` Rev 5 §3.2 (SW-02 Class B rationale), §5.2 (SW-02 module inventory), §9.4 (SOUP); `NP-HW-HUB-001` Rev 4 §7.2–§7.2.2 (`PBM_CRANIAL_EN`, per-cluster gates, `HUB-REQ-C05`); `NP-FW-EMMC-001` Rev 2 §4 (partitions), §9 (Config/Calibration), §12 (session data classification); `NP-FW-EMMC-002` Rev 3 §C (UHDR key + mount), §F (Mode F); `NP-FW-CVNS-001` Rev 1 §6 (cardiac interlock); `NP-FW-NVRAM-001` Rev 2 §4 (power-loss atomicity), §9 (Class B argument); `NP-SOUP-LFS-001` Rev 1 (LittleFS SOUP record + hazard analysis); `NP-HEX-ZM-001` Rev 3 §7 (`OI-HUB-SOCKET-01`); `NP-NPPS-REF-001` (`.npps` grammar, the compiler's input); `NP-MOD-ID-001` Rev 1; `NP-PRIV-REM-001` Rev 3 STEP-33 (adaptation events); `NP-CONV-001` Rev 6 §4, §5, §6, §8; `firmware/hub_control/`; `app/web/src/lib/hubCompiler.ts`
**Related Issues:** #339 (`OI-DOC-01`), #69 (original implementation), #179, #298
**Gate:** G2-14 (SW-02 design outputs documented) — this document is the missing half of that gate.
**IEC 62304 Class:** **SW-02 Class B.** Argued in §9. The Class C item is the separate safety MCU program (`firmware/safety_mcu/`, `NP-SW-001` SW-01); nothing in this document is Class C and no change here may add a bit to a Class C wire format without SW-01 review.
**Supersedes:** None — new document. It does **not** supersede `NP-FW-REQ-001` (already SUPERSEDED) and does not absorb `NP-FW-CVNS-001`, `NP-FW-EMMC-001/002` or `NP-FW-NVRAM-001`, which remain the governing documents for what they specify.
**Parent Document:** `NP-SW-001`

---

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
>    `OI-FWHUB-01` — the only **blocking** item this document raises.
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
| 7 | `np_mod_reg_scan(shdr_zone_auth_cb)` | populates it, emitting one SHDR zone-auth record per probed slot |
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
fixed in the same change that issued this document.

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
| `task_module_detect` | 1 | 512 | polls for insertion/removal while idle; yields immediately while a session is `NP_SESSION_RUNNING` |

Receiving and running are **separate tasks on purpose**: `np_runner_run()` blocks for the whole
session, so a single task doing both could not accept the next protocol until the current one
finished, and the transport's single-slot mailbox (§2.3) would then backpressure for the session's
duration rather than for the handoff.

`REQ-FWHUB-02`: the heartbeat task holds the **highest** priority in the program and must never
block for longer than one heartbeat period. Its deadline is not a performance target — missing it
for `NP_SAFETY_WATCHDOG_MS` (1500 ms) is what makes the safety MCU cut every stimulation channel.
The priority ordering below it is a consequence: telemetry and detection exist to be preempted.

`REQ-FWHUB-03`: `task_module_detect` must not run a probe concurrently with a session. Probing
drives `GAIN_SEL` and reads impedance on lines a running session owns.

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

Every zone-slot probe emits one SHDR authentication record (pass or fail) through the
`np_mod_reg_shdr_cb_t` callback, wired in `np_hub_control_main.c` to `np_log_shdr_zone_auth()`. The
callback may be `NULL`, in which case auth events are skipped rather than buffered.

`np_mod_reg_rescan_zone()` re-probes a single slot on a hot-plug event without a full rescan.

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
write UHDR session-start record
send per-channel electrode geometry to the safety MCU   ← §5.4
loop:
    if NP_EV_SESSION_ABORT | NP_EV_SAFETY_FAULT  → break
    dispatch every command with start_ms ≤ elapsed_ms
    process_stops(elapsed_ms)
    every NP_RUNNER_TELEM_INTERVAL_MS: telemetry() each present slot → np_log_telemetry()
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

**Area only — never a pre-computed limit.** The 40 µC/cm² density constant stays resident on the
Class C safety MCU, which derives the limit itself. Sending a limit would move a Class C constant
onto the Class B side.

| Channel | Source of the area | Behaviour |
|---|---|---|
| `NP_SAFETY_CH_CLIN_STIM` (13) | implied by the HD-tDCS montage code — ring / bilateral 4×1 use 3.5 mm electrodes | fixed `NP_HD_SMALL_ELECTRODE_AREA_MCM2` = 96 milli-cm², **floored** so the derived limit never exceeds 40 µC/cm² |
| `NP_SAFETY_CH_TDCS` (6) | declared per protocol in the signed descriptor (`electrode_area_mcm2`) | **the smallest declared area wins** across several tDCS commands |

T1 tDCS pad area is not implied by anything else the descriptor carries — `electrode_pair` names
10-20 sites, not pad sizes — so it travels in the signed descriptor, and the app pre-flight and this
enforcer divide by the same declared number.

**The fail-closed gates are the point.** A tDCS command declaring **no** area leaves `area_mcm2` at 0
("keep default" to the MCU) *while still arming the tDCS geometry gate*, so the MCU never grants
`TDCS` at all. Silently running such a protocol against the 25 cm² fallback is the fail-**open**
behaviour `OI-CHARGE-03` rejected. The two gates are deliberately **separate flags**
(`set_geom_required` for `CLIN_STIM`, `set_geom_required_tdcs` for `TDCS`) because the gates are
per-channel: declaring tDCS geometry must not gate off clinical tACS, which shares the `CLIN_STIM`
enable bit and declares no geometry.

### 5.5 Shutdown

On completion or abort: `state := STOPPING` → `np_safety_spi_disable_all()` → `control(slot, NULL, 0)`
on **every** slot (modules ramp down internally) → wait `NP_RUNNER_SHUTDOWN_MS` (5 s) → flush all
stops → finalise the UHDR and SHDR records → `np_log_session_end()` → `np_log_flush()` →
`COMPLETE` or `FAULT`.

`np_safety_spi_disable_all()` comes **first**, before the module stops. The enable lines are the
thing that matters; the graceful ramp is a comfort and dose consideration layered on top of a cut
that has already been requested.

`REQ-FWHUB-12`: `abort_reason` is authoritative on `s_ctx`, not on the record. The session-end block
overwrites `shdr.abort_reason` from `s_ctx.abort_reason` unconditionally, so a driver writing the
record field directly has its value erased before it is ever logged.

### 5.6 What the runner cannot dispatch

`dispatch_command()` handles `NP_PROTO_TARGET_SLOT` and nothing else. Any other target kind is
logged as an SHDR fault **against `NP_HUB_SLOT_NONE`** — not slot 0, because attributing the drop to
the retired zone-0 slot would put a fault in the device-health log against a module that was never
involved — sets `NP_ABORT_MOD_FAULT`, and returns false.

A dropped command does **not** set its bit in `uhdr.mods_active_mask`. UHDR is the patient's dose
record; a command that was dropped must not appear in it as delivered. This is the correct behaviour
and it is also the thing that makes the gap detectable from the record rather than only from the code.

**Consequence, stated plainly:** combined with §3.3, no transcranial PBM command of either addressing
form can reach `np_mod_pbm_control()`. `OI-FWHUB-01`, blocking. This document does not resolve it —
the resolution is a socket-indexed dispatch registry, which is genuine design work with a hardware
half already proposed in `NP-HW-HEXTILE-001` §8.4 (per-cluster VLED gate as the coarse hardware cut)
and `NP-HW-HUB-001` Rev 4 §7.2.2 (the 18 Class B cluster gates commanded by this processor).

---

## 6. Session log

### 6.1 Buffers and record tags

Two 4 KiB static buffers (`s_uhdr_buf`, `s_shdr_buf`), each record prefixed by a one-byte tag:

| UHDR tags | | SHDR tags | |
|---|---|---|---|
| `0x10` session start | `0x15` stim | `0x80` session end | `0x83` zone auth |
| `0x11` session end | `0x16` visual | `0x81` PBM health | `0x84` NTC peak |
| `0x12` EEG band | `0x17` EEG impedance | `0x82` fault | `0x85` EEG calibration |
| `0x13` PBM dose | `0x18` adaptation event | | |
| `0x14` VNS/HRV | | | |

The high bit separates the two spaces, which is not decorative: a tag byte misrouted between
partitions is then a visibly invalid tag rather than a plausible one.

EEG sample blocks **bypass** the buffer and go straight to the HAL — 500 Hz × 8 ch × 3 B = 12 000 B/s
would otherwise be copied through a 4 KiB intermediate for no benefit.

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

---

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

While `set_geom_required` / `set_geom_required_tdcs` is set, every heartbeat carries the
corresponding `session_status` bit and the MCU keeps that channel **out of `granted_mask`** until it
has applied a valid electrode-area command. A lost or delayed area command therefore fails **closed**.
Both are cleared on session end/abort and by `np_safety_spi_disable_all()`. See §5.4.

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

**Reachable only through slots 0–4, which the parser rejects — see §3.3 and `OI-FWHUB-01`.**

### 8.2 EEG — `np_mod_eeg.c`

ADS1299, 8 channels, 500 Hz, 24-bit. **Fixed hardware: `detect()` always returns OK.** Waveform data
is DMA-driven and written directly to UHDR from the DMA ISR via `np_log_eeg_sample_block()`; this
driver handles configuration, impedance reads and band-power computation for adaptive feedback.

Self-calibration at session start routes the ADS1299 internal reference to all channels for one
epoch; gain/offset coefficients are stored in the Config partition (SHDR class) per
`NP-FW-EMMC-001` Rev 2 §9.

The Oz channel is the photoparoxysmal detector that halts the goggles (§8.6).

### 8.3 Stimulation — `np_mod_stim.c` (BES/tACS + tDCS)

One driver, two slots (§3.1), because both use the same stimulation DAC. Firmware-layer caps:
BES/tACS ≤ 1000 µA, tDCS ≤ 2000 µA, minimum 30 s ramp.

`REQ-FWHUB-20`: **these are secondary caps, and must always be stated as such.** The binding limit is
the safety MCU's 40 µC/cm² hardware charge-density limit, which the app cannot override. A firmware
cap that is described as *the* limit invites someone to relax it.

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
   log — is read by the safety MCU. Its limits are resident constants (40 µC/cm² density, HR-change
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

### 10.1 Binding requirements, all met by the code as of 2026-09-13

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

### 10.2 Requirements the code does NOT currently meet

Stated separately and deliberately. A specification written after the code that quietly matched the
code everywhere would be describing, not specifying.

| ID | Requirement | Gap | Item |
|---|---|---|---|
| `REQ-FWHUB-25` | A verified socket-addressed command reaches the addressed emitters | `dispatch_command()` drops every non-`SLOT` target kind | **`OI-FWHUB-01`** (blocking) |
| `REQ-FWHUB-26` | Every modality in CLAUDE.md §3's T1 roster has a dispatchable path | transcranial PBM has none (§3.3 + §5.6) | **`OI-FWHUB-01`** |
| `REQ-FWHUB-27` | Every source file's `Document:` banner cites a revision of this document that exists | three files cite Rev 2 | `OI-FWHUB-02` (fixed in this change) |
| `REQ-FWHUB-28` | The wire format has a mechanical agreement check against `hubCompiler.ts`, falsified in both directions per `NP-CONV-001` §8 | no such check exists | `OI-FWHUB-03` |

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

---

## 12. Risk register

| ID | Hazard | Sev | Mitigation | Residual |
|---|---|---|---|---|
| `RISK-FWHUB-01` | A socket-addressed command is dispatched through the slot path and delivers stimulation to a site the protocol never named | **High** | `dispatch_command()` drops it; the parser independently rejects retired slots; `ISC-87` asserts the drop | **None on wrong-site.** The residual is the opposite failure — no dose at all (`OI-FWHUB-01`) |
| `RISK-FWHUB-02` | Hub hangs with stimulation enabled | High | MCU watchdog cuts in < 50 ms on heartbeat loss; §2.4 forbids a beating fault path | Accepted (Class C backstop) |
| `RISK-FWHUB-03` | A protocol runs with no stop, from a wrapped deadline or an under-counted `cmd_count` | High | both rejected at parse (§4.4); `FWHUB-DRC-05/06` | Accepted with the checks in place |
| `RISK-FWHUB-04` | Charge limit circumvented by pause/resume | High | `ACTIVE` covers `PAUSED` (§5.1) | Accepted |
| `RISK-FWHUB-05` | tDCS runs against the 25 cm² default with small electrodes | High | geometry gate arms even when no area is declared, so the MCU never grants (§5.4) | Accepted |
| `RISK-FWHUB-06` | A cardiac re-enable happens before the MCU lockout expires | High | hub window strictly contains the MCU window; MCU denies independently (§7a) | Accepted |
| `RISK-FWHUB-07` | An SHDR record discloses user biology by redaction *shape* | Medium | unconditional suppression (§6.3); `check-redaction-shape.ts`; `FWHUB-DRC-11` | Accepted |
| `RISK-FWHUB-08` | Log records lost on power loss | Low | bounded to one flush interval (§6.5) — **but the bound is asserted against a component no NeurOne test has exercised.** `NP-SOUP-LFS-001` Rev 2 pins and vendors littlefs `v2.11.3` (closing `OI-LFS-01`), which resolves "not in the tree" and resolves nothing about the bound; `OI-LFS-02` now blocks reliance on it. The log partitions' own instance parameters are additionally unstated — `OI-LFS-05` | **Open until `OI-LFS-02`** |
| `RISK-FWHUB-09` | Emission into a lifted goggle | High | Hall cutoff is a GPIO interrupt, plus three independent layers (§8.6) | Accepted |
| `RISK-FWHUB-11` | Boot-time module authentication is not evidenced in fleet telemetry | Low | **was unmitigated — the records were discarded.** Fixed 2026-09-14 (§2.1) and held by `scripts/check-hub-bringup-order.ts`, falsified against the pre-fix commit | Accepted; records-integrity only, no emission path |
| `RISK-FWHUB-10` | Per-tile PBM drive magnitude bounded only by a thermal cutoff | Medium | carried, not closed — `OI-NVRAM-10`; re-derive §9 before a differing tile variant ships | **Open** |

---

## 13. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **`OI-FWHUB-01`** | **Transcranial PBM has no dispatchable path.** `np_mod_pbm_*` is reachable only through slots 0–4, which the parser rejects; socket-addressed commands parse and resolve but are dropped by `dispatch_command()`. Needs the socket-indexed dispatch registry — the missing half of `OI-HUB-SOCKET-01`, whose hardware half is proposed in `NP-HW-HEXTILE-001` §8.4 and `NP-HW-HUB-001` Rev 4 §7.2.2. Fails closed, so not a hazard; it is a **capability absence in the product's primary optical modality** | FW | **BLOCKING — any T1 PBM session; `REQ-FWHUB-25`, `-26`** |
| ~~`OI-FWHUB-02`~~ | ✅ **CLOSED 2026-09-13 in the same change.** Three files cited `NP-FW-HUB-001 Rev 2` against a document with no Rev 1. Re-pointed to Rev 1 §8.9 and §6.4 | FW | — |
| **`OI-FWHUB-03`** | **No mechanical agreement check between §4 and `hubCompiler.ts`.** `NP-CONV-001` §8 requires cross-artifact interface agreement to be verified by diff, never by review, and falsified in both directions first. `scripts/check-tcap-map.ts` is the pattern. Until it exists, the wire format's two implementations agree only by inspection — which is exactly the state that made `OI-DOC-01` expensive | FW + CI | `REQ-FWHUB-28` |
| **`OI-FWHUB-04`** | **`uhdr_write()`/`shdr_write()` flush *before* appending the full buffer**, so the newly appended 4 KiB is unsynced until the next flush. Consistent with §6.5's stated durability bound and therefore not a defect, but reversed from the obvious reading, and the obvious reading is what a future editor will assume. Decide: reorder, or comment the intent | FW | Documentation accuracy |
| ~~**`OI-FWHUB-07`**~~ | ✅ **CLOSED 2026-09-14.** Boot-time SHDR zone-auth records were written before the logger was initialised — stamped with session count 0, then discarded by `np_log_init()`'s `s_shdr_pos = 0U`. `np_log_backend_init()` and `np_log_init()` now precede `np_mod_reg_scan()`, which keeps `np_safety_spi_init()`'s `GAIN_SEL` precedence intact, and the misleading comment is corrected. Held by `scripts/check-hub-bringup-order.ts`, **falsified against the pre-fix commit**, not only fixtures | — (closed) | — |
| ~~**`OI-FWHUB-08`**~~ | ✅ **CLOSED 2026-09-14.** The banner and the register said "four tasks"; the code creates five. All three corrected, and the banner's count is now **counted against `xTaskCreate()`** by the same gate rather than read | — (closed) | — |
| **`OI-FWHUB-05`** | **The `np_mod_reg_scan()` SHDR auth callback fires "per zone slot"**, and the zone slots are retired. Confirm whether auth records are still expected for slots 0–4, or whether the callback should now follow the socket lattice, once `OI-FWHUB-01` is resolved | FW + Quality | Follows `OI-FWHUB-01` |
| **`OI-FWHUB-06`** | **This document has no approver.** Issued as a design output with `Approved By` blank pending review against §10.3. `21 CFR §820.30(d)` expects design outputs to be reviewed and approved before release; until that happens the register entry should say DRAFT-pending-approval rather than imply a completed review | Principal | Design-control completeness |

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

**What is deliberately not here.** No verification, no approval, and no requirement the code does not
meet except the four in §10.2 that are marked unmet and carried as open items — chiefly
`OI-FWHUB-01`, which is a capability absence and genuine design work, not a defect this change could
have absorbed.

---

## 15. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 1 | 2026-09-13 | NeurOne Firmware Engineering | **Initial release — closes `OI-DOC-01` (Issue #339) by authoring the specification that had been cited as governing since 2026-05-16 without existing.** Written against `firmware/hub_control/` as on `main`, back-dating nothing: 26 requirements met by the code (§10.1), 4 explicitly **not** met and carried as open items (§10.2), 23 decisions, 11 risk rows, 13 design-review checks, 8 open items of which 4 close here. **Four findings that did not survive being written down:** (i) transcranial PBM — CLAUDE.md §3 modality ① — **has no dispatchable path at all**, because `np_mod_pbm_*` sits only in the five retired zone slots the parser rejects *and* socket-addressed commands are dropped by `dispatch_command()`; each half was individually documented and fail-closed, their conjunction was not (`OI-FWHUB-01`, blocking); (ii) three source files cited a `Rev 2` of a document that had no `Rev 1` — re-pointed to §8.9 and §6.4 in this change (`OI-FWHUB-02`, closed); (iii) the wire format has been revised twice (`slot_mask` → `slot_id` + target block; `electrode_area_mcm2`) while the register still described `Rev 1`, because **a register entry naming an unreadable document cannot go visibly stale**; and **(iv) writing the bring-up table found two live defects in `np_hub_control_app_main()`, and both are FIXED in this change** — `np_mod_reg_scan()` ran before `np_log_init()`, so every boot-time SHDR zone-auth record was stamped with a session count of 0 and then discarded when the logger zeroed its buffer, meaning module authentication reached SHDR not at all, under a source comment three lines away asserting the opposite (`OI-FWHUB-07`); and the file banner and the document register both said "four tasks" where the code creates five, `task_protocol_rx` having been omitted (`OI-FWHUB-08`). Also records that §4 is the specification `hubCompiler.ts` compiles against and that no mechanical check enforces their agreement (`OI-FWHUB-03`), contrary to `NP-CONV-001` §8. **Both fixes are gated, not merely applied:** `np_hub_control_app_main()` is ARM-cross-only and reachable by no host test — which is how a defect dating to 2026-05-16 survived — so `scripts/check-hub-bringup-order.ts` asserts the four ordering constraints and the task count against the function itself, and was falsified **against the pre-fix commit**, where it reports exactly those two violations (`NP-CONV-001` §8). Beyond those two fixes and three corrected `Document:` banners, no code behaviour changed. **§2.1, §2.2, §10, §12 and §13 were amended within this same unmerged change to describe the corrected code rather than the code as first found; Rev 1 is issued once, describing what merges.** |
