# Cervical VNS Safety Interlock Firmware Specification

**Project:** NeurOne
**Document:** NP-FW-CVNS-001
**Revision:** 11
**Date:** 2026-10-01
**Status:** BASELINED
**Effective Date:** 2026-08-05
**Author:** Steve Hickman (CEO, interim Quality authority)
**Approved By:** Steve Hickman, CEO
**References:** CLAUDE.md §3 T2 additions (cervical VNS accessory); NP-HW-CVNS-001 Rev 1 (the A14 hardware specification, from 2026-09-20)
**Related Issues:** GitHub Issue #24; GitHub Issue #343 (§9 FAI serial disposition); GitHub Issue #332 (A14 hardware specification — issued 2026-09-20 as NP-HW-CVNS-001)
**Gate:** NP-COORD-001 G3-08
**IEC 62304 Class:** SW-01 Class C (safety MCU) / SW-02 Class B (main processor)
**Supersedes:** NP-FW-CVNS-001 Rev 10
**Parent Document:** NP-SW-001

---

**Rev 11 (2026-10-01): OI-CVNS-13 analysed. The hub now pulses `RPEAK_IN` for long intervals, and the item stays open for the residual rate (principal decision, 2026-10-01).**
- **A false-trip path no simulation had modelled.** The hub (`np_cvns_interlock.c`, §6.2 step 7) pulsed `RPEAK_IN` only for intervals inside 300–2000 ms. Below 60 BPM, one missed detection makes an interval over 2000 ms, so the next real beat was not pulsed either. The safety MCU then saw a gap over 3 s, and its staleness cutoff (§5.4 step 4) fired. At 50 BPM with 1 % of beats missed, 61–64 % of 120 s sessions tripped. §14.5.1 reported ≤ 2 %, because its simulation fed the MCU raw detections.
- **Changed (Class B):** the hub pulses every peak ≥ `NP_CVNS_RR_MIN_VALID_MS` after the last one, with no upper bound. Its own buffer filter is unchanged. At 50 BPM with 1 % missed, trips fall to 0.3–0.7 %. Every qualifying step and dropped-beat case is still cut (§14.6).
- **No Class C code, constant or threshold changed.**
- **Levers rejected, with evidence (§14.6):**
  - A median estimator never detects a dropped-beat bradycardia (every 4th beat absent, 70 → 52.5 BPM: 0 %), and it misses some ±16 BPM steps.
  - A 12-interval window breaks the 18 s detection bound (19.8 s).
  - A 1 s persistence requirement barely helps.
  - An MCU refractory adds nothing behind the hub's 300 ms bound.
- **Still open: the residual.** It is missed detections at ≥ 70 BPM and split detections at 50–70 BPM. The safety MCU cannot filter a missed detection, because it is indistinguishable from a physiologically dropped beat, which is the hazard. What settles it is measured detection quality on A13 (FAI-CV03 or earlier) against a clinical acceptable nuisance rate (§14.6).
- **Firmware:** `np_cvns_interlock.c` (step 7 split into forward and buffer decisions), plus the `rpeak_pulses` counter and `np_cvns_interlock_rpeak_pulses()`. Host test: `np_cvns_fai_tests` `fai_rpeak_forwarding`, which fails if the upper bound is restored on the pulse gate. Simulation: `firmware/safety_mcu/tests/analysis/run_oi_cvns_13_sim.sh`.

**Rev 10 (2026-10-01): OI-CVNS-12 closed. The cardiac interlock compares against an 18 s history, not a refreshed baseline (principal decision, 2026-10-01).**
- **The rule.** The safety MCU snapshots its 8-interval mean every 1 s and keeps 18 snapshots. It cuts when the current mean differs from **any** of them by more than 15 BPM. The unconditional 5 s baseline refresh (`NP_CARDIAC_OBS_MS`) is retired. That refresh was the race that absorbed a step in transit (§5.3 step 6, §5.4 step 2).
- **Constants.** `NP_CARDIAC_HR_SNAP_MS` = 1 000 and `NP_CARDIAC_HR_HIST_LEN` = 18. The derivation is §5.3: 1 s of snapshot phase, plus the 5 s spread CLAUDE.md §4.2 allows, plus 8 intervals at 40 BPM. The 40 BPM floor is the principal's. The window stays at 8 intervals, and the 15 BPM threshold and the strict `>` are unchanged.
- **Detection and cutoff are now separate requirements** (CLAUDE.md Rev 61 §4.2, principal). A qualifying change is detected once the 8-interval mean has turned over, no later than 18 s after it began. The GPIO is cut within 100 ms of detection. FAI-CV02 is amended to measure each (§9).
- **Result.** On the §14.5 simulation, every instantaneous step and every 2.5–5 s ramp of 16–40 BPM, from 50–110 BPM, is cut in 100 % of trials (Rev 9: 0–100 %). That includes the 70 → 50 BPM fall that was never cut and FAI-CV02's 70 → 90 step, which was cut in 31 % (§14.5.1).
- **Cost: more false trips (new OI-CVNS-13).** On white-noise R-R jitter with missed and split beats, false trips per 120 s session rise from 0–93 % to 0–100 %. The rise is largest at 100 BPM, and at 70 BPM with 3 % artefacts (6 % → 36 %). A false trip is a fail-safe cutoff, but each costs a 30 s lockout and an app confirmation. OI-CVNS-11's validity filter is the lever.
- **Firmware:** `np_cardiac_interlock.c`, `np_safety_config.h`. Host tests: `test_refresh_absorbs_sustained_fall_KNOWN_DEFECT` and `test_rolling_baseline_absorbs_slow_drift` are replaced by seven real-time tests. Two of them pin the 18 (a 12 s horizon fails one, and a 16 s horizon fails the other), and one pins the history reseed at lockout expiry. All are mutation-checked. The executed-line floor rises from 524 to 591.
- **Not changed:** RISK-25 is not re-scored. That is still the Quality Lead's act, and `NP-RISK-002` Rev 9 records it as owed.

**Rev 9 (2026-09-29): OI-CVNS-10 closed as moot, and a defect larger than the question found behind it (new OI-CVNS-12).**
- **OI-CVNS-10 (window 8 vs 5) is closed without changing either number.** Both candidates rested on the ±5 BPM main-vs-MCU baseline cross-check. That check was never built. The safety MCU's wire protocol has no baseline command, the main-processor module sends 0x13 through a stub, and `NP-FMEA-001` Rev 12 had already withdrawn the claim. With no comparison between them, the two windows need not agree and are not related. The MCU's window is a parameter of its own detector (§14.3).
- **§5.3 step 5 and §6.3 corrected.** They still said the MCU rejects an enable when the baselines disagree. It does not.
- **New OI-CVNS-12: the interlock misses most qualifying heart-rate changes.** The 5 s baseline refresh (`NP_CARDIAC_OBS_MS`) is unconditional. It adopts the mean before a step has finished moving through the 8-interval window. At 70 BPM, a sustained 20 BPM fall is never cut, and a 20 BPM rise is cut in about 30 % of cases. That rise is FAI-CV02's own test step. CLAUDE.md §4.2 requires a cutoff for any change over 15 BPM within 5 s. A 5-interval window is better but still fails (§14.5).
- **Evidence.** A simulation that links the real `np_cardiac_interlock.c` (§14.5). A new host test pins the defect: `test_refresh_absorbs_sustained_fall_KNOWN_DEFECT` passes today by asserting it, and fails if the refresh is removed.
- **No firmware behaviour, constant or threshold changed.** OI-CVNS-12 is a Class C design decision for the principal. `NP-RISK-002` Rev 8 records its effect on RISK-25 control C1.

**Rev 8 (2026-09-25): the safety MCU's cardiac interlock is no longer blind before it arms or after R-peaks stop (`NP-RISK-002` OI-RISK2-05, principal decision; GitHub #437).**
- **§5.4 step 3, pre-arm hold.** `NP_SAFETY_EN_CVNS` is withheld until 8 intervals arm the baseline. Before this, CVNS was granted on request, and the interlock could not fire until armed.
- **§5.4 step 4, staleness.** While CVNS is granted, 3 s with no R-peak edge is a cardiac cutoff.
- **§5.4 preamble corrected.** The check runs every main-loop iteration; there is no TIM6 ISR.
- Firmware: `np_cardiac_interlock.c`, `NP_CARDIAC_RPEAK_STALE_MS`, `np_cardiac_interlock_arm_reset()`, called by main on each new CVNS request. Host-tested (`np_cardiac_interlock_tests`, 6 new tests; mutations caught). `NP-FMEA-001` Rev 12 re-authors §3.5 against it.
- **Operational effect:** cervical VNS starts about 8 heartbeats after the hub requests it. The hub waits without faulting.
- **Not done here:** the safety MCU does not report *which* condition cut. A staleness cut and a heart-rate cut both set CARDIAC with fault slot 10. The hub's UHDR `fault_reason` comes from its own PPG path, which records `DATA_LOSS` only at its 10 s timer. Distinguishing the two in the heartbeat reply would need a wire-format change, which is left for the owner to decide.

**Rev 7 (2026-09-23): §13 re-scored — RISK-25 is S5 × P2 = ALARP, not "Critical (S4) … Low" (`NP-RISK-002` §4.3).**
- **Severity.** "Critical (S4)" mixed two scale levels. `NP-RM-001` §4.1 names cardiac arrhythmia
  from cervical VNS as its S5 example.
- **Rating.** "Low" is not a rating the matrix produces at S5.
- **Residual probability.** It was P1, resting partly on `NP-FMEA-001` FMEA-M05-06's battery-backed
  lockout, which cannot exist on this device. It is now P2 until the controls are verified on
  hardware, with P1 as the target.

No requirement, threshold or implementation changed. Ratings approved by the Quality Lead (interim: Steve Hickman, CEO) 2026-09-23.

**Rev 6 (2026-09-23): the cardiac cutoff survives power loss, is held per user, and withholds cervical VNS only; the UHDR record says which fault stopped the session (`NP-SW-FAULTMSG-001` Rev 2, principal decisions 2026-09-22).** Three changes.
- **§5.4 step 2c now holds across a power-on reset.** It used to hold only while the device stayed powered, and the headset has no battery. In Mode 3, unplugging the power bank cleared the lockout, and `REQ-CVNS-09`'s app confirmation was skipped (`OI-FAULTMSG-01`). The safety MCU now records the cutoff in its own flash (new §5.4.1).
- **The cutoff is scoped.**
  - It withholds `NP_SAFETY_EN_CVNS` only (`NP_CARDIAC_BLOCK_MASK`). Before this revision, CARDIAC was in the all-channel cutoff mask, which stopped every modality.
  - It is held for the user who triggered it, and nobody else.
- **§4.5 carries `fault_reason`.** `cutoff_occurred` is set only for a heart-rate change, and `cutoff_hr_at_event_bpm` is now written (`OI-FAULTMSG-02`). Before, every interlock fault set `cutoff_occurred`, so a lost R-peak signal read as a cardiac cutoff.

§13 is updated to match. No threshold, latency, lockout duration or SHDR field changed.

**Rev 5 (2026-09-20): §9's note said no A14 hardware specification exists; one does now, and the conclusion it supported is unchanged.** GitHub #332 issued **`NP-HW-CVNS-001` Rev 1**, the mechanical and electrical specification for the electrode assembly, cable and connector. `NP-FAI-CVNS-001` remains unwritable and remains a named absence in `NP-ART-001` §3.2 — `NP-FAI-001` §2 F1 requires `BASELINED` or `ACTIVE` and that document is DRAFT — so **only the stated reason changes**, from *absence* to *maturity* (`NP-FAI-001` §2.2, and OI-FAI-07 re-scoped rather than closed). §9's header note and §10's Rev 4 note are updated to say so. **`NP-HW-CVNS-001` `REQ-CVNS-12` records that FAI-CV01's placement, impedance and open-detection criteria are A14's acceptance criteria carried by a firmware test procedure, and that the artifact checklist inherits them when F1 is met — nothing moves out of §9, which stays the test specification and the record of file.** **No constant, limit, criterion, item number, test procedure or firmware behaviour changed. IEC 62304 SW-01 Class C / SW-02 Class B: no code, no interface and no verification is affected.**

**Rev 4 (2026-09-14): §9 stops being cited as a document that does not exist.** `firmware/CMakeLists.txt` registered `np_cvns_fai_tests` as *"NP-FAI-CVNS-001: cardiac interlock FAI suite"* and the binary printed that serial as its own document header. Neither half held: the serial has never been written, and the suite tests the main-processor module rather than the Class C interlock. GitHub #343 / `NP-FAI-001` §2.1 retire the citation — **§9 is the test specification** — while `NP-FAI-CVNS-001` itself stays a *named absence* in `NP-ART-001` §3.2, because A14 still has no hardware specification (#332) and §2 F1 therefore fails. §9 gains a header note saying so, and §10's note is corrected. The Rev 3 banner below is retained as written; its description of the old registration is a record of what was true then. **No constant, limit, criterion, item number or firmware behaviour changed.**

**Rev 3 (2026-08-12): §8.2 contradicted itself on the cutoff time offset; resolved to UHDR.** This document classified *Time-of-cutoff offset (s after stim onset)* as **UHDR — user biology**, and then two rows later routed the *Safety MCU fault log (offset, reason, no HR)* to **SHDR** — where §5.6's `np_cvns_fault_log_entry_t` carried that same quantity as `cutoff_offset_ms`, at **finer** resolution than the UHDR original. One datum, two partitions, the SHDR copy the more revealing of the two.

