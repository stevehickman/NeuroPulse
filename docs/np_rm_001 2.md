# ISO 14971 Risk Management Plan

**Project:** NeuroPulse  
**Document:** NP-RM-001  
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
**Applicable Standard:** ISO 14971:2019 — Medical Devices: Application of Risk Management  
**Next Review:** 2027-05-13 (annual) or upon any significant design change  
**Applicable Devices:** NeuroPulse Home (T1), NeuroPulse Pro (T2)

---

## 1. Purpose

This Risk Management Plan defines the process, responsibilities, criteria, and records for risk management throughout the NeuroPulse device lifecycle, in compliance with ISO 14971:2019. It formally brings the existing NeuroPulse risk register (RISK-01 through RISK-25, documented in NP-RISK-001 Rev A) under QMS change control as of the effective date above.

---

## 2. Scope

Risk management applies to the complete NeuroPulse device system including:

- All hardware modalities (T1 and T2)
- All firmware components (Safety MCU Class C, main processor Class B)
- iOS/Android application (Class B)
- Consumables (intranasal sleeves, electrode hydrogel tips, VNS clip pads)
- Accessories (zone modules, cervical VNS module, mastoid LRA pad)
- Cloud services (T2 HIPAA cloud, FHIR R4 interface)

Risk management covers the full device lifecycle: concept → design → development → production → post-market → end of life.

---

## 3. Responsibilities

| Role | Risk management responsibility |
|---|---|
| Quality Lead (interim: CEO) | Risk Management Plan owner; final residual risk acceptability decisions; signs Risk Management Report |
| VP Engineering (open) | Technical risk identification; mitigation implementation oversight |
| Lead Hardware Engineer (open) | Hardware hazard identification and mitigation design |
| Lead Firmware Engineer (open) | Software-related hazard identification per IEC 62304 §7.1 |
| Regulatory Counsel (external) | Regulatory risk inputs; applicable standards review |
| Clinical Advisor (SAB, future) | Clinical hazard identification for T2 modalities |

---

## 4. Risk Acceptability Criteria

### 4.1 Severity classification

| Severity level | Definition | Examples for NeuroPulse |
|---|---|---|
| S1 — Negligible | No injury or discomfort expected | Minor LED colour inconsistency, cosmetic scratching |
| S2 — Minor | Temporary, reversible discomfort or minor injury | Mild scalp irritation from electrode contact, temporary tinnitus from bone conduction |
| S3 — Moderate | Reversible injury requiring medical attention | Skin burn from thermal runaway, transient seizure from photostimulation |
| S4 — Serious | Irreversible injury or serious harm | Sustained seizure, significant burns, permanent hearing damage |
| S5 — Critical | Life-threatening or fatal | Cardiac arrhythmia from cervical VNS, fire from battery thermal runaway |

### 4.2 Probability classification

| Probability level | Definition | Approximate frequency |
|---|---|---|
| P1 — Improbable | Unlikely in device lifetime | < 1 in 10,000 devices |
| P2 — Remote | Could occur rarely | 1 in 1,000 – 10,000 devices |
| P3 — Occasional | Could occur in some devices | 1 in 100 – 1,000 devices |
| P4 — Probable | Likely in some devices | 1 in 10 – 100 devices |
| P5 — Frequent | Expected to occur regularly | > 1 in 10 devices |

### 4.3 Risk acceptability matrix

|  | S1 Negligible | S2 Minor | S3 Moderate | S4 Serious | S5 Critical |
|---|---|---|---|---|---|
| **P5 Frequent** | ALARP | ALARP | UNACCEPTABLE | UNACCEPTABLE | UNACCEPTABLE |
| **P4 Probable** | ACCEPTABLE | ALARP | ALARP | UNACCEPTABLE | UNACCEPTABLE |
| **P3 Occasional** | ACCEPTABLE | ACCEPTABLE | ALARP | ALARP | UNACCEPTABLE |
| **P2 Remote** | ACCEPTABLE | ACCEPTABLE | ACCEPTABLE | ALARP | ALARP |
| **P1 Improbable** | ACCEPTABLE | ACCEPTABLE | ACCEPTABLE | ACCEPTABLE | ALARP |

**ACCEPTABLE:** No further risk reduction required. Document rationale.  
**ALARP:** As Low As Reasonably Practicable — risk reduction measures required unless benefit clearly outweighs residual risk. Document ALARP justification.  
**UNACCEPTABLE:** Design must be modified. Device cannot be released with this risk rating.

