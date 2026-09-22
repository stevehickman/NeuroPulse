# Offline Fault Explanation — Proposal

**Project:** NeurOne
**Document:** NP-SW-FAULTMSG-001
**Revision:** 1
**Date:** 2026-09-22
**Status:** DRAFT — **proposal for principal review. Nothing here is implemented or decided.**
**Effective Date:** —
**Author:** NeurOne Systems Engineering
**Approved By:** — (pending principal review)
**References:** CLAUDE.md §4.2 (safety MCU owns the enable lines; cervical VNS cardiac interlock), §4.5 (USB-C power only, no battery), §4.6 (Mode 3), §4.7 (status LED), §5.1 (UHDR/SHDR; redaction shape), §17 (firmware renders no text); NP-FW-CVNS-001 §3.5, §5.4, §5.5; NP-HW-CVNS-001 `REQ-CVNS-09`; NP-REG-CVNS-001 §7.3, §7.4 (proposed in PR #370); NP-RISK-002 RISK-25; `OI-ACC-07` (`docs/status/pending-decisions.md`); `firmware/safety_mcu/src/np_cardiac_interlock.c`, `np_fault_latch.c`, `np_safety_main.c`; `firmware/cervical_vns/src/np_cvns_session.c`, `include/np_cvns_types.h`; `firmware/hub_control/include/np_hub_types.h`; `app/ios/NeurOne/Session/EDFDownloader.swift`
**Related Issues:** PR #370 (cervical gel pad alert and the user-doc Mode 3 route this proposal completes)
**Gate:** N/A
**IEC 62304 Class:** Touches SW-01 Class C (safety MCU, §4.3), SW-02 Class B (hub, cervical library) and the apps
**Supersedes:** None — new document.
**Change Summary:** Rev 1 (2026-09-22) — first draft, at the principal's request: explain faults that happened while no phone was connected, from the device's own logs, when a phone next connects. Records three findings the design depends on (§3), the first of which is a safety gap in the cardiac lockout.
**Review Cadence:** On principal decision; again when any §7 item closes.

---

## 1. The problem

In Mode 3 the headset runs a pre-programmed protocol from a USB-C power bank, with no phone
(CLAUDE.md §4.6). It has no text of its own (CLAUDE.md §17). Its only fault signal is one LED state
that is the same for every fault: a red blink on the power LED (CLAUDE.md §4.7;
`np_led_state_t` has three values: idle, session, fault). So the wearer learns *that* something
failed, but not *what* failed or *what to do*.

The user doc now routes this to the phone. The proposed IFU text (`NP-REG-CVNS-001` §7.4, PR #370)
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
phone. No new SHDR field is added. P1's flag is device-internal and never reported. The app's
"last acknowledged session" pointer is a session counter with no timestamps, held on the phone.

## 7. Open items raised

| ID | Item | Owner |
|---|---|---|
| **OI-FAULTMSG-01** | **The cardiac lockout does not survive a power cycle (F1)**, so in Mode 3 an unplug and re-plug restarts cervical stimulation with no app confirmation, and nothing keeps cervical VNS out of autonomous protocols. Decide P1, P1b or both. Record the hazard under RISK-25 (NP-RISK-002 §4) and in NP-FW-CVNS-001 §5.4. **Must close before cervical VNS ships** | Safety + FW + Quality (ISO 14971) |
| **OI-FAULTMSG-02** | The UHDR session record cannot tell a cardiac cutoff from other interlock faults, and `cutoff_hr_at_event_x10` is never written (F2). P2 | FW |
| **OI-FAULTMSG-03** | Choose the connect-time transport for the fault summary: USB-C parameter-log download, a Bluetooth request/response pair, or both (P3) | FW + App |
| **OI-FAULTMSG-04** | App gate and fault text on iOS and Android (P4), then the user-doc change (P5) | App + Regulatory |

## 8. Decisions requested

1. **F1:** P1 (persist the lockout in the safety MCU), P1b (no cervical VNS in Mode 3), or both.
   This is the decision that matters most, and it stands on its own whatever happens to the rest.
2. Accept P2–P4 as the shape of the offline-fault explanation, cervical first.
