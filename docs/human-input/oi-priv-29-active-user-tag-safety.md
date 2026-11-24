# Safety request: OI-PRIV-29 (NP-PRIV-ANALYSIS-004) (does naming an active-user tag over BLE get round a cardiac hold?)

**Status:** Q2 ANSWERED 2026-11-23 by the principal (section 6). Q1 DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#957](https://github.com/stevehickman/NeuroPulse/issues/957)
**From:** NeurOne Software + Firmware (`OI-PRIV-29` (NP-PRIV-ANALYSIS-004), Finding M2 of `docs/np_priv_analysis_004.md`)
**To:** the embedded safety team and the Quality Lead, for `NP-RISK-002` hazard 25-c; the principal if the answer reopens the per-user scope decision of 2026-09-22. No names are assigned.
**Response needed:** a choice in section 4 for Q1, a yes or no for Q2, each with a short rationale, a name, a role and a date.
**Related:** `oi-cvns-13-nuisance-cutoff-rate.md` (25-c is already provisional on the nuisance-cutoff rate).

## 1. The two questions

1. **Q1.** Hazard 25-c ("the blocked person selects another profile and is not blocked", S5 × P2 = ALARP) is
   scored on control C6: switching needs a deliberate act **past two warnings in the app**. The safety MCU
   itself accepts any tag that reaches it between sessions. Does 25-c's rating still hold when the tag can be
   written by **any** central on an encrypted BLE link, not only by the NeurOne app that shows the warnings?
2. **Q2.** The privacy repair changes every person's tag once (section 3). Is the migration rule there, "move
   to the new tag only when the helmet reports that nobody on it has an outstanding cutoff", an acceptable
   control against releasing a hold during the change?

## 2. What the code does today (checked 2026-11-21)

- **Who chooses the tag.** The apps send the first four bytes of the profile UUID. One implementation
  serves every app (`common/core/.../CervicalFaultStatus.kt` `ActiveUserTag`); the SwiftUI iOS app, which
  derived it the same way, was retired in #946.
- **What the hub accepts.** GATT `ACTIVE_USER` (0x0016) takes any 4-byte value except 0 and `0xFFFFFFFF`
  (`firmware/hub_control/src/np_cvns_fault_summary.c` `np_cvfs_on_active_user_write`). The characteristic needs an
  encrypted link. Nothing ties the value to the app or to a profile.
- **What the safety MCU does with it.** Between sessions (nothing granted, no NV write owed) it records the new tag
  and re-arms the cardiac interlock with **that tag's** hold only
  (`firmware/safety_mcu/src/np_safety_main.c`, the active-user block; `np_cardiac_interlock.c`
  `np_cardiac_interlock_user_changed`). `np_nv_cardiac_blocked(u)` (`np_nv_state.c`) is true for `u` only if `u`
  has an outstanding cutoff, or an unattributable one is outstanding. **A tag that has never been named is not
  blocked.** The previous tag's hold stays in flash and returns if that tag is named again.
- **So:** a central that writes a fresh tag between sessions gets a cervical VNS line the MCU will grant for a
  person who is held under their own tag. The app's two warnings (the blanket "someone on this device has an
  outstanding cutoff" confirmation, §9.1 item 5 of `NP-SW-FAULTMSG-001`) are not in that path. Running cervical
  VNS also needs a signed protocol uploaded; whether a central other than the app can do that is outside this
  item and is assumed possible for the question.
- **This is by design for profile switching** (the principal, 2026-09-22: a cutoff is held for the person who
  triggered it and nobody else; a device-wide block was rejected). The question is only whether C6, which lives in
  the app, is a control at all once the app is not the only writer.

## 3. The privacy repair and its migration

`REQ-FAULTMSG-01` (`NP-SW-FAULTMSG-001` §9.2) replaces the UUID prefix with HMAC-SHA-256 keyed by a per-profile
secret over the helmet serial. The core is built and tested (`common/npps-core/src/active_user_tag.rs`); no app
uses it yet. Every existing (profile, helmet) pair therefore changes tag once. Presenting the new tag while a hold is
held under the old one would release that hold without the re-enable sequence (`REQ-CVNS-09`).