### 4.4 ALARP justification requirement

For any risk rated ALARP, the risk file must document:
1. All technically feasible risk control options considered
2. Why selected controls are implemented
3. Why further reduction was not reasonably practicable (cost, technical feasibility, clinical benefit trade-off)
4. The residual risk after controls
5. Confirmation that benefits of use outweigh residual risk

---

## 5. Risk Management Process

The NeuroPulse risk management process follows ISO 14971:2019 clauses 4–9:

```
┌─────────────────────────────────────────────────────────────┐
│  RISK MANAGEMENT PLANNING (§4)                              │
│  This document — NP-RM-001                                  │
└─────────────────────────┬───────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────┐
│  HAZARD IDENTIFICATION (§5)                                 │
│  Intended use, reasonably foreseeable misuse, hazard lists  │
└─────────────────────────┬───────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────┐
│  RISK ESTIMATION (§5)                                       │
│  Severity × Probability → Risk level                        │
└─────────────────────────┬───────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────┐
│  RISK EVALUATION (§6)                                       │
│  Compare to acceptability criteria                          │
└─────────────────────────┬───────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────┐
│  RISK CONTROL (§7)                                          │
│  Mitigations: design → protective measures → information    │
└─────────────────────────┬───────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────┐
│  OVERALL RESIDUAL RISK EVALUATION (§8)                      │
│  Benefit-risk analysis; overall residual risk acceptable?   │
└─────────────────────────┬───────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────┐
│  RISK MANAGEMENT REVIEW (§9)                                │
│  Completeness check; risk management report                 │
└─────────────────────────┬───────────────────────────────────┘
                          │
┌─────────────────────────▼───────────────────────────────────┐
│  PRODUCTION + POST-MARKET SURVEILLANCE (§10)                │
│  Fleet SHDR monitoring; complaint analysis; PMS plan        │
└─────────────────────────────────────────────────────────────┘
```

---

## 6. Intended Use and Intended Users

### 6.1 Intended use (T1 — NeuroPulse Home)

NeuroPulse Home is a general wellness device intended to support user wellbeing through multi-modal neurostimulation and neurofeedback. Intended users are adults (18+) seeking wellness support for focus, relaxation, sleep, and general cognitive function. The device is intended for home use by lay users without clinical training.

**Not intended for:** Diagnosis, treatment, or monitoring of any medical condition; paediatric users (<18); users with active implanted electronic devices; users with known seizure disorders (photoparoxysmal EEG detection required before visual stimulation use); users who are pregnant.

### 6.2 Intended use (T2 — NeuroPulse Pro)

NeuroPulse Pro is a prescription-use medical device intended for use under clinician supervision as an adjunctive treatment for neurological and psychiatric conditions. T2 indications will be defined per modality in the 510(k) submission. Intended users: trained clinicians and their patients under clinical supervision.

### 6.3 Reasonably foreseeable misuse

The following misuse scenarios must be addressed in hazard identification:
- Using device while driving or operating heavy machinery
- Ignoring electrode impedance warnings and using with compromised contact
- Circumventing the 3-session tDCS daily limit via device reset
- Using intranasal probe without hygiene sleeve
- Attempting to modify firmware to exceed stimulation limits
- Using cervical VNS without clinician qualification (T2)
- Charging with non-PD-compliant charger

---

## 7. Hazard Categories

The following hazard categories apply to NeuroPulse. Each category must be systematically evaluated during design review:

