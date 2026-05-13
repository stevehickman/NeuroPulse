# NP-DHF-001 Rev A — NeuroPulse Design History File Index

**Document number:** NP-DHF-001  
**Revision:** A  
**Status:** ACTIVE  
**Effective date:** 2026-05-13  
**Author:** Quality Lead (interim: CEO)  
**Approved by:** CEO  
**Next review:** Ongoing — updated with each new design document release

---

## 1. Purpose

This Design History File (DHF) Index serves as the master index of all design documentation for the NeuroPulse device platform under 21 CFR §820.30(j) and ISO 13485:2016 clause 7.3.10. It provides a single reference point to locate any design record and demonstrates that the device was designed in accordance with the approved design plan.

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

---

## 5. Master Document Index

### 5.1 QMS and Quality Documents

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-QMS-001 | QMS Manual | A | 2026-05-13 | `docs/neuropulse_qms_manual_001.md` | ACTIVE | QMS |
| NP-DHF-001 | Design History File Index (this document) | A | 2026-05-13 | `docs/neuropulse_dhf_index_001.md` | ACTIVE | QMS |
| NP-QMS-DC-001 | Design Controls Procedure | A | 2026-05-13 | `docs/neuropulse_design_controls_001.md` | ACTIVE | QMS |
| NP-RM-001 | ISO 14971 Risk Management Plan | A | 2026-05-13 | `docs/neuropulse_risk_mgmt_plan_001.md` | ACTIVE | RISK |
| NP-SW-001 | IEC 62304 Software Development Plan | A | 2026-05-13 | `docs/neuropulse_sw_dev_plan_001.md` | ACTIVE | QMS |
| NP-QMS-CAPA-001 | CAPA Procedure | A | 2026-05-13 | `docs/neuropulse_capa_001.md` | ACTIVE | QMS |

### 5.2 Design Briefs and Product Specifications

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-DB-001 | Design Brief | 1 | pre-2026-05-13 | `docs/neuropulse_design_brief.docx` | SUPERSEDED | REQ |
| NP-DB-002 | Design Brief Revision 2 | 2 | pre-2026-05-13 | `docs/neuropulse_design_brief_r2.docx` | SUPERSEDED | REQ |
| NP-DB-003 | Design Brief Revision 3 | 3 | pre-2026-05-13 | `docs/neuropulse_brief_r3.docx` | SUPERSEDED | REQ |
| NP-DB-004 | Design Brief Revision 4 | 4 | pre-2026-05-13 | `docs/neuropulse_brief_r4.docx` | ACTIVE | REQ |
| — | CLAUDE.md — Project Design Memory | 7 | 2026-05-13 | `CLAUDE.md` | ACTIVE | REQ |

**Note on CLAUDE.md:** CLAUDE.md serves as the living design authority document capturing all locked design decisions and pending items. It is under git version control and constitutes a design record for DHF purposes. Each revision (tracked by git commit) is a controlled design change.

### 5.3 Hardware Specifications

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-HW-FPC-001 | FPC Zone Module Specification | E | 2026-05-13 | `docs/neuropulse_fpc_zone_module_spec_revA.docx` | ACTIVE | SPEC-HW |
| NP-HW-FPC-001 (variant) | FPC Smart Zone Module (1064nm) Hardware Spec | E | 2026-05-13 | `docs/neuropulse_hw_fpc_smart_001.md` | ACTIVE | SPEC-HW |
| NP-PROC-FPC-001 | FPC Procurement Requirements | A | pre-2026-05-13 | `docs/neuropulse_fpc_procurement_requirements.docx` | ACTIVE | PROC |
| NP-PROC-FPC-1064-001 | 1064nm Smart Module Component Procurement | A | 2026-05-12 | `docs/neuropulse_proc_fpc_1064_001.md` | ACTIVE | PROC |

