# Evidence for OI-SW03-01: what, below the app, independently enforces each app and core refusal

**Status:** DRAFT, evidence for the request in `oi-sw03-01-app-safety-class.md`; not sent (no addressee named). Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Software (`OI-SW03-01` in `docs/status/pending-decisions.md`; GitHub #823)
**To:** Quality and Safety (the item's owner), as an attachment to the request above.
**Response needed:** the corrections to the table in section 3 (a row that is wrong, a control that exists and was missed, a gap that is accepted), with the reviewer's name, role and date.
**What this is not:** a classification, and not a verification. It was made by reading source on 2026-11-11. Nothing was run, built or tested. It does not decide whether SW-03 stays Class B.

## 1. Question this table answers

The request asks whether Class B is still right for SW-03 (the companion app and the shared core). Its rationale reads "SW-01 and SW-02 provide independent stimulation safety" (`NP-SW-001` §3.1). That holds for a given refusal only if SW-01 (safety MCU, Class C) or SW-02 (hub firmware, Class B) enforces the same limit on a quantity it receives or measures itself. This file records, per refusal, whether it does. It replaces the preliminary eight-row table in section 3 of the request, which was marked unverified.

## 2. What the safety MCU (SW-01) can see

Its inputs are: an enable-request bitmask, the session hash and Ed25519 signature, **commanded** current per channel, an electrode area and a phase width per channel (both **declared by the hub**, in a frame protected by a checksum and not a signature), R-peak pulses, NTC readings and contact impedance.

It receives no PBM irradiance, duty, frequency or wavelength, no audio level, no visual frequency, no stimulation timing beyond the heartbeat and no session length. A limit on any of those cannot be enforced by it.

Legend for the verdict column:

- **Independent:** SW-01 or SW-02 enforces the same limit on a quantity it receives or measures, so the app's refusal is not the only control.
- **Partial:** something bounds it, but not the same limit, or the bound is not built, or it trusts a value the hub supplies, or it clamps where `CLAUDE.md` §3 says to refuse.
- **App only:** nothing below the app was found.
- **Hardware:** bounded by circuit design, outside SW-01 and SW-02. It does not bear on the software class.

## 3. The table

| # | Refusal in the app or core | Lower-level enforcement found | Verdict |
|---|---|---|---|
| 1 | Session signature: an unsigned protocol is unusable | SW-02 `np_protocol_verify_and_parse` refuses a bad signature (`BAD_SIGNATURE`, `WRONG_DEVICE`). SW-01 `np_session_sig.c` verifies Ed25519 again and withholds `granted_mask` while `SIG_PENDING` | **Independent for integrity and origin.** It does not check that signed content is safe; see row 31 |
| 2 | Tier: a T2 modality on a T1 unit | SW-02 `np_protocol_tier_admit` works from the modality set, not from the app-computed `FLAG_T2_TIER`. SW-01 `np_tier_identity.c` strips T2 enable bits unless its signed OTP record says T2. Not in force until `OI-UPG-08`: every unit is T1 | **Independent**, fail-closed to T1 |
| 3 | Hardware version: a protocol compiled for another layout | SW-02 `np_protocol_hw_version_admit` (version from the MCU's verified record), `np_layout_admit`, `np_layout_check_protocol_addresses` | **Independent** |
| 4 | Charge per phase, 40 µC/cm² (BES/tACS, VNS, cVNS, clinical tACS) | SW-01 `np_charge_monitor.c`: commanded current times phase width divided by area, checked every tick. **Area and phase width are what the hub declares.** `OI-SIMFW-34`: an area raised mid-session moved the budget and the channel was not cut | **Partial.** Independent of the app, not of the hub. Fixed-pad areas are unmeasured constants (`UC-006`, #386) |
| 5 | DC charge, 150 mC/cm² per session (tDCS, HD-tDCS) | SW-01 accumulates current times time per channel against 150 times the area. A tDCS command with no declared area arms the geometry gate and the MCU never grants the channel | **Partial**, same caveat as row 4. The 150 is provisional (`OI-CHARGE-06`) |
| 6 | Commanded current: BES at most 1 mA, tDCS at most 2 mA, auricular VNS at most 40 mA | SW-02 drivers **clamp** (`STIM_MAX_AMP_BES_UA`, `STIM_MAX_AMP_TDCS_UA`, `NP_VNS_MAX_CURRENT_UA`). SW-01 has no current ceiling, only rows 4 and 5. **Update 2026-11-11: the three drivers now refuse (`NP_HUB_ERR_INVALID_ARG`) at the same values, at dispatch and at upload (`NP-FW-HUB-001` `REQ-FWHUB-78`)** | **Partial.** The hub bounds it by clamping, which §3 says a ceiling must not do. The 40 mA is a stop-gap (#554, #559). **Update 2026-11-11: independent at SW-02 only**, as row 7; the clamping objection is gone |
| 7 | tDCS 30 s ramp, at most 3 electrode pairs | SW-02 `STIM_MIN_RAMP_S` is 30 s in `np_mod_stim.c`. No SW-01 check found. *(This reading missed that the driver raised a shorter ramp to 30 s, a clamp. Since 2026-11-11 it refuses one, `REQ-FWHUB-78` (b); the constant is in `np_cmd_limits.h`.)* | **Independent at SW-02 only** |
| 8 | PBM peak irradiance, 400 mW/cm² | No irradiance value reaches SW-01. The drive stage U3 caps peak current in hardware (`REQ-TDRV-01`). The core's per-channel refusal is the channel's full-scale output (403 mW/cm² at 660 and 808 nm, 28 at 1064 nm), not 400 | **Hardware.** The core's bound differs from the §3 figure (`OI-HWFIT-01`) |
| 9 | PBM time-averaged weighted sum, Σ Ēᵢ/(200 × C_A) at most 1 | **Not built** in the core (`OI-HEXTILE-30`, `-31`). U4's average bound is not built. C_A constants are unverified (`OI-BIBPBM-01`) | **App only, and not built there either.** The clearest gap |
| 10 | PBM duty and continuous wave | The validator rejects duty above `PBM_DUTY_CYCLE_MAX_PERCENT`. Separately `compiler.rs duty_reg` silently clamps the register at 0x32 (25 %). SW-02 `np_mod_pbm.c` clamps to `NP_PBM_DUTY_MAX_REG`. The 25 % cap was retired as policy at Rev 59; these clamps are what `OI-HEXTILE-31` says to remove. **Update 2026-11-11: SW-02 now refuses above 25 % (`REQ-FWHUB-78` (e), interim until `OI-HEXTILE-31`); the core's `duty_reg` clamp is unchanged. Second update 2026-11-11: the core refuses above 25 % too (`REQ-NPPS-33` (a)), so a CW block is refused by the validator, the compiler and SW-02** | **Partial.** Both clamps contradict refuse-never-reshape. **Update 2026-11-11: one clamp is left, the core's. Second update: none is left; three independent refusals at the same interim 25 %** |
| 11 | Scalp temperature, 42 °C | SW-01 `np_thermal_interlock.c` cuts `PBM_CRANIAL` at 62 °C junction. Its table does not match its stated divider (`OI-SIMFW-24`) | **Independent** of any parameter, but a cutoff after the fact. A thermal-dose admission gate is not built (#440) |
| 12 | PBM concurrent power | SW-02 `np_pbm_power_gov.c` refuses every load until `OI-FWHUB-09` | **Independent**, by refusing everything |
| 13 | Audio, 85 dB SPL | SW-02 `np_mod_audio.c` refuses `level_half_db` above 85 dB SPL (`REQ-AUDIO-19`). No SW-01 check. No exposure-duration model (`OI-AUDIOHW-02`) | **Independent at SW-02 only**, for level only |
| 14 | Visual: photic driving at 3 to 30 Hz | SW-02 `np_protocol_photic_admit` at load and `np_mod_visual.c` at dispatch (`REQ-FWHUB-65`, `-66`). The detector is not built (`OI-FWHUB-29`). IEC 62471 exposure: IR proximity, Hall sensor and a hardware current limit | **Independent at SW-02**, plus hardware |
| 15 | Zone model: danger and caution on current density | The charge-density boundaries equal rows 4 and 5. The RMS and mean current-density boundaries are advisory per `zones.rs`; no SW-01 or SW-02 equivalent found. All are placeholders (`UC-073` to `UC-077`) | **App only** for RMS and mean density. **Partial** for charge density |
| 16 | Caution acknowledgement | Nothing below the app checks it | **App only**, by design (a consent step) |
| 17 | Session finite and non-zero (`SWR-SS-01`) | The hub runner ends a session at the descriptor's `duration_ms`, which the app writes. The hub parse rejects only an overflow of start plus duration (`np_protocol.c`). No session-length cap in SW-01 or SW-02 except cervical VNS (`NP_CVNS_SESSION_MAX_S`, 120 s). The pause limit defaults to no limit (`np_pause_limit.h`). Searched `firmware/hub_control`, `firmware/safety_mcu` and `firmware/common` | **App only**, except cervical VNS. Most modalities have no defined health limit to compare against (#563) |
| 18 | Command count, socket overlap, composite overlap | SW-02 `CMD_TOO_MANY`, address checks, parameter-length checks | **Independent** (structural) |
| 19 | VNS contact impedance, 500 ohm to 5 kohm | SW-01 `np_impedance_check.c`; SW-02 pre-check | **Independent** of the app |
| 20 | Cervical cardiac interlock | SW-01 `np_cardiac_interlock.c`: R-peak monitor, pre-arm hold, staleness cutoff | **Independent** of the app |
| 21 | Unsupported VNS trigger, burst rules, pulse-width and frequency fit | SW-02 parameter handling only | **App only**, but these refuse unsupported behaviour, not a harm limit |
| 22 | Pause and stop (`SWR-SS-05`, `-06`) | SW-02 runner implements both. The independent stop is pulling the USB-C cable (`SWR-SS-08`), whose safety is open (`RISK-PWRSRC-01`) | **Partial.** The app is the only commanded stop |
| 23 | PBM wavelength: an unmapped wavelength is refused, never moved to the nearest channel | `wavelength.rs`, `resolve_one_channel`. The mapping windows are user-editable inputs (`wavelength_rules`); the shipped default is each §3 band widened by 10 nm a side. The hub receives a channel and a drive register, not a wavelength. Versioning of the rules is #564 | **App only** |
| 24 | TMS (T2): intensity, frequency, pulse count | The core refuses intensity above its 8-bit register but **silently clamps** frequency and pulse count at 65535 (`encode_tms`); the validator checks `maxPulsesPerSession` separately. The hub TMS driver is a stub that returns `NOT_PRESENT` (`np_mod_t2_stubs.c`). SW-01 has the tier gate and an enable line, no dose control. No patient-facing dose control exists (#389, #563) | **App only.** Hardware and driver not built |
| 25 | 1170 nm deep PBM (T2), at most 1000 mW/cm² | The core refuses above 1000; duty clamps as in row 10. The hub driver is a stub. SW-01 has the tier gate and an enable line; its NTC channels are cranial and hub | **App only.** Driver not built |
| 26 | Clinical tACS (T2), at most 4 mA, channel count in range | The core refuses both. Hub driver is a stub. SW-01 charge monitor covers the shared `CLIN_STIM` line using a hub-declared area | **Partial**, as row 4 |
| 27 | HD-tDCS (T2), at most 2 mA | The core refuses above 2 mA. Hub driver is a stub. SW-01 DC charge check with the geometry gate; ring and bilateral montages imply a fixed 3.5 mm electrode area (`OI-CHARGE-02`) | **Partial**, as row 5 |
| 28 | Cervical VNS (T2): current at most 2 mA, frequency at most 25 Hz | The core refuses (`encode_cvns`). SW-02 `np_mod_cvns.c` **refuses**, not clamps, an amplitude above `NP_CVNS_CURRENT_UA_MAX`, a frequency above its maximum and a pulse width outside 200 to 1000 µs, and caps duration at 120 s. SW-01 has the cardiac interlock, the charge check and the tier gate | **Independent.** The one modality with a real hub driver that refuses |
| 29 | Intranasal PBM | The core refuses above the channel's full-scale output. SW-02 `np_mod_intranasal.c` requires an authenticated probe (`SAFETY_REJECTED` otherwise) and clamps duty at 25 % (`INS_DUTY_MAX`); it has no irradiance check. **Update 2026-11-11: it now refuses a duty above 25 % (`REQ-FWHUB-78` (d)).** SW-01 has the enable line only. Scalp ceilings do not apply (`OI-NASAL-02`) | **Partial** (authentication and a duty clamp; irradiance is the drive stage's, outside software) |
| 30 | Vibrotactile accessory: at most 1.2 G | The core refuses outside 0.6 to 1.2 G. No hub driver among `firmware/hub_control/modules/`; no SW-01 enable line for it | **App only.** Hazard class not assessed here |
| 31 | The signing step itself | `HubDescriptorCompiler.compile` runs the core's compile (which holds the refusals above), then `signer.sign` over the unsigned region; nothing is checked after signing. SW-01 and SW-02 verify against an Ed25519 public key in OTP, written at manufacture (provisioning in `NP-FW-OTA-001`, not traced here, including who holds the private key) | **Note on row 1.** The signature proves who signed and that nothing changed. Whatever the signer signs, the helmet accepts. The app is the root of trust for protocol content |

## 4. Reading the table

1. **Where SW-01 is independent, it covers electrical channels, on values the hub declares.** Rows 4, 5, 26 and 27 do not depend on the app but do depend on the hub's declared area and phase width (`OI-SIMFW-34`).
2. **PBM, audio, visual, wavelength and timing limits never reach SW-01.** For PBM the independent control is circuit hardware (row 8), and the average-irradiance limit is unbuilt (row 9).
3. **Rows 7, 13, 14 and 18 rest on SW-02 alone.** SW-02 is Class B, the same class as the app. Whether that counts as "independent" for option A is part of the decision.
4. **App only (the app's refusal is the only software control):** rows 9 (not built anywhere), 15 for RMS and mean density, 16, 17, 21, 23, 24, 25 and 30. Rows 24 and 25 are T2 modalities whose hub drivers are stubs, so there is as yet nothing below the app. Rows 9, 15, 16 and 17 bear on T1 as it stands.
5. **Clamps where §3 says refuse:** SW-02 for BES, tDCS, VNS and PBM duty (rows 6, 10, 29), and the core for PBM duty, TMS frequency and TMS pulse count (rows 10, 24). Cervical VNS (row 28) shows the refusing form on a hub driver. **Update 2026-11-11: the SW-02 clamps in this finding are refusals now (`NP-FW-HUB-001` `REQ-FWHUB-78`). The core's remain, with others found since (`OI-FWHUB-35`).**
6. **The signature does not add independence for content** (row 31). It adds integrity and origin.

## 5. What this does not establish

- **Nothing was run.** Read from source and comments. A comment saying a control exists is not evidence that it works; `OI-CHARGE-01` is the example, where the monitor was wired on one side only.
- **Constants were not checked against limits.** I did not compare each hub clamp's constant with the matching limit in `CLAUDE.md` §3.
- **Row 17 is a bounded search.** I searched `firmware/hub_control`, `firmware/safety_mcu` and `firmware/common` for session-length names and read the runner's and parser's duration handling. A cap under another name, or in a file outside those directories, was not looked for.
- **Row 31's key custody is not traced.** I did not trace who holds the signing private key, whether the OTP key is per unit or a fleet key, or how the app's signing key is held (`OI-UI-KMP-07`).
- **Not covered:** the validator's non-PBM rules one by one, the hex-tile firmware (SW-04) beyond what rows 8 and 9 cite, the bootloader and OTA path, and the Swift and Windows signing paths.

## 6. What we need back

1. Corrections: a row that is wrong, a control that exists and was missed, a gap accepted as residual risk.
2. For each **App only** row, whether Quality and Safety treat the absence of a lower-level control as a gap to close before release or an accepted residual risk.
3. For rows resting on SW-02 alone (7, 13, 14, 18), whether a Class B control counts as independent of a Class B app.
4. Reviewer name, role and date.

An answer takes effect only when it is recorded in `NP-SW-001` §3.1 and logged in `docs/status/completed-decisions.md`. Nothing in the repository acts as if an answer has been given.
