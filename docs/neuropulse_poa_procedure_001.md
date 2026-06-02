# NP-PROC-POA-001 Rev A — Healthcare Power of Attorney Upload Procedure

**Document number:** NP-PROC-POA-001  
**Revision:** A  
**Status:** ACTIVE  
**Effective date:** 2026-06-02  
**Author:** Quality Lead (interim: Steve Hickman, CEO)  
**Approved by:** Steve Hickman, CEO  
**Review cadence:** Annual; also triggered by any POA processing incident  
**References:** NP-PRIV-REM-001 STEP-08; HIPAA §164.312; GDPR Art. 9(2)(c); 45 CFR Part 46; NP-QMS-CAPA-001 Rev A  

---

## 1. Purpose and Scope

This procedure governs the receipt, storage, review, and retention of healthcare Power of Attorney (POA) documents submitted by POA holders who wish to manage NeuroPulse research consent and clinical data access on behalf of a patient who lacks capacity.

A healthcare POA document is among the most sensitive document types NeuroPulse will handle. It contains: the grantor's legal name, date of birth, residential address, the identity of the attorney-in-fact, the legal basis and scope of health decision authority, and in many jurisdictions the triggering health conditions. This procedure ensures that these documents are handled with appropriate controls at every stage and are retained only as long as necessary.

This procedure applies to:
- All POA documents received via the NeuroPulse app upload mechanism
- All NeuroPulse personnel who access the POA vault
- Any third-party service provider involved in the upload or storage infrastructure

---

## 2. Accepted Document Types

The following POA document types are accepted:

| Type | Description | Jurisdictional scope |
|---|---|---|
| Durable Healthcare POA | General healthcare decision authority, survives incapacity | All US states |
| Healthcare Proxy | Similar to durable POA — jurisdiction-specific terminology | NY, MA, and others |
| Advance Healthcare Directive (with POA component) | Living will plus POA agent designation | All US states |
| Personal Welfare LPA | UK equivalent — Lasting Power of Attorney for personal welfare | United Kingdom |
| Mandat de protection future | French equivalent | France |
| EU-equivalent healthcare proxy | Any EU member state equivalent document with court or notarial certification | EU member states |

**Not accepted:** General financial POA (no health authority scope); self-authored letters; unwitnessed documents; documents more than 5 years old without re-execution or renewal documentation.

---

## 3. Upload Process

### 3.1 Initiating an upload

1. POA holder navigates to the app's Research Consent section → "I am a legally authorised representative."
2. App presents: (a) description of what a valid POA document must contain; (b) clear statement that the document will be reviewed by a human NeuroPulse reviewer; (c) statement that the original document will be permanently deleted 30 days after review is complete and only a structured summary will be retained.
3. POA holder selects "Upload Document."

### 3.2 Secure upload mechanism

1. App requests a signed upload URL from the NeuroPulse POA endpoint (POST /api/poa/upload-token).
2. The endpoint returns a time-limited (15-minute expiry) pre-signed URL pointing directly to the encrypted POA vault storage bucket.
3. The app uploads the document directly from the device to the vault storage bucket via the signed URL. The document **does not pass through NeuroPulse application servers** in plaintext at any point.
4. Vault storage bucket configuration:
   - Server-side encryption: AES-256 with vault-specific KMS key (not the same key as SHDR or UHDR)
   - Bucket policy: no public access; no cross-account access; access restricted to the vault IAM role only
   - Versioning: disabled (single copy; no version accumulation)
5. On successful upload, the app receives a POA submission token (opaque UUID). This token is stored in the user's device Config partition, not in UHDR. It is the only reference the app holds to the POA submission.
6. The upload event is logged: timestamp, device ID (not patient identity), file hash (SHA-256 of the uploaded document), POA submission token. This log goes to SHDR. The log entry does not contain patient name, DOB, or document content.

### 3.3 Upload failure handling

