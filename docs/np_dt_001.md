# Design Input/Output Traceability Matrix

**Project:** NeurOne
**Document:** NP-DT-001
**Revision:** B
**Date:** 2026-07-22
**Status:** DRAFT
**Effective Date:** 2026-06-07
**Author:** Steve Hickman (CEO, interim Quality authority)
**Approved By:** Steve Hickman, CEO
**References:** NP-QMS-DC-001 Rev A, NP-DP-001 Rev A, NP-DHF-001 Rev F, NP-RM-001 Rev A, NP-SW-001 Rev A, 21 CFR §820.30, ISO 13485:2016 §7.3
**Related Issues:** GitHub Issue #122
**Gate:** NP-DP-001 §6.4 G2 exit criterion
**IEC 62304 Class:** —
**Supersedes:** —
**Parent Document:** NP-QMS-DC-001

---

## §1 Purpose and Scope

NP-DT-001 is the formal Design Input/Output Traceability Matrix for the NeurOne platform (T1 Home + T2 Pro). It satisfies:
- **21 CFR §820.30(c):** design inputs shall be documented and shall address the intended use of the device, including the needs of the user and patient
- **21 CFR §820.30(d):** design outputs shall be documented and shall be expressed in terms that allow an adequate evaluation of conformance to design input requirements
- **ISO 13485:2016 §7.3.3:** design and development inputs shall include functional, performance, usability, safety, and regulatory requirements
- **ISO 13485:2016 §7.3.4:** design and development outputs shall meet design and development input requirements
- **NP-DP-001 §6.4:** G2 gate exit criterion — DI/DO traceability matrix must be complete before Design Verification phase begins
- **NP-QMS-DC-001 §6.2:** "each design output shall meet or be traceable to design inputs"

**Scope:** All T1 (NeurOne Home) and T2 (NeurOne Pro) hardware, firmware, software, and tooling design outputs. Includes the headset assembly, zone module system, control hub, iOS/Android application, and all accessory modules.

**Tier differentiation:** T1 inputs apply to the FDA-exempt wellness configuration (NeurOne Home). T2 inputs apply to the FDA 510(k) configuration (NeurOne Pro). Inputs marked T1+T2 apply to the shared platform base and are inherited by both configurations.

---

## §2 Methodology

### §2.1 Design Input (DI) definition and classification

A design input is a requirement the design must satisfy. Design inputs derive from:
- Intended use and indications for use (product design specifications)
- User needs (patient, clinician, caregiver)
- Regulatory standards and statutory requirements
- Safety and risk management outputs (ISO 14971 risk controls)
- Interface requirements (USB-C PD, Bluetooth, FHIR, WatchOS)

**ID format:** `DI-{CAT}-{NN}` where:
- `PERF` = performance and functional requirements
- `SAFE` = safety requirements (all safety MCU-owned interlocks)
- `USE` = usability requirements
- `REG` = regulatory and standards compliance requirements
- `INT` = external interface requirements

**Priority levels:**
- `Safety-Critical` — failure mode analysis required; DO must reference FMEA and risk register; VE must include hardware bench result or equivalent
- `High` — must be traceable to DO and VE before G3 gate
- `Medium` — must be traceable to DO before G2 gate; VE can be pending lab testing
- `Low` — provisional or T2-roadmap items; traceability required before shipping

### §2.2 Design Output (DO) definition and classification

A design output is a documented design artefact that implements or satisfies one or more design inputs. Design outputs include:
- Hardware specifications and procurement documents
- Tooling specifications
- Firmware specifications and source code modules
- Software application source code
- Risk documents (risk register, FMEA, risk management plan)
- Regulatory documents (510(k) pre-submissions, opinion briefs)
- Test plans and FAI checklists

**ID format:** `DO-{TYPE}-{NN}` where:
- `HW` = hardware specification, tooling specification, or procurement document
- `FW` = firmware specification or firmware source code
- `SW` = software source code
- `API` = API or integration specification
- `FHIR` = FHIR ImplementationGuide
- `RISK` = risk document
- `REG` = regulatory document
- `TEST` = test plan, FAI checklist, or verification protocol

### §2.3 Verification Evidence (VE) definition

Verification evidence is a test record, FAI result, software analysis pass, or regulatory opinion that demonstrates a design output meets its corresponding design input(s).

**ID format:** `VE-{NN}`

**Evidence types:**
- `Regulatory opinion` — outside counsel opinion letter (required for irradiance claims)
- `Software PASS` — static analysis, unit test, or architecture review passing defined criteria
- `FAI Checklist` — First Article Inspection results against specification
- `Design Review` — gate review record confirming design output completeness
- `Privacy Audit` — independent or systematic code-level privacy analysis
- `FMEA` — failure modes and effects analysis results
- `Standards testing` — accredited laboratory testing against IEC/FCC/FDA standards
- `HFE Testing` — human factors engineering formative or summative study results

### §2.4 Status codes

| Code | Meaning | G3 implication |
|------|---------|----------------|
| `Traced` | DI→DO→VE chain complete; evidence on file | No action required |
| `Partial` | DO exists and is baselined; VE pending hardware bench, lab testing, or regulatory opinion | VE must be obtained before G3 gate for safety-critical and high-priority inputs |
| `Open` | DO not yet authored, or DO is incomplete; VE necessarily absent | DO must be authored before G2 gate (high) or G3 gate (medium) |

---

## §3 Design Input Registry

### §3.1 Performance inputs (DI-PERF)

