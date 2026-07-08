# Breach Response Plan

**Project:** NeurOne
**Document:** NP-SEC-BR-001
**Revision:** A
**Date:** 2026-06-02
**Status:** ACTIVE
**Effective Date:** 2026-06-02
**Author:** Quality Lead (interim: Steve Hickman, CEO)
**Approved By:** Steve Hickman, CEO
**References:** HIPAA 45 CFR §§164.400–414; FTC HBNR 16 CFR Part 318; GDPR Art. 33–34; NP-PRIV-REM-001 §4.5–4.8; NP-QMS-CAPA-001 Rev A
**Related Issues:** —
**Gate:** —
**IEC 62304 Class:** —
**Applicable Standard:** HIPAA 45 CFR §§164.400–414; FTC HBNR 16 CFR Part 318; GDPR Art. 33–34
**Next Review:** Annual; also triggered by any actual breach event. Next rehearsal before T1 launch (Month 18, target November 2027).
**Jurisdiction Scope:** US federal (HIPAA, FTC HBNR), US state (all 50 states breach notification), EU/EEA GDPR, UK GDPR

---

## 1. Purpose and Scope

This plan governs NeurOne's detection, response, notification, and post-incident review procedures for any event that constitutes or may constitute a security breach involving personal data. It applies to all NeurOne systems and personnel and to all personal data held by or on behalf of NeurOne, including:

- SHDR fleet database (device telemetry linked to warranty tokens)
- Warranty registration database (name, email, address, serial number)
- T2 clinical platform (UHDR-derived clinical records, FHIR resources)
- POA vault (healthcare power of attorney documents)
- App analytics and crash report data
- Any third-party processor holding NeurOne personal data under a BAA or DPA

UHDR stored on user devices is encrypted with a key NeurOne does not hold. A breach of the device itself is a notification obligation of the device owner (the user), not NeurOne — unless NeurOne's own systems were the attack vector. This distinction must be assessed at the time of any incident.

---

## 2. Escalation Chain

| Role | Responsibility | Contact (update at each person change) |
|---|---|---|
| **Incident Reporter** | Any employee or contractor who discovers or suspects a breach | — |
| **Incident Commander** | CEO (Steve Hickman) — owns all breach response decisions | everyonecontactsme@gmail.com |
| **Legal Counsel** | Outside counsel with HIPAA/FTC/GDPR expertise — advises on notification obligations | **INTERIM: CEO (Steve Hickman) — everyonecontactsme@gmail.com.** Replace with named outside counsel before any production system holding personal data goes live. Candidate requirements: HIPAA + FTC HBNR experience; available on nights/weekends under retainer; jurisdiction coverage: US all 50 states + EU GDPR. *Action: Engage and name counsel; update this table.* |
| **Technical Lead** | Senior engineer responsible for containment and forensic preservation | **INTERIM: CEO (Steve Hickman).** Replace with a named senior engineer when first engineering hire is made. Responsibilities include: isolating affected systems, taking forensic images, preserving logs. *Action: Update this table when first engineer joins.* |
| **Communications** | Responsible for user notification drafting and media statements | **INTERIM: CEO (Steve Hickman) — everyonecontactsme@gmail.com.** The notification templates in Appendices A–C are pre-drafted; the CEO fills in incident-specific fields. *Action: Designate a dedicated communications contact when communications hire is made.* |
| **Quality Lead** | Responsible for CAPA initiation, DHF documentation, and regulatory record | CEO initially; VP Quality when hired (Month 6 target) |

**On-call rule:** The Incident Commander and Technical Lead must be reachable at all times once the company has any production system holding personal data. Define an on-call rotation when headcount exceeds 5.

---

## 3. Breach Detection Signals

The following automated alerts should be configured in the fleet monitoring and infrastructure systems. Each alert should page the Technical Lead within 5 minutes of trigger.

