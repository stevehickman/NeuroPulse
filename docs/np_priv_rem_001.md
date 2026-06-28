# Privacy Remediation Master Plan

**Project:** NeuroPulse  
**Document:** NP-PRIV-REM-001  
**Revision:** B  
**Date:** 2026-06-03  
**Status:** ACTIVE  
**Effective Date:** 2026-06-03  
**Author:** Quality Lead (interim: Steve Hickman, CEO)  
**Approved By:** Steve Hickman, CEO  
**References:** NP-SEC-BR-001, NP-PROC-POA-001, NP-APP-TELEMETRY-001 Rev B, NP-FW-EMMC-002  
**Related Issues:** —  
**Gate:** —  
**IEC 62304 Class:** —  
**Supersedes:** NP-PRIV-REM-001 Rev A  
**Change Summary:** Three new steps added (STEP-31, STEP-32, STEP-33) from NP-PRIV-001 Rev B delta findings: HIPAA Expert Determination certification pathway, adaptive stimulation right-to-explanation, and session_sequence coarsening. NP-APP-TELEMETRY-001 updated to Rev B (session_sequence → engagement_tier). NP-FW-EMMC-002 §G added. Capability matrix rows added for STEP-31 through STEP-33. Direct remediations section updated.  
**Parent Analysis:** NP-PRIV-001 Rev A + Rev B delta (Privacy Analysis and Repair — 2026-06-02 / 2026-06-03)

---

## 1. Purpose and Scope

This document translates every finding in NP-PRIV-001 Rev A into a tracked remediation programme. It serves four functions:

1. **Option selection** — where NP-PRIV-001 presented multiple remediation paths, this document selects the most thorough option and records the rationale.
2. **Master calendar** — all remediation steps are integrated into a single milestone-linked schedule, cross-referenced to the NeuroPulse design timeline in NP-DP-001.
3. **Capability matrix** — each step identifies the performer role, minimum authority level, whether the step can be automated, and whether an external party is required.
4. **Authority submission guide** — for every step that requires a submission to a regulatory body, certification authority, or app platform, this document specifies the authority, submission format, content requirements, and prerequisites.

Steps already completed by the authoring session (2026-06-02) are marked **DIRECT REMEDIATION COMPLETE** and cross-reference the document or code change produced. All others carry a status of OPEN with a target milestone.

---

## 2. Remediation Option Selections

The following table records the chosen option for every finding that presented alternatives.

| Finding | Options in NP-PRIV-001 | Selected option | Rationale |
|---|---|---|---|
| SHDR impact-event data | A (binary flag), B (UHDR + derived SHDR metric), C (on-device only, binary alert to SHDR) | **C** | Raw accelerometer series never leaves the device; the binary alert gives NeuroPulse the maintenance signal it needs. Cost: ~1 sprint firmware work. No hardware change. |
| UHDR key lifecycle — biometric revocation | Single-layer re-encrypt vs. two-layer key scheme (master key wrapper) | **Two-layer master key scheme** | Biometric change re-encrypts only the 32-byte wrapper, not 6.9 GB UHDR. Faster, safer, no data loss risk. |
| UHDR key lifecycle — device transfer | Soft reset vs. full factory reset (SANITIZE + SHDR wipe + warranty token clear + new TRNG salt) | **Full factory reset** | Soft reset leaves ciphertext on eMMC. SANITIZE removes all residual data per NIST SP 800-88 and satisfies GDPR Art. 17 and HIPAA §164.310(d)(1). |
| Scratch partition anonymisation | Encrypt alone vs. encrypt + atomic design vs. encrypt + atomic + post-run SANITIZE | **All three combined** | Defence in depth. Each layer addresses a different attack window. Marginal engineering cost is low given the atomic design must be implemented anyway. |
| EU transfer mechanism | DPF self-certification vs. SCCs vs. EU data residency | **DPF + EU data residency for T2 clinical data** | DPF covers all consumer SHDR and warranty flows (fastest path, lowest cost). EU data residency for T2 clinical cloud eliminates the transfer question for the most sensitive data class and is a competitive differentiator with EU clinical buyers. |
| Clinician access revocation | At next billing cycle vs. immediate API-level token invalidation | **Immediate invalidation** | GDPR Art. 7(3) requires withdrawal to be as easy and immediate as granting consent. Billing dispute is handled separately from access rights. |
| HealthKit data residency | Document existing Apple policy vs. add binding engineering constraint prohibiting transmission | **Binding spec constraint** | Policy documents can be overridden by future feature requests. A spec constraint in NP-APP-ROADMAP-001 requires a formal design change order and legal review before any HealthKit transmission is ever proposed. |
| Mode F consent | General session consent assumed to cover vs. separate explicit consent + default-off + ambient indicator | **Separate explicit consent + default-off + ambient indicator + IEC 62471 cumulative dose calculation** | Retinal NIR PBM during apparent non-session wear is the highest-sensitivity consent gap in the product. Default-off is Cavoukian Principle 2 applied. Ambient indicator satisfies IEC 62471 awareness requirement. |
| FHIR R4 scope | Ad-hoc field selection vs. formal ImplementationGuide | **Formal ImplementationGuide (NP-INT-FHIR-001)** | An IG is machine-validatable; ad-hoc selection drifts. IG creates a versioned, auditable specification that feeds 510(k) software documentation. |

---

## 3. Master Remediation Calendar

### Calendar conventions

- **Milestone references** are from NP-DP-001 (G1 = Month 6, G2 = Month 10, G3 = Month 14, T1 Launch = Month 18). Month 0 = May 2026 (company formation date 2026-05-13).
- Calendar dates use formation-relative months. Absolute dates: Month 1 = June 2026, Month 6 = November 2026, Month 10 = March 2027, Month 18 = November 2027.
- **TRIGGER** identifies what event causes the step to activate, distinct from the milestone target. A step may be triggered before its milestone if the trigger condition occurs earlier.
- **STATUS** values: OPEN · IN PROGRESS · COMPLETE · BLOCKED (with blocker named).
- **DIRECT** tag = completed by this authoring session on 2026-06-02.

---

### Phase 0 — Immediate (by 2026-06-16, two weeks from analysis)

These steps must be complete before any external engagement (beta users, warranty registration, researcher outreach, investor demos that include a live device).

---

#### STEP-01 — Firmware spec: warrant token architecture
**Document:** NP-FW-EMMC-002 Rev A  
**Finding:** CRITICAL-01 (Warranty Owner ID re-identifies SHDR)  
**Trigger:** Before warranty registration system is built or any warranty ID is issued.  
**Status:** DIRECT REMEDIATION COMPLETE → `docs/np_fw_emmc_002.md`  
**Deliverable:** Firmware spec section defining: (a) opaque 256-bit TRNG warranty token as the only device-to-person linkage in SHDR; (b) warranty database join prohibition in production; (c) SHDR boundary test procedure.  
**Performer:** Firmware architect + Privacy Lead (interim: CEO)  
**Authority required:** CEO sign-off on SHDR boundary test procedure.  
**Automation:** The no-join rule can be enforced by database-level permissions (warranty DB and SHDR fleet DB never in the same DB instance, no cross-DB foreign keys). Automated test: attempt a join query in CI; confirm it fails.  
**External party:** No.

---

#### STEP-02 — Firmware spec: device factory reset procedure
**Document:** NP-FW-EMMC-002 Rev A  
**Finding:** HIGH-03 (UHDR key lifecycle — device transfer)  
**Trigger:** Before device serial numbers are assigned or any device is shipped outside the company.  
**Status:** DIRECT REMEDIATION COMPLETE → `docs/np_fw_emmc_002.md`  
**Deliverable:** Spec for user-initiated factory reset: (a) eMMC SANITIZE on UHDR partition; (b) SHDR reset to fresh device-ID-only state, session counter zeroed; (c) warranty token cleared from Config partition; (d) new Argon2id salt generated via TRNG; (e) UHDR master key wrapper deleted; (f) app UI flow with explicit "this will erase all your data" confirmation requiring two-step acknowledgement.  
**Performer:** Firmware engineer + UX designer  
**Authority required:** UX copy requires legal review for GDPR Art. 17 compliance.  
**Automation:** Factory reset can be initiated via app; the hardware operations (SANITIZE, TRNG reseed) are fully automatable in firmware.  
**External party:** No.

---

#### STEP-03 — Firmware spec: two-layer UHDR key scheme
**Document:** NP-FW-EMMC-002 Rev A  
**Finding:** HIGH-03 (UHDR key lifecycle — biometric revocation)  
**Trigger:** Before any UHDR is written to production firmware; must precede eMMC firmware implementation.  
**Status:** DIRECT REMEDIATION COMPLETE → `docs/np_fw_emmc_002.md`  
**Deliverable:** Spec for two-layer key architecture: (a) UHDR master key (32-byte random, generated at first boot by TRNG); (b) master key stored in Config partition encrypted with Argon2id-derived key (biometric/PIN); (c) biometric change triggers master key re-wrap only — UHDR partition ciphertext unchanged; (d) PIN always available as fallback; (e) master key zeroed from SRAM immediately after use.  
**Performer:** Firmware architect + cryptography reviewer  
**Authority required:** Cryptography reviewer must be qualified (either internal senior engineer or external specialist engaged for review session).  
**Automation:** Key re-wrap on biometric change is app-triggered, fully automatable.  
**External party:** Optional: engage external cryptography reviewer for sign-off before production firmware is written ($3,000–8,000 engagement).

---

#### STEP-04 — Firmware spec: Scratch partition encryption for research anonymisation
**Document:** NP-FW-EMMC-002 Rev A  
**Finding:** MEDIUM-04 (Research Scratch partition anonymisation window)  
**Trigger:** Before research anonymisation code is written; must precede first study descriptor deployment.  
**Status:** DIRECT REMEDIATION COMPLETE → `docs/np_fw_emmc_002.md`  
**Deliverable:** Spec for: (a) one-time session key K_scratch = AES-256-CTR(TRNG_256, session_nonce) derived before each anonymisation task; (b) K_scratch stored in on-chip SRAM only, zeroed with memset_explicit on completion or reset; (c) atomic pipeline design — partial runs discard all output; (d) eMMC SANITIZE on Scratch blocks after verified extract is written.  
**Performer:** Firmware engineer  
**Authority required:** None beyond standard design review.  
**Automation:** Fully automatable. TRNG key generation and SANITIZE are hardware operations.  
**External party:** No.

---