| Category | Hazard sources | Primary interlock |
|---|---|---|
| Electrical stimulation | Excessive current/charge density; charge imbalance; electrode loss of contact | Safety MCU hardware GPIO; 40µC/cm² hardware limit; impedance monitoring |
| Optical / photobiological | Retinal exposure (visual LEDs, Mode F NIR); skin thermal injury (PBM scalp); IEC 62471 MPE | Hardware current limit; Hall sensor goggle lift cutoff; IR proximity; NTC thermal throttle |
| Thermal | Scalp burn from PBM zone; battery / supercapacitor thermal runaway | NTC per zone → hardware current throttle at 62°C junction; IEC 60601-1 42°C limit |
| Electromagnetic | EMF exposure from device operation; interference with external pacemakers/ICDs | 5-layer passive + active Helmholtz shielding; contraindication labelling |
| Mechanical | Zone module ejection during use; Boa dial failure; eye goggle impact | Snap-fit detent on eject lever; Boa 50,000-cycle rating; clamshell case |
| Biological / chemical | Skin sensitisation from silicone/PDMS; infection risk from shared intranasal probe | ISO 10993 biocompatibility; single-use hygiene sleeves; no shared electrode tips |
| Software / cybersecurity | Malicious session protocol; unauthorised firmware; UHDR data breach | Ed25519 session signing; Ed25519 OTA firmware verification; biometric-derived UHDR key |
| Usability | Incorrect zone module insertion; confusion between modalities; failure to notice thermal warning | Five-layer keying (RISK-15); bone conduction audio alerts; non-dismissible safety alerts |
| Cardiac (T2 cervical VNS) | Vasovagal response; cardiac arrhythmia from carotid stimulation | Safety MCU cardiac monitor; HR change >15 BPM → GPIO cutoff <100ms (RISK-25) |

---

## 8. Existing Risk Register — QMS Formalisation

### 8.1 Baseline risk register

The risk register RISK-01 through RISK-25, documented in NP-RISK-001 Rev A (`docs/neuropulse_fpc_zone_module_risks_revA.docx`), is hereby formally placed under QMS change control effective 2026-05-13. This constitutes the baseline ISO 14971 risk file.