The rule built (`migration_step`): an app keeps naming the old tag to a helmet until it reads
`NP_SAFETY_NV_FLAG_OUTSTANDING` clear (nobody on the helmet has a cutoff), then moves that pair for good. The
per-user flag cannot be used, because the helmet does not report which tag is current.

**One engineering condition the app wiring must meet:** before the hub has published a frame, `np_cvfs_read` returns
a header with the flags byte 0, which reads as "nothing outstanding". The app must move a pair only on a frame the
hub published from a valid safety-MCU report. The blanket warning already depends on the same flag.

**The repair does not change Q1.** It makes the tag unlinkable across helmets; it does not stop a central writing an
arbitrary value.

## 4. Options for Q1

| Option | What it means | Cost |
|---|---|---|
| **A. 25-c holds as scored** | Writing a tag outside the app counts as the same deliberate misuse C6 assumes. Record the rationale in `NP-RISK-002` §4.3.4 | None in code. The rating then rests on a control the attacker path skips |
| **B. Re-score 25-c with C6 absent for non-app centrals** | Quality Lead re-scores under `NP-RM-001` §8.2 | Analysis only. Might land above ALARP, which forces C or D |
| **C. Make the device ask** | The hub refuses a change of active user while `NP_SAFETY_NV_FLAG_OUTSTANDING` is set unless the change is confirmed on the helmet, or the safety MCU withholds cervical VNS from a newly named tag while anything is outstanding until a re-enable-style confirmation | Class B or Class C firmware change. The second option narrows the 2026-09-22 per-user scope, which is the principal's decision |
| **D. Bind tag writes to the app** | Only an authenticated app session (the bonded app, or a signed write) may write `ACTIVE_USER` | Depends on the authentication work in other `OI-PRIV` items; does not stop a modified app |

**Recommendation:** B, then C's first form if the re-score is above ALARP. A re-score is cheap and is owed anyway
while 25-c is provisional on `OI-CVNS-13`. C's first form keeps the per-user scope and moves the existing warning
from the app to the device, so it holds whoever the central is. D is worth doing for privacy (reading another tag's
fault records) but is not a safety control on its own.

**For Q2, recommendation: yes.** When nothing is outstanding on the helmet, no tag carries a hold, so the change can
release none. The cost is that a pair whose helmet always has someone held never migrates and keeps the linkable
tag; that is the safe direction.

## 5. What to send back

- Q1: A, B, C or D (or another), the rationale, and whether it reopens the principal's 2026-09-22 per-user scope.
- Q2: yes or no; if no, the condition under which a pair may move.
- Name, role, date. Record the answer in `NP-RISK-002` (25-c) and `NP-SW-FAULTMSG-001` §9.2, and log it in
  `docs/status/completed-decisions.md`.

## 6. Answer to Q2 (2026-11-23)

**Yes, with the condition the engineering note in section 3 named.** The principal chose to wire the per-helmet tag with
the migration rule, and to add a validity bit to the fault-status frame first, so "no valid report" cannot read as "nothing
outstanding". Recorded in `NP-SW-FAULTMSG-001` §9.2 (`REQ-FAULTMSG-01` (c)–(e)) and in the decisions log.

What was built: the hub sets `CVNS_FAULT_STATUS` byte 3 bit 7 only for a validated safety-MCU report (frame version 2);
the app moves a pre-existing profile to its per-helmet tag on a helmet only on a frame with that bit set and nobody
outstanding, records the move before naming the new tag, and never names the old tag to that helmet again. A profile made
since gets the per-helmet tag at once. The record of the move travels with the profile.

**Residual, for Safety to weigh with Q1:** two copies of one profile on two phones that are not synchronised (one copied
before the other migrated) can name different tags to one helmet; the copy that has not moved still names the old tag,
and a cutoff held under the new one does not apply to it. The same profile on two phones is not a supported use today,
and the blanket warning still shows on both.

**Q1 is still open.**
