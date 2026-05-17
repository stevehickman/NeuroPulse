# NP-DHF-001 Rev C — NeuroPulse Design History File Index

**Document number:** NP-DHF-001  
**Revision:** C  
**Status:** ACTIVE  
**Effective date:** 2026-05-17  
**Author:** Quality Lead (interim: CEO)  
**Approved by:** CEO  
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

---

## 5. Master Document Index

### 5.1 QMS and Quality Documents

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-QMS-001 | QMS Manual | A | 2026-05-13 | [neuropulse_qms_manual_001.md](./neuropulse_qms_manual_001.md) | ACTIVE | QMS |
| NP-DHF-001 | Design History File Index (this document) | C | 2026-05-17 | [neuropulse_dhf_index_001.md](./neuropulse_dhf_index_001.md) | ACTIVE | QMS |
| NP-QMS-DC-001 | Design Controls Procedure | A | 2026-05-13 | [neuropulse_design_controls_001.md](./neuropulse_design_controls_001.md) | ACTIVE | QMS |
| NP-RM-001 | ISO 14971 Risk Management Plan | A | 2026-05-13 | [neuropulse_risk_mgmt_plan_001.md](./neuropulse_risk_mgmt_plan_001.md) | ACTIVE | RISK |
| NP-SW-001 | IEC 62304 Software Development Plan | A | 2026-05-13 | [neuropulse_sw_dev_plan_001.md](./neuropulse_sw_dev_plan_001.md) | ACTIVE | QMS |
| NP-QMS-CAPA-001 | CAPA Procedure | A | 2026-05-13 | [neuropulse_capa_001.md](./neuropulse_capa_001.md) | ACTIVE | QMS |

### 5.2 Design Briefs and Product Specifications

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-DB-001 | Design Brief | 1 | 2026-05-02 | [neuropulse_design_brief_superseded.docx](./neuropulse_design_brief_superseded.docx) | SUPERSEDED by NP-DB-005 | REQ |
| NP-DB-002 | Design Brief | 2 | 2026-05-03 | [neuropulse_design_brief_r2_superseded.docx](./neuropulse_design_brief_r2_superseded.docx) | SUPERSEDED by NP-DB-005 | REQ |
| NP-DB-003 | Design Brief | 3 | 2026-05-04 | [neuropulse_brief_r3_superseded.docx](./neuropulse_brief_r3_superseded.docx) | SUPERSEDED by NP-DB-005 | REQ |
| NP-DB-004 | Design Brief | 4 | 2026-05-07 | [neuropulse_brief_r4_superseded.docx](./neuropulse_brief_r4_superseded.docx) | SUPERSEDED by NP-DB-005 | REQ |
| NP-DB-005 | Master Design Brief | 5 | 2026-05-16 | [neuropulse_design_brief_r5.docx](./neuropulse_design_brief_r5.docx) | ACTIVE | REQ |
| — | CLAUDE.md — Project Design Memory | 11 | 2026-05-17 | [CLAUDE.md](../CLAUDE.md) | ACTIVE | REQ |

**Note on CLAUDE.md:** CLAUDE.md serves as the living design authority document capturing all locked design decisions and pending items. It is under git version control and constitutes a design record for DHF purposes. Each revision (tracked by git commit) is a controlled design change.

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
| NP-FW-EMMC-001 | eMMC Partition Architecture and Storage Encryption | A | 2026-05-11 | [neuropulse_fw_emmc_001.docx](./neuropulse_fw_emmc_001.docx) | ACTIVE | SPEC-FW |
| NP-FW-HUB-001 | Hub Control Program — main SW-02 application firmware (module registry, session runner, telemetry, safety SPI) | A | 2026-05-16 | [../firmware/hub_control/](../firmware/hub_control/) | ACTIVE | SPEC-FW |
| NP-FW-HRV-001 | HRV Biofeedback Protocol Firmware Specification | A | 2026-05-11 | [neuropulse_fw_hrv_001.md](./neuropulse_fw_hrv_001.md) | ACTIVE | SPEC-FW |
| NP-FW-ZA-001 | Zone Module Bone Conduction Announcement Firmware | A | 2026-05-11 | [neuropulse_fw_zone_announce_001.md](./neuropulse_fw_zone_announce_001.md) | ACTIVE | SPEC-FW |
| NP-FW-HD-001 | sLORETA-Guided HD-tDCS Firmware Specification | A | 2026-05-11 | [neuropulse_fw_sloreta_hdtdcs_001.md](./neuropulse_fw_sloreta_hdtdcs_001.md) | ACTIVE | SPEC-FW |
| NP-FW-CVNS-001 | Cervical VNS Safety Interlock Firmware Specification | A | 2026-05-11 | [neuropulse_fw_cvns_001.md](./neuropulse_fw_cvns_001.md) | ACTIVE | SPEC-FW |
| NP-FW-PBM1064-001 | 1064nm Smart Zone Module Firmware Specification | A | 2026-05-12 | [neuropulse_fw_pbm1064_001.md](./neuropulse_fw_pbm1064_001.md) | ACTIVE | SPEC-FW |
| NP-FW-REQ-001 | Zone Module Firmware Requirements | A | 2026-05-10 | [neuropulse_fw_requirements_001_superseded.docx](./neuropulse_fw_requirements_001_superseded.docx) | SUPERSEDED by individual firmware specs (NP-FW-PBM1064-001, NP-FW-HRV-001, NP-FW-CVNS-001, NP-FW-HD-001, NP-FW-ZA-001, NP-FW-EMMC-001) | REQ |

