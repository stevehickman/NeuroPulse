# Design and Development Plan

**Project:** NeuroPulse
**Document:** NP-DP-001
**Revision:** A
**Date:** 2026-05-17
**Status:** ACTIVE
**Effective Date:** 2026-05-17
**Author:** Steve Hickman (CEO, interim Quality authority)
**Approved By:** Steve Hickman, CEO
**References:** —
**Related Issues:** —
**Gate:** —
**IEC 62304 Class:** —
**Applicable Standard:** 21 CFR §820.30(b), ISO 13485:2016 §7.3.1
**Next Review:** At each gate review (G1, G2, G3)
**Supersedes:** None (first issue). CLAUDE.md and NP-COORD-001 Rev A.8 served as interim design plan prior to this document.

---

## Revision History

| Rev | Date | Author | Description |
|-----|------|--------|-------------|
| A | 2026-05-17 | Steve Hickman | Initial release. Formalises design and development plan per 21 CFR §820.30(b) and ISO 13485:2016 §7.3.1. Incorporates all design decisions made during pre-formation concept phase (pre-2026-05-13). |

---

## 1. Purpose

This document defines the design and development plan for the NeuroPulse closed-loop multi-modal neuromodulation wearable platform. It establishes:

- The phases and stages of design and development activity
- Responsibilities and organisational interfaces for each phase
- The methods and criteria for design review, verification, validation, and design transfer
- Integration with risk management (NP-RM-001) and software development (NP-SW-001)
- Document control arrangements during the design phase

This plan is a living document. It shall be reviewed and updated at each gate review (G1, G2, G3) and whenever significant design changes occur, per Section 12 of this document.

---

## 2. Scope

This plan covers design and development of:

| Tier | Regulatory | Timeline from formation |
|------|-----------|------------------------|
| T1 — NeuroPulse Home | FDA general wellness (exempt) | Month 0 → Month 18 (target) |
| T2 — NeuroPulse Pro | FDA 510(k) | Month 12 → Month 36–54 (target) |

Both tiers share a single chassis, processor stack, and firmware platform. The T2 development phase (Phase 6) begins concurrently with T1 Phase 3 (Design Verification) to allow shared prototype infrastructure. The scope boundary between T1 and T2 is defined by the modality stack in CLAUDE.md §3 and §3b.

**Excluded from scope:** Manufacturing process design (covered under design transfer, Section 11); post-market surveillance activities (governed by NP-PMS-001, planned Month 12); clinical investigation activities beyond first-in-human tolerability (governed separately under IRB protocols).

---

## 3. References

| Document | Number | Notes |
|----------|--------|-------|
| Design controls procedure | NP-QMS-DC-001 | Defines design control process this plan implements |
| QMS manual | NP-QMS-001 | Overarching quality system |
| Engineering coordination checklist | NP-COORD-001 | Gate item checklists — G1 (15 items), G2 (14 items), G3 (6 items) |
| Design history file index | NP-DHF-001 | Master DHF record index |
| ISO 14971 risk management plan | NP-RM-001 | Risk management activities integrated with this plan |
| IEC 62304 software development plan | NP-SW-001 | Software development activities integrated with this plan |
| Design input/output traceability matrix | NP-DT-001 | Planned Month 6 — links all requirements to verification evidence |
| Human factors engineering plan | NP-HFE-001 | Planned Month 9 — IEC 62366-1 / FDA HFE Guidance 2016 |
| Post-market surveillance plan | NP-PMS-001 | Planned Month 12 |
| CAPA procedure | NP-QMS-CAPA-001 | Governs design problem resolution |
| Design controls | 21 CFR §820.30 | Regulatory requirement |
| Quality management systems | ISO 13485:2016 §7.3 | Regulatory requirement |
| Software lifecycle processes | IEC 62304:2006+AMD1:2015 | Software requirement |
| Risk management for medical devices | ISO 14971:2019 | Risk management requirement |
| Human factors engineering | IEC 62366-1:2015 | HFE requirement |
| CLAUDE.md | Rev 11 (2026-05-17) | Authoritative product design specification and locked decisions record |

---

## 4. Definitions

| Term | Definition |
|------|-----------|
| Design input | Physical and performance requirements of the device derived from intended use, user needs, and applicable regulations. Governed by NP-QMS-DC-001 §4. |
| Design output | Results of design and development activity — specifications, drawings, firmware, procedures — that define the device to be manufactured. Governed by NP-QMS-DC-001 §5. |
| Design review | Formal documented evaluation of design results at a defined stage, conducted by qualified personnel including at least one individual who does not have direct responsibility for the design stage under review. |
| Design verification | Confirmation by examination and objective evidence that design outputs meet design inputs. |
| Design validation | Confirmation by examination and objective evidence that the device conforms to defined user needs and intended use. |
| Design transfer | Process by which the design is transformed into production specifications sufficient for routine manufacture. |
| Gate review | A scheduled formal design review at which a defined checklist (NP-COORD-001) must be satisfied before the project proceeds to the next phase. |
| DHF | Design History File — compilation of records describing the design history of a finished device (21 CFR §820.30(j)). Master index: NP-DHF-001. |
| UHDR | User Health Data Record — user-owned biometric data; NeuroPulse never accesses. See CLAUDE.md §5. |
| SHDR | System Health Data Record — device-condition telemetry; NeuroPulse-owned, never user-linked. See CLAUDE.md §5. |
| T1 | NeuroPulse Home — 8-modality FDA-exempt wellness tier. |
| T2 | NeuroPulse Pro — 11-modality FDA 510(k) medical device tier. |
| Formation date | 2026-05-13 — date of QMS establishment and retroactive entry of all pre-formation design records under change control. Month 0 for all relative timelines in this document. |

---

## 5. Product Overview

NeuroPulse is a closed-loop multi-modal neuromodulation wearable platform consisting of a head-worn device and a control hub, sharing a single chassis, NXP i.MX RT1062 main processor, STM32G071 safety MCU, 8 GB eMMC storage, USB-C 3.2 Gen 1 primary connectivity, and BT 5.3 LE / Wi-Fi 6 wireless connectivity.