### 5.4 Firmware Specifications

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-FW-EMMC-001 | eMMC Partition Architecture and Storage Encryption | A | 2026-05-11 | `docs/neuropulse_fw_emmc_001.docx` | ACTIVE | SPEC-FW |
| NP-FW-HRV-001 | HRV Biofeedback Protocol Firmware Specification | A | 2026-05-11 | `docs/neuropulse_fw_hrv_001.md` | ACTIVE | SPEC-FW |
| NP-FW-ZA-001 | Zone Module Bone Conduction Announcement Firmware | A | 2026-05-11 | `docs/neuropulse_fw_zone_announce_001.md` | ACTIVE | SPEC-FW |
| NP-FW-HD-001 | sLORETA-Guided HD-tDCS Firmware Specification | A | 2026-05-11 | `docs/neuropulse_fw_sloreta_hdtdcs_001.md` | ACTIVE | SPEC-FW |
| NP-FW-CVNS-001 | Cervical VNS Safety Interlock Firmware Specification | A | 2026-05-11 | `docs/neuropulse_fw_cvns_001.md` | ACTIVE | SPEC-FW |
| NP-FW-PBM1064-001 | 1064nm Smart Zone Module Firmware Specification | A | 2026-05-12 | `docs/neuropulse_fw_pbm1064_001.md` | ACTIVE | SPEC-FW |
| NP-FW-REQ-001 | Firmware Requirements | A | pre-2026-05-13 | `docs/neuropulse_fw_requirements_001.docx` | ACTIVE | REQ |

### 5.5 Session and Application Specifications

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-SES-1064-001 | 1064nm Multi-Wavelength Session Protocol | A | 2026-05-12 | `docs/neuropulse_session_protocol_1064_001.md` | ACTIVE | SES |
| NP-APP-ROADMAP-001 | iOS App Development Roadmap | A | 2026-05-11 | `docs/neuropulse_ios_app_roadmap_001.md` | ACTIVE | APP |

### 5.6 Tooling and Manufacturing Specifications

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-TOOL-ZM-001 | Zone Module Tooling Specification | A | pre-2026-05-13 | `docs/neuropulse_tool_zone_module_001.docx` | ACTIVE | SPEC-TOOL |
| NP-TOOL-ZM-SM-001 | 1064nm Smart Zone Module Tooling Variant | A | 2026-05-12 | `docs/neuropulse_tool_zone_module_smart_001.md` | ACTIVE | SPEC-TOOL |
| NP-TOOL-SHELL-001 | Shell Tooling Specification | A | 2026-05-10 | `docs/neuropulse_tool_shell_001.docx` | ACTIVE | SPEC-TOOL |
| NP-TOOL-LENS-001 | Lens and Goggle Assembly Tooling Specification | B | 2026-05-10 | `docs/neuropulse_tool_lens_001.docx` | ACTIVE | SPEC-TOOL |

### 5.7 Risk Management Records

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-RISK-001 | Zone Module Risk Register (RISK-01 through RISK-25) | A | pre-2026-05-13 | `docs/neuropulse_fpc_zone_module_risks_revA.docx` | ACTIVE | RISK |

**Note:** The risk register containing RISK-01 through RISK-25 (23 MITIGATED, 2 OPEN: RISK-03 regulatory opinion, RISK-20 CFRP Ra confirmation) is formally brought under QMS change control as of 2026-05-13 per NP-RM-001 §5.1. All future risk register updates require change control per NP-QMS-DC-001.

### 5.8 First Article Inspection and Test Records

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-FAI-ZM-001 | Zone Module FAI Checklist | A | pre-2026-05-13 | `docs/neuropulse_fai_zone_module.docx` | ACTIVE | FAI |

### 5.9 Supplier and Procurement Records

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-PROC-SUP-001 | Supplier Selection Checklist | A | pre-2026-05-13 | `docs/neuropulse_supplier_selection_checklist.docx` | ACTIVE | PROC |

### 5.10 Engineering Coordination and Gate Records

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-COORD-001 | Engineering Coordination Checklist | A.7 | 2026-05-11 | `docs/neuropulse_eng_coordination_checklist.docx` | ACTIVE | COORD |
| NP-DRV-SHELL-001 | Shell FPC Routing Review | B | 2026-05-10 | `docs/neuropulse_shell_fpc_routing_review.docx` | ACTIVE | COORD |

### 5.11 Regulatory Strategy Documents

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-REG-CVNS-001 | Cervical VNS 510(k) Q-Sub Pre-Submission | A | 2026-05-11 | `docs/neuropulse_cvns_510k_presub_001.md` | ACTIVE | REG |