### 5.5 Session, Protocol, and Application Specifications

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-SES-1064-001 | 1064nm Multi-Wavelength Session Protocol | A | 2026-05-12 | [neuropulse_session_protocol_1064_001.md](./neuropulse_session_protocol_1064_001.md) | ACTIVE | SES |
| NP-APP-ROADMAP-001 | iOS App Development Roadmap | A | 2026-05-11 | [neuropulse_ios_app_roadmap_001.md](./neuropulse_ios_app_roadmap_001.md) | ACTIVE | APP |
| — | NPPS Protocol Scripting Language Reference | — | 2026-05-16 | [npps-reference.md](./npps-reference.md) | ACTIVE | APP |
| NP-SIM-001 | Helmet Simulator — interactive 3D browser visualisation | v0.1.0 | 2026-05-17 | [../simulator/](../simulator/) | ACTIVE — Issue #81 / PR #76 | SIM |

**Note on NP-SIM-001:** The helmet simulator is a software design output (browser-based Three.js application). It is not a DHF record in the medical device regulatory sense — it is a design tool, marketing asset, and protocol development aid. It is listed here for completeness as a versioned design output under git control. The simulator source is at `simulator/` in the repository root and is tracked under PR #76. Open sub-issues tracked under GitHub issue #81: #77 (WebSocket device API), #78 (intranasal probe animation), #79 (T2 TMS coil), #80 (geometry update pending shell CAD finalisation).

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

**Note:** The risk register (RISK-01 through RISK-25; 23 MITIGATED, 2 OPEN: RISK-03 regulatory opinion, RISK-20 CFRP Ra confirmation) is formally under QMS change control per NP-RM-001 §5.1. All future risk register updates require change control per NP-QMS-DC-001.

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
| NP-COORD-001 | Engineering Coordination Checklist | A.8 | 2026-05-17 | [neuropulse_eng_coordination_checklist.docx](./neuropulse_eng_coordination_checklist.docx) | ACTIVE | COORD |
| NP-DRV-SHELL-001 | Shell FPC Routing Review | B | 2026-05-10 | [neuropulse_shell_fpc_routing_review.docx](./neuropulse_shell_fpc_routing_review.docx) | ACTIVE | COORD |

### 5.11 Regulatory Strategy Documents

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-REG-CVNS-001 | Cervical VNS 510(k) Pre-Submission (Q-Sub) Package | A | 2026-05-11 | [neuropulse_cvns_510k_presub_001.md](./neuropulse_cvns_510k_presub_001.md) | ACTIVE | REG |
| NP-REG-PBM1064-001 | RISK-03 Scope Expansion Brief — 1064nm irradiance, aggregate irradiance, T2 combined session, depth-tier penetration claims | A | 2026-05-13 | [neuropulse_reg_pbm1064_risk03_001.md](./neuropulse_reg_pbm1064_risk03_001.md) | ACTIVE — pending outside counsel opinion letter | REG |

### 5.12 Clinical Strategy and Evidence

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-CLIN-001 | Clinical Trials Strategy | A | 2026-05-02 | [neuropulse_clinical_trials_strategy.docx](./neuropulse_clinical_trials_strategy.docx) | ACTIVE | CLIN |
| NP-MOD-EXT-001 | Additional Modalities Specification | A | 2026-05-02 | [neuropulse_additional_modalities_superseded.docx](./neuropulse_additional_modalities_superseded.docx) | SUPERSEDED — all modalities incorporated into individual firmware specs and CLAUDE.md | CLIN |
| NP-BIB-001 | Clinical Evidence Bibliography (39 entries, 12 modality sections) | — | 2026-05-02 | [neuropulse_bibliography.docx](./neuropulse_bibliography.docx) | ACTIVE | CLIN |
| NP-BIB-1064-001 | 1064nm PBM Clinical Evidence Bibliography Addendum | A | 2026-05-13 | [neuropulse_bibliography_1064nm_001.md](./neuropulse_bibliography_1064nm_001.md) | ACTIVE — entries incorporated into NP-BIB-001 | CLIN |
| NP-SBIR-001 | SBIR Phase I Draft | — | 2026-05-02 | [neuropulse_sbir_phase1_draft.docx](./neuropulse_sbir_phase1_draft.docx) | ACTIVE | CLIN |
| — | Researcher Candidate List | — | 2026-05-02 | [neuropulse_researchers.docx](./neuropulse_researchers.docx) | ACTIVE | CLIN |

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

