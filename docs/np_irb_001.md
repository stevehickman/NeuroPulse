# IRB Protocol for Research Consent Architecture Validation

**Project:** NeurOne
**Document:** NP-IRB-001
**Revision:** A
**Date:** 2026-07-27
**Status:** DRAFT
**Effective Date:** N/A (effective on IRB/REB approval or exemption determination)
**Author:** Steve Hickman (CEO, interim Quality authority)
**Approved By:** Pending — requires PI co-signature and IRB/REB approval before ACTIVE status
**References:** NP-PRIV-REM-001 Rev A STEP-20; NP-FW-ANON-001 Rev A; NP-FW-EMMC-001 Rev A §15; CLAUDE.md §6.2 (a priori research consent), §5.3 (research data anonymization architecture); NP-PRIV-NOTICE-001 Rev C §5; NP-HFE-001 Rev A §7.2 (eject-lever accessibility study — the first study anticipated to run under this protocol's exemption/expedited pathway)
**Related Issues:** —
**Gate:** Prerequisite for NP-HFE-001 §7.2 eject-lever study and for any research study descriptor sent to a device (NP-FW-ANON-001)
**IEC 62304 Class:** N/A
**Applicable Standard:** 45 CFR Part 46 (Common Rule); 21 CFR Part 50/56 (if FDA-regulated research); Tri-Council Policy Statement (Canada, if Unity Health REB path is used)
**Next Review:** Annual re-approval per IRB/REB continuing-review requirement, or per protocol amendment
**Supersedes:** None (first issue). Referenced as an open item (NP-PRIV-REM-001 STEP-20) prior to this document.

---

## 1. Purpose and Scope

This document is the IRB/REB protocol template and submission-readiness package for any human-subjects research NeurOne facilitates, whether (a) a formal clinical/observational study using NeurOne devices as the data-collection platform, or (b) a usability/accessibility study of NeurOne's own device (e.g., the NP-HFE-001 §7.2 eject-lever study with Parkinson's/post-stroke subjects). It formalizes the procedure described in `docs/np_priv_rem_001.md` STEP-20.

**This is a template plus a decision procedure, not a per-study protocol.** Each actual study gets its own signed protocol instance derived from this template, its own IRB/REB submission, and — where research anonymization firmware is involved — its own Expert Determination certification (NP-ANON-CERT-[study_id], per §13.4 STEP-32/STEP-31 in NP-PRIV-REM-001). This document is what makes those per-study submissions fast to produce, by fixing the boilerplate (anonymization architecture, consent mechanism, data security) once.

---

## 2. Two Distinct Study Types Under This Protocol

| Type | Example | Regulatory posture | Data source |
|---|---|---|---|
| **A — Usability/accessibility study of the device itself** | NP-HFE-001 §7.2 eject-lever study (n=5, Parkinson's/post-stroke) | May qualify for exempt or expedited IRB review as minimal-risk usability observation of a commercial product, but eligibility is determined per-study by IRB/REB counsel, not assumed here, because the subject population is selected by diagnosis (Parkinson's, post-stroke), which some IRBs treat as inherently non-exempt regardless of task risk | Observational (force, time, verbal feedback) — no NeurOne device UHDR/session data collected as research data |
| **B — Clinical/observational research using NeurOne as a data platform** | e.g., a PBM/EEG/HRV combination-therapy study per `docs/neurone_clinical_trials_strategy.docx` | Full IRB/REB review; may also qualify for 45 CFR §46.104(d)(4) exemption if data is de-identified at the point of collection (see §5) | On-device anonymized UHDR extracts per NP-FW-ANON-001 |

Both types share the recruitment, consent-withdrawal, and data-security sections below; Type A does not need §5 (anonymization architecture) if no device UHDR is collected as research data, but must still address consent and withdrawal for the observational data actually collected (video/audio recording of the task, if used).

---

## 3. Primary Pathway: University Collaborator IRB/REB

**Preferred over a central commercial IRB** because it avoids the "principal investigator affiliation" requirement at university IRBs and is materially cheaper.

### 3.1 Candidate PI and institution