Resolved in favour of UHDR, which is both the conservative reading (CLAUDE.md §5.1: when in doubt → UHDR) and the substantively correct one: for `reason = HR_CHANGE` the offset is the latency from stimulation onset to the wearer's heart rate deviating past `NP_CARDIAC_HR_DELTA_BPM` — an **autonomic response latency**, a measurement of the person. The UHDR session record (§4.5) already carries it as `cutoff_time_offset_s`, so nothing is lost. `cutoff_offset_ms` is removed from the SHDR record (`firmware/cervical_vns/include/np_cvns_types.h`; struct 12 → 8 bytes), **unconditionally** — zeroing it only for the cardiac reason would have recreated the oracle retired from the fault latch the same day, and would leak doubly here because `reason` is in the same record. `reason` is retained: the fault *kind* is the deliberate, already-published SHDR channel (`fault_log.fault_type = 'CVNS_HR_CUTOFF'`). The struct had no producer and no consumer, so no data path or stored record was affected. §8.2, §11 and the stale §11 cross-reference (NP-FW-EMMC-001 "Rev 1 §12, 27 elements") updated; the element is now carried explicitly in NP-FW-EMMC-001 Rev 2 §12, which was silent on it — the silence is why the contradiction survived two revisions. IEC 62304 SW-01 Class C / SW-02 Class B: record-shape change only, no interlock behaviour altered.

*Revision numbering note:* revisions are positive integers per NP-CONV-001 Rev 6 §4.1; the change note below is headed "Rev B", which is this document's Rev 2 under the published letter→integer mapping (B ≡ 2).

**Rev B (2026-08-05):** Reconciles §5 (safety MCU, Class C) with the firmware it specifies. Rev 1 §5 described the safety MCU using the **main processor's** constants and behaviours — the root cause of most of the divergences below — and specified one operation the firmware cannot perform.

*Corrected outright (documentation errors; no firmware behaviour changed):*
- **§5.3 constants.** Rev 1 cited `NP_CVNS_RR_WINDOW_SIZE` (20) and `NP_CVNS_BASELINE_BEATS_MIN` (5) as the safety MCU's. Both live in `firmware/cervical_vns/include/np_cvns_config.h` — the **main-processor** module specified in §6 of this same document. The safety MCU has its own header and its own values (`NP_CARDIAC_BASELINE_BEATS` = 8, `NP_RR_BUF_SIZE` = 8). §5.3 now names the safety MCU's own constants and states explicitly that the two sides compute baselines independently and cross-validate at ±5 BPM — which is the *reason* they are separate implementations.
- **§5.3 step 2 R-R validity window.** The 300–2000 ms discard rule is main-processor behaviour; the safety MCU applies no such filter. Marked as not implemented on the MCU side and escalated (OI-CVNS-11).
- **§5.4.2a dual-enable cutoff.** Rev 1 instructed asserting `CVNS_ENABLE_L` **and** `CVNS_ENABLE_R`. The firmware has a single CVNS enable (`NP_EN_CVNS`); the two-line assertion is unimplementable as written. §5.4.2a now describes the cutoff the firmware actually performs, and the enable-line count is escalated (OI-CVNS-09).
- **§5.4.3 conservative hold.** The "fewer than 3 valid intervals → warning flag → 10 s → soft cutoff" behaviour is the main processor's (`NP_CVNS_DATA_LOSS_TIMEOUT_S`). The safety MCU's hold is different and stricter: it will not arm at all until 8 intervals have accumulated, and it has no soft-cutoff timer. Corrected to describe the MCU.
- **§5.1 pin map.** Marked **unverified** rather than corrected in either direction — see OI-CVNS-08. Rev 1's map is retained verbatim as the claim under dispute; it is no longer presented as the requirement.

*Escalated, deliberately NOT resolved here* — each is a design or clinical decision, not a documentation defect: **OI-CVNS-08** (STM32G071 pin map), **OI-CVNS-09** (single vs dual CVNS enable line), **OI-CVNS-10** (baseline window 5 vs 8), **OI-CVNS-11** (R-R validity filtering on the MCU). Each carries two candidate resolutions, the evidence for each, and what would settle it. No number in either the doc or the firmware was changed to make the other side agree.

*Verification added:* `firmware/safety_mcu/tests/np_cardiac_interlock_tests.c` — the first host-test coverage of the Class C cardiac interlock. Rev 1's only "cardiac interlock" suite (`np_cvns_fai_tests`, registered as NP-FAI-CVNS-001) exercises the **main-processor** module on the other side of the SPI boundary and asserts on main-processor constants; it never touched the unit that owns the enable GPIO. The new suite pins current behaviour, including the constants under OI-CVNS-10, so any later change surfaces as a reviewed failing assertion rather than a silent edit.

---

## 1. Scope

This document specifies the firmware implementation of the NeurOne cervical VNS (tcVNS) T2 accessory safety interlock. The accessory stimulates the cervical vagus trunk via transcutaneous gel electrodes placed over the carotid sheath. Because the current path runs near the carotid artery and vagus nerve trunk, a dedicated cardiac safety interlock in the STM32G071 safety MCU owns stimulation enable and monitors cardiac rhythm in real time.

Modules covered:

| Module | Location | Description |
|--------|----------|-------------|
| `np_cvns_interlock` | `firmware/cervical_vns/` | Cardiac interlock state machine (main processor side) |
| `np_cvns_stim` | `firmware/cervical_vns/` | Stimulation delivery, ramp control, charge balance |
| `np_cvns_session` | `firmware/cervical_vns/` | Session orchestration, prechecks, UHDR/SHDR record |

**Safety MCU bare-metal behavior** is specified in §5 below; the C implementation of the safety MCU firmware is separate (IEC 62304 Class C, ~500 lines bare-metal on STM32G071, certified independently).

**Not in scope:** auricular taVNS (handled by HRV biofeedback firmware, NP-FW-HRV-001 Rev 1); hub accessory port electrical driver.

---

## 2. Clinical Context

### 2.1 Indication and predicate

| Item | Detail |
|------|--------|
| Target anatomy | Cervical vagus nerve trunk, stimulated transcutaneously via skin overlying the carotid sheath |
| Electrode placement | Bilateral or unilateral gel electrodes; left or right lateral neck |
| Stimulation modality | Biphasic charge-balanced electrical stimulation |
| Indication (T2 target) | Cluster headache, migraine, depression, PTSD, post-stroke rehabilitation |
| FDA predicate | electroCore gammaCore K163334 (cluster headache, 2016) and K173323 (migraine, 2017) |
| Regulatory pathway | 510(k); substantial equivalence to gammaCore predicates (see NP-REG-CVNS-001 Rev 1) |

### 2.2 Rationale for safety MCU ownership

The carotid sheath contains the common carotid artery, internal jugular vein, and vagus nerve trunk. Electrical stimulation in this region can influence:
- Baroreceptor reflex (carotid sinus → HR and blood pressure changes)
- Direct vagal efferent activity (cardiac rate, AV conduction)

The safety MCU (STM32G071, bare-metal, IEC 62304 Class C) must own the cervical VNS enable GPIO independently of the main processor. An app crash, main processor fault, or SPI communication loss must result in stimulation cutoff within the watchdog interval (1.5 s), but the cardiac interlock cutoff must occur within 100 ms of detecting a qualifying HR change — this is faster than the SPI heartbeat period and is therefore implemented by the safety MCU independently using a dedicated R-peak GPIO signal from the main processor.

---

## 3. Configuration Constants (`np_cvns_config.h`)

### 3.1 Stimulation parameters

| Constant | Value | Description |
|----------|-------|-------------|
| `NP_CVNS_FREQ_HZ_MIN` | 1 | Minimum stimulation frequency (Hz) |
| `NP_CVNS_FREQ_HZ_MAX` | 25 | Maximum stimulation frequency (Hz) — matches gammaCore range |
| `NP_CVNS_CURRENT_UA_MAX` | 2000 | Maximum current: 2 mA (CLAUDE.md §3 T2 spec) |
| `NP_CVNS_PULSE_WIDTH_US_MIN` | 200 | Minimum pulse width (µs) |
| `NP_CVNS_PULSE_WIDTH_US_MAX` | 1000 | Maximum pulse width (µs) |
| `NP_CVNS_SESSION_MIN_S` | 60 | Minimum session duration (s) |
| `NP_CVNS_SESSION_MAX_S` | 120 | Maximum session duration (s) — gammaCore protocol: 2 min |

### 3.2 Electrode and contact

| Constant | Value | Description |
|----------|-------|-------------|
| `NP_CVNS_ELECTRODE_COUNT` | 2 | Bilateral electrode assembly |
| `NP_CVNS_IMPEDANCE_MAX_KOHM` | 5.0 | Cervical gel electrode impedance limit (kΩ) |
| `NP_CVNS_IMPEDANCE_CHECK_FREQ_HZ` | 1000 | AC impedance check frequency (1 kHz) |

### 3.3 Ramp timing

| Constant | Value | Description |
|----------|-------|-------------|
| `NP_CVNS_RAMP_UP_S` | 10 | Current ramp-up period (s) — comfort and vagal adaptation |
| `NP_CVNS_RAMP_DOWN_S` | 5 | Current ramp-down period (s) |

### 3.4 Cardiac interlock

| Constant | Value | Description |
|----------|-------|-------------|
| `NP_CVNS_HR_CHANGE_LIMIT_BPM` | 15 | Cutoff threshold (BPM change from baseline) — CLAUDE.md §3 |
| `NP_CVNS_HR_WINDOW_S` | 5 | Observation window (s) |
| `NP_CVNS_CARDIAC_POLL_MS` | 5 | Safety MCU cardiac polling interval (200 Hz) |
| `NP_CVNS_CUTOFF_LATENCY_MAX_MS` | 100 | FAI-CV02 pass criterion: GPIO low within 100 ms of detection |
| `NP_CVNS_BASELINE_BEATS_MIN` | 5 | Minimum beats to establish stable baseline before enable |
| `NP_CVNS_RR_WINDOW_SIZE` | 20 | Rolling R-R interval buffer depth (samples) |
| `NP_CVNS_RR_MAX_VALID_MS` | 2000 | Maximum valid R-R interval (≈30 BPM) |
| `NP_CVNS_RR_MIN_VALID_MS` | 300 | Minimum valid R-R interval (≈200 BPM) |
| `NP_CVNS_RPEAK_DEBOUNCE_US` | 30 | R-peak GPIO edge debounce (µs) |

### 3.5 Re-enable policy

| Constant | Value | Description |
|----------|-------|-------------|
| `NP_CVNS_REENABLE_LOCKOUT_S` | 30 | Minimum lockout period after cardiac interlock cutoff |

After any cardiac interlock cutoff, re-enabling requires **all three** of:
1. Lockout period elapsed (`NP_CVNS_REENABLE_LOCKOUT_S`)
2. Explicit app confirmation command over signed session protocol
3. Repeat impedance check (both electrodes must pass) and cardiac baseline re-established

---

## 4. Type Definitions (`np_cvns_types.h`)

### 4.1 Status codes

```c
typedef enum {
    NP_CVNS_OK                    =  0,
    NP_CVNS_ERR_INVALID_ARG       = -1,
    NP_CVNS_ERR_IMPEDANCE_HIGH    = -2,  /* electrode impedance exceeds limit     */
    NP_CVNS_ERR_SAFETY_REJECTED   = -3,  /* safety MCU denied enable              */
    NP_CVNS_ERR_CARDIAC_CUTOFF    = -4,  /* cardiac interlock triggered           */
    NP_CVNS_ERR_BASELINE_INVALID  = -5,  /* baseline HR not yet established       */
    NP_CVNS_ERR_SESSION_ACTIVE    = -6,
    NP_CVNS_ERR_NO_SESSION        = -7,
    NP_CVNS_ERR_LOCKOUT           = -8,  /* re-enable lockout active              */
    NP_CVNS_ERR_CHARGE_LIMIT      = -9,  /* charge density limit would be exceeded*/
    NP_CVNS_ERR_WAVEFORM_INVALID  = -10, /* biphasic balance check failed         */
} np_cvns_status_t;
```

### 4.2 Interlock state machine

```
PRE_SESSION ──[impedance OK + baseline established]──► ENABLED
    ENABLED ──[HR change > 15 BPM]──────────────────► FAULT
    ENABLED ──[session end]──────────────────────────► IDLE
      FAULT ──[lockout elapsed + app confirm + re-check]──► PRE_SESSION
```

```c
typedef enum {
    NP_CVNS_INTERLOCK_IDLE        = 0,
    NP_CVNS_INTERLOCK_PRE_SESSION = 1,  /* impedance + baseline checks           */
    NP_CVNS_INTERLOCK_ENABLED     = 2,  /* stimulation active; cardiac monitoring */
    NP_CVNS_INTERLOCK_FAULT       = 3,  /* cutoff; lockout active                */
} np_cvns_interlock_state_t;
```

### 4.3 Session workflow stage

```c
typedef enum {
    NP_CVNS_STAGE_IDLE         = 0,
    NP_CVNS_STAGE_IMPEDANCE    = 1,  /* impedance check in progress             */
    NP_CVNS_STAGE_BASELINE     = 2,  /* cardiac baseline accumulation           */
    NP_CVNS_STAGE_RAMP_UP      = 3,  /* current ramping to target              */
    NP_CVNS_STAGE_ACTIVE       = 4,  /* full stimulation                        */
    NP_CVNS_STAGE_RAMP_DOWN    = 5,  /* current ramping to zero                */
    NP_CVNS_STAGE_COMPLETE     = 6,  /* session ended normally                  */
    NP_CVNS_STAGE_FAULT        = 7,  /* cardiac interlock or safety rejection   */
} np_cvns_stage_t;
```

### 4.4 Stimulation phase

```c
typedef enum {
    NP_CVNS_STIM_IDLE      = 0,
    NP_CVNS_STIM_RAMP_UP   = 1,
    NP_CVNS_STIM_ACTIVE    = 2,
    NP_CVNS_STIM_RAMP_DOWN = 3,
    NP_CVNS_STIM_DONE      = 4,
    NP_CVNS_STIM_FAULT     = 5,
} np_cvns_stim_phase_t;
```

### 4.5 UHDR session record

Written to the UHDR partition at session end, AES-256-XTS encrypted with the user's biometric-derived key. NeurOne never holds the decryption key.

