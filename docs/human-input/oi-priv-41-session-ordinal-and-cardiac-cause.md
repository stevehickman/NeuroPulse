# Question for the principal and Safety: OI-PRIV-41 (NP-PRIV-ANALYSIS-004), the shared session ordinal, the cardiac cutoff's cause in SHDR, and the NVSTATE fault

**Status:** ANSWERED 2026-11-23 by the principal (see §Ruling at the end). Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#969](https://github.com/stevehickman/NeuroPulse/issues/969)
**From:** NeurOne Firmware (`OI-PRIV-41` (NP-PRIV-ANALYSIS-004) in `docs/status/pending-decisions.md`; `docs/np_priv_analysis_004.md` Findings L3 and L6)
**To:** the principal (Q1, Q2) and Safety (Q2, Q3)
**Response needed:** an option for each question, a short rationale, anything that would reopen it, and your name, role and the date.

Points 3 and 6 of the item were implemented with this brief (comment reclassification; one GATT link at a time). Points 4, 5 and 8 are
specified in the open-item row. This brief covers the three points that need a ruling. All three were re-read against the code on 2026-11-21.

## Q1. Should the UHDR side stop using the device session count as its ordinal? (point 1; the principal, with Firmware)

### Present state (checked)

One counter, `s_device_session_count` (`firmware/hub_control/src/np_session_log.c:46`), is the ordinal on both sides of the §5 boundary:

| Where | Record | Code |
|-------|--------|------|
| SHDR | every SHDR record the logger writes (session open, session end, module records, faults) | `np_session_log.c:244`, `:300`, `:335`, `:356`, `:398`, `:506`, `:560`, `:582` |
| SHDR fleet | `fault_log.session_index` and the other `session_index` columns | `ci/shdr/shdr_fleet_schema.sql:1356` |
| UHDR | the cervical offline-fault summary sent to the app (`CVNS_FAULT_STATUS`), one `session_counter` per record | `np_cvns_fault_summary.c:236`, fed by `np_mod_cvns.c:642` |
| UHDR | **the name of every UHDR session file** (`%020llu`) | `np_log_backend.c:231` → `np_log_hal_part_open(UHDR, session_counter)`; `platform_sim/src/np_sim_storage.c:169` |

The finding holds and is **wider than reported**: it is not only the cervical fault summary. Every UHDR session file is named by the same number,
so any UHDR session that leaves the person's control (a clinician grant, an EDF+ export, a backup, a disclosure) joins exactly to the SHDR history of
that helmet in the fleet database, and through that to the warranty token.

**What the apps join on.** The shared core and the Swift app use `sessionCounter` as the record's identity and as an acknowledgement ledger
("show only records newer than the last one acknowledged"), with a wrap rule: if the newest is below the last acknowledged, all are shown again
(`common/core/.../models/CervicalFaultStatus.kt:163-176`; `app/ios/NeurOne/Models/CervicalFaultStatus.swift:149-160`). They need a number that rises
per helmet. They do not need it to equal the SHDR count.

### Options

- **A. A separate UHDR ordinal (recommended).** The hub keeps a second counter in the UHDR partition, advanced at each session begin, and uses it for
  UHDR file names and the cervical summary. SHDR keeps the device count. *Cost:* a persisted UHDR counter (its loss or rollback must not reuse a file
  name: exclusive create already refuses that, `np_log_backend.c:233`); a one-time reset of each app's acknowledgement ledger (the wrap rule already
  handles a lower number by showing records again once); the UHDR file-name format and the EDF+ download path are re-specified; host tests for the
  logger, the backend, the summary and the two app ledgers. Firmware and app work, no data moved toward SHDR.
- **B. A keyed ordinal.** The UHDR side carries an order-preserving value derived from the device count under a key held only in UHDR. *Cost:* an
  order-preserving keyed function is either weak (an offset or a multiplier is undone by two known pairs) or heavy; the gain over A is only that no
  second counter is persisted. Not recommended.
- **C. Accept.** Record that a UHDR disclosure joins to SHDR history, and why that is tolerable. *Cost:* every UHDR disclosure becomes a join key into
  NeurOne's fleet data; this is the kind of linkage §5.3 forbids for research ("no linkage table exists"). Not recommended.

**A decision is owed** because the change crosses firmware storage layout, the EDF+ path and every app's ledger, and because option C is a §5
trade-off. A moves nothing toward SHDR, so if the principal prefers, A can proceed as ordinary Firmware work on a "yes".

## Q2. What may SHDR say about the cause of a cardiac cutoff? (point 2; Safety and the principal)

### Present state (checked)

The safety MCU's `cardiac_cutoff()` (`firmware/safety_mcu/src/np_cardiac_interlock.c:225-235`) sets `NP_SAFETY_STATUS_CARDIAC | NP_SAFETY_STATUS_CUTOFF`
for **four** causes, which the hub cannot tell apart:

1. a heart-rate step (`:542-544`, `REQ-CVNS-09`);
2. R-peak input staleness while the channel is granted (`:493-498`, `OI-RISK2-05`);
3. disagreement between the MCU's rate and the hub's own estimate (`:510-516`, `OI-CVNS-14`);
4. the hub's estimate going quiet after an agreement (`:519-522`).

The hub routes every one to a single SHDR event, `NP_CVNS_SHDR_EV_CUTOFF` (0xC1, `np_hub_control_main.c:181`), which the fleet schema's comment, the
redaction gate and three firmware comments call `fault_log.fault_type = 'CVNS_HR_CUTOFF'` (`ci/shdr/shdr_fleet_schema.sql:1352`;
`scripts/check-redaction-shape.ts:90`; `np_cvns_types.h:151`; `np_fault_latch.c:141`; `np_fault_latch_privacy_tests.c:17`). So SHDR labels a
sensor dropout or a disagreement as a change in the wearer's heart rate. The finding holds.

