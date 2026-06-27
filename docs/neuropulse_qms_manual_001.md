# NeuroPulse Quality Management System Manual

**Project:** NeuroPulse  
**Document:** NP-QMS-001  
**Revision:** A  
**Date:** 2026-05-13  
**Status:** ACTIVE  
**Effective Date:** 2026-05-13  
**Author:** Quality Lead (position open — interim authority: Steve Hickman, CEO)  
**Approved By:** Steve Hickman, CEO  
**References:** —  
**Related Issues:** GitHub Issue #33  
**Gate:** —  
**IEC 62304 Class:** —  
**Applicable Standard:** 21 CFR Part 820, ISO 13485:2016  
**Next Review:** 2027-05-13 (annual)

---

## 1. Purpose and Scope

### 1.1 Purpose

This Quality Management System (QMS) Manual defines the framework, processes, and responsibilities governing all design, development, manufacturing, and post-market activities for NeuroPulse medical devices. It establishes the quality infrastructure required to support T2 FDA 510(k) clearance and ISO 13485:2016 certification.

### 1.2 Scope

This QMS covers:

- All T1 (NeuroPulse Home, FDA-exempt wellness) design and development activities from company formation onward
- All T2 (NeuroPulse Pro, FDA 510(k) target) design and development activities
- Software development for all three IEC 62304 software components (Safety MCU Class C, Main Processor Class B, iOS/Android App Class B)
- Supplier qualification and incoming quality control
- Post-market surveillance and complaint handling
- Records and document control

**Exclusions:** Manufacturing process validation (deferred to contract manufacturer qualification, post-tooling). Clinical investigation activities (governed separately by IRB and applicable regulations; QMS governs design inputs/outputs from those investigations).

### 1.3 Regulatory framework

| Standard / Regulation | Applicability |
|---|---|
| 21 CFR Part 820 (FDA Quality System Regulation) | T2 510(k) devices — mandatory |
| ISO 13485:2016 (Medical Devices QMS) | T1 and T2 — voluntary adoption for T2 readiness |
| ISO 14971:2019 (Risk Management) | All devices — mandatory for T2 |
| IEC 62304:2006+AMD1:2015 (Software Lifecycle) | All software — mandatory for T2 |
| IEC 62366-1:2015 (Usability Engineering) | T2 — Human Factors / HFE |
| 21 CFR Part 11 (Electronic Records) | UHDR/SHDR digital records |
| ISO/IEC 80001-1 (IT Networks — Medical Devices) | HIPAA cloud (T2 Pro Full) |

---

## 2. Quality Policy

NeuroPulse designs devices that are safe and effective by engineering intent, not by regulatory compliance alone. Quality is built into every design decision — the UHDR/SHDR separation, the dual-processor safety architecture, the hardware-enforced stimulation limits, and the five-layer EMF shielding are all expressions of quality.

**Quality commitments:**
1. All design decisions affecting safety are documented, reviewed, and traceable to design inputs
2. Risks are identified, assessed, and mitigated before they reach users
3. Software safety classification drives the rigour of each software component's lifecycle
4. No QMS requirement is treated as a checkbox — each process element exists because the failure mode it prevents is real

---

## 3. Document Numbering Scheme

### 3.1 Document number format

```
NP-[CATEGORY]-[SUBJECT]-[SEQ]  Rev [LETTER]
```

| Category code | Category |
|---|---|
| QMS | Quality Management System procedures |
| DHF | Design History File index and entries |
| FW | Firmware specification |
| HW | Hardware specification |
| TOOL | Tooling specification |
| FAI | First Article Inspection / test |
| REG | Regulatory submission |
| PROC | Procurement / supplier |
| COORD | Engineering coordination |
| RM | Risk Management |
| SW | Software Development Plan |
| SES | Session protocol |
| CLIN | Clinical strategy |
| MOD | Modality extension |

### 3.2 Revision convention

- Rev A: Initial release
- Rev B, C, ...: Each change requiring approval
- Draft suffix (e.g., "Rev A DRAFT"): Pre-approval working version

### 3.3 Change control

All documents at Rev A or above are under change control. Changes require:
1. Author drafts revision with tracked changes or new rev letter
2. Technical reviewer approval (subject matter expert)
3. Quality approval (Quality Lead or interim authority)
4. Effective date set; prior revision archived

---

## 4. QMS Process Map

