# Offline Fault Explanation

**Project:** NeurOne
**Document:** NP-SW-FAULTMSG-001
**Revision:** 3
**Date:** 2026-09-23
**Status:** DRAFT — **decided by the principal (§9); implemented in the safety MCU, the hub and both apps, not yet verified on hardware.** The hub now builds and publishes CVNS_FAULT_STATUS (§9.6); what is still missing is the BLE GATT server that carries it (OI-WA-03) and the UHDR storage glue (OI-LFS-05)
**Effective Date:** —
**Author:** NeurOne Systems Engineering
**Approved By:** Principal (decisions §9.1, 2026-09-22) — document approval pending
**References:** CLAUDE.md §4.2 (safety MCU owns the enable lines; cervical VNS cardiac interlock), §4.5 (USB-C power only, no battery), §4.6 (Mode 3), §4.7 (status LED), §5.1 (UHDR/SHDR; redaction shape), §17 (firmware renders no text); NP-FW-CVNS-001 §3.5, §5.4, §5.5; NP-HW-CVNS-001 `REQ-CVNS-09`; NP-REG-CVNS-001 §7.3, §7.4 (added in PR #370, merged 2026-09-22); NP-RISK-002 RISK-25; `OI-ACC-07` (`docs/status/pending-decisions.md`); `firmware/safety_mcu/src/np_cardiac_interlock.c`, `np_fault_latch.c`, `np_safety_main.c`; `firmware/cervical_vns/src/np_cvns_session.c`, `include/np_cvns_types.h`; `firmware/hub_control/include/np_hub_types.h`; `app/ios/NeurOne/Session/EDFDownloader.swift`
**Related Issues:** PR #370, merged (cervical gel pad alert and the user-doc Mode 3 route this proposal completes)
**Gate:** N/A
**IEC 62304 Class:** Touches SW-01 Class C (safety MCU, §4.3), SW-02 Class B (hub, cervical library) and the apps
**Supersedes:** None — new document.
**Change Summary:** Rev 3 (2026-09-23) — **the hub side of P3 is built (§9.6).** A Class B module keeps each user's recent cervical faults across power loss, builds the `CVNS_FAULT_STATUS` frame byte-for-byte as the apps parse it, publishes it on change, and handles the `CVNS_REENABLE_CONFIRM` and `ACTIVE_USER` writes. The BLE GATT server and the UHDR storage glue remain platform seams. Rev 2 (2026-09-23) — records the principal's decisions (§9.1): P1 chosen, P2–P4 accepted, a cardiac cutoff withholds cervical VNS only, and it is held per user with a blanket warning to every user of the device. Adds §9 (what was built, the per-user model, the NV log, the wire formats, what is not verified) and updates §6 for the two bits that now cross to the app. Rev 1 (2026-09-22) — first draft, at the principal's request: explain faults that happened while no phone was connected, from the device's own logs, when a phone next connects. Records three findings the design depends on (§3), the first of which is a safety gap in the cardiac lockout.
**Review Cadence:** On principal decision; again when any §7 item closes.

---

## 1. The problem

In Mode 3 the headset runs a pre-programmed protocol from a USB-C power bank, with no phone
(CLAUDE.md §4.6). It has no text of its own (CLAUDE.md §17). Its only fault signal is one LED state
that is the same for every fault: a red blink on the power LED (CLAUDE.md §4.7;
`np_led_state_t` has three values: idle, session, fault). So the wearer learns *that* something
failed, but not *what* failed or *what to do*.

The user doc now routes this to the phone. The proposed IFU text (`NP-REG-CVNS-001` §7.4, merged in PR #370)
says: if the red light blinks without the app, stop, connect your phone and start again from the
app. That works for a gel pad fault, because starting again re-runs the pad check and the app names
the side. It does not work for a **cardiac cutoff**. §7.3 says not to restart after one without
medical advice, but the wearer cannot tell the red blink was cardiac, and the app is not told about a
fault that happened while it was not connected.

The principal's question (2026-09-22): the device logs its faults — why not use the log when the
phone connects? **This proposal does that.** Writing it up turned up a more basic problem first
(§3, F1).

## 2. What already exists

| Piece | Where | What it gives us |
|---|---|---|
| Cervical session record, written at the end of every session including a fault | `np_cvns_session_record_t` (`np_cvns_types.h`); written by `session_advance_to_fault()` (`np_cvns_session.c`) to the UHDR partition | Montage, stimulation parameters, `cutoff_occurred`, HR baseline, time of cutoff, left/right impedance, `abort_reason` |
| SHDR fault summary | `shdr_summary.fault_reason` (same function) | The fault kind (`np_cvns_fault_reason_t`: HR change, R-peak data loss, watchdog, safety MCU), with no timing |
| Both on eMMC | UHDR and SHDR partitions (CLAUDE.md §4.1) | **Survives power loss**, unlike anything held in RAM |
| Download on reconnect | `EDFDownloader.swift` (Mode 4) | Fetches a session's EDF+ file and parameter log over USB-C, per session ID, when asked |
| Safety MCU fault latch | `np_fault_latch.c` | Last fault status and slot. Survives a warm reset and is reported on the first heartbeat after it. **Cleared by a power-on reset** |

What is missing is the other half: **nothing reads these records to tell the wearer what
happened**, and nothing runs at connect, before a session can start.

## 3. Findings the design depends on (verified against the code, 2026-09-22)

### F1 — Unplugging the power bank clears the cardiac lockout (safety gap)

After a cardiac cutoff, the safety MCU refuses re-enable for 30 s, then requires app confirmation
and a repeat impedance check (NP-FW-CVNS-001 §5.4 step 2c; `REQ-CVNS-09`; CLAUDE.md §4.2).
**That state is held in RAM and does not survive a power cycle:**

- `s_lockout_active` in `np_cardiac_interlock.c` is a plain static, reset to `false` by
  `np_cardiac_interlock_init()`, which `np_safety_main.c` calls on **every** boot.
- The fault latch lives in a SRAM section that a warm reset keeps and **a power-on reset clears**
  (`np_fault_latch.c` header).
- The headset has **no battery** (CLAUDE.md §4.5), so unplugging the power bank *is* a power-on reset.

**Consequence.** In Mode 3, a wearer who has a cardiac cutoff and then unplugs and re-plugs the power
bank gets a device with no memory of it. A new session can arm the baseline and enable cervical
stimulation again, **with no app confirmation at all**. `REQ-CVNS-09`'s confirmation step holds only
while the device stays powered.

Nothing else stops it. `NP_PROTO_FLAG_AUTONOMOUS` is defined in `np_hub_types.h` and **read
nowhere**, so nothing keeps cervical VNS out of an autonomous protocol.

This is not recorded in NP-RISK-002 RISK-25, NP-FW-CVNS-001 or NP-HW-CVNS-001. Deciding what to do
about it is an ISO 14971 decision, and it is the most important item here (`OI-FAULTMSG-01`).

### F2 — The session record cannot tell a cardiac cutoff from other interlock faults

`np_cvns_session_tick()` maps **every** interlock fault to `NP_CVNS_ERR_CARDIAC_CUTOFF`, so
`cutoff_occurred = 1` for a heart-rate change **and** for R-peak data loss, a watchdog expiry or a
safety-MCU refusal. The interlock's real `fault_reason` goes only to the SHDR summary. And
`cutoff_hr_at_event_x10` is never written anywhere. So the one record the user owns cannot say
which fault happened. An app reading it would call a lost R-peak signal a cardiac cutoff. (This is
the "residual" `np_mod_cvns.c` already records, and it is wider than that note says.)

### F3 — The download path is history, not a gate

`EDFDownloader` fetches one session at a time, on request, over USB-C. It is built for session
history. Nothing on connect asks "did anything go wrong since you last saw me?" before the app offers
a session.

## 4. Proposal

Four parts, each owned by the layer that already owns the behaviour.

### P1 — Close F1 where the interlock lives (safety MCU, Class C) — recommended

Make the cardiac lockout survive power loss. On a cardiac cutoff, the safety MCU also writes a
**"cutoff not yet acknowledged"** flag to its own flash (the STM32G071 has 128 KB, and cutoffs are
rare, so wear is negligible). At boot it reads the flag, and while it is set it **refuses the CVNS
enable** until the app confirmation that `REQ-CVNS-09` already requires arrives, together with the
repeat impedance check. Clearing the flag is the existing re-enable path, not a new one.

Why here: CLAUDE.md §4.2 puts every stimulation enable line under the safety MCU, and says no app-side
path may bypass it. Only the safety MCU can make the lockout hold in every mode, including one with
no phone.

**Alternative (P1b), weaker:** keep cervical VNS out of autonomous protocols. The app and web
compilers reject a cervical block when the autonomous flag is set, and the hub refuses one at load.
That is Class B enforcement of a Class C property. It also removes Mode 3 cervical use entirely,
which is a product decision. It could be added *as well as* P1, but not instead of it.

Either way, the flag carries no timing and is never reported to SHDR. Whether it is set is itself
cardiac information, so it stays device-internal like the fault latch's `tick_ms` (CLAUDE.md §5.1).

### P2 — Record the fault kind in the user's own record (cervical library, Class B)

- Carry `np_cvns_fault_reason_t` in `np_cvns_session_record_t`, using one of its three reserved bytes.
- Set `cutoff_occurred` **only** for `NP_CVNS_FAULT_HR_CHANGE`.
- Write `cutoff_hr_at_event_x10`.

The record stays UHDR and its size is unchanged. The SHDR summary does not change.

### P3 — A fault summary on connect (hub + app)

When the app connects, it asks the hub for the cervical session records since the last session it
has acknowledged. It tracks that with the existing session counter, which carries no timestamps.
Those records are the user's own UHDR, read by the user's own phone. Nothing goes to NeurOne. The
transport can reuse the existing parameter-log download over USB-C. Over Bluetooth it needs a small
request/response pair. Choosing between them is `OI-FAULTMSG-03`.

**The summary runs before the app offers any session.** It is a gate, not a history screen.

### P4 — Explain, then gate (apps)

For each unacknowledged fault, the app shows what happened and what to do, **before** it offers a
cervical session:

| Logged fault (after P2) | What the app says | Before cervical stimulation can start again |
|---|---|---|
| HR change (cardiac cutoff) | Stimulation stopped because the heart rate changed. §7.3's text: do not use cervical stimulation again without medical advice | Acknowledgement, **and** P1's safety-MCU confirmation path |
| R-peak data loss | Stimulation stopped because the heart-rate signal from the ear clip was lost. Check the ear clip is on and its pads are in contact | Acknowledgement |
| Pad contact (pre-enable or mid-session) | Which side of the neck, from the record's impedance and montage fields, like the live alert in PR #370 | None beyond the live pad check |
| Watchdog / safety MCU | Stimulation stopped for a device fault; contact support if it repeats | Acknowledgement |

All text goes in `locales/*.json` (CLAUDE.md §17). The wording in the table is only the intent.

### P5 — Then change the user doc

Once P1–P4 exist, `NP-REG-CVNS-001` §7.4's no-app paragraph becomes "connect your phone and open
the NeurOne app; it will tell you what happened before you use the device again", and §7.3's
cardiac warning gains the same route. Until then, the PR #370 text stands.

## 5. Scope

**In:** cervical VNS, because it is the only modality whose restart rule depends on knowing which
fault happened. **Not in, yet:** the other modalities' faults. P3 and P4 are built to extend to them;
listing them is a follow-on once this shape is accepted. **Not proposed:** tone or LED fault codes.
The user doc already routes Mode 3 faults to the phone (principal, 2026-09-22), and this proposal
makes that route tell the truth.

## 6. Privacy

Everything the app shows comes from UHDR records on the user's own device, read by the user's own
phone. No new SHDR field is added. The app's "last acknowledged session" pointer is a session
counter with no timestamps, held on the phone, one per profile.

**Rev 2 — two bits now leave the safety MCU, to the app only (§9.3).** P1's flag is still never
reported to SHDR, never timed and never logged by the hub. But the per-user decision (§9.1) needs the
app to know (a) whether the *active* user's cervical VNS is withheld and (b) whether *anyone* on the
device has an outstanding cutoff. Bit (b) is shown to every profile as the blanket warning. **That
is a disclosure, chosen by the principal:** anyone who connects to a shared device learns that
*someone* who uses it had a cardiac cutoff. It never says who. The alternative was letting a person
switch profiles to get round their own block without any prompt. The per-user identity sent to the
device is an opaque tag derived from a random profile UUID, with no name (§9.2).

## 7. Open items raised

| ID | Item | Owner |
|---|---|---|
| **OI-FAULTMSG-01** | *(Rev 2: **decided — P1**, implemented §9.4; open until hardware-verified and RISK-25 is re-scored.)* **The cardiac lockout does not survive a power cycle (F1)**, so in Mode 3 an unplug and re-plug restarts cervical stimulation with no app confirmation, and nothing keeps cervical VNS out of autonomous protocols. Decide P1, P1b or both. Record the hazard under RISK-25 (NP-RISK-002 §4) and in NP-FW-CVNS-001 §5.4. **Must close before cervical VNS ships** | Safety + FW + Quality (ISO 14971) |
| **OI-FAULTMSG-02** | *(Rev 2: **implemented**, commit "Cervical VNS: record the real fault kind", §9.4.)* The UHDR session record cannot tell a cardiac cutoff from other interlock faults, and `cutoff_hr_at_event_x10` is never written (F2). P2 | FW |
| **OI-FAULTMSG-03** | *(Rev 3: **the hub builds, persists and publishes the frame and handles both writes, §9.6**; what remains is the platform: the BLE GATT server (OI-WA-03) and the UHDR blob store (OI-LFS-05). Rev 2: transport chosen — Bluetooth GATT 0x0014/0x0015/0x0016, §9.3.)* Choose the connect-time transport for the fault summary: USB-C parameter-log download, a Bluetooth request/response pair, or both (P3) | FW + App |
| **OI-FAULTMSG-04** | *(Rev 2: **apps implemented** on iOS and Android, §9.4; P5, the user-doc change, waits until the hub publishes the summary.)* App gate and fault text on iOS and Android (P4), then the user-doc change (P5) | App + Regulatory |

## 8. Decisions requested

1. **F1:** P1 (persist the lockout in the safety MCU), P1b (no cervical VNS in Mode 3), or both.
   This is the decision that matters most, and it stands on its own whatever happens to the rest.
2. Accept P2–P4 as the shape of the offline-fault explanation, cervical first.

## 9. Decisions and implementation (Rev 2)

### 9.1 Decisions (principal, 2026-09-22)

1. **P1** — the safety MCU persists the cardiac cutoff. P1b is not taken; cervical VNS stays
   available in Mode 3.
2. **P2–P4 accepted**, cervical first.
3. **A cardiac cutoff withholds only what it was meant to withhold.** It stops cervical VNS
   (`NP_CARDIAC_BLOCK_MASK` = `NP_SAFETY_EN_CVNS`). Every other modality stays available. Before
   Rev 2, CARDIAC was in the all-channel cutoff mask.
4. **Per user.** The cutoff is held for the person who triggered it, and nobody else. The device
   assumes the same person is using it until the app names someone else.
5. **Blanket warning.** If anyone on the device has an outstanding cutoff, every user sees a
   warning at connect and confirms it. A user who is not blocked must confirm before a cervical
   protocol is sent, so that switching profiles cannot quietly get round a block.
6. The user's identity is the app's **individual profile**. Enforcement is in the **safety MCU**,
   with per-user records. The warning is **at connect, with a confirmation**.

### 9.2 The per-user model

- **Active user.** An opaque 32-bit tag. On both apps it is the first four bytes of the profile's
  random UUID, read little-endian (`ActiveUserTag`). 0 means *unspecified* and `0xFFFFFFFF` means
  *any*; both are reserved and never produced. The hub forwards the tag in a 10-byte SPI frame
  (`np_safety_user_cmd_t`, `NP_SAFETY_CMD_ACTIVE_USER`) only while no session is running. The
  safety MCU accepts it only when nothing is granted and no NV write is owed.
- **Blocking rule.** `blocked(u)` holds if any of these is true:
  - an unattributable cutoff is outstanding: one recorded before any user was named, or read back
    from a torn record;
  - `u` has an outstanding cutoff;
  - `u` is unspecified and any cutoff is outstanding.

  The rule fails closed throughout.
- **Clearing.** A completed re-enable (`REQ-CVNS-09`: app confirmation and repeat impedance) clears
  the confirming user's own entry and every unattributable one. The person who ran the
  confirmation owns those.
- **At boot.** The safety MCU replays the log. If the current user is blocked, it arms the cutoff
  *latently*: cervical VNS is withheld at the first cervical request, and nothing else is affected.

### 9.3 Wire formats

| Link | Frame | Contents |
|---|---|---|
| Hub → safety MCU | `np_safety_user_cmd_t`, 10 B | Command 0x04 + tag + checksum |
| Safety MCU → hub | `np_safety_nv_report_t`, 4 B at MISO offset 16 of every reply | Magic 0x6C. Flags: bit 0 = the active user is blocked, bit 1 = a cutoff is outstanding for someone. Checksum |
| Hub → app | GATT 0x0014 CVNS_FAULT_STATUS, READ/NOTIFY, 4 + 8n B | Version, hub re-enable state, n, the flags byte above, then up to four records (session counter, fault kind, pad-side mask). Records are the active user's own |
| App → hub | GATT 0x0015, WRITE 1 B | 0x01: the wearer confirms resuming (`REQ-CVNS-09`) |
| App → hub | GATT 0x0016, WRITE 4 B | Active-user tag, little-endian |

None of the three GATT characteristics is in the required set. A T1 hub has no cervical accessory,
and no hub publishes them yet.

### 9.4 What was built

- **Safety MCU (Class C).**
  - `np_nv_state.c`: an append-only record log on flash pages 62–63, each record a 64-bit double
    word. Record types are SET, CLR, USER, RESET and COMMIT. The log compacts to a committed
    snapshot, and at most 8 users can be pending before it overflows to "any".
  - Every failure mode reads back as blocked: an uncommitted snapshot merges additively, a torn
    record counts as SET(any), and an erase or program failure becomes a FAULT after 3 attempts.
  - NV writes stall the core, so they happen only while no channel is granted.
  - The CARDIAC mask is narrowed to cervical VNS, and user changes are scoped.
  - Tests: `np_nv_state_tests` (12, on a NOR-flash double with tear, stop and erase-fail
    injection) and the cardiac interlock tests (`test_cardiac_blocks_only_cvns`,
    `test_user_change_scopes_the_cutoff`).
- **Cervical library (Class B), P2.**
  - `fault_reason` is now in the UHDR session record.
  - `cutoff_occurred` is set only for a heart-rate change.
  - `cutoff_hr_at_event_x10` is now written.
- **Hub (Class B).**
  - Parses the NV report.
  - Forwards the active-user tag while idle (`np_hub_set_active_user`).
- **Apps (iOS and Android).**
  - Parse 0x0014 and keep a per-profile acknowledgement ledger.
  - An offline-fault summary that must be acknowledged.
  - An unread cardiac cutoff blocks the upload of a cervical protocol.
  - The resume card sends 0x0015 while the hub awaits confirmation.
  - The blanket warning is shown once per connection.
  - A one-shot "is this your profile?" confirmation before a cervical upload while someone else's
    cutoff is outstanding.
  - The active-profile tag is written to 0x0016.
  - All text is in `locales/*.json` (CLAUDE.md §17).

### 9.5 Not done, not verified

- ~~**The hub does not build the 0x0014 frame.**~~ *Built 2026-09-23, §9.6.* **The hub still has no
  BLE GATT server** (OI-WA-03), and the UHDR partition has no littlefs parameters (OI-LFS-05), so
  the three seams §9.6 names are traps on target. Until they exist the path is complete and tested
  on the host but unreachable on hardware, and P5 waits.
- **Not verified:** ARM cross-build and silicon for the NV driver; Swift compilation (no toolchain
  here); the Android `:app` module (the AGP build was not reachable here). The Android `:core` tests
  pass.
- ~~**Android has no profile picker yet.**~~ *Added 2026-09-23.* Settings → *Who is using the
  device* (`ProfilesScreen.kt`) adds, activates, deactivates and deletes profiles. Profiles and the
  active profile now persist across restarts, so a relaunch does not become "nobody named".
  Until a profile is chosen, or after one is deactivated, nothing is sent and the device keeps
  assuming its last user, as §9.1(4) requires. That is the same as iOS.
- **Flash hazards.**
  - A double-bit ECC error on reading a torn flash word raises an NMI.
  - Any future safety-MCU update path must never erase NV pages 62–63.

  Both must be recorded against RISK-25.
- **Auricular VNS is not in the CARDIAC mask.** Whether it belongs there is a hazard-analysis
  question, not an implementation one.
- ~~**One path skips the app's check.**~~ *Fixed 2026-09-23.* The wire-level
  `upload(_ proto: NPSessionProtocol)` and `programAutonomous(_ proto: NPSessionProtocol)` on iOS
  were public, and they skipped the checks the definition path runs. Both are now private. Every
  upload, Mode 2 or Mode 3, enters through a protocol definition, passes the same cervical check
  and shows the same message and "is this your profile?" confirmation in the protocol menu. The
  wire format has no cervical VNS case, so the old path could not actually send cervical
  stimulation; the gap was the unchecked entry point, not a cervical session that went unchecked.

### 9.6 The hub side of P3 (Rev 3, 2026-09-23)

`firmware/hub_control/src/np_cvns_fault_summary.c` (Class B, SW-02) is what the app reads at
connect.

**What it keeps.** The last 8 cervical fault stops across all users. Each carries:
- the device session counter;
- the fault kind;
- for a pad fault, which side of the neck failed;
- the user named when it happened.

It also keeps the hub's copy of the last-named user, so a Mode 3 fault after a power cycle is still
attributed to someone. It stores no heart-rate value and no timestamp.

**Where it comes from.** When a cervical session ends on a fault, `np_mod_cvns`'s session-end
callback records it with its real `fault_reason` (the P2 field).
- **Pad side.** For a pad fault, the side comes from the session record's per-side impedances. A
  side out of window, or not a finite positive number, is named. If neither is out of window, both
  are named rather than neither.
- **Caveat.** Which physical pad is "left" is still `OI-CVNSHW-01`'s wiring question.

**Who sees what.** A frame carries the active user's records and the unattributed ones, and never
another named user's. That mirrors the safety MCU's rule (§9.2). Attribution changes when the
heartbeat forwards a new user to the safety MCU, which happens between sessions. So the hub and the
safety MCU always name the same person for a given session's fault. It does not change when the app
writes `ACTIVE_USER`, which can happen mid-session.

**The frame.** The frame is exactly §9.3's layout:
- the four newest matching records, oldest first;
- the hub re-enable state;
- the safety MCU's two flags, sent as 0 until the first valid report arrives.

A host test byte-compares it against the frame the iOS and Android parser tests use, so the three
cannot drift apart silently.

**Publishing.** `np_cvfs_poll()` runs on every heartbeat. It rebuilds the frame, notifies only when
the frame changed, and persists a pending change outside the critical section. A failed save is
retried on the next poll. `np_cvfs_read()` serves an ATT read.

**Writes.**
- **0x0016** (`ACTIVE_USER`) accepts exactly 4 bytes, not 0 and not `0xFFFFFFFF`, and posts to
  `np_hub_set_active_user()`.
- **0x0015** (`CVNS_REENABLE_CONFIRM`) accepts exactly `0x01` and forwards to
  `np_hub_cvns_reenable_confirm()`.

A refusal returns an error, so the app's write-with-response fails, as its UI expects.

**Persistence.** The store is a 90-byte-maximum blob with a version byte and a CRC-32. A missing,
short, wrong-version, CRC-failing or out-of-range blob loads as empty with no user named. That is
the right failure for information: the safety MCU holds the cutoff regardless (C4 in `NP-RISK-002`
§4.3.2).

**Platform seams.** There are three: `np_cvfs_hal_load`, `np_cvfs_hal_save` and
`np_cvfs_hal_notify`.
- They are declared once, in `np_cvns_fault_summary.h`, and trap-defined in
  `firmware/platform/src/np_platform_stub.c`.
- The SW-02 platform census rises from 94 to 97.
- They wait on the UHDR partition's littlefs parameters (`OI-LFS-05`) and the BLE GATT server
  (`OI-WA-03`). This is the same shape as `np_transport.h`'s producer seam.

**Tests.** `np_cvns_fault_summary_tests` covers:
- the golden frame, flags and state;
- per-user scope;
- the newest-four window, eviction and record validation;
- pad sides;
- persistence round trip, corrupt blobs and save retry;
- notify-on-change and read;
- both write handlers.

`np_mod_cvns_tests` gains the check that a fault reaches the summary once, with its kind and
counter.

Three mutations were each caught: dropping the user filter, reversing the order, and losing the save
retry. Host ctest is 38 of 38 (8 Class C, 30 Class B).

**Not verified, and a known cost.**
- The ARM cross-build runs in CI only.
- The save runs in the 200 ms heartbeat task. It is rare (a fault or a user change), and a slow
  eMMC write delays one beat, well inside the safety MCU's 1.5 s watchdog, but its worst-case
  duration is not measured.
- A power loss between a fault and the next poll (≤ 200 ms) loses that record's explanation, not
  its cutoff.