```c
typedef struct {
    uint32_t session_start_unix;     /* UTC epoch seconds                       */
    uint32_t session_duration_s;     /* actual stimulation duration              */
    uint16_t stim_freq_hz;           /* programmed frequency                    */
    uint16_t stim_current_ua;        /* programmed current at end of ramp       */
    uint16_t stim_pulse_width_us;    /* pulse width                             */
    uint8_t  electrode_config;       /* 0=bilateral, 1=unilateral left, 2=right */
    uint8_t  cutoff_occurred;        /* 1 if cardiac interlock fired            */
    uint16_t cutoff_hr_baseline_bpm; /* baseline HR at session start (× 10)    */
    uint16_t cutoff_hr_at_event_bpm; /* HR at cutoff moment (× 10)             */
    uint32_t cutoff_time_offset_s;   /* seconds after stim onset (0 if no cut) */
    float    impedance_left_kohm;    /* pre-session impedance                   */
    float    impedance_right_kohm;
    float    mean_impedance_kohm;
    uint8_t  abort_reason;           /* 0=normal, else np_cvns_status_t        */
    uint8_t  fault_reason;           /* np_cvns_fault_reason_t; NONE if normal  */
    uint8_t  reserved[2];
} np_cvns_session_record_t;
```

**Rev 6.**
- `cutoff_occurred` is 1 **only** for `NP_CVNS_FAULT_HR_CHANGE`, the cardiac cutoff.
- `fault_reason` records which interlock fault stopped the session: HR change, R-peak data loss, watchdog, safety MCU or pad impedance. It takes one of the reserved bytes, so the record size is unchanged.
- `cutoff_hr_at_event_bpm` is written at the cutoff.

Before Rev 6, every interlock fault set `cutoff_occurred`, and the event HR was never written, so the user's own record could not say which fault happened (`NP-SW-FAULTMSG-001` F2).

### 4.6 SHDR session summary

Written to the SHDR partition. Contains device metrics only — no user biology.

```c
typedef struct {
    uint8_t  electrode_config;
    uint16_t stim_freq_hz;
    uint16_t stim_current_ua;
    uint32_t stim_duration_s;
    float    impedance_left_kohm;
    float    impedance_right_kohm;
    uint8_t  impedance_check_pass;   /* 1 if both electrodes passed             */
    uint8_t  cutoff_occurred;        /* 1 if cardiac interlock fired — no HR data*/
    uint8_t  abort_reason;           /* 0=normal                                */
} np_cvns_shdr_summary_t;
```

**UHDR/SHDR boundary:** HR time series, baseline HR, cutoff HR, and all cardiovascular signal data are UHDR (user biology). The bare fact that a cutoff occurred (without the HR values) is SHDR (device safety event log). Per CLAUDE.md §5.1 boundary resolution rule: when in doubt → UHDR.

---

## 5. Safety MCU Bare-Metal Cardiac Interlock (STM32G071)

This section specifies the STM32G071 safety MCU bare-metal behavior. The C implementation is certified separately under IEC 62304 Class C.

### 5.1 Hardware interfaces

> **⚠ UNVERIFIED — do not build to this table.** Every row below disagrees with `firmware/safety_mcu/include/np_safety_config.h`, and there is no schematic in this repository to settle which is intended (`hardware/` holds only `np_socket_map.json` and `np_helmet_surface.json`). The table is retained verbatim as Rev 1's claim, not as the requirement. **Tracked as OI-CVNS-08 (pin map) and OI-CVNS-09 (enable-line count); both are BLOCKING on PCB layout (gate G1).** `np_safety_config.h` itself carries the caveat "GPIO bank assignments are provisional pending PCB layout (G1 gate)", so neither side is currently authoritative.

| Signal | Direction | STM32G071 pin (Rev A claim) | In-tree firmware | Description |
|--------|-----------|------------------------------|------------------|-------------|
| `CVNS_ENABLE_L` | Output | PA5 | *no such line* | Active-low cervical VNS enable GPIO — left electrode driver |
| `CVNS_ENABLE_R` | Output | PA6 | *no such line* | Active-low cervical VNS enable GPIO — right electrode driver |
| — | Output | — | **`NP_EN_CVNS` on PB5** | The single active-low CVNS enable the firmware actually owns |
| `RPEAK_IN` | Input | PB0 | **PA8** (`NP_RPEAK_IN_PIN`) | R-peak pulse from main processor (rising edge, 5 ms) |
| `SPI1_SCK/MOSI/MISO/NSS` | SPI slave | PA1–PA4 | **PA5/PA6/PA7 + PA4 NSS** per the header comment at `np_safety_config.h:24` | SPI interface to main processor |
| `HEARTBEAT_WATCHDOG` | Input | PA7 | *no such pin* — the heartbeat is **in-band on SPI** (`NP_SAFETY_WDG_TIMEOUT_MS` = 1500) | SPI heartbeat; 1.5 s timeout → force cutoff |

Note that PB0 in the firmware is `NP_EN_BES_PIN`, the BES stimulation **enable output** — so Rev 1's map places a cardiac *input* on a stimulation *output*. That single row is sufficient reason not to treat this table as buildable.

### 5.2 R-peak GPIO protocol

The main processor signals each detected R-peak to the safety MCU via a 5 ms active-high pulse on `RPEAK_IN`. The safety MCU measures the interval between consecutive rising edges using TIM2 (32-bit, 1 MHz tick, period = 1 µs) to compute instantaneous HR.

Debounce: any edge detected within 30 µs of the previous edge is discarded (`NP_CVNS_RPEAK_DEBOUNCE_US`).

### 5.3 Baseline establishment

**Two independent baselines exist; do not conflate them.** The main processor computes its own baseline from PPG-derived R-peaks using the §3.4 constants in `firmware/cervical_vns/include/np_cvns_config.h`. The safety MCU computes its own, from the `RPEAK_IN` pulse train, using its own constants. The two sides are **separate implementations**. *Rev 9: this sentence went on to say that separation is what made the ±5 BPM cross-validation in step 5 meaningful. No such cross-validation exists (step 5).* Rev 1 of this section attributed the main processor's constants to the safety MCU; the safety MCU has never used them.

Safety MCU constants — the values that actually govern §5.3 and §5.4:

| Constant | Value | Defined in | Meaning |
|----------|-------|-----------|---------|
| `NP_RR_BUF_SIZE` | 8 | `src/np_cardiac_interlock.c` (file-private, not the header) | R-R ring-buffer depth |
| `NP_CARDIAC_BASELINE_BEATS` | 8 | `include/np_safety_config.h` | Intervals required before the interlock arms |
| `NP_CARDIAC_HR_DELTA_BPM` | 15 | `include/np_safety_config.h` | Cutoff threshold (strict `>`) |
| ~~`NP_CARDIAC_OBS_MS`~~ | ~~5 000~~ | — | ~~Rolling-baseline refresh window~~ **Retired Rev 10 (OI-CVNS-12).** The unconditional refresh absorbed a step in transit (step 6) |
| `NP_CARDIAC_HR_SNAP_MS` | 1 000 | `include/np_safety_config.h` | Cadence of heart-rate snapshots (Rev 10) |
| `NP_CARDIAC_HR_HIST_LEN` | 18 | `include/np_safety_config.h` | Snapshots kept, so the comparison horizon is 18 s (Rev 10; derivation below) |
| `NP_CARDIAC_LOCKOUT_MS` | 30 000 | `include/np_safety_config.h` | Re-enable lockout after a cutoff |

> **OI-CVNS-10 — closed 2026-09-29 (Rev 9), moot.** The safety MCU uses an 8-interval window and the main processor uses 5 (`NP_CVNS_BASELINE_BEATS_MIN`). Nothing compares the two baselines, so they need not agree. The MCU's window is now a parameter of **OI-CVNS-12**, because the window and the 5 s refresh together decide what the interlock can detect. **Still do not harmonise by editing one number to match the other** (§14.3).

Before stimulation is enabled:
1. Safety MCU accumulates R-R intervals in a circular buffer of depth `NP_RR_BUF_SIZE` (8). The first R-peak after init or re-enable is a priming edge and produces no interval, so 8 intervals require 9 edges.
2. **Not implemented on the safety MCU.** Rev 1 specified discarding intervals outside [`NP_CVNS_RR_MIN_VALID_MS`, `NP_CVNS_RR_MAX_VALID_MS`] (300–2000 ms); those are main-processor constants and the safety MCU applies no validity filter — every measured interval enters the buffer. A physiologically impossible interval is handled downstream instead, by saturating the BPM conversion at `INT16_MAX` (see `rr_to_bpm()`). Escalated as **OI-CVNS-11**.
3. The baseline arms when `NP_CARDIAC_BASELINE_BEATS` (8) intervals have accumulated. There is no outlier-rejection criterion on the MCU side.
4. Baseline HR (BPM) = 60,000,000 / mean(all intervals currently in the ring buffer, in µs). Because the buffer is 8 deep and arming requires 8 intervals, at the arming tick this is the mean of the last 8.
5. **Not implemented (Rev 9).** This step said that the main processor confirms its baseline via `NP_CVNS_SPI_CMD_HR_BASELINE_SET`, and that the safety MCU rejects the enable if the two differ by more than `NP_CVNS_BASELINE_CROSSVAL_BPM` (5 BPM). The safety MCU never receives the main processor's baseline. Its wire protocol (`np_spi_wire_types.h`) has no baseline command, and `np_cvns_interlock.c` sends 0x13 through a stub that transmits nothing. `NP-FMEA-001` Rev 12 withdrew the same claim on 2026-09-25. Whether a cross-check is wanted is part of OI-CVNS-12.
6. **(Rev 10)** The arming mean is the first entry of a **heart-rate history**. Once armed, the current mean is appended every `NP_CARDIAC_HR_SNAP_MS` (1 s), and the oldest entry is overwritten after `NP_CARDIAC_HR_HIST_LEN` (18). §5.4 step 2 compares against every entry. Slow drift does not accumulate into a cutoff, because an entry older than 18 s is gone: a drift is cut only if it moves more than 15 BPM within 18 s. When a lockout expires, the history is emptied and reseeded from the current mean. No snapshot is taken during the lockout, so the stale entries are the pre-event rates, and comparing against them would re-trip at once and make re-enable unreachable. *Superseded, retained: Once armed, the baseline is refreshed to the current rate every `NP_CARDIAC_OBS_MS` (5 s) so that slow physiological drift does not accumulate into a cutoff. **Rev 9: the refresh is unconditional. It also absorbs a sustained step that takes longer than 5 s to move the 8-interval mean past 15 BPM, which at resting rates is most steps (OI-CVNS-12, §14.5).***

**Horizon derivation (Rev 10, CLAUDE.md §18).** A change of just over 15 BPM crosses the threshold only when all 8 intervals in the mean are at the new rate. It is seen only against a snapshot taken before the change began, so that snapshot must still be in the history when the 8th interval lands:

| Term | Value | Source |
|---|---|---|
| Snapshot phase: the last pre-change snapshot can be up to one cadence older than the change | 1 s | `NP_CARDIAC_HR_SNAP_MS` |
| Spread: the change may take this long to reach its final rate | 5 s | CLAUDE.md §4.2, "within 5 s" |
| Transit: 8 intervals at the slowest final rate the interlock must resolve | 8 × 60 / 40 = 12 s | Window (step 3); **40 BPM floor, principal 2026-10-01** |
| **Horizon** | **18 s** | `NP_CARDIAC_HR_HIST_LEN` × `NP_CARDIAC_HR_SNAP_MS` |

**What fails if it is shorter:** a qualifying fall to near 40 BPM, the RISK-25 hazard direction, goes uncut. `test_floor_fall_to_40_cut_in_real_time` fails at 11, and `test_floor_two_stage_to_40_cut_in_real_time` fails at 16. **Below 40 BPM** a change of just over 15 BPM can outlast the horizon. In simulation, 50 → 34 BPM was still cut in every trial. Below 20 BPM, the 3 s staleness cutoff (§5.4 step 4) applies whatever the history holds.

Host-test coverage: `firmware/safety_mcu/tests/np_cardiac_interlock_tests.c` (`np_cardiac_interlock_tests`).

### 5.4 Cardiac interlock monitoring loop

While cervical VNS is requested, the safety MCU runs this check on **every main-loop iteration** (`np_cardiac_interlock_tick()`). *Rev 8 correction: Rev 1 described a 200 Hz TIM6 ISR, and there is none. The cutoff lands within one main-loop iteration, and bench latency against the 100 ms specification is FAI-CV02.*

**Per iteration:**

1. Compute `window_hr_bpm` = 60,000,000 / mean(R-R intervals currently in the ring buffer, in µs).
2. **(Rev 10)** If `NP_CARDIAC_HR_SNAP_MS` has elapsed since the last snapshot, append `window_hr_bpm` to the history (§5.3 step 6). Then, if `|window_hr_bpm − h| > NP_CARDIAC_HR_DELTA_BPM` (15 BPM) for **any** entry `h` in the history, the following applies. *Rev 9 and earlier compared with a single `baseline_hr_bpm` that was refreshed every 5 s (OI-CVNS-12).* The comparison is **signed** (FMEA-M05-02; since Rev 10 each difference is formed in `int32_t`), so a fall below an entry is compared by magnitude rather than underflowing to a large positive value. The threshold is a strict `>`: a delta of exactly 15 BPM holds:
   a. Clear `NP_SAFETY_EN_CVNS` from the granted enable mask, driving the single CVNS enable GPIO (`NP_EN_CVNS`) to its disabled state and stopping stimulation. **Rev A specified asserting `CVNS_ENABLE_L` *and* `CVNS_ENABLE_R`; the firmware has one CVNS enable line, so that instruction was unimplementable as written.** Whether per-electrode cutoff is *required* is open — see OI-CVNS-09 — but nothing in this document should be read as a claim that per-electrode cutoff exists today.
   b. Record the fault: `NP_SAFETY_STATUS_CARDIAC` and `NP_SAFETY_STATUS_CUTOFF` set, `fault_slot` = 10 (CVNS).
   c. Start the `NP_CARDIAC_LOCKOUT_MS` (30 s) re-enable lockout. Re-enable is refused for the whole window; after it expires, re-enable additionally requires explicit app confirmation and a repeat impedance check (§5.5, CLAUDE.md §4.2).
   d. Send FAULT SPI notification to main processor on next SPI transaction.
   e. **(Rev 6)** Record the cutoff in safety-MCU flash for the active user (§5.4.1), once every channel is off. The CARDIAC status withholds `NP_SAFETY_EN_CVNS` **only** (`NP_CARDIAC_BLOCK_MASK`). Every other channel stays grantable.