The platform is designed for autonomous closed-loop operation without a smartphone. All therapeutic session protocols are cryptographically signed. The Safety MCU (STM32G071) owns all stimulation enable GPIO lines and maintains an independent hardware watchdog; an application crash cannot cause unsafe stimulation.

Full product specification, locked design decisions, configurations, pricing, and modality stack are maintained in CLAUDE.md Rev 11 and referenced documents. This plan governs how those specifications are developed, verified, validated, and transferred to manufacturing.

### 5.1 Intended Use — T1

The NeuroPulse Home is a general wellness wearable device intended for use by healthy adults (18+) to support wellbeing goals including relaxation, focus, sleep, and cognitive performance, using photobiomodulation, non-invasive brain stimulation, EEG-guided neurofeedback, vagus nerve stimulation, and audio/visual entrainment. It is not intended to diagnose, cure, treat, mitigate, or prevent any disease or medical condition.

### 5.2 Intended Use — T2

The NeuroPulse Pro is intended for use by or under the supervision of licensed healthcare professionals as an adjunctive neuromodulation system for conditions including but not limited to depression, PTSD, migraine, cluster headache, and motor rehabilitation, incorporating transcranial magnetic stimulation, 21-channel qEEG, cervical transcutaneous vagus nerve stimulation, sLORETA-guided HD-tDCS, and deep photobiomodulation. Intended use will be formally defined per 21 CFR §814.20 at the Pre-Submission (Q-Sub) meeting planned Month 20.

### 5.3 Intended Users and Use Environments

| Tier | Primary user | Use environment |
|------|-------------|----------------|
| T1 | Healthy adult consumer, self-administered | Home, office, remote — uncontrolled environment |
| T2 | Licensed clinician (TMS/neuromodulation), or patient under clinical supervision | Clinical office, hospital, research institution |

---

## 6. Design and Development Phases

The NeuroPulse design and development programme is structured in six phases. Phases 1–5 address T1; Phase 6 addresses T2 additions. Phases overlap where indicated.

### 6.1 Phase summary

| Phase | Name | Period (from formation) | Gate | Key deliverables |
|-------|------|------------------------|------|-----------------|
| 0 | Concept and pre-formation | Pre-Month 0 (complete) | — | CLAUDE.md Rev 1–10; design brief R1–R5; risk register RISK-01..25; all pre-formation NP-* specs |
| 1 | System architecture | Month 0–6 | G1 | Tooling specs frozen; FPC layout frozen; safety architecture baselined; eMMC partition spec; all firmware module specs written |
| 2 | Detailed design | Month 6–10 | G2 | All firmware written and unit-tested; PCB layouts complete; shell/lens/hub CAD released to supplier; supplier qualifications complete |
| 3 | Design verification | Month 10–14 | G3 | All FAI bench tests complete; EMC pre-compliance; prototype system integration; software FAI complete |
| 4 | Design validation | Month 14–18 | — | Human factors summative (T1); T1 first-in-human tolerability; IEC 60601 safety testing at notified lab |
| 5 | Design transfer and T1 launch | Month 16–18 | — | Design transfer package; pilot production run; T1 commercial launch (target Month 18) |
| 6 | T2 development | Month 12–36+ | G3-T2 | T2 prototype; TMS/tcVNS FAI; clinical seeding studies; 510(k) submission (Month 36+) |

### 6.2 Phase 0 — Concept and Pre-Formation (Complete)

**Period:** Pre-2026-05-13 (all records retroactively entered under QMS change control at formation per NP-DHF-001).

**Status:** Complete. All Phase 0 outputs are recorded in the DHF under NP-DHF-001 and are listed in CLAUDE.md §13.5 (completed and locked decisions).

**Key outputs produced in Phase 0:**
- Complete product specification (CLAUDE.md Rev 1–10, Design Brief R1–R5)
- Two-tier platform strategy with shared chassis
- Full modality stack (T1: 8 modalities, T2: +3 modalities + accessories)
- Configuration and pricing structure (§2 CLAUDE.md)
- Risk register RISK-01..25 (25 risks; 23 mitigated)
- Safety architecture (Safety MCU ownership of stimulation GPIO)
- UHDR/SHDR data separation framework
- All tooling specifications (NP-TOOL-ZM-001, NP-TOOL-ZM-SM-001, NP-TOOL-SHELL-001, NP-TOOL-LENS-001)
- FPC zone module specification (NP-HW-FPC-001 Rev E)
- Hub PCB Rev B specification (NP-HW-HUB-001 Rev B)
- All firmware module specifications (NP-FW-EMMC-001, NP-FW-PBM1064-001, NP-FW-HD-001, NP-FW-CVNS-001, NP-FW-HRV-001, NP-FW-HUB-001, NP-FW-ZA-001)
- Firmware source code: bootloader, zone_announce, hrv_biofeedback, sloreta_hdtdcs, cervical_vns, pbm_1064nm, hub_control
- QMS establishment (NP-QMS-001, NP-DHF-001, NP-QMS-DC-001, NP-RM-001, NP-SW-001, NP-QMS-CAPA-001)
- Cervical VNS 510(k) pre-submission argument (NP-REG-CVNS-001)
- eMMC partition architecture and dual-bank OTA bootloader firmware
- iOS app development roadmap (NP-APP-ROADMAP-001)
- Helmet simulator (NP-SIM-001 v0.1.0)

### 6.3 Phase 1 — System Architecture (Month 0–6, Gate G1)

**Objective:** Freeze all system-level architecture decisions that affect tooling or PCB layout. No tooling steel may be cut until G1 is closed.

**Entry criteria:** QMS established (complete at Month 0).

**Key activities:**