- **Candidate:** Neda Rashidi-Ranjbar, Unity Health Toronto (contact: neda.rashidi-ranjbar@unityhealth.to)
- **Rationale:** Existing funded research program (2025 MCI PBM RCT); an established REB relationship at Unity Health Toronto avoids standing up a new IRB relationship from zero.
- **Outreach status:** Not yet initiated as of this document's issue date (§9 open items).
- **REB, not IRB:** Unity Health Toronto's Research Ethics Board (REB) is the Canadian-institution equivalent; this protocol's content is designed to satisfy both REB and US IRB review with the same document, since the underlying human-subjects protections (informed consent, minimal risk, data security) are materially the same.

### 3.2 Outreach sequence

1. Initial email to Rashidi-Ranjbar: brief NeurOne description, reference to her 2025 MCI PBM RCT, proposed pilot using NeurOne as the study platform, offer of device loan + data access + co-authorship. Attach the 2-page executive summary of `docs/neurone_clinical_trials_strategy.docx`.
2. If interested: confirm whether she can serve as PI or co-PI on a NeurOne-facilitated protocol, and discuss the Unity Health REB submission process.
3. If she agrees to PI: proceed to §4 (protocol content) using the REB's submission format, with NP-FW-ANON-001 attached as the primary technical appendix describing the anonymization architecture.
4. File for a US exemption determination under **45 CFR §46.104(d)(4)** (secondary research with de-identified data) in parallel with REB review, using the same protocol document — applicable to Type B studies where on-device anonymization means no identifiable data leaves the device (see §5).

---

## 4. Fallback Pathway: Sponsor-Investigator with Commercial IRB

If the university-collaborator path is unavailable (Rashidi-Ranjbar unavailable and no alternative university PI identified):

1. **Alternative academic contact:** Mayank Jog, UCLA (mjog@mednet.ucla.edu) — NIH K99/R00 grant aligned to HD-tDCS protocols.
2. **Commercial IRB fallback:** WCG IRB (wcgirb.com) or Advarra IRB (advarra.com), single-site sponsor-investigator study with CEO as PI. Estimated cost $3,000–8,000 for initial review.
3. **Prerequisite for CEO-as-PI:** Current human-subjects research training — CITI Program certification (citiprogram.org, ~4 hours, free).
4. **Federalwide Assurance (FWA):** Required if NeurOne is sponsor-investigator (not routing through a university's existing FWA). Filed with OHRP at ohrp.cit.nih.gov/efile. Free; 2–4 week processing; annual renewal required.

---

## 5. Anonymization Architecture (Type B studies — reference, not restated)

Full technical detail lives in NP-FW-ANON-001 and CLAUDE.md §5.3; this section states only what an IRB/REB reviewer needs to evaluate exemption eligibility:

- All anonymization occurs **on-device**, before any data leaves the device. NeurOne infrastructure never holds the biometric-derived UHDR decryption key and cannot access raw UHDR at any point, including for research.
- Each study receives a signed study descriptor (approved element list, k≥10, l≥3 l-diversity, ε≤1.0 differential privacy per NP-FW-ANON-001) before any device processes it.
- Extracts are transmitted keyed to study ID and device ID only — no persistent per-user identifier, no linkage table.
- This architecture is the basis for the 45 CFR §46.104(d)(4) exemption application in Type B studies: the data is de-identified *before* the researcher (or NeurOne) ever receives it, not de-identified afterward by a party who once held identifiable data.

---

## 6. Consent Mechanism

- **Type B (device data):** The a priori research consent flow (CLAUDE.md §6.2, four layers L1–L4) is the standing consent mechanism for any NeurOne user whose anonymized data may be included in an IRB-approved study. This protocol does not create a separate consent flow — it is the IRB-facing description of the consent flow that already exists in the app, per NP-PRIV-NOTICE-001 §5.
- **Type A (in-person usability study, e.g. NP-HFE-001 §7.2):** A separate written informed-consent form is required — the app's a priori consent layers do not cover in-person observational studies with video/audio recording. This form is drafted at protocol-instance time using standard REB/IRB informed-consent templates and is not pre-specified here.
- **Both types:** Subjects may withdraw at any time without penalty; withdrawal from a Type A study stops further observation/use of that subject's data prospectively (standard human-subjects consent withdrawal — no special anonymization consideration since Type A doesn't use the anonymization pipeline). Withdrawal from Type B (device-data) research consent follows the forward-effectiveness rule in CLAUDE.md §5.3: withdrawal permanently blocks future study descriptors from being processed, for any data period including sessions predating withdrawal; already-published extracts cannot be individually removed (irreversibility notice given at consent time, CLAUDE.md §6.2 L3).