| Signal | Threshold | System | Priority |
|---|---|---|---|
| SHDR fleet DB: query returns >500 unique device IDs in a single session from a non-production IP | Any occurrence | Fleet DB monitoring | P1 |
| Warranty DB: bulk read of >50 registrant records in a single API session | Any occurrence | Warranty system | P1 |
| T2 clinical platform: API key used from a new IP + geolocation inconsistent with prior use | Any occurrence | API gateway | P1 |
| POA vault: access by a user not on the named reviewer list | Any occurrence | Vault access log | P1 |
| SHDR fleet DB: new outbound connection to an IP not in the approved egress list | Any occurrence | Network monitoring | P1 |
| App crash reporter: crash report volume spike >5× baseline in 1 hour | Any occurrence | Crash reporting platform | P2 |
| SSL certificate expiry within 7 days | Any occurrence | Certificate monitor | P2 |
| Admin account login from an unrecognised device | Any occurrence | Identity provider | P1 |

**Manual signals:** Users may report suspected breaches via the in-app privacy contact mechanism or by email to [privacy@neurone.life — designate before T1 launch]. All user reports are treated as P1 until assessed.

---

## 4. Incident Severity Classification

| Severity | Criteria | Initial response time |
|---|---|---|
| **P1 — Critical** | PHI or health-inferrable personal data involved OR >100 individuals affected OR ransomware or destructive malware confirmed OR breach by a state actor suspected | Incident Commander engaged within 30 minutes |
| **P2 — High** | Non-health personal data (warranty registrant contact info) involved, <100 individuals, no suspected state actor | Incident Commander engaged within 4 hours |
| **P3 — Low** | Potential breach, not yet confirmed; or confirmed breach with no personal data exfiltrated | Technical Lead assesses within 24 hours |

**Regulatory notification trigger:** Any P1 or P2 incident requires a notification trigger assessment within 24 hours of the Incident Commander's engagement. See §6.

---

## 5. Response Procedures

### 5.1 Immediate containment (0–4 hours)

1. Technical Lead isolates the affected system (revoke API keys, rotate secrets, disable affected accounts, isolate network segment as applicable).
2. Do NOT wipe or reimage any system until forensic images are taken (see §5.2).
3. Incident Commander convenes response team call within 30 minutes of P1 classification.
4. Legal Counsel engaged immediately for P1; within 4 hours for P2.
5. Document all actions taken with timestamps in the incident log (see §5.4).

### 5.2 Forensic preservation

1. Technical Lead takes read-only forensic image of all affected systems before any remediation.
2. Access logs, application logs, network flow logs, and database query logs for the 30 days preceding the incident are preserved and write-protected.
3. Log retention: forensic logs must be retained for a minimum of 6 years (HIPAA record retention requirement) or the duration of any regulatory proceeding, whichever is longer.
4. Chain of custody log: document who handled forensic images and when.

### 5.3 Scope determination

The following questions must be answered before notification timing can be assessed:

1. What data was accessed, exfiltrated, or exposed?
2. What categories of personal data are involved (health-inferrable, warranty registrant contact, clinical PHI)?
3. What is the approximate number of affected individuals?
4. What is the geographic distribution of affected individuals (states, EU member states)?
5. Is there a low probability of compromise? (For HIPAA: can a four-factor risk assessment support a "low probability" determination that excludes this from the Breach Notification Rule?)
6. Was the data encrypted at the time of exposure? (UHDR is always encrypted with a key NeurOne does not hold — UHDR device breaches may be excluded from notification if the key was not compromised.)

### 5.4 Incident log

Maintain a running timestamped log throughout the incident. Minimum entries:
- Date/time of discovery and by whom
- Date/time of Incident Commander engagement
- Date/time of Legal Counsel engagement
- Each containment action taken
- Each forensic preservation action taken
- Scope determination answers (§5.3)
- Each notification made and to whom

---

## 6. Regulatory Notification Requirements

### 6.1 Decision tree

```
Was personal data involved?
  No → Close as security incident, no privacy notification required. Document in CAPA log.
  Yes ↓

Is NeurOne a HIPAA covered entity for this data?
  Yes (T2 clinical operations) → HIPAA Breach Notification Rule applies (§6.2)
  No (T1 consumer / PHR operations) → FTC HBNR applies (§6.3)
  Both may apply → follow both

Are EU residents affected?
  Yes → GDPR Art. 33-34 applies (§6.4) in addition to above
  No → skip §6.4

Are US state residents affected?
  Yes → State breach laws apply (§6.5)
  No → skip §6.5
```