**The privacy direction matters here.** Today the label is wrong but the biology it asserts is *diluted*: a fleet reader cannot tell a real heart-rate
step from a lost R-peak. Splitting the causes in SHDR would make "the wearer's heart rate stepped during cervical stimulation" a **sharper, certain**
SHDR fact, which moves data toward SHDR and needs the principal. Splitting would also touch the Class C status word.

### Options

- **A. Rename, do not split (recommended).** The fleet label becomes cause-neutral (`CVNS_CARDIAC_INTERLOCK_CUTOFF`), and the comments, the schema
  comment and the redaction gate's text say that the event covers all four causes and asserts no heart-rate change. *Cost:* documentation and a
  schema comment; no firmware behaviour, no safety MCU change, no cutoff behaviour change. Fleet maintenance still cannot tell a sensor problem from a
  physiological one through this event (the R-peak and impedance paths have their own device-condition signals to examine).
- **B. Split, device causes only.** A second SHDR code for causes 2–4 (sensor and cross-check), and the existing code for cause 1. *Cost:* the
  heart-rate step becomes a certain SHDR fact about the wearer (a §5.1 move toward SHDR: the principal); the cause must cross the SPI reply, so the
  Class C status word or the reply frame changes (Safety, IEC 62304 Class C); and the conditional split must be shown not to recreate the oracle
  pattern of `np_fault_latch.c` (rule 2 of §5.1, `check-redaction-shape.ts`). Not recommended.
- **C. Split, cause to UHDR only.** The cause is written to the user's UHDR session record and SHDR keeps the single neutral event (A). *Cost:* the
  cause still has to cross the SPI reply (Safety, Class C). Useful to the person and their clinician; optional on top of A.

**Constraint on any option:** no cutoff timing, threshold or grant behaviour changes. The repair is to what is *reported*, never to what *cuts*.

## Q3. May the NVSTATE fault slot stay as it is? (point 7; Safety)

### Present state (checked)

`np_safety_main.c:677-681` sets `fault_slot = NP_FAULT_SLOT_NVSTATE` when a cardiac flag write (SET after a cutoff, CLR after a re-enable) fails
`NP_NV_WRITE_ATTEMPTS` times, and `:713` does the same when an active-user change fails to persist. The slot reaches the hub in the reply frame and is
SHDR (`np_fault_latch.c`, "slot → SHDR"). So an NVSTATE fault tells the fleet that one of two kinds of write was in flight: a cardiac flag write, or a
change of active user. The finding holds.

**What it adds.** A cardiac flag SET follows a cutoff the SHDR log already records (Q2), so that half adds nothing. A CLR follows a re-enable, which
SHDR also records (`NP_CVNS_SHDR_EV_REENABLED`). The user-change half says that more than one person uses the helmet, at that moment; it is a weak
signal and is already ambiguous with the cardiac half. The fault itself is a device-condition fact (the MCU's persistent store failed to take a write),
which predictive maintenance needs.

### Options

- **A. Accept with this reason (recommended).** No change to the Class C code. The row records why. *Cost:* the weak "more than one user" inference
  stays.
- **B. A distinct, cause-free store-failure slot.** Any persistent-store write failure on the MCU reports one slot, whatever it was writing, so the
  slot never narrows the cause. Equivalent to today in effect (the slot is already shared by the two causes) unless more writers are added later.
  *Cost:* a Class C change with no present privacy gain.

## Recommendation

Q1 A, Q2 A (optionally C later), Q3 A. Q2 A and Q3 A change no firmware behaviour. Q1 A is Firmware and app work and moves nothing toward SHDR.

## Ruling (the principal, 2026-11-23)

**Q1: the UHDR session order is per person, not per helmet; option A of the follow-up below.** The principal did not take this brief's
option A (a separate per-helmet UHDR counter). In their words: UHDR should have a user-specific session count, because in a clinic a person
may use several helmets over time and a per-helmet value collides; and a user-specific count also stops SHDR and UHDR being joined on it.
Mode 3 runs sessions with no phone, so the helmet cannot ask for the person's next number. Two designs were put to the principal in chat:

- **A (chosen).** The helmet names each UHDR session with a random 128-bit ID drawn at session start (no collision across helmets, joins to
  nothing), and the app assigns the person's own sequence number when the session reaches it, counting across every helmet.
- B (not chosen). The app hands the helmet the person's next number with the active user, and the helmet counts on from it in Mode 3. It
  collides when one person runs Mode 3 on two helmets between syncs.

The device session count stays SHDR only. The cervical fault summary's ordering and the apps' acknowledgement ledger follow the same rule.
Not built; the work stays under `OI-PRIV-41`, alongside `OI-PRIV-34`'s per-person sealing on a shared helmet.

**Q2: option A.** SHDR keeps one cardiac cutoff event and never learns its cause. In the principal's words: SHDR does not need to know why it
happened, since it does not bear on hardware warranty, and it should not know, because the cause is UHDR. The fleet label becomes
`CVNS_CARDIAC_INTERLOCK_CUTOFF`. No firmware behaviour changes. Whether SHDR carries the cutoff at all is `OI-PRIV-22` (open); this ruling
governs its form if it does. Option C (the cause in the person's UHDR session) was not asked for and is not taken up.

**Q3: option A, accepted.** The NVSTATE slot stays as it is, for the reasons in Q3.

Recorded in `docs/reference/data-architecture-detail.md` §5.1, `docs/status/completed-decisions.md` (2026-11-23) and the `OI-PRIV-41` row.