**Current status at QMS establishment:**
- 23 risks: **MITIGATED** (residual risk documented with controls in NP-RISK-001)
- 2 risks: **OPEN** — require external action before closure:
  - **RISK-03:** PBM irradiance regulatory opinion — not yet obtained. Scope covers: 660/808nm at 400 mW/cm² peak pulsed (original scope, Issue #5); 1064nm irradiance; three-channel aggregate irradiance ceiling (600 mW/cm²); T2 combined 1064nm+1170nm session; depth-tier penetration claim (expanded scope per NP-REG-PBM1064-001 Rev A, Issue #56). *Blocking for all public marketing claims involving irradiance or depth claims.* Owner: CEO. Action: engage regulatory counsel with NP-REG-PBM1064-001 Rev A as the expanded instruction brief (12 questions for counsel, §8 of that document).
  - **RISK-20:** CFRP shell slot rim Ra ≤ 1.6 µm — written supplier confirmation not yet received. *Blocking for tooling release.* Owner: VP Engineering (open). Action: obtain letter + coupon measurement data from CFRP shell supplier.
- 1 risk: **MITIGATED — HARDWARE BENCH PENDING:**
  - **RISK-25:** Cervical VNS cardiac reflex. Safety MCU firmware specification complete (NP-FW-CVNS-001 Rev A). FAI-CV02 hardware timing bench required to confirm <100ms cutoff. Remains software-baselined; full closure requires T2 prototype bench.

### 8.2 Risk register change control

All future changes to the risk register (new risks, revised severity/probability, mitigation updates, risk closure) must follow the change order process in NP-QMS-DC-001 §8.1. Each risk register update must:
1. Reference the triggering event (design change, audit finding, complaint, post-market data)
2. Document the pre-change and post-change risk rating
3. Be approved by the Quality Lead

### 8.3 New risk identification triggers

New risks must be evaluated when:
- A significant design change is approved (NP-QMS-DC-001 §8.1)
- A new modality or accessory is added to the device
- A supplier process change is notified
- A post-market complaint or adverse event is received
- A new applicable standard or regulatory guidance is issued
- An internal audit finding identifies an unaddressed hazard

---

## 9. Risk Control Hierarchy

Per ISO 14971:2019 §7.4 and 21 CFR §820.30, risk controls shall be applied in this priority order:

1. **Inherently safe design** — eliminate the hazard or reduce risk by design (preferred)
2. **Protective measures in the device or manufacturing process** — guards, interlocks, hardware limits
3. **Information for safety** — labelling, IFU warnings, on-device alerts (last resort, least effective)

NeuroPulse design philosophy prioritises level 1 and 2 controls. Examples:
- Hardware-enforced 40µC/cm² charge density limit (Safety MCU GPIO, not software) — level 1/2
- Hall sensor goggle lift cutoff at hardware level — level 2
- Ed25519 signed session protocols (safety MCU rejects unsigned protocols) — level 2
- "Not for use by persons with active implanted cardiac devices" — level 3 (supplementary only)

---

## 10. Combination Product and Software Risk Considerations

### 10.1 Software-related hazards (per IEC 62304 §7.1)

Software failure modes that could cause hazards shall be identified and documented in each firmware specification. The Safety MCU (Class C) must document all software failure modes and their consequences at the unit level.

Currently documented software safety requirements:
- SPI heartbeat loss → all-stimulation cutoff within 50ms (NP-FW-CVNS-001, NP-FW-EMMC-001)
- Safety MCU watchdog 1.5s → all-stimulation cutoff within 50ms
- HR change >15 BPM → cervical VNS GPIO cutoff within <5.1ms worst-case (NP-FW-CVNS-001)
- Photoparoxysmal EEG detection → goggle LED cutoff within <200ms
- 25% duty cycle ceiling enforced unconditionally for all PBM channels (NP-FW-PBM1064-001)
- OTA firmware requires Ed25519 signature; unsigned firmware rejected at bootloader (NP-FW-EMMC-001)

### 10.2 Cybersecurity risk

Per FDA Cybersecurity Guidance for medical devices (2023), cybersecurity risks must be managed within the risk management framework. Key cybersecurity risk items:
- Malicious session protocol injection (mitigated: Ed25519 signing)
- Unauthorised OTA firmware update (mitigated: Ed25519 verification at bootloader)
- UHDR data exfiltration (mitigated: biometric-derived AES-256-XTS key, never held by NeuroPulse)
- USB-C DFU exploitation (mitigated: Ed25519 gating on DFU path)

SBOM requirement: Software Bill of Materials required for 510(k) submission. To be generated from firmware build system (planned: NP-SBOM-001, Year 2).

---

## 11. Overall Residual Risk Evaluation

### 11.1 Pre-release requirement

Before T2 510(k) submission, a formal Overall Residual Risk Evaluation (per ISO 14971:2019 §8) must be performed, documenting:
1. All residual risks remaining after all controls are implemented
2. An aggregate benefit-risk analysis demonstrating that the overall residual risk is acceptable given the clinical benefits
3. Comparison to other marketed devices with similar indications (Vielight, Neuronic, NeuroStar TMS, electroCore)

### 11.2 Current overall risk rating

At QMS establishment (pre-prototype), a formal overall residual risk evaluation cannot yet be completed. The following conditions must be met before evaluation:
- All OPEN risks (RISK-03, RISK-20) must be closed
- All ALARP risks must have documented ALARP justification
- Hardware FAI bench tests must be completed to validate mitigation effectiveness
- Human factors summative testing must confirm that usability risks are adequately mitigated

---

## 12. Post-Market Risk Management

### 12.1 Feedback loops

Post-launch, the following data sources feed back into the risk management file:

| Source | Data type | Risk management action |
|---|---|---|
| Fleet SHDR | LED degradation, thermal profiles, EMF attenuation, impact events | Update probability estimates; trigger CAPA if trend indicates hazard |
| Complaint data | User-reported adverse events, near-misses | Re-evaluate severity and probability; update risk register |
| NP-PMS-001 (planned) | Post-market surveillance plan outputs | Systematic trend analysis; annual risk review |
| Regulatory recalls / field safety notices (industry) | Competitor device failures; new hazard types | Evaluate applicability to NeuroPulse; update hazard identification |

### 12.2 Post-market risk review triggers

An unscheduled risk management review shall be triggered by:
- Any serious adverse event (injury or death) associated with the device
- Any field safety corrective action or recall
- New clinical evidence indicating an unrecognised hazard
- Regulatory authority request

---

## 13. Risk Management Report

A Risk Management Report (per ISO 14971:2019 §9) shall be authored and approved before T2 510(k) submission. The report will:
1. Confirm that the risk management plan (this document) was followed
2. Confirm that all identified risks have been evaluated and controlled
3. Document the overall residual risk evaluation and benefit-risk conclusion
4. Be signed by the Quality Lead and VP Engineering

**Target date:** Month 18 (concurrent with 510(k) pre-submission preparation)

---

## 14. Document History

| Rev | Date | Author | Description |
|---|---|---|---|
| A | 2026-05-13 | Interim Quality (CEO) | Initial release. Risk Management Plan established at QMS formation. RISK-01 through RISK-25 formally entered under QMS change control. |

---

*NP-RM-001 Rev A — ACTIVE — Effective 2026-05-13*