### 5.12 Clinical Strategy and Evidence

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-CLIN-001 | Clinical Trials Strategy | A | pre-2026-05-13 | `docs/neuropulse_clinical_trials_strategy.docx` | ACTIVE | CLIN |
| NP-MOD-EXT-001 | Additional Modalities Specification | A | pre-2026-05-13 | `docs/neuropulse_additional_modalities.docx` | ACTIVE | CLIN |
| NP-BIB-001 | Clinical Evidence Bibliography | — | pre-2026-05-13 | `docs/neuropulse_bibliography.docx` | ACTIVE | CLIN |
| NP-SBIR-001 | SBIR Phase I Draft | — | pre-2026-05-13 | `docs/neuropulse_sbir_phase1_draft.docx` | ACTIVE | CLIN |
| — | Researcher Candidate List | — | pre-2026-05-13 | `docs/neuropulse_researchers.docx` | ACTIVE | CLIN |

---

## 6. Firmware Source Code as DHF Records

Under IEC 62304 and 21 CFR §820.30, software source code and associated build artefacts are design outputs and therefore DHF records. The following firmware directories are DHF source code records:

| Firmware item | IEC 62304 class | Repository path |
|---|---|---|
| Dual-bank OTA bootloader | Class B (boundary) | `firmware/bootloader/` |
| HRV biofeedback protocol | Class B | `firmware/hrv_biofeedback/` |
| Zone module bone conduction announcement | Class B | `firmware/zone_announce/` |
| sLORETA-guided HD-tDCS | Class B (T2) | `firmware/sloreta_hdtdcs/` |
| Cervical VNS safety interlock | **Class C** | `firmware/cervical_vns/` |
| 1064nm smart zone module PBM | Class B | `firmware/pbm_1064nm/` |

Each firmware directory is under git version control. Git commit hashes constitute the version record for source code. Release candidates must be tagged per NP-SW-001 §9.

---

## 7. DHF Completeness Assessment (2026-05-13 baseline)

This section identifies design phases and their DHF coverage status.

| Design phase | 21 CFR §820.30 | Coverage | Gap / action |
|---|---|---|---|
| Design planning | §820.30(b) | **Partial** — CLAUDE.md Rev 7 serves as master plan; formal design plan document not yet authored | Author NP-DP-001 Design and Development Plan (Year 1, Month 3) |
| Design inputs | §820.30(c) | **Partial** — Design briefs Rev 1–4 and CLAUDE.md capture inputs; formal requirements specification not yet in structured format | NP-FW-REQ-001 addresses firmware; hardware requirements document needed |
| Design outputs | §820.30(d) | **Good** — Hardware specs, firmware specs, tooling specs present and indexed | Ensure all outputs traceable to inputs; traceability matrix (NP-DT-001) needed |
| Design review | §820.30(e) | **Partial** — Engineering coordination checklist (NP-COORD-001) serves as gate record; formal design review minutes not authored | First formal design review record at each gate closure going forward |
| Design verification | §820.30(f) | **Partial** — FAI checklist (NP-FAI-ZM-001) defined; most FAI items PENDING (require prototype hardware) | FAI execution against checklist constitutes verification evidence |
| Design validation | §820.30(g) | **Not yet started** — Requires device prototype and human factors testing | Planned for Year 2 (T2 development phase) |
| Design transfer | §820.30(h) | **Not yet started** — No manufacturing transfer yet | Required before first production run |
| Design changes | §820.30(i) | **Partial** — Git commit history tracks changes; formal change order process now established by NP-QMS-DC-001 | Use NP-QMS-DC-001 change order process from this date forward |
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
| NP-FAI-SM-001 | 1064nm Smart Module FAI (hardware items) | Post-prototype | FAI-SM-04, -06, -07, -08 pending hardware bench |
| NP-FAI-HD-001 | sLORETA HD-tDCS Hardware FAI | Post-T2 prototype | FAI-HD01, HD03, HD04 pending |
| NP-FAI-CV-001 | Cervical VNS Hardware FAI | Post-T2 prototype | FAI-CV01, CV02, CV03 pending |

---

## 9. Document History

| Rev | Date | Author | Description |
|---|---|---|---|
| A | 2026-05-13 | Interim Quality (CEO) | Initial release. All pre-formation design documents entered retroactively under change control. DHF established at QMS formation. |

---

*NP-DHF-001 Rev A — ACTIVE — Effective 2026-05-13*
