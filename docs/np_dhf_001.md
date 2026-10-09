# NeurOne Design History File Index

**Project:** NeurOne
**Document:** NP-DHF-001
**Revision:** 165
**Date:** 2026-10-09
**Status:** ACTIVE
**Effective Date:** 2026-07-21
**Author:** Steve Hickman (CEO, interim Quality authority)
**Approved By:** Steve Hickman, CEO
**References:** —
**Related Issues:** GitHub Issue #33
**Gate:** —
**IEC 62304 Class:** —
**Applicable Standard:** 21 CFR §820.30(j), ISO 13485:2016 §7.3.10
**Next Review:** Ongoing — updated with each new design document release

---

## 1. Purpose

This Design History File (DHF) Index serves as the master index of all design documentation for the NeurOne device platform under 21 CFR §820.30(j) and ISO 13485:2016 clause 7.3.10. It provides a single reference point to locate any design record and demonstrates that the device was designed in accordance with the approved design plan.

Relative links in the File column are navigable — click any link to open the source document directly. Links are relative to the `docs/` directory. All links assume the repository directory structure is unchanged; if the structure changes, links in this index must be updated.

The DHF Index is a living document — each new controlled document is added at release, and each revision is recorded.

---

## 2. DHF Scope

The DHF covers both device tiers sharing the NeurOne platform:

| Tier | Product | Regulatory pathway |
|---|---|---|
| T1 | NeurOne Home | FDA-exempt wellness (general wellness device) |
| T2 | NeurOne Pro | FDA 510(k) clearance target |

Because T1 design decisions directly feed into T2 (shared chassis, processor stack, firmware architecture, and modality hardware), all T1 design records are included in the DHF as T2 design history.

---

## 3. Initial Entry Declaration

All documents listed in this index that predate the formal QMS effective date of **2026-05-13** are hereby entered retroactively into the Design History File under change control as of that date. Their content represents design decisions made during the pre-formation design phase. No design decision or specification in those documents is considered closed to revision — all remain subject to the design controls procedure (NP-QMS-DC-001) from this date forward.

Change description for all initial-entry documents: **"Initial DHF entry — retroactive entry of pre-formation design document at QMS establishment (NP-QMS-001 Rev 1 effective 2026-05-13)"**

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
| FEAS | Feasibility assessments and competitive analysis (exploratory; not §820.30 design records) |

---

## 5. Master Document Index

### 5.1 QMS and Quality Documents

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-QMS-001 | NeurOne Quality Management System Manual | 1 | 2026-05-13 | [np_qms_001.md](./np_qms_001.md) | ACTIVE | QMS |
| NP-DHF-001 | NeurOne Design History File Index | 165 | 2026-10-09 | [np_dhf_001.md](./np_dhf_001.md) | ACTIVE | QMS |
| NP-QMS-DC-001 | Design Controls Procedure | 2 | 2026-10-04 | [np_qms_dc_001.md](./np_qms_dc_001.md) | ACTIVE | QMS |
| NP-RM-001 | ISO 14971 Risk Management Plan | 1 | 2026-05-13 | [np_rm_001.md](./np_rm_001.md) | ACTIVE | RISK |
| NP-SW-001 | IEC 62304 Software Development Plan | 12 | 2026-10-09 | [np_sw_001.md](./np_sw_001.md) | ACTIVE | QMS |
| NP-QMS-CAPA-001 | Corrective and Preventive Action Procedure | 1 | 2026-05-13 | [np_qms_capa_001.md](./np_qms_capa_001.md) | ACTIVE | QMS |
| NP-DP-001 | Design and Development Plan | 1 | 2026-05-17 | [np_dp_001.md](./np_dp_001.md) | ACTIVE | QMS |
| NP-DT-001 | Design Input/Output Traceability Matrix | 7 | 2026-10-04 | [np_dt_001.md](./np_dt_001.md) | DRAFT | QMS |
| NP-HFE-001 | Human Factors Engineering Plan | 1 | 2026-07-27 | [np_hfe_001.md](./np_hfe_001.md) | ACTIVE | QMS |
| NP-HFE-002 | Accessible Zone-Module Position Identification and Guided Placement | 2 | 2026-08-20 | [np_hfe_002.md](./np_hfe_002.md) | DRAFT | QMS |
| NP-PMS-001 | Post-Market Surveillance Plan | 2 | 2026-07-27 | [np_pms_001.md](./np_pms_001.md) | ACTIVE | QMS |
| NP-IRB-001 | IRB Protocol for Research Consent Architecture Validation | 2 | 2026-08-16 | [np_irb_001.md](./np_irb_001.md) | DRAFT | QMS |

### 5.2 Design Briefs and Product Specifications

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-DB-001 | Design Brief | 1 | 2026-05-02 | [np_db_001.docx](./superseded/np_db_001.docx) | SUPERSEDED by NP-DB-005 | REQ |
| NP-DB-002 | Design Brief | 2 | 2026-05-03 | [np_db_002.docx](./superseded/np_db_002.docx) | SUPERSEDED by NP-DB-005 | REQ |
| NP-DB-003 | Design Brief | 3 | 2026-05-04 | [np_db_003.docx](./superseded/np_db_003.docx) | SUPERSEDED by NP-DB-005 | REQ |
| NP-DB-004 | Design Brief | 4 | 2026-05-07 | [np_db_004.docx](./superseded/np_db_004.docx) | SUPERSEDED by NP-DB-005 | REQ |
| NP-DB-005 | Master Design Brief | 6 | 2026-08-11 | [np_db_005.docx](./np_db_005.docx) | DRAFT — PENDING APPROVAL | REQ |
| NP-HEX-ZM-001 | Hexagonal Standardized Zone-Module Design Brief | 9 | 2026-09-28 | [np_hex_zm_001.md](./np_hex_zm_001.md) | DRAFT | REQ |
| — | CLAUDE.md — Project Design Memory | 68 | 2026-10-08 | [CLAUDE.md](../CLAUDE.md) | ACTIVE | REQ |

**Note on CLAUDE.md:** CLAUDE.md serves as the living design authority document capturing all locked design decisions and pending items. It is under git version control and constitutes a design record for DHF purposes. Each revision (tracked by git commit) is a controlled design change. Rev 24 (2026-06-09) adds: OTA firmware update locked decision entry to §13.5 (opcode wire values, SPKI pinning, fingerprint-first ordering, 27 iOS + 21 firmware tests); OTA document entry in §14 document register; ISA progress updated to 36/164.

### 5.3 Hardware Specifications

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-HW-FPC-001 | FPC Zone Module Specification (base module, ZM-01–ZM-05) | 4 (written as Rev D) | 2026-05-09 | [np_hw_fpc_001.docx](./superseded/np_hw_fpc_001.docx) | SUPERSEDED 2026-07-28 by NP-HEX-ZM-001 | SPEC-HW |
| NP-HW-FPC-001 (variant) | 1064nm Smart Zone Module FPC Layout Variant | 5 | 2026-05-13 | [np_hw_fpc_001.md](./superseded/np_hw_fpc_001.md) | SUPERSEDED 2026-07-28 — see in-doc note | SPEC-HW |
| NP-HW-HUB-001 | Hub PCB — Cranial Socket Interface, I2C Fan-Out and PD Analog Front End | 8 | 2026-09-25 | [np_hw_hub_001.md](./np_hw_hub_001.md) | DRAFT | SPEC-HW |
| NP-HW-HEXTILE-001 | Hex-Tile Module — Electrical / FPC Specification (T1-A, T1-C) | 26 | 2026-09-29 | [np_hw_hextile_001.md](./np_hw_hextile_001.md) | DRAFT | SPEC-HW |
| NP-HW-TCAP-001 | T2 Clinical Electrode Cap — Wiring and Driver Channel Assignment | 2 | 2026-09-20 | [np_hw_tcap_001.md](./np_hw_tcap_001.md) | DRAFT | SPEC-HW |
| NP-HW-AUDIO-001 | Audio Cup Assembly — Hardware Specification | 4 | 2026-09-24 | [np_hw_audio_001.md](./np_hw_audio_001.md) | DRAFT | SPEC-HW |
| NP-HW-NASAL-001 | Intranasal Bilateral Y-Probe and Hygiene Sleeve — Hardware Specification | 2 | 2026-09-23 | [np_hw_nasal_001.md](./np_hw_nasal_001.md) | DRAFT | SPEC-HW |
| NP-HW-VNSCLIP-001 | Auricular VNS / HRV Clip — Hardware Specification | 1 | 2026-09-20 | [np_hw_vnsclip_001.md](./np_hw_vnsclip_001.md) | DRAFT | SPEC-HW |
| NP-HW-CVNS-001 | Cervical VNS Accessory — Electrode Assembly, Cable and Connector Hardware Specification | 3 | 2026-10-01 | [np_hw_cvns_001.md](./np_hw_cvns_001.md) | DRAFT | SPEC-HW |
| NP-HW-TMS-001 | TMS Focal Figure-8 Coil Applicator (T2) — Hardware Specification | 1 | 2026-09-20 | [np_hw_tms_001.md](./np_hw_tms_001.md) | DRAFT | SPEC-HW |
| NP-HW-TACSDRV-001 | 21-Channel Clinical tACS Driver Stage (T2) — Hardware Specification | 4 | 2026-09-23 | [np_hw_tacsdrv_001.md](./np_hw_tacsdrv_001.md) | DRAFT | SPEC-HW |
| NP-PROC-FPC-001 | Emitter Supply, Flex Laminate and Optical Window — Procurement Requirements | 9 | 2026-09-28 | [np_proc_fpc_001.docx](./np_proc_fpc_001.docx) | ACTIVE | PROC |
| NP-PROC-FPC-1064-001 | 1064nm Smart Zone Module — Component Selection and Procurement Specification | 3 | 2026-09-25 | [np_proc_fpc_1064_001.md](./np_proc_fpc_1064_001.md) | BASELINED | PROC |
| NP-FEAS-FNIRS-001 | fNIRS on Existing NeurOne NIR Optics — Feasibility Assessment | 2 | 2026-09-27 | [np_feas_fnirs_001.md](./np_feas_fnirs_001.md) | DRAFT | FEAS |
| NP-FEAS-PBMCH-001 | Independent 660 nm / 808 nm PBM Channel Control — Costed Design Study | 4 | 2026-09-29 | [np_feas_pbmch_001.md](./np_feas_pbmch_001.md) | DRAFT | FEAS |
| NP-PWRSRC-001 | Power-Source Architecture — Multi-Source Supply, Purchase-Time Selection, and the Deliberate-Heat Proposition (Design Study) | 3 | 2026-09-26 | [np_pwrsrc_001.md](./np_pwrsrc_001.md) | DRAFT | SPEC-HW |