| Activity | Owner | Target | Reference |
|----------|-------|--------|-----------|
| Trademark clearance (US, EU, CA, AU) | CEO/Legal | Month 2 | CLAUDE.md §13.1 BLOCKING |
| 400 mW/cm² regulatory opinion letter (RISK-03) | External counsel | Month 2–3 | CLAUDE.md §13.1 BLOCKING |
| CFRP shell slot rim Ra ≤ 1.6 µm supplier confirmation (RISK-20) | Engineering Lead | Month 3 | CLAUDE.md §13.1 BLOCKING |
| PDMS CAT-C supplier selection + thermal cycling qualification start (RISK-04) | Engineering Lead | Month 3 | CLAUDE.md §13.1 BLOCKING |
| Zone module mould design review (NP-TOOL-ZM-001 §5 checklist, all F-01–F-08) | ME Lead + supplier | Month 4 | NP-COORD-001 G1-05 |
| LED emitter pulse current verification (660nm + 808nm at 120–180 mA) | EE Lead | Month 3 | CLAUDE.md §13.4 |
| ZONE_ID firmware debounce spec in firmware requirements | FW Lead | Month 2 | NP-COORD-001 |
| Hub tooling design review (probe dock, anchor posts, Boa channel, fan) | ME Lead + supplier | Month 4 | CLAUDE.md §13.4 |
| Factory calibration procedure for InGaAs PD coefficients (OI-PBM-04) | FW Lead | Month 5 | NP-FW-PBM1064-001 |
| Safety MCU unit-level FMEA for SW01-M01..M08 (IEC 62304 §7.1, Class C) | FW Lead | Month 5 | NP-SW-001 §11 |
| G1 gate review | CEO + independent reviewer | Month 6 | NP-COORD-001 G1 |

**Gate G1 exit criteria:** All 15 items in NP-COORD-001 G1 checklist verified and signed. All BLOCKING items above resolved. Gate review meeting minutes in DHF.

### 6.4 Phase 2 — Detailed Design (Month 6–10, Gate G2)

**Objective:** Complete all design outputs to a level sufficient for prototype fabrication. All firmware written, unit-tested, and code-reviewed. All PCB layouts complete. All CAD models supplier-released.

**Key activities:**

| Activity | Owner | Target | Reference |
|----------|-------|--------|-----------|
| Hub PCB Rev B Gerber build (OI-HUB-03) | EE Lead | Month 7 | NP-HW-HUB-001 Rev B |
| InGaAs PD calibration bench setup (OI-PBM-04) | EE + FW Lead | Month 7 | NP-FW-PBM1064-001 |
| Shell CAD release to supplier (EEG channel OI-09, anchor posts) | ME Lead | Month 7 | NP-TOOL-SHELL-001 |
| Lens/goggle arm CAD sign-off (OI-10: rim guard tether clearance) | ME Lead | Month 7 | NP-TOOL-LENS-001 Rev B |
| IEC 60068-2-14 200-cycle PDMS thermal cycling qualification (FAI-TC02) | Supplier + QE | Month 8 | NP-FAI-ZM-001 BLOCKING |
| IPX4 qualification after 10 field swap cycles (FAI-IPX-02) | QE | Month 8 | NP-FAI-ZM-001 BLOCKING |
| N42 magnet pocket ≥1 mm polymer wall verification (MR in NP-TOOL-LENS-001) | ME Lead | Month 7 | NP-TOOL-LENS-001 |
| AgNW outer coating supplier qualification | ME + Procurement | Month 7 | NP-TOOL-LENS-001 P-01 |
| All remaining firmware HAL stubs implemented (OI-ZA-01..04, OI-HRV-01..05, OI-PBM-01..08) | FW Lead | Month 8 | All FW specs |
| eQMS platform selection and deployment | Quality Lead | Month 9 | CLAUDE.md §13.4 |
| Human factors engineering plan (NP-HFE-001) authoring | HFE Lead | Month 9 | IEC 62366-1 |
| Design input/output traceability matrix (NP-DT-001) authoring | Quality Lead | Month 9 | 21 CFR §820.30 |
| Formative HFE study — sliding eject lever with Parkinson's/post-stroke (n=5) | HFE Lead | Month 9 | NP-FAI-ZM-001 FAI-A15 |
| Partner optician network contract (S3 Rx programme) | CEO/Commercial | Month 10 | CLAUDE.md §13.4 |
| G2 gate review | CEO + Engineering Lead + independent reviewer | Month 10 | NP-COORD-001 G2 |

**Gate G2 exit criteria:** All 14 items in NP-COORD-001 G2 checklist verified and signed. Prototype build authorised. Gate review meeting minutes in DHF.

### 6.5 Phase 3 — Design Verification (Month 10–14, Gate G3)

**Objective:** Demonstrate by objective evidence that all design outputs meet design inputs. All FAI bench tests complete. EMC pre-compliance complete.

**Key activities:**

| Activity | Owner | Target | Reference |
|----------|-------|--------|-----------|
| 1064nm optical bench tests (FAI-SM-04, FAI-SM-06, FAI-SM-07, FAI-SM-08) | EE Lead + QE | Month 11 | NP-FW-PBM1064-001 |
| sLORETA HD-tDCS phantom bench (FAI-HD01, FAI-HD03, FAI-HD04) | FW Lead + QE | Month 12 | NP-FW-HD-001 |
| Cervical VNS cardiac interlock timing bench (FAI-CV02) | FW Lead + QE | Month 12 | NP-FW-CVNS-001 |
| Cervical VNS electrode placement phantom (FAI-CV01) | ME + QE | Month 12 | NP-FW-CVNS-001 |
| Hub control system integration test (FAI-HUB-01..03) | FW Lead + QE | Month 12 | NP-FW-HUB-001 |
| EEG signal quality verification (ADS1299 noise floor, calibration) | EE + QE | Month 12 | CLAUDE.md §3 |
| Safety interlock full-stack test (all modality interlocks in §4.2) | FW Lead + Safety MCU | Month 12 | CLAUDE.md §4.2 |
| EMC pre-compliance (FCC Part 15, IEC 61000 radiated/conducted) | EE Lead + test house | Month 13 | CLAUDE.md §10 |
| IEC 62471 photobiological hazard assessment (PBM + visual) | External lab | Month 13 | CLAUDE.md §3 |
| IEC 60601-1 safety (electrical safety, thermal, mechanical) | External lab | Month 13 | CLAUDE.md §10 |
| IEC 62133 battery/cell safety | External lab | Month 13 | CLAUDE.md §10 |
| SBOM (NP-SBOM-001) authoring | FW Lead | Month 13 | CLAUDE.md §13.4 |
| G3 gate review | CEO + Engineering + Quality Lead + Regulatory Lead | Month 14 | NP-COORD-001 G3 |

