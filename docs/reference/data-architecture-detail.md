# Data architecture — boundary resolutions, predictive maintenance, anonymization

> Relocated from CLAUDE.md §5.1 (the specific boundary-resolution list), §5.2 and §5.3 (Rev 40), and
> from §5.1's two full contents enumerations (Rev 43), to slim the always-loaded core. Content is
> verbatim; section numbers are unchanged. CLAUDE.md §5 keeps the UHDR/SHDR definitions, the
> defining tests, the when-in-doubt rule, the conditional-redaction rule and a representative
> handful of each contents list — the parts that decide most classification questions without this
> file.
>
> **Read this file when:** classifying a new telemetry field, touching the SHDR schema or fleet DB,
> working on predictive maintenance, or implementing the research anonymization pipeline.
>
> **The list below is authoritative per field and is the record `scripts/check-redaction-shape.ts`
> enforces the shape of.** A field not on it is decided by CLAUDE.md §5.1's defining test, and the
> answer is added here.

### 5.1 Contents of each record — full enumeration (relocated from §5.1, Rev 43)

CLAUDE.md §5.1 keeps each record's owner, access rules, defining test, storage and the two general
rules, plus a representative handful of each contents list. The complete enumerations are here.

**UHDR contents:** EEG waveforms (all channels) · HRV time series · PPG optical signal · neurofeedback performance scores · session timestamps and duration · protocol parameters used · closed-loop adaptation events · PBM dose (J/cm²) per zone · user-entered symptom/outcome logs · eye-open/closed state during sessions

**SHDR contents:** LED output ratio per zone · NTC temperature profiles · EMF shielding attenuation ratio · device session count (unsigned integer, no timestamps) · consumable session counts · USB-C insertion counter · PD negotiation log · impact events (g-force, orientation — between sessions only) · fan RPM · supercapacitor cycles · firmware version history · OTA log · accessory authentication pass/fail · calibration coefficient history

> Neither list is closed, and neither overrides the defining test. A field that is not on a list is
> classified by CLAUDE.md §5.1's defining test, then added — to the boundary-resolution list below
> if the answer needed argument, or to the enumeration above if it did not.

### 5.1 Specific boundary resolutions (list relocated from §5.1)

