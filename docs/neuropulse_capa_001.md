# Corrective and Preventive Action Procedure

**Project:** NeuroPulse
**Document:** NP-QMS-CAPA-001
**Revision:** A
**Date:** 2026-05-13
**Status:** ACTIVE
**Effective Date:** 2026-05-13
**Author:** Quality Lead (interim: Steve Hickman, CEO)
**Approved By:** Steve Hickman, CEO
**References:** —
**Related Issues:** GitHub Issue #33
**Gate:** —
**IEC 62304 Class:** —
**Applicable Standard:** 21 CFR §820.100 (CAPA), ISO 13485:2016 §8.5.2/§8.5.3
**Next Review:** 2027-05-13

---

## 1. Purpose

This procedure defines the Corrective and Preventive Action (CAPA) process for NeuroPulse. CAPA ensures that:
- Non-conformances and quality problems are investigated and corrected
- Root causes are identified and eliminated to prevent recurrence (Corrective Action)
- Potential problems are identified and eliminated before they occur (Preventive Action)
- The effectiveness of actions taken is verified

---

## 2. Scope

This CAPA procedure applies to:
- All design non-conformances (failures to meet design inputs, verification failures, validation failures)
- Supplier non-conformances
- Internal audit findings
- Post-market complaints and adverse event reports
- Software problem reports with safety significance
- Risk register items that escalate in severity or probability
- Deviations from any controlled QMS procedure

---

## 3. Responsibilities

| Role | CAPA responsibility |
|---|---|
| Quality Lead (interim: CEO) | CAPA procedure owner; opens and closes all CAPAs; escalates safety-significant CAPAs; presents CAPA status at management review |
| VP Engineering (open) | Root cause investigation for design and firmware non-conformances; assigns technical action owners |
| Lead Hardware Engineer (open) | Root cause investigation for hardware non-conformances |
| Lead Firmware Engineer (open) | Root cause investigation for software problem reports |
| Any team member | Can identify and report a potential CAPA trigger; completes assigned actions |

---

## 4. CAPA Trigger Sources

A CAPA shall be opened for any of the following trigger events:

| Trigger category | Examples | Initial severity |
|---|---|---|
| Design verification failure | FAI test item fails acceptance criteria | Determine on case-by-case |
| Design validation failure | Usability summative study reveals critical use error | Likely Major or Critical |
| Software problem report | Firmware bug with safety significance; Class C unit test failure | Per SW problem severity classification (NP-SW-001 §8.4) |
| Internal audit finding | Procedure not followed; document not under change control | Minor to Major |
| Supplier non-conformance | Component out of specification; supplier process change not notified | Determine on case-by-case |
| Risk register escalation | RISK rated ACCEPTABLE escalates to ALARP or UNACCEPTABLE | Likely Major |
| Post-market complaint | User-reported device malfunction; adverse event | Per complaint severity classification |
| Regulatory inspection finding | FDA inspector observation; ISO audit non-conformance | Major or Critical |
| QMS procedure deviation | Controlled document changed without approval | Minor |
| Proactive trend detection | SHDR fleet data indicates emerging failure mode | Preventive — before harm occurs |

---

## 5. CAPA Severity Classification

| Severity | Definition | Response time to open CAPA |
|---|---|---|
| Critical | Actual or potential patient harm; field safety risk; regulatory reporting obligation | Same business day |
| Major | Significant quality system gap; failure of a safety-related function; supplier non-conformance affecting product safety | Within 3 business days |
| Minor | Procedural deviation with no direct safety impact; cosmetic non-conformance; documentation gap | Within 10 business days |

---

## 6. CAPA Process

### 6.1 Process overview

```
TRIGGER IDENTIFIED
      │
      ▼
STEP 1: CAPA INITIATION
  Open CAPA record; assign number; classify severity
      │
      ▼
STEP 2: IMMEDIATE CONTAINMENT (if applicable)
  Quarantine affected product; suspend affected process;
  notify relevant parties
      │
      ▼
STEP 3: ROOT CAUSE ANALYSIS
  Determine root cause(s); document investigation
      │
      ▼
STEP 4: ACTION PLANNING
  Define corrective / preventive actions; assign owners; set due dates
      │
      ▼
STEP 5: ACTION IMPLEMENTATION
  Execute actions; update design documents, procedures, or risk register as needed
      │
      ▼
STEP 6: EFFECTIVENESS VERIFICATION
  Verify that the actions taken actually resolved the problem and root cause
      │
      ▼
STEP 7: CAPA CLOSURE
  Quality Lead reviews and approves closure; update CAPA log
      │
      ▼
STEP 8: MANAGEMENT REVIEW INPUT
  Include CAPA trends in management review (NP-QMS-001 §5.2)
```

### 6.2 Step 1 — CAPA initiation

When a trigger event is identified, the initiator shall:
1. Open a CAPA record in the CAPA log with:
   - Unique CAPA number (format: `CAPA-YYYY-NNN`, e.g., `CAPA-2026-001`)
   - Date opened
   - Trigger source and description
   - Severity classification (Critical/Major/Minor)
   - Initiator name
   - Initial description of the problem or potential problem
2. Notify the Quality Lead (interim: CEO) immediately for Critical CAPAs; within the response time for Major and Minor

### 6.3 Step 2 — Immediate containment

For Critical and Major CAPAs, immediate containment actions shall be taken before root cause analysis begins, if the problem creates an ongoing risk:
- **In development:** Suspend the affected design activity; quarantine non-conforming components or prototypes
- **Post-market:** Evaluate whether a Field Safety Corrective Action (FSCA) is required; if yes, escalate per §9

Containment actions are temporary. They do not substitute for root cause analysis and permanent corrective action.

### 6.4 Step 3 — Root cause analysis