#### STEP-05 — Firmware spec: EDF+ header policy
**Document:** NP-FW-EMMC-002 Rev A  
**Finding:** MEDIUM-05 (EDF+ patient header field handling)  
**Trigger:** Before EDF+ writer code is written; must precede any EEG session recording implementation.  
**Status:** DIRECT REMEDIATION COMPLETE → `docs/np_fw_emmc_002.md`  
**Deliverable:** NeuroPulse EDF+ header policy: patient code = 16-char opaque UHDR token; sex = 'X'; birthdate = 'X'; patient name = 'X'; start date preserved; hospital code = 'NeuroPulse'; technician = 'X'; equipment = 'NeuroPulse_v[FW_VER]'. Plus: header validation step in research anonymisation pipeline that fails-closed if identity fields contain non-'X' values.  
**Performer:** Firmware engineer  
**Authority required:** None.  
**Automation:** Header validation is automatable as a unit test: any EDF+ file generated by the firmware must pass header field assertions.  
**External party:** No.

---

#### STEP-06 — Mode F: default-off + separate consent + ambient indicator spec
**Document:** CLAUDE.md §13.5 (locked decision), NP-FW-EMMC-002 Rev A §F  
**Finding:** HIGH-04 (Mode F autonomous retinal PBM — consent capture undefined)  
**Trigger:** Before any app UI or firmware code references Mode F as an enabled feature.  
**Status:** DIRECT REMEDIATION COMPLETE → CLAUDE.md updated; `docs/np_fw_emmc_002.md §F`  
**Deliverable:** Locked decisions: (a) Mode F is default-off; (b) Mode F requires a separately named, separately consented feature toggle in app onboarding; (c) when Mode F is active, the right temple amber LED breathes a distinct triple-pulse pattern (3 short pulses, 2s pause, repeat) distinct from the normal in-session pulse; (d) IEC 62471 cumulative daily retinal dose is calculated across all Mode F wear time per calendar day, not just per session; (e) Mode F is not available in T1 unless RISK-03 regulatory opinion letter explicitly covers 808-830nm bilateral retinal PBM in Mode F operating conditions.  
**Performer:** Firmware engineer + UX designer + regulatory counsel  
**Authority required:** Regulatory counsel sign-off required before Mode F is enabled in any shipping firmware (same RISK-03 engagement).  
**Automation:** Default-off is a firmware constant. Ambient indicator is a firmware state machine. Both automatable.  
**External party:** Regulatory counsel (existing RISK-03 engagement — add Mode F as Q-13 to the scope expansion brief NP-REG-PBM1064-001).

---

#### STEP-07 — Breach Response Plan: NP-SEC-BR-001
**Document:** NP-SEC-BR-001 Rev A  
**Finding:** CRITICAL-02 (No documented breach detection or response plan)  
**Trigger:** Before any personal data (warranty registrant, SHDR fleet upload, clinical data) is held by NeuroPulse on any server.  
**Status:** DIRECT REMEDIATION COMPLETE → `docs/np_sec_br_001.md`  
**Deliverable:** Complete breach response plan covering: detection signals; severity tiers; escalation chain; containment procedures; regulatory notification requirements (HIPAA, FTC HBNR, GDPR Art. 33, US state laws); user notification templates; forensic log retention; annual rehearsal procedure.  
**Performer:** CEO (interim Quality Lead) + Legal counsel  
**Authority required:** CEO approval. Legal counsel must review notification templates and regulatory triggers for jurisdiction accuracy.  
**Automation:** Detection signals (anomalous SHDR upload volumes, unusual admin access) should be automated alerts in the fleet monitoring system. The response procedures are human-executed.  
**External party:** Legal counsel (can use existing outside regulatory counsel or engage separate privacy counsel).

---

#### STEP-08 — POA Upload Procedure: NP-PROC-POA-001
**Document:** NP-PROC-POA-001 Rev A  
**Finding:** HIGH-06 (POA document upload is an unspecified high-risk data flow)  
**Trigger:** Before the POA upload feature is included in any app build, even internal.  
**Status:** DIRECT REMEDIATION COMPLETE → `docs/np_proc_poa_001.md`  
**Deliverable:** Complete POA procedure covering: encrypted upload via signed URL; restricted-access vault (access list ≤3 named reviewers); audit log of every access; 30-day document deletion after review; structured review output schema; annual re-verification workflow; capacity restoration procedure.  
**Performer:** CEO + Legal counsel + IT/Infrastructure  
**Authority required:** CEO approval. Legal counsel must confirm the POA review scope and jurisdiction-flagging criteria.  
**Automation:** Signed URL generation, document deletion at day 30, and annual re-verification prompts are automatable.  
**External party:** Legal counsel for jurisdiction flagging criteria. Consider engaging a healthcare POA specialist for the review rubric.

---

#### STEP-09 — App Telemetry Policy: NP-APP-TELEMETRY-001
**Document:** NP-APP-TELEMETRY-001 Rev A  
**Finding:** LOW-01 (App telemetry and crash reporting scope unspecified)  
**Trigger:** Before any analytics or crash reporting SDK is added to the iOS or Android codebase.  
**Status:** DIRECT REMEDIATION COMPLETE → `docs/np_app_telemetry_001.md`  
**Deliverable:** App telemetry policy covering: approved analytics vendor slot (TBD — DPA required); approved crash reporter slot (TBD — redacted payload mode required); prohibited event properties; permitted event properties; SDK initialisation gate; crash reporter configuration; annual review cadence.  
**Performer:** iOS/Android engineering lead + Privacy Lead (interim: CEO)  
**Authority required:** CEO approval. Vendor selection requires legal review of DPA and BAA.  
**Automation:** SDK initialisation gate (post-consent-flow only) is enforced in the app build. The prohibition on health-inferrable event properties can be partially automated via a custom linting rule or an event schema registry that rejects prohibited property names.  
**External party:** Selected analytics vendor and crash reporter vendor (DPA/BAA required from each).

---

### Phase 1 — Month 1–3 (June–August 2026)

---

#### STEP-10 — SHDR impact-event data: Option C on-device processing
**Document:** NP-FW-EMMC-002 Rev A §G (COMPLETE — authored 2026-06-03); firmware/shdr/np_accel_shdr.h/.c (to author)  
**Finding:** HIGH-01 (SHDR impact-event data enables motor-health inference); NP-PRIV-001 Rev B MEDIUM-06 (§G spec now complete)  
**Trigger:** Before any SHDR schema is frozen for the fleet database. §G spec is now complete — unblocks implementation.  
**Target milestone:** G1 (Month 6)  
**Status:** OPEN — spec complete, implementation pending  

**Detailed implementation instructions:**

1. **Create `firmware/shdr/np_accel_shdr.h`** with the struct and function declarations from NP-FW-EMMC-002 §G.2 and §G.6.

2. **Create `firmware/shdr/np_accel_shdr.c`** implementing `np_accel_shdr_process_session_gap()`:
   - Read raw 3-axis accelerometer data from the LSM6DSO (or equivalent IMU on the final BOM)
   - Compute peak resultant g-force: `g_peak = sqrt(ax² + ay² + az²)`
   - Apply threshold: `drop_detected = (g_peak > NP_ACCEL_DROP_THRESHOLD_G)`
   - Update rolling drop counter (ring buffer of last 7 days of `drop_detected` events)
   - Apply maintenance threshold: `maintenance_alert = (rolling_drop_count >= NP_ACCEL_MAINT_THRESHOLD)`
   - Call `np_shdr_write_accel_record({drop_detected, maintenance_alert})`
   - Zero the raw accelerometer buffer with `memset_explicit` before returning

3. **Call site:** `np_accel_shdr_process_session_gap()` is called from `np_hub_session_log` task after every session close event and after every boot with no preceding session.

4. **SHDR fleet DB schema migration:** Before any SHDR data is written to the fleet DB, apply the schema migration adding `drop_detected BOOLEAN` and `maintenance_alert BOOLEAN` columns. Run CI schema test `ci/test_shdr_schema.py` (OI-EMMC2-07) against the migration before deploying. The test must pass — no prohibited column patterns.

5. **CAPA trigger:** This is a design change (data element reclassification). Initiate a change order per NP-QMS-DC-001. Document the reclassification rationale: "Raw accelerometer series reclassified from SHDR to NEVER-WRITTEN due to motor-health inference risk identified in NP-PRIV-001 Rev A."

**Performer:** Firmware engineer + data architect (DB schema migration)  
**Authority required:** CEO sign-off on design change order (NP-QMS-DC-001).  
**Automation:** Firmware module is fully automatable. DB schema migration is automatable via migration script. OI-EMMC2-07 CI test is automatable.  
**External party:** No.

---

#### STEP-11 — Research anonymisation: l-diversity + differential privacy spec
**Document:** NP-FW-ANON-001 Rev A (COMPLETE — authored 2026-06-03); firmware/anon/ (to implement)  
**Finding:** HIGH-02 (k-anonymity alone is insufficient)  
**Trigger:** Before the research anonymisation engine is implemented; must precede first study descriptor deployment.  
**Target milestone:** G1 (Month 6)  
**Status:** OPEN — spec complete (NP-FW-ANON-001 Rev A), implementation pending  

**Detailed implementation instructions:**

1. **Read NP-FW-ANON-001 Rev A** (`docs/np_fw_anon_001.md`) in full before beginning implementation. The spec is complete and covers all module interfaces, data structures, algorithms, and FAI requirements.

2. **Create firmware/anon/ directory** with four modules:
   - `np_anon_kgroup.h/c` — k-anonymity grouping and l-diversity check (§5 of spec)
   - `np_anon_dp.h/c` — Laplace noise generation from TRNG (§6 of spec)
   - `np_anon_session.h/c` — main pipeline orchestration (§2 of spec)
   - `np_anon_output.h/c` — output formatting, signing, queue management (§7 of spec)

3. **Engage external DP reviewer** before implementing §6 (Laplace mechanism). Required sign-off on:
   - ε=1.0 total privacy budget is sufficient for the intended use (EEG spectral features + HRV aggregates)
   - Δ values per element in NP-FW-ANON-001 §3.2 are correct (especially HRV RMSSD Δ=200ms)
   - The FAI-ANON-09 adversarial re-identification test methodology
   - Budget: $5,000–15,000. Firms: Privacy Analytics (IQVIA), Tumult Labs, or academic collaborator with DP publications.

4. **Open items to resolve before first firmware build:** OI-ANON-01 through OI-ANON-05 in NP-FW-ANON-001 §10. Most critical: OI-ANON-03 (output encryption scheme) and OI-ANON-04 (age_decile collection in app onboarding).

5. **FAI execution:** All FAI-ANON-01 through FAI-ANON-09 must pass before any study descriptor is deployed to a real device. FAI-ANON-09 (adversarial re-identification) requires the external DP reviewer.

**Performer:** Firmware engineer (DSP/signal processing background preferred) + external DP reviewer  
**Authority required:** External DP reviewer sign-off required. Quality Lead sign-off on full FAI report.  
**Automation:** FAI-ANON-01 through FAI-ANON-08 are fully automatable as CI tests. FAI-ANON-09 requires human reviewer.  
**External party:** External DP reviewer (OI-ANON-01). Also feeds into IRB protocol (STEP-20) and Expert Determination certification (STEP-31).

---

