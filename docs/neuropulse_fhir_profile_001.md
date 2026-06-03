# NP-INT-FHIR-001 Rev A — FHIR R4 ImplementationGuide: NeuroPulse T2 Clinical Profile

**Document number:** NP-INT-FHIR-001  
**Revision:** A  
**Status:** ACTIVE  
**Effective date:** 2026-06-03  
**Author:** Quality Lead (interim: Steve Hickman, CEO)  
**Approved by:** Steve Hickman, CEO  
**References:** NP-PRIV-REM-001 STEP-14; NP-PRIV-001 Rev A MEDIUM-01; HL7 FHIR R4 (4.0.1); 45 CFR §164.502(b) (HIPAA Minimum Necessary); NP-FW-ANON-001 Rev A  

---

## 1. Purpose and Scope

This document defines the NeuroPulse FHIR R4 ImplementationGuide (IG) — the machine-validatable specification governing all FHIR resources created, transmitted, or stored by the NeuroPulse T2 clinical platform. All T2 EHR integration code must conform to this IG before any data is transmitted to a Business Associate.

**Design principle:** NeuroPulse FHIR resources use the **minimum necessary** patient demographic data consistent with HIPAA §164.502(b). Patient resources contain only an opaque clinic-assigned identifier — no name, date of birth, address, or contact information. All clinical observations use FHIR-standard LOINC coding where codes exist; NeuroPulse-defined codes are used for device-specific observations without established LOINC equivalents.

**Scope:** T2 NeuroPulse Pro clinical operations only. T1 NeuroPulse Home wellness operations do not use FHIR.

---

## 2. IG Metadata

