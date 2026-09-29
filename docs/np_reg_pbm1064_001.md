# RISK-03 Regulatory Opinion — Scope Expansion Brief
## 1064nm PBM · Three-Channel Aggregate Irradiance · T2 Combined Session

**Project:** NeurOne  
**Document:** NP-REG-PBM1064-001  
**Revision:** 2
**Date:** 2026-09-28  
**Status:** DRAFT  
**Effective Date:** 2026-05-13  
**Author:** NeurOne Regulatory Affairs  
**Approved By:** Steve Hickman, CEO  
**References:** docs/status/pending-decisions.md §13.1, §13.1a (RISK-03 OPEN); NP-FW-PBM1064-001 Rev 1 §5.4; NP-SES-1064-001 Rev 1; **NP-BIB-PBMIRR-001 Rev 1** (irradiance and temperature evidence record, Rev 2); NP-SES-PWR-001 `OI-SESPWR-02`; NP-HW-HEXTILE-001 §2 (R-4, R-5)  
**Related Issues:** GitHub Issue #5 (existing RISK-03 engagement), GitHub Issue #56  
**Gate:** —  
**IEC 62304 Class:** —  
**Prepared For:** Outside regulatory counsel (PBM/digital health specialist)  
**Applicable Standard:** IEC 62471, IEC 60825-1, IEC 60601-1, IEC 60601-2-57, ICNIRP 2013, FTC Act §5  

---

## 1. Purpose