If the signed URL expires before upload completes, the app prompts the user to retry. No partial uploads are accepted. The vault storage bucket has a lifecycle policy that deletes any objects not associated with a completed upload confirmation within 1 hour.

---

## 4. POA Vault — Access Controls

### 4.1 Reviewer access list

Access to the POA vault (including the ability to read any uploaded document) is restricted to a maximum of three named individuals:

| Role | Access type | Justification |
|---|---|---|
| POA Review Lead | Read, update status | Primary reviewer — responsible for jurisdiction determination and scope assessment |
| POA Review Backup | Read, update status | Covers absence of POA Review Lead; must not access a document already reviewed by the Lead unless escalation is required |
| CEO (break-glass only) | Read (requires reason logging) | Escalation path for disputed POA documents or legal challenges |

The access list is maintained in the vault IAM configuration. Any addition requires CEO written approval and a formal access log entry. The list is reviewed and re-confirmed quarterly.

### 4.2 Audit log

Every access to the POA vault (including viewing a document, updating status, or deleting a document) is logged:
- Timestamp
- Reviewer identity
- POA submission token (document reference — not patient name)
- Action taken (view / status update / delete)
- Reason (required for CEO break-glass access)

The audit log is write-once and cannot be modified or deleted except by the CEO with written justification. It is retained for 6 years minimum.

### 4.3 Prohibited actions

The following are explicitly prohibited:
- Copying a POA document to any system outside the vault
- Emailing a POA document
- Printing a POA document
- Creating a summary that includes the grantor's full name, date of birth, or address
- Searching the vault by grantor name or other personal identifier (search is by POA submission token only)

---

## 5. Review Procedure

### 5.1 Review timeline

POA Review Lead acknowledges receipt within 24 hours of upload and completes review within 3 business days.

### 5.2 Review scope

The reviewer assesses the following and records the outcome in the structured review record (see §5.3):

| Assessment item | Criteria |
|---|---|
| Document type | Is this an accepted POA type (§2)? |
| Validity | Is the document signed, witnessed/notarised as required by the indicated jurisdiction? Is it within date? |
| Scope | Does the document explicitly grant healthcare decision authority? Does it include research consent as a permitted scope (or is the scope broad enough to encompass research consent)? |
| Jurisdiction | Which jurisdiction's law governs this POA? Flag any jurisdiction where NeuroPulse's legal counsel has not confirmed the POA form's validity (see §5.4). |
| Capacity trigger | Does the document specify a triggering event (e.g. physician declaration of incapacity)? If so, has the triggering event occurred? (POA holder must provide confirmation if required.) |
| Identity verification | Does the POA holder's identity match the attorney-in-fact named in the document? (App account name / email used as proxy; not a full KYC check — flag if substantial discrepancy.) |
| Scope limitation | Are there explicit exclusions that would limit research consent authority (e.g. "no experimental procedures")? If yes, note the limitation. |

### 5.3 Structured review record

On completion, the reviewer creates a structured review record stored in a separate database (not in the vault) containing:

```
poa_submission_token:  [UUID — links to vault object]
review_date:           [YYYY-MM-DD]
reviewer_id:           [reviewer identifier — not stored with the document]
document_type:         [enum: DURABLE_HEALTHCARE_POA | HEALTHCARE_PROXY | ADVANCE_DIRECTIVE | LPA_PERSONAL_WELFARE | EU_EQUIVALENT]
jurisdiction:          [ISO 3166-2 state/country code]
validity_determination: [VALID | INVALID | REQUIRES_SUPPLEMENTAL_INFORMATION]
scope_includes_research: [true | false | unclear]
scope_limitations:     [text or null]
capacity_trigger_present: [true | false]
capacity_trigger_met:  [true | false | unknown — reviewer cannot determine]
identity_match:        [CONFIRMED | DISCREPANCY_FLAGGED | UNABLE_TO_VERIFY]
annual_reverification_due: [YYYY-MM-DD — 12 months from review_date]
notes:                 [text — no patient name, DOB, or address]
```

