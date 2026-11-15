# Health limits on session length and cumulative dose — survey by modality

**Status:** Process record, not a controlled document (no serial, no revision, not in `NP-DHF-001`). **Written:** 2026-11-11, for GitHub #563 (raised from `OI-UI-KMP-09`).
**§1 to §7 decide nothing and set no limit; §8 records the principal's decisions of 2026-11-13 and what the research read for them says.** Every figure below is quoted from the document or source file named beside it. Where this file and an owning document disagree, the owning document wins and this file is the defect.

**Why it is here and not in a controlled document.** The table crosses sixteen modalities whose limits are owned by a dozen controlled documents (`NP-DT-001`, `NP-FW-PBM1064-001`, `NP-HW-AUDIO-001`, `NP-FW-CVNS-001` and others). Putting the figures in one more controlled document would make a second copy of each. A process record beside `docs/reference/safety-zones.md` (the zone model, which is the only built part of `SWR-SS-02`) can index them, say what is missing, and be corrected freely. `NP-SW-001` §6.3.6 `SWR-SS-02` points here.

## 1. The rule this survey serves

The principal's direction (2026-10-08, `OI-UI-KMP-09`; `NP-SW-001` §6.3.6):

- A session of any finite length is admissible (`SWR-SS-01`).
- Nothing beyond a **health** safety limit runs without the user's explicit acknowledgement (`SWR-SS-02`).
- Nothing beyond a **hardware or system** limit runs at all (`SWR-SS-03`).

`SWR-SS-02` needs, for every modality, a health limit on session duration or cumulative dose to compare a session against. This file records, per modality, whether the repository states one, where it comes from, and what enforces it.

**How a row was filled.** A limit is listed only where a document or source file states it. "None found" means the search below found none; it does not mean none is needed. A source is marked **VERIFIED** only where the cited text was read on 2026-11-11 (URL given in §6) or where the owning document records its own check; everything else is **UNVERIFIED**. No figure here comes from memory.

**Searched:** `CLAUDE.md` §3 and §4.2; `docs/reference/modality-stack.md`; `docs/reference/safety-zones.md`; `docs/np_dt_001.md`; `docs/np_sw_001.md` §6.3; `docs/np_npps_ref_001.md` §4 and §7; the six `docs/np_hw_*_001.md` accessory specifications; `docs/np_fw_cvns_001.md`, `docs/np_reg_cvns_001.md`, `docs/np_fw_hd_001.md`, `docs/np_hw_tcap_001.md`, `docs/np_fw_pbm1064_001.md`, `docs/np_ses_1064_001.md`, `docs/np_bib_pbmirr_001.md`, `docs/np_fw_emmc_002.md` §F, `docs/np_pwrsrc_001.md` §5, `docs/np_rm_001.md`; `common/npps/constants.json`; `common/npps-core/src/validate.rs`, `zones.rs`, `compiler.rs`; `firmware/safety_mcu/`, `firmware/hub_control/`, `firmware/pbm/`, `firmware/cervical_vns/`, `firmware/hrv_biofeedback/`, `firmware/sloreta_hdtdcs/`. **Not read:** the `.docx` documents, `docs/np_priv_001.pdf`, the protocol databases (`.csv`, `.tsv`), and the citations inside `protocols/predefined/*.npps`.

## 2. Findings that cut across modalities