**Gate G3 exit criteria:** All 6 items in NP-COORD-001 G3 checklist verified and signed. All FAI items for G3 modalities complete. Safety testing at accredited lab complete or formally scheduled. Gate review minutes in DHF. Design validation plan approved (NP-HFE-001 summative section).

### 6.6 Phase 4 — Design Validation (Month 14–18)

**Objective:** Confirm by examination and objective evidence that the T1 device conforms to user needs and intended use under actual or simulated conditions of use. Distinct from verification — validation addresses whether the right device was built.

**Key activities:**

| Activity | Owner | Target | Reference |
|----------|-------|--------|-----------|
| Summative human factors study (T1) — per NP-HFE-001 | HFE Lead | Month 15–16 | IEC 62366-1; FDA HFE 2016 |
| T1 first-in-human tolerability — all T1 modalities (n=10 minimum) | Clinical Lead + IRB | Month 14–16 | NP-COORD-001 G3-05 |
| Software validation (iOS/Android app, Class B) | App Lead + QA | Month 15–16 | NP-SW-001 |
| Biocompatibility assessment (ISO 10993) — skin-contact materials | External lab | Month 14–15 | 21 CFR §820.30(g) |
| Performance qualification — full T1 configuration at operating extremes | QE | Month 15–16 | All modality specs |
| Residual risk evaluation (pre-launch) per ISO 14971 §8 | Quality Lead | Month 16 | NP-RM-001 |
| Risk Management Report — T1 | Quality Lead | Month 16–17 | NP-RM-001 |
| 21 CFR Part 11 / cybersecurity review | FW Lead + external | Month 16 | CLAUDE.md §10 |

**Phase 4 exit criteria:** All validation activities complete with objective evidence in DHF. Overall residual risk formally accepted. Risk Management Report signed. Summative HFE study report complete.

### 6.7 Phase 5 — Design Transfer and T1 Launch (Month 16–18)

**Objective:** Transform design outputs into manufacturing specifications sufficient for routine reproducible production.

**Key activities:**

| Activity | Owner | Target | Reference |
|----------|-------|--------|-----------|
| Design transfer package (device master record — DMR) | Engineering + Quality | Month 16–17 | 21 CFR §820.30(h) |
| Manufacturing process qualification (IQ/OQ/PQ) — first production line | Manufacturing + QE | Month 17 | 21 CFR §820.75 |
| Pilot production run (n=25–50 units) — process capability confirmation | Manufacturing + QE | Month 17 | — |
| Factory acceptance test (FAT) procedure sign-off | Quality Lead | Month 17 | — |
| Post-market surveillance plan finalisation (NP-PMS-001) | Quality Lead | Month 17 | CLAUDE.md §13.4 |
| FTC claim substantiation review (all T1 marketing claims) | Regulatory + Legal | Month 17 | CLAUDE.md §10 |
| Product registration / UDI assignment | Regulatory Lead | Month 17–18 | 21 CFR §830 |
| T1 commercial launch | CEO | Month 18 | — |

**Phase 5 exit criteria:** Design transfer complete and documented per 21 CFR §820.30(h). Pilot production units pass FAT. PMS plan approved. UDI assigned.

### 6.8 Phase 6 — T2 Development (Month 12–36+)

**Objective:** Develop and validate T2-specific additions (TMS, 21-ch qEEG wet gel, 1170nm deep PBM, clinical tACS, sLORETA-guided HD-tDCS, cervical VNS accessory) on the shared T1 platform. Submit 510(k).

**Phase 6 runs concurrently with Phase 3 and beyond.** T2-specific design inputs, verification, and validation activities are governed by a separate T2 design and development plan addendum (NP-DP-001 Addendum T2, planned Month 12). The T2 programme is subject to the same gate structure as T1 but with an independent G3-T2 gate.

**Key Phase 6 milestones:**

| Milestone | Target |
|-----------|--------|
| T2 prototype fabrication | Month 15 |
| TMS coil integration and safety testing | Month 18–20 |
| 1170nm deep PBM bench qualification | Month 18 |
| Cervical VNS first-in-human tolerability (FAI-CV03, IRB required) | Month 18–22 |
| Pre-Submission (Q-Sub) meeting with FDA | Month 20 |
| sLORETA HD-tDCS phantom bench and first clinical use | Month 20–24 |
| T2 clinical seeding studies (research institutions) | Month 24–36 |
| 510(k) submission | Month 36–42 |
| T2 commercial launch | Month 42–54 |

---

## 7. Responsibilities and Organisational Interfaces

### 7.1 Key roles

| Role | Person | Authority | Phase involvement |
|------|--------|-----------|-------------------|
| CEO / Design Authority | Steve Hickman | Final design decision authority; interim Quality authority until Quality Lead hired | All phases |
| Quality Lead | TBD (target hire Month 6) | QMS compliance; design review independence; DHF completeness | Phase 1 onward |
| Engineering Lead (ME/EE) | TBD (target hire Month 3) | Mechanical and electronic design outputs; supplier management; tooling release | Phase 1 onward |
| Firmware Lead | TBD (target hire Month 3) | SW-01 (Safety MCU) and SW-02 (main processor) firmware; firmware safety architecture; IEC 62304 Class C activities | Phase 1 onward |
| Regulatory Lead | TBD (target hire Month 6); external counsel engaged for RISK-03 and NP-REG-CVNS-001 | Regulatory strategy; 510(k) preparation; standards compliance | Phase 2 onward |
| HFE Lead | TBD (target hire Month 6) | NP-HFE-001; formative and summative studies; use error risk assessment | Phase 2 onward |
| App Lead | TBD | SW-03 (iOS/Android app); NP-APP-ROADMAP-001 | Phase 2 onward |
| Clinical Lead | TBD | IRB protocols; first-in-human studies; researcher relationships | Phase 3 onward |

