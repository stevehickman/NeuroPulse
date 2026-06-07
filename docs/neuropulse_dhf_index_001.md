# NP-DHF-001 Rev L — NeuroPulse Design History File Index

**Document number:** NP-DHF-001  
**Revision:** L  
**Status:** ACTIVE  
**Effective date:** 2026-06-07  
**Author:** Steve Hickman (CEO, interim Quality authority)  
**Approved by:** Steve Hickman, CEO  
**Next review:** Ongoing — updated with each new design document release

---

## 1. Purpose

This Design History File (DHF) Index serves as the master index of all design documentation for the NeuroPulse device platform under 21 CFR §820.30(j) and ISO 13485:2016 clause 7.3.10. It provides a single reference point to locate any design record and demonstrates that the device was designed in accordance with the approved design plan.

Relative links in the File column are navigable — click any link to open the source document directly. Links are relative to the `docs/` directory. All links assume the repository directory structure is unchanged; if the structure changes, links in this index must be updated.

The DHF Index is a living document — each new controlled document is added at release, and each revision is recorded.

---

## 2. DHF Scope

The DHF covers both device tiers sharing the NeuroPulse platform:

| Tier | Product | Regulatory pathway |
|---|---|---|
| T1 | NeuroPulse Home | FDA-exempt wellness (general wellness device) |
| T2 | NeuroPulse Pro | FDA 510(k) clearance target |

Because T1 design decisions directly feed into T2 (shared chassis, processor stack, firmware architecture, and modality hardware), all T1 design records are included in the DHF as T2 design history.

---

## 3. Initial Entry Declaration

All documents listed in this index that predate the formal QMS effective date of **2026-05-13** are hereby entered retroactively into the Design History File under change control as of that date. Their content represents design decisions made during the pre-formation design phase. No design decision or specification in those documents is considered closed to revision — all remain subject to the design controls procedure (NP-QMS-DC-001) from this date forward.

Change description for all initial-entry documents: **"Initial DHF entry — retroactive entry of pre-formation design document at QMS establishment (NP-QMS-001 Rev A effective 2026-05-13)"**

---

## 4. Document Categories

| Category | Description |
|---|---|
| REQ | Design inputs and requirements |
| SPEC-HW | Hardware specifications |
| SPEC-FW | Firmware specifications |
| SPEC-TOOL | Tooling and manufacturing specifications |
| RISK | Risk management records |
| FAI | First Article Inspection and test records |
| PROC | Procurement and supplier qualification |
| COORD | Engineering coordination and gate records |
| REG | Regulatory strategy documents |
| CLIN | Clinical strategy and evidence |
| QMS | Quality Management System procedures |
| SES | Session protocol specifications |
| APP | Application and software roadmap |
| SIM | Simulation and visualisation tools |
| PRIV | Privacy analysis, remediation, and data protection |
| SEC | Security procedures and incident response |

---

## 5. Master Document Index

### 5.1 QMS and Quality Documents

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-QMS-001 | QMS Manual | A | 2026-05-13 | [neuropulse_qms_manual_001.md](./neuropulse_qms_manual_001.md) | ACTIVE | QMS |
| NP-DHF-001 | Design History File Index (this document) | F | 2026-06-02 | [neuropulse_dhf_index_001.md](./neuropulse_dhf_index_001.md) | ACTIVE | QMS |
| NP-QMS-DC-001 | Design Controls Procedure | A | 2026-05-13 | [neuropulse_design_controls_001.md](./neuropulse_design_controls_001.md) | ACTIVE | QMS |
| NP-RM-001 | ISO 14971 Risk Management Plan | A | 2026-05-13 | [neuropulse_risk_mgmt_plan_001.md](./neuropulse_risk_mgmt_plan_001.md) | ACTIVE | RISK |
| NP-SW-001 | IEC 62304 Software Development Plan | A | 2026-05-13 | [neuropulse_sw_dev_plan_001.md](./neuropulse_sw_dev_plan_001.md) | ACTIVE | QMS |
| NP-QMS-CAPA-001 | CAPA Procedure | A | 2026-05-13 | [neuropulse_capa_001.md](./neuropulse_capa_001.md) | ACTIVE | QMS |
| NP-DP-001 | Design and Development Plan | A | 2026-05-17 | [neuropulse_dp_001.md](./neuropulse_dp_001.md) | ACTIVE | QMS |
| NP-DT-001 | Design Input/Output Traceability Matrix — 57 design inputs (DI-PERF/SAFE/USE/REG/INT), 35 design outputs, 13 verification evidence entries; full DI→DO→VE traceability matrix; DHF completeness design-inputs row upgraded from Partial to Good; 5 open items (OI-DT-01..05); G2 exit criterion per NP-DP-001 §6.4 | A | 2026-06-07 | [neuropulse_dt_001.md](./neuropulse_dt_001.md) | ACTIVE | QMS |

### 5.2 Design Briefs and Product Specifications

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-DB-001 | Design Brief | 1 | 2026-05-02 | [neuropulse_design_brief_superseded.docx](./neuropulse_design_brief_superseded.docx) | SUPERSEDED by NP-DB-005 | REQ |
| NP-DB-002 | Design Brief | 2 | 2026-05-03 | [neuropulse_design_brief_r2_superseded.docx](./neuropulse_design_brief_r2_superseded.docx) | SUPERSEDED by NP-DB-005 | REQ |
| NP-DB-003 | Design Brief | 3 | 2026-05-04 | [neuropulse_brief_r3_superseded.docx](./neuropulse_brief_r3_superseded.docx) | SUPERSEDED by NP-DB-005 | REQ |
| NP-DB-004 | Design Brief | 4 | 2026-05-07 | [neuropulse_brief_r4_superseded.docx](./neuropulse_brief_r4_superseded.docx) | SUPERSEDED by NP-DB-005 | REQ |
| NP-DB-005 | Master Design Brief | 5 | 2026-05-16 | [neuropulse_design_brief_r5.docx](./neuropulse_design_brief_r5.docx) | ACTIVE | REQ |
| — | CLAUDE.md — Project Design Memory | 22 | 2026-06-07 | [CLAUDE.md](../CLAUDE.md) | ACTIVE | REQ |

**Note on CLAUDE.md:** CLAUDE.md serves as the living design authority document capturing all locked design decisions and pending items. It is under git version control and constitutes a design record for DHF purposes. Each revision (tracked by git commit) is a controlled design change. Rev 22 (2026-06-07) adds: NP-FMEA-001 Rev A (SW-01 Safety MCU FMEA), NP-API-001 Rev A (T2 Scripting API), and NP-DT-001 Rev A (Design I/O Traceability Matrix) to §14 document register; SHDR warranty token PARTIAL status updated (app-side Keychain token in place, hub-provisioned TRNG pending); all NP-PRIV-ANALYSIS-002 code-level findings resolved.

