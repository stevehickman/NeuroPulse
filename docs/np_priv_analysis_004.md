# Privacy Analysis and Repair — Firmware

**Project:** NeurOne  
**Document:** NP-PRIV-ANALYSIS-004  
**Revision:** 1
**Date:** 2026-11-21  
**Status:** DRAFT  
**Effective Date:** —  
**Author:** NeurOne Quality (privacy review)  
**Approved By:** — (new document)  
**References:** NP-PRIV-ANALYSIS-002, NP-PRIV-ANALYSIS-003, NP-PRIV-AUDIT-001, NP-PRIV-REM-001 (STEP-01…05, STEP-33), NP-FW-HUB-001 §2.5, NP-FW-EMMC-002 §B/§C/§E/§H, NP-FW-CVNS-001 §4.6, NP-FW-HRV-001, NP-FW-NVRAM-001 §3.2/§8.1, NP-SW-FAULTMSG-001; CLAUDE.md §4.2, §4.7, §5, §6.0, §23  
**Related Issues:** GitHub #949–#969 (`OI-PRIV-21` to `OI-PRIV-41`); related #93, #340, #443, #887, #889  
**Gate:** —  
**IEC 62304 Class:** — (review record; the code reviewed spans SW-01 Class C and SW-02 Class B)  
**Jurisdiction Scope:** Global (inferred from NP-PRIV-ANALYSIS-002/-003): US federal (FTC Act §5, HBNR, HIPAA for T2), US state (BIPA IL, WA MHMD, CCPA/CPRA CA), EU/EEA GDPR, UK GDPR and DPA 2018, Canada PIPEDA and Quebec Law 25. The binding frame is CLAUDE.md §5, §6.0 and §23, which are stricter than any of these laws.  
**Change Summary:** Rev 1 — first code-level privacy review of `firmware/`. 26 findings; 21 new ones filed as `OI-PRIV-21` to `OI-PRIV-41`.

---

## Scope and method

**Tree reviewed:** `201b992`. **Scope:** `firmware/` excluding `vendor/` and `crypto/vendor/`: about 60k lines of non-test source, with tests read only to confirm behaviour. The review was read-only, and no firmware was changed by it.

**Method:** four parallel read-only passes (storage, keys and erasure · transports, crypto and DFU · safety MCU, cardiac and cervical VNS · sensor modules, session runtime and logging). The reviewing session then re-read the cited lines of every High finding and most Medium ones.

- **Checked:** the cited lines were re-read and confirmed.
- **Reported:** one pass found it and nobody re-read it, so confirm before fixing.
- **Inferred:** a conclusion drawn from the code, not observed.

**Records.** This document names the two health data records of CLAUDE.md §5: the **User Health Data Record (UHDR)**, which belongs to the user and which NeurOne never accesses, and the **System Health Data Record (SHDR)**, which belongs to NeurOne and is linked to the device only.

