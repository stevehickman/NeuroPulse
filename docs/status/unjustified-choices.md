# Unjustified-choices register

**Status:** Process record. Not a controlled document: no serial, no revision letter, and not in the DHF index (`docs/np_dhf_001.md`).
**First written:** 2026-10-04 · **Rule that keeps it current:** `CLAUDE.md` §19 · **Gate:** `scripts/check-unjustified-register.ts`

> **What this is.** One place that lists every choice in the project that has no derivation, no measurement,
> or no checked source, with a link to where it was decided or discussed, so that any of them can be
> revisited later. It is an **index, not an authority**. Each row points at the owning open item (OI), and
> the OI row in its owning document stays the record of the reasoning. When this file and an owning
> document disagree, the owning document wins and this file is the defect.
>
> **What it is not.** It is not `docs/status/pending-decisions.md` (the log of open items, organised by item and
> authoritative for them). It is not the `NP-CONV-001` §7.1.1 sweep (`OI-CONV-08`, closed, which covered
> requirement rows only). It is not `hardware/np_pbm_model.json`, which tags its own constants but only for the
> PBM power and thermal model. Those four are where this register's rows were found.

## How to read it

Seven columns in every table below.

| Column | Holds |
|---|---|
| **ID** | `UC-nnn`. Never reused and never renumbered. A retired row keeps its ID. |
| **Choice** | The value or assumption, in a few words. The figure is quoted only where the owning document quotes it. |
| **Lives in** | The artifact that carries it: a code path, a document and section, or both. Code is named by file, not line, because lines drift. |
| **Class** | One of the six below. |
| **Why it is open** | The one fact that makes it unjustified. |
| **Decision / discussion** | Where it was decided or argued: the owning OI, the document section, and the GitHub issue or pull request. |
| **Closes when** | What would justify it, or retire it. |

| Class | Means |
|---|---|
| `NO-DERIVATION` | A number nothing traces to. The owning document says so. |
| `UNMEASURED` | The figure stands in for a measurement nobody has made. |
| `ASSUMED` | An input taken as true with no data behind it. |
| `PLACEHOLDER` | A value that is there only so something compiles or runs. It is marked as such in the source. |
| `UNVERIFIED-SOURCE` | A source exists but has not been checked against the standard, the datasheet or the regulator. |
| `ATTRIBUTION` | A limit credited to a source that does not state it. |

**Link policy.** The git history of this repository was squashed, so a commit cannot be traced back to the
decision that made a row. The durable links are therefore the OI ID (grep it in `docs/`), the document and
section, and the GitHub issue or pull request that the owning document itself cites. A GitHub number appears
here only where an owning document or a verified commit subject gives it. Where none does, the cell says so
rather than guessing. Issue and pull request links use `https://github.com/stevehickman/NeuroPulse/issues/N`,
which GitHub redirects to a pull request when the number is one.

**One choice, several sources.** The Decision / discussion cell lists every source that bears on a choice: the OI, the document section, each GitHub issue or pull request. Add a source when one turns up. Do not replace one with another.

**Fail-safe direction** is called out where a row has one, because it decides how freely a value may be revised.

**Cost figures are not in this file.** Every cost figure in the document set is a floor (`CLAUDE.md` §2.1), and
nothing here may be read as a price or margin. Row UC-029 points at the cost documents and quotes nothing.

---

## A. Safety ceilings and interlocks