3. **Pre-arm hold (Rev 8, principal 2026-09-25, `NP-RISK-002` OI-RISK2-05).** Until `NP_CARDIAC_BASELINE_BEATS` (8) fresh intervals have armed the baseline, the safety MCU **withholds `NP_SAFETY_EN_CVNS`**. The hold is silent: no status bit, no lockout and no NV write. The hub reads an absent grant as request latency, not as a fault (`np_mod_cvns.c`), and holds its stimulation at 0 until granted. A new CVNS request re-arms from fresh beats (`np_cardiac_interlock_arm_reset()`), and so does every re-enable. *Rev 7 and earlier: the MCU would not fire a cutoff until armed, but it still granted CVNS, so a session whose R-peaks never arrived ran with no Class C monitoring. The hub's `NP_CVNS_DATA_LOSS_TIMEOUT_S` hold (§6) is a separate, Class B mechanism.* Superseded text, retained: **Conservative hold.** The safety MCU will not fire a cutoff at all until the baseline has armed — that is, until `NP_CARDIAC_BASELINE_BEATS` (8) intervals have accumulated. This is stricter than Rev 1's "fewer than 3 valid intervals" rule, which described the *main processor's* data-loss handling (`NP_CVNS_DATA_LOSS_TIMEOUT_S`, §6). The safety MCU has **no** warning flag and **no** 10 s soft-cutoff timer; it holds, silently and unconditionally, until armed. Re-enable invalidates the baseline, so the hold applies again after every cutoff.
4. **Staleness cutoff (Rev 8, same decision).** While CVNS is granted and the baseline is armed, **no R-peak edge for `NP_CARDIAC_RPEAK_STALE_MS` (3 s)** triggers step 2a–2e exactly as a heart-rate excursion does: CVNS withheld, CARDIAC and CUTOFF set, fault slot 10, 30 s lockout, and the cutoff persisted for the active user. 3 s is an R-R interval of 20 BPM, which is non-physiological for an eligible patient, so a live rhythm never trips it. A lost R-peak stream (cable, PPG or main-processor fault) is therefore caught by Class C code within 3 s, rather than only by the hub's 10 s Class B timer. It is evaluated only while CVNS is granted, so it cannot re-trip on lockout expiry.

#### 5.4.1 Persistence and per-user scope (Rev 6)

The step 2c lockout used to be RAM state, and the fault latch is cleared by a power-on reset. The headset has no battery (CLAUDE.md §4.5), so in Mode 3 an unplug and re-plug restarted cervical stimulation with no app confirmation (`NP-SW-FAULTMSG-001` F1). Module SW01-M09 (`np_nv_state.c`) closes this. The design and its rationale are in `NP-SW-FAULTMSG-001` §9.

- **Active user.** The app names the person using the device with an opaque tag, derived from the profile's random UUID. The hub forwards it (`NP_SAFETY_CMD_ACTIVE_USER`, 10-byte frame) only while no session is running. The safety MCU accepts it only when nothing is granted. The same person is assumed until the app names someone else.
- **Record.** A cutoff is recorded against the active user.
  - A cutoff recorded before any user is named is unattributable and blocks everyone.
  - The completed re-enable (§5.5: lockout expired, app confirmation, repeat impedance) clears the confirming user's entry and every unattributable one.
- **Boot.** The log is replayed. If the current user has an outstanding cutoff, CARDIAC is re-armed *latently*: it is asserted at the first cervical enable request, and no other channel is affected.
- **Fail closed.**
  - A torn record counts as an unattributable cutoff.
  - An uncommitted compaction snapshot is merged additively.
  - An overflow past 8 pending users becomes "any".
  - A flash write that fails 3 times raises a FAULT (slot `NP_FAULT_SLOT_NVSTATE`).
- **Stall.** Flash writes stall the core for up to 40 ms, so they run only while no channel is granted.
- **Reported to the hub, not to SHDR.** Two bits go to the hub in every SPI reply (`np_safety_nv_report_t`): *this user is withheld*, and *someone on this device has an outstanding cutoff*. They reach the app only. They are never timed, never logged and never SHDR-reportable (CLAUDE.md §5.1).
- **Flash layout.** Pages 62–63 (0x0801F000, 4 KB) are reserved in the linker script. A future safety-MCU update path must never erase them.

**Cutoff latency guarantee:** The GPIO assertion occurs within the TIM6 ISR, with a maximum latency of one 5 ms tick after the condition is first detected. The ISR itself executes in < 100 µs on the STM32G071 at 64 MHz (< 6,400 cycles). **Total worst-case cutoff latency: < 5.1 ms — well within the 100 ms specification (`NP_CVNS_CUTOFF_LATENCY_MAX_MS`).**

### 5.5 SPI command set (safety MCU slave)

| Command | Value | Direction | Description |
|---------|-------|-----------|-------------|
| `IMPEDANCE_CHECK` | 0x10 | Main → MCU | Request AC impedance measurement on both electrodes |
| `ENABLE` | 0x11 | Main → MCU | Request stimulation enable; MCU validates baseline first |
| `DISABLE` | 0x12 | Main → MCU | Immediate disable; no fault logged |
| `HR_BASELINE_SET` | 0x13 | Main → MCU | Push main-processor-computed baseline HR for cross-validation |
| `STATUS_QUERY` | 0x14 | Main → MCU | Request status response |
| `STATUS_RESPONSE` | 0x80 | MCU → Main | Current state, impedance values, fault reason |
| `FAULT_NOTIFY` | 0x81 | MCU → Main | Asynchronous fault notification |
| `IMPEDANCE_RESULT` | 0x82 | MCU → Main | Impedance measurement result (both electrodes) |
| `ENABLE_GRANTED` | 0x83 | MCU → Main | Enable request accepted |
| `ENABLE_REJECTED` | 0x84 | MCU → Main | Enable request denied (with reason byte) |

### 5.6 Safety MCU fault log (SHDR)

Each cutoff event generates an **8-byte** fault log entry written to a dedicated SHDR sub-partition:

```c
typedef struct {
    uint32_t session_id;         /* session counter (unsigned, no timestamps) */
    uint8_t  reason;             /* 0=HR_CHANGE, 1=DATA_LOSS, 2=WATCHDOG      */
    uint8_t  reserved[3];
} np_cvns_fault_log_entry_t;
```

**No HR values in the fault log** — those are in the UHDR session record (user biology, biometric-derived AES key).

**No event timing in the fault log either (Rev 3, 2026-08-12).** Rev 2 carried a `uint32_t cutoff_offset_ms` ("ms after stim enable when cutoff occurred") in this SHDR record while §8.2 classified the very same quantity — *Time-of-cutoff offset (s after stim onset)* — as **UHDR, user biology**, two rows above routing this record to SHDR. Both could not be right, and the UHDR row is the correct one:

- For `reason = HR_CHANGE`, the offset is the latency from stimulation onset to the wearer's heart rate deviating past `NP_CARDIAC_HR_DELTA_BPM`. That is an **autonomic response latency** — a measurement of the person, not of the device. It fails the §5.1 test ("does this record tell us something about the person?") outright.
- The UHDR session record (§4.5) **already carries this datum** as `cutoff_time_offset_s`. The SHDR copy was a duplicate at *finer* resolution (ms vs s), so it was strictly more revealing than the UHDR original it shadowed.
- Fault event timing is UHDR generally: the SHDR fleet schema's `fault_log` has no timing column and states *"Precise fault event timing, where it exists at all, is UHDR under the user's key"*, and `np_log_shdr_fault()` discards its `session_ms` argument for every caller.

The removal is **unconditional** — the field leaves the record shape for every `reason` value. Zeroing it only for the cardiac reason would have recreated exactly the oracle retired from the fault latch on the same date (a redaction conditioned on a sensitive predicate leaks that predicate, CLAUDE.md §5.1); here it would leak doubly, because `reason` is itself in the record. `reason` stays: the fault **kind** is the deliberate, already-published SHDR channel (`fault_log.fault_type`, e.g. `'CVNS_HR_CUTOFF'`), per the locked "safety interlock log → SHDR" rule.

The struct was declared but never populated — no producer and no consumer existed at the time of the change, so no data path or stored record was affected.

---

## 6. Cardiac Interlock Module — Main Processor Side (`np_cvns_interlock.c`)

### 6.1 Responsibilities

The main processor side of the interlock:
1. Processes PPG samples (at 200 Hz) to detect R-peaks.
2. Asserts the `RPEAK_IN` GPIO pulse to the safety MCU on each R-peak.
3. Maintains a local R-R interval buffer and computes baseline HR for cross-validation.
4. Handles SPI exchange with the safety MCU for enable/disable/status.
5. Propagates FAULT notifications from the safety MCU to the session manager.

### 6.2 R-peak detection

Pan-Tompkins-derived bandpass detection on the PPG signal from the VNS accessory PPG sensor (808–830 nm, co-located in the clip mount):

1. Bandpass FIR: 0.5–40 Hz (eliminates motion artefact and high-frequency noise).
2. Differentiate: first difference of filtered signal.
3. Square: element-wise squaring.
4. Moving window integrate: 150 ms window.
5. Adaptive threshold: 75% of running maximum over last 2 s; updated after each confirmed R-peak.
6. Refractory period: 200 ms after each confirmed peak (prevents double-detection).
7. **(Rev 11)** Forward and buffer are separate decisions. A peak ≥ `NP_CVNS_RR_MIN_VALID_MS` (300 ms) after the last detected peak is pulsed on `RPEAK_IN`, with **no upper bound**. Only an interval inside 300–2000 ms enters this side's R-R buffer (§6.3). A peak inside 300 ms is neither pulsed nor buffered, but it still becomes the last detected peak. *Rev 10 and earlier gated the pulse on 2000 ms as well. Below 60 BPM, one missed detection then hid the next real beat, and the safety MCU's 3 s staleness cutoff fired (OI-CVNS-13, §14.6).*

### 6.3 Baseline HR computation

Similar to the safety MCU's (§5.3), running in parallel on the main processor, but over 5 intervals (`NP_CVNS_BASELINE_BEATS_MIN`) of a 20-deep buffer and after the 300–2000 ms validity filter. The main processor calls `platform_spi_send(NP_CVNS_SPI_CMD_HR_BASELINE_SET, …)` before requesting enable. *Rev 9 correction: this paragraph said the safety MCU cross-validates that baseline and blocks enable on a > 5 BPM discrepancy. `platform_spi_send` is a stub, the hub↔MCU protocol has no such command, and the MCU never compares the two (§5.3 step 5).* This side has no heart-rate excursion cutoff of its own; its only cutoff is data loss (`NP_CVNS_DATA_LOSS_TIMEOUT_S`).

### 6.4 API summary

```c
/* Lifecycle */
np_cvns_status_t np_cvns_interlock_init(np_cvns_interlock_ctx_t *ctx,
                                         np_cvns_interlock_config_t config,
                                         np_cvns_fault_cb_t fault_cb);
void             np_cvns_interlock_deinit(np_cvns_interlock_ctx_t *ctx);

/* PPG feed-in (call from PPG ISR at 200 Hz) */
void np_cvns_interlock_push_ppg(np_cvns_interlock_ctx_t *ctx,
                                  uint32_t sample,
                                  uint32_t timestamp_ms);

/* Session control */
np_cvns_status_t np_cvns_interlock_request_enable(np_cvns_interlock_ctx_t *ctx);
void             np_cvns_interlock_disable(np_cvns_interlock_ctx_t *ctx);
np_cvns_status_t np_cvns_interlock_request_reenable(np_cvns_interlock_ctx_t *ctx);

/* Safety MCU SPI callbacks */
void np_cvns_interlock_spi_response(np_cvns_interlock_ctx_t *ctx,
                                     uint8_t cmd,
                                     const uint8_t *payload,
                                     uint8_t len);

/* State accessors */
np_cvns_interlock_state_t np_cvns_interlock_state(const np_cvns_interlock_ctx_t *ctx);
float                     np_cvns_interlock_baseline_hr(const np_cvns_interlock_ctx_t *ctx);
bool                      np_cvns_interlock_baseline_valid(const np_cvns_interlock_ctx_t *ctx);
uint32_t                  np_cvns_interlock_reenable_lockout_remaining_s(
                                     const np_cvns_interlock_ctx_t *ctx,
                                     uint32_t now_s);
```

---

## 7. Stimulation Delivery (`np_cvns_stim.c`)

### 7.1 Waveform specification

Biphasic charge-balanced stimulation, identical waveform topology to the gammaCore predicate:

| Phase | Duration | Polarity | Charge |
|-------|----------|----------|--------|
| Active phase | `pulse_width_us` | Cathodic first (cervical VNS convention) | q = I × t |
| Inter-phase gap | 100 µs | Zero | 0 |
| Charge-recovery phase | `pulse_width_us` | Anodic | −q |
| Inter-stimulus interval | 1/freq_hz − 2×pulse_width_us − 200µs | Zero | 0 |

Charge balance is hardware-enforced: the driver circuit integrates charge delivery and terminates the recovery phase when cumulative charge returns to zero (within ±1 µC tolerance). The safety MCU independently monitors the integrated charge via a dedicated current-sense ADC channel.

### 7.2 Ramp state machine

```
IDLE → RAMP_UP → ACTIVE → RAMP_DOWN → DONE
              ↓                    ↓
            FAULT               FAULT
```

- **RAMP_UP**: current increases linearly from 0 to `target_current_ua` over `NP_CVNS_RAMP_UP_S` (10 s). Step size: `target_ua / (ramp_s × tick_rate_hz)`.
- **ACTIVE**: steady-state stimulation at `target_current_ua` until session duration elapsed or external stop.
- **RAMP_DOWN**: current decreases linearly over `NP_CVNS_RAMP_DOWN_S` (5 s). Initiated by session timer or by external stop request.
- **FAULT**: entered immediately on cardiac interlock cutoff or safety MCU rejection; current set to zero atomically.

### 7.3 Tick rate