Specific boundary resolutions:
- Raw EEG impedance → UHDR; derived trend slope → SHDR
- Accelerometer during active sessions → UHDR; impact events between sessions → SHDR **as two derived booleans only** (`drop_detected`, `maintenance_alert` — NP-FW-EMMC-002 §G.2). **Both booleans are computed from unvalidated placeholder thresholds** (`15.0f` g; 3 drop-bearing gaps in 7), so what a row means is not yet established — OI-EMMC2-09. **One bounded exception, NP-FW-EMMC-002 §H:** an *enrolled* device additionally emits a coarsened per-gap impact histogram (counts per fixed g-bin — never a raw series, per-event value, orientation vector or per-event timestamp) into `shdr_accel_characterisation`, for the duration of a time-boxed characterisation window, consented separately by the **warranty owner**, purpose-bound to predictive-maintenance model training only, deleted from the fleet DB at window close. The window is denominated in **records, not calendar time** — this device has no clock that survives a disconnect — and expires fail-closed in firmware. Non-enrolled devices, which is the fleet by default, are governed by §G.3 unqualified. **Caveat, OI-EMMC2-11:** §G.3's ban on a cumulative drop count and on per-drop timing is already defeated by aggregation over `shdr_accel_records`' per-gap rows, independently of §H
- Raw ambient light → UHDR; cumulative UV exposure index → SHDR
- Raw VNS impedance → UHDR; contact resistance trend → SHDR
- Cervical VNS per-electrode impedance reported by the safety MCU to the hub for cross-validation (OI-CVNS-HUB-11) → UHDR (raw tissue impedance), transferred device-internally only, NEVER written to SHDR; the hub-vs-MCU divergence FLAG (`NP_CVNS_SHDR_EV_IMP_CROSSVAL`, no kΩ values, suppressed timestamp) → SHDR
- Cervical VNS gel pad contact pass/fail per electrode, with the side of the neck each pad is on (`CVNS_PAD_STATUS`, `OI-ACC-07`) → UHDR (derived from raw tissue impedance; the side reveals the wearer's montage). Delivered to the user's own app for display only — held in memory, never persisted, cleared on disconnect, NEVER written to SHDR
- Cervical VNS offline-fault summary (`CVNS_FAULT_STATUS`, `NP-SW-FAULTMSG-001` P3). Contents: the fault kind and failing-pad side from the active user's own session records, ordered by device session counter, with no timestamps and no heart-rate values → UHDR. Delivered to the user's own app. The app persists only a per-profile "last acknowledged session counter", on the phone. NEVER written to SHDR **Hub store (2026-09-23, §9.6):** the hub persists the last 8 fault stops across all users (session counter, kind, pad side, and the opaque user tag named at the time) and the last-named tag, as one CRC-checked blob in the **UHDR** partition only. A frame never carries another named user's records.
- Safety-MCU cardiac-cutoff persistence (`np_nv_state.c`: which user tags have an outstanding cutoff) → device-internal. Never SHDR-reportable, never timed, like the fault latch's `tick_ms`. **Two bits cross to the app, and only to the app** (`np_safety_nv_report_t`): *the active user's cervical VNS is withheld*, and *someone on this device has an outstanding cutoff*. The second is shown to every profile as the blanket warning. **That is a deliberate, principal-approved disclosure** (2026-09-22): anyone connecting to a shared device learns that *someone* on it had a cardiac cutoff, never who. The alternative let a person switch profiles to get round their own block with no prompt. Its shape is fixed, a flag set whatever the viewer's identity, so it does not also act as an oracle for *which* profile is blocked (§5.1 rule 2)
- Active-user tag (`ACTIVE_USER`, first 4 bytes of the app profile's random UUID) → neither UHDR nor SHDR content. It is an opaque, unnamed handle held by the safety MCU so that a cutoff is scoped to the right person. It is never uploaded
- Module UID (tile, and any accessory that carries one) → SHDR-class as a component identifier (`NP-HEX-ZM-001` §4), **but not SHDR-uploadable until `OI-UPG-07` closes** (2026-09-23). Modules carry over from a T1 to its owner's T2 (`NP-REG-UPG-001` Rev 2 §7.0), so a UID seen on two devices joins their SHDR records into *"one owner held both, and moved from a wellness unit to a medical device"*. That is an inference about a person, from a record linked to device and warranty token only. The options are: never upload it, upload a per-device salted form that cannot be joined across devices, or show that the join is harmless. No module UID is uploaded today
- IR eye state during sessions → UHDR; safety interlock log → SHDR
- In-cup microphone (audio cup, `NP-HW-AUDIO-001` `REQ-AUDIO-14`, 2026-09-24): **raw audio → neither record** — it is never stored, logged, streamed or transmitted anywhere, because it carries bystanders' voices and a bystander is neither CLAUDE.md §6.0 consent subject (`REQ-AUDIO-15`); it exists only inside the measurement call that reduces it. Seal score and in-cup level derived from it → **UHDR** (they depend on the wearer's head, hair, glasses and fit), computed and used on-device, and the foam prompt raised from them is raised on-device. NEVER written to SHDR; `CONSUMABLE_STATUS` stays the SHDR-class session count. A fleet-reported "foam worn" flag would need §5.1 rule 1's positive demonstration first
- Device session count (unsigned integer) → SHDR; session timestamps → UHDR
- Research anonymization pipeline `failed_step` (which stage aborted, esp. `NP_ANON_STEP_VALIDATE`) → UHDR/app-side only (drives user retry prompt). A per-device count of validate failures weakly signals the wearer is a re-identification outlier (small anonymity set) — health-adjacent under WA MHMD / GDPR Art. 9. If a device-health signal is needed in SHDR, log only a coarse `anonymization_failed: bool` without the stage. See `firmware/anon/include/np_anon_pipeline.h` (`np_anon_step_t`).
- Fault-latch `count` (distinct-fault-transition tally) → SHDR; fault-latch `status` + `slot` → SHDR (already in the 8-byte reply frame); fault-latch `tick_ms` (event time) → **NOT SHDR-reportable at all** — fault event timing is UHDR, matching `fault_log`'s own statement in `ci/shdr/shdr_fleet_schema.sql` ("Precise fault event timing, where it exists at all, is UHDR under the user's key") and the hub logger `np_log_shdr_fault()`, which discards its `session_ms` argument for every caller. Enforced via the single fixed-shape marshaller `np_fault_latch_build_report()` → `np_fault_latch_report_t {status, slot, count}`. **The reported record must never contain a field whose value or presence depends on the latched status word** (2026-08-12): `tick_ms` was previously zeroed only for `NP_SAFETY_STATUS_CARDIAC`, which made `count > 0 && tick_ms == 0` a self-interpreting one-bit cardiac oracle — *worse* than no redaction, because a bare relative SysTick value needs a session record SHDR does not hold, whereas the redaction pattern needed nothing. A redaction conditioned on a sensitive predicate leaks that predicate. Note the cardiac predicate is in any case published to SHDR deliberately and separately, as `fault_log.fault_type = 'CVNS_HR_CUTOFF'` (`NP_CVNS_SHDR_EV_CUTOFF` 0xC1), per the locked "safety interlock log → SHDR" rule above.

### 5.2 Predictive maintenance system (SHDR-based)

Three phases:
- **Phase 1** (0–1,000 devices, Year 1): Population-average survival analysis on time-to-failure data
- **Phase 2** (1,000–10,000 devices, Year 2): Fleet-trained LSTM on SHDR sensor trajectories
- **Phase 3** (10,000+ devices, Year 3+): Bayesian personalization — continuously revised RUL predictions

All models version-stamped by hardware revision. New revision falls back to Phase 1 until fleet data accumulates. Models deployed back to devices via OTA — competitive moat grows automatically with fleet size.

**Characterisation cohort (NP-FW-EMMC-002 §H).** Phases 2–3 are the *only* sanctioned consumer of the §H extended impact set. Devices whose warranty owner has opted in contribute a coarsened per-gap impact histogram for the duration of the characterisation window, and receive Phase 2/3 personalisation first — genuine reciprocity rather than an inducement, since a model cannot personalise for a device whose handling it cannot see. Three constraints bind this:

- **Non-coercion (CHAR-4).** Participation buys *earlier and better maintenance prediction*, never baseline safety. Every §4.2 interlock and every safety-critical reminder fires identically for non-participants; this is asserted in `firmware/shdr/tests/np_accel_shdr_tests.c`, not merely stated.
- **Selection bias.** The cohort is self-selected and plausibly differs from the fleet in the variable under study — people who opt into a handling study may handle differently. Per `NP-MOD-ID-001` §7.6, a *negative* result generalises comfortably and a positive one sized on a skewed cohort does not.
- **A known gap in the Phase 2 premise.** Phase 2 is specified as a *"fleet-trained LSTM on SHDR sensor trajectories"*, but roughly thirty SHDR fields are boolean flags derived from thresholds frozen before any fleet existed — flag streams, not trajectories. §H obtains real trajectories for one of those thirty. The general remedy is recorded as **OI-EMMC2-13** and is not adopted.

**Reminder engine rules:**
- Safety-critical: cannot be dismissed — blocks session start
- Performance-critical: snooze max 3×
- Comfort/longevity: snooze max 5×
- All reminders measurement-triggered, not calendar-triggered
- Every reminder includes measured data that triggered it + one-tap order link

### 5.3 Research data anonymization architecture (locked)

All anonymization of UHDR data for research purposes must occur **on-device**, within the NeurOne app, before any data leaves the device. NeurOne cannot access raw UHDR at any point — including for research purposes — because the biometric-derived AES-256 key is never held by NeurOne infrastructure.

**Data flow per approved study:**
1. NeurOne server sends device a signed study descriptor (study ID, approved UHDR element list, anonymization parameters: k≥10, suppression rules, date-rounding ≥1-week interval). Descriptor is cryptographically signed.
2. App reads encrypted UHDR partition in-app, applies on-device anonymization transformations: k-anonymity grouping, date/time rounding, direct identifier removal, quasi-identifier suppression per study descriptor.
3. Only the pre-anonymized, signed extract is transmitted to NeurOne research infrastructure. Raw UHDR never leaves the device.
4. NeurOne servers store extract keyed to study ID and device ID only. No persistent per-user anonymized data store. No linkage table exists that could re-identify users.
5. Researchers access aggregated study datasets with no device ID fields.

**Consent withdrawal effect:** Because each study extract is generated on-device on-demand, withdrawing consent permanently blocks the device from processing future study descriptors. No further extracts are generated or transmitted — **for any data period, including sessions predating withdrawal**. Already-published extracts cannot be individually removed from datasets (irreversibility notice given at consent time); no new data flows ever.

**Audit trail (SHDR):** Study ID, study descriptor hash, extract transmission timestamp, and extract byte count are logged in SHDR. User can inspect all studies their device has contributed to via the app. This log is never shared with researchers.

---