**Note on NP-HW-HEXTILE-001:** This is the electrical/FPC counterpart to NP-HEX-ZM-001's mechanical/socket/addressing model, and the successor to the retired NP-HW-FPC-001 for the hex-tile form factor. It is a **DESIGN STUDY, not a tooling baseline** — every numeric value is a derived engineering proposal with its assumption stated inline, not a measured or locked figure, and the document says so in its own status line and §10. It is indexed here for traceability of the design reasoning. It will formalise into design inputs (NP-DT-001) and a baselined hardware specification once its blocking open items close — principally OI-HEXTILE-01 (bezel-width conflict between NP-HEX-ZM-001 §3.1 and NP-THERM-BEZEL-001, which moves every irradiance figure by ±14.5 %) and OI-HEXTILE-02 (base-tile emitter selection, on which the irradiance derivation depends). **Two findings need principal decisions rather than engineering closure:** the ~$920/headset driver-and-metering BOM at 80 populated sockets (§6.4, quantifying the cost NP-HEX-ZM-001 §4a recorded as "not yet quantified"), and the ~6-concurrent-tile power ceiling the existing USB-C PD envelope imposes on whole-vault tiling (§9). The latter also surfaces OI-HEXTILE-09, a safety-adjacent gap in the delivered Protocol v2 wire format: nothing currently prevents a socket-mask protocol from commanding more tiles than the power contract supports. **No existing controlled document was modified by this release** beyond index entries; NP-HW-HUB-001's Rev 3 requirements (OI-HEXTILE-10) are stated as requirements on that future revision, not applied to Rev 2.

**Note on NP-FEAS-FNIRS-001:** This is a competitive-analysis feasibility assessment, **not a §820.30 design record** (no design input, output, or locked decision is created). It is indexed here for traceability of the engineering evaluation. If an fNIRS modality is pursued, its findings would be formalised into design inputs (NP-DT-001) and a hardware/firmware spec at that time. Gating unknown is empirical (far-field optical coupling through hair); resolve via the breath-hold bench in §6 before any spec work.

**Note on NP-FEAS-PBMCH-001:** This is a **costed feasibility study, not a §820.30 design record** — it creates no design input, no design output and no locked decision, and it modifies no code, protocol or other specification. It is indexed here for traceability of the engineering evaluation, on the same basis as NP-FEAS-FNIRS-001. **Its principal finding is a correction of record:** per-channel 660/808 nm current is already the T1-A architecture (NP-HW-HEXTILE-001 §6.2 per-channel FET and sense resistor; §8.1's series strings of different length; NP-FW-PBM1064-001 §5.1's independent `CUR_A`/`CUR_B`), so the change it costs is language, five runtimes and dose accounting — **not hardware**. It is explicitly **contact-neutral and therefore does not gate socket tooling**, and its zero BOM delta keeps it outside OI-HEXTILE-06 and OI-COST-10. If per-channel control is adopted, its findings formalise into NP-NPPS-REF-001 (language), NP-FW-PBM1064-001 (dose attribution and the R-5 restatement) and NP-HW-HEXTILE-001 §2 (R-5 wording). Seven open items OI-PBMCH-01…07, of which OI-PBMCH-04 (`wavelength` never reaches the wire on iOS or Windows) and OI-PBMCH-03 (§6.5's per-channel dose shutdown has no T1-A implementation) are **pre-existing defects it surfaced rather than created**.

**Note on NP-PWRSRC-001:** A **costed design study, not a §820.30 design record** — it creates no design input, no design output and no locked decision, and it modifies no code, protocol, locked CLAUDE.md section or other specification. Indexed here for traceability of the engineering evaluation, on the same basis as NP-FEAS-PBMCH-001 and NP-FEAS-FNIRS-001. **Its central result is a unit change, not a new measurement:** `NP-PWR-BUDGET-001` §4.4.4 argued the second-inlet question in *tiles*, where the thermal and power ceilings read as coincident; restated in the watts `NP-PWR-BUDGET-001` **D-4** requires, the sealed cavity supports **27.6–49.1 W** of aggregate emitter power against an existing 40 W delivery, so **thermally achievable protocol coverage is 2/23 under every candidate source, identical to today**. **Its second result inverts a hazard:** protocols that fit the envelope accumulate ≤3.5 × 10⁻⁶ CEM43 over a 108-session course, while **cascading — `NP-SES-PWR-001` §4's own sanctioned remedy for insufficient power — puts 13 of 20 protocols past the conservative CEM43 reference line**, because the 42 °C interlock caps temperature and has no duration input. Three open items are **BLOCKING and pre-exist the study**: `OI-PWRSRC-10` (the shipping USB-C supply is IEC 62368-1, not IEC 60601-1, on a device with conductive applied parts — blocking for **VE-11**), `OI-PWRSRC-11` (the hub's hold-up is undimensioned on a device with no battery — blocking for `OI-PWR-13`) and `OI-PWRSRC-12` (the thermal budget is not a number the governor can read). If its recommendations are adopted they formalise into CLAUDE.md §4 (the power table, §13), CLAUDE.md §2 (charger policy, §14 — **raised, not changed**), `NP-RISK-004` (twelve rows) and `NP-SW-001` (the three small Class C elements of §16).


### 5.4 Firmware Specifications

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-FW-EMMC-001 | eMMC Partition Architecture and Storage Encryption | 3 | 2026-09-24 | [np_fw_emmc_001.docx](./np_fw_emmc_001.docx) | ACTIVE | SPEC-FW |
| NP-FW-EMMC-002 | Firmware Specification: Privacy Remediation Delta | 5 | 2026-09-27 | [np_fw_emmc_002.md](./np_fw_emmc_002.md) | ACTIVE | SPEC-FW |
| NP-FW-ANON-001 | Research Anonymisation Engine Firmware Specification | 1 | 2026-06-03 | [np_fw_anon_001.md](./np_fw_anon_001.md) | ACTIVE | SPEC-FW |
| NP-FW-HUB-001 | Hub Control Program — Firmware Specification | 22 | 2026-10-07 | [np_fw_hub_001.md](./np_fw_hub_001.md) · [../firmware/hub_control/](../firmware/hub_control/) | DRAFT | SPEC-FW |
| NP-FW-MMSOCK-001 | Multi-Modality Drive at One Hex-Tile Socket — Design Study | 3 | 2026-09-26 | [np_fw_mmsock_001.md](./np_fw_mmsock_001.md) | DRAFT | SPEC-FW |
| NP-FW-HRV-001 | HRV Biofeedback Protocol Firmware Specification | 3 | 2026-09-27 | [np_fw_hrv_001.md](./np_fw_hrv_001.md) | BASELINED | SPEC-FW |
| NP-FW-ZA-001 | Zone Module Bone Conduction Announcement Firmware | 1 | 2026-05-11 | [np_fw_za_001.md](./superseded/np_fw_za_001.md) | SUPERSEDED | SPEC-FW |
| NP-FW-HD-001 | sLORETA-Guided HD-tDCS Firmware Specification | 6 | 2026-09-14 | [np_fw_hd_001.md](./np_fw_hd_001.md) | BASELINED | SPEC-FW |
| NP-FW-CVNS-001 | Cervical VNS Safety Interlock Firmware Specification | 14 | 2026-10-04 | [np_fw_cvns_001.md](./np_fw_cvns_001.md) | BASELINED | SPEC-FW |
| NP-FW-PBM1064-001 | 1064nm Smart Zone Module Firmware Specification | 5 | 2026-09-27 | [np_fw_pbm1064_001.md](./np_fw_pbm1064_001.md) | BASELINED | SPEC-FW |
| NP-FW-NVRAM-001 | Hub NVRAM Hardware Abstraction Layer and the On-Helmet Module Record | 11 | 2026-09-28 | [np_fw_nvram_001.md](./np_fw_nvram_001.md) | DRAFT | SPEC-FW |
| NP-FW-BENCH-001 | Head-Presence Gate and Bench / Service Mode | 5 | 2026-09-27 | [np_fw_bench_001.md](./np_fw_bench_001.md) | DRAFT | SPEC-FW |
| NP-FW-HEXTILE-001 | Hex-Tile On-Module Driver Firmware Specification | 3 | 2026-09-29 | [np_fw_hextile_001.md](./np_fw_hextile_001.md) | DRAFT | SPEC-FW |
| NP-FW-REQ-001 | Zone Module Firmware Requirements | 1 | 2026-05-10 | [np_fw_req_001.docx](./superseded/np_fw_req_001.docx) | SUPERSEDED by individual firmware specs | REQ |

