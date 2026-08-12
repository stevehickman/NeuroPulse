# Design Controls Procedure

**Project:** NeurOne
**Document:** NP-QMS-DC-001
**Revision:** 1
**Date:** 2026-05-13
**Status:** ACTIVE
**Effective Date:** 2026-05-13
**Author:** Quality Lead (interim: Steve Hickman, CEO)
**Approved By:** Steve Hickman, CEO
**References:** —
**Related Issues:** GitHub Issue #33
**Gate:** —
**IEC 62304 Class:** —
**Applicable Standard:** 21 CFR §820.30 (Design Controls), ISO 13485:2016 §7.3
**Next Review:** 2027-05-13

---

## 1. Purpose

This procedure defines NeurOne's design controls process in compliance with:
- 21 CFR §820.30 (FDA Quality System Regulation — Design Controls)
- ISO 13485:2016 clause 7.3 (Design and Development)

Design controls apply from this document's effective date to all design and development activities on the NeurOne device platform.

---

## 2. Scope

This procedure applies to:
- All new hardware design activities (T1 and T2 modalities, zone modules, chassis, accessories)
- All firmware development (Safety MCU, main processor, bootloader)
- All software development (iOS/Android application, cloud services)
- Significant changes to existing approved designs
- Design transfers to manufacturing

**Exclusion:** Minor changes that do not affect safety, efficacy, or regulatory compliance follow the simplified change notification process (§8.2).

---

## 3. Roles and Responsibilities

| Role | Design controls responsibility |
|---|---|
| CEO / Founder | Approves major design changes; chairs design reviews until VP Eng hired |
| VP Engineering (open) | Technical lead for design and development; design review chair |
| Quality Lead (open; interim: CEO) | Procedure owner; approves design verification and validation protocols; signs off DHF completeness |
| Lead Hardware Engineer (to be hired) | Design input capture for hardware; design output verification |
| Lead Firmware Engineer (to be hired) | Design input capture for firmware; IEC 62304 lifecycle management |
| Regulatory Counsel (external) | Reviews design inputs for regulatory requirements; attends design reviews for T2 |

---

## 4. Design Planning (21 CFR §820.30(b) / ISO 13485:2016 §7.3.2)

### 4.1 Design plan requirement

A Design and Development Plan (NP-DP-001, to be authored by Month 3) shall be prepared for each significant design phase and shall include:
- Scope of the design phase
- Assigned responsibilities
- Schedule with review checkpoints
- Required resources
- Applicable standards and regulatory requirements
- Interfaces with other design activities or suppliers

### 4.2 Interim arrangement

Until NP-DP-001 is authored, the NP-COORD-001 Engineering Coordination Checklist serves as the design plan. Gate G1, G2, and G3 items constitute the design review checkpoints.

### 4.3 Design phase gates

The NeurOne design process uses three gates defined in NP-COORD-001:

| Gate | Phase | Key design control milestones |
|---|---|---|
| G1 | Pre-tooling design freeze | All design inputs defined; specifications at Rev 1; risk register populated |
| G2 | Firmware baseline | Software architecture complete; safety MCU interlock firmware baselined |
| G3 | T2 clinical readiness | Verification evidence complete; validation plan approved; 510(k) pre-sub filed |

---

## 5. Design Inputs (21 CFR §820.30(c) / ISO 13485:2016 §7.3.3)

### 5.1 Types of design inputs

Design inputs are the requirements that the device must meet. They include:

**Performance requirements:**
- Stimulation parameters (current ranges, waveforms, frequency ranges, charge density limits)
- EEG recording specifications (channel count, sampling rate, ADC resolution, noise floor)
- PBM irradiance and dose specifications (wavelengths, LED count, power density, J/cm² limits)
- Connectivity and operating modes
- Battery / power draw specifications

**Safety requirements:**
- Hardware-enforced stimulation limits (40µC/cm² charge density, 2mA current limits)
- Thermal limits per modality (IEC 60601-1, 42°C scalp surface limit)
- Safety MCU watchdog and heartbeat timing requirements
- IEC 62471 retinal MPE limit for visual stimulation
- Regulatory naming constraints (consumer nomenclature vs. medical device claim avoidance)

**Usability requirements:**
- Fit range (52–62cm head circumference)
- Electrode contact force (80–120g per pod)
- Zone module insertion/extraction force (≤1N for accessible design target)
- LED status indicator readability at 3 metres

**Regulatory and standards requirements:**
- IEC 60601-1 general safety
- IEC 60601-2-10 (particular requirements for nerve and muscle stimulators)
- IEC 62471 (photobiological safety)
- IEC 62133 (battery safety)
- FCC Part 15 (RF emissions)
- IEC 62304 (software lifecycle)
- 21 CFR Part 820 (QMS for T2)
- FDA cybersecurity guidance (SBOM, OTA security, vulnerability disclosure)