**Open items.** Every finding not already tracked is filed in `docs/status/pending-decisions.md` and on GitHub. The mapping is in [Open items filed](#open-items-filed) below.

---

## Summary

**This is the first code-level privacy review of `firmware/`.** The earlier analyses covered the iOS app (NP-PRIV-AUDIT-001, NP-PRIV-ANALYSIS-002/-003), the app privacy review of 2026-10-18 (OI-ANALYTICS/FLEET/BACKUP/EDF-01), and `common/` (OI-PRIV-01…20, 2026-11-15). Firmware reached the open items only through spec steps (NP-PRIV-REM-001 STEP-01…05, STEP-33) and the transport audit (OI-SIMFW-62/70–73).

**The finding count is 26: 7 High, 13 Medium and 6 Low (some Low rows group several small items).** Twenty-one are not tracked anywhere. Five are already tracked, or extend an existing item. **Pure-privacy failures dominate.** The firmware writes UHDR-class facts into SHDR, the partition whose key NeurOne can re-derive (`np_fw_nvram_001.md` §3.2), so the data is disclosed on write, not only on upload. The writes are: session duration, which modalities ran, abort reasons including "photoparoxysmal detection", a conditional photoparoxysmal halt record, raw cVNS tissue impedance and stimulation parameters, and what a person did after a cardiac cutoff.

**The redaction-shape gate (`scripts/check-redaction-shape.ts`) passes, and that is false assurance.** It inspects five named functions for bit names. Every High SHDR finding below sits in a function it does not inspect.

**The single highest-leverage change:** make SHDR writes go through one fixed-shape marshaller per record type, put every SHDR-writing function on the gate's list, and delete the session-path SHDR writes (H1–H3).

---

## Findings

### HIGH

#### H1 · The SHDR session-end record carries session duration, the modalities used and the abort reason, and per-second SHDR records also encode duration (**new**, checked) → `OI-PRIV-21`, #949
**Where:**
- `hub_control/src/np_session_runner.c:1137-1139` (`s_ctx.shdr.duration_s`, `.abort_reason`, `.mods_active_mask`).
- `hub_control/src/np_session_log.c:297-302`, whose comment says "no user biology".
- The per-second writes: `np_session_log.c:353-356` (`NP_LOG_TAG_SHDR_EEG_CAL`, session count only, on every 1 s telemetry tick) and `:331-339` (`SHDR_PBM_HEALTH`: `pd_ratio`, `ntc_peak_c`, throttle events from session data).

**Category:** Pure privacy.

**Issue:**
- `data-architecture-detail.md` §5.1 classifies "session timestamps and duration" and "protocol parameters used" as UHDR.
- `mods_active_mask` tells NeurOne per session whether intranasal, VNS, visual, PBM or stimulation ran. The runner's own comment two lines later (`:1141-1143`) writes PBM counts every session "so the record's presence says nothing about the modality (§6.8, F13)". The mask in the same record then states the modality outright.
- `np_abort_reason_t` is labelled "SHDR-safe" (`np_hub_types.h:532`), yet it includes `NP_ABORT_EEG_SEIZURE /* photoparoxysmal detection at Oz */`, `NP_ABORT_USER` and `NP_ABORT_PAUSE_TIMEOUT`. Those are neurology and behaviour.
- Even with `duration_s` removed, the count of `EEG_CAL` records per session count is the duration in seconds.
- The in-session `pd_ratio`/NTC record contradicts the 2026-09-27 F9 ruling: the trend may come only from the idle latest-state record, never from session data.
- **Verify-context drift:** `ci/shdr/shdr_fleet_schema.sql` has no duration, modality or abort column. The device writes fields the fleet database was never designed to accept.
- WA MHMD treats data that reveals health-service use as consumer health data, and GitHub #93 is already open on exactly that question.

**Reference:** CLAUDE.md §5.1 ("when in doubt → UHDR"). Anti-patterns: Collect Now, Decide Later; Aggregation Creep. LINDDUN Linkability and Detectability.

**Remediation:**
- Reduce `np_session_shdr_record_t` to `{session_count, fw_version}`. Consumables already take the mask from the UHDR record (`np_cons_on_session_end`).
- If a device-class abort value is wanted, map the reason to `{none, device_fault, log_fail}`.
- Delete the `EEG_CAL` tick record and the zone-slot `PBM_HEALTH` branch, which is retired-path code.
- Add the ruling rows to `data-architecture-detail.md` §5.1.

#### H2 · A photoparoxysmal halt is written to SHDR conditionally and rewritten every second (**new**, checked; latent: the detector is built but not armed, OI-FWHUB-29/-32) → `OI-PRIV-22`, #950
**Where:** `np_session_log.c:394-402` (`if (v->ppx_halt) { … code = 0xEEU; /* EEG PPX halt code */ }`) and `hub_control/modules/np_mod_visual.c:305` (`v->ppx_halt = s_state.ppx_halt; /* SHDR */`).

**Category:** Pure privacy.

**Issue:**
- A photoparoxysmal response is a neurological fact about the wearer, and possibly a seizure marker.
- The write is conditional on that predicate. That is the exact pattern CLAUDE.md §5.1 rule 2 forbids, and the fault-latch cardiac oracle (fixed 2026-08-12) is the same shape.
- The halt latches, so the record repeats on every 1 s tick, and the record count gives time-since-event.
- The record also omits the `type` byte that `fault_queue_drain` writes (`:614-618`), so two layouts share tag `0x82` and a reader will misparse one of them.
- The only covering text is the generic "safety interlock log → SHDR" (`data-architecture-detail.md:49`). It predates both the detector and the conditional-redaction rule.

**Reference:** CLAUDE.md §5.1 rule 2. LINDDUN Detectability. GDPR Art. 9 (health). Precedent: FTC actions on health-adjacent apps.

**Remediation:**
- Delete the SHDR branch, and keep the halt in UHDR (the visual record already exists).
- If a device-safety counter is needed, use the fault-latch `count`, which is unconditional.
- Get an explicit §5.1 ruling that narrows line 49 for detector-driven interlocks.

#### H3 · The cVNS "SHDR" session summary holds raw tissue impedance and stimulation parameters, and closing OI-CVNS-04 as worded would ship them (**new as a privacy issue**, checked; stub today) → `OI-PRIV-23`, #951
**Where:**
- `cervical_vns/include/np_cvns_types.h:114-126`: `np_cvns_shdr_summary_t` holds `stim_freq_hz`, `stim_current_ua`, `stim_duration_s`, `electrode_config` and `impedance_left/right_kohm`, under the comment "no user biology".
- `cervical_vns/src/np_cvns_session.c:303-306`. The comment one line earlier says "raw measurement = user biology", and the next two lines copy the same values into the SHDR summary.
- `platform_shdr_write()` (`:26-31`) is a no-op whose comment reads "OI-CVNS-04: implement before compliance".
- The spec `docs/np_fw_cvns_001.md` §4.6 repeats the struct.

**Category:** Pure privacy.

**Issue:**
- §5.1 (`data-architecture-detail.md:36-37`) says cVNS per-electrode impedance is "NEVER written to SHDR".
- Protocol parameters and duration are UHDR.
- Joined to the SHDR cardiac-cutoff events by session count, this builds a per-session table of neck-stimulation dose against cardiac response, keyed to the warranty token. On a personally owned unit that token is the wearer.

**Reference:** CLAUDE.md §5.1. GDPR Art. 9. HIPAA minimum necessary (T2). Anti-pattern: Fake Anonymisation.

**Remediation:**
- Do not implement OI-CVNS-04 against this struct. Retire it, or reshape it the way OI-FWHUB-21 reshaped PBM: an impedance pass flag only, with no parameters and no duration.
- Correct `np_fw_cvns_001.md` §4.6.
- Add the summary builder to the gate's `REPORTING_PATHS`.
- Re-word OI-CVNS-04 (`np_fw_cvns_001.md` §14) so that nobody closes it by wiring the leak. A note pointing at `OI-PRIV-23` is added with this review.

#### H4 · RESUME restarts stimulation over the unencrypted, unauthenticated pause characteristic, including after a photoparoxysmal-detector halt (**extends OI-SIMFW-73**, checked) → `OI-PRIV-24`, #952
**Where:**
- `hub_control/src/np_gatt_server.c:139-142`: `SESSION_PAUSE` has no `NP_GATT_PROP_ENC`.
- `np_session_stop.c:50-60`: `0x02` routes to `s_resume`.
- `np_session_runner.c:1213-1223` (`np_runner_resume`) and `:987-989`: `do_resume()` runs for any `PAUSED` state.
- `do_panic` (`:547-611`) puts detector halts (`NP_HOLD_DETECTOR_DISCHARGE/BLIND`) into that same `PAUSED` state, and nothing checks the hold cause before resuming.

**Category:** Security, which is also a safety failure.

**Issue:**
- The §23 exception rests on the claim that stop/pause/panic "can only reduce stimulation" (`docs/human-input/oi-simfw-71-secure-channel-design.md:187`; `REQ-FWHUB-81` (c)).
- That is false for this characteristic: it also carries resume. Any unpaired central in radio range can restart a paused session.
- That includes a session the photoparoxysmal detector halted. `REQ-FWHUB-67` requires "a second step" there and says nothing of who may take it.
- This is outside strict privacy, but it is the most serious item found, and it sits on the §23 boundary.

**Reference:** OWASP missing authentication on a sensitive endpoint. Defence in Depth. ISO 14971 (a hazard control's justification is wrong).

**Remediation:**
- Split the characteristic. Stop (0x01) and panic (0x03) stay open. Resume (and pause, if wanted) moves to an `NP_GATT_PROP_ENC` characteristic.
- Refuse resume while a detector cause is held unless it comes on an authenticated link (OI-SIMFW-72).
- Correct the design note's claim, and record the split under OI-SIMFW-73.

#### H5 · The safety MCU's per-user cardiac-cutoff log sits in plaintext flash, keeps its order, and survives factory reset (**new**, checked; a safety-versus-privacy decision) → `OI-PRIV-25`, #953
**Where:**
- `safety_mcu/src/np_nv_state.c:27-34`: an append-only log of SET(tag), CLR(tag) and USER(tag), with a lifetime `seq`. Records are erased only when a page compacts.
- `factory_reset/include/np_factory_reset.h` R-1…R-12 has no safety-MCU step, and the MCU has no SPI command to clear the store.
- `docs/np_sw_faultmsg_001.md:293` and `np_fw_cvns_001.md:445` say "never erase NV pages 62–63".
- No RDP or option-byte setup appears under `safety_mcu/` (it may live in manufacturing tooling; not verified).

**Category:** Both.

**Issue:**
- After resale or a clinic handover, the old users' tags and any outstanding cutoff remain readable over SWD, and the order of cutoff, acknowledgement and user-switch events stays reconstructable.
- That contradicts CLAUDE.md §5.1: "an erase … returns the device to factory state".
- The next owner also inherits the blanket "someone on this device has an outstanding cutoff" withhold.
- `data-architecture-detail.md:42` calls it "never timed", but `seq` orders it.
- The decision to persist the cutoff (OI-FAULTMSG-01, P1) is correct for safety. Nobody decided what an erase does to it.

**Reference:** CLAUDE.md §5.1 (erase = factory state). Anti-pattern: Backup Immortality, by analogy. GDPR Art. 17.

**Remediation:**
- An ISO 14971 decision is owed (Safety, FW, the principal) on an authenticated "NV purge" command run as part of factory reset, plus what to show the next owner.
- Store only the current view (compact on every write, or erase the superseded page) so the history does not persist.
- Confirm RDP Level 1 or 2 in the production programming procedure.

#### H6 · The EDF+ research gate passes exact recording times, the recording ID, and a patient code that stays the same in every extract (**new**, checked; the spec shares the gap) → `OI-PRIV-26`, #954
**Where:**
- `edf/src/np_edf_validator.c:18-33`: it checks only sex, birth date, the name heuristic and the `"NP"` prefix.
- `np_edf_writer.c:322-346` writes `startdate`/`starttime` to the second.
- `:250-258`: the patient code is `"NP"+hex(token[0..6])`, the same in every file.
- `:196`: the name check stops at the first space (reported).

**Category:** Pure privacy.

**Issue:**
- CLAUDE.md §5.3 requires dates rounded to at least a week and **no linkage**. A code that is stable across studies lets two extracts be joined to the same person.
- The recording-ID field (§E.2 forbids a clinician name there) is never checked.
- This matches `NP-FW-EMMC-002` §E.2/§E.4, so the spec has the same gap. The STEP-05 work was marked closed (`pending-decisions.md:761`).

**Reference:** CLAUDE.md §5.3. Anti-pattern: Re-identification via Join (Netflix Prize, AOL). HIPAA Safe Harbor dates (T2).

**Remediation:**
- The gate requires blank or week-rounded dates, a per-study random code or `X`, a recording-ID check, and a scan of the whole patient-ID field.
- Amend §E.2/§E.4 and reopen the STEP-05 closure.

#### H7 · Erasure does not do what it claims (**partly new**: the CMD38 mis-mapping is new; the HAL traps are known under OI-RESET) → `OI-PRIV-27`, #955
**Where:**
- `bootloader/src/np_main.c:350-356`, the "re-sanitize all data partitions" step, calls `np_emmc_erase`. That sends `CMD38 arg=0 /* erase (not secure erase) */` (`bootloader/src/np_emmc.c:432-434`).
- `docs/np_fw_emmc_002.md:92` specifies "SANITIZE (CMD38, SANITIZE_START)". That is the wrong command: eMMC SANITIZE is started by CMD6 SWITCH to EXT_CSD `SANITIZE_START`.
- The target factory-reset HALs are `NP_PLATFORM_TRAP()`.
- `np_factory_reset_execute()` has no caller outside tests, and no handler exposes it (reported).
- Config is zeroed rather than purged, so the wrapped UKMD may survive in wear-levelled blocks.

**Category:** Pure privacy (erasure).

**Issue:** OI-RESET-01 is checked off as "IMPLEMENTED + host-tested". On silicon, an erase would be a plain erase that is not NIST 800-88 Purge, and a user cannot trigger it at all.

**Reference:** NIST SP 800-88. GDPR Art. 17. CLAUDE.md §5.1.

**Remediation:**
- Correct the spec's command, and implement SANITIZE through EXT_CSD.
- Use it in both the bootloader path and R-5.
- Purge Config as well as zeroing it.
- Wire an authenticated factory-reset command.
- Reopen or annotate OI-RESET-01.

### MEDIUM

| # | Finding | Where | Status vs existing |
|---|---|---|---|
| M1 | **cVNS lifecycle events go to SHDR without a ruling:** CONFIRM_OPEN, IMP_FAILED, IMP_TIMEOUT, REENABLED and ASSERT_TIMEOUT, each with the session count. Together they show whether the person chose to resume neck stimulation after a cardiac event, and how many contact attempts it took | `hub_control/src/np_hub_control_main.c:181-186`; `np_session_log.c:582` | **New** → `OI-PRIV-28`, #956. None of the codes appears in `data-architecture-detail.md` (checked) |
| M2 | **The active-user tag is the profile UUID's first 4 bytes**, not "a random 32-bit value" (`np_spi_wire_types.h:535`). The same tag goes to every helmet, it is stored in plaintext MCU flash (H5), and any encrypted central can write any tag to read that tag's cardiac fault records. *Inferred:* switching the tag may sidestep a per-user cardiac withhold. That is a safety question | `app/ios/NeurOne/Models/CervicalFaultStatus.swift:129-135`, `common/core/.../CervicalFaultStatus.kt:139-147`, `np_cvns_fault_summary.c:323-339` | **New** → `OI-PRIV-29`, #957. The profile id also appears in exported `.npps` files (OI-PRIV-11, #889), so the tag joins to them |
| M3 | **HRV coherence trend (30 per-session means plus slope) and the personal resonance breathing rate are designated SHDR/Config**, and OI-HRV-04 tasks someone with implementing the write | `hrv_biofeedback/include/np_hrv_types.h:145-151`, `np_hrv_pacer.h:11-15`, `np_hrv_session.c:279` | **New** as a privacy issue (checked) → `OI-PRIV-30`, #958. Re-scope OI-HRV-04 to UHDR |
| M4 | **No replay protection for signed protocols, and the serial guard fails open.** `compiled_at_unix` is never checked and there is no nonce or counter. If the serial HAL read fails, the cross-device check is skipped | `hub_control/src/np_protocol.c:194-201`, `:216` | **New** → `OI-PRIV-31`, #959. No open item covers freshness |
| M5 | **No anti-rollback.** `hdr.version` is signed but never compared, so any old signed image flashes over USB DFU | `bootloader/src/np_ota.c:411-414`, `np_dfu.c:296-313` | **New** → `OI-PRIV-32`, #960 (with M6). No open item (grepped) |
| M6 | **USB DFU is a built firmware-transfer path in the clear**, and the §2.5 transfer table omits it (it lists OTA only, as "no path exists") | `bootloader/src/np_dfu.c:8-14`, `:288` | **Extends OI-SIMFW-62/-71** → `OI-PRIV-32`, #960 (with M5) |
| M7 | **Just Works pairing:** any paired central reads `WARRANTY_TOKEN`, `DEVICE_SERIAL`, `CVNS_FAULT_STATUS` (UHDR) and session progress and hold. The warranty token goes to any paired phone, not only the warranty owner's, which weakens §6.0's separation of the two consent subjects | `np_gatt_server.h:159-160`, `np_gatt_server.c:114-120` | **Known: OI-SIMFW-72**; token release is **new** → `OI-PRIV-33`, #961 (relates to OI-WA-03 / #340, REQ-SHDR-05) |
| M8 | **One UHDR key per device** (one UKMD, files named by the device-wide count). Whoever holds the credential reads every user's UHDR, so on a clinic unit the clinic holds more than §5.1's per-element, time-limited clinician access | `uhdr_key/`, `hub_control/include/np_log_backend.h:40` | **New, inferred** → `OI-PRIV-34`, #962. No document addresses multiple users per device. Confirm against NP-APP-PRESENCE-001 |
| M9 | **The XTS key is 32 bytes.** AES-256-XTS needs 64 (K1 and K2), so this is either AES-128-XTS or the degenerate K1 = K2 case | `uhdr_key/include/np_uhdr_key_config.h:32` | **New** → `OI-PRIV-35`, #963 (with M10). Belongs in the OI-EMMC2-02 crypto sign-off scope |
| M10 | **No limit on failed unlocks** (the PIN fallback can be guessed online without limit). *Inferred:* Argon2id at 64 MiB does not fit 32 MB of RAM, and lowering it would breach "protection only goes up" | `uhdr_key/src/np_uhdr_key.c` unlock path (grep finds no attempt or lockout code) | **New** → `OI-PRIV-35`, #963 (with M9). Related to OI-UHDRK-02/-09 |
| M11 | **The accelerometer characterisation budget fails open.** Both writes are `(void)`, so a failed budget write still sends the record, and the §H window does not end. This contradicts the comment above it and CHAR-2 | `shdr/src/np_accel_shdr.c:316-317` | **New** (checked) → `OI-PRIV-36`, #964. Related to #443 |
| M12 | **No stealth mode in firmware**, and `NP_LED_FAULT` lights on a user stop or a pause timeout (any non-NONE abort becomes `ERR_SAFETY_FAULT`). The amber frequency pulse is unimplemented | `platform/include/np_sw02_platform_hal.h:165-170`, `np_hub_control_main.c:375-379`, `np_session_runner.c:1173` | **New** (checked) → `OI-PRIV-37`, #965. Only `hardware-detail.md:136` mentions stealth. LED HAL not built |
| M13 | **Data planned for or written to Config/SHDR that §5.1 would class UHDR:** the Map 3 journal keeps `dose_accum` and `ordinal` per module per session in Config, which NeurOne can re-derive (not yet written). The EDF gate emits one SHDR fault per excluded file, which reveals that a research extract ran (not wired). Accessory-authentication records carry the session count, so intranasal insertions would form a timeline (partly inferred) | `np_map3_record.c:51-56`; `edf/src/np_edf_anon_gate.c:54-55`; `np_session_log.c:555-566` | **New** → `OI-PRIV-38`, #966. The EDF-gate part relates to OI-PRIV-07 / #887 (study participation in SHDR) |

### LOW

| # | Finding | Where | Status |
|---|---|---|---|
| L1 | Plaintext lingers in RAM: `s_blob` (the protocol), `s_ctx`, the FFT scratch buffers, `s_uhdr_buf` and the adaptation ring are not zeroed at session end or at `np_uhdr_key_background()`. Debug-port lock is unverified | `np_transport.c:155-163`; `np_hrv_coherence.c:22-24`; `np_sloreta.c:63-66`; `np_hd_session.c:63` | New → `OI-PRIV-39`, #967. The firmware counterpart of OI-PRIV-12 |
| L2 | The redaction-shape gate does not cover `np_log_telemetry`, `np_log_session_end`, `log_cvns_event` or the cVNS summary builder. It matches bit names, not data flow, so it missed that IMPEDANCE (which it classes as sensitive) goes out in the fault-latch `status` byte | `scripts/check-redaction-shape.ts:76`, `:95-125` | New (a gap in the tooling) → `OI-PRIV-40`, #968 |
| L3 | The same device session counter numbers UHDR fault summaries, cVNS log entries and SHDR `fault_log`, so any UHDR disclosure joins exactly to SHDR history. CARDIAC is also set by R-peak staleness and by hub cross-check disagreement, which SHDR then labels an HR cutoff | `np_cvns_fault_summary.c:236`; `np_cardiac_interlock.c:495-512` | New → `OI-PRIV-41`, #969 |
| L4 | Comments classify commanded current and waveform as SHDR, which invites a future SHDR write | `np_spi_wire_types.h:302`, `:433`; `np_safety_spi.c:79`, `:293` | New → `OI-PRIV-41`, #969 |
| L5 | The GATT base UUID ends `"NEUP"`, so service discovery identifies a NeurOne neurostimulator. Advertising and GAP (name, RPA) are not built, so they cannot be assessed | `common/include/np_app_wire_constants.h:9` | New → `OI-PRIV-41`, #969 |
| L6 | Small items: full refusal reasons go back before authentication (`np_upload_check.c:55`); the link-encryption flag is global, not per connection (`np_gatt_server.c:30`, matters only if the stack allows two links); anon scratch AES-CTR reuses keystream if a block is rewritten (inferred, OI-ANON-AES-02); an NVSTATE fault code implies that a cardiac or user write was in flight (`np_safety_main.c:677-680`) | various | New → `OI-PRIV-41`, #969, except the scratch item (under OI-ANON-AES-02, listed in OI-PRIV-41 so it is not lost) |

---

## Comparison with existing identified issues

**Already tracked (not counted as new):**

| Existing item | What it covers | This review |
|---|---|---|
| `OI-SIMFW-62`, `-70`, `-71`, `np_fw_hub_001.md` §2.5 | No payload encryption on any transport, none at all on USB-C CDC; "the rule is not met" | Confirmed. **Adds** USB DFU, which is missing from the table (M6) |
| `OI-SIMFW-72` | Whether pairing is authenticated | Confirmed (M7) |
| `OI-SIMFW-73`, `REQ-FWHUB-81` (c) | Stop and pause/panic deliberately unencrypted | **Contradicted:** the justification is wrong because resume rides the same characteristic (H4) |
| `OI-FAULTMSG-01` (P1), the 2026-08-12 fault-latch oracle fix | Persisting the cardiac cutoff; unconditional redaction of `tick_ms` | The fix holds (`np_fault_latch.c:185-193`). Persisting was right; **erasure of the persisted store was never decided** (H5) |
| `OI-RESET-01` (checked off), `OI-EMMC2-02`, `OI-UHDRK-02/-07/-09`, `OI-ANON-AES-01…05` | Reset, key scheme and scratch implemented on host; target HALs trap; crypto sign-off pending | **Adds** the CMD38 / not-SANITIZE defect in code and spec (H7), the 32-byte XTS key (M9) and the missing unlock limit (M10). These belong in the OI-EMMC2-02 reviewer's scope |
| `OI-CVNS-04` | Implement the cVNS SHDR write | **Reframed:** implementing it as specified is the leak (H3) |
| `OI-HRV-04` | Implement the HRV SHDR trend write | **Reframed:** same problem (M3) |
| `OI-FWHUB-29/-32`, `REQ-FWHUB-67` | Photoparoxysmal detector built, not armed | **Adds** the conditional SHDR write it would trigger (H2) |
| `OI-FWHUB-21`, the PBM rulings F1–F14 (2026-09-27) | PBM SHDR reshaped to remove per-session biology | The socket-lattice path complies. **The retired zone-slot path and the EEG tick do not** (H1) |
| `OI-EDF-01`, NP-PRIV-REM-001 STEP-05 | iOS Files-app exposure of EDF+; EDF header policy | Different defects. STEP-05's gate is incomplete (H6) |
| GitHub #93 (WA MHMD), #443 (SHDR fleet telemetry, UID linkage), #340 (eMMC data-at-rest, warranty-token characteristic), #887 / `OI-PRIV-07` (study participation in SHDR), #889 / `OI-PRIV-11` (profile id in exports) | Policy-level SHDR and identifier questions | H1, M2, M7, M11 and M13 are concrete firmware instances of these |
| `OI-PRIV-12` | Keys and plaintext in unwiped memory (app/common) | L1 is the firmware counterpart |

**Not tracked anywhere (new):** H1, H2, H3 (as a privacy issue), H5, H6, the CMD38 part of H7, M1–M5, M7 (token release), M8–M13, and L1–L6 except the scratch item.

---

## Open items filed

Each new finding is an open item in `docs/status/pending-decisions.md` (§13.2, after the `common/` review's `OI-PRIV-01` to `OI-PRIV-20`) and a GitHub issue. Closely related findings share one item.

| Open item | GitHub | Findings | Title |
|---|---|---|---|
| `OI-PRIV-21` | #949 | H1 | The SHDR session-end record carries duration, modalities and abort reason; per-second SHDR records encode duration |
| `OI-PRIV-22` | #950 | H2 | A photoparoxysmal halt is written to SHDR conditionally and rewritten every second |
| `OI-PRIV-23` | #951 | H3 | The cervical VNS SHDR summary carries raw impedance and stimulation parameters; `OI-CVNS-04` as worded would ship it |
| `OI-PRIV-24` | #952 | H4 | Resume restarts stimulation over the unencrypted pause characteristic (extends `OI-SIMFW-73`) |
| `OI-PRIV-25` | #953 | H5 | The safety MCU's cardiac-cutoff log is plaintext, ordered, and survives factory reset |
| `OI-PRIV-26` | #954 | H6 | The EDF+ research gate passes exact times, the recording ID and a code that stays the same across extracts |
| `OI-PRIV-27` | #955 | H7 | Erasure is a plain eMMC erase, not SANITIZE; the spec names the wrong command; no reset is reachable |
| `OI-PRIV-28` | #956 | M1 | Cervical VNS lifecycle events go to SHDR with no §5.1 ruling |
| `OI-PRIV-29` | #957 | M2 | The active-user tag is a profile-UUID prefix, the same on every helmet, and any encrypted central can name any tag |
| `OI-PRIV-30` | #958 | M3 | The HRV coherence trend and the resonance breathing rate are designated SHDR; `OI-HRV-04` as worded would write them |
| `OI-PRIV-31` | #959 | M4 | A signed protocol can be replayed on the same device; the serial check fails open |
| `OI-PRIV-32` | #960 | M5, M6 | No anti-rollback; USB DFU is built, unencrypted and missing from the §2.5 table |
| `OI-PRIV-33` | #961 | M7 | The warranty token goes to any paired central, not only the warranty owner's |
| `OI-PRIV-34` | #962 | M8 | One UHDR key per device (inferred) |
| `OI-PRIV-35` | #963 | M9, M10 | A 32-byte XTS key; no unlock attempt limit; Argon2id memory against the RAM |
| `OI-PRIV-36` | #964 | M11 | The accelerometer characterisation budget fails open |
| `OI-PRIV-37` | #965 | M12 | No stealth mode in firmware; the fault LED lights on a user stop |
| `OI-PRIV-38` | #966 | M13 | Map 3 dose per session, EDF-gate faults and accessory insertions planned for Config or SHDR |
| `OI-PRIV-39` | #967 | L1 | Plaintext stays in hub RAM after a session and after the key is dropped |
| `OI-PRIV-40` | #968 | L2 | `check-redaction-shape.ts` misses every SHDR leak found here |
| `OI-PRIV-41` | #969 | L3–L6 | Minor items: shared counter, CARDIAC labelling, classification comments, vendor UUID, refusal codes, link flag, NVSTATE code |

`OI-CVNS-04` (`NP-FW-CVNS-001`) and `OI-HRV-04` (`NP-FW-HRV-001`) carry a note pointing at `OI-PRIV-23` and `OI-PRIV-30`, so that neither is closed by wiring the write it describes.

---

## What looks good

- **The fault-latch report has a fixed shape**, and `np_log_shdr_fault()` discards `session_ms` for every caller, unconditionally. The 2026-08-12 lesson is applied where the gate looks.
- **The socket-lattice PBM health record follows the rulings:** idle only, whole lattice, no session count, and presence that does not depend on the modality.
- **Encryption on the GATT table fails closed.** The link starts unencrypted and a disconnect clears it, the server re-checks the stack, and a host test forces every row to be ENC or a named exception.
- **The UHDR key design is serious:** a hardware-bound WKMD, salt and KDF parameters authenticated as GCM AAD, enforced KDF floors, and the UKMD scrubbed on every failure path.
- **The warranty token comes from the TRNG**, is never regenerated over an unreadable value, and the target stub traps rather than returning a fake.
- **The MCU UID never leaves the MCU.** Heart rate is RAM-only on the MCU. The charge accumulators are RAM-only. Adaptation events and HRV session records go to UHDR only.
- **Factory reset has a durable, read-back marker and fails closed.** The anonymisation orchestrator always tears down. The simulator labels its UHDR as plaintext.

## What could not be reviewed

- **Everything behind a platform trap:** the BLE stack (advertising, RPA, bonding and IRK storage, pairing method), the USB CDC driver, the eMMC XTS mounts and SANITIZE, the TRNG, the LED HAL, and how the protocol key and serial are provisioned. Share these with the platform layer (#447).
- **The SHDR uploader and fleet ingest**, so which of the SHDR records above actually leave the device, and whether the app strips fields the schema lacks.
- **Production programming:** STM32 RDP and option bytes, and i.MX JTAG/SWD lock and secure boot fuses.
- **The app-side anonymisation steps** (k ≥ 10, small-group suppression, week rounding). The firmware has only the callbacks.
- **Whether the active-user tag can be used to sidestep a cardiac withhold** (M2). That needs a safety review, not a privacy one.

## Recommended next steps

1. **Stop the SHDR biology leaks at the source (about 1–2 days, FW).** Shrink the session-end record, delete the EEG tick, the zone-slot PBM and the ppx-halt SHDR writes, and remove the lifecycle codes that have no ruling (H1, H2, M1). Then put every SHDR-writing function on the redaction-shape gate's list (L2). Get the missing §5.1 rulings from the principal in the same change.
2. **Re-word OI-CVNS-04 and OI-HRV-04 before anyone "closes" them (about 1 hour, docs).** Retire or reshape `np_cvns_shdr_summary_t`, re-scope the HRV trend to UHDR, and correct `np_fw_cvns_001.md` §4.6 (H3, M3).
3. **Split resume off the unencrypted pause characteristic (about half a day, FW plus apps; a safety review).** Correct the OI-SIMFW-73 justification (H4).
4. **Open the ISO 14971 / principal decision on erasing the safety-MCU cardiac log, and fix erasure itself (a decision, then 1–3 days).** That covers the authenticated NV purge, SANITIZE through EXT_CSD instead of CMD38 in code and spec, purging Config, a reachable reset command, and reopening OI-RESET-01 (H5, H7).
5. **Fix the EDF+ gate and spec §E.2/§E.4 (about 1 day)** (H6). Hand the crypto items (M9 XTS key length, M10 unlock limit, M4 replay, M5 anti-rollback) to the OI-EMMC2-02 cryptography reviewer as one package.

The remaining items (M2, M6–M8, M11–M13 and the Low items) are tracked in the open items above. Re-run this review on `firmware/` once steps 1–4 land, so the next priorities are set against the reduced surface.