| DI-ID | Category | Input Description | Source | Priority | Scope |
|-------|----------|-------------------|--------|----------|-------|
| DI-PERF-01 | Performance | EEG: 8-channel semi-dry hydrogel, 500 Hz, 24-bit ADS1299, Fp1/2 F3/4 C3/4 P3/4; ADS1299 self-calibration at session start | CLAUDE.md §3 modality 3 | High | T1+T2 |
| DI-PERF-02 | Performance | EEG T2: 21-channel wet-gel qEEG, full 10-20 + FC3/FC4 + Oz + A1/A2 (linked-ear reference on VNS clips) | CLAUDE.md §3 T2 | High | T2 |
| DI-PERF-03 | Performance | PBM transcranial: 660nm + 808–830nm on T1-A base PBM tiles, tiled across the hex-socket lattice (NP-HEX-ZM-001) — LED total scales with tiles populated, not a fixed 5-zone count; 120–180 mA/LED, L70 ≥80,000 hours | CLAUDE.md §3 modality 1 | High | T1+T2 |
| DI-PERF-04 | Performance | PBM irradiance: 400 mW/cm² peak pulsed (≤25% duty cycle, firmware-enforced); 200 mW/cm² CW max; 7 frequency presets | CLAUDE.md §3 modality 1 | High | T1+T2 |
| DI-PERF-05 | Performance | Real-time J/cm² dose metering per zone per wavelength; dual-PD (PD1 forward + PD2 scalp backscatter); PD1/PD2 ratio separates fouling from LED aging | CLAUDE.md §3 modality 1, RISK-14 | High | T1+T2 |
| DI-PERF-06 | Performance | 1064 nm smart module (T1-C, NP-HEX-ZM-001 §4a): on-module ATtiny402 I2C driver + 3× IRLML6344 N-FETs, Hamamatsu G12180-010A InGaAs PD dose-metering. **⚠ Superseded 2026-07-28:** 550-LED count and ZONE_ID detection were sized for the retired 66×78mm/5-slot module; driver/PD component selection remains relevant, element count and detection mechanism do not. | CLAUDE.md §3 modality 1; NP-HEX-ZM-001 §4a | High | T1+T2 |
| DI-PERF-07 | Performance | BES/tACS T1: 0.5–40 Hz, ≤1 mA, charge-balanced biphasic; adaptive EMF notch prevents Helmholtz cancellation of stimulus | CLAUDE.md §3 modality 4 | High | T1 |
| DI-PERF-08 | Performance | Clinical tACS T2: ≤4 mA, 16-channel arbitrary waveform; per-electrode independent current control | CLAUDE.md §3 T2 | High | T2 |
| DI-PERF-09 | Performance | tDCS: 0.1–2 mA DC, ≤3 electrode pairs, per-electrode impedance monitoring | CLAUDE.md §3 modality 5 | High | T1+T2 |
| DI-PERF-10 | Performance | Auricular VNS: 1–25 Hz, ≤2 mA biphasic charge-balanced; PPG HRV 808–830 nm in same clip; A1/A2 EEG reference contacts on clip pads | CLAUDE.md §3 modality 6 | High | T1+T2 |
| DI-PERF-11 | Performance | Cervical VNS (tcVNS) T2: gel electrodes at carotid sheath; ≤2 mA; bilateral or unilateral; 10 s ramp-up, 5 s ramp-down | CLAUDE.md §3 T2 | High | T2 |
| DI-PERF-12 | Performance | TMS T2: focal figure-8 coil, 0.1–0.5 T, rTMS + TBS; non-conductive CFRP window at coil site; EMF gating 5 ms pre-pulse / 50 ms post-pulse hold | CLAUDE.md §3 T2 | High | T2 |
| DI-PERF-13 | Performance | 1170 nm deep PBM T2: laser diodes, 35–40 mm subcortical depth, TEC stabilisation, ≤1,000 mW/cm² | CLAUDE.md §3 T2 | High | T2 |
| DI-PERF-14 | Performance | sLORETA-guided HD-tDCS T2: scalar sLORETA 2447 voxels × 21 channels; 4×1 ring montage; 7 clinical targets (DLPFC_L/R, VLPFC_L, ACC, MPFC, M1_L/R); ≤2 mA/electrode | CLAUDE.md §3 T2; NP-FW-HD-001 Rev A | High | T2 |
| DI-PERF-15 | Performance | Neural audio entrainment: over-ear planar magnetic 40 mm + bone conduction at mastoid; binaural beats + isochronic tones + pink/brown noise; EEG-adaptive frequency | CLAUDE.md §3 modality 7 | Medium | T1+T2 |
| DI-PERF-16 | Performance | Visual stimulation: 108 micro-LEDs/lens (660 nm + 808–830 nm), 6 zones/eye; photic driving 0.5–100 Hz; EMDR L/R alternation; Mode F invisible NIR retinal PBM | CLAUDE.md §3 modality 8 | High | T1+T2 |
| DI-PERF-17 | Performance | Closed-loop EEG-adaptive stimulation: autonomous without phone or app in Mode 3; full modality orchestration from signed session descriptor on eMMC | CLAUDE.md §4.6 | High | T1+T2 |
| DI-PERF-18 | Performance | Operating modes: Mode 1 connected real-time <1 ms; Mode 2 protocol upload <5 s; Mode 3 autonomous from any USB-C PD power bank; Mode 4 download EDF+ on reconnect | CLAUDE.md §4.6 | High | T1+T2 |
| DI-PERF-19 | Performance | UHDR/SHDR separation: AES-256-XTS Argon2id-derived key; biometric/PIN input; NeurOne never holds decryption key; two-layer UKMD/WKMD wrapper | CLAUDE.md §5; NP-FW-EMMC-002 §C | High | T1+T2 |
| DI-PERF-20 | Performance | Storage: 8 GB industrial eMMC (SLC, 30,000+ P/E cycles), 9-partition GPT layout; LittleFS; UHDR 6,903 MiB; SHDR 512 MiB; Scratch 500 MiB zeroed on boot | CLAUDE.md §4.1; NP-FW-EMMC-001 Rev A §4 | Medium | T1+T2 |
| DI-PERF-21 | Performance | Dual-bank OTA firmware: Ed25519 signature verification on all images; SNVS_LPGPR0 bank flag; 9-step OTA sequence with readback; USB-C DFU recovery always available | CLAUDE.md §4.1; NP-FW-EMMC-001 Rev A §8 | High | T1+T2 |
| DI-PERF-22 | Performance | EMF shielding: 5-layer passive (CFRP 30–50 dB + mu-metal 15–25 dB ELF + palladium fabric 40–60 dB RF + absorber foam + port filters) + active Helmholtz 35–45 dB ELF combined | CLAUDE.md §4.3 | High | T1+T2 |
| DI-PERF-23 | Performance | Fit system: Boa dial 10 cm range, 0.5 mm/click, 50,000-cycle; enclosed PTFE-lined cable channel; 5-position bridge; spring-decoupled electrode pods 80–120 g / ±12 mm; 1 adult SKU covers 52–62 cm | CLAUDE.md §4.4 | Medium | T1+T2 |
| DI-PERF-24 | Performance | Zone modules: snap-in field-upgradeable hex tiles, count scales with sockets populated (not a fixed 5); user self-service swap; connector ≥1,000 insertion cycles; sliding eject lever ≤1 N extraction force. **⚠ Superseded 2026-07-28** — "5 snap-in" and the Hirose FH34S connector-per-zone assumption are retired; see `docs/np_hw_fpc_001.md`. | CLAUDE.md §7.1; NP-HEX-ZM-001 | Medium | T1+T2 |
| DI-PERF-25 | Performance | HRV biofeedback: resonance-frequency breathing pacer (default 6 BPM, personalised sweep 4–7 BPM); coherence score 0–10; 4 protocols (standalone, HRV+taVNS, HRV+EEG dual, HRV+PBM); software-only | CLAUDE.md §3 modality 6 | Medium | T1+T2 |

### §3.2 Safety inputs (DI-SAFE)