**Interface requirements:**
- USB-C Power Delivery protocol compliance
- Bluetooth 5.3 LE Audio GATT service compatibility
- FHIR R4 interoperability (T2 Pro Full)

### 5.2 Design input sources

Design inputs shall be gathered from:
1. Applicable standards (normative requirements — mandatory)
2. Regulatory guidance documents (FDA, ISO, IEC)
3. Clinical evidence / intended use / intended users
4. Human factors engineering / usability research
5. Competitive analysis (Vielight, Neuronic, Sens.ai benchmarks)
6. Supplier and component capabilities
7. Service and maintenance requirements
8. Business requirements (cost targets, timeline, pricing)

### 5.3 Design input documentation

Design inputs shall be documented as:
- A structured requirements specification (NP-REQ-HW-001 for hardware, NP-FW-REQ-001 for firmware — hardware requirements document pending)
- At minimum, in the Design Brief (NP-DB-004 / CLAUDE.md Rev 7 pending formal requirements extraction)

All design inputs shall be reviewed for completeness and adequacy before design begins. Conflicting requirements shall be resolved with documented rationale before the design input is baselined.

---

## 6. Design Outputs (21 CFR §820.30(d) / ISO 13485:2016 §7.3.4)

### 6.1 Types of design outputs

Design outputs are the results of the design process that define the device. They include:

| Output type | Examples | DHF location |
|---|---|---|
| Hardware specifications | FPC zone module spec (NP-HW-FPC-001), tooling specifications | §5.3, §5.6 of NP-DHF-001 |
| Firmware specifications | NP-FW-EMMC-001, NP-FW-HRV-001, NP-FW-CVNS-001, etc. | §5.4 of NP-DHF-001 |
| Source code | All firmware/ directories | §6 of NP-DHF-001 |
| Manufacturing specifications | Bill of materials, tooling drawings | NP-TOOL-* documents |
| Test specifications | FAI checklists, acceptance criteria | NP-FAI-* documents |
| Labelling | Device labelling, IFU | TBD (Year 1) |
| Risk management outputs | Risk register, residual risk report | NP-RISK-002 (index + disposition), NP-RISK-003 (hex-tile module), NP-RISK-004 (shell/socket/interconnect/hub). NP-RISK-001 superseded 2026-08-11 — retained in `docs/superseded/` as the record for 2026-05-13 to 2026-08-11 |

### 6.2 Design output requirements

Each design output shall:
1. Meet or be traceable to design inputs (traceability matrix NP-DT-001, planned Month 6)
2. Contain or reference acceptance criteria to determine whether the output is acceptable
3. Identify characteristics that are critical to safety and proper functioning
4. Be reviewed and approved before release

### 6.3 Design output approval

Design outputs at initial release (Rev A) require:
- Technical author sign-off
- Peer technical review (a second engineer or the VP Eng if hired)
- Quality review (Quality Lead or interim authority)

Minor revisions that do not affect safety or efficacy follow the expedited path (single approver).

---

## 7. Design Review (21 CFR §820.30(e) / ISO 13485:2016 §7.3.5)

### 7.1 Scheduled design reviews

Formal design reviews shall occur at each gate defined in §4.3. The gate review meeting shall:
1. Be attended by at least two engineers and the Quality Lead (or interim)
2. Assess whether all gate items are complete and adequate
3. Identify any remaining open items and assign owners and dates
4. Be documented with minutes filed in the DHF

### 7.2 Design review record content

Each design review record shall contain:
- Date and attendees
- Gate being reviewed (G1, G2, or G3)
- Items reviewed (list of documents and open items checked)
- Pass / Fail / Conditional pass determination
- List of action items with owner and target date
- Next review date or trigger condition

### 7.3 Independence requirement

At least one design review attendee shall not have direct responsibility for the design being reviewed. For pre-organisation stages, this may be the Quality Lead (interim: CEO) reviewing engineering outputs they did not author, or an engaged external reviewer.

### 7.4 Unscheduled design reviews

A design review is also required before any significant design change is approved per §8.1.

---

## 8. Design Changes (21 CFR §820.30(i) / ISO 13485:2016 §7.3.9)

### 8.1 Significant change process (Change Order)

A change is significant if it affects:
- Device safety or performance characteristics
- A previously approved design specification
- Regulatory classification or intended use
- Manufacturing processes for components with defined acceptance criteria

**Change Order process:**
1. **Initiation:** Author documents the proposed change, the reason, the design inputs affected, and the risk impact assessment
2. **Technical review:** VP Engineering (or interim) reviews technical adequacy
3. **Risk impact:** Quality Lead reviews risk register to determine whether RISK-01..25 are affected by the change, and whether new risks are introduced
4. **Regulatory review:** If the change affects regulatory classification, safety MCU Class C firmware, or 510(k) predicate, Regulatory Counsel must review
5. **Approval:** Quality Lead and VP Engineering approve the change
6. **Implementation:** Revised document released at new revision letter; superseded revision archived
7. **DHF update:** NP-DHF-001 updated with new revision record