1. **No per-day limit is enforced anywhere, for any modality.** No firmware or app code counts sessions, pulses or dose across sessions. The safety MCU's DC charge accumulator is zeroed when a session starts (`np_charge_monitor_reset_session()`, `firmware/safety_mcu/src/np_charge_monitor.c`, called on the `session_active` 0→1 transition; `NP-FMEA-001` FMEA-M03-04), so the 150 mC/cm² ceiling bounds one session and says nothing about the next one. The PBM dose accumulator is per session too.
2. **A per-day limit has no clock to stand on.** The headset has no RTC backup (`CLAUDE.md` §2.3, §4.5), so the device cannot know that a day has passed across a power cycle. The record of what a person received today is session history, which is UHDR and is held by the app (`CLAUDE.md` §5.1). Where a daily limit would be kept, and by which part, is a design decision that has not been made (`OI-HLIM-01`).
3. **The `limits` block has fields for these limits, ships empty, and the validator ignores four of them.** `NP-NPPS-REF-001` §7 defines owner-set limits at three levels (global, helmet, individual), with fields for session dose, daily dose, session duration, sessions per day, pulses per session and per day, and sessions per week. No limits set ships in `protocols/predefined/`, so nothing is in force until an owner authors one. The validator enforces `maxSessionDurationSeconds` (against one on-interval, not the session: `validate.rs` `session_duration`), `maxSessionDoseJCm2` and `maxPulsesPerSession`; it parses and never checks `maxSessionsPerDay`, `maxDailyDoseJCm2`, `maxPulsesPerDay` and `maxSessionsPerWeek`. The figures in the §7 example (`T1 Home Defaults`) are an illustration with no source and are not limits (`UC-172`, `OI-HLIM-16`).
4. **The acknowledgement path exists for electrical dose, and (since 2026-11-13) for session length.** The zone model (`docs/reference/safety-zones.md`, `OI-ZONE-01`) is the built part of `SWR-SS-02`: the shared compiler refuses a caution-zone protocol unless the request acknowledges it. Its electrical axes are per-phase charge density, Shannon `k`, RMS and mean current density, and DC session charge density; every caution boundary is a placeholder (`UC-073` to `UC-077`). **Added by #569: a session-length axis for every modality but audio (§8).** There is still no axis for a daily total, or for any optical, acoustic or magnetic dose.
5. **Two general duration advisories exist and are not health limits.** The validator warns on a session under 60 s or over 120 min (`SESSION_SHORT_WARNING_SECONDS`, `SESSION_LONG_WARNING_SECONDS`, `validate.rs`), for every modality alike; the figures have no derivation (`UC-067`) and a warning asks for no acknowledgement.
6. **Whether a given limit is a health limit or a hardware limit is not recorded for most of them**, and it decides which of `SWR-SS-02` and `SWR-SS-03` applies. That is a clinical judgement and is asked in `docs/human-input/oi-hlim-02-health-or-hardware-limit.md`.

## 3. The table

Per session and per day are separate columns because the repository treats them differently: several per-session limits exist, and no per-day limit does.

