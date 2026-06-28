# RISK-03 Regulatory Opinion — Scope Expansion Brief
## 1064nm PBM · Three-Channel Aggregate Irradiance · T2 Combined Session

**Project:** NeuroPulse  
**Document:** NP-REG-PBM1064-001  
**Revision:** A  
**Date:** 2026-05-13  
**Status:** DRAFT  
**Effective Date:** 2026-05-13  
**Author:** NeuroPulse Regulatory Affairs  
**Approved By:** Steve Hickman, CEO  
**References:** CLAUDE.md §13.1 (RISK-03 OPEN); NP-FW-PBM1064-001 Rev A §5.4; NP-SES-1064-001 Rev A  
**Related Issues:** GitHub Issue #5 (existing RISK-03 engagement), GitHub Issue #56  
**Gate:** —  
**IEC 62304 Class:** —  
**Prepared For:** Outside regulatory counsel (PBM/digital health specialist)  
**Applicable Standard:** IEC 62471, FTC Act §5  

---

## 1. Purpose

This document extends the scope of the existing RISK-03 regulatory opinion engagement (Issue #5) to cover three new items arising from the addition of the 1064nm smart zone module to the NeuroPulse Home and NeuroPulse Pro platforms. The existing RISK-03 engagement covers the 400 mW/cm² peak pulsed irradiance claim for the 660nm + 808–830nm LED channels. No 1064nm-specific regulatory opinion has been obtained.

**This document is a scope expansion brief to be provided to outside regulatory counsel.** It states the four new questions that must be answered in the expanded opinion letter and provides the technical background for each. The marketing copy gate described in §7 remains in force for all 1064nm claims until the expanded opinion letter is received.

Coordinate with the existing RISK-03 counsel engagement (Issue #5). Do not initiate a parallel engagement; add this scope to the existing instruction if feasible.

---

## 2. Background — Existing RISK-03 Scope

The original RISK-03 regulatory opinion (Issue #5, status: OPEN — external) covers:

- 660nm LEDs at up to 400 mW/cm² peak pulsed (≤ 25% duty cycle, firmware-enforced)
- 808–830nm LEDs at up to 400 mW/cm² peak pulsed
- FDA general wellness device pathway applicability for T1 transcranial PBM
- IEC 62471 photobiological hazard classification for both wavelengths individually
- FTC claims substantiation for the 400 mW/cm² peak pulsed claim as a marketing figure

The existing opinion does not address any 1064nm wavelength, any multi-channel aggregate irradiance, or the T2 1170nm laser system.

---

## 3. New Scope Item 1 — 1064nm LED Irradiance: FDA General Wellness Pathway

### 3.1 Technical Description

The NeuroPulse Home T1 1064nm smart zone module uses EPITEX L1064-02AU LED emitters (150 per zone module, 5 zone modules) operated via an on-module Microchip ATtiny402 I2C driver. Operating parameters:

| Parameter | Value |
|-----------|-------|
| Emitter type | LED array (not laser diode) |
| Wavelength | 1064 nm (±15 nm FWHM) |
| LED count per zone | 150 |
| Current per LED | ≤ 150 mA |
| Peak pulsed irradiance per zone | ≤ 400 mW/cm² (≤ 25% duty cycle, firmware ceiling DUTY_MAX_REG = 0x32) |
| CW irradiance (Vascular Baseline preset) | ≤ 200 mW/cm² |
| Application site | Transcranial scalp surface |
| Application duration | 10–30 min per session |

The existing RISK-03 opinion addresses 660nm and 808–830nm LEDs under the FDA general wellness device pathway. The 1064nm emitters are LED arrays (not classified as laser devices under 21 CFR Part 1040). However, NeuroPulse requires counsel confirmation that:

1. 1064nm LED arrays applied transcranially at the above parameters remain within the FDA general wellness pathway for T1.
2. Adding 1064nm does not alter the predicate analysis or the planned T2 510(k) predicate strategy.
3. No product code change, IDE requirement, or additional 510(k) predicate is triggered solely by the addition of 1064nm LEDs (all other parameters unchanged from the existing 660/808nm design).

### 3.2 IEC 62471 Context

IEC 62471:2006 classifies photobiological hazards of lamps and lamp systems. The relevant hazard groups for 1064nm are:

- **EH2 (Near-Infrared Radiation Hazard to the Eye):** Action spectrum covers 780–3000nm. The 1064nm irradiance from each zone module must be assessed against the EH2 MPE limit.
- **EH1 (Thermal Hazard to the Eye):** Applies to retinal thermal exposure; relevant if any 1064nm irradiance reaches the eye aperture. The NeuroPulse goggle lens system provides hardware cutoff (Hall sensor + IR proximity sensor per §4.2 of CLAUDE.md) preventing scalp-module operation with goggles removed during goggle sessions, but scalp modules can operate independently.

**Question for counsel:** At 400 mW/cm² peak pulsed (25% duty cycle, 100 mW/cm² average) at 1064nm applied to the scalp vertex/frontal zone, does the EH2 or EH1 IEC 62471 hazard classification change vs the 660/808nm assessment already obtained? Is a new IEC 62471 group classification required, or does the existing Exempt Group assessment extend to 1064nm at these parameters?

---

## 4. New Scope Item 2 — Three-Channel Aggregate Irradiance Ceiling (600 mW/cm²)

### 4.1 Technical Description

When a 1064nm smart zone module is inserted, the hub firmware monitors aggregate irradiance across all three channels:

```
P_aggregate = P_CH_A (660nm) + P_CH_B (808nm) + P_CH_C (1064nm)
```

The firmware ceiling is: `PBM_AGGREGATE_IRRADIANCE_LIMIT_MW_CM2 = 600` (Config partition, `np_pbm1064_dose.c` §6.4). This value is the trigger for the proportional throttle cascade (CH_C first → CH_B → CH_A). It is not a marketed peak claim; it is a safety governor.

**Example maximum simultaneous values (Gamma Clarity preset, all channels at 25% duty):**

| Channel | Peak pulsed irradiance | Average irradiance |
|---------|------------------------|-------------------|
| CH_A (660nm) | 400 mW/cm² | 100 mW/cm² |
| CH_B (808nm) | 400 mW/cm² | 100 mW/cm² |
| CH_C (1064nm) | 400 mW/cm² | 100 mW/cm² |
| **Aggregate peak pulsed** | **1,200 mW/cm²** | — |
| **Aggregate average** | — | **300 mW/cm²** |

The throttle ceiling of 600 mW/cm² applies to aggregate **peak pulsed** irradiance (matching the duty-cycle convention of the existing 400 mW/cm² single-channel claim). At 25% duty, average aggregate irradiance at the throttle point would be 150 mW/cm².

### 4.2 Marketing and Public Material Implications

The 600 mW/cm² aggregate ceiling value may appear in:
- App display: real-time three-channel irradiance bar graph (user's own device — not a marketing claim)
- Product specifications page: "aggregate irradiance ceiling: 600 mW/cm²"
- Investor materials: "three-channel aggregate safety governor at 600 mW/cm²"

App display to the user on their own device of real-time irradiance values is not a marketing claim and is not gated by this regulatory opinion (per Issue #56 §3 policy). However, any use of the 600 mW/cm² figure in public-facing marketing, product specifications, or investor materials is gated until the opinion is received.

### 4.3 Questions for Counsel

1. **IEC 62471 aggregate assessment:** The IEC 62471:2006 standard assesses photobiological hazards per-wavelength using additive effective irradiance weighted by the applicable action spectra. For the three wavelengths operating simultaneously (660nm, 808nm, 1064nm), must an additive aggregate assessment be performed per IEC 62471 Clause 4.3 (additive evaluation), or does the per-wavelength MPE assessment already obtained for 660/808nm + the new 1064nm assessment constitute a complete photobiological hazard evaluation?

2. **Classification change risk:** Does operating three wavelengths simultaneously at these parameters create a risk that the aggregate assessment moves the device from Exempt Group to Risk Group 1, 2, or 3 under IEC 62471? If yes, what parameters must be constrained to maintain Exempt Group?

3. **FTC substantiation:** If the aggregate irradiance ceiling (600 mW/cm²) appears in marketing as a safety specification or performance claim, is it substantiated by (a) the firmware-enforced ceiling alone, (b) the IEC 62471 assessment, or (c) both? What minimum substantiation is required for this figure under FTC guidelines?

4. **Aggregate ceiling value confirmation:** Should `PBM_AGGREGATE_IRRADIANCE_LIMIT_MW_CM2` remain at 600 mW/cm², be increased to allow maximum simultaneous operation (1,200 mW/cm² peak pulsed), or be reduced to ensure Exempt Group classification? Counsel's opinion on the appropriate value given the regulatory picture is requested.

---

## 5. New Scope Item 3 — T2 Combined 1064nm + 1170nm Simultaneous Session

### 5.1 Technical Description

The NeuroPulse Pro (T2) combined session coordinates 1064nm LED zone modules (LED array, T1 hardware in T2 configuration) with the T2 1170nm laser diode deep PBM subsystem. Parameters:

| Subsystem | Technology | Wavelength | Peak irradiance | Penetration target |
|-----------|-----------|------------|-----------------|-------------------|
| 1064nm modules | LED array | 1064 nm | ≤ 400 mW/cm² pulsed | Cortical (~30mm) |
| 1170nm deep PBM | Laser diode | 1170 nm | ≤ 1,000 mW/cm² | Subcortical (~40mm) |

The 1170nm subsystem uses laser diodes. These are governed by IEC 60825-1 (Laser Safety) rather than IEC 62471 (Lamp Safety). Operating both simultaneously in the same session creates a combined exposure from two distinct regulatory frameworks.

### 5.2 Questions for Counsel

1. **Dual-standard exposure:** When a patient simultaneously receives 1064nm LED irradiance (assessed under IEC 62471) and 1170nm laser irradiance (assessed under IEC 60825-1), must a combined photobiological hazard assessment be performed? If yes, under which standard, or is a custom combined assessment required per IEC TR 60825-14?

2. **510(k) predicate impact:** The T2 1170nm laser deep PBM subsystem is expected to require 510(k) clearance as part of the T2 regulatory package. Does operating 1064nm LEDs simultaneously with a cleared 1170nm laser device require a separate or supplementary 510(k) covering the combined use, or is the combined session governed by the T2 510(k) for the primary modality (TMS or 1170nm)?

3. **IEC 60601-2-57 applicability:** IEC 60601-2-57:2011 (particular requirements for therapeutic light source equipment) may be the more relevant standard for the 1170nm laser subsystem. Does adding 1064nm LED operation during the same session trigger any new IEC 60601-2-57 assessment requirements?

4. **T2 labelling requirements:** Must the T2 combined session capability appear in device labelling differently from the individual modality claims? Specifically: if we claim "three-tier transcranial photobiomodulation (660nm/1064nm cortical + 1170nm subcortical)" as a T2 feature, does this constitute a new intended use or a combination of separately cleared intended uses?

---

## 6. New Scope Item 4 — Three-Wavelength Depth-Tier Penetration Claim

### 6.1 Claim Text (Proposed)

> "Three independent penetration depths, independently dosed and metered: 660nm surface cortex · 1064nm mid-cortex · 1170nm deep subcortical"

> "Deeper than 810nm: NeuroPulse's 1064nm channel reaches cortical tissue layers at depths not accessible to 810nm devices"

### 6.2 Evidence Basis

The depth-tier penetration claim rests on biophysical tissue optics data:

- **660nm:** In biological tissue, optical penetration is limited by melanin and oxyhemoglobin absorption. Effective penetration depth ~8–12mm (superficial cortex).
- **1064nm:** Falls in the NIR "optical window" (900–1100nm) where water absorption is minimal and oxyhemoglobin/deoxyhemoglobin absorption is relatively low. Published tissue optics studies indicate effective penetration depth ~25–35mm (mid-cortex / deep cortex).
- **1170nm:** Between water absorption bands (970nm and 1450nm peaks), with deeper penetration than 1064nm. Published data: ~35–40mm effective penetration depth for neural tissue. Used in T2 subcortical targeting.

The primary human clinical evidence for 1064nm transcranial effects at cortical depth is Yao et al. (2022), Science Advances (DOI: 10.1126/sciadv.abj7390; see bibliography addendum NP-BIB-1064-001 Rev A).

### 6.3 Questions for Counsel

1. **FTC substantiation — depth claim:** Under FTC guidelines (16 CFR Part 255; FTC Guides Concerning Use of Endorsements and Testimonials; 2022 Enforcement Policy Statement on Deceptive or Unfair Health Claims), is the "deeper penetration at 1064nm vs 810nm" claim adequately substantiated by tissue optics studies and the Yao et al. (2022) human RCT? What minimum evidence standard applies to a depth-of-penetration claim for a general wellness device?

2. **FTC substantiation — three-tier claim:** Is the "three independent penetration depths, independently dosed and metered" claim (as a system performance claim, not a clinical outcomes claim) substantiated by the combination of biophysical tissue optics data + real-time dose metering hardware + the Yao et al. evidence? Or does this claim require a prospective human study comparing all three simultaneously?

3. **Implied clinical outcome risk:** Does the depth-tier claim, in context of the marketing materials, create an implied clinical outcomes claim that exceeds what a general wellness device may claim? If yes, what language revision mitigates this risk?

---

## 7. Marketing Copy Gate (Pending Opinion Receipt)

Until counsel provides the expanded opinion letter covering all four scope items above, the following rules apply:

| Item | Gate status |
|------|-------------|
| 1064nm irradiance value (e.g., "400 mW/cm² at 1064nm") in marketing | **GATED** — Issue #56 |
| Three-channel aggregate irradiance value (600 mW/cm²) in marketing | **GATED** — Issue #56 |
| "Deeper than 810nm" or "three-tier penetration" claims | **GATED** — Issue #56 |
| T2 combined 1064+1170nm session claims in marketing | **GATED** — Issue #56 |
| Real-time irradiance/dose display to user in app on their own device | **NOT gated** — internal device display, not marketing claim |
| Existing 660/808nm irradiance claim (400 mW/cm²) | **Gated separately** — existing RISK-03 (Issue #5) |

These gates are tracked in CLAUDE.md §13.1 and §13.4. They are lifted independently; receipt of the expanded opinion may lift some or all 1064nm gates while Issue #5 remains open for the existing 660/808nm gate.

---

## 8. Deliverables Requested from Counsel

The expanded opinion letter should address the following questions (consolidated from §§3–6 above):

| Q# | Topic | Scope item |
|----|-------|-----------|
| Q1 | 1064nm LED array — FDA general wellness pathway confirmation | §3 |
| Q2 | 1064nm at 400 mW/cm² pulsed — IEC 62471 EH1/EH2 hazard group classification | §3 |
| Q3 | Effect of 1064nm addition on T1 general wellness classification and T2 predicate strategy | §3 |
| Q4 | IEC 62471 additive assessment requirement for three simultaneous wavelengths | §4 |
| Q5 | Aggregate irradiance classification change risk; appropriate value for 600 mW/cm² ceiling | §4 |
| Q6 | FTC substantiation requirements for aggregate irradiance ceiling in marketing | §4 |
| Q7 | Combined 1064nm LED + 1170nm laser simultaneous session: photobiological hazard framework | §5 |
| Q8 | T2 combined session: 510(k) coverage and IEC 60601-2-57 applicability | §5 |
| Q9 | T2 combined session labelling: new intended use vs combination of existing | §5 |
| Q10 | FTC substantiation: "deeper penetration at 1064nm" depth claim | §6 |
| Q11 | FTC substantiation: "three independent penetration depths" system performance claim | §6 |
| Q12 | Implied clinical outcome risk in depth-tier marketing language | §6 |

**Format required:** Written opinion letter on counsel letterhead; question-by-question responses; explicit statement of applicable standard or statute for each answer; statement of any material limitations or assumptions in the opinion.

**Timeline requested:** 4–6 weeks from engagement expansion. This is not a first-priority item relative to the existing RISK-03 opinion (Issue #5); coordinate with the existing engagement to determine feasibility of including these items in the same letter.

**Estimated incremental cost:** $5,000–10,000 incremental to the existing RISK-03 engagement scope (counsel assessment required).

---

## 9. RISK-03 Status Impact

Upon receipt of the expanded opinion letter:

| Risk register item | New status |
|--------------------|-----------|
| RISK-03 (existing — 400 mW/cm² 660/808nm) | Status unchanged; governed by Issue #5 |
| RISK-03 extended: 1064nm irradiance | MITIGATED (pending letter receipt) |
| RISK-03 extended: aggregate irradiance ceiling | MITIGATED (pending letter receipt) |
| RISK-03 extended: T2 combined 1064+1170nm | MITIGATED (pending letter receipt) |
| RISK-03 extended: depth-tier penetration claim | MITIGATED (pending letter receipt) |
| OI-PBM-05 (`PBM_AGGREGATE_IRRADIANCE_LIMIT_MW_CM2`) | CLOSED — value confirmed or revised per Q5/Q6 |

Update CLAUDE.md §13.4 and the risk register document (`docs/neuropulse_fpc_zone_module_risks_revA.docx`) when opinion is received.

---

## 10. References

| Document | Relevance |
|----------|-----------|
| NP-FW-PBM1064-001 Rev A §5.4 | OI-PBM-05: aggregate irradiance ceiling firmware spec |
| NP-SES-1064-001 Rev A | Session presets and dose limits by wavelength |
| NP-HW-FPC-001 Rev E | 1064nm smart module FPC and LED emitter spec |
| `docs/np_bib_1064_001.md` (NP-BIB-1064-001 Rev A) | 1064nm evidence bibliography — supports Q10/Q11 |
| IEC 62471:2006 | Photobiological safety of lamps — assessment standard |
| IEC TR 62778:2014 | Application of IEC 62471 for blue light hazard (reference) |
| IEC 60825-1:2014 | Safety of laser products — T2 1170nm subsystem |
| IEC TR 60825-14:2004 | User's guide for IEC 60825-1 |
| IEC 60601-2-57:2011 | Therapeutic light source equipment — T2 relevance |
| 21 CFR Part 1040 | Performance standards for light-emitting products |
| FTC 2022 Enforcement Policy Statement on Health Claims | FTC substantiation framework |