The stim module is driven by a 100 ms tick (`np_cvns_stim_tick(ctx, now_ms)`) from the application scheduler. The safety MCU drives the actual pulse generation; the main processor computes target current levels and sends them via SPI.

### 7.4 API summary

```c
np_cvns_status_t np_cvns_stim_init(np_cvns_stim_ctx_t *ctx,
                                    const np_cvns_stim_config_t *config,
                                    np_cvns_safety_response_cb_t safety_cb);
void             np_cvns_stim_deinit(np_cvns_stim_ctx_t *ctx);

np_cvns_status_t np_cvns_stim_start(np_cvns_stim_ctx_t *ctx,
                                     uint16_t current_ua,
                                     uint16_t duration_s,
                                     uint32_t now_ms);
void             np_cvns_stim_stop(np_cvns_stim_ctx_t *ctx);
void             np_cvns_stim_tick(np_cvns_stim_ctx_t *ctx, uint32_t now_ms);

/* Safety MCU callback (invoked from SPI ISR) */
void np_cvns_stim_safety_mcu_response(np_cvns_stim_ctx_t *ctx,
                                       bool granted,
                                       const float impedance_kohm[NP_CVNS_ELECTRODE_COUNT]);

np_cvns_stim_phase_t np_cvns_stim_phase(const np_cvns_stim_ctx_t *ctx);
uint16_t             np_cvns_stim_current_ua(const np_cvns_stim_ctx_t *ctx);
```

---

## 8. Session Management (`np_cvns_session.c`)

### 8.1 Session workflow

```
np_cvns_session_start()
    │
    ▼
STAGE_IMPEDANCE  ── safety MCU measures both electrode impedances
    │                both ≤ 5 kΩ ?
    │ yes
    ▼
STAGE_BASELINE   ── accumulate ≥ 5 valid R-R intervals
    │                cross-validate main processor vs safety MCU baseline
    │ agree within 5 BPM?
    │ yes → safety MCU confirms enable request
    ▼
STAGE_RAMP_UP    ── 10-second current ramp
    ▼
STAGE_ACTIVE     ── steady stimulation; cardiac monitoring at 200 Hz
    │
    │ cardiac interlock fires?        session timer expires?
    ▼                                 ▼
STAGE_FAULT                       STAGE_RAMP_DOWN → STAGE_COMPLETE
```

### 8.2 UHDR / SHDR data routing

| Data element | Partition | Notes |
|---|---|---|
| Full R-R interval time series during session | UHDR | User biology |
| Baseline HR (BPM) | UHDR | User biology |
| Instantaneous HR at cutoff | UHDR | User biology |
| Time-of-cutoff offset (s after stim onset) | UHDR | User biology |
| Impedance (both electrodes, pre-session) | UHDR | Raw measurement = user biology |
| Session start timestamp | UHDR | User biology per CLAUDE.md §5.1 |
| Session duration (s) | UHDR | User biology |
| Stimulation parameters (freq, current, pulse width) | UHDR | Linked to a specific user session |
| Cutoff occurred flag (0/1 only, no HR values) | SHDR | Device safety event — no user biology |
| Electrode impedance pass/fail (boolean) | SHDR | Device contact quality metric |
| Safety MCU fault log (session ordinal + reason; **no offset**, no HR) | SHDR | Device event log. **Rev 3:** the `cutoff_offset_ms` field was removed — it duplicated the *Time-of-cutoff offset* row above, which this same table already classifies UHDR, at finer resolution. Fault event timing is UHDR (§5.6). |
| Session count increment | SHDR | Unsigned integer |

### 8.3 Session configuration structure

```c
typedef struct {
    uint16_t freq_hz;              /* 1–25 Hz                                   */
    uint16_t current_ua;           /* 0–2000 µA                                 */
    uint16_t pulse_width_us;       /* 200–1000 µs                               */
    uint16_t duration_s;           /* 60–120 s                                  */
    uint8_t  electrode_config;     /* 0=bilateral, 1=unilateral_L, 2=unilateral_R */
} np_cvns_session_config_t;
```

Sessions are delivered via cryptographically signed protocol from the app (same CSPRNG signing mechanism as all other NeurOne modalities).

### 8.4 API summary

```c
np_cvns_status_t np_cvns_session_init(np_cvns_session_ctx_t   *ctx,
                                       np_cvns_interlock_ctx_t *interlock,
                                       np_cvns_stim_ctx_t      *stim,
                                       np_cvns_session_end_cb_t end_cb);

np_cvns_status_t np_cvns_session_start(np_cvns_session_ctx_t       *ctx,
                                        const np_cvns_session_config_t *config,
                                        uint32_t                     now_ms);

void np_cvns_session_stop(np_cvns_session_ctx_t *ctx);
void np_cvns_session_tick(np_cvns_session_ctx_t *ctx, uint32_t now_ms);

np_cvns_stage_t np_cvns_session_stage(const np_cvns_session_ctx_t *ctx);
```

---

## 9. FAI Test Specification