| # | Modality | Health limit per session (duration or cumulative dose) | Per day | Source | Enforced where | Status |
|---|---|---|---|---|---|---|
| 1 | **PBM transcranial** (660–670, 808–830 nm; 1064 nm module) | **60 J/cm²** at 660 nm, **60 J/cm²** at 808 nm, **36 J/cm²** at 1064 nm, per socket per wavelength (`NP-FW-PBM1064-001` §6.5; `NP-SES-1064-001` §7; `NP-HW-HEXTILE-001` §2 R-7). No session-duration limit | None found | **None recorded.** No derivation of the 60 was found; the 36 is labelled "conservative" and waits on a regulatory opinion (`OI-SES-01`, `UC-041`) | **Hub, Class B:** `np_pbm_dose_tick()` (`firmware/pbm/src/np_pbm_dose.c`) disables the channel when the metered dose reaches `NP_PBM_DOSE_LIMIT_*_J_CM2` (`firmware/pbm/include/np_pbm_config.h`); the session carries on with the other channels. Not the safety MCU. **Not checked before signing**: the compiler checks a session dose only when an owner's limits set gives one (`validate.rs`, `maxSessionDoseJCm2`) | **Placeholder figure; class unclear.** A channel cut in mid-session is neither an acknowledgement nor a refusal before signing. `OI-HLIM-04`. Cumulative thermal dose is tracked separately (`OI-PWRSRC-01`, `OI-PWRSRC-02`, `UC-020`) |
| 2 | **PBM intranasal** | None found. The driver records a dose proxy with a placeholder constant and compares it with nothing (`np_mod_intranasal.c`) | None found | None | **Not enforced.** The driver has a 25 % duty ceiling (`REQ-NASAL-11`) and no irradiance, thermal, dose or duration limit (`NP-HW-NASAL-001` §5.2) | **No limit.** `OI-NASAL-02` (blocking) covers irradiance and temperature; dose and duration are `OI-HLIM-05` |
| 3 | **EEG neurofeedback** (8-ch semi-dry) | None found. No stimulation is delivered; no wear-time, contact-pressure or skin-contact duration limit was found | None found | None | Not enforced | **No limit found; whether one is needed is a clinical question.** `OI-HLIM-06` |
| 4 | **BES / tACS** | None on duration or session total. The limit that exists is per phase, **40 µC/cm²** (`NP-DT-001` DI-SAFE-01a), and by its own definition is not integrated over a session. App-side caution boundaries on RMS and mean current density are placeholders (`UC-075`, `UC-076`) | None found | Per-phase figure: Shannon 1992 and McCreery 1990, as cited by `NP-DT-001` §3.2.1. **UNVERIFIED**: the 40 was not confirmed against either paper (§6) | Per phase: **safety MCU** (`NP_CHARGE_PHASE_LIMIT_UC_CM2`, `np_charge_monitor.c`). Zone cautions: **app** (shared compiler, `zones.rs`). Duration and daily: not enforced | **No duration or cumulative limit.** `OI-HLIM-07`; `OI-ZONE-01`; pad area `OI-CHARGE-07` |
| 5 | **tDCS** | **150 mC/cm² per session** per electrode (`CLAUDE.md` §3; `NP-DT-001` DI-SAFE-01), PROVISIONAL. App-side caution at 100 mC/cm² (placeholder, `UC-077`). No separate duration limit: at a given current and pad the charge ceiling fixes the longest session | None enforced. `NP-RM-001` §6.3 names a "3-session tDCS daily limit" as something a user might circumvent; no requirement, constant or code states it (`UC-171`) | `NP-DT-001` §3.2.1: the envelope of `docs/tdcs_database_full.csv` (Brunoni ELECT-TDCS, 144 mC/cm²) and Liebetanz et al. 2009 (52,400 C/m²). **Liebetanz figure VERIFIED as a number** from a commentary on the paper, which calls it an *extrapolated zero-lesion intercept* for *cathodal* stimulation in rats, where `NP-DT-001` calls it a "measured lesion threshold" (§6). Brunoni: UNVERIFIED | Per session: **safety MCU** (`NP_CHARGE_DC_LIMIT_MC_CM2`, hard cutoff, no acknowledgement). Caution: **app**. Pre-signing check: **app** (`TDCS_MAX_SESSION_CHARGE_DENSITY_MC_CM2`). Daily: not enforced | **Per session: provisional, enforced** (`OI-CHARGE-06`, `docs/human-input/oi-charge-06-dc-charge-ceiling.md`). **Per day: no limit.** `OI-HLIM-08` |
| 6 | **VNS auricular + HRV biofeedback** | None on duration or session total. Per phase **40 µC/cm²**; **≤ 40 mA** stop-gap, stated not to be a sensory, heating or cardiac limit (`CLAUDE.md` §3). HRV coherence training is described as 5–20 min and `NP_HRV_SESSION_MIN_S` / `_MAX_S` (300, 1200) exist, but no code reads them (`UC-173`) | None found | Per phase: as row 4, UNVERIFIED. 40 mA: internal arithmetic on a provisional pad area (`UC-079`) | Per phase and 40 mA: **safety MCU** and hub. Zone cautions: **app**. Duration and daily: not enforced | **No duration or cumulative limit.** `OI-HLIM-10`; `OI-VNSCLIP-09`; `OI-ZONE-01` |
| 7 | **Audio** (planar; bone conduction) | **None, by decision.** The ceiling is a level, **85 dB SPL** at the ear, "no exposure-duration model and no daily-dose figure is set or claimed" (`NP-HW-AUDIO-001` §5.1, `REQ-AUDIO-19`; principal, 2026-11-09, `OI-AUDIOHW-01`). Bone path: the same hearing level as a force level, figure not yet computed (`REQ-AUDIO-21`, `OI-AUDIOHW-02`, `OI-AUDIOHW-13`) | None, by the same decision: the device cannot know exposure outside a session | NIOSH 98-126 and Directive 2003/10/EC, recorded as VERIFIED by the owning document (`OI-AUDIOHW-12`, narrowed, still open for the paywalled texts). Not re-checked here | Level: **app** (compiler refuses above 85, `compiler.rs`) and **hub** (`NP_AUDIO_MAX_LEVEL_HALF_DB`, `np_mod_audio.c`). Calibration at the ear is not measured (`OI-AUDIOHW-11`). Bone: no gating, the HAL is a stub | **Decided: level only.** `SWR-SS-02` has no duration figure to compare for audio; what it does then is `OI-HLIM-01` |
| 8 | **Visual stimulation** (photic, EMDR, retinal PBM mode) | None stated as a duration or dose. The stated control is "IEC 62471 MPE at 50 % of exempt-group threshold" (`CLAUDE.md` §3; `NP-DT-001` DI-SAFE-05). No document found states the exposure time that figure is evaluated for, or a session or daily exposure | None found | IEC 62471. **UNVERIFIED** (paywalled; not read here, and no document found records a reading) | Hardware current limit, IR proximity and Hall sensor (`CLAUDE.md` §4.2). The hub's `np_mod_visual_hal_mpe_check()` is an unbuilt HAL stub with no stated criterion (`np_platform_stub.c`). Photic driving at 3–30 Hz is refused outright (`REQ-FWHUB-65`), which is a frequency control, not a duration one | **No duration or cumulative limit stated.** `OI-HLIM-11`; `DI-REG-03` |
| 9 | **Visual, Mode F** (NIR retinal walk) | Not defined. The design calls for a limit on cumulative daily retinal dose, not a session limit (`NP-FW-EMMC-002` §F.1 item 5) | **Placeholder: `NP_MODE_F_DAILY_DOSE_LIMIT_J_CM2` = 0.0, "always blocked until cleared"** (`NP-FW-EMMC-002` §F.2, `UC-174`). The counter is specified to reset at midnight UTC, which needs a clock the headset does not keep (finding 2) | To be set by the RISK-03 opinion, question Q-13 (`DI-REG-03`; `docs/status/pending-decisions.md`, "Mode F regulatory opinion (Q-13)") | Mode F cannot run: `NP_MODE_F_REGULATORY_CLEARED` = 0 (`firmware/hub_control/include/np_hub_config.h`). The daily counter is not built | **Blocked; limit is a placeholder.** `DI-REG-03`, Q-13; `OI-HLIM-12` |
| 10 | **qEEG 21-ch wet gel** (T2) | None found. The validator says so: "no dosage limits are defined" (`validate.rs`, `qeeg_21ch`) | None found | None | Not enforced; the hub driver is a stub | **No limit found; as row 3.** `OI-HLIM-06` |
| 11 | **TMS** (T2) | None. "No dose or train limit, no inter-train interval floor, no cumulative-pulse bound, no contraindication interlock" (`NP-HW-TMS-001` §6.2). The acoustic output of the pulse has no limit either (`OI-TMS-02`) | None | None named. `NP-HW-TMS-001` says naming the binding guidance is a Clinical + Regulatory decision | Not enforced; the hub driver is a stub. The validator warns above 120 % of motor threshold (`UC-067`) and checks `maxPulsesPerSession` only if an owner sets it | **No limit.** `OI-TMS-03`, `OI-TMS-02` (no new item) |
| 12 | **1170 nm deep PBM** (T2) | **60 J/cm² per session**, "conservative first-revision limit" (`NP_T2_1170_DOSE_LIMIT_J_CM2`; `NP-SES-1064-001` §7) | None found | **None recorded** (`UC-170`) | **Not enforced in the code found:** the constant is defined in `firmware/pbm/include/np_pbm_config.h` and no source file reads it, although `NP-SES-1064-001` §7 says "laser disable on limit". The hub driver is a stub | **Placeholder figure, not enforced.** `OI-HLIM-13` |
| 13 | **Clinical tACS** (T2, ≤ 4 mA) | None on duration or session total; per phase **40 µC/cm²** as row 4 | None found | As row 4, UNVERIFIED | Per phase: **safety MCU** (CLIN_STIM channel). The hub driver is a stub | **No duration or cumulative limit.** `OI-HLIM-07` |
| 14 | **HD-tDCS** (T2) | **150 mC/cm² per session** per electrode, as row 5, against the declared 0.0962 cm² electrode. On that area the ceiling is reached in seconds, so the figure and the electrode are not consistent (`docs/human-input/oi-charge-06-dc-charge-ceiling.md` §2; `OI-TCAP-01`) | None found | As row 5 | **Safety MCU** (CLIN_STIM channel, geometry-gated). The hub driver is a stub | **Unresolved**: the per-session limit cannot be met by a conventional session on the declared electrode. `OI-CHARGE-06` (Q2), `OI-TCAP-01`; `OI-HLIM-09` |
| 15 | **Cervical VNS** (T2) | **Retired 2026-11-11 (`REQ-CVNS-04` Rev 5, the principal): the 60–120 s bound is no longer a limit and `NP_CVNS_SESSION_*` are removed.** The row below is as written on 2026-11-11 and is out of date; §8 holds the current position. Was: **60–120 s per session** (`NP_CVNS_SESSION_MIN_S`, `NP_CVNS_SESSION_MAX_S`; `REQ-CVNS-04`). Stated as the predicate's protocol, an equivalence argument (`NP-REG-CVNS-001` §3.2); no document says it is a health limit | None found. Nothing counts sessions, and a second session can start once the first ends | gammaCore, K163334 (`NP-REG-CVNS-001` §3). **UNVERIFIED**: the manufacturer's instructions could not be fetched (§6) | **Hub, Class B:** `np_cvns_stim.c` refuses a duration outside 60–120 s, and `np_mod_cvns.c` sets the library's timer to 120 s. Not the safety MCU, which owns the cardiac interlock | **Per session: stated, class unclear. Per day: no limit.** `OI-HLIM-14` |
| 16 | **Vibrotactile 40 Hz** (provisional accessory) | None found. Only an acceleration range, 0.6–1.2 G (`VIBROTACTILE_MIN_G`, `VIBROTACTILE_MAX_G`) | None found | None | Not enforced; the hub driver is a stub and no hardware specification exists | **No limit.** `OI-HLIM-15` |