```
┌────────────────────────────────────────────────────────────────┐
│                    MANAGEMENT RESPONSIBILITY                   │
│            Quality Policy · Resource Management                │
└────────────────────┬───────────────────────────────────────────┘
                     │
        ┌────────────▼────────────┐
        │   DOCUMENT CONTROL      │  ◄── All QMS records
        │   NP-QMS-DC-001         │
        └────────────┬────────────┘
                     │
     ┌───────────────┼───────────────────┐
     │               │                   │
┌────▼────┐   ┌──────▼──────┐   ┌───────▼──────┐
│ DESIGN  │   │    RISK     │   │   SOFTWARE   │
│CONTROLS │   │ MANAGEMENT  │   │ DEVELOPMENT  │
│NP-QMS-  │   │  NP-RM-001  │   │  NP-SW-001   │
│DC-001   │   │(ISO 14971)  │   │(IEC 62304)   │
└────┬────┘   └──────┬──────┘   └───────┬──────┘
     │               │                   │
     └───────────────┼───────────────────┘
                     │
        ┌────────────▼────────────┐
        │    SUPPLIER CONTROLS    │  NP-PROC-SUP-001
        └────────────┬────────────┘
                     │
        ┌────────────▼────────────┐
        │         CAPA            │  NP-QMS-CAPA-001
        └────────────┬────────────┘
                     │
        ┌────────────▼────────────┐
        │   POST-MARKET SURV.     │  (NP-PMS-001, planned Year 2)
        └─────────────────────────┘
```

---

## 5. Management Responsibility

### 5.1 Quality organisation (current state)

| Role | Current assignment | QMS authority |
|---|---|---|
| CEO / Founder | Steve Hickman | Apex authority; approves all QMS procedures |
| Quality Lead | **OPEN — hire Year 1** | Day-to-day QMS ownership; document approvals; CAPA management |
| VP Engineering | **OPEN — hire Year 1** | Technical review of all design specifications |
| Regulatory Counsel | External (to be engaged per §13.1) | Regulatory strategy input to QMS |

**Interim arrangement until Quality Lead is hired:** Founder/CEO exercises Quality Lead authority. All QMS documents are flagged "Quality Lead: interim — CEO" in their approval block.

### 5.2 Management review

Minimum annual management review covering:
- QMS performance metrics (CAPA trends, document change frequency, audit findings)
- Post-market surveillance data (post-launch)
- Resource adequacy
- QMS updates driven by regulatory changes
- Design phase status vs. development timeline

Record: management review minutes filed in DHF under NP-QMS-001 appendix log.

### 5.3 Internal audit

- Annual internal audit of QMS against 21 CFR Part 820 and ISO 13485:2016
- First audit: within 12 months of first QMS document effective date
- Audit findings become CAPA inputs
- Third-party gap assessment recommended prior to first 510(k) submission (Month 18 target)

---

## 6. Document and Record Control

### 6.1 Controlled document storage

All controlled documents are stored in the NeuroPulse GitHub repository (`stevehickman/NeuroPulse`) under version control. GitHub commit history constitutes the version and change audit trail.

For documents requiring wet or electronic signatures before GitHub release (e.g., regulatory submissions), a parallel PDF signature record is maintained in the secure document archive (location: to be designated at QMS system formalisation).

### 6.2 Document states

| State | Meaning |
|---|---|
| DRAFT | Working version, not yet approved |
| ACTIVE | Approved, currently effective |
| SUPERSEDED | Replaced by higher revision; archived |
| OBSOLETE | Withdrawn; no longer applicable |

### 6.3 Record retention

| Record type | Minimum retention |
|---|---|
| Design History File | Life of device + 2 years (21 CFR §820.180) |
| Risk Management File | Life of device + 2 years |
| Software lifecycle records | Life of device + 2 years |
| CAPA records | 5 years minimum |
| Supplier qualification | 3 years post last procurement |
| Clinical investigation records | Per applicable regulation (≥15 years for 510(k) devices) |

---

## 7. Design Controls — Process Summary

Full procedure: **NP-QMS-DC-001 Rev A**

Design controls apply to all T1 and T2 design activities. The process follows 21 CFR §820.30 and ISO 13485:2016 clause 7.3:

| Phase | 21 CFR §820.30 | Key output |
|---|---|---|
| Planning | §820.30(b) | Design plan, team assignments |
| Design inputs | §820.30(c) | Requirements, standards, use cases |
| Design outputs | §820.30(d) | Specifications, drawings, software |
| Design review | §820.30(e) | Formal review records |
| Design verification | §820.30(f) | Test protocols and reports |
| Design validation | §820.30(g) | Validation protocols and reports |
| Design transfer | §820.30(h) | Manufacturing transfer records |
| Design changes | §820.30(i) | Change orders under document control |
| Design history file | §820.30(j) | DHF index NP-DHF-001 |

---

## 8. Risk Management — Process Summary

Full plan: **NP-RM-001 Rev A**

Risk management follows ISO 14971:2019 across the full device lifecycle. The risk register (RISK-01 through RISK-25) initiated during pre-formation design work is formally brought under QMS change control effective 2026-05-13.

Risk acceptability matrix is defined in NP-RM-001 §4. All risks rated OPEN in the register must achieve MITIGATED or ACCEPTED WITH RATIONALE status before 510(k) submission.

---

## 9. Software Development — Process Summary

Full plan: **NP-SW-001 Rev A**

Three software items, each with assigned IEC 62304 safety class:

| Software item | Class | Rationale |
|---|---|---|
| Safety MCU firmware (STM32G071) | **Class C** | Controls stimulation enable GPIO; failure or anomalous behaviour could directly harm patient |
| Main processor firmware (NXP i.MX RT1062) | **Class B** | Session orchestration, EEG processing; safety MCU provides independent safety backstop |
| iOS/Android application | **Class B** | Signs session protocols; UHDR/SHDR management; safety MCU provides independent safety backstop |

Class C requires unit-level traceability from requirement → design → implementation → test, and full software problem resolution process.

---

## 10. Supplier Controls — Process Summary

Full procedure: **NP-PROC-SUP-001 Rev A** (existing document)

QMS adds the following requirements to existing supplier selection:
- Supplier qualification records filed in DHF under the procurement category
- Critical suppliers (CAT-A moulding, CAT-B CFRP, CAT-C PDMS) require written quality agreements
- Incoming inspection acceptance criteria defined per component type
- Supplier change notification requirement: supplier must notify NeuroPulse of any process changes affecting qualified parameters

---

## 11. CAPA — Process Summary

Full procedure: **NP-QMS-CAPA-001 Rev A**

CAPA is triggered by:
- Internal audit findings
- Design verification or validation failures
- Risk register items that escalate in severity
- Supplier non-conformances
- Post-market complaints (post-launch)
- Any deviation from a controlled QMS procedure

All open CAPAs are tracked in the CAPA log (to be established in QMS system at formalisation). CAPA status is reviewed at each management review.

---

## 12. QMS Formalisation Roadmap

The following actions are required to progress from initial QMS establishment to full 510(k)-ready QMS:

| Action | Owner | Target date |
|---|---|---|
| Hire Quality Lead | CEO | Month 6 |
| Engage QMS consultant for gap assessment | CEO / Quality Lead | Month 3 |
| Implement electronic QMS (eQMS) platform | Quality Lead | Month 9 |
| Transfer all documents from GitHub-native to eQMS with formal signature blocks | Quality Lead | Month 12 |
| Conduct first internal QMS audit | Quality Lead | Month 12 |
| Engage third-party 510(k) readiness assessment | Quality Lead + Regulatory Counsel | Month 18 |
| ISO 13485:2016 certification audit (optional but recommended pre-510(k)) | Quality Lead | Month 24 |

**eQMS platform candidates (not yet selected):** MasterControl, Veeva Vault QMS, Greenlight Guru (purpose-built for FDA-regulated startups), SimplerQMS. Selection criteria: 21 CFR Part 11 compliance, IEC 62304 module, DHF module, cost-appropriate for startup stage.

---

## 13. Relationship to Design History File

This QMS Manual is the apex document. The Design History File (NP-DHF-001) indexes all design documentation and serves as the 510(k) technical file backbone. Every design specification, test report, risk file entry, and software plan is a DHF record. The DHF is maintained from first QMS effective date forward; prior-formation documents are entered retroactively under the "initial entry" change description.

---

## 14. Document History

| Rev | Date | Author | Description |
|---|---|---|---|
| A | 2026-05-13 | Interim Quality (CEO) | Initial release. QMS established at company formation. All NP-QMS, NP-DHF, NP-RM, NP-SW, and NP-QMS-CAPA procedures released simultaneously. |

---

*NP-QMS-001 Rev A — ACTIVE — Effective 2026-05-13*