### 8.2 Minor change notification process

A change is minor if it:
- Corrects a typographical error, cross-reference, or formatting issue
- Updates a document number or title to match an approved change
- Adds clarifying language without changing technical requirements

**Minor change process:** Single approver (document author + Quality Lead countersign). Documented in document revision history block.

### 8.3 Version control

All documents are version-controlled in the GitHub repository. Each commit that changes a controlled document shall include:
- The new revision letter (if a revision change)
- A concise change description in the commit message
- Reference to any triggering CAPA, risk register item, or issue number

---

## 9. Design Verification (21 CFR §820.30(f) / ISO 13485:2016 §7.3.6)

### 9.1 Purpose

Design verification confirms that the design output meets the design input requirements. Verification answers: "Did we build it right?"

### 9.2 Verification methods

Accepted verification methods:
- **Analysis:** Mathematical analysis, simulation, FMEA (e.g., charge density calculations, thermal models)
- **Inspection:** Physical measurement, dimensional inspection, optical inspection
- **Test:** Functional bench testing, laboratory measurements (e.g., FAI optical bench for PBM irradiance, oscilloscope for stimulation waveforms)
- **Demonstration:** Functional operation demonstration against acceptance criteria

### 9.3 Verification protocol requirements

Each verification activity shall have a written protocol (or FAI checklist section) specifying:
- The design input requirement being verified
- The acceptance criterion
- The test method and equipment
- The sample size and pass/fail rules

### 9.4 Current verification status

The following verification items are defined but PENDING hardware prototype availability:

| FAI item | Requirement being verified | Blocking condition |
|---|---|---|
| FAI-SM-04, FAI-SM-06, FAI-SM-07, FAI-SM-08 | 1064nm smart module optical bench | Requires calibrated optical bench + Hub PCB Rev B (OI-PBM-HW-01) |
| FAI-HD01, FAI-HD03, FAI-HD04 | sLORETA HD-tDCS localisation and focality | Requires T2 prototype phantom |
| FAI-CV01, FAI-CV02 | Cervical VNS electrode placement and cardiac interlock timing | Requires T2 prototype + oscilloscope bench |
| FAI-IPX-02 | IPX4 rating after 10 zone module swap cycles | Requires production-intent zone modules |
| FAI-TC02 | PDMS 200-cycle IEC 60068-2-14 thermal cycling | Requires PDMS supplier + test samples — BLOCKING |
| RISK-20 | CFRP shell slot rim Ra ≤ 1.6 µm | Requires supplier written confirmation — BLOCKING |

### 9.5 Software verification

Software verification is governed by NP-SW-001. For Class C Safety MCU firmware, unit-level test traceability is required from requirement → code → test result.

---

## 10. Design Validation (21 CFR §820.30(g) / ISO 13485:2016 §7.3.7)

### 10.1 Purpose

Design validation confirms that the device meets the needs of the intended user under actual or simulated conditions of use. Validation answers: "Did we build the right thing?"

### 10.2 Validation requirements for T2

T2 validation shall include:
1. **Human factors / usability validation (summative):** Per FDA HFE Guidance (2016) and IEC 62366-1. Summative testing must address the critical tasks and use errors identified in URRA and formative testing
2. **Clinical performance validation:** Clinical data demonstrating safety and effectiveness of each T2 modality per intended use (seeded units in research institutions, Years 2–3)
3. **Software validation:** Per NP-SW-001; includes end-to-end functional testing of session protocols and safety interlock behaviour
4. **Sterility/biocompatibility (if applicable):** ISO 10993 biocompatibility assessment for skin-contacting materials (electrode gel pads, silicone surfaces, PDMS optical windows)

### 10.3 Validation timing

Design validation must be completed on finished devices or representative devices under actual or simulated conditions of use — not on prototypes with temporary solutions. Validation occurs after verification is complete.

### 10.4 Validation plan

NP-HFE-001 (Human Factors Engineering Plan) is planned for Month 9 and will define the URRA, formative testing program, and summative study design.

---

## 11. Design Transfer (21 CFR §820.30(h) / ISO 13485:2016 §7.3.8)

Design transfer is the process of moving the device from design into production. It is not applicable until manufacturing partners are qualified.

**Design transfer shall include:**
1. Written manufacturing specifications and procedures
2. Incoming inspection specifications for all purchased components
3. Manufacturing process qualification (IQ/OQ/PQ)
4. First production unit inspection against all specifications
5. Training records for production personnel
6. Transfer review meeting with documented approval

---

## 12. Document History

| Rev | Date | Author | Description |
|---|---|---|---|
| 1 | 2026-05-13 | Interim Quality (CEO) | Initial release. Design controls procedure established at QMS formation. Applies retroactively to all in-progress design activities. |
