# Post-Market Surveillance Plan

**Project:** NeurOne
**Document:** NP-PMS-001
**Revision:** A
**Date:** 2026-07-27
**Status:** ACTIVE
**Effective Date:** 2026-07-27
**Author:** Steve Hickman (CEO, interim Quality authority)
**Approved By:** Steve Hickman, CEO
**References:** 21 CFR Part 803 (Medical Device Reporting); 21 CFR Part 820 (QSR); ISO 13485:2016 §8.2 (feedback), §8.5 (improvement); ISO 14971:2019 §10 (post-market information); NP-QMS-CAPA-001 Rev A; NP-RM-001 Rev A §12; NP-SEC-BR-001 Rev A (privacy/security incident — distinct scope, cross-referenced not duplicated); NP-DP-001 Rev A
**Related Issues:** —
**Gate:** T1 launch prerequisite (NP-DP-001 §6.4 gate schedule, Month 17 target)
**IEC 62304 Class:** N/A
**Applicable Standard:** 21 CFR Part 803; ISO 13485:2016 §8.2; ISO 14971:2019 §10; EU MDR 2017/745 Art. 83–86 (if/when EU CE marking is pursued — see §8)
**Next Review:** Annual, and after any CAPA classified Critical or any regulatory reporting event
**Supersedes:** None (first issue). Referenced as "planned Month 12" / "planned Year 2" in NP-DHF-001, NP-DP-001, NP-QMS-001, NP-RM-001 prior to this document.

---

## 1. Purpose and Scope

This plan defines how NeurOne systematically collects, analyzes, and acts on post-market information about device safety and performance, per ISO 13485:2016 §8.2 and, for the eventual T2 510(k) product, 21 CFR Part 803. It closes the "(NP-PMS-001, planned Year 2)" placeholder in the QMS process map (`docs/np_qms_001.md`) and the NP-RM-001 §12 post-market risk feedback loop reference.

**Scope:** All shipped NeurOne hardware (T1 and T2), firmware, and companion apps, across their service life. Distinguishes three post-market information streams that must not be conflated:

1. **Device safety/performance signals** — this document's primary subject: complaints, adverse events, SHDR fleet telemetry trends, service/repair data.
2. **Privacy/security incidents** (data breach, unauthorized access) — governed by NP-SEC-BR-001, not this document. A privacy incident that also indicates a device safety hazard (rare, but e.g. a security flaw enabling unauthorized stimulation control) triggers both plans in parallel; §7 defines the handoff.
3. **General product feedback** (feature requests, comfort preferences) — routed to product management, not a quality-system input unless it rises to the complaint definition in §3.

---

## 2. Regulatory Basis and Applicability

| Requirement | Applies to | Notes |
|---|---|---|
| ISO 13485:2016 §8.2.1 (feedback) | T1 + T2 | QMS-wide requirement regardless of regulatory tier |
| 21 CFR §820.198 (complaint files) | T1 + T2 | US QSR complaint handling requirement |
| 21 CFR Part 803 (MDR) | T2 only, post-510(k) clearance | Death/serious injury reports within 30 days (5 days if remedial action needed to prevent unreasonable risk); malfunction reports per schedule. T1 wellness-exempt devices are not subject to MDR, but any T1 complaint is still evaluated against the same severity criteria in case it reveals a hazard shared with T2 hardware (shared chassis, shared zone modules) |
| EU MDR 2017/745 Art. 83–86 | Only if/when EU CE marking is pursued (not currently in the regulatory strategy — see `docs/reference/regulatory-strategy.md`) | Included here as a forward reference so the PMS architecture (SHDR trending, complaint database, PSUR-equivalent reporting cadence) is compatible with EU PMS/PMCF requirements without a structural rework if the EU strategy changes |
| electroCore gammaCore predicate vigilance obligations | T2 cervical VNS only, post-clearance | Any post-market signal materially different from the predicate's known profile is evaluated for FDA notification per NP-REG-CVNS-001 |

---

## 3. Complaint Definition and Intake

A **complaint** is any written, electronic, or oral communication alleging deficiencies related to identity, quality, durability, reliability, safety, or performance of a NeurOne device after it has left NeurOne's control — the 21 CFR §820.3(b) definition, applied uniformly to T1 and T2.

### 3.1 Intake channels