Root cause analysis shall be proportionate to severity:

| Severity | Minimum RCA method |
|---|---|
| Critical | Formal fishbone (Ishikawa) diagram or Fault Tree Analysis; 5-Why minimum |
| Major | 5-Why analysis documented |
| Minor | Brief description of identified cause acceptable |

The root cause analysis shall distinguish between:
- **Proximate cause:** The immediate mechanism of failure
- **Root cause:** The underlying systemic cause that allowed the proximate cause to exist
- **Contributing factors:** Other factors that contributed but are not the root cause

Root cause analysis must be completed within:
- Critical: 5 business days of CAPA opening
- Major: 15 business days
- Minor: 30 business days

### 6.5 Step 4 — Action planning

Based on the root cause, actions shall be defined:

**Corrective actions** (address the root cause of an existing non-conformance):
- Design change → follow NP-QMS-DC-001 §8.1 change order process
- Procedure update → revise and re-approve the affected QMS procedure
- Supplier action → issue formal supplier corrective action request (SCAR)
- Training → document training requirement and completion
- Risk register update → update NP-RISK-001 per NP-RM-001 §8.2

**Preventive actions** (address the root cause of a potential non-conformance):
- Design review trigger → add item to design review checklist
- Supplier qualification update → add requirement to NP-PROC-SUP-001
- Monitoring enhancement → add SHDR metric or alert threshold

Each action shall have:
- A unique action item number within the CAPA
- A named owner
- A due date
- Clear acceptance criteria for completion

### 6.6 Step 5 — Action implementation

Action owners implement their assigned actions and report completion to the Quality Lead. For actions that require document changes, the change must be approved per the relevant procedure before the action is marked complete.

### 6.7 Step 6 — Effectiveness verification

After all actions are implemented, the Quality Lead shall verify that:
1. The root cause has been eliminated or adequately controlled
2. The non-conformance has not recurred (monitoring period per severity: Critical = 90 days, Major = 60 days, Minor = 30 days)
3. No new non-conformances have been introduced by the corrective actions

Effectiveness verification methods:
- Re-testing the previously failed test item
- Reviewing SHDR fleet data for trend reversal
- Reviewing subsequent audit results for the same area
- Confirming no repeat complaint of the same type

### 6.8 Step 7 — CAPA closure

The Quality Lead approves CAPA closure when:
1. All actions are implemented and verified complete
2. Effectiveness verification confirms the root cause is eliminated
3. All related documents, risk register entries, and procedures are updated

The Quality Lead signs and dates the closure in the CAPA log.

### 6.9 Step 8 — Management review

The Quality Lead presents CAPA status (open count by severity, closure trends, overdue items, repeat non-conformances) at each management review per NP-QMS-001 §5.2.

---

## 7. CAPA Log

The CAPA log is maintained by the Quality Lead and contains a record of every CAPA opened. Until a formal eQMS platform is implemented, the CAPA log is maintained as a controlled document in the GitHub repository at `docs/neuropulse_capa_log.md`.

**Minimum CAPA log fields:**

| Field | Description |
|---|---|
| CAPA number | `CAPA-YYYY-NNN` |
| Date opened | ISO 8601 date |
| Severity | Critical / Major / Minor |
| Trigger source | Category from §4 |
| Problem description | Brief summary |
| Root cause | Summary after RCA |
| Actions | List of actions with owner and due date |
| Date closed | ISO 8601 date or blank if open |
| Effectiveness verified | Yes / No / Pending |
| Quality Lead approval | Initials + date |

---

## 8. Distinction Between Corrective and Preventive Action

| | Corrective Action (CA) | Preventive Action (PA) |
|---|---|---|
| Trigger | A non-conformance has occurred | A potential non-conformance is identified before it occurs |
| Examples at NeuroPulse | FAI test failure → root cause → design change | SHDR trend shows LED degradation approaching threshold → design improvement proactively |
| ISO 13485 reference | §8.5.2 | §8.5.3 |
| 21 CFR reference | §820.100(a) | §820.100(b) |

Both are handled in a single CAPA record, with the type (CA, PA, or both) noted at initiation.

---

## 9. Regulatory Reporting and Field Safety Corrective Actions

### 9.1 Medical Device Reporting (MDR) obligations

For T2 devices post-clearance, the following must be reported to FDA under 21 CFR Part 803:
- **Death or serious injury** caused by or contributed to by a device malfunction: MDR within 30 days
- **Device malfunction** that would likely cause or contribute to death or serious injury if it recurred: MDR within 30 days
- **Imminent hazard:** Notify FDA within 5 days

MDR submission obligations are managed by Regulatory Counsel. A CAPA-Critical involving a reportable event must be escalated to Regulatory Counsel within 24 hours.

### 9.2 Field Safety Corrective Actions (FSCAs)

An FSCA (recall, field safety notice, or software update push) is required when:
- A device in the field poses a risk that requires user action or device modification
- The CAPA root cause analysis identifies a systemic issue affecting all or a subset of devices in the field

FSCA decisions are made by the Quality Lead in consultation with Regulatory Counsel. All FSCAs must be documented and, for 510(k) devices, reported to FDA.

---

## 10. Records and Retention

CAPA records shall be retained for a minimum of 5 years from the date of CAPA closure, or for the life of the device plus 2 years if the CAPA relates to a design change, whichever is longer.

CAPA records are confidential quality records and are not routinely shared externally. FDA inspectors and notified bodies may review CAPA records during inspections.

---

## 11. Document History

| Rev | Date | Author | Description |
|---|---|---|---|
| A | 2026-05-13 | Interim Quality (CEO) | Initial release. CAPA procedure established at QMS formation. |

---

*NP-QMS-CAPA-001 Rev A — ACTIVE — Effective 2026-05-13*