## 4. Hardware and system limits, listed only to keep them apart

These bound the equipment or an instantaneous quantity. They are the `SWR-SS-03` side of the rule and are not health limits on duration or dose, with the caveat of finding 6: for some of them the class is itself unsettled.

| Limit | Applies to | Enforced by | Note |
|---|---|---|---|
| Time-averaged weighted irradiance sum ≤ 1; 400 mW/cm² peak | PBM scalp | Hub; the pre-signing check is not built (`OI-HEXTILE-30`, `OI-HEXTILE-31`) | The 400 peak is also a hazard control at 1064 nm (`CLAUDE.md` §3, `OI-BIBPBM-03`) |
| 42 °C applied-part limit; 62 °C junction throttle, 65 °C cutoff | PBM scalp, all tiles | Safety MCU (NTC) | Caps temperature and has no duration input (`NP-PWRSRC-001` §5.1) |
| ≤ 1,000 mW/cm²; TEC 45 °C throttle, 50 °C cutoff | 1170 nm | `firmware/pbm` (T2 combined) | |
| 25 % duty ceiling | Intranasal probe | Hub driver | Bounds duty, not irradiance (`NP-HW-NASAL-001` §5.2) |
| ≤ 1 mA (T1), ≤ 4 mA (T2); 0.5–40 Hz | BES / tACS, clinical tACS | Hub, app | |
| 0.1–2 mA; 30 s ramp; ≤ 3 electrode pairs | tDCS, HD-tDCS | Safety MCU (ramp), hub, app | |
| 6 A/m² electrode current density | HD-tDCS | `firmware/sloreta_hdtdcs` | No source (`UC-058`, `OI-TCAP-01`) |
| ≤ 2 mA; 1–25 Hz; 200–1000 µs | Cervical VNS | Hub library | Predicate equivalence (`NP-REG-CVNS-001` §3) |
| Contact impedance windows | Every electrical channel | Safety MCU | `UC-059` |
| Cardiac interlock (`REQ-CVNS-09`) | Cervical VNS | Safety MCU | A health interlock on an event, not a dose |
| Coil protection: EMF cancellation gating; adiabatic pulse budget (516 pulses, `NP-HW-TMS-001` §5.1) | TMS | Safety MCU (gating) | A thermal bound on the coil, "not to be reused as the clinical control" (`OI-TMS-03`) |
| Peak power 45–50 W (T1), 70–74 W (T2) | System | USB-C PD | `CLAUDE.md` §4.5 |