**Note:** Until named roles are filled, the CEO holds interim authority. All gate reviews must include at least one person not directly responsible for the design stage under review (21 CFR §820.30(e) independence requirement). Until internal hires are in place, an external consultant shall serve as the independent reviewer for gate reviews.

### 7.2 Organisational interfaces

| Interface | Method | Frequency |
|-----------|--------|-----------|
| Engineering ↔ Quality | Design review meetings; DHF record submission protocol | Each gate + on design change |
| Engineering ↔ Regulatory | Regulatory input review of design outputs; regulatory opinion loop-back (RISK-03) | Monthly from Phase 1 |
| Engineering ↔ Firmware | Shared FPC pinout, HAL interface specs; firmware requirements traceability (NP-DT-001) | Weekly from Phase 1 |
| Engineering ↔ Suppliers | SUP qualification checklist (NP-PROC-SUP-001); supplier design reviews; FAI records | Per NP-PROC-SUP-001 |
| Firmware ↔ Safety MCU | SPI heartbeat protocol; GPIO ownership map; IEC 62304 Class C/B interface spec | Defined in NP-SW-001 |
| Clinical ↔ Engineering | User needs capture; first-in-human feedback to design inputs; HFE studies | Phase 3 onward |
| NeuroPulse ↔ External counsel | Regulatory opinion letters; trademark clearance; FTC substantiation review | As needed; RISK-03 active |

### 7.3 Entity status note

NeuroPulse is not yet incorporated as a legal entity as of this document's date (2026-05-17). Company formation is in progress. This document is authored under the interim operating name "NeuroPulse." All QMS documents, DHF records, and agreements executed prior to formal incorporation shall be ratified under the incorporated entity at formation. Until formation, the CEO is the sole accountable individual for all design authority decisions.

---

## 8. Design Inputs

Design inputs define the physical and performance requirements of the device. The primary design input record is CLAUDE.md Rev 11, which incorporates user needs, regulatory requirements, standards requirements, and design constraints. Design inputs are formalised and traced in NP-DT-001 (Design Input/Output Traceability Matrix), planned Month 6.

### 8.1 Input categories