| DI-ID | Category | Input Description | Source | Priority | Scope |
|-------|----------|-------------------|--------|----------|-------|
| DI-SAFE-01 | Safety | Charge density limit: 40 µC/cm² hardware-enforced by safety MCU STM32G071; app and main processor cannot override; covers BES/tDCS/tACS/HD-tDCS | CLAUDE.md §4.2; NP-SW-001 SW01-M03 | Safety-Critical | T1+T2 |
| DI-SAFE-02 | Safety | SPI heartbeat watchdog: safety MCU receives 200 ms heartbeat; 1.5 s timeout without heartbeat → all stimulation GPIO cutoff ≤50 ms; fault latch requires explicit clear | CLAUDE.md §4.2; NP-SW-001 SW01-M02 | Safety-Critical | T1+T2 |
| DI-SAFE-03 | Safety | Cervical VNS cardiac interlock: HR change >15 BPM within 5 s rolling observation window → CVNS GPIO cutoff ≤100 ms; 30 s lockout; baseline cross-validation PPG vs GPIO-timer within ±5 BPM | CLAUDE.md §4.2; NP-FW-CVNS-001 Rev A; NP-SW-001 SW01-M05 | Safety-Critical | T2 |
| DI-SAFE-04 | Safety | Photoparoxysmal EEG detection at Oz electrode → goggle LED hard cutoff ≤200 ms; clinician-unlock required to re-enable for 3–30 Hz frequency range | CLAUDE.md §3 modality 8; NP-SW-001 SW01-M06 | Safety-Critical | T1+T2 |
| DI-SAFE-05 | Safety | Visual retinal safety: IEC 62471 MPE ceiling enforced by 3 independent hardware layers — IR proximity sensor, Hall sensor (goggle lift), and hardware current limit; 50% of exempt group threshold | CLAUDE.md §3 modality 8 | Safety-Critical | T1+T2 |
| DI-SAFE-06 | Safety | Session protocol: Ed25519 cryptographic signature required before any stimulation GPIO enable; headset rejects unsigned or corrupted protocol descriptors | CLAUDE.md §4.2; NP-SW-001 SW01-M07 | Safety-Critical | T1+T2 |
| DI-SAFE-07 | Safety | tDCS ramp: 30 s ramp up / 30 s ramp down hardware-enforced by safety MCU; not overridable by app or main processor; covers both T1 consumer and T2 HD-tDCS | CLAUDE.md §3 modality 5; NP-SW-001 SW01-M01 | Safety-Critical | T1+T2 |
| DI-SAFE-08 | Safety | Thermal management: NTC thermistor per zone; hardware throttle at 62 °C junction temperature; hardware cutoff at 65 °C; IEC 60601-1 scalp surface ≤42 °C compliance required | CLAUDE.md §4.2; NP-SW-001 SW01-M04 | Safety-Critical | T1+T2 |
| DI-SAFE-09 | Safety | Fault latch: any safety cutoff latches the fault state; explicit app confirmation + repeat impedance check required to re-enable; safety faults cannot be suppressed by stealth mode | CLAUDE.md §4.2; NP-SW-001 SW01-M08 | Safety-Critical | T1+T2 |
| DI-SAFE-10 | Safety | TMS pulse protection: EMF Helmholtz cancellation gated off 5 ms before pulse trigger; 50 ms hold-off after pulse; non-conductive CFRP window at coil site prevents eddy current field loss | CLAUDE.md §4.2 | Safety-Critical | T2 |
| DI-SAFE-11 | Safety | Mode F retinal PBM: default-off; requires separate explicit user consent distinct from session consent; right temple amber LED triple-pulse pattern (3×150 ms) when active and not suppressible; firmware build flag `NP_MODE_F_REGULATORY_CLEARED = 0` until RISK-03 Q-13 opinion received | NP-FW-EMMC-002 Rev A §F | Safety-Critical | T1+T2 |
| DI-SAFE-12 | Safety | Impedance check: 1 kHz AC synchronous impedance measurement required before any stimulation pulse; no timeout condition shall produce a false PASS; contact not confirmed → GPIO enable held low | CLAUDE.md §4.2; NP-SW-001 SW01-M06 | Safety-Critical | T1+T2 |
| DI-SAFE-13 | Safety | Scalp-facing surface (applied part) ≤42 °C maintained under normal operation **and single-fault loss of forced convection** (fan / heatsink airflow loss): direct scalp-facing NTC co-located with PD2 (SR-FAN-01/02, Path B1) + natural-convection-safe PBM duty derate (SR-FAN-03: halt/trickle on fan loss, ≈4.5 mW/cm² ceiling at 43.3 °C ambient) + firmware ambient/duty envelope gate (NP-ENV-OPRANGE-001 / NP-FW-POE-001). **Base thermal design rejects module heat via BN-boss conductive export to an external heatsink with the shielded interior left un-ventilated** (preserves the EMF-shield / IP-seal). The DI-SAFE-08 junction throttle (62/65 °C) is proven insufficient to bound the face and does not substitute for this requirement (NP-THERM-CFD-R1-001). | NP-REQ-FANHEALTH-001 (SR-FAN-01…06); NP-THERM-CFD-R1-001; NP-SW-001 SW01-M04; IEC 60601-1 | Safety-Critical | T1+T2 |

### §3.3 Usability inputs (DI-USE)

| DI-ID | Category | Input Description | Source | Priority | Scope |
|-------|----------|-------------------|--------|----------|-------|
| DI-USE-01 | Usability | Head fit: single adult SKU covers 52–62 cm head circumference; 5-position adjustable forehead bridge at 5 mm steps | CLAUDE.md §4.4 | High | T1+T2 |
| DI-USE-02 | Usability | Zone module extraction: ≤1 N force via 3:1 mechanical advantage sliding eject lever; target user: Parkinson's Hoehn & Yahr stage II–III; RISK-22 Option A eject lever required | CLAUDE.md §7.1 RISK-22 | High | T1+T2 |
| DI-USE-03 | Usability | Status LED readability: amber right-temple LED pulses at session frequency; caregiver-readable at ≥3 m to confirm correct protocol is running; fault = power LED red blink | CLAUDE.md §4.7 | Medium | T1+T2 |
| DI-USE-04 | Usability | Interface protection covers: all tethered to headset by integral anchor posts; cannot be permanently lost without deliberate cutting; zone slot plugs 5 colours position-coded | CLAUDE.md §8.3 | Medium | T1+T2 |
| DI-USE-05 | Usability | **REDESIGNED 2026-07-28 (replaces RISK-15 five-layer keying, retired):** all sockets share one identical shape — mechanical type/position keying is gone, replaced by (1) an orientation-only mating feature (not rotationally symmetric where pin alignment requires it) and (2) software placement validation (`np_module_map` UID inventory + `np_module_map_check_placement()` blocks a protocol from running against a mismatched module map). **Open gap:** braille/raised-numeral and tactile-dot accessible position-ID (old Layers 3–4) were designed for 5 positions and don't scale to ~30–80 sockets. **Bone-conduction audio (old Layer 5) doesn't work for this at all, at any socket count** — it requires head contact, but module insertion requires the helmet off-head. Redirect to companion-app audio instead (app already receives module status over BLE). See `docs/np_hfe_001.md` CT-01, `docs/np_fw_za_001.md`. | CLAUDE.md §7.1 RISK-15 | High | T1+T2 |
| DI-USE-06 | Usability | EEG pod contact force: spring-decoupled pods 80–120 g contact force, ±12 mm travel, Shore 30A silicone mount; independent of Boa dial tension; snap-off bayonet hydrogel tips | CLAUDE.md §4.4 | Medium | T1+T2 |
| DI-USE-07 | Usability | Electrode tip replacement: snap-off bayonet; 30–60 sessions per tip; app impedance trend prompts replacement; moisture-barrier hydration caps WVTR <0.5 g/m²/day for storage | CLAUDE.md §3 modality 3 | Medium | T1+T2 |

### §3.4 Regulatory and standards inputs (DI-REG)