### 5.3 Hardware Specifications

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-HW-FPC-001 | FPC Zone Module Specification (base module, ZM-01–ZM-05) | D | 2026-05-09 | [neuropulse_fpc_zone_module_spec_revA.docx](./neuropulse_fpc_zone_module_spec_revA.docx) | ACTIVE | SPEC-HW |
| NP-HW-FPC-001 (variant) | FPC Smart Zone Module (1064nm) — layout variant NP-FPC-ZM-SM-01 | E | 2026-05-13 | [neuropulse_hw_fpc_smart_001.md](./neuropulse_hw_fpc_smart_001.md) | ACTIVE | SPEC-HW |
| NP-HW-HUB-001 | Hub PCB Rev B — Vishay DG2788A TIA Gain Switch + NXP PCA9546A I2C Mux | B | 2026-05-13 | [neuropulse_hw_hub_pcb_revb_001.md](./neuropulse_hw_hub_pcb_revb_001.md) | ACTIVE | SPEC-HW |
| NP-PROC-FPC-001 | FPC Procurement Requirements | B | 2026-05-08 | [neuropulse_fpc_procurement_requirements.docx](./neuropulse_fpc_procurement_requirements.docx) | ACTIVE | PROC |
| NP-PROC-FPC-1064-001 | 1064nm Smart Module Component Procurement | A | 2026-05-12 | [neuropulse_proc_fpc_1064_001.md](./neuropulse_proc_fpc_1064_001.md) | ACTIVE | PROC |

### 5.4 Firmware Specifications

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-FW-EMMC-001 | eMMC Partition Architecture and Storage Encryption | A | 2026-05-11 | [neuropulse_fw_emmc_001.docx](./neuropulse_fw_emmc_001.docx) | ACTIVE — Rev B planned to incorporate NP-FW-EMMC-002 delta | SPEC-FW |
| NP-FW-EMMC-002 | Firmware Privacy Remediation Delta — §A warranty token, §B factory reset, §C two-layer UHDR key, §D Scratch encryption, §E EDF+ header policy, §F Mode F spec, **§G SHDR accelerometer reclassification** (added 2026-06-03: `drop_detected: bool` + `maintenance_alert: bool` only; raw accelerometer prohibited; fleet DB schema CI test OI-EMMC2-07 BLOCKING for schema freeze) | A | 2026-06-02 (§G: 2026-06-03) | [neuropulse_fw_emmc_002.md](./neuropulse_fw_emmc_002.md) | ACTIVE — supersedes conflicting sections of NP-FW-EMMC-001 Rev A; to be incorporated in NP-FW-EMMC-001 Rev B | SPEC-FW |
| NP-FW-ANON-001 | Research Anonymisation Engine Firmware Specification — on-device k-anonymity (k≥10) + l-diversity (l≥3) + differential privacy Laplace mechanism (ε≤1.0, δ≤10⁻⁵); study descriptor schema v1 with Ed25519 verification; prohibited element list (raw EEG waveforms, sub-week timestamps); per-study privacy budget tracking; output encrypted with study public key (NeuroPulse cannot read); 9 FAI tests (FAI-ANON-01 through FAI-ANON-09) including adversarial re-identification; 5 open items (OI-ANON-01 through OI-ANON-05) | A | 2026-06-03 | [neuropulse_fw_anon_001.md](./neuropulse_fw_anon_001.md) | ACTIVE — implementation pending; OI-ANON-01 (DP reviewer sign-off) BLOCKING for FAI-ANON-04/09 | SPEC-FW |
| NP-FW-HUB-001 | Hub Control Program — main SW-02 application firmware (module registry, session runner, telemetry, safety SPI) | A | 2026-05-16 | [../firmware/hub_control/](../firmware/hub_control/) | ACTIVE | SPEC-FW |
| NP-FW-HRV-001 | HRV Biofeedback Protocol Firmware Specification | A | 2026-05-11 | [neuropulse_fw_hrv_001.md](./neuropulse_fw_hrv_001.md) | ACTIVE | SPEC-FW |
| NP-FW-ZA-001 | Zone Module Bone Conduction Announcement Firmware | A | 2026-05-11 | [neuropulse_fw_zone_announce_001.md](./neuropulse_fw_zone_announce_001.md) | ACTIVE | SPEC-FW |
| NP-FW-HD-001 | sLORETA-Guided HD-tDCS Firmware Specification | A | 2026-05-11 | [neuropulse_fw_sloreta_hdtdcs_001.md](./neuropulse_fw_sloreta_hdtdcs_001.md) | ACTIVE | SPEC-FW |
| NP-FW-CVNS-001 | Cervical VNS Safety Interlock Firmware Specification | A | 2026-05-11 | [neuropulse_fw_cvns_001.md](./neuropulse_fw_cvns_001.md) | ACTIVE | SPEC-FW |
| NP-FW-PBM1064-001 | 1064nm Smart Zone Module Firmware Specification | A | 2026-05-12 | [neuropulse_fw_pbm1064_001.md](./neuropulse_fw_pbm1064_001.md) | ACTIVE | SPEC-FW |
| NP-FW-REQ-001 | Zone Module Firmware Requirements | A | 2026-05-10 | [neuropulse_fw_requirements_001_superseded.docx](./neuropulse_fw_requirements_001_superseded.docx) | SUPERSEDED by individual firmware specs (NP-FW-PBM1064-001, NP-FW-HRV-001, NP-FW-CVNS-001, NP-FW-HD-001, NP-FW-ZA-001, NP-FW-EMMC-001) | REQ |

**Note on NP-FW-EMMC-002:** Delta specification created by the privacy analysis programme. All sections take precedence over NP-FW-EMMC-001 Rev A. §G (SHDR accelerometer reclassification) was added 2026-06-03 by NP-PRIV-001 Rev B finding MEDIUM-06; it is BLOCKING for SHDR fleet DB schema freeze (OI-EMMC2-07 CI test must pass before schema is frozen). NP-FW-EMMC-001 Rev B will incorporate all delta sections and this document will then be marked INCORPORATED.

**Note on NP-FW-ANON-001:** This specification was created as a direct remediation of NP-PRIV-001 Rev A HIGH-02 (k-anonymity insufficient). It supersedes the informally-described anonymisation architecture in CLAUDE.md §5.3 for all firmware implementation purposes. OI-ANON-01 (external DP reviewer sign-off on ε and Δ values) is required before any firmware implementation begins and before FAI-ANON-04/09 can be executed.