| ID | Choice | Lives in | Class | Why it is open | Decision / discussion | Closes when |
|---|---|---|---|---|---|---|
| UC-001 | PBM weighted-sum rule and the C_A(λ) constants behind the "Σ Ēᵢ / (200 × C_A) ≤ 1" ceiling | `docs/np_bib_pbmirr_001.md` §3.1; `docs/reference/modality-stack.md` §3; `CLAUDE.md` §3 | `UNVERIFIED-SOURCE` | The constants and the repetitive-pulse and multi-wavelength rules come from secondary reading. The purchased standards have not been checked. They gate the pre-signing check, which is not built | `OI-BIBPBM-01`; `OI-HEXTILE-31`, `OI-HEXTILE-30` in `docs/np_hw_hextile_001.md` §11; D-10 (`OI-HEXTILE-32`); [PR #501](https://github.com/stevehickman/NeuroPulse/pull/501), [PR #503](https://github.com/stevehickman/NeuroPulse/pull/503), [PR #502](https://github.com/stevehickman/NeuroPulse/pull/502) | Every [K] figure verified against the standards, and the FDA 2023 draft guidance and IEC 60601-2-57 read in full |
| UC-002 | 400 mW/cm² PBM peak (R-4) | `docs/np_hw_hextile_001.md` §2; `docs/np_bib_pbmirr_001.md` §5; `CLAUDE.md` §3; `firmware/pbm/include/np_pbm_config.h` | `NO-DERIVATION` | The standards bound the average, not the peak, and the 400 figure "still has no recorded derivation". It is nonetheless the hazard control that holds 1064 nm to 400, so it may not be raised or retired while `OI-BIBPBM-03` is open (`CLAUDE.md` §3, §18) | `RISK-03` in `docs/np_rm_001.md`; `OI-BIBPBM-03`; `docs/status/pending-decisions.md` §13.1, §13.1a; [issue #5](https://github.com/stevehickman/NeuroPulse/issues/5) | The RISK-03 regulatory opinion is obtained, or a derivation is recorded |
| UC-003 | 42 °C scalp limit, attributed to IEC 60601-1 | `CLAUDE.md` §3, §4.2; `docs/np_bib_pbmirr_001.md` §3.3; `hardware/np_pbm_model.json` (`FACE_LIMIT_C`) | `ATTRIBUTION` | IEC 60601-1 states 43 °C (≥ 10 min) and a labelling duty above 41 °C. The 42 °C is more conservative than the standard, and its source is unsupported | `OI-BIBPBM-03`; [PR #501](https://github.com/stevehickman/NeuroPulse/pull/501) | A derivation is found, or the attribution is corrected. **The value is not to be raised on this finding** (`CLAUDE.md` §18) |
| UC-004 | 600 mW/cm² three-channel aggregate ceiling (R-5), compiled in as `NP_PBM_AGGREGATE_IRRADIANCE_MW_CM2` | `docs/np_hw_hextile_001.md` §2 (R-5); `firmware/pbm/include/np_pbm_config.h`; `docs/np_fw_pbm1064_001.md` §6.4 | `NO-DERIVATION` | It is exactly half of 3 × 400 with no derivation recorded. The chain ends in a request to counsel that was never sent. Its form (a sum of peaks) also differs from the standards' weighted sum of averages. **Whether D-10 (Rev 26) retires R-5 was not checked in this pass** | `docs/status/pending-decisions.md` §13.1a; `OI-BIBPBM-02`; `OI-HEXTILE-20`; `OI-PBM-05`; `docs/np_reg_pbm1064_001.md` §4 | Counsel answers, or R-5 is restated as the weighted sum |
| UC-005 | 150 mC/cm² per session DC charge ceiling | `firmware/safety_mcu/include/np_safety_config.h` (`NP_CHARGE_DC_LIMIT_MC_CM2`); `docs/np_dt_001.md` §3.2.1 | `UNVERIFIED-SOURCE` | It is derived from the tDCS literature and one rat lesion threshold, with a 35× margin. It has not been reviewed by a regulator. The firmware comment calls it PROVISIONAL | `OI-CHARGE-06` in `docs/status/pending-decisions.md`; closure of `OI-CHARGE-05` in `docs/status/completed-decisions.md` | Regulatory sign-off |
| UC-006 | Fixed-pad electrode areas the per-phase interlock divides by: BES/tACS 25 cm², auricular clip 0.5 cm², cervical collar 2 cm² | `firmware/hub_control/include/np_hub_config.h` (`NP_BES_`, `NP_VNS_`, `NP_CVNS_ELECTRODE_AREA_MCM2`); `docs/np_hw_vnsclip_001.md` §5.2; `docs/np_hw_cvns_001.md` §5.3 | `UNMEASURED` | No pad, drawing or measurement exists. **Fail-safe direction: a smaller declared area gives a smaller charge budget, so each may be revised down freely and up only against a measurement** | `OI-CHARGE-07`; `OI-VNSCLIP-02`; `OI-CVNSHW-04`; `OI-ART-11` and `OI-MMSOCK-11` in `docs/status/pending-decisions.md`; `OI-MMSOCK-02` | Each pad's wetted contact area is specified and measured with a tolerance |
| UC-007 | 25 cm² default area for a channel that declares no geometry | `firmware/safety_mcu/include/np_safety_config.h` (`NP_ELECTRODE_AREA_CM2`) | `ASSUMED` | A fallback. tDCS and HD-tDCS never run against it, because the geometry gate holds them off | `OI-CHARGE-03`, `OI-CHARGE-04` (closed 2026-09-09), recorded in `docs/status/completed-decisions.md` | Any channel can still fall back to it, and the owner confirms none should |
| UC-008 | Independent watchdog timeout, 1000 ms nominal (941–1085 ms across the LSI spread) | `firmware/safety_mcu/include/np_safety_config.h` (`NP_SAFETY_IWDG_TIMEOUT_MS`); `docs/np_sw_ci_001.md` | `UNMEASURED` | It must exceed the main loop's worst case, which includes an Ed25519 verify and a 40 ms flash erase. No WCET has ever been measured | `OI-SWCI-49`; [issue #437](https://github.com/stevehickman/NeuroPulse/issues/437) as cited in `docs/np_sw_ci_001.md`; `OI-RISK2-05`; `FMEA-M02-02` in `docs/np_fmea_001.md` | WCET measured on silicon, then the value tightened |
| UC-009 | Impedance front end: 1 kHz excitation on TIM3, 10 kΩ reference leg, ADC channel assignment | `firmware/safety_mcu/include/np_safety_config.h` (`NP_IMP_SENSE_R_OHM` and neighbours) | `PLACEHOLDER` | Nothing in the repository specifies the excitation source, sense amplifier or reference leg. The ohms returned rest on no bench measurement. **Fail-safe: errors read as an impedance above the maximum, which refuses the enable** | `OI-SWCI-34`; `docs/np_sw_ci_001.md` phase 7 | An analog front end is specified and the reference leg calibrated |
| UC-010 | Safety MCU pin assignments (NTC sense pins, ADC1 inputs) and SPI mode 0 | `firmware/safety_mcu/include/np_safety_config.h`; `firmware/safety_mcu/platform/np_hal_adc.c` | `ASSUMED` | Provisional pending PCB layout (gate G1). Nothing records the hub's SPI clock polarity or phase | `OI-SWCI-28`; `OI-SWCI-32` | PCB layout (G1), and the hub's SPI mode recorded |
| UC-011 | Delivered-versus-commanded stimulation cross-check: 10 % tolerance, 100 µA floor, 3 consecutive snapshots | `firmware/hub_control/include/np_hub_config.h` (`NP_STIM_XCHECK_*`); `docs/np_fmea_001.md` §3.3 | `PLACEHOLDER` | The source comment says "not derived" and trace to no measurement. FMEA-M03-02's residual score depends on them | `OI-FMEA-09`; `docs/np_hw_tacsdrv_001.md` §5.3 | A threshold derived from the sense path's measured accuracy and the charge headroom |
| UC-012 | Cardiac cross-check tolerance, ±5 BPM | `firmware/safety_mcu/include/np_safety_config.h` (`NP_CARDIAC_XCHECK_BPM`); `docs/np_fw_cvns_001.md` §14.6, §14.7 | `NO-DERIVATION` | Chosen by the principal and inherited from `NP_CVNS_BASELINE_CROSSVAL_BPM`. The document says "No derivation" and does not retire it because the principal chose it. Looser misses a false rhythm near the real one. Tighter adds nuisance trips (30 s lockout and app confirmation each, hazard 25-e). The false-trip rate on a moving heart rate is unmeasured | `OI-CVNS-15` (a), (b); `OI-CVNS-13`; `OI-CVNS-14`; [PR #521](https://github.com/stevehickman/NeuroPulse/pull/521); `FMEA-M05-10`; `RISK-25` hazard 25-e | A derivation against `CLAUDE.md` §18's two questions, and a measured false-trip rate (FAI-CV03) |
| UC-036 | Cardiac cross-check report staleness, 3 s (3000 ms) | `firmware/safety_mcu/include/np_safety_config.h` (`NP_CARDIAC_XCHECK_STALE_MS`); `docs/np_fw_cvns_001.md` §14.7 | `NO-DERIVATION` | Chosen to match `NP_CARDIAC_RPEAK_STALE_MS`, which is not itself traced here. The hub reports about once a second, so two lost frames are tolerated. The document says "No derivation". It also sets how long a silent hub is tolerated after agreement, and a stale report after agreement cuts CVNS | `OI-CVNS-15` (a); [PR #521](https://github.com/stevehickman/NeuroPulse/pull/521) | A derivation, with the R-peak staleness it was copied from traced too |
| UC-037 | Cross-check averaging span, 8 intervals | `firmware/common/include/np_spi_wire_types.h` (`NP_SAFETY_HR_REPORT_INTERVALS`); `firmware/safety_mcu/include/np_safety_config.h` (`NP_CARDIAC_XCHECK_HR_INTERVALS`); `docs/np_fw_cvns_001.md` §14.7 | `ASSUMED` | The `_Static_assert` against `NP_RR_BUF_SIZE` is a consistency check, so the hub and the MCU agree. It does not justify 8. The origin of the MCU's own 8-interval window was not found in this pass. The §14.7 constants table gives only the assert as its trace | [PR #521](https://github.com/stevehickman/NeuroPulse/pull/521); `OI-CVNS-15` does not list it | The origin of `NP_RR_BUF_SIZE` = 8 is found and linked here, or an open item is raised for it |
| UC-013 | Impact thresholds: 15 g drop, 3 drops, and a 7-session-gap window | `firmware/shdr/include/np_accel_shdr.h`; `docs/np_fw_emmc_002.md` §G.2, §H.4 | `PLACEHOLDER` | Chosen before any hardware existed and never compared against a failed device. The 7-gap window also replaced a 7-day window that no clock on the device can count | `OI-EMMC2-09`; `OI-EMMC2-10`; `docs/np_priv_rem_001.md` | The §H.4 review gate, which is the only sanctioned path |
| UC-014 | Tier-authority public key, all zeros in source | `firmware/safety_mcu/include/np_safety_config.h` (`NP_TIER_AUTHORITY_PUBKEY_INIT`) | `PLACEHOLDER` | **It fails closed: every unit reads as T1.** The key is generated at a manufacturing key ceremony that has not happened | `OI-UPG-08`; `OI-UPG-01`; `docs/np_reg_upg_001.md` §7 | The key ceremony is held |
| UC-015 | Bootloader signing key, a build-time placeholder | `firmware/bootloader/src/np_signature.c` | `PLACEHOLDER` | The source says it is overwritten by the build system from a secure key store. No design document for that key store was found in this pass | None found in this pass | A key-store process is documented and linked here |
| UC-016 | Intranasal duty ceiling inherited from the zone modules | `docs/np_hw_nasal_001.md` §5; `INS_DUTY_MAX` in the nasal firmware | `NO-DERIVATION` | "The duty ceiling is inherited, not derived." It bounds duty, not irradiance, and is not a mucosal exposure limit. The probe also has no NTC | `docs/np_hw_nasal_001.md` §5 item 3; `D-10` in `docs/np_hw_hextile_001.md` (the probe is outside R-4) | A mucosal exposure limit is found or derived |
| UC-017 | 1064 nm drive limits: 200 mA at 10 ms pulse width | `docs/np_proc_fpc_1064_001.md` §3.3; `docs/np_fw_pbm1064_001.md` §5.5 | `NO-DERIVATION` | Marked "underived" in the document. Its V_f rows also disagree with the tile's design basis | `OI-PBM-HW-10`; `OI-CONV-08` (a) | Re-derived against the tile's design basis, or retired in place under `CLAUDE.md` §18 |

## B. Power, thermal and hardware figures

| ID | Choice | Lives in | Class | Why it is open | Decision / discussion | Closes when |
|---|---|---|---|---|---|---|
| UC-018 | PBM power and thermal model constants: per-tile watts, available watts, non-PBM overhead, face and cavity time constants and resistances, one calibration point | `hardware/np_pbm_model.json`; `firmware/hub_control/include/np_pbm_model.generated.h`; `docs/np_pwrsrc_001.md` | `UNMEASURED` | Every constant is tagged `provisional` or `placeholder` in the JSON. None is measured, and the watts inherit the emitter that has not been chosen | `OI-PWRSRC-13` ([issue #440](https://github.com/stevehickman/NeuroPulse/issues/440)); `OI-HEXTILE-02`; `OI-HEXTILE-20` | An emitter is selected and the constants are measured |
| UC-019 | 28 W interim thermal budget for the PBM governor | `hardware/np_pbm_model.json` (`THERMAL_BUDGET_W`); `docs/np_pwrsrc_001.md` §11 | `PLACEHOLDER` | A conservative interim ceiling "until the thermal budget is derived". It binds below the electrical budget in every row, and it blocks `OI-PWRSRC-14` | `OI-PWRSRC-12` ([issue #335](https://github.com/stevehickman/NeuroPulse/issues/335)); `OI-FAN-01`; `OI-PWRTH-08` | The thermal budget is derived and the fan constants verified |
| UC-020 | CEM43 reference lines at 2 and 40 | `hardware/np_pbm_model.json` | `PLACEHOLDER` | "Literature value, not a project figure". No gate may act on them | `OI-PWRSRC-02` | A sourced clinical input |
| UC-021 | 7 % tile-power allocation | `docs/np_hw_hextile_001.md` §8 | `NO-DERIVATION` | "Asserted here, not derived." It traces to no tile power or temperature limit | `OI-HEXTILE-18` | Re-derived from §9.3 |
| UC-022 | Bezel lateral width, 2.5 mm | `docs/np_hw_hextile_001.md` §3; `docs/np_hex_zm_001.md` §3.1 | `NO-DERIVATION` | A column-header input of the zone-map document. Active field area, irradiance and emitter count are all computed from it | `OI-HEXTILE-01`; `OI-RISK3-01` in `docs/np_risk_003.md` | Derived against the co-moulded gasket and retention groove |
| UC-023 | No 660/808 nm emitter selected, and the 660 nm part cannot run at 400 mW/cm² on 45 sites inside its rated current range | `docs/np_hw_hextile_001.md` §4, §11; `docs/np_cost_001.md` | `UNMEASURED` | The cost term for the emitter is uncosted, so every cost figure is a floor and `OI-HUB-C08` cannot be closed | `OI-HEXTILE-02`; `OI-HEXTILE-29`; `OI-HEXTILE-06`; `OI-COST-10`; [issue #333](https://github.com/stevehickman/NeuroPulse/issues/333); [PR #494](https://github.com/stevehickman/NeuroPulse/pull/494) | An emitter is selected, and `OI-HEXTILE-06` is decided before any price is set |
| UC-024 | Convection coefficients h = 30 (stirred), 30 (forced external), 10 (natural) W/m²K, and literature conductivities for PBT, BN, foam and CFRP | `docs/np_therm_cool_001.md` §1; `docs/np_therm_cfd_r1_001.md` | `ASSUMED` | Assumed, not measured | `OI-THCOOL-03`; `OI-R1-04` | CFD or bench values replace them |
| UC-025 | Aggregate-concurrency inputs: 10 % of tile heat reaching the cavity, a ~0.1 m² vault area, and cavity resistances of 0.41 and 0.23 m²K/W borrowed from another path | `docs/np_pwr_budget_001.md` §3.2 | `ASSUMED` | The document's own header reads "Assumptions (stated, not measured)" | `OI-PWR-03`, `OI-PWR-11` in that document; `OI-PWRSRC-12` | THERM-1 characterisation |
| UC-026 | TMS coil geometry (wing radius, turns, energy store) | `docs/np_hw_tms_001.md` §3 (`REQ-TMS-02`…`05`); `docs/np_pwr_therm_001.md` | `ASSUMED` | The geometry is "chosen here, not specified by ME", and wing radius is the largest lever on depth falloff, inductance and energy | `OI-PWRTH-09`; `OI-PWR-03`; `SPEC-TMS-01` | ME specifies the coil |
| UC-027 | EMF shielding attenuation targets, 35–45 dB ELF magnetic and 40–60 dB RF | `CLAUDE.md` §4.3; `docs/reference/hardware-detail.md` §4.3; `docs/np_bib_emf_001.md` | `UNMEASURED` | Design targets. `EMF-1` has never run, and the benefit of shielding to EEG or to therapy is itself unmeasured | `OI-BIBEMF-03`; `OI-BIBEMF-10`; `OI-EMCCAV-01`; `OI-EMCCAV-16`; [issue #391](https://github.com/stevehickman/NeuroPulse/issues/391) (`REQ-CAV-04`) | `EMF-1` is run |
| UC-028 | Socket geometry and the scan-based helmet surface | `hardware/np_socket_map.json`; `hardware/np_helmet_surface.json`; `docs/np_hex_zm_001.md` §3.4 | `ASSUMED` | Both files say PROVISIONAL, pending gates `REG-1` and `ACT-1`. The thermal study uses topology only, not magnitudes | `OI-N1-07` in `docs/np_therm_cfd_n1_001.md`; `OI-HUB-C13` | `REG-1` and `ACT-1` pass |
| UC-029 | Every cost figure in the document set | `docs/np_cost_001.md`; `docs/reference/commercial-model.md` §2 | `UNMEASURED` | Each is a floor, because the emitter cost is uncosted. No figure may be quoted without reading `np_cost_001.md` first | `OI-COST-08`, `OI-COST-09`, `OI-COST-10`; `OI-HEXTILE-06` | The emitter is costed. Nothing is quoted here |
| UC-030 | Registration of a 10-20 montage across the head-size range: seating concentricity, η and PD2 albedo discrimination | `docs/np_hw_eegnet_001.md` §1.1 and the open-items table | `UNMEASURED` | Concentricity is "unspecified and unmeasured" and is the dominant registration term. η is unmeasured. PD2 is unvalidated across Fitzpatrick I–VI | `OI-EEGNET-14`; `OI-EEGNET-02`; `OI-EEGNET-05`; `OI-BIBPBM-04` (no skin-type term in any dose model) | Gates `NET-1`, `NET-2` and `REG-1` |

## C. Tooling, procurement and consumable thresholds

| ID | Choice | Lives in | Class | Why it is open | Decision / discussion | Closes when |
|---|---|---|---|---|---|---|
| UC-031 | Hub dock force ≤ 2 N, F-02 boss 1.0 × 0.5 mm, 6 mm louvre slot, 12×-OD Boa bend radius | `docs/np_tool_hub_001.md` §3 | `NO-DERIVATION` | Each is stated with no source. The louvre slot is 2× the finger-probe figure with no stated basis, and the dock has no retention floor at all | `OI-HTOOL-10`; `OI-HTOOL-09`; `OI-HTOOL-11`; `OI-HTOOL-02`; `OI-CONV-08` (c); [issue #394](https://github.com/stevehickman/NeuroPulse/issues/394) | A source or derivation, or the number is retired in place |
| UC-032 | Lens tooling figures: arm stiffness ≤ 0.5° under 5 N, tether ≥ 50 N, PDMS haze ≤ 10 %, rim-guard snap 5–20 N; and ≥ 100 nits at 40 Hz in the app roadmap | `docs/np_tool_lens_001.docx`; `docs/np_app_roadmap_001.md` Phase 4; `docs/np_tool_shell_002.md` SH-19 | `NO-DERIVATION` | Searched for and not found, so raised and not retired (`CLAUDE.md` §18) | `OI-CONV-09` (a), (e), (f); [issue #394](https://github.com/stevehickman/NeuroPulse/issues/394) | Each is sourced or retired in place |
| UC-033 | Audio cup foam replaced at 150 sessions | `docs/reference/commercial-model.md` §2.3; the consumable inventory on both platforms | `PLACEHOLDER` | "The calendar-era number, retained deliberately rather than re-derived." The trigger kind (exposure count) is settled, but the threshold is not derived from compression set | `OI-ACC-04`; `OI-ACC-02`; `OI-AUDIOHW-08` | The seal-loss criterion in dB is measured |
| UC-034 | EEG hydrogel tip replaced at 45 sessions | the same | `NO-DERIVATION` | 45 is the midpoint of "30–60 sessions" and no document sources that range. The row names an impedance trend that no platform computes | `OI-ACC-09`; `OI-ACC-06` ([issue #387](https://github.com/stevehickman/NeuroPulse/issues/387)); `FMEA-G02-01` | An impedance-trend producer is built, or the threshold is sourced |
| UC-035 | The product name "NeurOne" | `docs/status/pending-decisions.md` §13.1 | `ASSUMED` | An uncleared placeholder. No trademark search has been run, and one is required before any external conversation | The §13.1 row | Trademark search and clearance |

---

## Flagged, not traced

These look like the rows above but were **not** traced in this pass. They are listed so that nobody
mistakes their absence from the tables for a finding that they are justified. Each needs its derivation
looked for, and then a row above or a note here that it is justified.

- 1064 nm session dose of 36 J/cm² (`firmware/pbm/include/np_pbm_config.h` says "conservative; see `OI-SES-01`") and the 60 J/cm² limits at 660 and 808 nm.
- `k ≥ 10` and the ≥ 1-week date-rounding floor (`CLAUDE.md` §5.3; `docs/reference/data-architecture-detail.md` §5.3).
- Reminder snooze caps of 3× and 5× (`docs/reference/data-architecture-detail.md` §5.2).
- The hub heart-rate report period, 1000 ms (`NP_CVNS_HR_REPORT_PERIOD_MS`, also from [PR #521](https://github.com/stevehickman/NeuroPulse/pull/521)); the decision there that hub silence after agreement stays a cut was recorded but its basis was not read.
- The 2.0 g impact event floor (`firmware/shdr/include/np_accel_shdr.h`).
- Phase thresholds of the predictive-maintenance system (0–1,000, 1,000–10,000, 10,000+ devices).
- The 52–62 cm head range and the 30 s tDCS ramp.

## Not surveyed

**Not read in this pass:** `app/`, `protocols/`, `npps/`, `simulator/`, `locales/`, `infra/`,
`docs/np_ses_pwr_001.md`, `docs/np_dt_001.md` (beyond §3.2.1), `docs/reference/{clinical,competitive-position,marketing-notes,regulatory-strategy}.md`
and the `.docx` documents other than the two named above. The survey method was a keyword search of the
documents and firmware headers, followed by reading each hit's owning row. Anything whose weakness is not
labelled in its owning document will not have been found this way.

## Justified or retired

Append-only. When a row above is justified, retired or found to have been wrong, move it here with its
ID, the date, and the link to the justification. Do not delete it from history.

| ID | Choice | Date | Outcome | Justified by / retired by |
|---|---|---|---|---|
| — | none yet | — | — | — |