---

## 7. Data Security

- **Type B:** Per NP-FW-EMMC-001 §15 (research anonymization Scratch workspace) and NP-FW-EMMC-002 §D (Scratch partition encryption) — per-task AES-256-CTR key, SRAM-only, zeroed on completion, eMMC SANITIZE post-run. Extract transmission is TLS-encrypted; NeurOne research infrastructure stores extracts keyed to study ID + device ID only.
- **Type A:** Any video/audio recording or force-gauge data from the eject-lever study is stored per the study site's own IRB-approved data security plan (not a NeurOne UHDR/SHDR data flow at all — it's traditional human-subjects research data, held by the study site or a CEO-as-PI sponsor-investigator record, per the standard investigator data-security obligations under 45 CFR Part 46).

---

## 8. Risks and Benefits (template — completed per study instance)

| Category | Type A (usability) | Type B (clinical/observational) |
|---|---|---|
| Physical risk | Minimal — task is normal device use (module extraction), no stimulation applied during the observed task | Depends on modality under study; existing NeurOne hardware safety interlocks (CLAUDE.md §4.2) apply regardless of research context — a study does not disable any safety interlock |
| Privacy risk | Minimal — video/audio per site's standard consent; no NeurOne UHDR involved | Mitigated by on-device anonymization (§5); residual risk is the general re-identification risk of any k-anonymized dataset, disclosed to subjects per the standard irreversibility notice (CLAUDE.md §6.2 L3) |
| Benefit to subject | Direct: informs whether NeurOne's accessibility design actually works for their population | Indirect: contributes to evidence base; direct benefit only if study includes therapeutic protocol arms, assessed per study |
| Benefit to society | Informs FDA HFE Guidance compliance for a device intended to serve this population | Clinical evidence contribution, addresses CLAUDE.md "zero published clinical trials" gap (`docs/status/pending-decisions.md` §13.1) |

---

## 9. Open Items

| ID | Description | Blocking | Target |
|---|---|---|---|
| OI-IRB-01 | Initiate outreach to Rashidi-Ranjbar (§3.2 step 1) | All downstream steps | Month 9 |
| OI-IRB-02 | CEO CITI human-subjects training certification (needed regardless of which pathway is used, since CEO may serve as co-PI or sponsor-investigator) | §4.3 fallback readiness | Month 9 |
| OI-IRB-03 | File OHRP FWA if sponsor-investigator path is used | §4.4 | Concurrent with fallback-path activation |
| OI-IRB-04 | Draft Type A informed-consent form for the eject-lever study once a study site/PI is confirmed | NP-HFE-001 §7.2 start | Concurrent with §3.2/§4 resolution |
| OI-IRB-05 | Confirm with IRB/REB counsel whether the eject-lever study qualifies for exempt/expedited review given diagnosis-based subject selection (§2 Type A caveat) | NP-HFE-001 §7.2 start | Concurrent with protocol submission |
| OI-IRB-06 | First Type B study descriptor requires its own NP-ANON-CERT-[study_id] Expert Determination certification (NP-PRIV-REM-001 STEP-31/STEP-32) — separate from this protocol's IRB approval | Any Type B study deployment | Per-study, before descriptor signing |

---

## 10. Change Control

Each per-study protocol instance derived from this template is version-controlled separately (e.g., as an IRB/REB submission artifact, not necessarily in this repository). This template document is updated when the underlying consent mechanism, anonymization architecture, or pathway selection changes — each such change is a significant change under NP-QMS-DC-001.