**Note on NP-FW-EMMC-002:** Delta specification created by the privacy analysis programme. All sections take precedence over NP-FW-EMMC-001 Rev 1. §G (SHDR accelerometer reclassification) was added 2026-06-03 by NP-PRIV-001 Rev 2 finding MEDIUM-06; it is BLOCKING for SHDR fleet DB schema freeze (OI-EMMC2-07 CI test must pass before schema is frozen). NP-FW-EMMC-001 Rev 2 will incorporate all delta sections and this document will then be marked INCORPORATED.

**Note on NP-FW-ANON-001:** This specification was created as a direct remediation of NP-PRIV-001 Rev 1 HIGH-02 (k-anonymity insufficient). It supersedes the informally-described anonymisation architecture in CLAUDE.md §5.3 for all firmware implementation purposes. OI-ANON-01 (external DP reviewer sign-off on ε and Δ values) is required before any firmware implementation begins and before FAI-ANON-04/09 can be executed.

### 5.5 Session, Protocol, and Application Specifications

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-SES-1064-001 | 1064nm Smart Zone Module — Session Protocol Specification | 3 | 2026-08-18 | [np_ses_1064_001.md](./np_ses_1064_001.md) | BASELINED | SES |
| NP-APP-ISA-001 | Core iOS App ISA — Ideal State Artifact for Issue #51 | E4 | 2026-06-04 | [../app/ios/ISA.md](../app/ios/ISA.md) | ACTIVE | APP |
| NP-APP-ROADMAP-001 | iOS App Development Roadmap | 5 | 2026-09-23 | [np_app_roadmap_001.md](./np_app_roadmap_001.md) | BASELINED | APP |
| NP-APP-TELEMETRY-001 | App Analytics and Crash Reporting Policy | 2 | 2026-06-03 | [np_app_telemetry_001.md](./np_app_telemetry_001.md) | ACTIVE | APP |
| NP-NPPS-REF-001 | NPPS Language Reference | 19 | 2026-10-07 | [np_npps_ref_001.md](./np_npps_ref_001.md) | ACTIVE | SES |
| NP-API-001 | T2 Clinical Scripting API Specification | 1 | 2026-06-07 | [np_api_001.md](./np_api_001.md) | DRAFT | APP |
| NP-SIM-001 | Helmet Simulator — interactive 3D browser visualisation | v0.3.0 | 2026-05-17 | [../simulator/](../simulator/) | ACTIVE | SIM |

**Note on NP-APP-ROADMAP-001 Rev 2:** The §9 Privacy Constraints added in Rev 2 are binding engineering constraints enforceable under NP-QMS-DC-001. They cannot be overridden without a formal design change order with Privacy Lead sign-off. Open items OI-PA-01 through OI-PA-04 must be resolved before the corresponding features ship.