---

## 7. DHF Completeness Assessment

This section identifies design phases and their DHF coverage status.

| Design phase | 21 CFR §820.30 | Coverage | Gap / action |
|---|---|---|---|
| Design planning | §820.30(b) | **Partial** — CLAUDE.md Rev 9 + NP-DB-005 serve as master plan; formal design plan document not yet authored | Author NP-DP-001 Design and Development Plan (Year 1, Month 3) |
| Design inputs | §820.30(c) | **Partial** — Design briefs Rev 1–5 and CLAUDE.md capture inputs; NP-FW-REQ-001 superseded by individual firmware specs; hardware requirements document needed | Individual firmware specs (NP-FW-PBM1064-001, NP-FW-HRV-001, etc.) cover firmware requirements; traceability matrix NP-DT-001 needed |
| Design outputs | §820.30(d) | **Good** — Hardware specs, firmware specs (7 written modules), tooling specs present and indexed; hub_control program written | Ensure all outputs traceable to inputs; traceability matrix (NP-DT-001) needed |
| Design review | §820.30(e) | **Good** — NP-COORD-001 Rev A.8 gate records; G1-15, G1-16, G2-10, G2-11, G2-12 CLOSED; G3-07/G3-08 SOFTWARE BASELINED | Formal design review minutes at each gate closure going forward |
| Design verification | §820.30(f) | **Partial** — NP-FAI-ZM-001 checklist defined; software FAI items passed for all firmware modules; hardware FAI pending prototype | FAI execution on prototype hardware constitutes verification evidence |
| Design validation | §820.30(g) | **Not yet started** — Requires device prototype and human factors testing | Planned for Year 2 (T2 development phase) |
| Design transfer | §820.30(h) | **Not yet started** — No manufacturing transfer yet | Required before first production run |
| Design changes | §820.30(i) | **Partial** — Git commit history tracks changes; NP-QMS-DC-001 change order process established | Use NP-QMS-DC-001 change order process from 2026-05-13 forward |
| DHF maintenance | §820.30(j) | **Established** — This document | Maintain index with each new document release |

---

## 8. Future Document Additions

When a new controlled document is created, this index must be updated before the new document is released. The update is a change to NP-DHF-001 and requires approval per §3.3 of NP-QMS-001.

Planned near-term additions:

| Planned doc number | Title | Target date | Trigger |
|---|---|---|---|
| NP-DP-001 | Design and Development Plan | Month 3 | Formal planning document per §820.30(b) |
| NP-DT-001 | Design Input/Output Traceability Matrix | Month 6 | Links requirements to verification evidence |
| NP-HFE-001 | Human Factors Engineering Plan | Month 9 | IEC 62366-1 / FDA HFE guidance |
| NP-PMS-001 | Post-Market Surveillance Plan | Month 12 | Required before product launch |
| NP-FAI-SM-001 | 1064nm Smart Module FAI (hardware items) | Post-prototype | FAI-SM-04, -06, -07, -08 pending hub PCB Rev B Gerber build + optical bench |
| NP-FAI-HD-001 | sLORETA HD-tDCS Hardware FAI | Post-T2 prototype | FAI-HD01, HD03, HD04 pending |
| NP-FAI-CV-001 | Cervical VNS Hardware FAI | Post-T2 prototype | FAI-CV01, CV02, CV03 pending |

---

## 9. Document History

| Rev | Date | Author | Description |
|---|---|---|---|
| A | 2026-05-13 | Interim Quality (CEO) | Initial release. All pre-formation design documents entered retroactively under change control. DHF established at QMS formation. |
| B | 2026-05-17 | Interim Quality (CEO) | Updated: NP-DB-004 (R4) marked SUPERSEDED; NP-DB-005 (R5, neuropulse_design_brief_r5.docx) added as ACTIVE. NP-HW-HUB-001 Rev B added (§5.3). NP-FW-HUB-001 Rev A (hub_control program) added to firmware specs (§5.4) and firmware source code table (§6). NPPS Language Reference added (§5.5). NP-FW-REQ-001 marked SUPERSEDED. NP-MOD-EXT-001 marked SUPERSEDED. NP-RISK-001 bumped to Rev B (RISK-25 added). NP-COORD-001 updated to Rev A.8. CLAUDE.md updated to Rev 9. Relative navigation links added to all File column entries. DHF completeness assessment updated to reflect current gate status (NP-COORD-001 Rev A.8). |
| C | 2026-05-17 | Interim Quality (CEO) | NP-SIM-001 v0.1.0 (Helmet Simulator) added to §5.5 with new SIM category added to §4. CLAUDE.md updated to Rev 11. Note added below §5.5 table clarifying simulator regulatory status. Open sub-issues #77–#80 recorded; parent tracking Issue #81 / PR #76 referenced. |

---

*NP-DHF-001 Rev C — ACTIVE — Effective 2026-05-17*