| Category | Sources | Primary document |
|----------|---------|-----------------|
| User needs — T1 wellness | Consumer user research; intended use definition §5.1 | CLAUDE.md §1, §2 |
| User needs — T2 clinical | Clinician advisory input; researcher candidates (NP-CLIN-001) | CLAUDE.md §11, NP-CLIN-001 |
| Regulatory requirements — T1 | 21 CFR general wellness guidance; IEC 60601-1; IEC 62471; FCC Part 15 | CLAUDE.md §10 |
| Regulatory requirements — T2 | 21 CFR §820, 510(k) predicate devices (K083538, K122288, K142485, K163334, K173323) | CLAUDE.md §10, NP-REG-CVNS-001 |
| Safety requirements | ISO 14971 risk register (RISK-01..25); modality-specific interlocks | NP-RM-001, CLAUDE.md §4.2 |
| Software requirements | IEC 62304 classification; safety response time requirements | NP-SW-001, CLAUDE.md §4.2 |
| Human factors requirements | Intended users; use environments; accessibility (Parkinson's/post-stroke — RISK-22) | CLAUDE.md §4.4, NP-HFE-001 |
| Standards | IEC 60601-1, -2-10; IEC 62471; IEC 62133; IEC 62304; IEC 62366-1; ISO 14971; ISO 10993 | CLAUDE.md §10 |
| Competitive and commercial | Competitive position; pricing constraints; configuration structure | CLAUDE.md §9 |

### 8.2 Design input changes

Any change to a design input after G1 gate closure is a significant change and must follow the design change process defined in Section 12 and NP-QMS-DC-001 §8. Changes to design inputs that affect safety or intended use require Quality Lead sign-off and risk register update.

---

## 9. Design Outputs

Design outputs are the translated result of design inputs — specifications, drawings, firmware, and procedures that define the device to be manufactured. All design outputs are recorded in the DHF under NP-DHF-001.

### 9.1 Output categories

| Category | Current primary records |
|----------|------------------------|
| Product specification | CLAUDE.md Rev 11; Design Brief R5 (NP-DB-005) |
| Mechanical design | NP-TOOL-ZM-001, NP-TOOL-ZM-SM-001, NP-TOOL-SHELL-001, NP-TOOL-LENS-001; supplier CAD (pending) |
| Electronic design | NP-HW-FPC-001 Rev E; NP-HW-HUB-001 Rev B; PCB layouts (pending) |
| Firmware | firmware/bootloader/, firmware/zone_announce/, firmware/hrv_biofeedback/, firmware/sloreta_hdtdcs/, firmware/cervical_vns/, firmware/pbm_1064nm/, firmware/hub_control/ |
| Software specifications | NP-FW-EMMC-001, NP-FW-PBM1064-001, NP-FW-HD-001, NP-FW-CVNS-001, NP-FW-HRV-001, NP-FW-HUB-001, NP-FW-ZA-001 |
| iOS/Android app | NP-APP-ROADMAP-001; app source (pending) |
| Procurement specifications | NP-PROC-FPC-001, NP-PROC-FPC-1064-001, NP-PROC-SUP-001 |
| Safety and risk documentation | NP-RM-001, RISK-01..25, NP-FAI-ZM-001 |
| Session protocol language | NPPS Language Reference (docs/npps-reference.md) |
| Simulator | NP-SIM-001 v0.1.0 (simulator/) |
| Regulatory submissions | NP-REG-CVNS-001; NP-REG-PBM1064-001 |

### 9.2 Essential output requirements

The following outputs are subject to formal approval before release to manufacturing, per 21 CFR §820.30(d):

- Device master record (DMR) — design transfer deliverable, Phase 5
- Acceptance criteria for each production inspection step
- Labelling (including UDI, IFU, packaging)
- Installation and service procedures

---

## 10. Design Reviews

### 10.1 Scheduled gate reviews

Three formal gate reviews are scheduled. Each uses the NP-COORD-001 checklist for the relevant gate.

| Gate | Phase exit | Checklist | Minimum attendees |
|------|-----------|-----------|------------------|
| G1 | Phase 1 / Month 6 | NP-COORD-001 G1 (15 items) | CEO; Engineering Lead; independent reviewer (external consultant until Quality Lead hired) |
| G2 | Phase 2 / Month 10 | NP-COORD-001 G2 (14 items) | CEO; Engineering Lead; Firmware Lead; Quality Lead; Regulatory Lead; independent reviewer |
| G3 | Phase 3 / Month 14 | NP-COORD-001 G3 (6 items) | CEO; all functional leads; independent reviewer; external regulatory counsel |

### 10.2 Review requirements

- Each gate review shall be documented with a meeting record in the DHF: date, attendees, checklist disposition for each item (pass/fail/conditional), action items with owners and due dates, and formal gate pass/fail decision.
- No gate may be passed with open BLOCKING items.
- Items conditionally passed shall have written closure criteria and a re-verification date.
- Gate G1 shall not be passed while RISK-03 (regulatory opinion) or RISK-20 (CFRP Ra confirmation) remain open.
- Design reviews may be called outside the gate schedule when a significant design change is proposed. Ad-hoc review requirements are defined in NP-QMS-DC-001 §5.

### 10.3 Independence requirement

Per 21 CFR §820.30(e), at least one attendee at each gate review must not have direct responsibility for the design stage under review. Until the Quality Lead is hired (target Month 6), this shall be an external engineering or quality consultant retained specifically for this purpose.

---

## 11. Design Verification

Design verification confirms that design outputs meet design inputs. Verification is distinct from validation — it answers "did we build it right?" rather than "did we build the right thing?"

### 11.1 Verification methods

| Method | Applicability |
|--------|--------------|
| Analysis | Calculation-based verification (e.g. charge density, dose metering, power budgets) |
| Inspection | Physical measurement against specification (e.g. Ra surface finish, LED pitch, wall thickness) |
| Test | Bench or laboratory measurement (FAI items per NP-FAI-ZM-001, per-firmware spec FAI checklists) |
| Similarity | Reference to verified predecessor design (not applicable for first-generation NeuroPulse) |

### 11.2 Verification planning

Verification protocols are embedded within each design output specification (FAI checklists). The master verification tracking table is maintained in NP-COORD-001 and NP-DHF-001. Specific blocking verification items are:

| Item | Specification | Status |
|------|--------------|--------|
| PDMS thermal cycling 200 cycles (FAI-TC02) | NP-FAI-ZM-001 | BLOCKING — open |
| IPX4 after 10 swap cycles (FAI-IPX-02) | NP-FAI-ZM-001 | BLOCKING — open |
| CFRP Ra ≤ 1.6 µm (RISK-20) | NP-PROC-SUP-001 SUP-B-01 | BLOCKING — open |
| 1064nm optical bench (FAI-SM-04, -06, -07, -08) | NP-FW-PBM1064-001 | Pending Hub PCB Rev B Gerber |
| sLORETA phantom bench (FAI-HD01, -HD03, -HD04) | NP-FW-HD-001 | Pending T2 prototype |
| Cervical VNS cardiac interlock bench (FAI-CV02) | NP-FW-CVNS-001 | Pending T2 prototype |
| Hub control system integration (FAI-HUB-01..03) | NP-FW-HUB-001 | Pending first prototype |

Software verification (unit tests, integration tests, FAI software items) is governed by NP-SW-001. Software FAI items marked "Software PASS" in the firmware specification records are considered verified against software requirements but do not substitute for hardware bench tests.

### 11.3 Verification records

All verification records are DHF design outputs per 21 CFR §820.30(f). Records shall include: protocol reference, test date, equipment identification (calibrated instruments only), raw data, pass/fail disposition, and signature of responsible engineer.

---

## 12. Design Validation

Design validation confirms the finished device conforms to user needs and intended use. Validation uses initial production units or equivalent, in actual or simulated use conditions.

### 12.1 Validation activities — T1

| Activity | Method | Responsible | Target |
|----------|--------|-------------|--------|
| Summative HFE study | Representative users (T1 intended population, n=15 minimum); all critical and essential tasks per NP-HFE-001 | HFE Lead | Month 15–16 |
| First-in-human tolerability | 10 healthy adults; all T1 modalities at intended parameters; safety monitoring; reported AEs | Clinical Lead + IRB | Month 14–16 |
| Software validation — app | Clinical workflow scenarios; use error scenarios; per IEC 62304 Class B requirements | App Lead + QA | Month 15–16 |
| Biocompatibility | ISO 10993-1 risk-based assessment; skin sensitisation and cytotoxicity for contact materials | External lab | Month 14–15 |
| Simulated-use performance | Full T1 session at operating extremes (temperature, humidity, power bank, head size range) | QE | Month 15–16 |
| Sterilisation/reprocessing validation | Cleaning protocol validation; PDMS and electrode material compatibility with cleaning agents | QE | Month 15 |

### 12.2 Validation records

Validation records shall be held in the DHF per 21 CFR §820.30(g). Each validation activity shall produce a protocol (approved before execution) and a report (completed after execution). Deviations and adverse findings shall trigger a CAPA per NP-QMS-CAPA-001.

### 12.3 T2 validation

T2 validation activities (TMS, cervical VNS clinical performance, sLORETA HD-tDCS efficacy) are addressed in the T2 programme plan addendum (NP-DP-001 Addendum T2). FAI-CV03 (3 healthy adults, cervical VNS tolerability) requires IRB approval and is a gate item for G3-T2.

---

## 13. Design Transfer

### 13.1 Transfer requirements

Design transfer ensures design outputs are translated into production specifications that enable routine and reproducible manufacture, per 21 CFR §820.30(h).

Design transfer is complete when:
1. The device master record (DMR) is approved and controlled under QMS change control
2. Manufacturing processes are validated (IQ/OQ/PQ complete)
3. A pilot production run (n=25–50) has been completed and evaluated against acceptance criteria
4. All production test procedures and acceptance criteria are approved
5. Quality Lead has signed off DMR completeness

### 13.2 Transfer deliverables

| Deliverable | Owner |
|------------|-------|
| Device master record (DMR) — specifications, drawings, labelling, procedures | Engineering + Quality |
| Bill of materials (final, with approved supplier list) | Engineering + Procurement |
| Manufacturing process specifications | Manufacturing |
| Production test procedures and acceptance criteria | Quality |
| Labelling — device, packaging, IFU | Regulatory + Engineering |
| Production training records | Manufacturing |
| Design transfer report | Quality Lead |

### 13.3 Production ramp

The production line is shared across T1 configurations (Core, Home Lite, Home Standard, Home Premium). Configuration differentiation is achieved through firmware SKU flags, charger selection, and module inclusion — not separate production lines. The DMR shall address all T1 configurations.

---

## 14. Design Changes

All design changes after G1 gate closure are subject to the change control process defined in NP-QMS-DC-001 §8.

### 14.1 Significant vs. minor changes

| Change type | Trigger | Process |
|------------|---------|---------|
| Significant | Affects safety, intended use, design inputs, locked decisions in CLAUDE.md, or any BLOCKING item | Formal design change order (DCO); mandatory design review; risk register update; CLAUDE.md revision |
| Minor | Typo corrections, clarifications, non-safety editorial changes to specifications | Minor change notification (MCN); Quality Lead sign-off; DHF record update |

### 14.2 CLAUDE.md as the locked decisions record

CLAUDE.md §13.5 (Completed and Locked Decisions) serves as the authoritative record of all design decisions that have been through full design review. Any change to a locked decision requires a Significant design change order with CEO approval. CLAUDE.md revision history is maintained in the GitHub repository.

### 14.3 Changes during verification and validation

Changes identified during verification or validation testing (FAI failures, HFE use errors, adverse events) shall be processed via CAPA (NP-QMS-CAPA-001) in addition to design change control. The CAPA shall assess whether the change requires restarting any verification or validation activities.

---

## 15. Risk Management Integration

Risk management activities are integrated throughout all design phases per NP-RM-001 and ISO 14971:2019.

### 15.1 Risk management touchpoints by phase

| Phase | Risk management activity |
|-------|------------------------|
| 0 (complete) | Initial hazard identification; risk register RISK-01..25 established; 23 risks mitigated |
| 1 | RISK-03 and RISK-20 closure; Safety MCU FMEA; new hazards from detailed design |
| 2 | Risk control verification (design outputs confirmed to implement risk controls) |
| 3 | Residual risk evaluation from verification test results |
| 4 | Overall residual risk acceptability evaluation (pre-launch); Risk Management Report |
| 5 | Design transfer risk assessment (manufacturing risks) |
| Post-launch | SHDR fleet telemetry as post-market risk signal; PMS plan (NP-PMS-001) |

### 15.2 Open risks

At the date of this document, two risks remain open:
- **RISK-03:** 400 mW/cm² irradiance regulatory opinion — BLOCKING for all public material. Scope extended to include 1064nm, aggregate irradiance, and T2 combined sessions (NP-REG-PBM1064-001 Rev A). External counsel engaged. Target resolution: Month 2–3.
- **RISK-20:** CFRP shell slot rim Ra ≤ 1.6 µm — written supplier confirmation required. BLOCKING for tooling release. Target resolution: Month 3.

RISK-25 (cervical VNS cardiac reflex) is mitigated at the software level (NP-FW-CVNS-001 Rev A) but remains HARDWARE BENCH PENDING pending FAI-CV02 and FAI-CV03. Full RISK-25 closure is a G3-T2 gate requirement.

---

## 16. Software Development Integration

Software development is governed by NP-SW-001 (IEC 62304 Software Development Plan). Software activities are integrated with the design phases as follows:

| Software item | Classification | Primary design phase | Gate |
|--------------|---------------|---------------------|------|
| SW-01: Safety MCU (STM32G071, bare-metal) | **Class C** | Phase 1–2 (firmware written Phase 0); FMEA Phase 1 | G3 |
| SW-02: Main processor (i.MX RT1062, FreeRTOS) | **Class B** | Phase 1–2 (firmware written Phase 0); HAL stubs Phase 2 | G2 |
| SW-03: iOS/Android app | **Class B** | Phase 2–4; Watch sync app post-launch | G3/validation |

All firmware source code is in the NeuroPulse GitHub repository and constitutes DHF design output records per NP-DHF-001 §5.

Safety-critical software response time requirements (from NP-SW-001):
- SPI heartbeat → stimulation cutoff: ≤50 ms
- Cardiac interlock (cervical VNS): ≤100 ms
- Photoparoxysmal detection → goggle halt: ≤200 ms

These requirements must be verified by FAI bench test, not software analysis alone.

---

## 17. Supplier and Outsourced Design Activities

Selected design activities are outsourced to suppliers. All suppliers performing design or manufacturing activities are managed per NP-PROC-SUP-001.

| Activity | Supplier category | Qualification requirement |
|----------|-----------------|--------------------------|
| CFRP shell tooling and moulding | CAT-B (CFRP shell) | SUP-B-01: written Ra ≤ 1.6 µm confirmation (BLOCKING); RISK-20 |
| PDMS optical window bonding | CAT-C (PDMS bonding) | SUP-C-08: IEC 60068-2-14 200-cycle qualification capability (BLOCKING); RISK-04 |
| Zone module injection moulding | CAT-A (moulding) | SUP-M-07 qualification items; NP-TOOL-ZM-001 mould design review |
| Lens tooling (AgNW, hard coat, PDMS diffuser) | CAT-A / specialist coating | AgNW supplier qualification (BLOCKING, NP-TOOL-LENS-001 P-01) |
| IEC 60601-1 / IEC 62471 / EMC testing | Accredited test laboratory | ISO 17025 accreditation; NeuroPulse test plan approval |
| Regulatory counsel (RISK-03, NP-REG-CVNS-001) | External legal/regulatory | Engaged; scope per NP-REG-PBM1064-001 Rev A |

Outsourced design activities do not reduce NeuroPulse's responsibility for device safety and compliance. All supplier design outputs are reviewed and approved by NeuroPulse before acceptance into the DHF.

---

## 18. Document Control During Design Phase

### 18.1 Interim system (Month 0 to Month 9)

Until the eQMS platform is deployed (target Month 9), document control is maintained via:

- **GitHub repository** (`stevehickman/neuropulse`) — version-controlled storage for all `.md` and source code design outputs
- **CLAUDE.md** — authoritative locked decisions record; revision tracked in git history
- **NP-DHF-001** — master DHF index listing all documents with status, location, and revision
- **Document numbering:** NP-[SYSTEM]-[SEQ]-[REV] scheme per NP-QMS-001. Revisions are sequential (Rev A, Rev B, ...). Each revision requires a documented reason for change.
- **Approval:** Until Quality Lead is hired, the CEO approves all design output documents. From Month 6, Quality Lead co-approves all documents with safety or regulatory impact.

### 18.2 eQMS transition (Month 9)

On eQMS deployment, all existing controlled documents shall be migrated per a migration plan to be authored by the Quality Lead. The eQMS shall be validated for 21 CFR Part 11 compliance before formal reliance. Document control requirements in 21 CFR §820.40 shall be fully satisfied in the eQMS.

### 18.3 Document numbering for this plan

This document is NP-DP-001 Rev A. Subsequent revisions shall be lettered sequentially. This plan shall be reviewed and a new revision issued at each gate review or whenever design phase timelines change by more than four weeks.

---

## 19. Open Items and Pending Decisions

The following items from CLAUDE.md §13.4 are unresolved at the date of this document and must be resolved within the Phase timelines noted.

| Item | Blocking for | Phase |
|------|-------------|-------|
| Product name trademark clearance | Any external conversation; all public material | Phase 1 (Month 2) |
| RISK-03: 400 mW/cm² regulatory opinion (includes 1064nm, aggregate, T2 combined) | All public marketing claims; gate G1 | Phase 1 (Month 2–3) |
| RISK-20: CFRP Ra ≤ 1.6 µm supplier written confirmation | Tooling release; gate G1 | Phase 1 (Month 3) |
| RISK-04: PDMS CAT-C supplier selection + qualification start | Production FPC build; gate G1 | Phase 1 (Month 3) |
| LED emitter pulse current verification (660nm + 808nm at 120–180 mA) | FPC BOM lock | Phase 1 (Month 3) |
| Zone module mould design review (NP-TOOL-ZM-001 §5 checklist) | Steel cut; gate G1 | Phase 1 (Month 4) |
| Hub tooling review (probe dock, anchor posts, Boa channel, fan) | Hub tooling release | Phase 1 (Month 4) |
| Factory calibration procedure for InGaAs PD coefficients (OI-PBM-04) | Production start | Phase 1 (Month 5) |
| Safety MCU unit-level FMEA (SW01-M01..M08) | IEC 62304 Class C; gate G3 | Phase 1 (Month 5) |
| AgNW coating supplier qualification | Lens tooling release | Phase 2 (Month 7) |
| Hub PCB Rev B Gerber build (OI-HUB-03) | FAI-SM-04, FAI-SM-06 | Phase 2 (Month 7) |
| Quality Lead hire | G2 gate independence | Phase 1 (Month 6) |
| NP-DT-001 (Design Input/Output Traceability Matrix) authoring | Gate G2; 510(k) completeness | Phase 2 (Month 6–9) |
| NP-HFE-001 (Human Factors Engineering Plan) authoring | Formative studies; gate G2 | Phase 2 (Month 9) |
| eQMS platform selection and deployment | Full 21 CFR Part 11 doc control | Phase 2 (Month 9) |
| NP-PMS-001 (Post-Market Surveillance Plan) | T1 launch | Phase 5 (Month 17) |
| NP-SBOM-001 (Software Bill of Materials) | Cybersecurity; 510(k) | Phase 3 (Month 13) |
| VP Quality hire | QMS maturity | Phase 1 (Month 6) |
| Partner optician network contract (S3 Rx programme) | T1 launch | Phase 2–5 |
| SBIR Phase I application | Clinical evidence timeline | Month 3 |
| SAB formation (Tsai first outreach) | Scientific credentialing | Month 3–6 |
| First researcher contacts (Rashidi-Ranjbar, Jog, Naeser) | Clinical trial seeding | Month 3 |
| FAI-CV01/CV02/CV03 bench and clinical | G3-T2 full closure; T2 launch | Phase 6 (Month 18–22) |
| NP-DP-001 Addendum T2 | T2 programme governance | Phase 3 start (Month 12) |

---

## 20. Plan Maintenance

This plan is a living document. It shall be updated:

1. At each gate review (G1, G2, G3) — revise milestones, close completed items, add new open items
2. When a significant design change is approved — revise affected phase activities
3. When a key personnel hire is made — update Section 7 roles and approval authorities
4. When the legal entity is incorporated — update entity name throughout and ratify under change control
5. At T1 launch (Month 18) — archive Phase 0–5 content; add Phase 6 detail for T2 programme

Responsibility for maintaining this document: Quality Lead (from Month 6) or CEO (prior to Month 6).

---

*This document is a controlled QMS record. Changes require Quality Lead or CEO approval and DHF update per NP-QMS-DC-001.*