### 6.2 HIPAA Breach Notification (T2 covered entity operations)

**Reference:** 45 CFR §§164.400–414; NP-PRIV-REM-001 §4.5  

| Action | Deadline | Responsible |
|---|---|---|
| Notify affected individuals (written, first-class mail or email if agreed) | ≤60 days from discovery | Incident Commander + Communications |
| Notify HHS OCR via ocrportal.hhs.gov | ≤60 days from discovery (≥500 in a state: immediately + media) | Legal Counsel |
| Notify media (≥500 individuals in a single state) | ≤60 days from discovery | Communications |
| Log breach for annual HHS report (<500 individuals per state) | By 60 days after end of calendar year | Quality Lead |

**Template:** See Appendix A (individual notification letter — HIPAA).

### 6.3 FTC Health Breach Notification Rule (T1 consumer operations)

**Reference:** 16 CFR Part 318; NP-PRIV-REM-001 §4.6  

| Action | Deadline | Responsible |
|---|---|---|
| Notify affected individuals | Without unreasonable delay; ≤60 days from discovery | Incident Commander + Communications |
| Notify FTC via ftc.gov/databreaches | Without unreasonable delay; ≤60 days from discovery | Legal Counsel |
| Notify media (≥500 individuals in a single state) | ≤60 days from discovery | Communications |

**Template:** See Appendix B (individual notification — FTC HBNR).

### 6.4 GDPR Art. 33-34 (EU residents affected)

**Reference:** GDPR Art. 33-34; EDPB Guidelines 9/2022; NP-PRIV-REM-001 §4.7  

| Action | Deadline | Responsible |
|---|---|---|
| Notify lead supervisory authority (or each national DPA if no EU establishment) | **72 hours** from becoming aware | Legal Counsel (EU privacy specialist) |
| Notify affected individuals if high risk | Without undue delay | Communications |
| File supplementary notification if initial is incomplete | As soon as information is available | Legal Counsel |

**EU representative (Art. 27):** Until NeurOne designates an EU Art. 27 representative, notify the DPA of each member state where affected data subjects are located. Designating a representative (before EU T1 launch) simplifies this to a single lead DPA notification.

**Template:** See Appendix C (GDPR supervisory authority notification — Art. 33).

### 6.5 US State Breach Notification Laws

**Reference:** NP-PRIV-REM-001 §4.8; NCSL compilation  

The following states have the most restrictive timelines and/or the highest populations of likely NeurOne customers:

| State | Deadline | Special provision | Portal |
|---|---|---|---|
| California | 30 days (medical info under CMIA) | CMIA: $1,000 statutory damages per affected CA resident | AG notification if >500 CA residents |
| Florida | 30 days | — | FL AG portal |
| Colorado | 30 days | — | CO AG portal |
| Washington | 30 days | My Health My Data Act: health data subject to enhanced requirements | WA AG portal |
| Hawaii | 72 hours | Most restrictive US state | HI OCP |
| New York | "Expedient" (30–45 days in practice) | SHIELD Act | NY AG portal |
| Texas | 60 days | TDPSA | TX AG portal |

**Operational requirement:** Legal Counsel must maintain a current state notification portal list. The list in this table must be reviewed and updated at each annual rehearsal (STEP-27).

---

## 7. User Notification Templates

### Appendix A — HIPAA Individual Notification Letter

> **[NeurOne letterhead]**
>
> **Notice of Privacy Practices Breach**
>
> Dear [Name],
>
> We are writing to inform you of an incident that may have affected the privacy of your health information that NeurOne maintains in connection with your NeurOne Pro device and clinical services.
>
> **What Happened:** On approximately [date], we discovered that [brief factual description of the incident — e.g., unauthorised access to our T2 clinical platform database occurred between [date] and [date]].
>
> **What Information Was Involved:** The following types of information may have been accessed: [list — e.g., EEG session data, HRV coherence scores, session timestamps, protocol parameters used during your sessions]. [Your name, address, and social security number were NOT involved / were also involved — choose as applicable.]
>
> **What We Are Doing:** We have [taken the following steps: describe containment, remediation, and security improvements]. We have also notified the Department of Health and Human Services as required by law.
>
> **What You Can Do:** We recommend that you [monitor your accounts / contact us with any questions]. This incident did not involve financial account numbers, social security numbers, or passwords [modify as applicable].
>
> **For More Information:** If you have questions, please contact our Privacy Officer at [privacy@neurone.life] or call [toll-free number] between [hours]. This line will remain active for at least 90 days.
>
> We sincerely apologise for this incident and take the security of your health information very seriously.
>
> Sincerely,  
> Steve Hickman  
> Chief Executive Officer, NeurOne