> **This section is the test specification for FAI-CV01…CV03, and there is no separate FAI
> checklist document.** The A14 *artifact* FAI checklist is named `NP-FAI-CVNS-001` by
> `NP-ART-001` §3.2 and **has never been written**. **Updated at Rev 5:** through Rev 4 the reason
> given was that *no mechanical or electrical specification exists for the electrode assembly,
> cable or connector (GitHub #332)*. One does — **`NP-HW-CVNS-001` Rev 1**, issued 2026-09-20 — and
> it is **DRAFT**, so `NP-FAI-001` §2 F1 still fails and §2 still requires a named absence rather
> than a checklist with invented accept criteria. The reason moved from absence to maturity;
> the conclusion did not (`NP-FAI-001` §2.2, OI-FAI-07 re-scoped).
> The items below are firmware verification items; their bench limbs are procedures, not
> results, and a PASS here is not an inspection record (`NP-FAI-001` §2.1, §3.1 / OI-FAI-07).
> **Their hardware limbs are A14's acceptance criteria** and the artifact checklist inherits them
> when F1 is met (`NP-HW-CVNS-001` `REQ-CVNS-12`); nothing moves out of this section.

### FAI-CV01 — Cervical electrode placement verification

**Category:** Hardware bench (cannot be software-verified in CI).

**Setup:**
- Cervical VNS accessory gel electrodes applied to anatomical neck phantom (silicone gel head-neck phantom with embedded vasculature model, 0.25 S/m tissue equivalent).
- NeurOne hub accessory port connected to cervical VNS cable assembly.
- Safety MCU impedance measurement activated.

**Procedure:**
1. Apply both bilateral gel electrodes per IFU (skin overlying left and right carotid sheath, 2 cm inferior to angle of jaw).
2. Apply moderate pressure (equivalent to 2N) and measure impedance at 1 kHz AC.
3. Verify both electrodes register ≤ 5 kΩ.
4. Remove left electrode; verify system detects single-electrode failure (impedance > limit or open circuit).
5. Repeat for right electrode.
6. Apply electrodes with deliberate misplacement (5 cm superior to correct position); verify that impedance > 5 kΩ flags the placement as invalid.

**Pass criteria:**
- CV01-A: Both electrodes correctly placed → impedance ≤ 5 kΩ on both channels.
- CV01-B: Single electrode removal → system reports high-impedance fault on removed channel within 500 ms.
- CV01-C: Deliberate misplacement → impedance > 5 kΩ on at least one channel; system blocks enable.
- CV01-D: Safety MCU reports impedance values to main processor within 200 ms of measurement completion.

**Result:** PENDING (hardware bench required).

---

### FAI-CV02 — Cardiac interlock response time

**Category:** Bench test (software plumbing check executable in CI; timing verification requires hardware).

**Setup:**
- R-peak signal generator injecting R-peak GPIO pulses to safety MCU at programmable HR.
- Oscilloscope monitoring `CVNS_ENABLE_L` GPIO.
- NeurOne T2 hub with safety MCU flashed; cervical VNS accessory connected.

**Procedure:**
1. Establish baseline HR: inject R-peak pulses at 70 BPM (R-R = 857 ms) for 10 seconds.
2. Enable stimulation: request enable via main processor SPI; verify safety MCU grants.
3. HR step event: abruptly change R-peak injection rate to 90 BPM (R-R = 667 ms) — a +20 BPM change exceeding the 15 BPM limit.
4. **(Rev 10)** Record two times on the oscilloscope. **Detection time** runs from the first post-step R-peak edge to the edge after which `CVNS_ENABLE_L` falls (the *detecting edge*). **Cutoff latency** runs from the detecting edge to the `CVNS_ENABLE_L` falling edge. *Rev 9 and earlier measured one time, from the first out-of-window edge to the falling edge. That mixed the two, and its ≤ 100 ms criterion was unreachable, because an 8-interval mean needs several post-step beats to cross 15 BPM (§14.5).*
5. Repeat 10 times; record each cutoff latency and detection time.
6. **(Rev 10)** Repeat steps 1–5 with a fall to 50 BPM (R-R = 1 200 ms), the RISK-25 hazard direction. Rev 9's procedure tested only a rise.

**Pass criteria:**
- CV02-A: All 20 measured cutoff latencies, **from the detecting edge**, are ≤ 100 ms (`NP_CVNS_CUTOFF_LATENCY_MAX_MS`). *(Rev 10: was "from the first out-of-window R-peak edge".)*
- CV02-F **(Rev 10)**: Every one of the 20 steps is cut. The detecting edge is no later than the 8th post-step edge, and detection takes ≤ 18 s (CLAUDE.md §4.2, §5.3 step 6). Host-predicted: the 7th edge for 70 → 90, and the 6th for 70 → 50 (`np_cardiac_interlock_tests`).
- CV02-B: Safety MCU sends FAULT_NOTIFY SPI message within 200 ms of cutoff GPIO event.
- CV02-C: Main processor `np_cvns_interlock_state()` returns `NP_CVNS_INTERLOCK_FAULT` within 300 ms of GPIO event (one SPI heartbeat interval).
- CV02-D: Re-enable is blocked until `NP_CVNS_REENABLE_LOCKOUT_S` (30 s) has elapsed.
- CV02-E: A HR change of exactly 15 BPM does NOT trigger cutoff (boundary condition); >15 BPM does.

**Software-verifiable component (CI):** The state machine boundary conditions (CV02-D, CV02-E) and constant values (`NP_CVNS_CUTOFF_LATENCY_MAX_MS`, `NP_CVNS_REENABLE_LOCKOUT_S`, `NP_CVNS_HR_CHANGE_LIMIT_BPM`) are verified by the FAI test binary.

**Result:** CV02-D, CV02-E SOFTWARE PASS (verified in `np_cvns_fai_tests.c`). CV02-A, CV02-B, CV02-C PENDING (hardware bench required). CV02-F **(Rev 10)** PENDING on the bench. Its host counterpart passes against the Class C unit with beats in real time (`np_cardiac_interlock_tests`, OI-CVNS-12 group).

---

### FAI-CV03 — Tolerability in 3 healthy adults

**Category:** Clinical (IRB approval required before execution).

**Setup:**
- 3 healthy adult volunteers (screened; exclusion: cardiac arrhythmia, carotid stenosis, implanted stimulator, pregnancy).
- NeurOne T2 system with cervical VNS accessory.
- Attending clinician and emergency equipment present.
- ECG monitoring throughout (12-lead or 3-lead continuous).

**Procedure:**
1. Apply bilateral gel electrodes per IFU. Confirm impedance CV01-A criterion met.
2. Initiate 60-second session at minimum parameters (1 Hz, 500 µA, 300 µs pulse width).
3. Record: sensation reports (0–10 numeric rating scale), HR (via ECG), blood pressure (5-min intervals), adverse events.
4. Confirm no cardiac interlock triggering at minimum parameters.
5. Optionally escalate to mid-range parameters (15 Hz, 1000 µA) per clinician discretion.

**Pass criteria:**
- CV03-A: No serious adverse events (no cardiac arrhythmia, syncope, severe carotid discomfort, or oxygen desaturation) in any of 3 participants.
- CV03-B: Maximum reported sensation ≤ 7/10 at minimum protocol parameters.
- CV03-C: Cardiac interlock does not trigger spuriously at minimum parameters (confirming correct baseline and threshold calibration).
- CV03-D: All 3 participants complete minimum protocol without requesting early termination.

**Result:** PENDING (IRB approval and T2 prototype required; not blocking for G3-08 software gate; blocking for T2 clinical release).

---

## 10. Module File Inventory

| File | Contents |
|------|---------|
| `include/np_cvns_config.h` | All configuration constants |
| `include/np_cvns_types.h` | All shared type definitions |
| `include/np_cvns_interlock.h` | Cardiac interlock API |
| `include/np_cvns_stim.h` | Stimulation delivery API |
| `include/np_cvns_session.h` | Session management API |
| `src/np_cvns_interlock.c` | R-peak detection, baseline, SPI exchange, state machine |
| `src/np_cvns_stim.c` | Biphasic waveform, ramp state machine |
| `src/np_cvns_session.c` | Session orchestration, UHDR/SHDR record |
| `tests/np_cvns_fai_tests.c` | FAI-CV01 procedure, FAI-CV02 constants + state machine, FAI-CV03 procedure |
| `CMakeLists.txt` | Static library build |

**Safety MCU side (SW-01 Class C, `firmware/safety_mcu/` — specified in §5, not §6–§8):**

| File | Contents |
|------|---------|
| `include/np_safety_config.h` | Safety MCU GPIO map + cardiac interlock constants (see OI-CVNS-08) |
| `src/np_cardiac_interlock.c` | SW01-M05: R-R capture, baseline arming, ±15 BPM cutoff, 30 s lockout |
| `tests/np_cardiac_interlock_tests.c` | Host tests for SW01-M05 — arming, cutoff both directions, FMEA-M05-02 signed-delta guard, lockout, conservative hold |

> Note: `np_cvns_fai_tests` exercises the **main-processor** module in this section's table and asserts on main-processor constants. It is not coverage of the Class C interlock; `np_cardiac_interlock_tests` is. **Corrected at Rev 4:** through Rev 3 the suite was registered in `firmware/CMakeLists.txt` as *"NP-FAI-CVNS-001: cardiac interlock FAI suite"* and printed that serial as its own document header — two claims in one string, both wrong. `NP-FAI-CVNS-001` has never been written and cannot be — through Rev 4, because A14 had no hardware specification (GitHub #332); **since Rev 5, because `NP-HW-CVNS-001` Rev 1 is DRAFT and `NP-FAI-001` §2 F1 requires `BASELINED` or `ACTIVE`** — so it is recorded as a named absence in `NP-ART-001` §3.2 rather than cited here; and the suite is not interlock coverage. Both the registration and the banner now name **§9 of this document**, which is the test specification for FAI-CV01…CV03 and the record of file. See `NP-FAI-001` §2.1 / OI-FAI-07.

---

## 11. UHDR / SHDR Data Routing Summary

Consistent with NP-FW-EMMC-001 Rev 2 §12 (33-element classification table; was cited at Rev 1 / 27 elements through Rev 2 of this document). Key additions from the cervical VNS module:

| Data element | Partition | Reasoning |
|---|---|---|
| R-R interval time series | UHDR | Directly reveals cardiac rhythm — unambiguously user biology |
| Baseline HR (BPM) | UHDR | Derived from R-R series; identifies resting cardiac state |
| HR at cutoff event | UHDR | User cardiac response data |
| Session timestamps | UHDR | Per CLAUDE.md §5.2 resolution: session timestamps → UHDR |
| Electrode impedance (raw) | UHDR | Raw measurement linked to a user session |
| Cutoff flag (boolean only) | SHDR | Device safety event; no user biology in the flag itself |
| Time-of-cutoff offset (s after stim onset) | UHDR | Latency from stim onset to the wearer's HR deviating past the cutoff threshold — an autonomic response latency, i.e. user biology. Carried in the UHDR session record as `cutoff_time_offset_s` (§4.5) and **nowhere in SHDR** (§5.6, Rev 3) |
| Electrode impedance pass/fail | SHDR | Device contact quality (aggregate) |
| Safety MCU fault log | SHDR | Device event log; no HR values **and no event timing** (§5.6) |
| Session count increment | SHDR | Unsigned integer; no timestamps |

---

## 12. Gate Closure: NP-COORD-001 G3-08

This document and its accompanying firmware (`firmware/cervical_vns/`) satisfy the G3-08 gate requirement:

- [x] Safety MCU cardiac interlock fully specified (§5)
- [x] Cutoff latency requirement confirmed: < 5.1 ms worst-case (§5.4), spec 100 ms
- [x] FAI-CV01 procedure specified (§9, hardware bench pending)
- [x] FAI-CV02 software constants verified, hardware bench pending (§9)
- [x] FAI-CV03 tolerability procedure specified (§9, clinical pending)
- [x] UHDR/SHDR data routing consistent with NP-FW-EMMC-001 Rev 1 §12
- [x] Re-enable policy specified (§3.5): lockout + app confirm + re-check
- [x] RISK-25 documented in risk register (NP-FW-CVNS-001 Rev 1 §13)
- [x] 510(k) Q-Sub substantial equivalence argument baselined (NP-REG-CVNS-001 Rev 1)

**G3-08 SOFTWARE BASELINED — 2026-05-11**
Hardware FAI (CV01 bench, CV02 timing, CV03 clinical) PENDING — blocking for T2 clinical release, not for software gate.

---

## 13. Risk Register Entry — RISK-25

| Field | Value |
|-------|-------|
| Risk ID | RISK-25 |
| Title | Cardiac reflex during cervical VNS — inadequate interlock response time |
| Hazard | Stimulation near carotid sheath activates baroreceptor reflex → uncontrolled HR drop or arrhythmia |
| Severity | **S5 — Critical** (`NP-RM-001` §4.1 names this harm as its S5 example). *Before 2026-09-23 this read "Critical (S4)", mixing two scale levels* |
| Probability (unmitigated) | P3 — Occasional (documented in gammaCore predicate safety data) |
| Risk (unmitigated) | S5 × P3 = **UNACCEPTABLE** |
| Mitigation | Safety MCU TIM6 ISR fires every 5 ms; cardiac interlock GPIO cutoff < 5.1 ms from detection trigger. ~~Baseline cross-validation blocks enable if main processor and safety MCU disagree.~~ *(Rev 9: never built, §5.3 step 5. **The detector itself misses most qualifying changes at resting rates, OI-CVNS-12 and `NP-RISK-002` OI-RISK2-08.** The TIM6 wording is Rev 1's; §5.4 records that there is no TIM6 ISR.)* *(Rev 10: the detector is redesigned and detects every simulated qualifying change down to a 40 BPM endpoint (§14.5.1). It now false-trips more often (OI-CVNS-13). Not re-scored here.)* 30 s re-enable lockout. Re-enable requires explicit app confirmation. **(Rev 6)** The cutoff persists in safety-MCU flash across power loss, per user, failing closed (§5.4.1). It withholds cervical VNS only. gammaCore predicate demonstrated equivalent interlock concept safe in K163334/K173323. |
| Residual probability | **P2 — Remote** now, because no control is yet verified on hardware; **P1** target after FAI-CV02, silicon verification of §5.4.1, `OI-CVNS-11` and `OI-CVNSHW-03` (`NP-RISK-002` §4.3) |
| Residual risk | **S5 × P2 = ALARP** (target S5 × P1, still ALARP). ALARP justification: `NP-RISK-002` §4.3.4. *Before 2026-09-23 this read "Low", which the `NP-RM-001` matrix cannot produce at S5* |
| Verification | FAI-CV02: measured cutoff latency ≤ 100 ms from detection, and detection ≤ 18 s, for a rise and a fall (10 trials each; Rev 10) |
| Status | **ALARP — re-scored 2026-09-23, approved by the Quality Lead (interim: Steve Hickman, CEO) 2026-09-23**; verification pending (FAI-CV02, silicon) |

---

## 14. Open Items

| ID | Description | Owner | Blocking |
|----|-------------|-------|---------|
| OI-CVNS-01 | Platform HAL stubs (`np_platform_cvns_rpeak_gpio_pulse`, `np_platform_cvns_spi_transfer`) must be implemented before integration testing | FW team | Integration test |
| OI-CVNS-02 | Safety MCU bare-metal firmware (STM32G071, IEC 62304 Class C) must be authored and certified separately | Embedded safety team | T2 clinical release |
| OI-CVNS-03 | UHDR session record commit to eMMC must call AES-256-XTS write with biometric-derived key | FW/Storage team | UHDR compliance |
| OI-CVNS-04 | SHDR session summary write must call SHDR storage API after each session | FW team | SHDR compliance |
| OI-CVNS-05 | IRB approval required before FAI-CV03 tolerability study can be executed | Regulatory/Clinical team | T2 clinical release |
| OI-CVNS-06 | FAI-CV02 hardware bench (R-peak signal generator, oscilloscope) must be completed on T2 prototype | HW team | Full G3-08 closure |
| OI-CVNS-07 | FAI-CV01 anatomical phantom bench must be completed with T2 accessory prototype | HW team | Full G3-08 closure |
| OI-CVNS-08 | STM32G071 pin map: §5.1 and `np_safety_config.h` disagree on every row. Nothing in-tree settles it. | HW/Embedded safety team | PCB layout (G1) |
| OI-CVNS-09 | One CVNS enable line or two (per-electrode)? Clinical/regulatory question, not a code-style one. | Regulatory/Clinical + Embedded safety | PCB layout (G1); T2 510(k) |
| ~~OI-CVNS-10~~ | ~~Cardiac baseline window: safety MCU 8 intervals vs main processor 5. Deliberate or accidental?~~ **CLOSED 2026-09-29 (Rev 9), moot:** the ±5 BPM cross-check that made the two related was never built. The MCU window moves to OI-CVNS-12 (§14.3) | Embedded safety team | — |
| OI-CVNS-11 | Safety MCU applies no R-R validity filter (§5.3 step 2). Intended, or a gap? **(Rev 10)** Now also the main lever on OI-CVNS-13's false trips. *(Rev 11: behind the hub, an MCU 300 ms refractory changes no false-trip figure, because the hub's 300 ms bound already removes those edges (§14.6). Its value is independence from Class B only.)* | Embedded safety team | Class C design freeze |
| ~~OI-CVNS-12~~ | ~~The Class C cardiac interlock does not meet CLAUDE.md §4.2's "HR change > 15 BPM within 5 s".~~ **CLOSED 2026-10-01 (Rev 10), principal decision.** The refresh is replaced by a comparison against an 18 s history of 1 s snapshots. The window stays at 8, and the cross-check is not built. CLAUDE.md §4.2 splits detection from cutoff, and FAI-CV02 is amended (§14.5.1) | Embedded safety team + principal | — |
| **OI-CVNS-13** | **The lagged comparison false-trips more than the refresh did.** In simulation, at 100 BPM with 50 ms R-R SD and 1 % artefacts, 75 % of 120 s sessions trip (Rev 9 rule: 36 %). The simulation's jitter is white noise, which is pessimistic for real HRV. The real rate is unmeasured, and so is its clinical cost (30 s lockout + app confirmation per trip) (§14.6). **(Rev 11, principal 2026-10-01)** Analysed through the hub stage. The largest cause below 60 BPM was the hub's pulse gate, not the rule, and that is fixed (§6.2 step 7). Median, longer-window and persistence levers are rejected. **Open for the residual:** missed and split detections, which need measured A13 detection rates and a clinical acceptable nuisance rate | Embedded safety team + Clinical | T2 clinical release; FAI-CV03 |

### 14.1 OI-CVNS-08 — STM32G071 pin map

Rev A §5.1 and `firmware/safety_mcu/include/np_safety_config.h` disagree on **every** signal: `RPEAK_IN` (PB0 vs PA8), SPI1 (PA1–PA4 vs the header's own PA4–PA7 comment), `HEARTBEAT_WATCHDOG` (PA7 vs no such pin — the heartbeat is in-band on SPI), and the CVNS enable itself (PA5/PA6 vs PB5).

This is not a typo. Rev 1 puts the cardiac *input* `RPEAK_IN` on PB0, which the header assigns to `NP_EN_BES_PIN`, a stimulation *output*. That single row means Rev 1's map cannot be adopted wholesale, whatever is decided about the rest.

**Updated 2026-08-05 after NP-HW-HUB-001 §7.2 landed (PR #247).** The PA-bank picture changed materially and *in Rev 1's favour*. The five per-zone PBM enables were retired for one `NP_EN_PBM_CRANIAL` on PA0, **releasing PA1–PA3 and resolving the PA4 double-assignment** (PA4 was claimed by both `NP_EN_PBM_ZONE4_PIN` and the SPI1 NSS comment; it is now SPI1 NSS only — see the header's own note and NP-HW-HEXTILE-001 OI-HEXTILE-13(c)). Consequences for this open item:

- Rev 1's "SPI1 on PA1–PA4" is **no longer blocked** — PA1–PA3 are free and PA4 is already NSS. The remaining disagreement is only *which* of the freed pins carry SCK/MOSI/MISO, not whether they exist.
- Rev 1's `CVNS_ENABLE_L`/`_R` on PA5/PA6 and `HEARTBEAT_WATCHDOG` on PA7 are consistent with that placement — the whole PA-bank half of Rev 1's map is now internally coherent as a board, which it was not before.
- **`RPEAK_IN` on PB0 remains the one row that cannot work in either reading**, since PB0 is still `NP_EN_BES_PIN`.

So the earlier framing — "one of the two documents a board that cannot exist" — was true of Rev 1 when written and is now too strong. The disagreement has narrowed to one impossible row plus a genuine bank-allocation choice.

| Candidate | Evidence for | Evidence against |
|-----------|--------------|------------------|
| **A — the code is right; §5.1 is stale.** Correct §5.1 to PA8 / PB5 / in-band SPI heartbeat. | The code is self-consistent for the cardiac path, is internally consistent on the PA bank since §7.2 resolved the PA4 double-assignment, and is what the Class C unit and its new host tests actually exercise. `np_safety_config.h` is the file an implementer reads. | The header states of itself that "GPIO bank assignments are provisional pending PCB layout (G1 gate)" — it does not claim to be authoritative, so "the code is right" is an assumption, not a finding. |
| **B — §5.1 is the intended board; the code's provisional map is stale.** Re-map the firmware at G1. | `np_safety_config.h` states in its own header comment: "GPIO bank assignments are provisional pending PCB layout (G1 gate)" — the code does not claim authority. | §5.1's `RPEAK_IN`-on-a-stimulation-output row means Rev 1's map cannot be adopted wholesale regardless. |

**Why the zone enables were never good evidence.** `NP_EN_PBM_ZONE0..4` was stale before it was retired: zones are not a fixed set of five hardware slots — a zone is a named set of sockets on the hex lattice, and `protocols/predefined/00-zones.npps` states in its own header that it "is the only definition of a zone" (see also CLAUDE.md §3 and `hardware/np_socket_map.json`). Anyone re-opening this item should not resurrect "SPI1 collides with the PBM zone enables" as an argument; that collision is gone.

**What would settle it:** the STM32G071 schematic / PCB netlist from the G1 layout package — the single artefact that makes one of these maps a fact. Until it exists, neither is a requirement. The former sequencing dependency on `NP_SAFETY_EN_PBM_CRANIAL` (OI-HUB-C07) is **discharged** — that landed in NP-HW-HUB-001 §7.2 — so this item is now blocked on G1 alone.

**Do not** edit `np_safety_config.h` to match §5.1, or §5.1 to match `np_safety_config.h`, before that package exists. The programme is pre-tooling with no hardware committed (CLAUDE.md), so changing the code is cheap today — but cheap is not the same as decided, and this is the enable path of a Class C cardiac interlock.

### 14.2 OI-CVNS-09 — single vs dual CVNS enable line

Rev A §5.1 specified `CVNS_ENABLE_L` and `CVNS_ENABLE_R`; §5.4.2a instructed asserting both. The firmware has a single `NP_EN_CVNS`. §5.4.2a has been corrected to describe the single-line cutoff the firmware performs — **that correction is descriptive, not a ruling that one line is sufficient.**

| Candidate | Evidence for | Evidence against |
|-----------|--------------|------------------|
| **A — one enable line is sufficient.** The hazard (RISK-25) is a baroreceptor reflex from stimulation near the carotid sheath; cutting all cervical stimulation addresses it. | Simplest thing that mitigates the stated hazard; one line, one failure mode, less Class C surface. `NP_CVNS_ELECTRODE_COUNT` = 2 describes a bilateral *assembly*, not two independent current paths. | Loses the ability to drop one electrode on a unilateral fault (e.g. the per-electrode impedance divergence already tracked as OI-CVNS-HUB-11 has no per-electrode remedy). |
| **B — per-electrode enables are required.** Restore two lines. | §5.2/§7 contemplate bilateral or unilateral montages; a unilateral electrode fault currently has only an all-or-nothing response. | No hazard in RISK-25 is currently shown to need per-electrode granularity; adding a second Class C enable line costs certification surface for an unquantified benefit. |

**What would settle it:** a hazard-analysis pass against the gammaCore 510(k) predicate (K163334 cluster headache, K173323 migraine) asking specifically whether the predicate's cleared interlock acts per-electrode or on the whole cervical channel, plus an FMEA line for "one electrode faults, the other continues". This is a clinical/regulatory determination and belongs with Regulatory/Clinical, not with firmware. Feed the result into RISK-25 before the T2 510(k) submission.

### 14.3 OI-CVNS-10 — cardiac baseline window, 8 vs 5

The safety MCU arms on 8 intervals (`NP_CARDIAC_BASELINE_BEATS`, ring buffer `NP_RR_BUF_SIZE` = 8); the main processor uses 5 (`NP_CVNS_BASELINE_BEATS_MIN`, buffer 20). Rev 1 documented the main processor's 5 as if it were the MCU's — so **the divergence was invisible until now, and no record exists of anyone choosing it.**

| Candidate | Evidence for | Evidence against |
|-----------|--------------|------------------|
| **A — the difference is deliberate.** The two sides are independent implementations that cross-validate at ±5 BPM; identical windows would make them correlated and weaken the check. Slower arming on the Class C side is the conservative direction. | The independence rationale is real and is now stated in §5.3. A longer window is more noise-tolerant, which matters more on the side with no validity filter (OI-CVNS-11). | Nothing in the DHF, this document, or the code comments says it was chosen. Nobody has written down the intent. |
| **B — the difference is accidental.** One side was written first and the other drifted. | The MCU's 8 equals its ring-buffer depth exactly, which reads more like "use the whole buffer" than like a tuned clinical parameter. | Even if accidental, 8 may still be the right value — accidental origin is not an argument for changing it. |

**What would settle it:** a Class C design rationale entry stating the intended arming latency for each side and the noise assumptions behind it, reviewed against the ±5 BPM cross-validation tolerance (a longer MCU window means the two baselines are computed over different physiological spans, which itself affects how often the ±5 BPM check trips spuriously — that interaction is currently unanalysed).

**Do NOT harmonise by editing one number to match the other.** This is a safety parameter on a cardiac interlock; making the numbers equal is a change to interlock timing dressed as a tidy-up. `np_cardiac_interlock_tests.c` now pins the MCU's 8-interval arm point with assertions that a 5-interval implementation fails, so any change surfaces as a reviewed diff.

#### 14.3.1 Disposition — closed 2026-09-29 (Rev 9), moot

**The settlement condition could not be met, because its premise is false.** It asked for the two arming latencies to be reviewed against the ±5 BPM cross-validation tolerance. Candidate A's independence argument rested on the same check. The check does not exist:

- The hub↔safety-MCU wire protocol (`firmware/common/include/np_spi_wire_types.h`) has four command types: session signature, channel limit, channel waveform and active user. None carries a heart rate.
- `np_cvns_interlock_request_enable()` sends `NP_CVNS_SPI_CMD_HR_BASELINE_SET` through `platform_spi_send()`, a stub that transmits nothing.
- The hub module (`np_mod_cvns.c`) gates cervical VNS on the heartbeat grant bit alone.
- `NP-FMEA-001` Rev 12 withdrew the claim on 2026-09-25 (§3.5). This document still carried it until this revision (§5.3 step 5, §6.3).

**So neither candidate stands.** The windows cannot be "deliberately different for independence", because nothing compares them. Their difference cannot be a harmful accident either, for the same reason. The main processor's 5 governs only its own Class B baseline gate. The origin of the MCU's 8 is still unrecorded, and it no longer matters.

**What does matter is what the MCU's window does to detection, and that is OI-CVNS-12.** The analysis this item asked for, run against the real code, found that the window and the 5 s refresh together decide which heart-rate changes the interlock can see. At resting rates it misses most changes the requirement covers, at either length (§14.5).

**The window is not changed here.** An 8-interval window tolerates single R-peak artefacts better, which matters while the MCU has no validity filter (OI-CVNS-11). A 5-interval window detects more. Choosing it in isolation would be the harmonising edit this item forbade, made for a new reason. It is a parameter of OI-CVNS-12's redesign.

### 14.4 OI-CVNS-11 — no R-R validity filter on the safety MCU

Rev A §5.3 step 2 specified discarding intervals outside 300–2000 ms. Those bounds (`NP_CVNS_RR_MIN_VALID_MS` / `_MAX_VALID_MS`) are main-processor constants; the safety MCU has no equivalent and admits every measured interval into its ring buffer.

| Candidate | Evidence for | Evidence against |
|-----------|--------------|------------------|
| **A — intended; filtering belongs upstream.** The main processor detects R-peaks (Pan-Tompkins, with debounce and refractory period) and only pulses `RPEAK_IN` for accepted beats, so the MCU sees pre-filtered edges. | Keeps the Class C unit minimal — the stated design goal (~500 lines bare-metal, CLAUDE.md §4.2). The `rr_to_bpm()` saturation clamp exists precisely to survive artefacts, and its comment names "noise/motion artifacts on the RPEAK_IN line" as the expected source. | If the MCU trusts the main processor to filter, the two sides are no longer independent for artefact rejection — which is the property §5.3's cross-validation depends on. |
| **B — a gap.** A stuck-high or noisy `RPEAK_IN` line injects garbage intervals straight into the baseline. | The saturation clamp is a *containment* measure, not rejection: a burst of impossible intervals still shifts the mean and can move the baseline or trip a cutoff. | Adding a filter adds Class C code and a new way to reject real beats (a false negative on a cardiac interlock is worse than a false positive). |

**What would settle it:** an FMEA line for `RPEAK_IN` line faults (stuck high, stuck low, ringing) tracing what each does to the baseline and to cutoff behaviour. If the answer is "the main processor's Pan-Tompkins stage is the mitigation", that mitigation needs to be stated as a requirement on the SW-02 side rather than left implicit — at which point the ±5 BPM cross-validation's independence assumption should be re-examined.

### 14.5 OI-CVNS-12 — the interlock misses most qualifying heart-rate changes

**Requirement.** CLAUDE.md §4.2: *"HR change > 15 BPM within 5 s → GPIO cutoff < 100 ms"*. RISK-25's hazard is a baroreceptor or vagal reflex, which is bradycardia or asystole (§13).

**Mechanism.** Three facts about `np_cardiac_interlock.c`, each correct on its own:

1. Current HR is the mean of the last 8 R-R intervals, so a step change reaches it one beat at a time.
2. The cutoff compares that mean with a baseline, and fires only when they differ by more than 15 BPM.
3. While no cutoff is active, the baseline is reset to the current mean every 5 s (`NP_CARDIAC_OBS_MS`), unconditionally.

If a step takes longer than 5 s to move the 8-interval mean past 15 BPM, a refresh always lands during the transition and adopts a part-moved mean. Less than 15 BPM is then left to travel. A 70 → 50 BPM fall needs 6 intervals at 1.2 s (7.2 s) to cross the threshold, so it is **never** cut. That holds whatever the phase of the refresh. Asystole is still caught, by the 3 s staleness cutoff (§5.4 step 4).

**Evidence.** A host simulation (`firmware/safety_mcu/tests/analysis/run_oi_cvns_12_sim.sh`, fixed seed, reproduces every figure below) linked the unmodified `np_cardiac_interlock.c` to a mocked HAL, ticking every 1 ms with beats in real time. It ran 200 trials per cell, each with a random step time and refresh phase and no heart-rate variability. The table gives the percentage of sustained instantaneous steps that were cut within 40 s:

| Start HR | Step | 8 intervals (as built) | 5 intervals | 8, refresh disabled | 5, refresh disabled |
|---|---|---|---|---|---|
| 60 | −20 | 0 % | 0 % | 100 % | 100 % |
| 70 | −20 | **0 %** | 37 % | 100 % | 100 % |
| 70 | −16 | 0 % | 0 % | 100 % | 72 % |
| 70 | +20 (FAI-CV02's step) | **31 %** | 76 % | 100 % | 100 % |
| 90 | −20 | 18 % | 55 % | 100 % | 100 % |
| 90 | +16 | 18 % | 51 % | 100 % | 100 % |

When a step is cut, the mean latency from the step is 3.4–6.5 s at N = 8. Steps of 40 BPM are usually cut, but a 40 BPM fall from 60 is cut in only 18 % of trials. Steps spread over 2.5–5 s do worse than instantaneous ones.

**The window is a trade-off, not a fix.** With 120 s of steady rhythm at 50–100 BPM, R-R SD ≤ 50 ms and 1 % of beats missed or split, the 8-interval window false-tripped in 0–36 % of sessions. The 5-interval window false-tripped in 0–74 %. The worst case for both is 100 BPM. The 8-interval window absorbs a single artefact that the 5-interval one does not, which matters while the MCU has no validity filter (OI-CVNS-11).

**Pinned.** *(Rev 10: this test is replaced, §14.5.1.)* `np_cardiac_interlock_tests.c` `test_refresh_absorbs_sustained_fall_KNOWN_DEFECT` asserts that the 70 → 50 fall is not cut in real time. As a control, it also asserts that the same fall *is* cut when SysTick is held back so that no refresh lands. With the refresh removed (mutation), the defect assertion fails. A fix is meant to fail it.

**FAI-CV02 as written cannot pass.** Its procedure is the 70 → 90 BPM step, which is cut in about 30 % of trials. CV02-A also measures latency **from the first out-of-window R-peak**. Even with no refresh, the 8-interval mean needs 7 of those beats (about 4 s) before it crosses 15 BPM, so ≤ 100 ms is unreachable. CLAUDE.md §4.2 reads as "< 100 ms from detection", and detection time has no stated bound. The requirement needs a detection-time bound as well as a cutoff latency, and FAI-CV02 needs to measure each separately.

**Candidates.** Each changes Class C behaviour on an S5 risk, so none is taken here. *(Rev 10: B, in its lagged form, was taken; §14.5.1.)*

| Candidate | For | Against |
|---|---|---|
| **A — refresh only when stable.** Adopt the new mean only if it is within a band (e.g. ≤ 5 BPM) of the current baseline, or only if the last N intervals are within a band of each other | Keeps drift tracking for the slow changes it was written for, and stops absorbing a step in transit. Smallest code change | Picks a new constant (the band) that needs its own derivation, per CLAUDE.md §18. A slow ramp faster than the band still escapes |
| **B — compare against a session baseline too.** Keep the rolling baseline for drift, but also cut on > 15 BPM against the baseline frozen at arming, or against the rolling baseline from 5 s earlier | Matches the literal requirement ("within 5 s" is a comparison 5 s apart). Detection no longer depends on the refresh phase | A fixed session baseline turns legitimate drift over a 120 s session into cutoffs. Needs a clinical view of how much drift a cervical session sees |
| **C — shorter window plus a validity filter** (OI-CVNS-11) | Faster detection, and artefact rejection moves into Class C | Rejecting intervals can reject a real bradycardic beat, and a false negative is the worse failure on this interlock. On its own it does not remove the refresh race |
| **D — build the ±5 BPM cross-check** (§5.3 step 5) | Restores the independence the document once claimed | Addresses neither detection sensitivity nor the refresh race. It is a separate question |

**What would settle it.** A principal decision on the detection rule and its constants. Each constant needs a derivation that answers §18's two questions. The decision should be verified against the simulation above, extended with heart-rate variability and artefacts, and pinned by host tests. FAI-CV02 then needs amending to test detection time and cutoff latency separately. RISK-25 control C1 cannot be counted as effective until this closes (`NP-RISK-002` OI-RISK2-08).

#### 14.5.1 Disposition — closed 2026-10-01 (Rev 10), principal decision

**Decided (principal, 2026-10-01):**
1. **Rule:** candidate B in its lagged form. The rolling baseline and its 5 s refresh are removed. The current 8-interval mean is compared with **every** 1 s snapshot of itself from the last 18 s, and the cutoff fires on any difference over 15 BPM (§5.3 step 6, §5.4 step 2). Nothing is refreshed, so nothing can race a step in transit.
2. **Floor:** the horizon must resolve a final rate down to **40 BPM**. With the 5 s spread the requirement allows and one snapshot of phase, that makes the horizon 18 s (§5.3, horizon derivation).
3. **Requirement text:** CLAUDE.md §4.2 now states detection and cutoff separately (Rev 61). A change of more than 15 BPM, spread over up to 5 s and ending at or above 40 BPM, is detected within 18 s of its onset. The GPIO is cut within 100 ms of detection. FAI-CV02 measures each (§9).

**Not taken:**
- **A.** A band-gated refresh still absorbs up to its band. With a 3 or 5 BPM band, 16 BPM steps were missed in 16–56 % of trials.
- **C** is left to OI-CVNS-11.
- **D** is not built.
- **The window stays at 8.** Under the new rule, every simulated step is already cut at 8. A 5-interval window false-trips more on single artefacts (§14.5). A 5-interval window under the new rule was not simulated.

*Correction, recorded because the decision was asked with it.* The question put to the principal derived the horizon as 8 × 60 / HR_floor = 12 s. That covers only an instantaneous step. At 12 s, a 70 → 54 BPM fall spread over 5 s is cut in 73.5 % of trials. The 40 BPM floor the principal chose is kept, and the horizon is derived again from it with the spread and the phase term: 18 s. Going from 12 s to 18 s costs 0–6 percentage points of false trips in the table below.

**Evidence** (`run_oi_cvns_12_sim.sh`, now run against the unit as built; same scenarios, seed and 200 trials per cell as §14.5):

| Start HR | Step | Rev 9 (refresh) | Rev 10 (18 s history) | Rev 10 mean / max latency |
|---|---|---|---|---|
| 60 | −20 | 0 % | **100 %** | 9.5 / 10.0 s |
| 70 | −20 | 0 % | **100 %** | 7.6 / 8.1 s |
| 70 | −16 | 0 % | **100 %** | 9.3 / 9.7 s |
| 70 | +20 (FAI-CV02's step) | 31 % | **100 %** | 5.1 / 5.5 s |
| 90 | −20 | 18 % | **100 %** | 5.5 / 5.8 s |
| 90 | +16 | 18 % | **100 %** | 4.9 / 5.2 s |

All 120 cells (starts 50–110 BPM, steps ±16–40 BPM, instantaneous or ramped over 2.5 or 5 s) are cut in 100 % of trials. Where the final rate is at least 40 BPM, the longest latency is 15.9 s, inside the 18 s bound. The worst case is a +16 BPM rise from 50 over 5 s. Below the floor nothing is guaranteed. The worst cell there is 50 → 34 over 5 s, cut in every trial at up to 18.3 s.

**False trips** (120 s sessions of steady rhythm; R-R jitter is white Gaussian noise; artefacts are beats missed or split at equal rates):

| HR | R-R SD | Artefacts | Rev 9 | Rev 10 |
|---|---|---|---|---|
| 50 | ≤ 80 ms | ≤ 3 % | 0–0.3 % | 0–2 % |
| 70 | 50 ms | 1 % | 0 % | 5.7 % |
| 70 | 50 ms | 3 % | 5.7 % | 36 % |
| 100 | 50 ms | 0 % | 1 % | 14 % |
| 100 | 50 ms | 1 % | 36 % | 75 % |
| 100 | 80 ms | 0 % | 49 % | 98 % |

*(Rev 11: this table fed raw detections to the MCU. Through the hub's pulse gate as built, missed detections below 60 BPM tripped 36–94 % of sessions through the staleness cutoff. §14.6.1 has the figures and the fix.)*

**The cost: more false trips.** The old refresh hid artefacts and HRV the same way it hid real steps. White-noise jitter overstates beat-to-beat variation compared with real HRV, which is correlated and falls as the rate rises. So these are upper-bound conditions, not predictions. A false trip is the fail-safe direction, but each one is a 30 s lockout, an app confirmation and a repeat impedance check. That cost is OI-CVNS-13.

**Pinned** (`np_cardiac_interlock_tests`; beats in real time with the main loop ticking every 1 ms):
- 70 → 50 is cut on the 6th post-step interval.
- 70 → 90 is cut within 8 intervals.
- 70 → 54 (16 below the truncated 69 snapshot) is cut on the 8th.
- 56 → 40 is cut. At its pinned snapshot phase, a horizon of 11 fails.
- 56 → 52 → 40 within 5 s is cut. At its pinned phase, a horizon of 16 fails. A realisable beat train cannot separate 17 from 18, so the 18th second is the analytic phase allowance.
- A 30 BPM drift over 60 s is not cut.
- A lockout expiry reseeds the history, so re-enable stays reachable at a still-elevated rate. Without the reseed, five existing lockout and re-enable assertions fail.

Each was mutation-checked. Executed lines rise from 524 to 591 (`ci/host-test-floors.txt`).

**Still open after closure:** RISK-25's re-score and the `NP-FMEA-001` row are owed by the Quality Lead (`NP-RISK-002` OI-RISK2-08, items 1–2). OI-CVNS-11 and OI-CVNS-13 remain. FAI-CV02 has not run.

### 14.6 OI-CVNS-13 — false trips under the lagged comparison

*Rev 10 text, retained:*

> **What is known.** §14.5.1's false-trip table: under white-noise jitter, a 120 s session at 100 BPM with 1 % artefacts trips 75 % of the time. At 70 BPM, 1 % artefacts give 6 %, and with no artefacts no trips occur up to 80 ms SD.
>
> **What is not.** The real R-R jitter and artefact rate at `RPEAK_IN`. They depend on A13's PPG and on the hub's Pan-Tompkins stage (`OI-CVNSHW-03`). How often a cervical session runs at 100 BPM. What a lockout costs a patient clinically.
>
> **Levers, none taken:** an R-R validity filter in Class C (OI-CVNS-11); a longer window, which slows detection; a requirement that the excursion persist across two snapshots, which adds up to 1 s of detection time. Each changes Class C behaviour on an S5 risk.
>
> **What would settle it:** a measured false-trip rate on recorded or bench R-peak trains from A13 (FAI-CV03 or earlier), set against a clinical judgement of an acceptable nuisance-cutoff rate. If the rate is not acceptable, OI-CVNS-11 is decided with it.

#### 14.6.1 Analysis (Rev 11)

**What §14.5.1 did not model.** The safety MCU does not see detections. It sees the pulses the hub emits, and step 7 of §6.2 decides which detections become pulses. §14.5.1's simulation fed raw detections to the MCU. The Rev 11 simulation (`firmware/safety_mcu/tests/analysis/run_oi_cvns_13_sim.sh`, fixed seed, 300 sessions per cell) puts the hub's rule in between, using its real constants. It links the real `np_cardiac_interlock.c`, and builds each Class C lever as an edited copy of it. It also separates missed detections from split ones, and adds two scenarios: a correlated heart-rate model (respiratory sinus arrhythmia, RSA) and physiologically dropped beats.

**Finding: below 60 BPM, the hub caused most of the trips.** As built, the hub pulsed a peak only if its interval was 300–2000 ms. At 50 BPM, one missed detection makes a 2400 ms interval. That peak was not pulsed, so the MCU's gap ran from the previous pulse to the next one, 3600 ms. That exceeds `NP_CARDIAC_RPEAK_STALE_MS` (3 s), and the staleness cutoff fired. The 3 s staleness bound was chosen as a 20 BPM rhythm (§5.4 step 4). The hub's gate had silently lowered it to "any single interval over 2 s" for a one-beat dropout. Nobody chose that.

False trips per 120 s session, 8-interval rule as built, white R-R jitter, artefact rate per beat:

| HR | R-R SD | 1 % missed: hub as built | 1 % missed: hub Rev 11 | 3 % missed: as built | 3 % missed: Rev 11 | 1 % split | 3 % split |
|---|---|---|---|---|---|---|---|
| 50 | 20 ms | 60.7 % | **0.3 %** | 93.7 % | **6.3 %** | 5.0 % | 27.3 % |
| 50 | 50 ms | 64.0 % | **0.7 %** | 93.7 % | **7.3 %** | 4.7 % | 30.0 % |
| 60 | 20 ms | 36.3 % | **1.0 %** | 76.0 % | **7.7 %** | 9.0 % | 35.0 % |
| 60 | 50 ms | 38.7 % | **1.7 %** | 78.7 % | **9.7 %** | 5.3 % | 32.0 % |
| 70 | 20 ms | 4.0 % | 4.0 % | 27.3 % | 21.7 % | 3.0 % | 35.3 % |
| 70 | 50 ms | 8.7 % | 8.3 % | 39.0 % | 39.0 % | 10.7 % | 47.7 % |
| 100 | 20 ms | 21.7 % | 21.7 % | 74.0 % | 74.0 % | 5.0 % | 21.7 % |
| 100 | 50 ms | 87.7 % | 87.7 % | 98.7 % | 98.7 % | 36.0 % | 75.7 % |

The split columns are the same for both hubs. With no artefacts, no session trips except at 100 BPM with 50 ms SD (10.7 %). So §14.5.1's 50 BPM row (≤ 2 %) held only for raw detections. Through the hub as built, it was 61–94 % whenever detections were missed.

**Correlated heart-rate variability.** RSA modulates R-R by ±5–15 %, with 10 ms of white jitter on top. At 0.25 Hz (spontaneous breathing), no session trips at 50, 70 or 100 BPM. At 0.1 Hz (paced breathing near the resonance frequency), none trip at 50 or 70 BPM. At 100 BPM, ±10 % trips 27 % and ±15 % trips 100 %. A 0.1 Hz swing is slow enough to pass through the 8-interval mean, and it is a real heart-rate change of about 20–30 BPM. Whether a cervical session would ever run with paced breathing at 100 BPM is a clinical question. It is recorded here because the platform's T1 HRV biofeedback (CLAUDE.md §3 ⑥) teaches exactly that breathing.

**Decided (principal, 2026-10-01): the hub pulses long intervals.** §6.2 step 7 now pulses every peak ≥ 300 ms after the last detected peak, with no upper bound, and buffers only 300–2000 ms intervals for its own baseline. The bold cells above are the result.

**What the change costs.** All 120 step and ramp cells are still cut in 100 % of trials, with a longest latency of 15.9 s for a final rate ≥ 40 BPM. With every 2nd, 3rd or 4th beat physiologically absent from 60, 70 or 90 BPM, every case is cut, with the same latencies as before. Two things are no longer cut *incidentally*, and the Quality Lead should see both when RISK-25 is re-scored (`NP-RISK-002` OI-RISK2-08):
- **A 50 → 37.5 BPM rhythm** (every 4th beat absent from 50). This is a 12.5 BPM change, which does not qualify under CLAUDE.md §4.2, and it ends below the 40 BPM floor. The hub as built cut it through staleness. From 50 BPM, every 2nd or 3rd beat absent (→ 25 or 33 BPM) is still cut, at up to 10.8 s and 15.6 s, against 4.2 s and 5.4 s before.
- **A single sinus pause of 2–3 s** at ≤ 60 BPM. The design bound for a pause is the 3 s staleness rule, and the MCU now applies that bound as specified.

**Levers on the MCU side, each simulated against the unit (hub Rev 11 unless stated):**

| Lever | Detection | False trips | Disposition |
|---|---|---|---|
| **Median of the 8 intervals** instead of their mean | **Never detects dropped-beat bradycardia:** every 4th beat absent from 70 (→ 52.5) or 90 (→ 67.5) is cut in 0 %. Up to 3 long intervals in 8 never move a median. It also misses the ±16 BPM steps in 9 of the 120 step cells, which are cut in 0 % of trials | Lower for missed detections (3 % missed at 100 BPM, 20 ms: 0.3 % vs 74 %). Higher for jitter (100 BPM, 50 ms, no artefacts: 46 % vs 10.7 %) and for 0.1 Hz RSA ±10 % at 100 BPM (96 % vs 27 %) | **Rejected.** A long interval is the signature of the hazard. Second-degree AV block is a vagal effect, so an estimator robust to long intervals is blind to the hazard |
| **12-interval window**, with the history re-derived to 24 s (1 + 5 + 12 × 60/40) | All steps cut, but the longest latency is **19.8 s**, beyond CLAUDE.md §4.2's 18 s | The largest reduction with no blind spot: 1 % missed at 100 BPM, 20 ms: 13 % vs 22 %; 3 % split at 70 BPM, 50 ms: 24 % vs 48 %; no RSA trips | **Not taken.** It needs §4.2's detection bound raised to about 24 s. Held in reserve if the measured residual is unacceptable |
| **Persistence:** the excursion must hold for 1 s (history 19) | All cut; the longest latency is 17.8 s, and the analytic bound becomes 19 s | Small: 1 % missed at 100 BPM, 20 ms: 17 % vs 22 % | **Not taken.** A missed detection stays in the window for 8 beats, so 1 s of persistence does not outlast it |
| **MCU refractory:** ignore an edge < 300 ms after the last | Unchanged | Behind the hub: identical to the unit as built, because the hub's 300 ms bound already removes those edges. On raw detections: 1 % split at 100 BPM, 20 ms: 86 % → 4 % | **Left to OI-CVNS-11.** Its only value is independence from Class B |
| **Ectopic or relative filter** (reject an interval far from the current mean) | Not simulated. A sustained 70 → 50 BPM step lengthens every interval by 40 %, so a filter tighter than that rejects the step itself | — | **Rejected** on that argument (§14.5 candidate C) |

**What is left, and why Class C cannot remove it.** After the hub fix, two kinds of trip remain:
- **Missed detections at ≥ 70 BPM.** One missed detection moves the 8-interval mean by HR / 9, which is 11 BPM at 100. Jitter does the rest.
- **Split detections at 50–70 BPM**, whose two halves are both over 300 ms, so the hub forwards them. One split moves the mean by HR / 7.

The MCU cannot tell a missed detection from a beat the heart did not make. Rejecting it on the MCU side is the median's blind spot again. A split only raises the rate, so merging short intervals cannot hide bradycardia. But a merge relative to the current mean would hide a sudden doubling of rate. It is not simulated or proposed here.

#### 14.6.2 What settles the rest

OI-CVNS-13 **stays open**. Its question is now about detection quality, not the rule:
1. **Measure** the missed-detection and split-detection rates at `RPEAK_IN` on A13. Use recorded or bench PPG across 50–100 BPM, at rest and with motion (FAI-CV03 or earlier; `OI-CVNSHW-03`). Then run the measured R-R trains through `run_oi_cvns_13_sim.sh` in place of the synthetic ones.
2. **Clinical:** set an acceptable nuisance-cutoff rate per session. Each trip costs a 30 s lockout, an app confirmation and a repeat impedance check.
3. **Then one of:**
   - the measured rate is acceptable, and the item closes;
   - a SW-02 requirement on missed and split rates is derived from the acceptable rate (CLAUDE.md §18: it is required only once step 2 sets the rate);
   - the 12-interval window with a ~24 s detection bound goes to the principal.

No requirement figure is set here, because step 2 has not set a rate for one to derive from.

**Pinned** (`np_cvns_fai_tests`, `fai_rpeak_forwarding`): at 50 BPM, a 2400 ms interval is pulsed and kept out of the hub's buffer, and a peak 250 ms after a beat is not pulsed. Restoring the 2000 ms bound on the pulse gate fails the test (mutation-checked).