| Field | Value |
|---|---|
| Canonical URL | `https://fhir.neuropulse.com/ig/NeuroPulse-T2-Clinical` |
| FHIR version | 4.0.1 |
| IG version | 1.0.0 |
| Status | Active |
| Jurisdiction | US (urn:iso:std:iso:3166#US) |
| Copyright | NeuroPulse Inc. — All rights reserved |
| Publisher | NeuroPulse Inc. |
| Contact | fhir@neuropulse.com |

---

## 3. Permitted Resource Types

Only the following FHIR resource types may appear in NeuroPulse T2 clinical data exchanges. Any FHIR resource not listed below must not be created, transmitted, or stored by NeuroPulse systems.

| Resource type | Profile URL | Purpose | Status |
|---|---|---|---|
| `Patient` | `https://fhir.neuropulse.com/StructureDefinition/NP-Patient` | Patient identity (opaque identifier only) | Required |
| `Observation` | `https://fhir.neuropulse.com/StructureDefinition/NP-Observation` | Clinical measurements (EEG, HRV, dose) | Required |
| `DiagnosticReport` | `https://fhir.neuropulse.com/StructureDefinition/NP-DiagnosticReport` | Session summary report | Required |
| `Procedure` | `https://fhir.neuropulse.com/StructureDefinition/NP-Procedure` | Neuromodulation session record | Conditional (Full Clinical tier only) |

**Explicitly prohibited resource types:** `RelatedPerson`, `Coverage`, `Claim`, `ClaimResponse`, `ExplanationOfBenefit`, `MedicationRequest`, `AllergyIntolerance`, `Condition` (NeuroPulse is not a diagnostic system), `CarePlan`, `ImagingStudy`.

---

## 4. Patient Profile (NP-Patient)

### 4.1 Profile constraints

| Element | Cardinality | Constraint | Rationale |
|---|---|---|---|
| `Patient.id` | 1..1 | System-generated FHIR logical ID | Required by FHIR |
| `Patient.identifier` | 1..1 | Clinic-assigned MRN (opaque) — see §4.2 | Minimum necessary identifier |
| `Patient.identifier.system` | 1..1 | Clinic's own identifier system URI | e.g. `https://clinic.example.com/patients` |
| `Patient.identifier.value` | 1..1 | Opaque MRN assigned by the clinic | Must not be NeuroPulse account ID, email, or UHDR token |
| `Patient.name` | 0..0 | **PROHIBITED** | Privacy — minimum necessary |
| `Patient.telecom` | 0..0 | **PROHIBITED** | Privacy — minimum necessary |
| `Patient.birthDate` | 0..1 | Year only (`YYYY`) if required by clinic's EHR. Day and month **PROHIBITED**. | Minimum necessary — year provides age context for clinical interpretation |
| `Patient.gender` | 0..1 | Permitted if required by EHR system | Acceptable clinical context |
| `Patient.address` | 0..0 | **PROHIBITED** | Privacy — not needed for neuromodulation data |
| `Patient.contact` | 0..0 | **PROHIBITED** | Privacy |
| `Patient.photo` | 0..0 | **PROHIBITED** | Privacy |
| `Patient.communication` | 0..0 | **PROHIBITED** | Not relevant |

### 4.2 Patient identifier policy

The Patient identifier is the clinic's own MRN for the patient. NeuroPulse does not assign, know, or store the mapping between the MRN and the patient's identity. The clinic holds this mapping in their EHR.

**Prohibited identifier systems:** `mailto:` (email address), `urn:oid:2.16.840.1.113883.4.1` (US SSN), any system that encodes a real-world identifier.

**Validation rule:** The FHIR validator shall reject any Patient resource where `identifier.value` matches the pattern of an email address (`.*@.*`), a US SSN (`\d{3}-\d{2}-\d{4}`), a NeuroPulse account token, or a UHDR token.

---

## 5. Observation Profile (NP-Observation)

### 5.1 Base constraints

| Element | Cardinality | Constraint |
|---|---|---|
| `Observation.status` | 1..1 | Must be `final` |
| `Observation.subject` | 1..1 | Reference to NP-Patient — no display name permitted in the reference |
| `Observation.effective[x]` | 1..1 | `effectivePeriod` (start/end of session week) — **not** `effectiveDateTime` at precision finer than 1 day |
| `Observation.performer` | 0..0 | **PROHIBITED** — NeuroPulse observations are device-generated, not clinician-authored |
| `Observation.note` | 0..0 | **PROHIBITED** — free text could contain PII |
| `Observation.value[x]` | 1..1 | Must be `valueQuantity` for numeric observations |

### 5.2 NeuroPulse Observation types

**EEG observations** — no established LOINC codes for quantitative EEG spectral features. NeuroPulse local coding system used:

| Observation type | Code | System | Unit | Notes |
|---|---|---|---|---|
| Alpha band power ratio | `NP-EEG-ALPHA` | `https://terminology.neuropulse.com/observations` | `{ratio}` (dimensionless, [0,1]) | Weekly session average |
| Theta band power ratio | `NP-EEG-THETA` | `https://terminology.neuropulse.com/observations` | `{ratio}` | Weekly session average |
| Delta band power ratio | `NP-EEG-DELTA` | `https://terminology.neuropulse.com/observations` | `{ratio}` | Weekly session average |
| Gamma band power ratio | `NP-EEG-GAMMA` | `https://terminology.neuropulse.com/observations` | `{ratio}` | Weekly session average |
| Neurofeedback performance score | `NP-NFB-SCORE` | `https://terminology.neuropulse.com/observations` | `{score}` (0–100) | Per-session; weekly average |

**HRV observations** — LOINC codes used where available:

| Observation type | Code | System | Unit | Notes |
|---|---|---|---|---|
| HRV RMSSD | `80404-7` | `http://loinc.org` | `ms` | Weekly session average |
| HRV coherence score | `NP-HRV-COHERENCE` | `https://terminology.neuropulse.com/observations` | `{score}` (0–10) | NeuroPulse-defined coherence metric |
| Heart rate | `8867-4` | `http://loinc.org` | `/min` | Session average |

**Session observations:**

| Observation type | Code | System | Unit | Notes |
|---|---|---|---|---|
| PBM session dose | `NP-PBM-DOSE` | `https://terminology.neuropulse.com/observations` | `J/cm2` | Per zone; weekly aggregate |
| BES session dose | `NP-BES-DOSE` | `https://terminology.neuropulse.com/observations` | `mC` (millicoulombs) | Weekly aggregate |
| Session count | `NP-SESSION-COUNT` | `https://terminology.neuropulse.com/observations` | `{count}` | Count of completed sessions in period |

### 5.3 Observation prohibited content

- Raw EEG waveform data: **PROHIBITED** — use band power ratios only
- Raw PPG optical signal: **PROHIBITED**
- Impedance raw values: **PROHIBITED** — impedance trend direction is permitted as a coded observation
- Any `Observation.note` free text: **PROHIBITED**
- Session timestamps with precision finer than one day in `effectivePeriod`

---

## 6. DiagnosticReport Profile (NP-DiagnosticReport)

### 6.1 Session summary report

A `DiagnosticReport` provides a weekly session summary for a patient. It aggregates multiple Observations for the period.

| Element | Cardinality | Constraint |
|---|---|---|
| `DiagnosticReport.status` | 1..1 | `final` |
| `DiagnosticReport.code` | 1..1 | `NP-SESSION-REPORT` from `https://terminology.neuropulse.com/reports` |
| `DiagnosticReport.subject` | 1..1 | Reference to NP-Patient |
| `DiagnosticReport.effectivePeriod` | 1..1 | ISO week (Monday 00:00 — Sunday 23:59 UTC) |
| `DiagnosticReport.result` | 1..* | References to NP-Observation instances for this period |
| `DiagnosticReport.conclusion` | 0..0 | **PROHIBITED** — free text |
| `DiagnosticReport.presentedForm` | 0..0 | **PROHIBITED** — no document attachments |

---

## 7. Procedure Profile (NP-Procedure)

Available on Full Clinical tier only. Records the neuromodulation session as a clinical procedure for EHR audit purposes.

| Element | Cardinality | Constraint |
|---|---|---|
| `Procedure.status` | 1..1 | `completed` |
| `Procedure.code` | 1..1 | SNOMED CT `229070002` (Biofeedback therapy) or NeuroPulse code `NP-NEUROMOD-SESSION` |
| `Procedure.subject` | 1..1 | Reference to NP-Patient |
| `Procedure.performedPeriod` | 1..1 | Session start/end, precision to minute (not second) |
| `Procedure.performer` | 0..0 | **PROHIBITED** — device-performed |
| `Procedure.note` | 0..0 | **PROHIBITED** |
| `Procedure.outcome` | 0..1 | Coded only: `{satisfactory}`, `{unsatisfactory}`, `{session-terminated-early}` |

---

## 8. LSL (Lab Streaming Layer) Security Requirement

If the Business Associate implements real-time LSL streaming from the NeuroPulse hub for research or clinical monitoring purposes, the following security requirements apply:

(a) **Encrypted transport required.** All LSL streams carrying NeuroPulse data must be tunnelled through TLS 1.2 or higher. Acceptable implementations: `stunnel` with TLS certificate pinning, WireGuard VPN with certificate-based authentication, or OpenVPN with TLS mutual authentication.

(b) **Network isolation.** The LSL stream must not traverse any network segment accessible to unauthorised users. Unsecured WiFi, guest networks, and shared office networks are prohibited.

(c) **NeuroPulse warranty.** NeuroPulse warrants the integrity of data exported from the hub via USB-C or BLE. NeuroPulse does not warrant the security of LSL deployments that do not use encrypted transport per (a).

(d) **BAA coverage.** The LSL transport infrastructure is subject to this BAA if the Business Associate operates it. If a third-party LSL infrastructure vendor is used, that vendor must be added as a subcontractor under §3.4.

---

## 9. FHIR Validator Integration

The NeuroPulse T2 clinical platform must include a FHIR validator integration that runs against all resources before they are transmitted to a Business Associate or accepted from a Business Associate. The validator must:

(a) Validate all resources against the NeuroPulse IG profile versions published at the canonical URL (§2).

(b) Reject any resource that fails profile validation. Failed resources must not be stored, transmitted, or accepted.

(c) The validation result (PASS/FAIL, validation timestamp, IG version used) is logged to SHDR: `fhir_validation_result: enum`. No PHI appears in the SHDR log.

**Recommended validator:** HL7 FHIR Java Validator (org.hl7.fhir.validation) or HAPI FHIR Validator. Both support custom IG packages.

---

## 10. Open Items

| OI-ID | Description | Blocking for |
|---|---|---|
| OI-FHIR-01 | LOINC mapping review by clinical informatics specialist — confirm NP-EEG-* codes are appropriate (no established LOINC) or identify standard codes | G3 gate |
| OI-FHIR-02 | FHIR IG package publication at canonical URL (requires web infrastructure) | First T2 EHR integration pilot |
| OI-FHIR-03 | FHIR validator CI integration — IG package added to T2 backend build pipeline | T2 EHR integration development start |
| OI-FHIR-04 | `effectivePeriod` precision policy — confirm 1-day precision is sufficient for clinical utility; no finer than 1-day permitted | First T2 data exchange |
| OI-FHIR-05 | `Procedure.code` SNOMED CT mapping review — `229070002` (Biofeedback therapy) may not be the most precise code for TMS or tDCS; clinical informatics review required | T2 clinical mapping |

---

*NP-INT-FHIR-001 Rev A — CONFIDENTIAL — NeuroPulse Design Programme*