### 5.5 Session, Protocol, and Application Specifications

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-SES-1064-001 | 1064nm Multi-Wavelength Session Protocol | A | 2026-05-12 | [neuropulse_session_protocol_1064_001.md](./neuropulse_session_protocol_1064_001.md) | ACTIVE | SES |
| NP-APP-ISA-001 | Core iOS App ISA — Ideal State Artifact for Issue #51 | E4 | 2026-06-04 | [../app/ios/ISA.md](../app/ios/ISA.md) | ACTIVE — 164 ISCs, 17 groups, PR #106 (feature/ios-parallel-integration). Three known validator safety gaps flagged as XCTExpectFailure: app-side charge density, PBM dose, zero-duration. OI-PA-01 (age gate legal threshold) open. | APP |
| NP-APP-ROADMAP-001 | iOS App Development Roadmap | B | 2026-06-03 | [neuropulse_ios_app_roadmap_001.md](./neuropulse_ios_app_roadmap_001.md) | ACTIVE — Rev B adds §9 Privacy Constraints (binding engineering constraints): HealthKit residency, minimum age gate (16+), BIPA written release screen for IL users, Adaptive Adjustments card, SDK init gate; OI-PA-01 OPEN; OI-PA-02/03 OPEN; OI-WA-06 OPEN. AgeGateView.swift now implemented (Issue #51, PR #106) — OI-PA-01 (legal counsel threshold confirmation) remains open. | APP |
| NP-APP-TELEMETRY-001 | App Analytics and Crash Reporting Policy | B | 2026-06-03 | [neuropulse_app_telemetry_001.md](./neuropulse_app_telemetry_001.md) | ACTIVE — Rev B: `session_sequence` (raw integer) replaced with `engagement_tier` (coarsened 3-bucket enum) per NP-PRIV-001 Rev B LOW-03; §3.2 implementation note added | APP |
| — | NPPS Protocol Scripting Language Reference | — | 2026-05-16 | [npps-reference.md](./npps-reference.md) | ACTIVE | APP |
| NP-API-001 | T2 Clinical Scripting API Specification — REST + WebSocket API (BAA required); UHDR three-condition access gate; SHDR-only default; 8 REST endpoints; rate limits (1,000/hr, 10,000/day); 256-bit key / 90-day expiry; audit log; 4 subscription tiers (Monitor $49, Assess $149, Full Clinical $299, Research $599); 7 open items (OI-API-01..07); G1 gate item per NP-PRIV-REM-001 STEP-15 | A | 2026-06-07 | [neuropulse_api_001.md](./neuropulse_api_001.md) | ACTIVE — independent security audit (NP-SEC-PENTEST-002) required before any clinical API key is issued | APP |
| NP-SIM-001 | Helmet Simulator — interactive 3D browser visualisation | v0.3.0 | 2026-05-17 | [../simulator/](../simulator/) | ACTIVE — Issue #81 / PRs #76, #84, #85 open; #79 CLOSED | SIM |

**Note on NP-APP-ROADMAP-001 Rev B:** The §9 Privacy Constraints added in Rev B are binding engineering constraints enforceable under NP-QMS-DC-001. They cannot be overridden without a formal design change order with Privacy Lead sign-off. Open items OI-PA-01 through OI-PA-04 must be resolved before the corresponding features ship.

**Note on NP-SIM-001 v0.3.0:** The helmet simulator is a software design output (browser-based Three.js application). It is not a DHF record in the medical device regulatory sense — it is a design tool, marketing asset, and protocol development aid. It is listed here for completeness as a versioned design output under git control. The simulator source is at `simulator/` in the repository root. v0.1.0 (PR #76): initial 3D scene, session engine, 9 NPPS protocol templates. v0.2.0 (PR #84, PR #85): WebSocket device API (Node.js server ws://localhost:9000); intranasal Y-probe animated insertion/removal; ACCESSORY_CONFIG message type; HOWTO v2.1 (Issues #77, #78 CLOSED). v0.3.0 (branch claude/add-t2-tms-coil-dLbh7): T2 TMS focal figure-8 coil added at DLPFC_L position; CFRP non-conductive window ring; blue-white pulse animation at rTMS rep rate; TMS modality pill in status bar; HOWTO v2.2 (Issue #79 CLOSED). Open sub-issue: #80 (geometry update pending shell CAD finalisation).

### 5.6 Tooling and Manufacturing Specifications

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-TOOL-ZM-001 | Zone Module Tooling Specification | A | 2026-05-06 | [neuropulse_tool_zone_module_001.docx](./neuropulse_tool_zone_module_001.docx) | ACTIVE | SPEC-TOOL |
| NP-TOOL-ZM-SM-001 | 1064nm Smart Zone Module Tooling Variant | A | 2026-05-12 | [neuropulse_tool_zone_module_smart_001.md](./neuropulse_tool_zone_module_smart_001.md) | ACTIVE | SPEC-TOOL |
| NP-TOOL-SHELL-001 | Shell Tooling Specification | A | 2026-05-10 | [neuropulse_tool_shell_001.docx](./neuropulse_tool_shell_001.docx) | ACTIVE | SPEC-TOOL |
| NP-TOOL-LENS-001 | Lens and Goggle Assembly Tooling Specification | B | 2026-05-10 | [neuropulse_tool_lens_001.docx](./neuropulse_tool_lens_001.docx) | ACTIVE | SPEC-TOOL |

### 5.7 Risk Management Records

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-RISK-001 | Zone Module Risk Register (RISK-01 through RISK-25) | B | 2026-05-17 | [neuropulse_fpc_zone_module_risks_revA.docx](./neuropulse_fpc_zone_module_risks_revA.docx) | ACTIVE | RISK |
| NP-FMEA-001 | SW-01 Safety MCU Unit-Level FMEA — IEC 62304 §7.1 Class C analysis for SW01-M01..M08; 43 failure modes across GPIO management, SPI heartbeat watchdog, charge density monitor, thermal interlock, cervical VNS cardiac interlock, impedance check, session signature verification, fault latch; ISO 14971 S×P scoring; two initial UNACCEPTABLE risks mitigated to ACCEPTABLE (FMEA-M03-01 charge accumulator overflow → 64-bit uint64_t; FMEA-M05-02 HR delta underflow → int16_t + MISRA C:2012 Rule 10.1); 5 open items OI-FMEA-01..05 (OI-FMEA-01/02 hardware bench pending G2 prototype) | A | 2026-06-07 | [neuropulse_sw_fmea_001.md](./neuropulse_sw_fmea_001.md) | ACTIVE — OI-FMEA-01 (watchdog GPIO bench ≤50ms) and OI-FMEA-02 (cardiac interlock bench <5.1ms) require G2 prototype; OI-FMEA-05 to be updated after FAI-CV02 hardware bench | RISK |

**Note:** The risk register (RISK-01 through RISK-25; 23 MITIGATED, 2 OPEN: RISK-03 regulatory opinion, RISK-20 CFRP Ra confirmation) is formally under QMS change control per NP-RM-001 §5.1. All future risk register updates require change control per NP-QMS-DC-001. Privacy risks identified in NP-PRIV-001 Rev A are tracked separately in NP-PRIV-REM-001 Rev A (not in the device safety risk register, as they are programme-level operational risks rather than device safety hazards).

### 5.8 First Article Inspection and Test Records

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-FAI-ZM-001 | Zone Module FAI Checklist | A | 2026-05-06 | [neuropulse_fai_zone_module.docx](./neuropulse_fai_zone_module.docx) | ACTIVE | FAI |

### 5.9 Supplier and Procurement Records

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-PROC-SUP-001 | Tooling and Process Supplier Selection Checklist | A | 2026-05-06 | [neuropulse_supplier_selection_checklist.docx](./neuropulse_supplier_selection_checklist.docx) | ACTIVE | PROC |

### 5.10 Engineering Coordination and Gate Records

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-COORD-001 | Engineering Coordination Checklist | A.8 | 2026-05-17 | [neuropulse_eng_coordination_checklist.docx](./neuropulse_eng_coordination_checklist.docx) | ACTIVE — Rev A.9 required to add G3-09 (FHIR ImplementationGuide gate) per NP-PRIV-REM-001 STEP-14 | COORD |
| NP-DRV-SHELL-001 | Shell FPC Routing Review | B | 2026-05-10 | [neuropulse_shell_fpc_routing_review.docx](./neuropulse_shell_fpc_routing_review.docx) | ACTIVE | COORD |

**Note on NP-COORD-001:** Rev A.9 is required to add gate item G3-09 (NP-INT-FHIR-001 FHIR ImplementationGuide approved before first T2 EHR integration pilot), per NP-PRIV-REM-001 STEP-14.

### 5.11 Regulatory Strategy Documents

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-REG-CVNS-001 | Cervical VNS 510(k) Pre-Submission (Q-Sub) Package | A | 2026-05-11 | [neuropulse_cvns_510k_presub_001.md](./neuropulse_cvns_510k_presub_001.md) | ACTIVE | REG |
| NP-REG-PBM1064-001 | RISK-03 Scope Expansion Brief — 1064nm irradiance, aggregate irradiance, T2 combined session, depth-tier penetration claims | A | 2026-05-13 | [neuropulse_reg_pbm1064_risk03_001.md](./neuropulse_reg_pbm1064_risk03_001.md) | ACTIVE — Rev B required to add Q-13 (Mode F retinal PBM); pending outside counsel opinion letter | REG |

**Note on NP-REG-PBM1064-001:** Rev B is required to add Q-13 covering Mode F (808-830nm bilateral retinal PBM during normal-looking wear) per NP-FW-EMMC-002 Rev A §F and NP-PRIV-REM-001 STEP-18. The Mode F firmware gate flag `NP_MODE_F_REGULATORY_CLEARED` remains 0 until the Rev B opinion letter is received.

### 5.12 Clinical Strategy and Evidence

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-CLIN-001 | Clinical Trials Strategy | A | 2026-05-02 | [neuropulse_clinical_trials_strategy.docx](./neuropulse_clinical_trials_strategy.docx) | ACTIVE | CLIN |
| NP-MOD-EXT-001 | Additional Modalities Specification | A | 2026-05-02 | [neuropulse_additional_modalities_superseded.docx](./neuropulse_additional_modalities_superseded.docx) | SUPERSEDED — all modalities incorporated into individual firmware specs and CLAUDE.md | CLIN |
| NP-BIB-001 | Clinical Evidence Bibliography (39 entries, 12 modality sections) | — | 2026-05-02 | [neuropulse_bibliography.docx](./neuropulse_bibliography.docx) | ACTIVE | CLIN |
| NP-BIB-1064-001 | 1064nm PBM Clinical Evidence Bibliography Addendum | A | 2026-05-13 | [neuropulse_bibliography_1064nm_001.md](./neuropulse_bibliography_1064nm_001.md) | ACTIVE — entries incorporated into NP-BIB-001 | CLIN |
| NP-SBIR-001 | SBIR Phase I Draft | — | 2026-05-02 | [neuropulse_sbir_phase1_draft.docx](./neuropulse_sbir_phase1_draft.docx) | ACTIVE | CLIN |
| — | Researcher Candidate List | — | 2026-05-02 | [neuropulse_researchers.docx](./neuropulse_researchers.docx) | ACTIVE | CLIN |

### 5.13 Privacy and Security Documents

Privacy and security documents are design programme records under NP-QMS-001. They are not device design records in the §820.30 sense but are required operational and compliance documents that support T1 and T2 launch readiness. They are indexed here for completeness and traceability.

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-PRIV-001 | Privacy Analysis and Repair — full system review of UHDR/SHDR architecture, consent engine, research data flows, clinical platform. 18 findings (2 Critical, 6 High, 7 Medium, 3 Low). | A | 2026-06-02 | [neuropulse_privacy_analysis_001.pdf](./neuropulse_privacy_analysis_001.pdf) | ACTIVE | PRIV |
| NP-PRIV-REM-001 | Privacy Remediation Master Plan — **Rev B** (2026-06-03): expanded to 36 steps (STEP-01 through STEP-36); new steps STEP-31 (HIPAA Expert Determination certifier), STEP-32 (per-study NP-ANON-CERT), STEP-33 (adaptive stimulation transparency), STEP-34 (BIPA), STEP-35 (MHMD), STEP-36 (children's age gate); enhanced detail on STEP-10/11/12/13/14/20 with operational instructions; direct remediations updated with Session 2 (2026-06-03) table. | B | 2026-06-03 | [neuropulse_privacy_remediation_001.md](./neuropulse_privacy_remediation_001.md) | ACTIVE — STEP-01 through STEP-11 specs COMPLETE (STEP-09 direct; STEP-10/11 specs authored); STEP-12 through STEP-36 OPEN | PRIV |
| NP-SEC-BR-001 | Breach Response Plan — escalation chain (**updated 2026-06-03: TBD contacts replaced with interim named roles — CEO is Incident Commander/Technical Lead/Communications**); detection signals; P1/P2/P3 severity; containment; regulatory notification (HIPAA, FTC HBNR, GDPR Art. 33–34, US state laws); notification templates (Appendices A–C); annual tabletop exercise procedure. | A | 2026-06-02 | [neuropulse_breach_response_001.md](./neuropulse_breach_response_001.md) | ACTIVE — tabletop exercise required before T1 launch; Legal Counsel to be named before first production data store | SEC |
| NP-PROC-POA-001 | Healthcare Power of Attorney Upload Procedure — accepted document types; E2E encrypted upload via signed URL; vault access controls (≤3 named reviewers; every access logged); structured review record schema (no PII retained); 30-day document deletion; annual re-verification; capacity restoration; P1 incident classification for vault breach. | A | 2026-06-02 | [neuropulse_poa_procedure_001.md](./neuropulse_poa_procedure_001.md) | ACTIVE — implementation required before POA feature in any app build | PRIV |
| NP-APP-TELEMETRY-001 | App Analytics and Crash Reporting Policy — **Rev B** (2026-06-03): `session_sequence` replaced with `engagement_tier` coarsened enum; §3.2 added. Rev A: vendor requirements; 9 permitted event properties; prohibited properties (health-inferrable strings, identity fields, biometrics); SDK init gate (post-consent only); crash reporter config; linting rule; annual review. | B | 2026-06-03 | [neuropulse_app_telemetry_001.md](./neuropulse_app_telemetry_001.md) | ACTIVE — also listed in §5.5 | APP |
| NP-FW-EMMC-002 | Firmware Privacy Remediation Delta — §A–§F (2026-06-02) + **§G SHDR accelerometer reclassification (2026-06-03)**: `drop_detected: bool` + `maintenance_alert: bool` only; raw accelerometer prohibited; OI-EMMC2-07 CI test BLOCKING for fleet DB schema freeze. 7 open items OI-EMMC2-01 through OI-EMMC2-07. Also listed in §5.4. | A | 2026-06-02 | [neuropulse_fw_emmc_002.md](./neuropulse_fw_emmc_002.md) | ACTIVE — cross-listed from §5.4 | SPEC-FW |
| NP-LEGAL-BAA-001 | Standard Business Associate Agreement Template — DRAFT (legal counsel review required before first execution). Full HIPAA BAA (45 CFR §164.504(e)) + NeuroPulse additions: §5.1 consent revocation deletion cascade (30 days); §5.2 no onward sharing; §3.3 5-business-day breach notification; §4.2 three-tier access table (Monitor/Assess/Full Clinical) with minimum-necessary data; §5.4 BIPA biometric data provision; §5.5 EEG biometric classification. Exhibit A scope template. | A | 2026-06-03 | [neuropulse_legal_baa_001.md](./neuropulse_legal_baa_001.md) | ACTIVE — DRAFT; requires legal counsel review before first execution | PRIV |
| NP-PRIV-AUDIT-001 | App Privacy Audit — iOS/Android app pre-beta privacy analysis. 16 findings (0 Critical, 5 High, 7 Medium, 4 Low). Scope: analytics vendor gap, SDK init gate, App Store nutrition label, BIPA screen, MHMD SHDR compliance, research consent UX, BLE GATT data scope, Watch app data flows, clinical consent UX, crash reporter CI gap, engagement_tier lifecycle, adaptive trigger enum, privacy notice process, age gate threshold, privacy regression tests. 16-item compliance checklist (AUDIT-01 through AUDIT-16), all OPEN. BLOCKING for first external beta (TestFlight / Play Store open beta). | A | 2026-06-03 | [neuropulse_priv_audit_001.md](./neuropulse_priv_audit_001.md) | ACTIVE — all 16 checklist items OPEN; AUDIT-01/02/03/04/05/06/08/09/13 BLOCKING for external beta | PRIV |
| NP-PRIV-NOTICE-001 | App Privacy Notice — Issue #98 / STEP-33 of NP-PRIV-REM-001. Full user-facing privacy notice for NeuroPulse iOS and Android apps. §2 UHDR/SHDR architecture in plain English. §3 app analytics data (engagement_tier, SDK gate). **§4 Automated processing and adaptive stimulation** — GDPR Art. 13(2)(f) disclosure: adaptive algorithm logic, adaptable parameters (audio beat frequency, PBM pulse frequency, tACS amplitude, VNS phase), what is recorded (classified trigger enum + parameter delta, no raw EEG/HRV), user's right to view in Session History. §5 research data (on-device anonymisation). §6 clinician access. §7 rights table (EEA/UK/CA). §8 retention table. §9 security summary. OI-PA-04 OPEN — §4 copy requires Privacy Lead sign-off before first public beta. | A | 2026-06-03 | [neuropulse_privacy_notice.md](./neuropulse_privacy_notice.md) | ACTIVE — OI-PA-04 OPEN (Privacy Lead copy sign-off required before external beta) | PRIV |
| NP-INT-FHIR-001 | FHIR R4 ImplementationGuide — NeuroPulse T2 Clinical Profile. Permitted resources: Patient (opaque MRN only; name/DOB/address/telecom prohibited), Observation (EEG band power ratios via NeuroPulse local codes; HRV via LOINC 80404-7/8867-4; session dose), DiagnosticReport (weekly summary), Procedure (Full Clinical tier). LSL TLS encryption required. FHIR validator CI spec. Canonical: `https://fhir.neuropulse.com/ig/NeuroPulse-T2-Clinical`. 5 open items (OI-FHIR-01 through OI-FHIR-05). Addresses NP-COORD-001 G3-09. | A | 2026-06-03 | [neuropulse_fhir_profile_001.md](./neuropulse_fhir_profile_001.md) | ACTIVE — IG package publication and CI integration pending; OI-FHIR-01 (LOINC mapping) required before publication | PRIV |

---

## 6. Firmware Source Code as DHF Records

Under IEC 62304 and 21 CFR §820.30, software source code and associated build artefacts are design outputs and therefore DHF records. The following firmware directories are DHF source code records:

| Firmware item | IEC 62304 class | Document | Repository path |
|---|---|---|---|
| Dual-bank OTA bootloader | Class B (boundary) | NP-FW-EMMC-001 Rev A §8 | [firmware/bootloader/](../firmware/bootloader/) |
| Hub control program | Class B | NP-FW-HUB-001 Rev A | [firmware/hub_control/](../firmware/hub_control/) |
| HRV biofeedback protocol | Class B | NP-FW-HRV-001 Rev A | [firmware/hrv_biofeedback/](../firmware/hrv_biofeedback/) |
| Zone module bone conduction announcement | Class B | NP-FW-ZA-001 Rev A | [firmware/zone_announce/](../firmware/zone_announce/) |
| sLORETA-guided HD-tDCS | Class B (T2) | NP-FW-HD-001 Rev A | [firmware/sloreta_hdtdcs/](../firmware/sloreta_hdtdcs/) |
| Cervical VNS safety interlock | **Class C** | NP-FW-CVNS-001 Rev A | [firmware/cervical_vns/](../firmware/cervical_vns/) |
| 1064nm smart zone module PBM | Class B | NP-FW-PBM1064-001 Rev A | [firmware/pbm_1064nm/](../firmware/pbm_1064nm/) |

Each firmware directory is under git version control. Git commit hashes constitute the version record for source code. Release candidates must be tagged per NP-SW-001 §9.

**Note on `firmware/cmake/`:** This directory contains the CMake toolchain and cross-compilation configuration for the arm-none-eabi build environment. It is build infrastructure, not a firmware module, and is not a separate DHF record. It is covered implicitly by the build reproducibility requirement in NP-SW-001 §9.3.

**Note on privacy-related firmware modules (NP-FW-EMMC-002):** The firmware modules specified in NP-FW-EMMC-002 Rev A (warranty token, factory reset, two-layer key, Scratch encryption, EDF+ writer, Mode F) do not yet have corresponding source code directories. When authored, they will be added to this table at the relevant firmware path. The specification in NP-FW-EMMC-002 Rev A is the design input for those modules.

### 6b. iOS Application Source Code Records (SW-03, IEC 62304 Class B)

The iOS application source code is a Class B software item under NP-SW-001. Source is at `app/ios/NeuroPulse/`. The Xcode project (`app/ios/NeuroPulse.xcodeproj`) and all Swift source files are under git version control. PR #106 (feature/ios-parallel-integration, 2026-06-04) constitutes the first substantive design output delivery for Issue #51.

| Module group | IEC 62304 class | Specification | Source path |
|---|---|---|---|
| BLE GATT layer (NeuroPulseGATTManager, GATTCharacteristics) | Class B | NP-APP-ISA-001 ISC-11–20 | `app/ios/NeuroPulse/BLE/` |
| Session display — Mode 1 Connected (SessionView) | Class B | NP-APP-ISA-001 ISC-21–34 | `app/ios/NeuroPulse/Views/SessionView.swift` |
| Protocol upload — Mode 2 (SessionProtocolUploader, ProtocolChunker) | Class B | NP-APP-ISA-001 ISC-35–47 | `app/ios/NeuroPulse/Session/`, `app/ios/NeuroPulse/Protocol/ProtocolChunker.swift` |
| Session history + EDF download — Mode 4 (SessionHistoryView, AdaptiveAdjustmentsCard) | Class B | NP-APP-ISA-001 ISC-48–55 | `app/ios/NeuroPulse/Views/SessionHistoryView.swift` |
| UHDR key management (UHDRKeyManager, UHDRBackupScheduler) | Class B | NP-APP-ISA-001 ISC-56–63 | `app/ios/NeuroPulse/Data/` |
| SHDR upload (SHDRUploader) | Class B | NP-APP-ISA-001 ISC-64–67 | `app/ios/NeuroPulse/Data/SHDRUploader.swift` |
| Clinical consent engine (ConsentEngine, ConsentStore) | Class B | NP-APP-ISA-001 ISC-68–82 | `app/ios/NeuroPulse/Consent/` |
| Privacy compliance (AgeGateView, Under16View, HealthKitSessionReader) | Class B | NP-APP-ISA-001 ISC-83–97 | `app/ios/NeuroPulse/Onboarding/`, `app/ios/NeuroPulse/Data/HealthKitSessionReader.swift` |
| Consumable tracker (ConsumableTracker) | Class B | NP-APP-ISA-001 ISC-98–106 | `app/ios/NeuroPulse/Consumable/` |
| OTA firmware update (OTAManager) | Class B | NP-APP-ISA-001 ISC-107–113 | `app/ios/NeuroPulse/OTA/` |
| Hardware setup wizard (HardwareSetupManager, SetupView) | Class B | NP-APP-ISA-001 ISC-114–121 | `app/ios/NeuroPulse/Setup/` |
| Apple Watch bridge (PhoneSessionManager) | Class B | NP-APP-ISA-001 ISC-122–125 | `app/ios/NeuroPulse/WatchBridge/` |
| App entry point and service wiring (NeuroPulseApp) | Class B | NP-APP-ISA-001 | `app/ios/NeuroPulse/NeuroPulseApp.swift` |

**Test target:** `app/ios/NeuroPulseTests/` — 6 XCTest suites, 44 tests, all passing as of PR #106. Three `XCTExpectFailure` entries document known validator gaps (charge density, PBM dose, zero-duration) requiring follow-up implementation.

---

## 7. DHF Completeness Assessment

This section identifies design phases and their DHF coverage status.

| Design phase | 21 CFR §820.30 | Coverage | Gap / action |
|---|---|---|---|
| Design planning | §820.30(b) | **Good** — NP-DP-001 Rev A (2026-05-17) is the formal design and development plan; CLAUDE.md Rev 17 + NP-COORD-001 Rev A.8 are the operational design planning instruments | NP-DP-001 to be updated at each gate review; NP-COORD-001 Rev A.9 required for G3-09 (FHIR IG gate) |
| Design inputs | §820.30(c) | **Good** — NP-DT-001 Rev A (2026-06-07) provides 57 formal design inputs (DI-PERF/SAFE/USE/REG/INT) with full DI→DO→VE traceability matrix; design briefs Rev 1–5 and CLAUDE.md capture remaining inputs | NP-DT-001 OI-DT-01..05 require resolution; hardware requirements to be traced as hardware matures |
| Design outputs | §820.30(d) | **Good** — Hardware specs, firmware specs (7 written modules + NP-FW-EMMC-002 delta), tooling specs present and indexed; hub_control program written; NP-DT-001 Rev A provides 35 mapped design outputs with traceability to inputs | NP-DT-001 OI-DT-01..05 outstanding; firmware modules not yet authored flagged in NP-FW-EMMC-002 notes |
| Design review | §820.30(e) | **Good** — NP-COORD-001 Rev A.8 gate records; G1-15, G1-16, G2-10, G2-11, G2-12 CLOSED; G3-07/G3-08 SOFTWARE BASELINED | Formal design review minutes at each gate closure going forward; G3-09 to be added in NP-COORD-001 Rev A.9 |
| Design verification | §820.30(f) | **Partial** — NP-FAI-ZM-001 checklist defined; software FAI items passed for all firmware modules; hardware FAI pending prototype | FAI execution on prototype hardware constitutes verification evidence |
| Design validation | §820.30(g) | **Not yet started** — Requires device prototype and human factors testing | Planned for Year 2 (T2 development phase) |
| Design transfer | §820.30(h) | **Not yet started** — No manufacturing transfer yet | Required before first production run |
| Design changes | §820.30(i) | **Partial** — Git commit history tracks changes; NP-QMS-DC-001 change order process established | Use NP-QMS-DC-001 change order process from 2026-05-13 forward |
| DHF maintenance | §820.30(j) | **Established** — This document | Maintain index with each new document release |
| **Privacy programme** | FTC Act §5; HIPAA; GDPR Art. 25; BIPA; MHMD | **Active** — NP-PRIV-001 Rev A + Rev B analyses complete (26 total findings); NP-PRIV-REM-001 Rev B calendar (36 steps); STEP-01–09 complete, STEP-10/11 specs authored; NP-FW-ANON-001, NP-LEGAL-BAA-001, NP-INT-FHIR-001, NP-FW-EMMC-002 §G all authored 2026-06-03 | STEP-12 through STEP-36 tracked in NP-PRIV-REM-001 Rev B; BIPA (STEP-34) and MHMD (STEP-35) require legal counsel engagement before device activations in those states; tabletop exercise required before T1 launch |

---

## 8. Future Document Additions

When a new controlled document is created, this index must be updated before the new document is released. The update is a change to NP-DHF-001 and requires approval per §3.3 of NP-QMS-001.

Planned near-term additions:

| Planned doc number | Title | Target | Trigger |
|---|---|---|---|
| ~~NP-DP-001~~ | ~~Design and Development Plan~~ | ~~Month 3~~ | **COMPLETE — NP-DP-001 Rev A released 2026-05-17** |
| NP-FW-EMMC-001 Rev B | eMMC Partition Architecture — incorporates all NP-FW-EMMC-002 §A–§G delta sections | Month 6 | When firmware team begins implementation; NP-FW-EMMC-002 becomes INCORPORATED |
| ~~NP-FW-ANON-001~~ | ~~Research Anonymisation Engine Firmware Specification~~ | ~~G1~~ | **COMPLETE — NP-FW-ANON-001 Rev A released 2026-06-03** |
| ~~NP-INT-FHIR-001~~ | ~~FHIR R4 ImplementationGuide~~ | ~~G1~~ | **COMPLETE — NP-INT-FHIR-001 Rev A released 2026-06-03; IG package publication and CI integration pending** |
| ~~NP-LEGAL-BAA-001~~ | ~~Standard Business Associate Agreement Template~~ | ~~Month 3~~ | **COMPLETE (DRAFT) — NP-LEGAL-BAA-001 Rev A released 2026-06-03; legal counsel review required before first execution** |
| ~~NP-API-001~~ | ~~T2 Scripting API Specification~~ | ~~G1 (Month 6)~~ | **COMPLETE — NP-API-001 Rev A released 2026-06-07 (PR #121); independent security audit NP-SEC-PENTEST-002 required before any clinical API key is issued** |
| NP-REG-DPF-001 | EU-US Data Privacy Framework Self-Certification Record | Month 3 | Before any EU resident's data reaches US infrastructure; NP-PRIV-REM-001 STEP-12 |
| NP-REG-BIPA-001 | BIPA Compliance Record — legal opinion + consent screen implementation + website policy | Month 2 | Before any Illinois device activation; NP-PRIV-REM-001 STEP-34 |
| NP-REG-MHMD-001 | Washington MHMD Compliance Record — legal analysis + SHDR consent redesign decision | Month 2 | Before any Washington device activation; NP-PRIV-REM-001 STEP-35 |
| ~~NP-DT-001~~ | ~~Design Input/Output Traceability Matrix~~ | ~~Month 6~~ | **COMPLETE — NP-DT-001 Rev A released 2026-06-07 (PR #122); design-inputs DHF coverage upgraded from Partial to Good** |
| NP-HFE-001 | Human Factors Engineering Plan | Month 9 | IEC 62366-1 / FDA HFE guidance |
| ~~NP-PRIV-AUDIT-001~~ | ~~App Privacy Audit~~ | ~~Month 9~~ | **COMPLETE — NP-PRIV-AUDIT-001 Rev A released 2026-06-03; 16 checklist items OPEN pending implementation** |
| NP-IRB-001 | IRB Protocol — research anonymisation and consent architecture | Month 9 | Before first study descriptor deployment; NP-PRIV-REM-001 STEP-20 §4.4 |
| NP-ARCH-CLOUD-001 | T2 Clinical Cloud Architecture — EU data residency decision | Month 9 | When T2 cloud vendor selected; NP-PRIV-REM-001 STEP-19 |
| NP-PMS-001 | Post-Market Surveillance Plan | Month 12 | Required before product launch |
| NP-FAI-SM-001 | 1064nm Smart Module FAI (hardware items) | Post-prototype | FAI-SM-04, -06, -07, -08 pending hub PCB Rev B Gerber build + optical bench |
| NP-FAI-HD-001 | sLORETA HD-tDCS Hardware FAI | Post-T2 prototype | FAI-HD01, HD03, HD04 pending |
| NP-FAI-CV-001 | Cervical VNS Hardware FAI | Post-T2 prototype | FAI-CV01, CV02, CV03 pending |
| NP-SEC-PENTEST-001 | POA Vault Penetration Test Report | G3 (Month 14) | After POA vault implementation; NP-PRIV-REM-001 STEP-25 |
| NP-SEC-PENTEST-002 | T2 Scripting API Security Audit Report | T2 pre-launch | Before any clinical API key issued; NP-PRIV-REM-001 STEP-26 |

---

## 9. Document History

| Rev | Date | Author | Description |
|---|---|---|---|
| A | 2026-05-13 | Interim Quality (CEO) | Initial release. All pre-formation design documents entered retroactively under change control. DHF established at QMS formation. |
| B | 2026-05-17 | Interim Quality (CEO) | NP-DB-004 (R4) marked SUPERSEDED; NP-DB-005 (R5) added as ACTIVE. NP-HW-HUB-001 Rev B added (§5.3). NP-FW-HUB-001 Rev A (hub_control) added to §5.4 and §6. NPPS Language Reference added (§5.5). NP-FW-REQ-001 marked SUPERSEDED. NP-MOD-EXT-001 marked SUPERSEDED. NP-RISK-001 → Rev B. NP-COORD-001 → Rev A.8. CLAUDE.md → Rev 9. Relative navigation links added. DHF completeness assessment updated. |
| C | 2026-05-17 | Interim Quality (CEO) | NP-SIM-001 v0.1.0 (Helmet Simulator) added to §5.5; SIM category added to §4. CLAUDE.md → Rev 11. Note added below §5.5 clarifying simulator regulatory status. Open sub-issues #77–#80 recorded under parent Issue #81 / PR #76. |
| D | 2026-05-17 | Steve Hickman (CEO, interim Quality authority) | NP-DP-001 Rev A (Design and Development Plan) released and added to §5.1. DHF completeness assessment — design planning row upgraded from Partial to Good. NP-DP-001 removed from §8 planned additions (marked COMPLETE). CEO name added to Author/Approved-by fields throughout. |
| E | 2026-05-17 | Steve Hickman (CEO, interim Quality authority) | NP-SIM-001 updated v0.1.0 → v0.2.0 in §5.5 note — WebSocket device API (Issue #77, PR #84 CLOSED) and intranasal Y-probe animation + ACCESSORY_CONFIG (Issue #78, PR #85 CLOSED). Open sub-issues reduced to #79–#80. CLAUDE.md → Rev 13. |
| L | 2026-06-07 | Steve Hickman (CEO, interim Quality authority) | **Three new documents added — NP-FMEA-001 Rev A (SW-01 Safety MCU Unit-Level FMEA, PR #120), NP-API-001 Rev A (T2 Clinical Scripting API Specification, PR #121), NP-DT-001 Rev A (Design Input/Output Traceability Matrix, PR #122).** NP-FMEA-001 added to §5.7 Risk Management Records (43 failure modes, 2 initial UNACCEPTABLE risks mitigated, 5 open items). NP-API-001 added to §5.5 Session/Protocol/Application Specifications (G1 gate item COMPLETE; security audit NP-SEC-PENTEST-002 required before clinical key issuance). NP-DT-001 added to §5.1 QMS and Quality Documents (G2 exit criterion per NP-DP-001 §6.4). §7 DHF completeness — design inputs and design outputs rows upgraded from Partial to Good. §8 planned additions — NP-API-001 and NP-DT-001 marked COMPLETE. CLAUDE.md updated Rev 17 → Rev 22 in §5.2. Header corrected Rev I → Rev L (footer was already at Rev K; this revision closes the discrepancy). |
| H | 2026-06-03 | Steve Hickman (CEO, interim Quality authority) | **NP-PRIV-AUDIT-001 Rev A released** — iOS/Android app privacy audit (STEP-21 of NP-PRIV-REM-001). 16 findings (0 Critical, 5 High, 7 Medium, 4 Low). 16-item compliance checklist (AUDIT-01 through AUDIT-16) all OPEN — gates external beta. NP-PRIV-AUDIT-001 added to §5.13 Privacy and Security Documents. NP-PRIV-AUDIT-001 marked COMPLETE in §8 planned additions. CLAUDE.md §13.4 pending item "App privacy audit" marked [x]. CLAUDE.md §14 document register updated with NP-PRIV-AUDIT-001 Rev A entry. |
| K | 2026-06-04 | Steve Hickman (CEO, interim Quality authority) | **PR #107 — iOS validator safety gaps closed + PrivacyInfo.xcprivacy bundle fix.** `PrivacyInfo.xcprivacy` was on disk but absent from `project.pbxproj` (zero refs) — added to Copy Bundle Resources; without this the privacy manifest does not ship in the app bundle (ISC-133 now verified). Three `XCTExpectFailure`-wrapped validator tests converted to real passing tests: tDCS charge density guard (ISC-38: `I×t/electrodeArea > 40 µC/cm² → error`; `tdcsDefaultElectrodeAreaCm2 = 35 cm²` added to `NPHardwareLimits`); zero-duration hard rejection (ISC-47: `dur ≤ 0 → .error`, was `.warning`); PBM session dose guard (ISC-47: estimated dose vs `maxSessionDoseJCm2` limit). `NPProtocolValidatorTests` 9/9 passing with zero `XCTExpectFailure`. NP-APP-ISA-001 updated: ISC-38, ISC-47, ISC-133 marked `[x]`; Decisions and Verification sections populated; progress 0/164 → 3/164. |
| J | 2026-06-04 | Steve Hickman (CEO, interim Quality authority) | **Issue #51 — Core iOS app parallel integration (PR #106).** E4 ISA scaffolded at `app/ios/ISA.md` (NP-APP-ISA-001, 164 ISCs). 9 parallel Engineer agents in git worktrees implemented: `AgeGateView.swift` + `Under16View.swift` (minimum age gate, CLAUDE.md §13.4 pending item marked [x]); `HealthKitSessionReader.swift` (session-scoped HRV read, ISC-94–96); `ProtocolChunker.swift` (BLE ≤512-byte framing with START/CONT/END/SINGLE headers, 17/17 edge cases); `SessionHistoryView.swift` + `AdaptiveAdjustmentsCard.swift` (from Issue #98, merged into this PR); `en.lproj/Localizable.strings` (98 keys); `Info.plist` + `PrivacyInfo.xcprivacy` + `AppStorePrivacyLabel.md`; `.swiftlint.yml`; 6 XCTest suites (44 tests, all passing). Modified: `GATTCharacteristics.swift` (sessionStop CBUUID added, duplicate GATTParser removed); `NeuroPulseGATTManager.swift` (sendSessionStop); `SessionView.swift` (stop command, mode labels, HealthKit card); `NeuroPulseApp.swift` (age gate sequence, HealthKitSessionReader injection); `SessionProtocolUploader.swift` (ProtocolChunker + programAutonomous); `SetupView.swift` (Mode 3 card); `NPProtocolDefinition.swift` (Identifiable conformance on 7 picker enums). iOS source table added to §6b. NP-APP-ISA-001 added to §5.5. NP-APP-ROADMAP-001 OI-PA-01 (age gate legal threshold) remains open. Three validator safety gaps documented via XCTExpectFailure: charge density, PBM dose, zero-duration — follow-up required. CLAUDE.md → Rev 19. |
| I | 2026-06-03 | Steve Hickman (CEO, interim Quality authority) | **Issue #98 — Adaptive stimulation transparency (STEP-33 of NP-PRIV-REM-001) COMPLETE.** np_adaptation_log.h new firmware header (np_adapt_trigger_t enum, 17 values; np_adaptation_event_t struct; np_adapt_log_event/flush/reset API). np_session_log updated with NP_LOG_TAG_UHDR_ADAPT_EVENT (0x18) and np_log_adapt_event(). iOS: AdaptationEvent.swift model (AdaptTrigger + plainLanguageDescription + CompletedSessionSummary); AdaptiveAdjustmentsCard.swift; SessionHistoryView.swift. NP-PRIV-NOTICE-001 Rev A released (docs/neuropulse_privacy_notice.md) — GDPR Art. 13(2)(f) §4 for adaptive stimulation. OI-PA-04 OPEN — Privacy Lead sign-off on plain-language copy required before Adaptive Adjustments card ships. T2 NP-API-001 schema deferred to G3. CLAUDE.md → Rev 18. NP-PRIV-NOTICE-001 added to §5.13. ISA at MEMORY/WORK/20260603-adaptive-transparency/ISA.md (E3, 38 ISCs). |
| G | 2026-06-03 | Steve Hickman (CEO, interim Quality authority) | **Privacy programme expansion — NP-PRIV-001 Rev B delta (8 new findings) + direct remediations:** Three new documents added to §5.4 and §5.13: NP-FW-ANON-001 Rev A (research anonymisation engine firmware spec — k-anonymity + l-diversity + differential privacy), NP-LEGAL-BAA-001 Rev A (BAA template — DRAFT), NP-INT-FHIR-001 Rev A (FHIR R4 IG — T2 clinical profile). NP-FW-EMMC-002 §G added (SHDR accelerometer reclassification, OI-EMMC2-07 BLOCKING for schema freeze). NP-APP-ROADMAP-001 updated A → Rev B (§9 Privacy Constraints binding). NP-APP-TELEMETRY-001 updated A → Rev B (`engagement_tier` replaces `session_sequence`). NP-SEC-BR-001 escalation chain updated (TBD → CEO interim). NP-PRIV-REM-001 updated A → Rev B (STEP-31 through STEP-36 added; STEP-10/11/12/13/14/20 enhanced with operational detail). CLAUDE.md updated Rev 15 → Rev 17 (Rev 16 intermediate). §7 completeness updated — Privacy programme row expanded (BIPA + MHMD added). §8 planned additions: NP-FW-ANON-001, NP-INT-FHIR-001, NP-LEGAL-BAA-001 marked COMPLETE; NP-REG-BIPA-001, NP-REG-MHMD-001 added as planned. |
| F | 2026-06-02 | Steve Hickman (CEO, interim Quality authority) | **Privacy programme documents added (NP-PRIV-001 Rev A, 2026-06-02 privacy analysis):** New PRIV and SEC document categories added to §4. New §5.13 Privacy and Security Documents created, containing: NP-PRIV-001 Rev A (privacy analysis PDF), NP-PRIV-REM-001 Rev A (remediation master plan), NP-SEC-BR-001 Rev A (breach response plan), NP-PROC-POA-001 Rev A (POA procedure), NP-APP-TELEMETRY-001 Rev A (app telemetry policy). NP-FW-EMMC-002 Rev A (firmware privacy delta) added to §5.4 with explanatory note; also cross-listed in §5.13 for privacy programme traceability. **NP-SIM-001 updated v0.2.0 → v0.3.0** — T2 TMS focal figure-8 coil added (Issue #79 CLOSED); open sub-issues reduced to #80. **CLAUDE.md updated Rev 13 → Rev 15** (Rev 14 was intermediate — privacy remediation locked decisions, 19 new §13.4 pending decisions, 5 new locked decisions, 6 new §14 entries; Rev 15 adds DHF index update). NP-APP-ROADMAP-001 Rev B noted as required (HealthKit binding constraint). NP-COORD-001 Rev A.9 noted as required (G3-09 FHIR IG gate). NP-REG-PBM1064-001 Rev B noted as required (Q-13 Mode F). DHF completeness assessment updated — Privacy programme row added. §8 planned additions expanded with 8 new privacy-related documents. |

---

*NP-DHF-001 Rev L — ACTIVE — Effective 2026-06-07*