| DI-ID | Category | Input Description | Source | Priority | Scope |
|-------|----------|-------------------|--------|----------|-------|
| DI-REG-01 | Regulatory | IEC 60601-1 general safety for medical electrical equipment: basic safety + essential performance | CLAUDE.md §10 | Regulatory | T1+T2 |
| DI-REG-02 | Regulatory | IEC 60601-2-10 particular requirements for nerve and muscle stimulators: BES, tDCS, tACS, VNS current limits and waveform parameters | CLAUDE.md §10 | Regulatory | T1+T2 |
| DI-REG-03 | Regulatory | IEC 62471 photobiological safety: visual stimulation MPE limits; PBM irradiance hazard classification; GoggModee F retinal cumulative dose | CLAUDE.md §10; RISK-03 | Regulatory | T1+T2 |
| DI-REG-04 | Regulatory | IEC 62133 battery and power bank safety: hub supercapacitor + USB-C PD power path | CLAUDE.md §10 | Regulatory | T1+T2 |
| DI-REG-05 | Regulatory | FCC Part 15 RF emissions: BT 5.3 LE Audio and Wi-Fi 6 radios; antennas in hub not headset | CLAUDE.md §10 | Regulatory | T1+T2 |
| DI-REG-06 | Regulatory | IEC 62304 software lifecycle: SW-01 Safety MCU = Class C; SW-02 Main processor = Class B; SW-03 iOS/Android app = Class B; unit-level FMEA required for SW-01 | NP-SW-001 | Regulatory | T1+T2 |
| DI-REG-07 | Regulatory | IEC 62366-1 human factors engineering: Use-Related Risk Analysis (URRA), formative testing, summative testing plan; FDA HFE Guidance 2016 | CLAUDE.md §10 | Regulatory | T1+T2 |
| DI-REG-08 | Regulatory | ISO 14971:2019 risk management: NP-RM-001 acceptability matrix S1–S5/P1–P5; residual risk evaluation before 510(k) submission | CLAUDE.md §10; NP-RM-001 | Regulatory | T1+T2 |
| DI-REG-09 | Regulatory | 21 CFR Part 820 quality system regulation: design controls; document control; CAPA | CLAUDE.md §10 | Regulatory | T2 |
| DI-REG-10 | Regulatory | FDA cybersecurity guidance: SBOM planned NP-SBOM-001 Year 2; OTA Ed25519 code signing; vulnerability disclosure policy; OTA update process documented | CLAUDE.md §10 | Regulatory | T1+T2 |
| DI-REG-11 | Regulatory | HIPAA Technical Safeguards: UHDR biometric-derived key architecture; NeurOne infrastructure cannot decrypt UHDR at any point; BAA required for T2 clinical tier | CLAUDE.md §5; NP-LEGAL-BAA-001 | Regulatory | T2 |
| DI-REG-12 | Regulatory | GDPR Art. 13(2)(f) adaptive stimulation transparency: classified np_adapt_trigger_t enum (17 values); user-visible AdaptiveAdjustmentsCard in Session History; plain-language description of each trigger | CLAUDE.md §13.4 (Issue #98) | Regulatory | T1+T2 |

### §3.5 Interface inputs (DI-INT)

| DI-ID | Category | Input Description | Source | Priority | Scope |
|-------|----------|-------------------|--------|----------|-------|
| DI-INT-01 | Interface | USB-C PD compliance: any PD-compliant charger must work; app displays "power level: reduced" informatively and never blocks session; EU requirement: no proprietary lock-in | CLAUDE.md §2.2 | High | T1+T2 |
| DI-INT-02 | Interface | BT 5.3 LE Audio GATT custom service: session status, HRV breathing ring, protocol upload; antennas in hub only; single rear toggle; sub-50 ms sync latency target | CLAUDE.md §4.1; NP-APP-ROADMAP-001 | Medium | T1+T2 |
| DI-INT-03 | Interface | FHIR R4 NeurOne T2 Clinical Profile: NP-Patient (opaque MRN), NP-Observation (EEG bands, HRV RMSSD, coherence, dose), NP-DiagnosticReport, NP-Procedure; LSL TLS streaming | NP-INT-FHIR-001 Rev A | High | T2 |
| DI-INT-04 | Interface | Apple Watch WatchConnectivity sync (provisional, HOPE Phase 3 gated): Channel 1 haptic 40 Hz; Channel 2 audio sync; Channel 3 visual flicker ≥100 nits; OI-WA-02 brightness gate | CLAUDE.md §3b; NP-APP-ROADMAP-001 | Low | T1 |
| DI-INT-05 | Interface | LSL streaming for research data acquisition: real-time EEG, HRV, PBM dose, session events over Lab Streaming Layer; TLS encryption required | CLAUDE.md §3 T2 | Medium | T2 |

---

## §4 Design Output Registry

### §4.1 Hardware outputs

| DO-ID | Type | Document / Artefact | Rev | Status | Scope |
|-------|------|---------------------|-----|--------|-------|
| DO-HW-01 | HW Spec | NP-HW-FPC-001 — FPC Zone Module Specification (20-pin pinout, dual-PD RISK-14 Option B, PDMS bonding, five-layer keying, gasket, eject lever) | Rev E | **Superseded 2026-07-28** — see in-doc note | T1+T2 |
| DO-HW-02 | HW Spec | NP-HW-HUB-001 — Hub PCB Rev B (Vishay DG2788A TIA gain switch ×5, NXP PCA9546A I2C mux, GAIN_SEL GPIO sequencing) | Rev B | **Superseded 2026-07-28** — needs Rev C per SMART-1 | T1+T2 |
| DO-HW-03 | HW Spec | NP-PROC-FPC-001 — FPC Procurement Requirements (LED Vf binning, Hirose FH34S exclusions, BCR421W spec, RA copper) | Rev A | Active | T1+T2 |
| DO-HW-04 | HW Spec | NP-TOOL-ZM-001 — Zone Module Tooling Specification (8 features F-01 through F-08, 12-item mould design review checklist, PDMS gasket, eject lever, keying) | Rev A | **Superseded 2026-07-28** — replaced by universal hex-tile mould (NP-HEX-ZM-001) | T1+T2 |
| DO-HW-05 | HW Spec | NP-TOOL-ZM-SM-001 — 1064 nm Smart Module Tooling Variant (rigidizer cavity 24×16×3.5 mm, +2.0 mm mechanical key prevents unprotected insertion) | Rev A | **Superseded 2026-07-28** — see in-doc note | T1+T2 |
| DO-HW-06 | HW Spec | NP-TOOL-SHELL-001 — Shell Tooling Specification (F-01 zone slot anchor posts ×5, F-02 accessory port anchor posts ×3, F-03 EEG cable routing channel, F-04 temporal wing boss PROVISIONAL) | Rev A | **⚠ Needs review** — F-01 anchor posts (×5) sized for the retired 5-zone shell; not yet checked against the hex-socket lattice | T1+T2 |
| DO-HW-07 | HW Spec | NP-TOOL-LENS-001 — Lens and Goggle Assembly Tooling (F-01/F-02 sliding rail, F-03/F-04 N42 magnet pockets, F-05/F-06 EC driver contacts, P-01 AgNW, P-02 hard coat, P-03 PDMS diffuser, F-07 lens rim guard hooks) | Rev B | Active | T1+T2 |

### §4.2 Firmware outputs

| DO-ID | Type | Document / Artefact | Rev | Status | Scope |
|-------|------|---------------------|-----|--------|-------|
| DO-FW-01 | FW Spec | NP-FW-EMMC-001 — eMMC Partition Architecture + OTA Bootloader (9-partition GPT, AES-256-XTS, Argon2id, dual-bank OTA, Ed25519, DFU) | Rev A | Active | T1+T2 |
| DO-FW-02 | FW Spec | NP-FW-EMMC-002 — Privacy Remediation Firmware Delta (warranty TRNG token, factory reset, two-layer key, Scratch encryption, EDF+ header, Mode F spec, SHDR accel reclassification) | Rev A | Active | T1+T2 |
| DO-FW-03 | FW Spec | NP-FW-HRV-001 — HRV Biofeedback Protocol Firmware (np_hrv_ppg, np_hrv_coherence 256-pt Welch, np_hrv_pacer, np_hrv_tavns_sync RSA slope, np_hrv_eeg_biofeedback, np_hrv_session) | Rev A | Active | T1+T2 |
| DO-FW-04 | FW Spec | NP-FW-ZA-001 — Zone Module Bone Conduction Announcement Firmware (RISK-15 Layer 5; LPADC1 ZONE_ID detection; DDS 8 kHz SAI3; 5 parallel slot state machines; SHDR auth log) | Rev A | **Superseded 2026-07-28** — detection retired, audio engine reusable, see in-doc note | T1+T2 |
| DO-FW-05 | FW Spec | NP-FW-HD-001 — sLORETA-Guided HD-tDCS Firmware (2447×21 weight matrix, 64-epoch covariance, 4×1 ring selection, ramp state machine, safety MCU SPI, 7 clinical targets) | Rev A | Active | T2 |
| DO-FW-06 | FW Spec | NP-FW-CVNS-001 — Cervical VNS Safety Interlock Firmware (STM32G071 TIM6 ISR 5 ms, RPEAK_IN GPIO, baseline cross-validation, 30 s lockout, np_cvns_interlock + np_cvns_stim + np_cvns_session) | Rev A | Active | T2 |
| DO-FW-07 | FW Spec | NP-FW-PBM1064-001 — 1064 nm Smart Zone Module Firmware (smart module detection 3.3 kΩ, LPI2C3 400 kHz, CH_A/B/C register map, dose metering, aggregate ceiling 600 mW/cm², T2 combined session) | Rev A | **Superseded 2026-07-28** — addressing retired, register map/calibration model reusable, see in-doc note | T1+T2 |
| DO-FW-08 | FW Spec | NP-FW-HUB-001 — Hub Control Program (FreeRTOS i.MX RT1062; module registry, session runner, session log UHDR/SHDR routing, safety SPI heartbeat; NPPS Ed25519 deserialiser) | Rev A | Active | T1+T2 |
| DO-FW-09 | FW Spec | NP-FW-ANON-001 — Research Anonymisation Engine Firmware (l-diversity l≥3, DP ε≤1.0, k-anonymity, on-device Scratch workspace, AES-256-CTR per-study key, eMMC SANITIZE post-run) | Rev A | Active | T1+T2 |
| DO-FW-10 | FW Spec | NP-FMEA-001 — SW-01 Safety MCU Unit-Level FMEA (SW01-M01 through SW01-M08 failure modes, potential harms, response times, mitigations) | Rev A | Active | T1+T2 |

### §4.3 Software source outputs

| DO-ID | Type | Document / Artefact | Rev | Status | Scope |
|-------|------|---------------------|-----|--------|-------|
| DO-SW-01 | Source | firmware/bootloader/ — Dual-bank OTA bootloader: np_main.c, np_boot_selector.c, np_emmc.c (USDHC2), np_signature.c (Ed25519 self-contained RFC 8032), np_ota.c (9-step), np_dfu.c (USB DFU 1.1) | — | Active | T1+T2 |
| DO-SW-02 | Source | firmware/hrv_biofeedback/ — HRV biofeedback 6-module static library (np_hrv_ppg, np_hrv_coherence, np_hrv_pacer, np_hrv_tavns_sync, np_hrv_eeg_biofeedback, np_hrv_session) | — | Active | T1+T2 |
| DO-SW-03 | Source | firmware/zone_announce/ — Zone detection + bone-conduction audio (np_zone_detect, np_zone_audio, np_zone_announce; LPADC1, SAI3 I2S, eDMA ping-pong) | — | Active | T1+T2 |
| DO-SW-04 | Source | firmware/sloreta_hdtdcs/ — sLORETA source imaging + HD-tDCS electrode selection and current distribution | — | Active | T2 |
| DO-SW-05 | Source | firmware/cervical_vns/ — Cervical VNS stimulation + cardiac interlock (np_cvns_interlock Pan-Tompkins, np_cvns_stim biphasic, np_cvns_session UHDR/SHDR) | — | Active | T2 |
| DO-SW-06 | Source | firmware/pbm_1064nm/ — 1064 nm PBM session orchestrator (smart module detect, LPI2C3 driver, duty-cycle ceiling, dose accumulator, T2 combined np_pbm1064_t2_combined) | — | Active | T1+T2 |
| DO-SW-07 | Source | firmware/hub_control/ — FreeRTOS hub application (SW02-M09..M12: np_hub_module_registry, np_hub_session_runner, np_hub_session_log, np_hub_safety_spi) | — | Active | T1+T2 |
| DO-SW-08 | Source | firmware/anon/ — Research anonymisation module (on-device k-anonymity, DP Laplace, Scratch staging, study descriptor validation, eMMC SANITIZE) | — | Active | T1+T2 |
| DO-SW-09 | Source | app/ios/ — iOS application (BLE GATT, session display Modes 1/2/4, UHDR data, SHDR upload, clinical consent, privacy compliance, consumable tracker, OTA, Watch bridge, AdaptiveAdjustmentsCard) | — | Active | T1+T2 |

### §4.4 Integration, risk, and regulatory outputs

| DO-ID | Type | Document / Artefact | Rev | Status | Scope |
|-------|------|---------------------|-----|--------|-------|
| DO-API-01 | API Spec | NP-API-001 — T2 Clinical Scripting API Specification (multi-patient dashboard, protocol scripting, FHIR export, LSL streaming endpoint) | Rev A | Active | T2 |
| DO-FHIR-01 | INT Spec | NP-INT-FHIR-001 — FHIR R4 ImplementationGuide (NP-Patient opaque MRN, NP-Observation EEG/HRV/dose, NP-DiagnosticReport, NP-Procedure; LSL TLS; FHIR validator CI) | Rev A | Active | T2 |
| DO-RISK-01 | Risk | NP-RISK-001 — Zone Module Risk Register (RISK-01 through RISK-25; 23 MITIGATED; RISK-03 and RISK-20 OPEN) | Rev B | Active | T1+T2 |
| DO-RISK-02 | Risk | NP-RM-001 — ISO 14971 Risk Management Plan (S1–S5/P1–P5 matrix, 9 hazard categories, formal QMS entry of RISK-01..25) | Rev A | Active | T1+T2 |
| DO-RISK-03 | Risk | NP-FMEA-001 — SW-01 Safety MCU Unit-Level FMEA (SW01-M01..M08: tDCS ramp, watchdog, charge density, thermal, CVNS interlock, impedance, Ed25519, fault latch) | Rev A | Active | T1+T2 |
| DO-REG-01 | Regulatory | NP-REG-PBM1064-001 — RISK-03 Scope Expansion Brief to outside regulatory counsel (12 questions: 1064 nm FDA pathway, aggregate irradiance, T2 combined session, FTC depth claim) | Rev A | Active | T1+T2 |
| DO-REG-02 | Regulatory | NP-REG-CVNS-001 — Cervical VNS 510(k) Pre-Submission Q-Sub (predicate: electroCore gammaCore K163334/K173323; SE argument; 6 Q-Sub questions; Type B meeting Month 20) | Rev A | Active | T2 |
| DO-TEST-01 | Test | NP-FAI-ZM-001 — Zone Module FAI Checklist (PDMS adhesion FAI-M01..M03, thermal cycling FAI-TC01..TC06 BLOCKING, accessibility FAI-A09..A15, IPX4 FAI-IPX-01..04 BLOCKING, lifecycle, system test) | Rev A | Active | T1+T2 |
| DO-TEST-02 | Test | NP-COORD-001 — Engineering Coordination Checklist (G1 15 items, G2 14 items, G3 6 items gate structure; G2-10/11/12/14 closed; G3-07/08 software baselined) | Rev A.8 | Active | T1+T2 |
| DO-TEST-03 | Test | NP-DRV-SHELL-001 — Shell FPC Routing Review (DRC 23-item checklist; EEG cable routing §2.4; DRC-18/22/23 CLOSED; 8×5 mm channel locked routing) | Rev B | Active | T1+T2 |

---

## §5 Traceability Matrix

| DI-ID | Input (brief) | DO-ID(s) | Output (brief) | VE-ID(s) | Status |
|-------|---------------|----------|----------------|----------|--------|
| DI-PERF-01 | 8-ch EEG 500 Hz 24-bit ADS1299 | DO-HW-01, DO-FW-08, DO-SW-07, DO-SW-09 | FPC spec; hub control; iOS app | VE-05 | Partial |
| DI-PERF-02 | 21-ch qEEG (T2) full 10-20 | DO-HW-01, DO-FW-05, DO-SW-04 | FPC spec; HD-tDCS fw; sLORETA source | VE-06 | Open |
| DI-PERF-03 | 660/808nm on hex-tiled T1-A modules (NP-HEX-ZM-001) | DO-HW-01, DO-HW-04, DO-HW-06 | FPC; zone module tooling; shell tooling | VE-07 | Partial |
| DI-PERF-04 | 400 mW/cm² pulsed ≤25% DC | DO-HW-01, DO-FW-07, DO-RISK-01, DO-REG-01 | FPC; 1064 nm fw duty ceiling; risk register; reg opinion | VE-01 | Partial |
| DI-PERF-05 | Real-time J/cm² metering dual-PD | DO-HW-01, DO-FW-07, DO-SW-06 | FPC dual-PD RISK-14; 1064 nm fw dose accumulator; source | VE-01, VE-07 | Partial |
| DI-PERF-06 | 1064 nm smart module ATtiny402 | DO-HW-01, DO-HW-02, DO-HW-05, DO-FW-07, DO-SW-06 | FPC Rev E; Hub PCB Rev B TIA switch; mould variant; fw spec | VE-07 | Partial |
| DI-PERF-07 | BES/tACS ≤1 mA 0.5–40 Hz (T1) | DO-FW-08, DO-SW-07, DO-RISK-02 | Hub control session runner; risk plan | VE-05 | Partial |
| DI-PERF-08 | Clinical tACS ≤4 mA 16-ch (T2) | DO-FW-08, DO-SW-07, DO-RISK-02 | Hub control; risk plan | VE-05 | Partial |
| DI-PERF-09 | tDCS 0.1–2 mA ≤3 pairs | DO-FW-08, DO-SW-07, DO-RISK-01 | Hub control; risk register | VE-05 | Partial |
| DI-PERF-10 | Auricular VNS + PPG HRV A1/A2 ref | DO-FW-03, DO-SW-02 | HRV fw spec np_hrv_tavns_sync; source | VE-04 | Traced |
| DI-PERF-11 | Cervical VNS (T2) ≤2 mA ramp | DO-FW-06, DO-SW-05, DO-REG-02 | CVNS fw; source; 510(k) Q-Sub | VE-03 | Partial |
| DI-PERF-12 | TMS focal figure-8 coil (T2) | DO-FW-08, DO-SW-07, DO-HW-07 | Hub control; lens tooling CFRP window | VE-06 | Open |
| DI-PERF-13 | 1170 nm deep PBM laser TEC (T2) | DO-FW-07, DO-SW-06 | 1064+1170 nm T2 combined session | VE-07 | Partial |
| DI-PERF-14 | sLORETA HD-tDCS 7 targets (T2) | DO-FW-05, DO-SW-04 | HD-tDCS fw spec; sLORETA source | VE-06 | Partial |
| DI-PERF-15 | Audio planar + bone conduction | DO-FW-08, DO-SW-07, DO-SW-09 | Hub control; iOS app | VE-05 | Partial |
| DI-PERF-16 | Visual 108 LEDs 0.5–100 Hz EMDR | DO-HW-07, DO-FW-08, DO-SW-07 | Lens tooling; hub control | VE-08 | Partial |
| DI-PERF-17 | Autonomous Mode 3 closed-loop | DO-FW-08, DO-SW-07 | Hub control session runner NPPS Ed25519 | VE-05 | Partial |
| DI-PERF-18 | 4 operating modes <1 ms / <5 s | DO-FW-01, DO-FW-08, DO-SW-01, DO-SW-07 | eMMC spec; hub control; bootloader | VE-02, VE-05 | Partial |
| DI-PERF-19 | UHDR/SHDR AES-256 biometric key | DO-FW-01, DO-FW-02, DO-SW-01, DO-SW-08, DO-SW-09 | eMMC spec; priv delta UKMD/WKMD; bootloader; anon; iOS | VE-09 | Partial |
| DI-PERF-20 | 8 GB eMMC 9-partition LittleFS | DO-FW-01, DO-SW-01 | eMMC spec Rev A §4; bootloader source | VE-02 | Traced |
| DI-PERF-21 | Dual-bank OTA Ed25519 + DFU | DO-FW-01, DO-SW-01 | eMMC spec §8 9-step; bootloader source | VE-02 | Traced |
| DI-PERF-22 | 5-layer EMF + active Helmholtz | DO-HW-06, DO-RISK-01 | Shell tooling palladium fabric; risk register RISK-10 | VE-08 | Partial |
| DI-PERF-23 | Boa dial 50,000-cycle PTFE | DO-HW-06, DO-HW-04 | Shell tooling F-03 channel; ZM tooling | VE-08 | Partial |
| DI-PERF-24 | Zone modules field-upgradeable Hirose FH34S | DO-HW-04, DO-HW-05, DO-FW-04, DO-SW-03 | ZM tooling RISK-22 eject lever; zone announce fw | VE-07 | Partial |
| DI-PERF-25 | HRV biofeedback 4 protocols coherence | DO-FW-03, DO-SW-02 | HRV fw spec; source G2-12 CLOSED | VE-04 | Traced |
| DI-SAFE-01 | Charge density 40 µC/cm² safety MCU HW | DO-FW-10, DO-RISK-01, DO-RISK-02, DO-RISK-03 | FMEA SW01-M03; risk register; risk plan | VE-01, VE-10 | Partial |
| DI-SAFE-02 | SPI heartbeat ≤50 ms watchdog | DO-FW-08, DO-FW-10, DO-SW-07, DO-RISK-03 | Hub safety SPI np_hub_safety_spi; FMEA SW01-M02 | VE-01, VE-10 | Partial |
| DI-SAFE-03 | CVNS cardiac interlock ≤100 ms | DO-FW-06, DO-FW-10, DO-SW-05, DO-RISK-03 | CVNS fw TIM6 ISR; FMEA SW01-M05; source | VE-03, VE-10 | Partial |
| DI-SAFE-04 | Photoparoxysmal Oz ≤200 ms | DO-FW-08, DO-HW-07, DO-SW-07 | Hub control; lens tooling Hall sensor, IR proximity | VE-05, VE-08 | Partial |
| DI-SAFE-05 | IEC 62471 visual MPE 3 HW layers | DO-HW-07, DO-FW-08, DO-RISK-01 | Lens tooling IR+Hall+current; hub control; risk register | VE-08 | Partial |
| DI-SAFE-06 | Ed25519 session signature gate | DO-FW-01, DO-FW-10, DO-SW-01, DO-SW-07 | eMMC spec §8; FMEA SW01-M07; bootloader; hub runner | VE-02, VE-10 | Partial |
| DI-SAFE-07 | tDCS 30 s ramp HW-enforced | DO-FW-08, DO-FW-10, DO-SW-07 | Hub control; FMEA SW01-M01; source | VE-05, VE-10 | Partial |
| DI-SAFE-08 | Thermal 62 °C throttle / 65 °C cutoff | DO-FW-08, DO-FW-10, DO-SW-07, DO-RISK-01 | Hub control NTC; FMEA SW01-M04; risk register RISK-07 | VE-05, VE-10 | Partial |
| DI-SAFE-09 | Fault latch + explicit clear required | DO-FW-10, DO-SW-07, DO-SW-09 | FMEA SW01-M08; hub control; iOS confirmation flow | VE-10 | Partial |
| DI-SAFE-10 | TMS EMF gating 5 ms / 50 ms + CFRP | DO-HW-07, DO-FW-08, DO-RISK-01 | Lens tooling CFRP window; hub control; risk register RISK-12 | VE-06 | Open |
| DI-SAFE-11 | Mode F default-off + triple-pulse LED | DO-FW-02, DO-SW-09 | FW priv delta §F regulatory gate flag; iOS consent screen | VE-09 | Partial |
| DI-SAFE-12 | 1 kHz AC impedance synchronous | DO-FW-08, DO-FW-10, DO-SW-07 | Hub control; FMEA SW01-M06; source | VE-05, VE-10 | Partial |
| DI-SAFE-13 | Face ≤42 °C under fan/heatsink loss; Path B1 NTC + SR-FAN derate; BN-boss export | DO-FW-08, DO-FW-10 (SW01-M04 ext.), DO-RISK (FMEA-G07-01) | NP-REQ-FANHEALTH-001 §4a Path B1; NP-THERM-CFD-R1-001 (Path A rejected, SR-FAN constants); FMEA-G07-01; THERM-1b bench + verification-grade CFD pending | VE-05, VE-10 | Open |
| DI-USE-01 | 52–62 cm 1 adult SKU 5-pos bridge | DO-HW-06, DO-HW-04 | Shell tooling; ZM tooling | VE-08 | Partial |
| DI-USE-02 | ≤1 N eject lever Parkinson's target | DO-HW-04, DO-RISK-01 | ZM tooling F-06 eject lever RISK-22; risk register | VE-07 | Partial |
| DI-USE-03 | Amber LED ≥3 m caregiver readable | DO-FW-08, DO-SW-07 | Hub control §4.7 LED driver | VE-05 | Partial |
| DI-USE-04 | Covers tethered anchor posts | DO-HW-06, DO-HW-07 | Shell tooling F-01/F-02 anchor posts; lens tooling F-07 hook | VE-08 | Partial |
| DI-USE-05 | 5-layer zone module keying RISK-15 | DO-HW-04, DO-FW-04, DO-SW-03 | ZM tooling RISK-15 all 5 layers; zone announce fw source | VE-07 | Traced |
| DI-USE-06 | EEG pods 80–120 g ±12 mm travel | DO-HW-06, DO-HW-04 | Shell tooling spring pod seat; ZM tooling | VE-08 | Partial |
| DI-USE-07 | Hydrogel tips snap-off bayonet 30–60 sessions | DO-HW-04, DO-SW-09 | ZM tooling bayonet seat; iOS consumable tracker | VE-07 | Partial |
| DI-REG-01 | IEC 60601-1 general safety | DO-RISK-02, DO-TEST-01, DO-TEST-02 | Risk plan; FAI checklist; coord checklist | VE-11 | Open |
| DI-REG-02 | IEC 60601-2-10 stimulator requirements | DO-RISK-02, DO-REG-02 | Risk plan; CVNS 510(k) Q-Sub | VE-11 | Open |
| DI-REG-03 | IEC 62471 photobiological safety | DO-REG-01, DO-RISK-02 | RISK-03 reg opinion brief; risk plan | VE-01, VE-11 | Partial |
| DI-REG-04 | IEC 62133 battery | DO-RISK-02 | Risk plan hazard category 6 | VE-11 | Open |
| DI-REG-05 | FCC Part 15 RF emissions | DO-HW-06, DO-RISK-02 | Shell tooling antenna separation; risk plan | VE-11 | Open |
| DI-REG-06 | IEC 62304 SW lifecycle Class C/B | DO-FW-10, DO-RISK-03 | FMEA SW-01 unit-level; NP-SW-001 | VE-10, VE-11 | Partial |
| DI-REG-07 | IEC 62366-1 HFE URRA + formative | DO-TEST-02 | Coord checklist G2 HFE items (formative Month 9) | VE-12 | Open |
| DI-REG-08 | ISO 14971 risk management | DO-RISK-01, DO-RISK-02, DO-RISK-03 | Risk register + risk plan + FMEA SW-01 | VE-10 | Traced |
| DI-REG-09 | 21 CFR Part 820 QSR | DO-RISK-02, DO-TEST-02 | Risk plan; coord checklist gate structure | VE-11 | Open |
| DI-REG-10 | FDA cybersecurity OTA Ed25519 | DO-FW-01, DO-SW-01 | eMMC spec §8 signing; bootloader np_signature.c | VE-02 | Partial |
| DI-REG-11 | HIPAA UHDR biometric key | DO-FW-01, DO-FW-02, DO-SW-08, DO-SW-09 | eMMC spec; priv delta UKMD/WKMD; anon fw; iOS key mgr | VE-09 | Partial |
| DI-REG-12 | GDPR Art. 13(2)(f) adaptive transparency | DO-FW-08, DO-SW-07, DO-SW-09 | Hub np_adaptation_log.h; hub control; iOS AdaptiveAdjustmentsCard | VE-09 | Traced |
| DI-INT-01 | USB-C PD any-charger compliance | DO-HW-02, DO-FW-08 | Hub PCB Rev B PD negotiation; hub control power mgr | VE-05 | Partial |
| DI-INT-02 | BT 5.3 LE Audio GATT custom service | DO-SW-09 | iOS app BLE GATT session service | VE-05 | Partial |
| DI-INT-03 | FHIR R4 NP T2 Clinical Profile | DO-FHIR-01, DO-API-01 | FHIR IG NP-Patient/Observation/Report; API spec | VE-06 | Partial |
| DI-INT-04 | Apple Watch provisional 40 Hz haptic | DO-SW-09 | iOS Watch bridge (provisional OI-WA-02 gate) | VE-13 | Open |
| DI-INT-05 | LSL streaming TLS (T2) | DO-FHIR-01, DO-API-01 | FHIR IG §5 LSL endpoint; API spec §4 | VE-06 | Open |

---

## §6 Verification Evidence Registry

| VE-ID | Evidence Type | Document / Test | Status | Gate |
|-------|---------------|-----------------|--------|------|
| VE-01 | Regulatory opinion | NP-REG-PBM1064-001 Rev A — RISK-03 outside counsel opinion letter; 12 questions on irradiance, aggregate IEC 62471, T2 combined 1064+1170 nm, FTC depth claims | Open (external) | G1 close |
| VE-02 | Software PASS | NP-FW-EMMC-001 Rev A §8 + firmware/bootloader/ — OTA 9-step sequence, Ed25519 RFC 8032 self-contained, DFU 1.1 USB class, rollback after 3 failed attempts | Complete | — |
| VE-03 | Software PASS | NP-FW-CVNS-001 Rev A FAI-CV02 software constants — TIM6 ISR ≤5.1 ms worst-case; cardiac interlock timing PASS on paper analysis; hardware oscilloscope bench pending | Partial (HW bench pending) | G3 |
| VE-04 | Software PASS | NP-FW-HRV-001 Rev A — 6-module HRV library PASS; coherence Welch PSD algorithm verified; taVNS RSA slope PASS; G2-12 CLOSED | Complete | — |
| VE-05 | Software PASS | NP-FW-HUB-001 Rev A G2-14 PARTIAL — hub control session runner, module registry, session log, safety SPI heartbeat; FAI-HUB-01..03 hardware bench pending | Partial | G2 |
| VE-06 | Software PASS | NP-FW-HD-001 Rev A FAI-HD02 — sLORETA electrode mapping software PASS all 6 criteria; FAI-HD01 phantom localisation ≤15 mm and FAI-HD03 FWHM ≤25 mm hardware bench pending | Partial (phantom bench pending) | G3 |
| VE-07 | FAI Checklist | NP-FAI-ZM-001 Rev A — zone module FAI; PDMS 200-cycle IEC 60068-2-14 thermal cycling BLOCKING (FAI-TC02); IPX4 after 10 swap cycles BLOCKING (FAI-IPX-02); eject lever ≤1 N FAI-A09 pending | Open | G2 |
| VE-08 | Design Review | NP-COORD-001 Rev A.8 G2 items — shell tooling NP-TOOL-SHELL-001 Rev A; lens tooling NP-TOOL-LENS-001 Rev B; G2-11 CLOSED (EEG routing); G2-13 OPEN (lens G2 deliverable) | Partial | G2 |
| VE-09 | Privacy Audit | NP-PRIV-ANALYSIS-002 Rev A — iOS app second-pass audit 12 findings all resolved; GDPR Art. 13(2)(f) adaptive trigger enum and disclosure; OI-PA-04 copy sign-off open | Complete (OI-PA-04 process item) | — |
| VE-10 | FMEA | NP-FMEA-001 Rev A — SW-01 Safety MCU FMEA SW01-M01..M08 complete; OI-FMEA-01 watchdog GPIO timing hardware bench and OI-FMEA-02 cardiac interlock oscilloscope bench open | Complete (HW bench OI items open) | G3 |
| VE-11 | Standards testing | IEC 60601-1, IEC 60601-2-10, IEC 62471, IEC 62133, FCC Part 15 at accredited laboratory; planned G3 phase Month 10–14 | Open | G3 |
| VE-12 | HFE Testing | NP-HFE-001 (planned Month 9) — URRA; formative testing including eject lever Parkinson's study 5 subjects; summative testing Month 14–18 | Open | G3 |
| VE-13 | Watch App testing | OI-WA-02 — Watch 40 Hz visual flicker brightness characterisation; ≥100 nits at 40 Hz required before phase 4 Watch app development begins | Open | Post-G3 |

---

## §7 Traceability Gaps and Risk

### §7.1 Safety-critical gaps (BLOCKING for G3 closure and 510(k) submission)

| Gap | Affected DIs | Open VE | Action |
|-----|--------------|---------|--------|
| RISK-03 regulatory opinion not received | DI-PERF-04, DI-REG-03 | VE-01 | Outside counsel engagement active; 3–5 weeks ETA from engagement |
| Hardware bench items OI-FMEA-01 (watchdog timing) and OI-FMEA-02 (cardiac interlock oscilloscope) | DI-SAFE-02, DI-SAFE-03 | VE-10 | Blocked until T2 hardware prototype; G3 gate prerequisite |
| Standards lab testing (IEC 60601, IEC 62471, FCC Part 15) | DI-REG-01/02/03/04/05/09 | VE-11 | Planned Month 10–14; accredited lab selection target Month 9 |
| sLORETA HD-tDCS phantom bench (FAI-HD01, FAI-HD03, FAI-HD04) | DI-PERF-14, DI-SAFE-03 | VE-06 | Requires T2 prototype; G3-07 hardware gate |
| CVNS cardiac interlock oscilloscope bench (FAI-CV02) | DI-SAFE-03, DI-PERF-11 | VE-03 | Requires oscilloscope + R-peak signal generator bench; G3-08 hardware gate |
| Zone module PDMS thermal cycling and IPX4 FAI (BLOCKING items) | DI-PERF-05, DI-USE-02 | VE-07 | FAI-TC02 and FAI-IPX-02 BLOCKING for production start; supplier engagement required |
| HFE testing — URRA, formative, summative | DI-REG-07 | VE-12 | NP-HFE-001 plan Month 9; formative study Month 10–12; summative Month 14–18 |

### §7.2 Non-blocking gaps (managed, no G1/G2/G3 gate impact)

| Gap | Affected DIs | Notes |
|-----|--------------|-------|
| Apple Watch provisional gated on HOPE Phase 3 | DI-INT-04 | Not required for T1 launch; OI-WA-02 brightness test open |
| LSL streaming T2 roadmap | DI-INT-05 | No T1 blocker; T2 development roadmap |
| T2 qEEG and TMS | DI-PERF-02, DI-PERF-12 | T2 platform only; not required for T1 launch |
| OI-PA-04 Privacy Lead copy sign-off on AdaptiveAdjustmentsCard | DI-REG-12 | Process gate only; code complete; must not ship without sign-off |
| BIPA legal opinion (biometric data) | DI-PERF-19, DI-REG-11 | Biometric written-release protection ships to ALL users (universal, 2026-07-10); the legal opinion confirms scope/possession analysis and does NOT gate activation or T1 design |

---

## §8 Summary Statistics

| Metric | Count |
|--------|-------|
| Total design inputs (DIs) | 58 |
| — Safety-Critical (DI-SAFE) | 12 |
| — High priority (DI-PERF + DI-USE + DI-INT) | 30 |
| — Medium priority | 11 |
| — Low / Provisional | 2 |
| — Regulatory (DI-REG) | 12 |
| Total design outputs (DOs) | 35 |
| — Hardware (DO-HW) | 7 |
| — Firmware (DO-FW) | 10 |
| — Software source (DO-SW) | 9 |
| — Integration + risk + regulatory + test (DO-API/FHIR/RISK/REG/TEST) | 9 |
| Total verification evidence (VEs) | 13 |
| — Complete | 4 (VE-02, VE-04, VE-09, VE-10) |
| — Partial (DO exists, VE hardware/lab pending) | 7 (VE-01, VE-03, VE-05, VE-06, VE-07, VE-08, VE-11) |
| — Open (VE not yet started) | 2 (VE-12, VE-13) |
| Traced DI chains | 7 |
| Partial DI chains | 42 |
| Open DI chains | 8 |

---

## §9 DHF Completeness Impact

With NP-DT-001 Rev A authored, the DHF design inputs completeness rating (previously **"Partial"** per NP-DHF-001 §7) is upgraded to **"Good"**.

**DHF completeness progression:**

| Phase | Event | Completeness |
|-------|-------|-------------|
| Pre-NP-DT-001 | DHF Rev F design inputs rating | Partial |
| NP-DT-001 Rev A issued | 57 DIs documented; DI→DO→VE chains established | Good |
| NP-HFE-001 Rev A (Month 9) | URRA complete; VE-12 entered | Good+ |
| Formative HFE testing (Month 10–12) | First VE-12 evidence | Good+ |
| Standards lab testing (G3) | VE-11 results entered | Near-Complete |
| Summative HFE testing + clinical validation (Month 14–18) | VE-12 complete | Complete |

Remaining requirements to reach **"Complete"**:
1. NP-HFE-001 Human Factors Engineering Plan → adds DO and partially completes VE-12
2. HFE formative testing → confirms DI-USE-01 through DI-USE-07 user needs coverage
3. HFE summative testing → fully completes VE-12 and closes DI-REG-07 to Traced
4. IEC 60601/62471/62133/FCC Part 15 accredited lab testing → completes VE-11 and closes all DI-REG-01/02/04/05 to Traced
5. RISK-03 opinion letter → closes VE-01 and traces DI-PERF-04, DI-REG-03

---

## §10 Open Items

| ID | Item | Owner | Target |
|----|------|-------|--------|
| OI-DT-01 | Validate DO-ID list against NP-DHF-001 after each new document release; update DI/DO registry and traceability matrix; mark closed items in revision history | Quality Lead | Rolling |
| OI-DT-02 | Enter VE-11 accredited lab test results when G3 standards testing completes; update all DI-REG-01/02/04/05/09 status to Traced | Quality Lead | G3 (Month 10–14) |
| OI-DT-03 | Author NP-HFE-001 Human Factors Engineering Plan; add as DO entry; update VE-12 status to Partial when plan is complete | Quality Lead | Month 9 |
| OI-DT-04 | Update DI-PERF-04 and DI-REG-03 from Partial to Traced once RISK-03 regulatory opinion letter received from outside counsel | Regulatory Lead | G1 close |
| OI-DT-05 | Confirm Apple Watch app DI-INT-04 scope: update from provisional/low to committed/medium if HOPE Phase 3 positive and Watch app moves to committed roadmap | Engineering | Month 12 |
| OI-DT-06 | Enter VE-07 FAI-TC02 (PDMS thermal cycling) and FAI-IPX-02 (IPX4 swap cycles) when PDMS CAT-C supplier qualification complete and 200-cycle test passes | Quality Lead | Pre-production |
| OI-DT-07 | Enter VE-03 hardware bench result (FAI-CV02 oscilloscope) and VE-06 phantom bench results (FAI-HD01/HD03/HD04) once T2 prototype hardware is available | Engineering | G3 hardware bench |
| OI-DT-08 | Update NP-DHF-001 to Rev G to reflect NP-DT-001 Rev A as active DHF document; update design inputs completeness rating from Partial to Good | Quality Lead | Within 5 business days of NP-DT-001 approval |

---

## §11 Change Control

This document is under QMS change control per NP-QMS-DC-001 Rev A §8.3.

**Significant changes** (requiring full Quality Lead review and DHF update):
- Addition of new design inputs (new DI-ID entries)
- Deletion of design inputs (requires documented justification)
- Change in safety classification of any DI-SAFE entry
- Change in scope (T1 vs T2 reclassification)
- Any change that affects a currently Open or Partial safety-critical chain

**Minor changes** (change notification only):
- Status updates (Partial → Traced) when VE evidence is received
- Addition of new DO-IDs for newly authored documents
- Open item closure with documented evidence
- Typographical corrections

All changes must update the §10 revision history entry.

---

## §12 Revision History

| Rev | Date | Author | Change Summary |
|-----|------|--------|----------------|
| A | 2026-06-07 | SmartyPants / PAI | Initial issue — G1 gate deliverable; 57 DIs across PERF/SAFE/USE/REG/INT; 35 DOs covering HW/FW/SW/Risk/Reg/Test; 13 VEs; full DI→DO→VE traceability matrix; §7 gap analysis; §8 statistics; §9 DHF completeness impact upgraded from Partial to Good |
| B | 2026-07-22 | Steve Hickman (CEO, interim Quality authority) | **DI-SAFE-13 added** — scalp-facing surface ≤42 °C under single-fault loss of forced convection (fan/heatsink loss): Path B1 scalp-facing NTC at PD2 (SR-FAN-01/02) + natural-convection-safe duty derate (SR-FAN-03) + ambient/duty envelope gate; **base thermal design = BN-boss conductive export to an external heatsink, shielded interior un-ventilated.** Records the THERM-1a first-pass outcome (NP-THERM-CFD-R1-001: DI-SAFE-08 junction throttle proven insufficient to bound the face → Path A rejected). Added to §3.2 registry + §5 DI→DO→VE matrix (status Open — SW01-M04 face-NTC implementation + THERM-1b verification pending); DI total 57 → 58. Traces to NP-REQ-FANHEALTH-001, NP-FMEA-GEOM-001 (FMEA-G07-01), NP-DHF-001 Rev V. Rev A → B. |