#### STEP-12 — EU-US Data Privacy Framework: self-certification
**Document:** NP-REG-DPF-001 Rev A (new regulatory document)  
**Finding:** LOW-03 (EU cross-border data transfer mechanism not specified)  
**Trigger:** Before any EU resident's personal data (warranty registration, SHDR fleet upload) is transmitted to or stored on US infrastructure.  
**Target milestone:** Month 3 (before any EU marketing or pre-order activity)  
**Status:** OPEN  

**Sequential pre-submission checklist (do in this order):**

1. **Update the NeuroPulse privacy policy** to include the following DPF-required elements (must be live at a publicly accessible URL before submitting the certification):
   - Explicit statement that NeuroPulse participates in the EU-US Data Privacy Framework
   - List of personal data types covered (warranty registrant contact info, SHDR device telemetry, T2 clinical data where applicable)
   - Purposes for which personal data is used
   - Contact information for privacy inquiries: [privacy@neuropulse.com]
   - Name of the independent dispute resolution provider (see step 4 below)
   - Statement that NeuroPulse is subject to the FTC's investigatory and enforcement powers
   - Link to the DPF list at dataprivacyframework.gov confirming NeuroPulse's participation

2. **Confirm EU privacy counsel review** of the privacy policy language. Recommended firm: EU-qualified privacy counsel (can be same firm used for GDPR Art. 13 notice requirements). Budget: $3,000–8,000.

3. **Complete the DPF self-assessment** or arrange a third-party assessment. A self-assessment is sufficient for most companies at NeuroPulse's stage. Document the assessment in NP-REG-DPF-001.

4. **Select an independent dispute resolution provider** from the DPF-approved list at dataprivacyframework.gov/s/idr-provider-list. Options: JAMS (most common for tech companies, ~$300/year), BBB National Programs, ICDR. Sign up and pay the annual fee.

5. **Submit the self-certification** at https://www.dataprivacyframework.gov:
   - Create an account under NeuroPulse Inc.'s legal name
   - Complete all required fields per §4.1 of this document
   - Upload the live privacy policy URL
   - Submit and await ITA confirmation (typically 2–5 business days)
   - Save the confirmation and DPF registration number in NP-REG-DPF-001

6. **Set a calendar reminder** for annual recertification — 335 days after submission date (gives 30-day buffer before the 1-year expiry).

7. **Author NP-REG-DPF-001** documenting: certification date, registration number, privacy policy URL, dispute resolution provider, self-assessment summary, annual recertification date.

**Performer:** CEO + EU privacy counsel  
**Authority required:** CEO must submit the certification (it is a legal attestation under FTC authority).  
**Automation:** Annual recertification alert is automatable. Privacy policy hosting is automatable. Substantive recertification requires CEO attestation.  
**External party:** EU privacy counsel. DPF-approved dispute resolution provider.

---

#### STEP-13 — Clinician BAA: consent revocation + deletion cascade obligations
**Document:** NP-LEGAL-BAA-001 Rev A (COMPLETE — template authored 2026-06-03); execution with each counterparty pending  
**Finding:** HIGH-05 (clinician consent revocation path); MEDIUM-07 (deletion cascade to third-party processors)  
**Trigger:** Before any clinician subscription is sold or any UHDR-derived data is shared with a clinician.  
**Target milestone:** Month 3 (template reviewed); first execution before first T2 clinical subscription  
**Status:** OPEN — template authored, legal review and counterparty execution pending  

**Detailed execution instructions:**

1. **Legal counsel review of template (NP-LEGAL-BAA-001 Rev A):** Healthcare legal counsel must review the template at `docs/np_legal_baa_001.md` before any execution. Key items for counsel to confirm:
   - §5.1 (consent revocation cascade, 30-day deletion obligation) is enforceable in the relevant jurisdiction
   - §5.4 (BIPA provision for Illinois users) is accurate and sufficient
   - The tier table in §4.2 correctly describes the minimum necessary data for each tier
   - The governing law (§8.5) should be the jurisdiction where NeuroPulse is incorporated

2. **Per-counterparty execution process:**
   - Complete Exhibit A (scope of services) for the specific counterparty before sending
   - Select the correct access tier in §4.2 and delete the other two tier rows
   - Send via e-signature platform (DocuSign or HelloSign). Both parties sign electronically.
   - Store the fully executed BAA in the NeuroPulse document management system under the counterparty name.
   - Create a calendar reminder for the annual review date.

3. **Consent revocation tracking system:** Before the first BAA is executed, implement a simple tracking system for consent revocations:
   - When a patient revokes clinical access in the app, an automated notification is sent to the clinician's registered email with the subject "NeuroPulse: Patient Consent Withdrawal Notification — Action Required Within 30 Days"
   - The notification references §5.1 of the BAA and requests written confirmation of deletion within 35 days
   - A tracking ticket is created in the NeuroPulse support system
   - On day 35, if no confirmation received, the CEO sends a follow-up

4. **Annual BAA review:** Each BAA should be reviewed annually to confirm it still covers the current scope of the relationship. Schedule reviews at the calendar reminder created in step 2.

**Performer:** Healthcare legal counsel (review); CEO (execution authority); Product/Engineering (consent revocation notification system)  
**Authority required:** CEO must execute each BAA. Healthcare legal counsel review required before first execution.  
**Automation:** BAA execution via e-signature (automatable). Consent revocation notification is automatable. Tracking ticket creation is automatable.  
**External party:** Healthcare legal counsel. Each clinician counterparty.

---

#### STEP-14 — FHIR ImplementationGuide: NP-INT-FHIR-001
**Document:** NP-INT-FHIR-001 Rev A (COMPLETE — authored 2026-06-03); IG package publication and CI integration pending  
**Finding:** MEDIUM-01 (FHIR R4 minimum population not bounded)  
**Trigger:** Before any T2 EHR integration code is written.  
**Target milestone:** G1 (Month 6) gate item NP-COORD-001 G3-09  
**Status:** OPEN — spec complete (NP-INT-FHIR-001 Rev A), IG package publication and CI integration pending  

**Detailed implementation instructions:**

1. **Review NP-INT-FHIR-001 Rev A** (`docs/np_int_fhir_001.md`) with the T2 clinical engineering team before any EHR integration code is written.

2. **Resolve open items** (NP-INT-FHIR-001 §10, OI-FHIR-01 through OI-FHIR-05) before the IG package is published:
   - OI-FHIR-01 (LOINC mapping): Engage a clinical informatics specialist to confirm the NP-EEG-* local codes are appropriate given no standard LOINC codes exist for quantitative EEG spectral features. Budget: $2,000–5,000 for a single clinical informatics review session.
   - OI-FHIR-05 (Procedure code): Confirm SNOMED CT `229070002` (Biofeedback therapy) is the best code, or identify a more precise code for TMS/tDCS.