| Channel | Owner | Routing |
|---|---|---|
| In-app support / feedback form | Support | Triaged against complaint definition; true complaints escalated within 1 business day |
| Clinician/clinic reports (T2, via service network) | Service network partner tier (`docs/reference/service-network.md`) | Contractually required to route safety-relevant reports to NeurOne within 2 business days |
| Warranty/repair intake | Support/Repair | Any returned unit with a safety-relevant fault (thermal, stimulation, EMF-shielding-related) is flagged as a complaint regardless of whether the customer used the word |
| Social media / public review monitoring | Marketing (informational only) | Only escalated to complaint status if it describes a specific safety/performance deficiency traceable to a specific user — not general sentiment monitoring |
| Regulatory authority inquiry | Quality Lead | Always a complaint; highest-priority triage |

### 3.2 Complaint record (per 21 CFR §820.198)

Each complaint record captures: device identification (warranty token — never the SHDR-linked identity resolved beyond what's needed for the specific investigation, per the UHDR/SHDR consent boundary in CLAUDE.md §5–§6), date received, nature of complaint, investigation performed (or documented rationale if no investigation), corrective action taken, and reply to complainant if applicable. Complaint records do not require or store UHDR — a complaint investigation that needs session-level data must go through the same per-element clinician consent gate as any other UHDR access (CLAUDE.md §6.1); it cannot bypass consent because "it's a complaint."

---

## 4. Post-Market Data Sources and Analysis Cadence

| Source | Data | Analysis cadence | Feeds into |
|---|---|---|---|
| SHDR fleet database | LED output ratio drift, NTC thermal profiles, EMF shielding attenuation, impact events, consumable session counts, fault latch counts | Continuous automated trending (Phase 1 population-average → Phase 2 fleet LSTM → Phase 3 Bayesian personalization, per CLAUDE.md §5.2); monthly summary report to Quality Lead | NP-RM-001 §12 probability re-estimation; CAPA trigger if trend crosses threshold |
| Complaint database | Per §3 | Monthly trend review; immediate review for any Critical-severity complaint | CAPA (NP-QMS-CAPA-001) |
| Service/repair data | Failure modes observed at Tier B/C service visits | Quarterly | FMEA updates (NP-FMEA-001); supplier corrective action if failure traces to a component |
| Literature / competitor field safety notices | Recalls, safety communications for comparable neuromodulation devices (industry-wide, not NeurOne-specific) | Quarterly scan | NP-RM-001 §12 hazard identification — evaluate applicability |
| Clinical evidence updates (post-launch RCTs, published literature on NeurOne's own modalities) | New efficacy/safety findings for PBM, tACS, VNS, TMS, HD-tDCS | Quarterly (owned by whoever holds the SAB / clinical liaison role once formed) | Marketing claims gate re-evaluation; NP-RM-001 hazard re-evaluation |
| Adverse event reports (T2, post-510(k)) | Per Part 803 | Per regulatory deadline (see §5) | MDR filing; CAPA |

### 4.1 SHDR trending thresholds

Concrete alert thresholds (e.g., "X% of fleet units show EMF attenuation degradation beyond Y dB") are defined and maintained in the predictive-maintenance model configuration (CLAUDE.md §5.2 reminder-engine rules), not duplicated here as static numbers — this document specifies the *process* by which a threshold breach becomes a quality-system event: any Phase 1/2/3 model output flagged as "safety-critical" (per the CLAUDE.md §5.2 reminder-engine severity classes) is automatically logged as a PMS signal and routed to the Quality Lead within 1 business day, without requiring a human to have first noticed a trend manually.

---

## 5. Regulatory Reporting Decision Tree (T2, post-510(k))

Applies once T2 receives 510(k) clearance. Until then, T2 (and T1, always) complaints are handled under §3–§4 quality-system process without MDR filing obligations.

1. **Is there a death or serious injury reasonably suggested to be device-associated?**
   - Yes → File MDR within 30 days (21 CFR §803.50); assess whether remedial action is needed to prevent unreasonable risk (if yes, 5-day report per §803.53); notify CEO and Quality Lead immediately; open CAPA as Critical.
   - No → continue.
2. **Is there a malfunction that would be likely to cause or contribute to death/serious injury if it recurred?**
   - Yes → File MDR per §803.50 malfunction reporting; open CAPA.
   - No → continue.
3. **Does the event indicate a need for a Field Safety Corrective Action (FSCA)** (correction or removal to reduce a health risk)?
   - Yes → FSCA process (parallel track: customer notification, corrective action plan, effectiveness check — modeled on NP-SEC-BR-001's severity/escalation structure for the notification mechanics, but this is a device-safety FSCA, not a data breach, and uses this document's CAPA linkage, not NP-SEC-BR-001's breach-notification content).
   - No → log as standard complaint/CAPA per §3–§4, no external reporting required.

This decision tree deliberately mirrors the structure already established in NP-SEC-BR-001 for privacy incidents, using the same escalation-chain discipline, so that staff who know one process can navigate the other without learning an unrelated framework — while keeping the two document scopes (privacy breach vs. device safety) legally and procedurally distinct, since they trigger different regulators (HHS OCR / state AGs vs. FDA).

---

## 6. Linkage to CAPA and Risk Management

- Every complaint classified Major or Critical (per NP-QMS-CAPA-001 severity table) automatically opens a CAPA.
- Every PMS-sourced CAPA feeds back into NP-RM-001 as a post-market risk review trigger (NP-RM-001 §12.2): a Critical CAPA triggers an unscheduled risk management file review.
- Aggregate PMS trends (not just individual events) are reviewed at the **annual PMS review** (§9) and compared against the residual risk conclusions in the Risk Management Report (NP-RM-001 §13, due Month 18) to confirm the pre-market residual risk estimate still holds in the field.

---

## 7. Handoff Between PMS and Breach Response (NP-SEC-BR-001)

If a single incident has both a privacy/security dimension and a device-safety dimension (e.g., a firmware vulnerability that could both expose UHDR and allow unauthorized stimulation parameter changes):

1. Both this plan's decision tree (§5) and NP-SEC-BR-001's severity classification run in parallel — they are not sequential gates.
2. A single incident commander is assigned (per whichever plan's escalation chain reaches the CEO/Quality Lead first — the plans share the same top-level escalation contact by design).
3. Regulatory notifications are filed under both frameworks independently where both apply (e.g., an MDR under Part 803 *and* a breach notification under state law/GDPR, if the incident qualifies under both definitions) — one does not substitute for the other.

---

## 8. EU Post-Market Surveillance and PMCF (forward reference, not currently active)

NeurOne's current regulatory strategy (`docs/reference/regulatory-strategy.md`) does not include EU CE marking. This section exists so that if that changes, the PMS architecture already defined here (SHDR trending, complaint database, annual review cadence) does not require a structural rebuild — only the addition of: a PSUR (Periodic Safety Update Report) on the EU-mandated schedule, a PMCF (Post-Market Clinical Follow-up) plan, and EU Authorized Representative reporting channels. Not further specified until the regulatory strategy activates this path.

---

## 9. Annual PMS Review

Once per year (and after any Critical CAPA), the Quality Lead compiles a PMS Review Report covering: complaint volume and trend by category, SHDR fleet trend summary, CAPA opened/closed in the period, any regulatory reports filed, comparison against the Risk Management Report's residual risk conclusions, and a recommendation on whether the risk management file (NP-RM-001) requires update. Filed in the DHF.

---

## 10. Responsibilities

| Role | Responsibility |
|---|---|
| Quality Lead (open req., interim CEO per NP-QMS-001) | Owns this plan; chairs annual PMS review; decision-tree owner for MDR filing determination |
| Support | Complaint intake triage (§3.1) |
| Service network partners | Contractual reporting obligation for T2 safety-relevant field reports (2 business days) |
| Engineering | SHDR trend model maintenance (CLAUDE.md §5.2 phases); root-cause support for CAPA |
| Regulatory Lead | MDR filing (§5); EU PMCF if/when activated (§8) |

---

## 11. Open Items

| ID | Description | Blocking | Target |
|---|---|---|---|
| OI-PMS-01 | Hire or contract Quality Lead (currently interim CEO) — same open item as NP-QMS-001/NP-RM-001 | Full PMS operation | Month 6 |
| OI-PMS-02 | Select complaint-handling database/tool (may be part of eQMS platform selection, NP-QMS-001 Month 9 item) | §3.2 record-keeping at scale | Month 9 |
| OI-PMS-03 | Define concrete SHDR alert thresholds per predictive-maintenance model phase (currently governed by CLAUDE.md §5.2 reminder-engine rules at a policy level; needs numeric thresholds per fleet size phase) | §4.1 | Concurrent with Phase 1 fleet data (Year 1) |
| OI-PMS-04 | Confirm service-network contract language includes the 2-business-day safety-report obligation in §3.1 | T2 launch | Before first T2 service partner contract signed |

---

## 12. Change Control

Any change to the complaint severity classification, MDR decision tree, or SHDR alert-threshold policy is a significant change under NP-QMS-DC-001 and requires Quality Lead sign-off (or CEO, while the role is interim).