---

### Appendix B — FTC HBNR Individual Notification

> **[NeurOne letterhead]**
>
> **Important Notice: Your Health Information**
>
> Dear [Name],
>
> NeurOne is contacting you because a security incident occurred that may have affected health-related information stored in your NeurOne account.
>
> **What happened:** [Date] — we discovered that [brief description].
>
> **What information was involved:** [List PHR identifiable health information — e.g., session records showing use of specific NeurOne features, HRV data, session timing].
>
> **What we have done:** [Describe containment and remediation steps].
>
> **What you can do:** [Steps specific to the incident — e.g., nothing further required / consider contacting your healthcare provider].
>
> **Contact us:** [privacy@neurone.life] or [toll-free number] — active for 90 days from this notice.
>
> Steve Hickman  
> CEO, NeurOne

---

### Appendix C — GDPR Art. 33 Supervisory Authority Notification

> **Personal Data Breach Notification under Article 33 GDPR**
>
> **To:** [Supervisory Authority name and address]  
> **From:** NeurOne, [address]; Data controller contact: [privacy@neurone.life]  
> **Date:** [Date — within 72 hours of awareness]  
> **Reference:** [Internal incident ID]  
>
> **1. Nature of the personal data breach:**  
> [Description of the incident — what happened, how, when discovered]
>
> **2. Categories and approximate number of data subjects:**  
> [Category — e.g., EU consumer device users / T2 clinical patients]; [Approximate number]
>
> **3. Categories and approximate number of personal data records:**  
> [Category — e.g., SHDR device telemetry records / T2 clinical FHIR records]; [Approximate number]
>
> **4. Contact point for further information:**  
> [Name, title, email, phone]
>
> **5. Likely consequences of the breach:**  
> [Assessment — e.g., risk of re-identification; risk of health status inference; financial risk]
>
> **6. Measures taken or proposed:**  
> [Containment actions; remediation actions; planned systemic improvements]
>
> **Note:** This notification is made within 72 hours of becoming aware of the breach. [If incomplete: Further information will be provided as it becomes available; the reason for this phased notification is [reason].]

---

## 8. Post-Incident Review

Within 14 days of incident closure (or as soon as containment is confirmed if sooner):

1. Conduct post-mortem: timeline of events, root cause, containment effectiveness, notification timeline compliance.
2. Identify systemic causes and initiate CAPA per NP-QMS-CAPA-001 Rev A.
3. Update NP-PRIV-REM-001 if the incident reveals a gap not already addressed in the remediation calendar.
4. Update this document (NP-SEC-BR-001) if detection signals, escalation chain, or templates require revision.
5. Document post-mortem record in DHF.

---

## 9. Annual Tabletop Exercise

**Frequency:** Annual. First exercise: before T1 launch (Month 18 target).  
**Duration:** 60 minutes.  
**Participants:** All roles named in §2.  
**Facilitator:** CEO (or external incident response specialist for the first exercise — recommended).

**Scenario for first exercise (adapt annually):**
> A researcher reports that a downloaded NeurOne research extract contains data that appears to re-identify a participant when combined with a publicly available hospital discharge dataset. The extract was transmitted 90 days ago. The study involved 45 participants from three US states (including California) and 12 EU residents.

**Exercise objectives:**
- Walk through §4 (scope determination) — what regulatory frameworks apply?
- Walk through §6 (notification decision tree) — which notifications are required and by when?
- Identify gaps: is the notification portal list current? Are contact details for Legal Counsel current? Are templates ready?
- Identify timeline risks: can the 72-hour GDPR window be met with current team capacity?

**Record:** Document exercise date, attendees, scenario used, gaps identified, and any NP-SEC-BR-001 updates triggered. File in DHF.