## 5. What this leaves `SWR-SS-02` able to do

- **Built today:** the acknowledgement for electrical dose in the caution zone (rows 4, 5, 6, 13, 14, through `zones.rs`), on placeholder boundaries.
- **A per-session figure exists but nothing compares a protocol with it before signing:** PBM transcranial (row 1), 1170 nm (row 12), cervical VNS (row 15). Each is enforced, where it is enforced at all, by the hub stopping the output during the session.
- **No figure to compare against:** PBM intranasal, EEG and qEEG, BES / tACS and clinical tACS duration, auricular VNS duration, audio duration (by decision), visual duration, TMS, vibrotactile; and every per-day quantity for every modality.

What the rule does for a modality in the third group is not decided, and it is the principal's decision: `OI-HLIM-01`, `docs/human-input/oi-hlim-01-rule-where-no-limit-exists.md`. Nothing in this repository may act as if it had been answered.

## 6. Citations checked on 2026-11-11

| Claim in the repository | What was read | Result |
|---|---|---|
| Liebetanz et al. 2009: rat epicranial DC "lesion threshold" of 52,400 C/m² (`NP-DT-001` §3.2.1) | Bikson, Datta & Elwassif, "Establishing safety limits for transcranial direct current stimulation", *Clin Neurophysiol* 120(6):1033–1034, 2009, a commentary on the paper: <https://pmc.ncbi.nlm.nih.gov/articles/PMC2754807>. The paper itself was not read | **Number confirmed; description differs.** The commentary gives 52,400 C/m² as "an extrapolated zero lesion size intercept", for **cathodal** stimulation, with lesions seen from 142.9 A/m² for more than 10 min, and says the result does not necessarily apply to anodal stimulation. `NP-DT-001` calls it a measured threshold and does not state the polarity. `OI-HLIM-03` |
| Shannon 1992 and McCreery et al. 1990 support **40 µC/cm² per phase** (`NP-DT-001` DI-SAFE-01a) | Search results only (bibliographic record <https://scilit.com/publications/dc18fbdf39e286be3c9897bbd1c8f782> and secondary summaries). Neither paper was read | **Not confirmed.** The paper and its subject are real (IEEE Trans Biomed Eng 39(4):424–426). No source read states 40 µC/cm² per phase; the summaries describe a line in charge and charge density with a parameter `k`. `OI-HLIM-03` |
| Cervical VNS session of 120 s follows gammaCore (`NP-REG-CVNS-001` §3) | The manufacturer's instructions for use (<https://gammacore.com/wp-content/themes/gammacore-p2/pdf/gammacore_IFU.pdf>) could not be fetched: the host is outside this session's network allowlist. A search summary only | **UNVERIFIED.** The search summary reports a two-minute stimulation and a stated maximum number of stimulations per day in the instructions. That would be a candidate source for a per-day figure; it is recorded in `OI-HLIM-14` as something to read, not as a limit |
| Published rTMS safety practice (`NP-HW-TMS-001` §6.2, no citation) | Search results only. Rossi et al., "Safety and recommendations for TMS use in healthy subjects and patient populations, with updates on training, ethical and regulatory issues: expert guidelines", *Clin Neurophysiol* 132(1):269–306, 2021, DOI 10.1016/j.clinph.2020.10.003, and the 2009 guidelines it updates. Not read | **A place to look, nothing more.** No figure is taken from it. `OI-TMS-03` |
| NIOSH 98-126, Directive 2003/10/EC (audio, 85 dB) | Not re-checked | Recorded as verified by `NP-HW-AUDIO-001` §5.1 (`OI-AUDIOHW-12`) |
| IEC 62471 exempt group; IEC 60601-1 | Not read (paywalled) | UNVERIFIED |

## 7. Open items raised by this survey

All are in `docs/status/pending-decisions.md` §13.4, family `OI-HLIM`. Register rows: `UC-170` to `UC-174` in `docs/status/unjustified-choices.md`.

| Item | Row | What is open |
|---|---|---|
| `OI-HLIM-01` | all | What `SWR-SS-02` does where no health limit is defined, and where a per-day total is kept. The principal's decision |
| `OI-HLIM-02` | all | Which existing limits are health limits and which are hardware limits; which modalities need a duration or daily limit. Clinical |
| `OI-HLIM-03` | 4, 5, 6, 13, 14 | Check the charge-ceiling citations against their sources |
| `OI-HLIM-04` | 1 | PBM transcranial: basis of the per-session dose figures, how they are applied, and a per-day figure |
| `OI-HLIM-05` | 2 | PBM intranasal: no dose or duration limit |
| `OI-HLIM-06` | 3, 10 | EEG and qEEG: no wear or contact duration limit found |
| `OI-HLIM-07` | 4, 13 | BES / tACS and clinical tACS: no session-duration or cumulative limit |
| `OI-HLIM-08` | 5 | tDCS: no per-day limit; the "3-session" statement has no source |
| `OI-HLIM-09` | 14 | HD-tDCS: no usable per-session limit on the declared electrode |
| `OI-HLIM-10` | 6 | Auricular VNS and HRV biofeedback: no session-duration or cumulative limit |
| `OI-HLIM-11` | 8 | Visual: no stated exposure time or cumulative exposure behind the IEC 62471 figure |
| `OI-HLIM-12` | 9 | Mode F: the daily dose limit is a placeholder and its reset needs a clock |
| `OI-HLIM-13` | 12 | 1170 nm: the per-session dose figure has no source and nothing enforces it |
| `OI-HLIM-14` | 15 | Cervical VNS: the class of the 120 s bound, and no per-day limit |
| `OI-HLIM-15` | 16 | Vibrotactile: no limit of any kind |
| `OI-HLIM-16` | all | The `limits` block: four fields parsed and never checked; unsourced example figures |

Already tracked, linked and not duplicated: `OI-AUDIOHW-01`, `OI-AUDIOHW-02` (decided), `OI-AUDIOHW-11`, `OI-AUDIOHW-12`, `OI-AUDIOHW-13`; `DI-REG-03` and Q-13; `OI-CHARGE-06`, `OI-CHARGE-07`; `OI-TCAP-01`; `OI-TMS-02`, `OI-TMS-03`; `OI-NASAL-02`; `OI-SES-01`; `OI-PWRSRC-01`, `OI-PWRSRC-02`; `OI-VNSCLIP-09`; `OI-ZONE-01`; `OI-BIBPBM-03`; `OI-HEXTILE-30`, `OI-HEXTILE-31`.

## 8. Decisions and research, 2026-11-13 (GitHub #563 and #569)

Added after the principal answered `docs/human-input/oi-hlim-01-rule-where-no-limit-exists.md` and directed that Table A come from the documents and Table B and Q-C from the research. **Status of the research:** read this session from the sources named, by agents that quoted the text; a source that could not be reached is said so and nothing is taken from it. This section is a process record, and where an owning document disagrees with it the owning document wins.

### 8.1 The principal's decisions (`OI-HLIM-01`, closed)

| Q | Answer | Consequence |
|---|---|---|
| Q1 | **B**: a general long-session acknowledgement | Above one stated length, any modality but audio asks the user to acknowledge, and the message says no health limit is defined. The level is the existing 120-minute advisory (`UC-067`), a choice with no health basis, and is never presented as a limit. Built in `common/npps-core` (the `sessionDuration` zone axis) |
| Q2 | **A**: audio is outside `SWR-SS-02` | Its 85 dB SPL level ceiling is the whole control. A session of audio alone has no session-length axis |
| Q3 | **A**: a per-day total, if ever adopted, is kept in the app from session history | None is adopted (§8.3), so nothing is built. The total would be UHDR and would not hold in Mode 3 |

### 8.2 Table A, from the documents (`OI-HLIM-02`): which existing limit is which

"Hardware" here is the repository's own reading of a limit its documents already call hardware-enforced and not overridable; no document classes any limit as a health limit, and no clinical reviewer has been named. The clinical question stays open for a reviewer to confirm.

| # | Limit | What the documents say | Class on the documents | Research (§8.3) |
|---|---|---|---|---|
| A1 | PBM transcranial dose, 60 / 60 / 36 J/cm² | `NP-FW-PBM1064-001` §6.5, `NP-SES-1064-001` §7: "channel disable on limit". The 36 is "conservative", waiting on `OI-SES-01`. No class stated | **Not classed.** A device-enforced placeholder (`UC-041`) | No research-established session dose limit; not removed (`CLAUDE.md` §18) |
| A2 | 1170 nm dose, 60 J/cm² | `NP-SES-1064-001` §7 "laser disable on limit"; no source reads the constant (`UC-170`) | **Not classed**, not enforced | None found |
| A3 | DC 150 mC/cm² per session | `NP-DT-001` DI-SAFE-01: "hardware-enforced by safety MCU … app and main processor cannot override" | **Hardware** (refuse). The app-side caution at 100 mC/cm² is a separate placeholder (`UC-077`) | No charge-density limit in either safety review; the 150 stays provisional (`OI-CHARGE-06`) |
| A4 | 40 µC/cm² per phase | `NP-DT-001` DI-SAFE-01a: "hardware-enforced by the same monitor" | **Hardware** (refuse) | Shannon 1992 and McCreery 1990 not reachable; the figure is in neither review read (`OI-HLIM-03`) |
| A5 | Cervical VNS 60–120 s | `REQ-CVNS-04` Rev 5: the session part retired; the length is the protocol's | **No longer a limit** | The FDA decision summary states no per-day cap and no session bound |
| A6 | Audio 85 dB SPL | `NP-HW-AUDIO-001` `REQ-AUDIO-19`: refused above it; a level, no duration model | **Level ceiling, outside `SWR-SS-02`** (Q2 A) | Not reopened |
| A7 | 42 °C applied part | `CLAUDE.md` §3: an external limit (IEC 60601-1); safety MCU throttles and cuts | **Hardware** (refuse) | Not a duration limit |

### 8.3 Table B and Q-C, from the research

**Rule applied (the principal, 2026-11-13):** where the research states a limit, one is added; where it states none, or the matter is unknown, there is none, and the reason is that **no research-established limit** was found. A limit is not made up.

| # | Modality | Per session | Per day | Basis (source, read how) |
|---|---|---|---|---|
| B1 | PBM transcranial | **None established** | **None** | Hamblin 2016, PMC5066074 (full text): a very high dose can damage, no threshold given. Cassano 2022, Photonics 9(8):507 (abstract only): no rise in adverse events up to daily sessions for 12 weeks at low dose. Nothing on 1064 or 1170 nm |
| B2 | PBM intranasal | **None established** | **None** | PMC7138738 (full text): "no serious adverse effects were reported … caution should be used with laser devices due to the risk for macular lesions". The caution belongs to `OI-NASAL-02` (irradiance), not a duration |
| B3 | EEG and qEEG wear | **None established** | **None** | PMC5310208 (full text): pressure-injury risk rises with monitoring days (4 days 20 %, 10 days 60 %; 92 % hyperaemia only), with no threshold; PMC10008537, PMC10033183 (full text): daily skin checks and scalp rest, qualitative. A "2–3 days" figure is a search summary of Alvarez and Rossetti 2015 (not read) and is not cited. Basis for an IFU caution, not a limit |
| B4 | BES / tACS, clinical tACS | **None established** | **None** | Antal 2017, PMC5985830 (full text): "agreement with regard to the safety … at the intensities and durations tested". Its scope is "<4 mA, up to 60 min duration per day"; see `OI-HLIM-17` |
| B5 | tDCS | **None established** | **None** | Bikson 2016, PMC5007190 (full text): "≤40 min, ≤4 mA, ≤7.2 Coulombs" produced no serious adverse effect over more than 33,200 sessions; it "avoided putative summary-metrics … charge or charge-density"; patients with over 1,000 sessions showed no cumulative effect. **No source states a "3-session daily limit"** (`NP-RM-001` §6.3 corrected) |
| B6 | Auricular VNS, HRV | **None established** | **None** | Kim et al., Sci Rep 2022, PMC9772204 (full text): adverse events rise with stimulation duration with no threshold; "no standardized stimulation parameters" for safety |
| B7 | Visual stimulation | Per-exposure limits only | **None** | ICNIRP 2013 (full text; equations garbled in extraction, none encoded): retinal limits are time-dependent and stop accumulating beyond fixed times, exposures "can be treated as independent", and medical use is outside its scope. Photoparoxysmal risk is a separate frequency control (`REQ-FWHUB-65`) |
| B8 | Mode F | Not applicable | **None established** | As B7; the daily placeholder stays blocked until the RISK-03 opinion (`OI-HLIM-12`) |
| B9 | TMS | **A per-train seizure table exists, not a duration or dose limit** | **None** | Rossi 2009, PMC3260536 (full text): Tables 4 and 5, maximum train duration and inter-train interval by frequency and intensity, motor cortex, seizure and EMG-spread endpoints. Rossi 2021, PMC9094636 (full text): "we have decided not to provide a formal update of the previous safety tables". Pulses per session and sessions per day are named as dosing parameters and capped nowhere. Not encoded: `OI-HLIM-18` |
| B10 | 1170 nm | As B1 | As B1 | No source found |
| B11 | HD-tDCS | **None established** | **None** | Antal 2017: no 4×1 limit; as B5 |
| B12 | Cervical VNS | **None established** | **None** | FDA De Novo DEN150048 decision summary (full text): "multiple treatments", "no cap stated"; "The risks with long-term stimulation … (both frequency of daily use and for how long) are unknown". The manufacturer's instructions for use could not be read; the earlier note about a daily maximum is still unverified. (K163334 is not gammaCore: it is a hemodynamic monitor.) |
| B13 | Vibrotactile 40 Hz | **None established** | **None** | No head or scalp limit found; whole-body limits are for the whole body and were seen only in search summaries |

**Q-C, combination across modalities and sites: no research-established rule was found, so none is stated.** No source read says a dose at one site adds to a dose at another (nasal and scalp light, two electrode montages, TMS with VNS). Rossi 2009 says simultaneous multi-site TMS "would need to be carefully examined". A session is therefore judged as one length, and no dose is summed across modalities or sites. This is an absence of evidence and not evidence that doses do not combine.

### 8.4 What is still open

`OI-HLIM-02` (a clinical reviewer to confirm §8.2); `OI-HLIM-17` and `OI-HLIM-18` (below); `OI-HLIM-03` (the two charge-ceiling citations, still unread at source); and each modality's own item. Not read: Liebetanz 2009 and Shannon 1992 / McCreery 1990 (not reachable), Poreisz 2007, Nitsche 2003, Wassermann 1998, the gammaCore instructions for use and 510(k) summaries, IEC 62471, ANSI Z136.1, ISO 5349 and 2631, and the full text of the 40 Hz vibrotactile papers.