**Note on NP-SIM-001 v0.3.0:** The helmet simulator is a software design output (browser-based Three.js application). It is not a DHF record in the medical device regulatory sense — it is a design tool, marketing asset, and protocol development aid. It is listed here for completeness as a versioned design output under git control. The simulator source is at `simulator/` in the repository root. v0.1.0 (PR #76): initial 3D scene, session engine, 9 NPPS protocol templates. v0.2.0 (PR #84, PR #85): WebSocket device API (Node.js server ws://localhost:9000); intranasal Y-probe animated insertion/removal; ACCESSORY_CONFIG message type; HOWTO v2.1 (Issues #77, #78 CLOSED). v0.3.0 (branch claude/add-t2-tms-coil-dLbh7): T2 TMS focal figure-8 coil added at DLPFC_L position; CFRP non-conductive window ring; blue-white pulse animation at rTMS rep rate; TMS modality pill in status bar; HOWTO v2.2 (Issue #79 CLOSED). Open sub-issue: #80 (geometry update pending shell CAD finalisation).

### 5.6 Tooling and Manufacturing Specifications

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-TOOL-ZM-001 | Zone Module Tooling Specification | 1 (written as Rev A) | 2026-05-06 | [np_tool_zm_001.docx](./superseded/np_tool_zm_001.docx) | SUPERSEDED 2026-07-28 by NP-TOOL-HEXTILE-001 | SPEC-TOOL |
| **NP-TOOL-HEXTILE-001** | Universal Hex-Tile Module Shell — Tooling Specification | 2 | 2026-09-26 | [np_tool_hextile_001.md](./np_tool_hextile_001.md) | DRAFT | SPEC-TOOL |
| NP-TOOL-ZM-SM-001 | 1064nm Smart Zone Module — Mould Variant Specification | 1 | 2026-05-12 | [np_tool_zm_sm_001.md](./superseded/np_tool_zm_sm_001.md) | SUPERSEDED 2026-07-28 — see in-doc note | SPEC-TOOL |
| NP-TOOL-SHELL-001 | Shell Tooling Specification | 3 | 2026-09-23 | [np_tool_shell_001.docx](./superseded/np_tool_shell_001.docx) | SUPERSEDED 2026-09-25 by NP-TOOL-SHELL-002 | SPEC-TOOL |
| **NP-TOOL-SHELL-002** | Two-Bowl Headset Shell Tooling Specification | 1 | 2026-09-25 | [np_tool_shell_002.md](./np_tool_shell_002.md) | DRAFT | SPEC-TOOL |
| NP-TOOL-LENS-001 | Lens and Goggle Assembly Tooling Specification | 2 | 2026-05-10 | [np_tool_lens_001.docx](./np_tool_lens_001.docx) | ACTIVE | SPEC-TOOL |
| NP-TOOL-HUB-001 | Hub Enclosure Tooling Specification | 4 | 2026-09-25 | [np_tool_hub_001.md](./np_tool_hub_001.md) | BASELINED | SPEC-TOOL |

### 5.7 Risk Management Records

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-RISK-001 | Zone Module Risk Register (RISK-01 through RISK-26) | 3 (written as Rev C) | 2026-07-22 | [np_risk_001.docx](./superseded/np_risk_001.docx) | SUPERSEDED 2026-08-11 by NP-RISK-002/003/004 | RISK |
| **NP-RISK-002** | Risk File Re-Baseline and NP-RISK-001 Disposition | 16 | 2026-10-08 | [np_risk_002.md](./np_risk_002.md) | ACTIVE | RISK |
| **NP-RISK-003** | Hex-Tile Module — Risk Register and Problem Analysis | 3 | 2026-09-29 | [np_risk_003.md](./np_risk_003.md) | ACTIVE | RISK |
| **NP-RISK-004** | Shell, Socket, Interconnect and Hub — Risk Register and Problem Analysis | 4 | 2026-09-25 | [np_risk_004.md](./np_risk_004.md) | ACTIVE | RISK |
| NP-FMEA-001 | SW-01 Safety MCU Unit-Level FMEA | 23 | 2026-10-04 | [np_fmea_001.md](./np_fmea_001.md) | DRAFT | RISK |

**Note:** The risk register (RISK-01 through RISK-25; 23 MITIGATED, 2 OPEN: RISK-03 regulatory opinion, RISK-20 CFRP Ra confirmation) is formally under QMS change control per NP-RM-001 §5.1. All future risk register updates require change control per NP-QMS-DC-001. Privacy risks identified in NP-PRIV-001 Rev 1 are tracked separately in NP-PRIV-REM-001 Rev 1 (not in the device safety risk register, as they are programme-level operational risks rather than device safety hazards).

### 5.8 First Article Inspection and Test Records

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-FAI-ZM-001 | Zone Module FAI Checklist | 1 (written as Rev A) | 2026-05-06 | [np_fai_zm_001.docx](./superseded/np_fai_zm_001.docx) | SUPERSEDED 2026-08-11 by NP-FAI-001, NP-ART-001 and NP-FAI-HUB-001 | FAI |
| **NP-FAI-001** | First Article Inspection Programme | 3 | 2026-09-20 | [np_fai_001.md](./np_fai_001.md) | ACTIVE | FAI |
| **NP-FAI-HUB-001** | Hub Enclosure — First Article Inspection Checklist | 3 | 2026-09-23 | [np_fai_hub_001.md](./np_fai_hub_001.md) | DRAFT | FAI |
| **NP-EMC-EMIRR-001** | ADS1299 EMI Rejection Ratio, 420 MHz – 3 GHz — Bench Test Procedure | 2 | 2026-09-25 | [np_emc_emirr_001.md](./np_emc_emirr_001.md) | DRAFT | FAI |
| **NP-ART-001** | Manufactured Artifact Register and Documentation Readiness | 9 | 2026-09-25 | [np_art_001.md](./np_art_001.md) | ACTIVE | REQ |

| **NP-COST-001** | Configuration Cost Model — Hex-Tile Re-derivation | 3 | 2026-09-09 | [np_cost_001.md](./np_cost_001.md) | ACTIVE | REQ |

### 5.9 Supplier and Procurement Records

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-PROC-SUP-001 | Tooling and Process Supplier Selection Checklist | 1 | 2026-05-06 | [np_proc_sup_001.docx](./np_proc_sup_001.docx) | ACTIVE | PROC |

### 5.10 Engineering Coordination and Gate Records

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| **NP-COORD-001** | Engineering Gate Definitions and Coordination Item Register | 12 | 2026-09-25 | [np_coord_001.md](./np_coord_001.md) | ACTIVE | COORD |
| NP-COORD-001 (Rev 11) | Zone Module FPC Engineering Coordination Checklist | 11 (written as Rev A.10; this row read `A.8` until 2026-09-25, and `document-register.md` read `Rev 1.9` — the `.docx` itself was right) | 2026-07-14 | [np_coord_001.docx](./superseded/np_coord_001.docx) | SUPERSEDED 2026-09-25 by NP-COORD-001 Rev 12 | COORD |
| NP-DRV-SHELL-001 | Shell FPC Routing Review | 2 (written as Rev B) | 2026-05-10 | [np_drv_shell_001.docx](./superseded/np_drv_shell_001.docx) | SUPERSEDED 2026-08-11 by NP-DRV-SHELL-002 and NP-REV-SHELL-001 | COORD |
| NP-DRV-SHELL-002 | Shell Socket Interconnect Architecture | 5 | 2026-09-25 | [np_drv_shell_002.md](./np_drv_shell_002.md) | DRAFT | SPEC-HW |
| **NP-REV-SHELL-001** | Shell Interconnect Design Review Record | 2 | 2026-09-25 | [np_rev_shell_001.md](./np_rev_shell_001.md) | DRAFT | COORD |

**Note on NP-COORD-001:** ~~Rev 1.9 is required to add gate item G3-09 (NP-INT-FHIR-001 FHIR ImplementationGuide approved before first T2 EHR integration pilot), per NP-PRIV-REM-001 STEP-14.~~ **Done at Rev 12 (2026-09-25), §5.** The note stood from 2026-06-03 while Rev 11 was issued without G3-09, and `NP-INT-FHIR-001` cited the item as its gate throughout.

### 5.11 Regulatory Strategy Documents

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-REG-CVNS-001 | Cervical VNS 510(k) Pre-Submission (Q-Sub) Package | 3 | 2026-10-01 | [np_reg_cvns_001.md](./np_reg_cvns_001.md) | BASELINED | REG |
| NP-SW-FAULTMSG-001 | Offline Fault Explanation | 3 | 2026-09-23 | [np_sw_faultmsg_001.md](./np_sw_faultmsg_001.md) | DRAFT | SPEC-FW |
| NP-REG-PBM1064-001 | RISK-03 Regulatory Opinion — Scope Expansion Brief | 2 | 2026-09-28 | [np_reg_pbm1064_001.md](./np_reg_pbm1064_001.md) | DRAFT | REG |
| NP-REG-UPG-001 | T1 → T2 Upgrade Path — Analysis and Decision | 3 | 2026-09-24 | [np_reg_upg_001.md](./np_reg_upg_001.md) | DRAFT | REG |

**Note on NP-REG-PBM1064-001:** Rev 2 (2026-09-28) added scope item 5 (Q14–Q18) and left Q-13 reserved. A further revision is still required to add Q-13 covering Mode F (808-830nm bilateral retinal PBM during normal-looking wear) per NP-FW-EMMC-002 Rev 1 §F and NP-PRIV-REM-001 STEP-18. The Mode F firmware gate flag `NP_MODE_F_REGULATORY_CLEARED` remains 0 until the Rev 2 opinion letter is received.

### 5.12 Clinical Strategy and Evidence

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-CLIN-001 | Clinical Trials Strategy | 1 | 2026-05-02 | [np_clin_001.docx](./np_clin_001.docx) | ACTIVE | CLIN |
| NP-MOD-EXT-001 | Additional Modalities Specification | 1 | 2026-05-02 | [np_mod_ext_001.docx](./superseded/np_mod_ext_001.docx) | SUPERSEDED by individual firmware specs and CLAUDE.md | CLIN |
| NP-BIB-001 | Clinical Evidence Bibliography (39 entries, 12 modality sections) | — | 2026-05-02 | [np_bib_001.docx](./np_bib_001.docx) | ACTIVE | CLIN |
| NP-BIB-1064-001 | 1064nm Transcranial PBM — Clinical Evidence Bibliography Addendum | 1 | 2026-05-13 | [np_bib_1064_001.md](./np_bib_1064_001.md) | ACTIVE | CLIN |
| NP-BIB-PBMIRR-001 | PBM Irradiance and Temperature — Regulatory Limits and Safety Evidence Record | 4 | 2026-10-04 | [np_bib_pbmirr_001.md](./np_bib_pbmirr_001.md) | DRAFT | CLIN |
| NP-SBIR-001 | SBIR Phase I Draft | — | 2026-05-02 | [np_sbir_001.docx](./np_sbir_001.docx) | ACTIVE | CLIN |
| — | Researcher Candidate List | — | 2026-05-02 | [neurone_researchers.docx](./neurone_researchers.docx) | ACTIVE | CLIN |

### 5.13 Privacy and Security Documents

Privacy and security documents are design programme records under NP-QMS-001. They are not device design records in the §820.30 sense but are required operational and compliance documents that support T1 and T2 launch readiness. They are indexed here for completeness and traceability.

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-PRIV-001 | Privacy Analysis and Repair | 1 | 2026-06-02 (HIGH-01 conclusion: 2026-08-12) | [np_priv_001.pdf](./np_priv_001.pdf) | ACTIVE | PRIV |
| NP-PRIV-REM-001 | Privacy Remediation Master Plan | 5 | 2026-09-14 | [np_priv_rem_001.md](./np_priv_rem_001.md) | ACTIVE | PRIV |
| NP-SEC-BR-001 | Breach Response Plan | 1 | 2026-06-02 | [np_sec_br_001.md](./np_sec_br_001.md) | ACTIVE | SEC |
| NP-PROC-POA-001 | Healthcare Power of Attorney Upload Procedure | 1 | 2026-06-02 | [np_proc_poa_001.md](./np_proc_poa_001.md) | ACTIVE | PRIV |
| NP-LEGAL-BAA-001 | Business Associate Agreement Template | 1 | 2026-06-03 | [np_legal_baa_001.md](./np_legal_baa_001.md) | DRAFT | PRIV |
| NP-PRIV-AUDIT-001 | NeurOne App Privacy Audit | 1 | 2026-06-03 | [np_priv_audit_001.md](./np_priv_audit_001.md) | ACTIVE | PRIV |
| NP-PRIV-NOTICE-001 | NeurOne Privacy Notice | 4 | 2026-08-16 | [np_priv_notice_001.md](./np_priv_notice_001.md) | ACTIVE | PRIV |
| NP-INT-FHIR-001 | FHIR R4 ImplementationGuide: NeurOne T2 Clinical Profile | 1 | 2026-06-03 | [np_int_fhir_001.md](./np_int_fhir_001.md) | ACTIVE | PRIV |

Also privacy-relevant, indexed once in their own subsection: NP-APP-TELEMETRY-001 (§5.5) and NP-FW-EMMC-002 (§5.4).

### 5.14 Mechanical, Thermal, and Environmental Design Studies (Helmet Geometry Programme)

These thirteen documents are the design-study output of the helmet layer-geometry work (2026-07-21; NP-THERM-CFD-R1-001 added 2026-07-22). They derive the four-station helmet layer stack (L0 per-module sealed faces → L1 socket layer → L2/L3 bonded EMF shell) from physical requirements, and follow the thermal-safety thread it opens through to a firmware module-boundary decision. **None is tooling-locked** — each is a design study whose numbers are provisional pending THERM-1a (the conjugate-heat CFD) and component datasheets. They are indexed here for traceability and will formalise into design inputs (NP-DT-001) and hardware/firmware specs as their gates close. Where a study surfaces a new hazard or requirement (NP-FMEA-GEOM-001 → FMEA-G07-01; NP-REQ-FANHEALTH-001 → SR-FAN-01…06), registering it into NP-RISK-001 / NP-SW-001 under change control is a tracked follow-on. At Rev 21 no existing controlled document was modified — cross-references into CLAUDE.md and prior specs were citations only. **Rev V (2026-07-22)** registers the THERM-1a first-pass outcome (NP-THERM-CFD-R1-001) into the cluster: NP-REQ-FANHEALTH-001 §4a (Path A rejected, Path B1 selected, provisional SR-FAN-03/04 constants), NP-FMEA-GEOM-001 (FMEA-G07-01 closure path), NP-HELMET-GEOM-001 §8 (THERM-1a gate status), and adopts the BN-boss conductive export as base thermal design (completed-decisions.md). The NP-RISK-001 `.docx` **RISK-26 line assigned (register Rev 2→C)** for FMEA-G07-01, with the Path B1 mitigation/closure path and OPEN status.

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-HELMET-GEOM-001 | NP-HELMET-GEOM-001 — Helmet Layer Geometry & Material Constraints | 2 | 2026-09-23 | [np_helmet_geom_001.md](./np_helmet_geom_001.md) | DRAFT | SPEC-HW |
| NP-HELMET-GEOM-ISA | ISA — NeurOne Helmet Layer Geometry (NP-HELMET-GEOM) | 1 | 2026-07-21 | [np_helmet_geom_isa.md](./np_helmet_geom_isa.md) | DRAFT | SPEC-HW |
| NP-THERM-BEZEL-001 | NP-THERM-BEZEL-001 — THERM-1 / BEZEL-1 Coupling Analysis | 1 | 2026-07-21 | [np_therm_bezel_001.md](./np_therm_bezel_001.md) | DRAFT | SPEC-HW |
| NP-THERM-CFD-001 | NP-THERM-CFD-001 — THERM-1a Conjugate-Heat CFD Boundary Conditions | 1 | 2026-07-21 | [np_therm_cfd_001.md](./np_therm_cfd_001.md) | DRAFT | SPEC-HW |
| NP-THERM-CFD-C2-001 | NP-THERM-CFD-C2-001 — Case C2 Go/No-Go Run Card (Path-A admissibility) | 1 | 2026-07-21 | [np_therm_cfd_c2_001.md](./np_therm_cfd_c2_001.md) | DRAFT | SPEC-HW |
| NP-THERM-CFD-R1-001 | NP-THERM-CFD-R1-001 — THERM-1a First-Pass Analysis Results (C2 / C3 / C6) + BN-Boss Conductive-Export Study | 2 | 2026-09-24 | [np_therm_cfd_r1_001.md](./np_therm_cfd_r1_001.md) | DRAFT | SPEC-HW |
| NP-THERM-CFD-N1-001 | N-Tile Aggregate Thermal Analysis — The Concurrency Ceiling on the Real Lattice | 2 | 2026-09-24 | [np_therm_cfd_n1_001.md](./np_therm_cfd_n1_001.md) | DRAFT | SPEC-HW |
| NP-THERM-SINK-001 | Rejection Specification at the Via Terminus — What `R_sink` Is, and What Is Attached To It | 3 | 2026-09-24 | [np_therm_sink_001.md](./np_therm_sink_001.md) | DRAFT | SPEC-HW |
| NP-THERM-BOWL-001 | Outer-Bowl Heat Budget After the Layer 4 Deletion — Bowl and Coil Temperature, and the Helmholtz Transfer Function | 2 | 2026-09-23 | [np_therm_bowl_001.md](./np_therm_bowl_001.md) | DRAFT | SPEC-HW |
| NP-PWR-THERM-001 | The Power Envelope Against a Specified Rejection Path — the T1 Source, the Second Inlet, and the First Electrical Specification of the TMS Coil | 2 | 2026-09-23 | [np_pwr_therm_001.md](./np_pwr_therm_001.md) | DRAFT | SPEC-HW |
| NP-FMEA-GEOM-001 | NP-FMEA-GEOM-001 — Helmet Layer-Stack Hardware FMEA | 1 | 2026-07-21 | [np_fmea_geom_001.md](./np_fmea_geom_001.md) | DRAFT | RISK |
| NP-REQ-FANHEALTH-001 | NP-REQ-FANHEALTH-001 — Forced-Convection (Fan) Health Thermal Interlock Requirement | 1 | 2026-07-21 | [np_req_fanhealth_001.md](./np_req_fanhealth_001.md) | DRAFT | REQ |
| NP-PLAN-FANHEALTH-001 | NP-PLAN-FANHEALTH-001 — Fan-Health Interlock Sequenced Work Plan | 1 | 2026-07-21 | [np_plan_fanhealth_001.md](./np_plan_fanhealth_001.md) | DRAFT | COORD |
| NP-ENV-001 | NP-ENV-001 — Environmental Envelopes: Survival/Warranty vs. Operating | 1 | 2026-07-21 | [np_env_001.md](./np_env_001.md) | DRAFT | REQ |
| NP-ENV-OPRANGE-001 | NP-ENV-OPRANGE-001 — Per-Modality Operating-Range Working Spec | 1 | 2026-07-21 | [np_env_oprange_001.md](./np_env_oprange_001.md) | DRAFT | REQ |
| NP-FW-POE-001 | NP-FW-POE-001 — Protocol Operating-Envelope Encoding | 1 | 2026-07-21 | [np_fw_poe_001.md](./np_fw_poe_001.md) | DRAFT | SPEC-FW |
| NP-FW-M09-ARCH-001 | NP-FW-M09-ARCH-001 — Architecture Decision: Operating-Envelope Gate as a new SW-01 module (SW01-M11) vs. extending SW01-M04 | 1 | 2026-07-21 | [np_fw_m09_arch_001.md](./np_fw_m09_arch_001.md) | DRAFT | SPEC-FW |
| NP-EMC-CAV-001 | Enclosure Cavity Resonance — Source, Band, and the Layer 4 Requirement | 23 | 2026-10-04 | [np_emc_cav_001.md](./np_emc_cav_001.md) | ACTIVE | SPEC-HW |

---

### 5.15 Documents indexed at the OI-CONV-07 reconciliation (2026-09-25)

These controlled documents had **no master-index row** in §5.1–§5.14 when `OI-CONV-07` reconciled the registers against the files (GitHub #394). Rev and Date are taken from each file's own front matter, and Category from the serial's family. **Moving a row into its thematic subsection is a later, reviewed edit.** Until then this subsection is where they are indexed, and `scripts/check-dhf-index.ts` keeps every row's Rev equal to its file's.

| Doc number | Title | Rev | Date | File | Status | Category |
|---|---|---|---|---|---|---|
| NP-ACC-PRIORITY-001 | Accessory and Companion-Software Priority Set | 4 | 2026-09-22 | [np_acc_priority_001.md](./np_acc_priority_001.md) | DRAFT | COORD |
| NP-ANALYTICS-001 | NeurOne PostHog Analytics — Self-Hosted Configuration Reference | 1 | 2026-06-13 | [np_analytics_001.md](./np_analytics_001.md) | ACTIVE | APP |
| NP-BIB-EMF-001 | EMF Shielding — Evidence Base and Claim Substantiation Record | 6 | 2026-09-23 | [np_bib_emf_001.md](./np_bib_emf_001.md) | ACTIVE | CLIN |
| NP-COND-LINK-001 | NP-COND-LINK-001 — Condition external-link UI | 1 | 2026-07-18 | [np_cond_link_001.md](./np_cond_link_001.md) | ACTIVE | CLIN |
| NP-CONV-001 | NeurOne Naming and Notation Conventions | 11 | 2026-10-01 | [np_conv_001.md](./np_conv_001.md) | ACTIVE | QMS |
| NP-HW-EEGNET-001 | EEG Electrode Net — Architecture, Sizing, and Interference Control | 8 | 2026-08-31 | [np_hw_eegnet_001.md](./np_hw_eegnet_001.md) | DRAFT | SPEC-HW |
| NP-HW-FITOVER-001 | Fit-Over Goggle Assembly — Concept Specification (Deferred) | 2 | 2026-09-26 | [np_hw_fitover_001.md](./np_hw_fitover_001.md) | DRAFT | SPEC-HW |
| NP-INFRA-001 | NeurOne Infrastructure Setup Guide | 1 | 2026-06-14 | [np_infra_001.md](./np_infra_001.md) | ACTIVE | APP |
| NP-MOD-ID-001 | Module Identity, History Portability, and Fleet Characterisation Data Programme | 1 | 2026-08-11 | [np_mod_id_001.md](./np_mod_id_001.md) | DRAFT | SPEC-FW |
| NP-OPT-PSF-001 | NP-OPT-PSF-001 — Transcranial PBM Optical Point-Spread Function and Spatial Resolution Floor | 2 | 2026-09-29 | [np_opt_psf_001.md](./np_opt_psf_001.md) | ACTIVE | SPEC-HW |
| NP-PRIV-ANALYSIS-002 | Privacy Analysis and Repair | 2 | 2026-06-05 | [np_priv_analysis_002.md](./np_priv_analysis_002.md) | ACTIVE | PRIV |
| NP-PRIV-ANALYSIS-003 | Privacy Analysis — Consumable Tracker Feature | 3 | 2026-06-08 | [np_priv_analysis_003.md](./np_priv_analysis_003.md) | ACTIVE | PRIV |
| NP-PRIV-KEYSPLIT-001 | Analytics Gate Architecture — Design Spec | 1 | 2026-06-16 | [np_priv_keysplit_001.md](./np_priv_keysplit_001.md) | ACTIVE | PRIV |
| NP-PWR-BUDGET-001 | Power Budget — Reverse-Engineered from Safety Ceilings (Design Study) | 4 | 2026-08-21 | [np_pwr_budget_001.md](./np_pwr_budget_001.md) | DRAFT | SPEC-HW |
| NP-SES-PWR-001 | PBM Session Power Audit — Predefined Protocol Library Against the Concurrency Ceiling | 2 | 2026-10-04 | [np_ses_pwr_001.md](./np_ses_pwr_001.md) | DRAFT | SES |
| NP-SOUP-BTLE-001 | btleplug SOUP Inventory — Desktop Hub Link (Companion App) | 2 | 2026-10-08 | [np_soup_btle_001.md](./np_soup_btle_001.md) | DRAFT | APP |
| NP-SOUP-CMSIS-001 | CMSIS SOUP Anomaly Evaluation — Safety MCU (SW-01, Class C) | 1 | 2026-08-09 | [np_soup_cmsis_001.md](./np_soup_cmsis_001.md) | DRAFT | SPEC-FW |
| NP-SOUP-LFS-001 | LittleFS SOUP Record and Hazard Analysis — Hub Storage (SW-02, Class B) | 15 | 2026-09-28 | [np_soup_lfs_001.md](./np_soup_lfs_001.md) | DRAFT | SPEC-FW |
| NP-SW-CI-001 | Firmware Cross-Compile Continuous Integration Plan | 33 | 2026-09-28 | [np_sw_ci_001.md](./np_sw_ci_001.md) | DRAFT | APP |
| NP-SW-PORTAL-API-001 | Device-Facing Consent Channel — Study Descriptors and Clinician Portal | 2 | 2026-09-14 | [np_sw_portal_api_001.md](./np_sw_portal_api_001.md) | DRAFT | APP |
| NP-THERM-COOL-001 | Cooling Architecture Options for the Sealed Cavity — Airflow, Liquid, Stored Coolth, and the Ambient Lever | 16 | 2026-09-23 | [np_therm_cool_001.md](./np_therm_cool_001.md) | DRAFT | SPEC-HW |

---

## 6. Firmware Source Code as DHF Records

Under IEC 62304 and 21 CFR §820.30, software source code and associated build artefacts are design outputs and therefore DHF records. The following firmware directories are DHF source code records:

| Firmware item | IEC 62304 class | Document | Repository path |
|---|---|---|---|
| OTA state host-testable C module | Class B (host-testable) | NP-APP-ISA-001 ISC-111 | [firmware/ota/](../firmware/ota/) |
| Dual-bank OTA bootloader | Class B (boundary) | NP-FW-EMMC-001 Rev 1 §8 | [firmware/bootloader/](../firmware/bootloader/) |
| Shared crypto library (np_crypto — OI-SW01-M07-02 CLOSED) | **Class C** (consumed by SW-01) / Class B (consumed by SW-02) | NP-SW-001 Rev 1 §9.4 SOUP table; `firmware/crypto/vendor/monocypher/VERSION` | [firmware/crypto/](../firmware/crypto/) |
| Vendored FreeRTOS-Kernel (SW-02 RTOS) | Class B (SOUP) | NP-SW-001 Rev 1 §9.4 SOUP table; `firmware/vendor/freertos/VERSION` (V11.3.0, LTS 202604.00, MIT). Config + hooks first-party in hub_control. Host smoke test `np_freertos_smoke_tests` green | [firmware/vendor/freertos/](../firmware/vendor/freertos/) |
| Vendored littlefs `v2.11.3` (SW-02 storage) | Class B (SOUP) — **conditionally; see NP-SOUP-LFS-001 §6.2 and `REQ-LFS-01`** | NP-SW-001 Rev 9 §9.4 SOUP table; **NP-SOUP-LFS-001 Rev 9** (hazard analysis §4–§6; **IEC 62304 §7.1.2 anomaly evaluation §11; power-loss verification §12**); `firmware/vendor/littlefs/VERSION` (tag `v2.11.3`, commit `6cb4e865`, BSD-3-Clause, per-file SHA-256). Configuration first-party in hub_control (`np_lfs_config.h`, `np_lfs_instance.{h,c}`); **caller rules first-party in hub_control (`np_cfg_store.{h,c}`, `np_lfs_log_instance.{h,c}`, NP-SOUP-LFS-001 Rev 4 §13)**; host tests `np_lfs_config_tests`, `np_lfs_powerloss_tests`, `np_cfg_store_tests` and `np_lfs_log_instance_tests` green; `REQ-LFS-01` gated by `scripts/check-lfs-caller-rules.ts` | [firmware/vendor/littlefs/](../firmware/vendor/littlefs/) |
| Hub control program | Class B | NP-FW-HUB-001 Rev 8 §9 (`docs/np_fw_hub_001.md`, authored 2026-09-13) | [firmware/hub_control/](../firmware/hub_control/) |
| HRV biofeedback protocol | Class B | NP-FW-HRV-001 Rev 1 | [firmware/hrv_biofeedback/](../firmware/hrv_biofeedback/) |
| Zone module bone conduction announcement | Class B | NP-FW-ZA-001 Rev 1 | [firmware/zone_announce/](../firmware/zone_announce/) |
| sLORETA-guided HD-tDCS | Class B (T2) | NP-FW-HD-001 Rev 1 | [firmware/sloreta_hdtdcs/](../firmware/sloreta_hdtdcs/) |
| Cervical VNS safety interlock | **Class C** | NP-FW-CVNS-001 Rev 1 | [firmware/cervical_vns/](../firmware/cervical_vns/) |
| 1064nm smart zone module PBM | Class B | NP-FW-PBM1064-001 Rev 1 | [firmware/pbm/](../firmware/pbm/) |
| Hex-tile on-module firmware (SW-04, U1 on every tile) | Class B | NP-FW-HEXTILE-001 Rev 1 | — (not yet written) |
| Two-layer UHDR key scheme (UKMD + WKMD; Argon2id + hardware binding) | Class B | NP-FW-EMMC-002 Rev 1 §C | [firmware/uhdr_key/](../firmware/uhdr_key/) |
| SHDR accelerometer processing — §G standing rule (two derived booleans) + §H characterisation window (coarsened impact histogram, record-denominated fail-closed expiry) | Class B | NP-FW-EMMC-002 Rev 2 §G, §H | [firmware/shdr/](../firmware/shdr/) |
| Device factory reset (SANITIZE + SHDR wipe + warranty-token clear) | Class B | NP-FW-EMMC-002 Rev 1 §B | [firmware/factory_reset/](../firmware/factory_reset/) |
| Research-anonymization Scratch encryption (AES-256-CTR, SRAM-only key, SANITIZE post-run) | Class B | NP-FW-EMMC-002 Rev 1 §D | [firmware/anon/](../firmware/anon/) |
| EDF+ privacy header writer + validator (opaque token; sex/DOB/name = 'X') | Class B | NP-FW-EMMC-002 Rev 1 §E | [firmware/edf/](../firmware/edf/) |

Each firmware directory is under git version control. Git commit hashes constitute the version record for source code. Release candidates must be tagged per NP-SW-001 §9.

**Note on the vendored littlefs record (moved from its Repository path cell, 2026-10-04, verbatim):** **pinned 2026-09-14 (`OI-LFS-01`), evaluated and power-loss tested 2026-09-14 (`OI-LFS-02`), caller rules built 2026-09-24 (`OI-LFS-03…09`), and still NOT INTEGRATED.** `L-1…L-4` hold against the `struct lfs_config` contract, not against the eMMC (`OI-LFS-07`); all three instances match NP-FW-EMMC-001 Rev 3's EMMC-FS-01 exactly (`OI-LFS-10` closed, `ECR-EMMC-002` applied, NP-SOUP-LFS-001 §13.9).

**Note on `firmware/cmake/`:** This directory contains the CMake toolchain and cross-compilation configuration for the arm-none-eabi build environment. It is build infrastructure, not a firmware module, and is not a separate DHF record. It is covered implicitly by the build reproducibility requirement in NP-SW-001 §9.3.

**Note on privacy-related firmware modules (NP-FW-EMMC-002):** Four of the modules specified in NP-FW-EMMC-002 Rev 1 are now authored and are listed as DHF source-code records in the table above: **§B device factory reset** ([firmware/factory_reset/](../firmware/factory_reset/)), **§C two-layer UHDR key scheme** ([firmware/uhdr_key/](../firmware/uhdr_key/)), **§D research-anonymization Scratch encryption** ([firmware/anon/](../firmware/anon/)), and **§E EDF+ privacy header writer/validator** ([firmware/edf/](../firmware/edf/)). Each has an `include/`, `src/`, and host-testable `tests/` tree under git version control, with the NP-FW-EMMC-002 section cited in the module headers. The remaining two — **§A warranty token** and **§F Mode F** — do not yet have dedicated source directories: the warranty-token linkage is currently realised app-side (`SHDRUploader` Keychain token) pending the hub GATT characteristic, and Mode F is a compile-gated code path inside `firmware/hub_control/` (`NP_MODE_F_REGULATORY_CLEARED`), not a standalone module. When either gains a dedicated directory it will be added to the table. The specification in NP-FW-EMMC-002 Rev 1 remains the design input for all six.

### 6b. iOS Application Source Code Records (SW-03, IEC 62304 Class B)

The iOS application source code is a Class B software item under NP-SW-001. Source is at `app/ios/NeurOne/`. All Swift source files are under git version control, and so is the XcodeGen specification `app/ios/project.yml`. **`app/ios/NeurOne.xcodeproj` is not: since 2026-09-14 (GitHub Issue #189) it is generated from that specification and git-ignored**, the arrangement `app/watchos/` has used since PR #190. The configuration-controlled design output is therefore the spec, which is a text file whose changes are reviewable in a diff; the project file is a build output, regenerated by `ios-ci.yml` before every `xcodebuild` invocation, and cannot drift from the specification because no tracked copy of it exists. PR #106 (feature/ios-parallel-integration, 2026-06-04) constitutes the first substantive design output delivery for Issue #51.

| Module group | IEC 62304 class | Specification | Source path |
|---|---|---|---|
| BLE GATT layer (NeurOneGATTManager, GATTCharacteristics) | Class B | NP-APP-ISA-001 ISC-11–20 | `app/ios/NeurOne/BLE/` |
| Session display — Mode 1 Connected (SessionView) | Class B | NP-APP-ISA-001 ISC-21–34 | `app/ios/NeurOne/Views/SessionView.swift` |
| Protocol upload — Mode 2 (SessionProtocolUploader, ProtocolChunker) | Class B | NP-APP-ISA-001 ISC-35–47 | `app/ios/NeurOne/Session/`, `app/ios/NeurOne/Protocol/ProtocolChunker.swift` |
| Session history + EDF download — Mode 4 (SessionHistoryView, AdaptiveAdjustmentsCard) | Class B | NP-APP-ISA-001 ISC-48–55 | `app/ios/NeurOne/Views/SessionHistoryView.swift` |
| UHDR key management (UHDRKeyManager, UHDRBackupScheduler) | Class B | NP-APP-ISA-001 ISC-56–63 | `app/ios/NeurOne/Data/` |
| SHDR upload (SHDRUploader) | Class B | NP-APP-ISA-001 ISC-64–67 | `app/ios/NeurOne/Data/SHDRUploader.swift` |
| Clinical consent engine (ConsentEngine, ConsentStore) | Class B | NP-APP-ISA-001 ISC-68–82 | `app/ios/NeurOne/Consent/` |
| Privacy compliance (AgeGateView, Under16View, HealthKitSessionReader) | Class B | NP-APP-ISA-001 ISC-83–97 | `app/ios/NeurOne/Onboarding/`, `app/ios/NeurOne/Data/HealthKitSessionReader.swift` |
| Consumable tracker (ConsumableTracker) | Class B | NP-APP-ISA-001 ISC-98–106 | `app/ios/NeurOne/Consumable/` |
| OTA firmware update (OTAManager, FirmwareUpdateService, OTAModels, OTAView) | Class B | NP-APP-ISA-001 ISC-107–113 | `app/ios/NeurOne/OTA/`, `app/ios/NeurOne/Models/OTAModels.swift` |
| Hardware setup wizard (HardwareSetupManager, SetupView) | Class B | NP-APP-ISA-001 ISC-114–121 | `app/ios/NeurOne/Setup/` |
| Apple Watch bridge (PhoneSessionManager) | Class B | NP-APP-ISA-001 ISC-122–125 | `app/ios/NeurOne/WatchBridge/` |
| App entry point and service wiring (NeurOneApp) | Class B | NP-APP-ISA-001 | `app/ios/NeurOne/NeurOneApp.swift` |

**Test target:** `app/ios/NeurOneTests/` — 8 XCTest suites, 76 tests, all passing as of 2026-06-09. Three `XCTExpectFailure` entries document known validator gaps (charge density, PBM dose, zero-duration) requiring follow-up implementation. `OTAManagerTests.swift` (27 tests, added 2026-06-09) covers ISC-107–113: manifest fetch, version comparison, opcode wire values, phase state machine, session progress, SPKI cert pinning (correct DER header), fingerprint-first privacy ordering, download failure wrapping. `SessionDisplayTests.swift` (5 tests) covers ISC-30 predicate and epoch mapping. Additionally: `firmware/ota/tests/np_ota_tests.c` 21 host tests — OTA state record CRC, validate, increment_attempts, bank bounds, attempt limit.

---

## 7. DHF Completeness Assessment

This section identifies design phases and their DHF coverage status.

| Design phase | 21 CFR §820.30 | Coverage | Gap / action |
|---|---|---|---|
| Design planning | §820.30(b) | **Good** — NP-DP-001 Rev 1 (2026-05-17) is the formal design and development plan; CLAUDE.md Rev 17 + NP-COORD-001 Rev 1.8 are the operational design planning instruments | NP-DP-001 to be updated at each gate review; ~~NP-COORD-001 Rev 1.9 required for G3-09 (FHIR IG gate)~~ G3-09 issued at NP-COORD-001 Rev 12 (2026-09-25); NP-DP-001 §6.3/§10.1 still describe the Rev 11 checklist (`OI-COORD-02`) |
| Design inputs | §820.30(c) | **Good** — NP-DT-001 Rev 1 (2026-06-07) provides 57 formal design inputs (DI-PERF/SAFE/USE/REG/INT) with full DI→DO→VE traceability matrix; design briefs Rev 1–5 and CLAUDE.md capture remaining inputs | NP-DT-001 OI-DT-01..05 require resolution; hardware requirements to be traced as hardware matures |
| Design outputs | §820.30(d) | **Good** — Hardware specs, firmware specs (7 core modules + 4 authored NP-FW-EMMC-002 modules: uhdr_key §C, factory_reset §B, anon §D, edf §E), tooling specs present and indexed; hub_control program written; NP-DT-001 Rev 1 provides 35 mapped design outputs with traceability to inputs | NP-DT-001 OI-DT-01..05 outstanding; NP-FW-EMMC-002 §A (warranty token) and §F (Mode F) remain spec-only, flagged in §6 note; helmet-geometry mechanical/thermal/environmental **design studies** indexed in §5.14 (not yet locked outputs — provisional pending THERM-1a + datasheets) |
| Design review | §820.30(e) | **Good** — NP-COORD-001 Rev 1.8 gate records; G1-15, G1-16, G2-10, G2-11, G2-12 CLOSED; G3-07/G3-08 SOFTWARE BASELINED | Formal design review minutes at each gate closure going forward; ~~G3-09 to be added in NP-COORD-001 Rev 1.9~~ done at Rev 12. **NP-COORD-001 Rev 1.8 ≡ Rev 9** under Rev 12 §6's mapping; from Rev 12 each gate review is a per-artifact disposition over NP-ART-001 §2 |
| Design verification | §820.30(f) | **Weak** — NP-FAI-ZM-001 superseded 2026-08-11; NP-ART-001 §3.2 records that **fourteen of the fifteen artifact FAI checklists cannot yet be written** (eleven §3.2 rows — the last covers four artifacts at once), each with its blocking open item named. NP-FAI-HUB-001 is the only artifact checklist issued. Software FAI items passed for all firmware modules. **OI-ART-05 closed 2026-09-14 (Issue #343):** of the five NP-FAI-* serials this index and the document set cited as controlled documents, **four were duplicate serials** for FAI sections that already exist in firmware specifications (NP-FW-PBM1064-001 §11, NP-FW-HD-001 §12, NP-FW-CVNS-001 §9, NP-FW-ANON-001 §9) and are retired in §8 below; **NP-FAI-CVNS-001 is the one real absence** and stays named-but-unwritten per NP-FAI-001 §2. No FAI item number or accept criterion changed | FAI execution on prototype hardware constitutes verification evidence; A14 hardware specification (#332) gates the only genuinely missing checklist |
| Design validation | §820.30(g) | **Partial** — NP-HFE-001 Rev 1 (2026-07-27) provides the human factors validation plan (URRA, formative and summative test protocols); NP-PMS-001 Rev 1 (2026-07-27) provides the post-launch validation feedback loop; actual formative/summative testing and clinical/software validation require device prototype | Formative testing targeted Month 10–12 (NP-HFE-001 §7); summative Month 14–18 (NP-DT-001 VE-12); Year 2 T2 clinical validation |
| Design transfer | §820.30(h) | **Not yet started** — No manufacturing transfer yet | Required before first production run |
| Design changes | §820.30(i) | **Partial** — Git commit history tracks changes; NP-QMS-DC-001 change order process established | Use NP-QMS-DC-001 change order process from 2026-05-13 forward |
| DHF maintenance | §820.30(j) | **Established** — This document | Maintain index with each new document release |
| **Privacy programme** | FTC Act §5; HIPAA; GDPR Art. 25; BIPA; MHMD | **Active** — NP-PRIV-001 Rev 1 + Rev 2 analyses complete (26 total findings); NP-PRIV-REM-001 Rev 2 calendar (36 steps); STEP-01–09 complete, STEP-10/11 specs authored; NP-FW-ANON-001, NP-LEGAL-BAA-001, NP-INT-FHIR-001, NP-FW-EMMC-002 §G all authored 2026-06-03 | STEP-12 through STEP-36 tracked in NP-PRIV-REM-001 Rev 2; BIPA (STEP-34) biometric written release and MHMD (STEP-35) no-sale + standalone authorization are applied to ALL users universally (2026-07-10) — the associated legal opinions confirm scope and do NOT gate activation; tabletop exercise required before T1 launch |

---

## 8. Future Document Additions

When a new controlled document is created, this index must be updated before the new document is released. The update is a change to NP-DHF-001 and requires approval per §3.3 of NP-QMS-001.

Planned near-term additions:

| Planned doc number | Title | Target | Trigger |
|---|---|---|---|
| ~~NP-DP-001~~ | ~~Design and Development Plan~~ | ~~Month 3~~ | **COMPLETE — NP-DP-001 Rev 1 released 2026-05-17** |
| NP-FW-EMMC-001 Rev 2 | eMMC Partition Architecture — incorporates all NP-FW-EMMC-002 §A–§G delta sections | Month 6 | When firmware team begins implementation; NP-FW-EMMC-002 becomes INCORPORATED |
| ~~NP-FW-ANON-001~~ | ~~Research Anonymisation Engine Firmware Specification~~ | ~~G1~~ | **COMPLETE — NP-FW-ANON-001 Rev 1 released 2026-06-03** |
| ~~NP-INT-FHIR-001~~ | ~~FHIR R4 ImplementationGuide~~ | ~~G1~~ | **COMPLETE — NP-INT-FHIR-001 Rev 1 released 2026-06-03; IG package publication and CI integration pending** |
| ~~NP-LEGAL-BAA-001~~ | ~~Standard Business Associate Agreement Template~~ | ~~Month 3~~ | **COMPLETE (DRAFT) — NP-LEGAL-BAA-001 Rev 1 released 2026-06-03; legal counsel review required before first execution** |
| ~~NP-API-001~~ | ~~T2 Scripting API Specification~~ | ~~G1 (Month 6)~~ | **COMPLETE — NP-API-001 Rev 1 released 2026-06-07 (PR #121); independent security audit NP-SEC-PENTEST-002 required before any clinical API key is issued** |
| NP-REG-DPF-001 | EU-US Data Privacy Framework Self-Certification Record | Month 3 | Before any EU resident's data reaches US infrastructure; NP-PRIV-REM-001 STEP-12 |
| NP-REG-BIPA-001 | BIPA Compliance Record — legal opinion (scope/possession) + consent screen (shipped, all users) + website policy | Month 2 | Written release protection is universal/shipped; opinion confirms scope, does NOT gate activation; NP-PRIV-REM-001 STEP-34 |
| NP-REG-MHMD-001 | Washington MHMD Compliance Record — legal analysis (scope) + universal no-sale + standalone-authorization record | Month 2 | Protections universal/shipped; counsel analysis confirms scope, does NOT gate activation; NP-PRIV-REM-001 STEP-35 |
| ~~NP-DT-001~~ | ~~Design Input/Output Traceability Matrix~~ | ~~Month 6~~ | **COMPLETE — NP-DT-001 Rev 1 released 2026-06-07 (PR #122); design-inputs DHF coverage upgraded from Partial to Good** |
| ~~NP-HFE-001~~ | ~~Human Factors Engineering Plan~~ | ~~Month 9~~ | **COMPLETE — NP-HFE-001 Rev 1 released 2026-07-27; formative/summative testing execution remains open (NP-DT-001 VE-12)** |
| ~~NP-PRIV-AUDIT-001~~ | ~~App Privacy Audit~~ | ~~Month 9~~ | **COMPLETE — NP-PRIV-AUDIT-001 Rev 1 released 2026-06-03; 16 checklist items OPEN pending implementation** |
| NP-IRB-001 | IRB Protocol — research anonymisation and consent architecture | Month 9 | **Template drafted — NP-IRB-001 Rev 1 (DRAFT) released 2026-07-27; PI outreach and IRB/REB approval still required before first study descriptor deployment; NP-PRIV-REM-001 STEP-20 §4.4** |
| NP-ARCH-CLOUD-001 | T2 Clinical Cloud Architecture — EU data residency decision | Month 9 | When T2 cloud vendor selected; NP-PRIV-REM-001 STEP-19 |
| ~~NP-PMS-001~~ | ~~Post-Market Surveillance Plan~~ | ~~Month 12~~ | **COMPLETE — NP-PMS-001 Rev 1 released 2026-07-27; operational execution (Year 1–2) remains open per the plan's own open items** |
| ~~NP-FAI-SM-001~~ | ~~1064nm Smart Module FAI (hardware items)~~ | ~~Post-prototype~~ | **RETIRED 2026-09-14 — this serial never existed and is not needed.** FAI-SM-01…11 are specified in `NP-FW-PBM1064-001` Rev 4 §11 (BASELINED), which is the record of file. The four hardware-bench items (FAI-SM-04, -06, -07, -08) inspect artifact **A2** and belong to `NP-FAI-HEXFPC-001` when `OI-HEXTILE-02/-12/-05` close — not to a smart-module serial. `NP-FAI-001` §2.1, OI-FAI-05 |
| ~~NP-FAI-HD-001~~ | ~~sLORETA HD-tDCS Hardware FAI~~ | ~~Post-T2 prototype~~ | **RETIRED 2026-09-14 — this serial never existed and is not needed.** FAI-HD01…HD04 are specified in `NP-FW-HD-001` Rev 6 §12 (BASELINED), which is the record of file. The bench limbs (FAI-HD01 phantom, HD03, HD04) inspect the T2 clinical electrode cap, whose only specification is DRAFT and which has no `NP-ART-001` §2 register row. `NP-FAI-001` §2.1, OI-FAI-06; `OI-TCAP-06` |
| ~~NP-FAI-CV-001~~ | ~~Cervical VNS Hardware FAI~~ | ~~Post-T2 prototype~~ | **RETIRED 2026-09-14 — this serial never existed, and it was a second name for the same artifact as `NP-FAI-CVNS-001`.** FAI-CV01…CV03 are specified in `NP-FW-CVNS-001` Rev 4 §9 (BASELINED), which is the record of file. A14's artifact checklist keeps the register-derived name `NP-FAI-CVNS-001` and stays **unwritten and named** — no hardware specification exists to inspect against (GitHub #332). `NP-FAI-001` §2.1, OI-FAI-07 |
| NP-SEC-PENTEST-001 | POA Vault Penetration Test Report | G3 (Month 14) | After POA vault implementation; NP-PRIV-REM-001 STEP-25 |
| NP-SEC-PENTEST-002 | T2 Scripting API Security Audit Report | T2 pre-launch | Before any clinical API key issued; NP-PRIV-REM-001 STEP-26 |

---

## 9. Document History

The DHF's revision table (Rev 1 onward, with the 2026-08-12 duplicate-revision correction note and the Rev 71 re-sort note) is in `docs/reference/np-dhf-001-revision-history.md`, moved verbatim. **Add the row for a new DHF revision to that file, not here**: `scripts/check-dhf-index.ts` rule C reads it from there. It is history, not an index; nothing in §1–§8 depends on reading it.