3. **Build the IG package** using the HL7 FHIR IG Publisher (https://github.com/HL7/fhir-ig-publisher):
   - Author StructureDefinition resources for NP-Patient, NP-Observation, NP-DiagnosticReport, NP-Procedure
   - Run `java -jar publisher.jar -ig ig.ini` to generate the full IG package
   - The generated package is the authoritative machine-validatable specification

4. **Publish the IG package** at the canonical URL: `https://fhir.neuropulse.com/ig/NeuroPulse-T2-Clinical` (requires web infrastructure — add to T2 cloud architecture task)

5. **Add FHIR validator to the T2 backend CI pipeline:**
   - Download the HAPI FHIR Validator JAR
   - Add to CI: `java -jar validator.jar <resource.json> -ig <NP IG package URL> -version 4.0.1`
   - Any FHIR resource that fails validation must not be committed

6. **Review with first T2 EHR integration partner** — share the IG with the clinic's IT team before integration begins. The IG sets their expectations for what data they will receive and in what format.

**Performer:** Clinical informatics engineer (LOINC review); Backend engineer (IG package, CI integration); Privacy Lead (review)  
**Authority required:** Privacy Lead sign-off before IG is published.  
**Automation:** FHIR validator CI integration is fully automatable.  
**External party:** Clinical informatics specialist (OI-FHIR-01). EHR integration partner at each T2 clinical site.

---

### Phase 2 — Month 3–6 (August–November 2026, pre-G1)

---

#### STEP-15 — Scripting API specification: NP-API-001
**Document:** NP-API-001  
**Finding:** MEDIUM-02 (scripting API data access scope undefined)  
**Trigger:** Before T2 scripting API is designed or any API schema is committed.  
**Target milestone:** G1 (Month 6)  
**Status:** OPEN  
**Deliverable:** API specification covering: SHDR-only default scope; UHDR access gating (three-condition requirement: active consent grant, API access toggle, element within use-case); audit log schema; rate limits (1,000 patient-record requests/hour, 10,000/day, bulk export requires separate signed credential); 256-bit random key scheme with 90-day expiry; key management procedure; test suite for authorisation boundary enforcement.  
**Performer:** API engineer + Privacy Lead  
**Authority required:** Privacy Lead sign-off required before API schema is published externally.  
**Automation:** Rate limits, key expiry, and authorisation checks are fully automatable. Audit log writes are automatable. Boundary enforcement tests should be in CI.  
**External party:** No, but an independent API security audit is recommended before T2 launch (see STEP-26).

---

#### STEP-16 — Apple App Store privacy nutrition label preparation
**Document:** NP-APP-ROADMAP-001 (update); App Store Connect submission  
**Finding:** LOW-02 (Apple Watch sync app HealthKit data residency); LOW-01 (app telemetry scope)  
**Trigger:** Before any public TestFlight or App Store submission.  
**Target milestone:** Month 6 (first internal TestFlight target)  
**Status:** OPEN — requires NP-APP-TELEMETRY-001 (STEP-09) and HealthKit permission list to be finalised first.  
**Deliverable:** (a) App Privacy Nutrition Label completed accurately in App Store Connect: data types collected, linked/not linked, tracking/not tracking; (b) HealthKit entitlement justification string (required by Apple for any app using HealthKit); (c) NP-APP-ROADMAP-001 binding constraint on HealthKit data residency (no transmission to NeuroPulse servers); (d) Privacy Manifest file (PrivacyInfo.xcprivacy) completed per Apple requirements (required for all apps using privacy-impacting APIs as of Spring 2024). See §4.2 for full authority submission requirements.  
**Performer:** iOS engineering lead + Legal counsel  
**Authority required:** Legal counsel review of nutrition label accuracy. CEO sign-off on HealthKit binding constraint.  
**Automation:** Xcode build system validates PrivacyInfo.xcprivacy at compile time. App Store Connect pre-review checks validate nutrition label completeness.  
**External party:** Apple App Store Review (authority — see §4.2). No pre-submission engagement possible; Apple reviews at submission time.

---

#### STEP-17 — Google Play Data Safety section preparation
**Document:** Google Play Console submission  
**Finding:** LOW-01 (app telemetry scope) — Android parity  
**Trigger:** Before any Google Play internal test track or public submission.  
**Target milestone:** Month 6 (first internal test track target)  
**Status:** OPEN — requires NP-APP-TELEMETRY-001 (STEP-09) to be finalised first.  
**Deliverable:** Google Play Data Safety section completed: data types collected/shared, whether data is encrypted in transit, whether users can request deletion, security practices. See §4.3 for requirements.  
**Performer:** Android engineering lead + Legal counsel  
**Authority required:** Legal counsel review. CEO sign-off.  
**Automation:** Google Play Console validates completeness but not accuracy — human review required.  
**External party:** Google Play Review (authority — see §4.3).

---

#### STEP-18 — Mode F: add Q-13 to RISK-03 regulatory counsel scope
**Document:** NP-REG-PBM1064-001 Rev B (update to existing counsel engagement brief)  
**Finding:** HIGH-04 (Mode F retinal PBM consent and safety)  
**Trigger:** When RISK-03 regulatory counsel engagement is initiated or next time counsel is contacted.  
**Target milestone:** Month 3 (RISK-03 is already a pre-existing blocking item)  
**Status:** OPEN — dependent on RISK-03 counsel engagement initiation.  
**Deliverable:** Updated NP-REG-PBM1064-001 Rev B adding Q-13: "For Mode F (continuous 808–830nm bilateral retinal PBM during normal-looking wear, ≤[X] mW/cm², ≤[Y] min/day cumulative): (a) IEC 62471 group classification for this operating mode; (b) whether the cumulative daily dose calculation method differs from peak pulsed session calculation; (c) whether Mode F requires separate user consent disclosure distinct from session-based PBM consent; (d) FDA general wellness pathway applicability for continuous ambient retinal exposure."  
**Performer:** Regulatory counsel (external, existing RISK-03 engagement)  
**Authority required:** CEO instruction to counsel to expand scope.  
**Automation:** No.  
**External party:** Existing RISK-03 regulatory counsel.

---

### Phase 3 — Month 6–9 (November 2026–February 2027, post-G1 / pre-G2)

---

#### STEP-19 — EU data residency for T2 clinical cloud: architecture decision
**Document:** NP-ARCH-CLOUD-001 Rev A (new architecture document)  
**Finding:** LOW-03 (EU cross-border data transfer mechanism)  
**Trigger:** When T2 clinical cloud infrastructure vendor is selected.  
**Target milestone:** Month 9 (eQMS platform selection is already a Month 9 item — cloud architecture decision should be concurrent)  
**Status:** OPEN  
**Deliverable:** Architecture decision record NP-ARCH-CLOUD-001 specifying: (a) EU clinical data (T2 UHDR-derived clinical records, FHIR resources, multi-patient dashboard data) hosted in an EEA cloud region (AWS eu-west-1/3, GCP europe-west1, or Azure northeurope); (b) SHDR fleet data and warranty tokens may remain in US region with DPF coverage; (c) cross-region replication of clinical data is prohibited; (d) EU customer contracts reference EU data residency as a service commitment; (e) T2 SLA documentation reflects EEA-region hosting.  
**Performer:** Infrastructure architect + Legal counsel  
**Authority required:** CEO decision on cloud vendor selection. EU privacy counsel to validate that EEA hosting eliminates (rather than merely reduces) the Chapter V transfer obligation for the hosted data.  
**Automation:** Cloud region enforcement is automatable via IaC (Terraform region constraints, AWS SCPs, GCP org policies).  
**External party:** Cloud infrastructure vendor. EU privacy legal counsel.

---

#### STEP-20 — IRB protocol for research consent architecture validation
**Document:** NP-IRB-001 (new IRB protocol document)  
**Finding:** HIGH-02 (research anonymisation); §6.3 research suggestion portal  
**Trigger:** Before the first study descriptor is sent to any device; before any patient is invited to participate in a NeuroPulse-facilitated study (even observational).  
**Target milestone:** Month 9  
**Status:** OPEN  

**Detailed instructions — preferred path (Rashidi-Ranjbar at St. Michael's Hospital Toronto):**

The preferred IRB path is through a university collaborator who already has a funded research programme and an established IRB relationship, rather than a central commercial IRB. This avoids the "principal investigator affiliation" requirement at university IRBs and reduces cost.

**Step 1 — Initial outreach to Neda Rashidi-Ranjbar (neda.rashidi-ranjbar@unityhealth.to):**
- Email subject: "NeuroPulse — Research Collaboration Opportunity: EEG + HRV + PBM Platform Study"
- Content: Brief description of NeuroPulse (wearable multi-modal neuromodulation); reference her 2025 MCI PBM RCT; propose a pilot study using NeuroPulse as the platform; offer device loan + data access + co-authorship
- Attach: NP-CLIN-001 clinical evidence summary (2-page executive summary from `docs/neuropulse_clinical_trials_strategy.docx`)
- Goal of call: confirm interest; identify whether she can serve as PI or co-PI on a NeuroPulse IRB protocol; discuss her REB (Research Ethics Board — Canadian equivalent of IRB) process at Unity Health Toronto

**Step 2 — If Rashidi-Ranjbar agrees, prepare REB/IRB protocol:**
- Unity Health Toronto uses the Unity Health REB (not a central IRB)
- NP-FW-ANON-001 is the primary technical document to attach — it describes the anonymisation architecture in detail
- Protocol content (per §4.4 of this document): title, PI, institution, study design (observational, retrospective, prospective), subject selection (NeuroPulse T1 users who opt in to research), data sources (NeuroPulse on-device anonymised extracts), anonymisation methods (reference NP-FW-ANON-001), consent mechanism (in-app a priori consent per CLAUDE.md §6.2), data security (NP-FW-EMMC-001), risks and benefits, consent withdrawal procedure
- **Exemption determination application:** Given the on-device anonymisation (no identifiable data leaving the device), file for an exemption determination under US 45 CFR §46.104(d)(4) (secondary research with de-identified data) in addition to Canadian REB review. The same protocol serves both.

**Step 3 — Parallel path if Rashidi-Ranjbar is not available:**
- Contact Mayank Jog at UCLA (mjog@mednet.ucla.edu) — K99/R00 NIH grant aligned to HD-tDCS protocols, familiar with device research
- Alternative: submit to WCG IRB (https://www.wcgirb.com) or Advarra IRB (https://www.advarra.com) as a single-site sponsor-investigator study with CEO as PI
- WCG/Advarra cost: $3,000–8,000 for initial review; CEO obtains CITI human subjects training certification first (free at https://about.citiprogram.org, ~4 hours)

**Step 4 — OHRP Federalwide Assurance (FWA):**
- If NeuroPulse is the sponsor-investigator (not using a university), file an FWA with OHRP at https://ohrp.cit.nih.gov/efile
- Required for studies subject to US federal regulations
- Free to file; annual update required
- Timeline: 2–4 weeks for OHRP to process

**Performer:** CEO (initial researcher outreach, FWA); Clinical research coordinator (protocol drafting — hire or contract); Privacy Lead (anonymisation architecture sections)  
**Authority required:** PI must have current human subjects research training (CITI certification). CEO can serve as PI for sponsor-investigator studies.  
**Automation:** No.  
**External party:** University collaborator (preferred) or commercial IRB (WCG/Advarra). OHRP for FWA.

---

#### STEP-21 — Internal privacy audit: app source code review
**Document:** NP-PRIV-AUDIT-001 Rev A (new audit record)  
**Finding:** LOW-01 (app telemetry scope — app source code not reviewed)  
**Trigger:** When iOS and Android app source code reaches feature-complete state for a major version.  
**Target milestone:** Month 9 (before first external beta)  
**Status:** OPEN  
**Deliverable:** Privacy-focused code review of iOS and Android app covering: third-party SDK list (confirmed against NP-APP-TELEMETRY-001 approved list); analytics event names (confirmed against prohibited property list); storage usage (no PII in UserDefaults/localStorage/unencrypted storage); HealthKit permission usage (confirmed against approved list); network calls (all to known domains, none to undisclosed third parties). Findings documented as NP-PRIV-AUDIT-001 with PASS/FAIL per criterion.  
**Performer:** Privacy-qualified engineer (not the original code author — independence required for audit validity)  
**Authority required:** Quality Lead sign-off on audit report. CEO review of any FAIL findings.  
**Automation:** Some checks are automatable: SDK list can be generated from Xcode dependency graph; network calls can be captured in a test harness; HealthKit permission list can be extracted from Info.plist. Automated checks are a supplement to, not a replacement for, manual code review.  
**External party:** Optional: engage external mobile app privacy auditor for the first audit to establish a baseline and train internal reviewers ($10,000–25,000).

---

### Phase 4 — Month 9–14 (February–July 2027, pre-G3)

---

#### STEP-22 — SHDR boundary CAPA: accelerometer reclassification implementation verification
**Document:** NP-QMS-CAPA-001 (CAPA trigger) → NP-COORD-001 gate item  
**Finding:** HIGH-01 (SHDR impact-event data)  
**Trigger:** When STEP-10 firmware module np_accel_shdr is implemented.  
**Target milestone:** G2 (Month 10)  
**Status:** OPEN — dependent on STEP-10 implementation  
**Deliverable:** Verification test FAI-ACCEL-01: confirm (a) raw accelerometer data does not appear in SHDR uploads for 50 consecutive test sessions; (b) `drop_detected` and `maintenance_alert` boolean fields appear correctly; (c) fleet DB schema rejects any upload containing g-force or orientation fields. PASS required to close NP-COORD-001 gate item.  
**Performer:** QA engineer  
**Authority required:** Quality Lead sign-off on test report.  
**Automation:** FAI-ACCEL-01 is fully automatable as an integration test.  
**External party:** No.

---

#### STEP-23 — Clinician consent revocation: API implementation and verification
**Document:** NP-COORD-001 new gate item (T2 pre-launch)  
**Finding:** HIGH-05 (clinician consent revocation path)  
**Trigger:** When T2 clinical platform API is implemented.  
**Target milestone:** G3 (Month 14)  
**Status:** OPEN  
**Deliverable:** Verification test FAI-REVOKE-01: (a) patient revokes consent in app; (b) confirm clinician API token returns 401 within 5 seconds of revocation; (c) confirm SHDR revocation log entry created; (d) confirm multi-patient dashboard removes patient record immediately; (e) confirm app displays correct disclosure message about data in clinician's possession. All five criteria must PASS.  
**Performer:** QA engineer + Clinical platform engineer  
**Authority required:** Quality Lead sign-off.  
**Automation:** Criteria (a)–(d) are automatable as integration tests. Criterion (e) requires UI screenshot review.  
**External party:** No.

---

#### STEP-24 — Research anonymisation verification: l-diversity + DP test suite
**Document:** NP-FAI-ANON-001 (new FAI document)  
**Finding:** HIGH-02 (research anonymisation)  
**Trigger:** When NP-FW-ANON-001 anonymisation engine is implemented.  
**Target milestone:** G3 (Month 14) for software PASS; hardware/data bench can be concurrent with T2 prototype  
**Status:** OPEN — dependent on STEP-11 implementation  
**Deliverable:** FAI test suite covering: (a) l-diversity: generate 1,000 synthetic UHDR datasets with known sensitive attribute distributions; confirm anonymised extracts achieve l≥3 in all 1,000 cases; (b) differential privacy: apply statistical disclosure limitation test — confirm ε ≤ 1.0 per NIST SP 800-226; (c) adversarial re-identification attempt: attempt to re-identify a known synthetic individual using only the anonymised extract plus a simulated background-knowledge dataset; (d) raw EEG prohibition: confirm no raw EEG waveform appears in any extract. All criteria PASS required before first real study descriptor deployment.  
**Performer:** Privacy engineer + external DP reviewer  
**Authority required:** External DP reviewer sign-off on ε calibration. Quality Lead sign-off on full FAI report.  
**Automation:** Test suite items (a)–(d) are automatable as software tests. External reviewer validation is human.  
**External party:** External differential privacy specialist reviewer (same engagement as STEP-11).

---

#### STEP-25 — POA vault implementation and penetration test
**Document:** NP-PROC-POA-001 Rev A update; NP-SEC-PENTEST-001 (new)  
**Finding:** HIGH-06 (POA document upload)  
**Trigger:** When POA upload infrastructure is built (before the feature is available in any app build).  
**Target milestone:** G3 (Month 14)  
**Status:** OPEN — dependent on STEP-08 procedural specification  
**Deliverable:** (a) Infrastructure implementation per NP-PROC-POA-001 (signed URL upload, encrypted vault, access list ≤3 reviewers, audit log, 30-day deletion job); (b) penetration test of the upload endpoint and vault storage — minimum test scope: unauthenticated access attempts, signed URL replay attacks, vault access control bypass, document deletion verification; (c) penetration test findings documented in NP-SEC-PENTEST-001; (d) all Critical and High pentest findings remediated before POA feature ships.  
**Performer:** IT/Infrastructure engineer + external penetration tester  
**Authority required:** CEO sign-off required before POA feature is enabled in production.  
**Automation:** Deletion job (30 days after review completion) is automatable. Audit log writes are automatable. Penetration test is human-conducted.  
**External party:** Penetration testing firm with healthcare data experience ($8,000–20,000 engagement).

---

### Phase 5 — Month 14–18 (July–November 2027, T1 launch)

---

#### STEP-26 — T2 scripting API independent security audit
**Document:** NP-SEC-PENTEST-002  
**Finding:** MEDIUM-02 (scripting API scope)  
**Trigger:** When T2 scripting API reaches feature-complete state, before any clinical customer is given API access.  
**Target milestone:** T1 Launch (Month 18) for audit initiation; T2 pre-launch for remediation completion  
**Status:** OPEN  
**Deliverable:** Independent API security audit covering: OWASP API Security Top 10; object-level authorisation (every patient-record endpoint); bulk enumeration protection; rate limit effectiveness; key management; audit log completeness. Findings documented in NP-SEC-PENTEST-002. All Critical and High findings remediated before any clinical API key is issued.  
**Performer:** External API security auditor  
**Authority required:** CEO sign-off on audit scope and remediation plan.  
**Automation:** OWASP API Security scanner can automate initial discovery; manual review required for authorisation logic.  
**External party:** API security firm with healthcare API experience ($15,000–35,000 engagement).

---

#### STEP-27 — Annual breach response tabletop exercise
**Document:** NP-SEC-BR-001 §9 (tabletop exercise record)  
**Finding:** CRITICAL-02 (breach response plan)  
**Trigger:** Annual — first exercise must be completed before T1 launch. Subsequent exercises: within 12 months of the previous.  
**Target milestone:** Month 18 (T1 launch) for first exercise; annually thereafter  
**Status:** OPEN  
**Deliverable:** 60-minute tabletop exercise: present a simulated breach scenario (e.g. SHDR fleet database unauthorised access); walk through NP-SEC-BR-001 escalation chain, notification triggers, template completion; identify gaps; update NP-SEC-BR-001 if gaps found. Exercise record (date, attendees, scenario, gaps identified, updates made) added to DHF.  
**Performer:** CEO + legal counsel + all roles named in NP-SEC-BR-001 escalation chain  
**Authority required:** CEO convenes. Legal counsel validates notification trigger decisions.  
**Automation:** No — the exercise must be conducted as a live discussion.  
**External party:** Optional: engage an incident response specialist to facilitate the exercise ($3,000–8,000). Recommended for the first exercise.

---

### Phase 6 — Post-T1 / T2 Pre-Launch (Month 18+)

---

#### STEP-28 — EU T2 data residency: IaC implementation and certification
**Document:** NP-ARCH-CLOUD-001 Rev B  
**Finding:** LOW-03 (EU transfer mechanism)  
**Trigger:** When T2 clinical cloud infrastructure is provisioned.  
**Target milestone:** T2 pre-launch  
**Status:** OPEN — dependent on STEP-19 architecture decision  
**Deliverable:** (a) IaC configuration (Terraform or equivalent) with EEA region constraints enforced for all T2 clinical data resources; (b) test confirming no clinical data writes to non-EEA regions; (c) EU customer service agreement updated to reflect EEA data residency commitment; (d) NP-ARCH-CLOUD-001 Rev B updated with implemented configuration.  
**Performer:** Infrastructure engineer  
**Authority required:** CEO sign-off on service agreement updates.  
**Automation:** Fully automatable via IaC. Cross-region writes can be blocked at the cloud IAM/policy level.  
**External party:** Cloud infrastructure vendor. EU privacy counsel to confirm EEA residency satisfies the Chapter V obligation.

---

#### STEP-29 — Annual DPF recertification
**Document:** NP-REG-DPF-001 (annual update)  
**Finding:** LOW-03 (EU transfer mechanism)  
**Trigger:** Annual — within 30 days before the DPF certification anniversary.  
**Target milestone:** Recurring annually from Month 3 certification  
**Status:** OPEN — dependent on STEP-12  
**Deliverable:** (a) Self-assessment confirming NeuroPulse's practices remain consistent with DPF principles; (b) privacy policy reviewed and updated if required; (c) recertification submitted via dataprivacyframework.gov; (d) NP-REG-DPF-001 updated with new certification date and any practice changes.  
**Performer:** CEO + Legal counsel  
**Authority required:** CEO certification submission.  
**Automation:** Certification expiry alert is automatable. Substantive self-assessment is human.  
**External party:** Legal counsel for any practice change review.

---

#### STEP-30 — Annual privacy programme review
**Document:** NP-PRIV-REVIEW-001 (new annual review record)  
**Finding:** All findings — ongoing programme maintenance  
**Trigger:** Annual — first review 12 months after NP-PRIV-001 Rev A (June 2027). Subsequent reviews: within 12 months of the previous.  
**Target milestone:** Recurring annually  
**Status:** OPEN  
**Deliverable:** Review of: (a) all OPEN items in this calendar (confirm still applicable or close with justification); (b) new privacy risks from product changes, new modalities, or new data flows in the previous 12 months; (c) regulatory changes (new US state privacy laws, GDPR guidance updates, FDA digital health guidance); (d) threat model update (new attack vectors, new adversary capabilities); (e) findings documented in NP-PRIV-REVIEW-001; (f) this document updated to Rev B with any new steps.  
**Performer:** Privacy Lead (by Month 6, a dedicated role should exist; interim: CEO) + Legal counsel  
**Authority required:** Quality Lead sign-off. CEO review of any new Critical or High findings.  
**Automation:** Regulatory change monitoring can be automated (RSS feeds for FR, EDPB, FTC; services like Radar/OneTrust regulatory intelligence). Human judgment required for applicability assessment.  
**External party:** Optional: annual privacy counsel engagement for regulatory change assessment.

---

### Phase 6 (continued) — Post-T1 / T2 Pre-Launch, New STEPS from Rev B

---

#### STEP-31 — Designate standing HIPAA Expert Determination certifier
**Document:** NP-ANON-CERT template (new document type); engagement agreement with certifier  
**Finding:** NP-PRIV-001 Rev B MEDIUM-04 (HIPAA Expert Determination certification gap)  
**Trigger:** Before NP-FW-ANON-001 (research anonymisation engine spec) is finalised; before any study descriptor is authored. Must precede STEP-32.  
**Target milestone:** Month 9 (concurrent with NP-IRB-001 and NP-FW-ANON-001)  
**Status:** OPEN  
**Deliverable:** (a) Named certifier (individual or specialist firm) under a standing engagement agreement; (b) NP-ANON-CERT document template finalised (see NP-PRIV-001 Rev B for required content: expert qualifications, methods, data elements reviewed, risk assessment, signed certification); (c) NP-ANON-CERT added to DHF planned document types; (d) STEP-32 procedure incorporated into the study deployment gate checklist.  
**Performer:** CEO + clinical research coordinator  
**Authority required:** CEO sign-off on certifier engagement. Quality Lead confirms NP-ANON-CERT template meets HHS Guidance on De-identification (2012) requirements.  
**Automation:** No — certification is a human expert judgment per 45 CFR §164.514(b)(1).  
**External party:** Yes — qualified biostatistician or specialist firm (WCG, Advarra, Privacy Analytics/IQVIA, or university collaborator biostatistician). Budget: $1,500–5,000 per study certification.  
**Reference:** 45 CFR §164.514(b)(1); HHS Guidance Regarding Methods for De-identification of Protected Health Information (2012)

---

#### STEP-32 — Obtain NP-ANON-CERT for each study before descriptor deployment
**Document:** NP-ANON-CERT-[study_id] Rev A (one per study)  
**Finding:** NP-PRIV-001 Rev B MEDIUM-04 (HIPAA Expert Determination certification gap)  
**Trigger:** For each individual study, before the study descriptor is cryptographically signed and deployed to any device.  
**Target milestone:** Recurring — before each study launch, from Month 9 onwards  
**Status:** OPEN — dependent on STEP-31 (certifier engaged)  
**Deliverable:** Completed and signed NP-ANON-CERT-[study_id] Rev A on file in DHF before any study descriptor is signed. Content: expert qualifications; specific DP parameters (ε, δ) applied to this study's data elements; l-diversity (l) and k-anonymity (k) values; per-element re-identification risk assessment; expert's signed certification that re-identification risk is very small per 45 CFR §164.514(b)(1).  
**Performer:** Expert certifier (engaged in STEP-31)  
**Authority required:** Quality Lead confirms certification is on file before authorising the study descriptor signing key to be used. Without NP-ANON-CERT on file, the study descriptor must not be signed.  
**Automation:** No. Gate enforcement can be partially automated: deployment pipeline can check for presence of a file matching `NP-ANON-CERT-[study_id]-signed.pdf` in the DHF vault before permitting the descriptor signing step.  
**External party:** Yes — the certifier from STEP-31.

---

#### STEP-33 — Adaptive stimulation transparency: firmware log + app UI + privacy notice
**Document:** firmware/hub_control/include/np_adaptation_log.h (existing header); NP-APP-ROADMAP-001; NP-API-001 (T2 clinical report); app privacy notice  
**Finding:** NP-PRIV-001 Rev B MEDIUM-05 (right to explanation for adaptive stimulation)  
**Trigger:** Before NP-FW-HUB-001 session runner is implemented; before the iOS/Android session-results screen UI is designed.  
**Target milestone:** G2 (Month 10) for firmware struct; G3 (Month 14) for T2 clinical report in NP-API-001  
**Status:** OPEN  
**Deliverable:**  
(a) `np_adaptation_event_t` struct and `np_adapt_trigger_t` enum added to hub_control firmware (see NP-PRIV-001 Rev B for full C struct definition) — logged to UHDR for every adaptive event during a session;  
(b) Session History "Adaptive Adjustments" card added to iOS/Android app UI spec (NP-APP-ROADMAP-001) — displays plain-language adaptive events from the trigger enum; maximum 5 shown; "view all" for longer sessions;  
(c) Plain-language trigger enum mapping (firmware code → user-facing copy) maintained in the app codebase and extended whenever new adaptive triggers are added;  
(d) T2 clinical adaptation report schema added to NP-API-001 — JSON format with session_offset_ms, trigger, feature_percentile, param_id, value_before, value_after, confidence_pct; accessible to Full Clinical and Assess tier clinicians;  
(e) GDPR Art. 13(2)(f) disclosure added to app privacy notice under "Automated processing" — plain-language description of adaptive algorithm logic and user's right to view session adjustment summaries.  
**Performer:** Firmware engineer (struct + enum); iOS/Android engineer (UI card + trigger mapping); Privacy Lead (GDPR Art. 13(2)(f) copy); API engineer (T2 clinical report schema)  
**Authority required:** Privacy Lead sign-off on consumer-facing plain-language trigger copy before any build ships with the Adaptive Adjustments card.  
**Automation:** Firmware logging is fully automatable (struct write per adaptive event). UI card rendering is deterministic from the enum. The trigger enum must be reviewed and extended by a human whenever new adaptive triggers are added to firmware — add this as a mandatory checklist item in the firmware change control procedure (NP-QMS-DC-001).  
**External party:** No.

---

### Phase 0 (continued) — Immediate, from Rev B findings

---

#### STEP-34 — BIPA compliance programme (Illinois biometric data)
**Document:** NP-REG-BIPA-001 Rev A (new); NP-APP-ROADMAP-001 §9.3 (BIPA release screen)  
**Finding:** NP-PRIV-001 Rev B HIGH-01 (EEG data is biometric under BIPA)  
**Trigger:** Before any Illinois resident activates a NeuroPulse device. This is a pre-commercial-launch blocker — not a post-launch item.  
**Target milestone:** Month 2 (legal opinion); Month 6 (app consent flow implementation)  
**Status:** OPEN  
**Deliverable:**

**Step 1 — Engage BIPA-specialised counsel (Week 1–2):**
- Identify and engage outside counsel with Illinois BIPA experience. Recommended firms: Seyfarth Shaw (Chicago BIPA practice), Littler Mendelson, Orrick BIPA team. Budget: $15,000–25,000 for opinion + consent review.
- Provide counsel with: CLAUDE.md §3 EEG modality spec; NP-FW-EMMC-001 UHDR architecture; description of data flows (EEG collected on device, encrypted on-device, NeuroPulse cannot decrypt).
- Request a written legal opinion addressing: (a) whether NeuroPulse "collects" or "possesses" biometric information under BIPA 740 ILCS 14/10; (b) whether the UHDR encryption architecture (NeuroPulse cannot decrypt) provides a "possession" defence; (c) required elements of the written release; (d) required destruction policy; (e) whether T2 clinical operations change the analysis (HIPAA vs. BIPA overlap).

**Step 2 — Publish biometric retention and destruction policy on website (before first Illinois device):**
- Content required (BIPA 740 ILCS 14/15(a)): "NeuroPulse collects brainwave (EEG) biometric data during sessions. This data is stored only on your device, encrypted under a key that NeuroPulse does not hold. NeuroPulse retains EEG biometric data until: (1) you delete your data in the app; (2) you perform a factory reset; or (3) you request account deletion. Upon any of these events, EEG data is permanently erased from the device using hardware-level secure erasure (eMMC SANITIZE)."
- Location: neuropulse.com/privacy/biometric — publicly accessible, no login required.
- Add this URL to the BIPA consent release screen (§9.3 of NP-APP-ROADMAP-001).

**Step 3 — Implement BIPA written release in app consent flow (NP-APP-ROADMAP-001 §9.3):**
- Illinois detection: IP geolocation + user-stated location in device settings. Apply BIPA screen if either signal indicates Illinois.
- Screen content per NP-APP-ROADMAP-001 §9.3 — reviewed and approved by BIPA counsel (OI-PA-03).
- "Yes, I consent" / "No, decline" — decline disables EEG modality; device otherwise fully functional.

**Step 4 — Author NP-REG-BIPA-001:**
- Document the legal opinion conclusions, the consent screen implementation, and the website policy.
- File in DHF. Reviewed annually and updated when BIPA case law develops (BIPA is actively litigated — new cases monthly).

**Performer:** CEO + BIPA-specialised outside counsel + iOS/Android engineering (consent screen)  
**Authority required:** CEO executes counsel engagement. Legal counsel reviews and approves consent screen copy.  
**Automation:** IP geolocation detection for Illinois trigger is automatable. Consent capture and storage is automatable.  
**External party:** Yes — BIPA-specialised counsel (required before any Illinois device activation).

---

#### STEP-35 — Washington My Health My Data Act (MHMD) compliance
**Document:** NP-REG-MHMD-001 Rev A (new)  
**Finding:** NP-PRIV-001 Rev B HIGH-02 (Washington MHMD applies to SHDR behavioral patterns)  
**Trigger:** Before any Washington state resident activates a NeuroPulse device.  
**Target milestone:** Month 2 (legal analysis); Month 4 (SHDR consent redesign for WA)  
**Status:** OPEN  
**Deliverable:**

**Step 1 — Washington privacy counsel analysis (Week 1–2):**
- Engage Washington-qualified privacy counsel. The MHMD is enforced by the Washington AG and has a private right of action — it is more aggressive than HIPAA in this respect.
- Brief counsel on: SHDR contents (consumable session counts, device session count, NTC profiles, LED output ratios); warranty token architecture; how the device is sold in WA (consumer direct or through a clinic).
- Request analysis of: (a) whether SHDR consumable session counts constitute "consumer health data" under RCW 70.372.010(2) (behavioral data that could identify health-seeking); (b) whether the warranty token + SHDR upload constitutes "collection" under MHMD; (c) what the standalone authorization requirement looks like in practice; (d) whether suppressing consumable session counts from WA users' SHDR solves the problem.

**Step 2 — SHDR redesign decision (two options):**
- *Option A (recommended):* Suppress consumable session counts and device session counts from SHDR uploads for WA users. NeuroPulse retains the device-condition metrics (NTC temperatures, LED output ratios, impact flags, firmware version) which are not behavioral. This eliminates the MHMD trigger without requiring a standalone authorization.
- *Option B:* Implement standalone MHMD authorization screen for WA users — separate from general consent, describes specific SHDR elements, acknowledges health-data status. More legally robust but requires ongoing consent management.
- Document the choice and rationale in NP-REG-MHMD-001. Apply the decision to the SHDR upload firmware configuration.

**Step 3 — Add MHMD to NP-SEC-BR-001 breach notification decision tree:**
- Washington MHMD has a 30-day breach notification requirement for breaches of consumer health data.
- Add a MHMD row to §6.5 (US state breach laws table): "Washington MHMD: 30 days, WA AG portal at atg.wa.gov, private right of action by individuals."

**Step 4 — Author NP-REG-MHMD-001:**
- Document the legal analysis conclusions, the SHDR redesign decision, and any ongoing monitoring obligations.

**Performer:** CEO + Washington privacy counsel + firmware engineer (SHDR suppression for WA, if Option A chosen)  
**Authority required:** CEO decision on Option A vs. B. Legal counsel approval of either path.  
**Automation:** WA user flag (IP geolocation + device locale) is automatable. SHDR field suppression per user flag is automatable in firmware.  
**External party:** Yes — Washington privacy counsel.

---

#### STEP-36 — Minimum age gate and minor patient pathway
**Document:** NP-APP-ROADMAP-001 §9.2 (binding constraint); NP-PROC-MINOR-001 Rev A (new — T2 minor patient guardian consent procedure)  
**Finding:** NP-PRIV-001 Rev B MEDIUM-03 (no minimum age or children's privacy mechanism)  
**Trigger:** Before any app consent flow is designed or implemented.  
**Target milestone:** Month 3 (age gate in app); Month 9 (T2 minor patient pathway for clinical launch)  
**Status:** OPEN  
**Deliverable:**

**Step 1 — Age gate in app consent flow (Month 3):**
- Add the minimum age declaration checkbox as the first screen of the consent flow, per NP-APP-ROADMAP-001 §9.2.
- Text: "I confirm I am 16 years of age or older." (Not pre-ticked. Required to proceed.)
- Legal counsel confirms 16 is correct (OI-PA-01). Expected outcome: yes — covers COPPA (13), most EU GDPR member states (16), BIPA (adults-only for written release), UK Children's Code (18 for some data types, but NeuroPulse's use is therapeutic not commercial).
- The age declaration is stored in UHDR as a consent record: `age_declaration_accepted: bool` + `age_declaration_date: week_ordinal` (no precise timestamp).

**Step 2 — Privacy notice update:**
- Add to the app privacy notice: "NeuroPulse is intended for use by individuals aged 16 and older. If you are under 16, please do not use this app."
- For EU GDPR: mention that parental or guardian consent is required for users under the applicable member state age threshold if they use the app.

**Step 3 — T2 minor patient pathway (Month 9, before T2 clinical launch):**
- Define and implement an "Authorised Guardian" consent pathway for T2 clinical environments where minor patients may be treated.
- The guardian consent pathway requires: (a) clinic staff identifies the patient as a minor in the clinical dashboard; (b) a guardian consent screen is presented to the guardian (not the patient); (c) guardian provides their own authenticated identity; (d) guardian consent is stored alongside the clinical access consent grant.
- Author NP-PROC-MINOR-001 governing the guardian consent workflow, record-keeping, and capacity-restoration when the minor reaches majority.
- Brief T2 clinical pilot sites on the minor patient workflow before any minor patient is enrolled.

**Performer:** iOS/Android engineering (age gate); Legal counsel (age threshold confirmation); Clinical operations (T2 minor patient pathway)  
**Authority required:** Legal counsel confirmation of age threshold. CEO approval of minor patient pathway before T2 clinical launch.  
**Automation:** Age gate checkbox is automatable. Declaration storage in UHDR is automatable.  
**External party:** Legal counsel for age threshold. T2 clinical site staff for minor patient pathway.

---

## 4. Authority Submission Guide

### 4.1 EU-US Data Privacy Framework (DPF) Self-Certification

**Authority:** US Department of Commerce, International Trade Administration (ITA)  
**Purpose:** Valid legal mechanism for transfers of personal data from the EU/EEA to the US under GDPR Art. 46.  
**Reference:** 15 CFR Part 7037; EU Commission Adequacy Decision C(2023) 4745 (July 10, 2023)  
**Online portal:** https://www.dataprivacyframework.gov  

**Prerequisites (must be complete before submission):**

| # | Prerequisite | Reference | Responsible |
|---|---|---|---|
| P1 | Privacy policy updated to: (a) reference DPF participation; (b) list types of personal data covered; (c) state the purpose for which data is collected; (d) describe how to contact NeuroPulse with privacy inquiries; (e) list the DPF independent dispute resolution provider; (f) state that NeuroPulse is subject to the FTC's investigatory and enforcement powers | 15 CFR §7037.2; DPF Principles §II | Legal counsel |
| P2 | Independent recourse mechanism selected and contracted | DPF Principles §III.11; DPF Annex I | CEO + Legal counsel |
| P3 | Internal compliance mechanism established (human resources data policy, if employees' EU personal data is processed) | DPF HR Data Principles | CEO |
| P4 | List of US third parties to whom NeuroPulse transfers EU personal data, confirmed as having privacy obligations at least equivalent to DPF | DPF Principles §III.9 | Privacy Lead |
| P5 | Verification method selected: self-assessment or third-party assessment | DPF Principles §III.7 | CEO |

**Submission format and content:**
Online form at dataprivacyframework.gov/s/join-the-framework. Required fields:
- Legal name and address of organization
- Description of personal data covered
- Description of purposes of processing
- Independent recourse mechanism (name and contact)
- Federal body with jurisdiction (FTC for most commercial entities)
- Privacy policy URL (must be publicly accessible)
- Verification method
- Date the organization's privacy practices became consistent with DPF Principles

**Post-certification obligations:**
- Annual recertification (STEP-29)
- Update certification within 3 months of any material change to privacy practices
- Respond to complaints within 45 days
- Cooperate with EU supervisory authorities in resolving complaints

**Cost:** No filing fee. ITA may conduct compliance reviews.

---

### 4.2 Apple App Store — Privacy Nutrition Label and HealthKit Entitlement

**Authority:** Apple Inc., App Store Review Guidelines  
**Reference:** App Store Review Guidelines §5.1; Apple Privacy Nutrition Label documentation; Apple HealthKit documentation  
**Submission portal:** App Store Connect (https://appstoreconnect.apple.com)  

**Prerequisites:**

| # | Prerequisite | Reference | Responsible |
|---|---|---|---|
| P1 | NP-APP-TELEMETRY-001 completed and approved | STEP-09 | Privacy Lead |
| P2 | HealthKit permission list finalised (approved in NP-APP-ROADMAP-001) | STEP-16 | iOS engineering lead |
| P3 | PrivacyInfo.xcprivacy manifest completed (required for all apps using privacy-impacting APIs since Spring 2024) | Apple Privacy Manifest documentation | iOS engineering lead |
| P4 | All third-party SDKs using privacy-impacting APIs have privacy manifests included in the app bundle | Apple Third-Party SDK Privacy Manifest requirement | iOS engineering lead |
| P5 | NSHealthShareUsageDescription string in Info.plist accurately describes the HealthKit data usage | Apple HealthKit entitlement requirements | iOS engineering lead |

**Submission format:**
- Privacy Nutrition Label: completed via App Store Connect web UI during app submission. Fields: data types used by the app and by third-party SDKs, whether each data type is linked to identity, whether used for tracking.
- HealthKit entitlement: declared in Entitlements file; usage string in Info.plist; HealthKit capability enabled in App ID configuration.
- Privacy Manifest: PrivacyInfo.xcprivacy file in the app bundle (Xcode validates at build time).

**Content requirements:**
- For each data type: accurately represent whether it is collected, whether it is linked to the user's identity, whether it is used for tracking.
- HealthKit usage string must be specific: "NeuroPulse uses heart rate variability data to display your HRV coherence score during sessions. This data is not transmitted to NeuroPulse or any third party." Vague strings ("for health features") are rejected.

**Review timeline:** App Store Review typically 1–3 business days. Privacy-sensitive categories (HealthKit, health/fitness apps) may trigger extended review. No pre-submission review available from Apple.

---

### 4.3 Google Play — Data Safety Section

**Authority:** Google LLC, Google Play Developer Policy  
**Reference:** Google Play Developer Policy Center — Data Safety section; Google Play Data Safety help page  
**Submission portal:** Google Play Console (https://play.google.com/console)  

**Prerequisites:**

| # | Prerequisite | Reference | Responsible |
|---|---|---|---|
| P1 | NP-APP-TELEMETRY-001 completed and approved | STEP-09 | Privacy Lead |
| P2 | Complete data flow mapping: all data types collected, all data types shared, all third-party recipients | Google Play Data Safety form requirements | Android engineering lead |
| P3 | Deletion request mechanism implemented in the app (Google Play requires apps to provide an in-app mechanism for users to request deletion of their account and data) | Google Play Policy — Account deletion | Android engineering lead |

**Submission format:** Form in Google Play Console with section: Data collected (by type), Data shared (by type and recipient), Security practices (data encrypted in transit, security review). Must be kept current — updates require re-submission.

**Review:** Google Play automated review for completeness; human review may be triggered for health/fitness apps or sensitive data categories.

---

### 4.4 Institutional Review Board (IRB) Protocol Submission

**Authority:** Accredited IRB (recommended: WCG IRB, Advarra, or university IRB of first research collaborator)  
**Reference:** 45 CFR Part 46 (Common Rule); 21 CFR Part 56 (FDA IRB regulations, applicable if any study involves an investigational device); OHRP IRB registration requirements  
**Governing law:** Federal Policy for the Protection of Human Subjects (Common Rule), revised 2018  

**Prerequisites:**

| # | Prerequisite | Reference | Responsible |
|---|---|---|---|
| P1 | NP-FW-ANON-001 anonymisation engine specification complete | STEP-11 | Firmware architect |
| P2 | Study protocol drafted describing: data sources, anonymisation method, researcher access controls, consent withdrawal mechanism | 45 CFR §46.111 | Clinical research coordinator |
| P3 | Principal Investigator identified and qualified (human subjects research training certification required — CITI Program or equivalent) | 45 CFR §46.103 | PI (CEO or collaborating researcher) |
| P4 | Conflict of interest declaration completed by all key personnel | 21 CFR §54; institution-specific policy | CEO + all named personnel |
| P5 | NeuroPulse OHRP registration (required if NeuroPulse is itself the engagement institution) — file FWA (Federalwide Assurance) via OHRP | 45 CFR §46.103(a) | CEO |

**Exemption determination:** The NeuroPulse research architecture — on-device anonymisation, k≥10 + l≥3 + DP, no identifiable data leaving the device — may qualify for IRB exemption under 45 CFR §46.104(d)(4) (secondary research with deidentified data) or §46.104(d)(2) (research involving collection of information via survey procedures where subjects cannot reasonably be identified). This determination must be made by the IRB, not by NeuroPulse. File a determination request rather than a full protocol if seeking exemption.

**Protocol content requirements (if full review):**
- Title, PI, institution, sponsor
- Background and significance
- Study design (observational, retrospective, prospective)
- Subject selection and number
- Data sources and anonymisation methods (reference NP-FW-ANON-001)
- Informed consent procedures (reference on-device consent engine architecture)
- Data security measures (reference NP-FW-EMMC-001)
- Risks and benefits
- Consent withdrawal procedure (reference NP-PRIV-REM-001)

**Timeline:** Central IRBs (WCG, Advarra): 5–15 business days for expedited review; 4–6 weeks for full board review. University IRBs: 2–8 weeks depending on institution and review type. Budget $1,500–5,000 for initial review fees.

---

### 4.5 HIPAA Breach Notification — HHS OCR (Triggered — not a pre-planned submission)

**Authority:** US Department of Health and Human Services, Office for Civil Rights (OCR)  
**Reference:** 45 CFR §§164.400–414 (HIPAA Breach Notification Rule)  
**Notification portal:** HHS OCR Breach Reporting Portal (https://ocrportal.hhs.gov/ocr/breach/wizard.jsf)  
**Trigger:** Discovery of a breach of unsecured PHI affecting T2 covered entity operations  

**Prerequisites before this submission could be triggered:**
- NP-SEC-BR-001 breach severity determination (PHI involved? Unsecured? Low probability of compromise assessment completed?)
- Forensic scope established: number of individuals affected, types of PHI

**Timing:**
- ≥500 individuals in a state: notify HHS within 60 days of discovery AND notify prominent media outlets in the affected state(s) within 60 days
- <500 individuals: notify HHS within 60 days after end of calendar year in which breach occurred
- All affected individuals: notify without unreasonable delay, no later than 60 days from discovery

**Content required (45 CFR §164.412):**
- Description of what happened, including date of breach and date of discovery
- Types of unsecured PHI involved (e.g. EEG session data, HRV time series, session timestamps, protocol parameters)
- Steps individuals should take to protect themselves
- Steps NeuroPulse is taking to investigate and mitigate harm
- Contact information for NeuroPulse (toll-free number, email, postal address, or website active for 90 days)

**Individual notification format:** Written notice by first-class mail (or email if individual agreed); if contact information is insufficient for 10+ individuals, substitute notice via website or media (45 CFR §164.404(d))

---

### 4.6 FTC Health Breach Notification Rule (Triggered)

**Authority:** Federal Trade Commission  
**Reference:** 16 CFR Part 318; FTC HBNR (amended 2024)  
**Notification portal:** https://www.ftc.gov/databreaches  
**Trigger:** Discovery of a breach of security of unsecured PHR identifiable health information in NeuroPulse's T1 personal health record (PHR) application or related services  

**Applicability:** FTC HBNR applies to vendors of PHRs and PHR-related entities that are not HIPAA-covered. T1 NeuroPulse Home, as a consumer wellness device and app that is not itself a covered entity, falls under HBNR rather than HIPAA for its consumer-facing operations.

**Timing:** Without unreasonable delay and no later than 60 days after discovery (notification to FTC and to affected individuals); media notification for breaches affecting 500+ residents of a state within 60 days

**Content (16 CFR §318.5):**
- Name and contact information of NeuroPulse
- Description of what PHR identifiable health information was involved
- Brief description of what happened (date of breach, date of discovery)
- Brief description of what NeuroPulse is doing to investigate the breach
- Brief description of steps individuals can take to protect themselves
- Contact information (free call, email, and postal address active for 90 days)

---

### 4.7 GDPR Art. 33 Supervisory Authority Notification (Triggered)

**Authority:** Lead EU supervisory authority (pre-EU-establishment: DPA of each member state in which EU data subjects affected by the breach reside)  
**Reference:** GDPR Art. 33; EDPB Guidelines 9/2022 on personal data breach notification under the GDPR  
**Timing:** Without undue delay and no later than **72 hours** after becoming aware of the breach (Art. 33(1))  
**Phased notification:** If complete information is not available within 72 hours, file initial notification and supplement. Document reason for delay.  

**Content required (Art. 33(3)):**
- Nature of the personal data breach (categories and approximate number of data subjects; categories and approximate number of records)
- Name and contact details of the data protection officer or other contact point
- Likely consequences of the breach
- Measures taken or proposed to address the breach, including mitigation measures

**Key point:** NeuroPulse has no EU establishment at formation. Until one is established, the "lead supervisory authority" principle does not apply. Instead, notify each national DPA where affected EU data subjects are located. This is operationally demanding for multi-country breaches; EU establishment (even a simple legal representative under GDPR Art. 27) designates a single lead DPA and simplifies this.

**Recommendation:** Designate a GDPR Art. 27 representative (EU-based legal entity or individual) before EU T1 launch. Cost: ~€2,000–5,000/year via a specialist representative service. This also designates a lead DPA, typically the DPA of the country where the representative is located.

**Individual notification (Art. 34):** Required when the breach is likely to result in a high risk to the rights and freedoms of natural persons. For NeuroPulse, a breach of SHDR (device telemetry) linked to a named individual (warranty registrant) would likely trigger Art. 34. A breach of UHDR would be high-risk and require individual notification.

---

### 4.8 US State Breach Notification Laws (Triggered)

**Authority:** Attorney General of each state where affected residents are located  
**Reference:** NCSL State Security Breach Notification Laws compilation (https://www.ncsl.org/technology-and-communication/security-breach-notification-laws); as of 2024, all 50 states + DC, PR, USVI, and Guam have breach notification laws  
**Key timing deadlines:**
- 30 days: Florida, Colorado, Delaware, Montana, New Mexico, Washington, and others
- 45 days: North Carolina, Ohio, Connecticut (under revised law)
- 60 days: California (CCPA §1798.82 for medical information), New York, others
- 72 hours: Hawaii (most restrictive US state law)
- "Expedient" / "reasonable time" (30–60 days in practice): majority of remaining states

**Operational requirement:** NP-SEC-BR-001 must include a current state notification law reference table (updated annually) so that notification deadlines are identified within hours of discovery, not days.

**CCPA medical information:** California Civil Code §56.36 (Confidentiality of Medical Information Act) provides for statutory damages of $1,000 per affected California resident if medical information is breached. NeuroPulse SHDR linked to warranty owner may constitute "medical information" under CMIA. Ensure breach severity assessment considers California CMIA specifically.

---

## 5. Direct Remediations Completed (2026-06-02)

The following documents were authored as part of this remediation programme.

**Session 1 — 2026-06-02 (NP-PRIV-001 Rev A):**

| Document | Document number | File | Steps addressed |
|---|---|---|---|
| Firmware spec: warranty token, factory reset, two-layer key, Scratch encryption, EDF+ headers, Mode F spec | NP-FW-EMMC-002 Rev A | `docs/np_fw_emmc_002.md` | STEP-01 through STEP-06 |
| Breach Response Plan | NP-SEC-BR-001 Rev A | `docs/np_sec_br_001.md` | STEP-07 |
| POA Upload Procedure | NP-PROC-POA-001 Rev A | `docs/np_proc_poa_001.md` | STEP-08 |
| App Telemetry Policy | NP-APP-TELEMETRY-001 Rev A | `docs/np_app_telemetry_001.md` | STEP-09 |
| This document | NP-PRIV-REM-001 Rev A | `docs/np_priv_rem_001.md` | Framework for STEP-01 through STEP-30 |

**Session 2 — 2026-06-03 (NP-PRIV-001 Rev B delta — 8 new findings):**

| Document / change | Number | File | Steps addressed |
|---|---|---|---|
| NP-APP-TELEMETRY-001 Rev B — `session_sequence` replaced with `engagement_tier` coarsened enum; §3.2 added | NP-APP-TELEMETRY-001 Rev B | `docs/np_app_telemetry_001.md` | LOW-03 (Rev B finding) |
| NP-FW-EMMC-002 §G added — SHDR accelerometer reclassification spec | NP-FW-EMMC-002 Rev A (§G appended) | `docs/np_fw_emmc_002.md` | MEDIUM-06 (Rev B finding); unblocks STEP-10 |
| NP-PRIV-REM-001 Rev B — STEP-31, STEP-32, STEP-33 added; capability matrix rows added | NP-PRIV-REM-001 Rev B | `docs/np_priv_rem_001.md` | STEP-31 through STEP-33 |

CLAUDE.md §13.4 updates applied in Session 2:
- BIPA legal opinion (before Illinois device activation) added as pending decision
- Washington MHMD regulatory analysis added as pending decision
- Children's age gate (minimum age 16 declaration) added as pending decision
- HIPAA Expert Determination certifier engagement (STEP-31) added as pending decision
- Adaptive stimulation transparency (STEP-33) added as pending decision
- `engagement_tier` replaces `session_sequence` in NP-APP-TELEMETRY-001 added as locked decision

---

## 6. Capability Matrix

| Step | Performer role | Min. authority | Legal expertise | Engineering | Clinical/medical | Regulatory | Can automate? | External required? |
|---|---|---|---|---|---|---|---|---|
| STEP-01 | Firmware architect | CEO sign-off | No | Yes | No | No | Partial (DB constraint) | No |
| STEP-02 | Firmware engineer + UX | Legal review of UX copy | No | Yes | No | No | Yes (hardware ops) | No |
| STEP-03 | Firmware architect + crypto reviewer | Qualified reviewer | No | Yes | No | No | Yes (key re-wrap) | Optional |
| STEP-04 | Firmware engineer | Standard design review | No | Yes | No | No | Yes | No |
| STEP-05 | Firmware engineer | None | No | Yes | No | No | Yes (unit test) | No |
| STEP-06 | Firmware + UX + regulatory | Regulatory counsel sign-off | No | Yes | No | Yes | Partial | Yes (counsel) |
| STEP-07 | CEO + Legal counsel | CEO approval | Yes | No | No | Partial | Partial (alerts) | Yes (counsel) |
| STEP-08 | CEO + Legal + IT | CEO approval | Yes | Yes | No | No | Partial (deletion job) | Yes (counsel) |
| STEP-09 | Engineering lead + Privacy Lead | CEO approval | Partial | Yes | No | No | Partial (linting) | Yes (vendor DPA) |
| STEP-10 | Firmware engineer + data architect | CEO sign-off (design change) | No | Yes | No | No | Yes | No |
| STEP-11 | Firmware + privacy engineer | Qualified DP reviewer | No | Yes | No | No | Yes (test suite) | Yes (DP reviewer) |
| STEP-12 | CEO + EU privacy counsel | CEO certification | Yes | No | No | Yes | Partial (annual alert) | Yes (counsel, IIDRP) |
| STEP-13 | Healthcare legal counsel | CEO execution | Yes | No | No | Partial | Partial (e-sig) | Yes (counsel) |
| STEP-14 | Clinical informatics + Privacy | Privacy Lead sign-off | No | Yes | Partial | No | Yes (CI validation) | Optional (HL7 consultant) |
| STEP-15 | API engineer + Privacy | Privacy sign-off | No | Yes | No | No | Yes (rate limits, tests) | No |
| STEP-16 | iOS engineering + Legal | CEO sign-off | Yes | Yes | No | No | Partial (Xcode) | Apple review |
| STEP-17 | Android engineering + Legal | CEO sign-off | Yes | Yes | No | No | Partial | Google review |
| STEP-18 | Regulatory counsel | CEO instruction | No | No | No | Yes | No | Yes (existing counsel) |
| STEP-19 | Infrastructure architect + Legal | CEO cloud vendor decision | Yes | Yes | No | Partial | Yes (IaC) | Yes (cloud vendor, EU counsel) |
| STEP-20 | Clinical research coordinator + PI | PI qualification required | Partial | No | Yes | Partial | No | Yes (IRB) |
| STEP-21 | Privacy-qualified engineer | Quality Lead sign-off | No | Yes | No | No | Partial | Optional |
| STEP-22 | QA engineer | Quality Lead sign-off | No | Yes | No | No | Yes | No |
| STEP-23 | QA + Clinical platform engineer | Quality Lead sign-off | No | Yes | No | No | Partial | No |
| STEP-24 | Privacy engineer + DP reviewer | DP reviewer + Quality Lead | No | Yes | No | No | Yes | Yes (DP reviewer) |
| STEP-25 | IT/Infrastructure + pentest | CEO sign-off for production | No | Yes | No | No | Partial | Yes (pentest firm) |
| STEP-26 | External security auditor | CEO scope approval | No | Yes | No | No | Partial | Yes (audit firm) |
| STEP-27 | CEO + Legal + all escalation roles | CEO convenes | Yes | No | No | No | No | Optional (facilitator) |
| STEP-28 | Infrastructure engineer | CEO sign-off on SLA | No | Yes | No | Partial | Yes (IaC) | Yes (cloud vendor, EU counsel) |
| STEP-29 | CEO + Legal | CEO certification | Yes | No | No | Partial | Partial | Yes (counsel) |
| STEP-30 | Privacy Lead + Legal | Quality Lead + CEO | Yes | Partial | No | Partial | Partial (monitoring) | Optional |
| STEP-31 | CEO + clinical research coord | CEO sign-off; QA confirms template | Partial | No | No | Partial | No | Yes (certifier) |
| STEP-32 | Expert certifier (STEP-31) | Quality Lead gate | No | No | Yes (biostatistics) | Partial | Partial (gate check) | Yes (certifier) |
| STEP-33 | Firmware + iOS/Android + Privacy Lead | Privacy Lead sign-off on copy | No | Yes | No | No | Partial (FW logging, UI render) | No |

---

## 7. Change Control

This document is under change control per NP-QMS-DC-001. Revisions required when:
- Any STEP status changes to COMPLETE (update status and add completion date)
- A new finding is identified that requires a new STEP
- A regulatory change materially affects an authority submission requirement (§4)
- A STEP target milestone is changed

Minor status updates (OPEN → IN PROGRESS → COMPLETE) may be made as minor revisions. New STEPS and changes to §4 authority requirements require a full revision cycle with CEO approval.