This record is the only persistent artefact of the POA review after the document is deleted (§6).

### 5.4 Jurisdiction flagging

Jurisdictions where NeuroPulse has confirmed POA validity criteria:
- All 50 US states: [to be populated as legal counsel confirms; initially populate with NCLC POA Act states]
- United Kingdom: personal welfare LPA registered with the Office of the Public Guardian
- EU member states: to be assessed by EU legal counsel before EU T1 launch

Jurisdictions not on the confirmed list are flagged for legal counsel review before the POA is accepted. Legal counsel's determination is added to the structured review record.

### 5.5 Rejection procedure

If the document is determined INVALID:
1. The reviewer updates the status to INVALID and records the reason in the notes field.
2. The app notifies the POA holder: "We were unable to verify your document. [Reason without disclosing protected information.] You may re-submit with a corrected document."
3. The original document is deleted immediately (not deferred to the 30-day window — no reason to retain an invalid document).

---

## 6. Retention and Deletion

### 6.1 Document retention

The original POA document is retained in the vault for a maximum of 30 days from the date the structured review record is created. On day 30, a deletion job automatically:
1. Issues a hard-delete instruction to the vault storage bucket for the document object.
2. Issues an erase command to eliminate the object from any bucket versioning or deletion tombstone (bucket versioning is disabled per §3.2, but this step is a double-check).
3. Records the deletion: timestamp, POA submission token, deletion method, in the audit log.
4. Updates the structured review record: `document_deleted: true`, `deletion_date: [date]`.

The structured review record (§5.3) — which contains no patient name, DOB, or address — is retained for 6 years to satisfy HIPAA record-keeping requirements (45 CFR §164.530(j)).

### 6.2 Early deletion

The document may be deleted before day 30 in these circumstances:
- POA holder requests cancellation of the review
- Document is rejected as INVALID (§5.5) — delete immediately
- Deletion requested by the grantor's legal representative (must be verified by the same process as §5.2 identity verification)

### 6.3 Annual re-verification

At 12 months from the review date (recorded in `annual_reverification_due`), the app notifies the POA holder:
> "Your authorisation on behalf of [patient token — not name] is due for annual re-verification. Please upload a current copy of your healthcare authorisation document."

The re-verification process follows §3 and §5 identically. On successful re-verification, the `annual_reverification_due` date is updated to 12 months from the new review date. The old structured review record is retained; a new record is created for the re-verification.

If re-verification is not completed within 30 days of the due date, the POA holder's authorisation is automatically suspended (the app does not permit further research consent or access grant decisions) until re-verification is complete.

---

## 7. Patient Capacity Restoration

If the patient regains capacity and wishes to revoke the POA holder's authorisation:

1. Patient contacts NeuroPulse via the in-app privacy contact mechanism.
2. NeuroPulse verifies that the patient is contacting from their own authenticated account (or via a new account with identity verification — process TBD with legal counsel).
3. POA holder's authorisation is immediately suspended.
4. Patient is presented with all research consent and clinical access decisions previously made by the POA holder and invited to ratify or revoke each.
5. Revocation of any consent or access decision follows the standard revocation procedure (NP-PRIV-REM-001 STEP-23 for clinical access; research consent withdrawal per NP-FW-ANON-001 for research data).
6. Structured review record updated: `capacity_restored: true`, `capacity_restoration_date: [date]`.

---

## 8. Incident Response

Any unauthorised access to the POA vault, or any suspected exfiltration of a POA document, is treated as a P1 incident under NP-SEC-BR-001. Given the nature of POA documents, the incident commander must engage legal counsel within 30 minutes regardless of the number of documents potentially affected.

---

*NP-PROC-POA-001 Rev A — CONFIDENTIAL — NeuroPulse Design Programme*