This document extends the scope of the existing RISK-03 regulatory opinion engagement (Issue #5) to cover three new items arising from the addition of the 1064nm smart zone module to the NeurOne Home and NeurOne Pro platforms. The existing RISK-03 engagement covers the 400 mW/cm² peak pulsed irradiance claim for the 660nm + 808–830nm LED channels. No 1064nm-specific regulatory opinion has been obtained.

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

The NeurOne Home T1 1064nm smart zone module uses EPITEX L1064-02AU LED emitters (150 per zone module, 5 zone modules) operated via an on-module Microchip ATtiny402 I2C driver. Operating parameters:

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

The existing RISK-03 opinion addresses 660nm and 808–830nm LEDs under the FDA general wellness device pathway. The 1064nm emitters are LED arrays (not classified as laser devices under 21 CFR Part 1040). However, NeurOne requires counsel confirmation that:

1. 1064nm LED arrays applied transcranially at the above parameters remain within the FDA general wellness pathway for T1.
2. Adding 1064nm does not alter the predicate analysis or the planned T2 510(k) predicate strategy.
3. No product code change, IDE requirement, or additional 510(k) predicate is triggered solely by the addition of 1064nm LEDs (all other parameters unchanged from the existing 660/808nm design).

### 3.2 IEC 62471 Context

IEC 62471:2006 classifies photobiological hazards of lamps and lamp systems. The relevant hazard groups for 1064nm are:

- **EH2 (Near-Infrared Radiation Hazard to the Eye):** Action spectrum covers 780–3000nm. The 1064nm irradiance from each zone module must be assessed against the EH2 MPE limit.
- **EH1 (Thermal Hazard to the Eye):** Applies to retinal thermal exposure; relevant if any 1064nm irradiance reaches the eye aperture. The NeurOne goggle lens system provides hardware cutoff (Hall sensor + IR proximity sensor per §4.2 of CLAUDE.md) preventing scalp-module operation with goggles removed during goggle sessions, but scalp modules can operate independently.

> **Correction, Rev 2 (2026-09-28).** Rev 1 of this section, and Q4 in §4.3, describe an IEC 62471
> assessment of the 660/808nm channels as *"already obtained"*, with an *"existing Exempt Group
> assessment"*. **No such assessment exists in the document set.** `docs/status/pending-decisions.md`
> §13.1a traced the chain and found no IEC 62471 derivation anywhere, and RISK-03 itself is recorded
> as *"not yet obtained"* (`NP-RISK-002`). The questions are kept as written so their numbering and
> history hold, but **counsel should read every reference to an existing 660/808nm assessment as a
> request that it be performed**, not as a premise. The wording is left in place as the record of
> what Rev 1 asserted (`NP-CONV-001` §7).

**Question for counsel:** At 400 mW/cm² peak pulsed (25% duty cycle, 100 mW/cm² average) at 1064nm applied to the scalp vertex/frontal zone, does the EH2 or EH1 IEC 62471 hazard classification change vs the 660/808nm assessment already obtained? Is a new IEC 62471 group classification required, or does the existing Exempt Group assessment extend to 1064nm at these parameters?

---

## 4. New Scope Item 2 — Three-Channel Aggregate Irradiance Ceiling (600 mW/cm²)

### 4.1 Technical Description

When a 1064nm smart zone module is inserted, the hub firmware monitors aggregate irradiance across all three channels:

```
P_aggregate = P_CH_A (660nm) + P_CH_B (808nm) + P_CH_C (1064nm)
```

The firmware ceiling is: `PBM_AGGREGATE_IRRADIANCE_LIMIT_MW_CM2 = 600` (Config partition, `np_pbm_dose.c` §6.4). This value is the trigger for the proportional throttle cascade (CH_C first → CH_B → CH_A). It is not a marketed peak claim; it is a safety governor.

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

The NeurOne Pro (T2) combined session coordinates 1064nm LED zone modules (LED array, T1 hardware in T2 configuration) with the T2 1170nm laser diode deep PBM subsystem. Parameters:

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

> "Deeper than 810nm: NeurOne's 1064nm channel reaches cortical tissue layers at depths not accessible to 810nm devices"

### 6.2 Evidence Basis

The depth-tier penetration claim rests on biophysical tissue optics data:

- **660nm:** In biological tissue, optical penetration is limited by melanin and oxyhemoglobin absorption. Effective penetration depth ~8–12mm (superficial cortex).
- **1064nm:** Falls in the NIR "optical window" (900–1100nm) where water absorption is minimal and oxyhemoglobin/deoxyhemoglobin absorption is relatively low. Published tissue optics studies indicate effective penetration depth ~25–35mm (mid-cortex / deep cortex).
- **1170nm:** Between water absorption bands (970nm and 1450nm peaks), with deeper penetration than 1064nm. Published data: ~35–40mm effective penetration depth for neural tissue. Used in T2 subcortical targeting.

The primary human clinical evidence for 1064nm transcranial effects at cortical depth is Yao et al. (2022), Science Advances (DOI: 10.1126/sciadv.abj7390; see bibliography addendum NP-BIB-1064-001 Rev 1).

### 6.3 Questions for Counsel

1. **FTC substantiation — depth claim:** Under FTC guidelines (16 CFR Part 255; FTC Guides Concerning Use of Endorsements and Testimonials; 2022 Enforcement Policy Statement on Deceptive or Unfair Health Claims), is the "deeper penetration at 1064nm vs 810nm" claim adequately substantiated by tissue optics studies and the Yao et al. (2022) human RCT? What minimum evidence standard applies to a depth-of-penetration claim for a general wellness device?

2. **FTC substantiation — three-tier claim:** Is the "three independent penetration depths, independently dosed and metered" claim (as a system performance claim, not a clinical outcomes claim) substantiated by the combination of biophysical tissue optics data + real-time dose metering hardware + the Yao et al. evidence? Or does this claim require a prospective human study comparing all three simultaneously?

3. **Implied clinical outcome risk:** Does the depth-tier claim, in context of the marketing materials, create an implied clinical outcomes claim that exceeds what a general wellness device may claim? If yes, what language revision mitigates this risk?

---

## 6A. New Scope Item 5 — What Sets the Irradiance and Temperature Boundary (added Rev 2)

### 6A.1 Why this is added

Items 1–4 ask counsel to confirm or classify NeurOne's chosen ceilings. None asks **what external
boundary, if any, those ceilings must sit under**. `NP-BIB-PBMIRR-001` Rev 1 reviewed the published
record for 600–1100 nm and found **no statute or regulation that sets a PBM irradiance limit**. What
exists is a definition (FDA: PBM is *"non-heating"*), exposure-limit standards written for laser and
lamp safety, and a contact-temperature standard. The figures, with their source quality, are in
that document's §2–§5. The ones this item turns on:

| Reference | Value | NeurOne figure it bears on |
|---|---|---|
| Laser skin limit, 10 s–30,000 s (ICNIRP 2013 / IEC 60825-1 / ANSI Z136.1) | 200 × C_A mW/cm² time-averaged: **200 at 660 nm**, ≈ 329 at 808 nm, **1000 at 1064 nm** | R-4 200 mW/cm² CW at 660 nm (**zero margin**); `07-vascular-baseline` ~322 mW/cm² CW (`OI-SESPWR-02`) |
| Same standards, multiple wavelengths | Assessed as a weighted sum of time-averaged irradiance, Σ Eᵢ/ELᵢ ≤ 1 | R-5, written as a sum of peaks (Q5) |
| IEC 62471 skin thermal | Limited for exposures under 10 s only; beyond that it relies on pain avoidance | A head-worn device, possibly during sleep |
| IEC 60601-1 (3rd ed.) Table 24 | **43 °C** for applied parts in contact ≥ 10 min; **labelling required above 41 °C** | CLAUDE.md §3's **42 °C**, attributed to IEC 60601 |
| Heating onset (Zein, Selting & Hamblin 2018) | ~300 mW/cm² at 600–700 nm; ~750 mW/cm² at 800–900 nm | R-4 peak; `OI-HEXTILE-20` two channels at 806 mW/cm² |
| Modelled scalp rise at 1064 nm (2025) | +0.38 °C at 100 mW/cm²; +3.76 °C at 1000 mW/cm²; asymptote ~10 min | 1064nm channel |
| Skin colour (2025) | ~3× heating / thermal-injury risk in darker skin, red-weighted | No skin-type term in any NeurOne model |

### 6A.2 Questions for Counsel

Q13 is reserved for Mode F retinal PBM (`NP-FW-EMMC-002` Rev 1 §F) and is **not** written in this
revision; the numbering below leaves it in place.

1. **(Q14) Wellness eligibility and "non-heating".** Does the FDA 2023 draft guidance's definition
   of PBM as light *"at an irradiance that does not induce heating"* bear on T1 as a general
   wellness product, and does the 2026 *General Wellness* revision's exclusion of *"technologies,
   like lasers or radiation, that could pose safety risks without regulatory controls"* reach an
   LED array at 400 mW/cm² peak / 200 mW/cm² CW on the scalp? What evidence of "non-heating" would
   FDA expect (skin-temperature rise, a bound, a test method)?
2. **(Q15) Governing exposure framework for an LED array worn > 10 s.** IEC 62471 sets no skin limit
   beyond 10 s. Should NeurOne assess scalp exposure against IEC 62471 alone, against the laser skin
   limit as a conservative reference, or under IEC 60601-2-57 (2023; EN IEC 60601-2-57:2026 with
   RG-1C) — and is the pain-avoidance assumption acceptable for a strapped-on device used during
   sleep?
3. **(Q16) The 660 nm CW margin.** R-4's 200 mW/cm² CW equals the laser skin limit at 660 nm. Is a
   ceiling with zero margin to the conventional reference defensible, and if `OI-SESPWR-02` finds
   no CW clamp, is `07-vascular-baseline` (~322 mW/cm² CW, `660_808nm`) a compliance defect under
   any applicable standard, not only against R-4?
4. **(Q17) The form of the aggregate ceiling.** The exposure standards assess multiple wavelengths as
   a weighted sum of **time-averaged** irradiance. R-5 is a sum of **peaks**. Should R-5 be expressed
   in the standards' form, and would that change the answer to Q5?
5. **(Q18) Temperature limit and skin type.** Which temperature limit governs a PBM applied part
   worn ≥ 10 min — IEC 60601-1 Table 24's 43 °C with its labelling duty above 41 °C, a particular
   standard (IEC 60601-2-57), or neither for a wellness product? Is CLAUDE.md §3's 42 °C correctly
   attributed? And does the published excess thermal risk in darker skin create a duty to derate,
   test across skin types, or label?

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

These gates are tracked in docs/status/pending-decisions.md §13.1 and §13.4. They are lifted independently; receipt of the expanded opinion may lift some or all 1064nm gates while Issue #5 remains open for the existing 660/808nm gate.

---

## 8. Deliverables Requested from Counsel

The expanded opinion letter should address the following questions (consolidated from §§3–6A above):

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
| Q13 | *Reserved* — Mode F retinal PBM (`NP-FW-EMMC-002` Rev 1 §F); not yet written | — |
| Q14 | FDA "non-heating" PBM definition and 2026 General Wellness exclusion applied to the T1 LED array | §6A |
| Q15 | Governing exposure framework for an LED array worn > 10 s (IEC 62471, laser skin limit, IEC 60601-2-57) | §6A |
| Q16 | 660nm CW ceiling at zero margin to the skin limit; `07-vascular-baseline` if no CW clamp exists | §6A |
| Q17 | Aggregate ceiling as a weighted sum of averages vs a sum of peaks | §6A |
| Q18 | Governing skin-contact temperature limit (43 °C / 41 °C labelling vs 42 °C) and skin-type duty | §6A |

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

Update docs/status/pending-decisions.md §13.4 and the risk register document (`docs/superseded/np_risk_001.docx`) when opinion is received.

---

## 10. References

| Document | Relevance |
|----------|-----------|
| NP-FW-PBM1064-001 Rev 1 §5.4 | OI-PBM-05: aggregate irradiance ceiling firmware spec |
| NP-SES-1064-001 Rev 1 | Session presets and dose limits by wavelength |
| NP-HW-FPC-001 Rev 5 | 1064nm smart module FPC and LED emitter spec |
| `docs/np_bib_1064_001.md` (NP-BIB-1064-001 Rev 1) | 1064nm evidence bibliography — supports Q10/Q11 |
| IEC 62471:2006 | Photobiological safety of lamps — assessment standard |
| IEC TR 62778:2014 | Application of IEC 62471 for blue light hazard (reference) |
| IEC 60825-1:2014 | Safety of laser products — T2 1170nm subsystem |
| IEC TR 60825-14:2004 | User's guide for IEC 60825-1 |
| IEC 60601-2-57:2011 | Therapeutic light source equipment — T2 relevance. **Rev 2:** superseded by the 2023 edition (EN IEC 60601-2-57:2026 adds RG-1C); relevant to T1 as well (Q15) |
| `docs/np_bib_pbmirr_001.md` (NP-BIB-PBMIRR-001 Rev 1) | Irradiance and temperature evidence record — supports Q14–Q18 |
| IEC 60601-1 (3rd ed.) Table 24 | Applied-part temperature limits — Q18 |
| ICNIRP 2013 laser and incoherent visible/IR guidelines; ANSI Z136.1 | Skin exposure limits — Q15–Q17 |
| FDA draft guidance, *PBM Devices — Premarket Notification [510(k)] Submissions* (2023-01) | "Non-heating" definition — Q14 |
| FDA *General Wellness: Policy for Low Risk Devices* (revised 2026-01-06) | Wellness eligibility — Q14 |
| 21 CFR Part 1040 | Performance standards for light-emitting products |
| FTC 2022 Enforcement Policy Statement on Health Claims | FTC substantiation framework |

---

## 11. Revision History

| Rev | Date | Author | Change |
|---|---|---|---|
| 1 | 2026-05-13 | NeurOne Regulatory Affairs | First issue: scope items 1–4, Q1–Q12 |
| 2 | 2026-09-28 | NeurOne Regulatory Affairs | Adds scope item 5 (§6A, Q14–Q18): what external irradiance and temperature boundary the PBM ceilings must sit under, from `NP-BIB-PBMIRR-001` Rev 1. Adds a correction banner to §3.2: the *"already obtained"* 660/808nm IEC 62471 assessment does not exist. Q13 (Mode F) stays reserved and unwritten. §8 and §10 extended. No existing question reworded |
