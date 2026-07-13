# fNIRS on Existing NeurOne NIR Optics — Feasibility Assessment

**Project:** NeurOne
**Document:** NP-FEAS-FNIRS-001
**Revision:** A
**Date:** 2026-07-13
**Status:** EXPLORATORY — feasibility assessment only; creates no locked decision. Feeds a go/no-go on adding an fNIRS brain-monitoring modality.
**Author:** SmartyPants (competitive analysis, Neurode Labs comparison)
**Approved By:** — (pending Steve Hickman review)
**References:** CLAUDE.md §3 (modality 1 PBM Transcranial, modality 6 PPG/HRV), §9 (competitive position — Neurode gap note); NP-HW-FPC-001 Rev E (dual-PD, 1064nm InGaAs); NP-FW-PBM1064-001 Rev A (per-channel dose metering, InGaAs PD coefficients); NP-HW-HUB-001 Rev B (DG2788A per-slot TIA gain switch); RISK-03 (NP-REG-PBM1064-001 — marketing/regulatory gate)
**Related:** Neurode headband competitive analysis (2026-07-13); GitHub Issue #193 (breath-hold coupling bench — §6 step 1)
**Gate:** —
**IEC 62304 Class:** — (would inherit SW-02 Class B if built)

---

## 1. Purpose

Neurode Labs' headband pairs tRNS stimulation with **fNIRS** (functional near-infrared spectroscopy, claimed "152" sensors) for real-time hemodynamic brain-activity visualization. fNIRS is the one Neurode capability category with **no NeurOne equivalent** (see §9 gap note). This document assesses whether NeurOne can add fNIRS **on its existing NIR optics** — i.e. as a firmware + minor-hardware effort rather than a new sensing subsystem — and identifies the technical blockers that would prevent a straight reuse.

**Conclusion up front:** A **functional blood-volume / total-hemoglobin trend monitor** ("real-time brain activity visualization" at marketing grade) is plausibly achievable on existing hardware with firmware plus a long-separation detector read. **Quantitative HbO₂/HbR oximetry** (the clinically meaningful form) is **not** clean on the stock 660 + 808–830 nm wavelength pair and needs a wavelength change. Four engineering risks gate any build.

---

## 2. What fNIRS requires

fNIRS injects NIR light into the scalp and measures diffusely back-reflected light at a **source–detector (S-D) separation of ~2.5–3.5 cm**. Photons follow a "banana"-shaped path; sampling depth ≈ half the S-D separation, so ~3 cm separation reaches ~1.5 cm — into cortex. Attenuation change at **two wavelengths bracketing the ~805 nm HbO₂/HbR isosbestic point** is converted by the modified Beer–Lambert law (mBLL) into ΔHbO₂ and ΔHbR concentration changes.

Minimum ingredients: (a) ≥2 suitable wavelengths, (b) a detector at a defined **cm-scale** separation from the source, (c) enough detector SNR to recover a signal ~10⁻⁶–10⁻⁹ of injected power, (d) source encoding so each detector sample is attributable to a known source+wavelength.

---

## 3. What NeurOne already has

| Asset | Spec | fNIRS relevance |
|---|---|---|
| Transcranial LEDs | 300×660nm + 300×808–830nm, 5 zones, 6 mm pitch (base module); +150×1064nm on smart module | Light **sources** — already scalp-coupled and per-channel PWM-addressable |
| Dual photodiodes / zone | PD1 (behind PDMS, forward emission) + PD2 (scalp-facing, backscatter). Smart module: InGaAs Hamamatsu G12180-010A | Candidate **detectors** — but co-located with source (see Risk B) |
| Hub TIA + gain switch | Per-slot TIA (47 kΩ; 22 kΩ for InGaAs via DG2788A) | Detector front-end — gain path already switchable (extensible) |
| Main processor | i.MX RT1062 M7, 600 MHz, ~98.9% idle | mBLL + source multiplexing + scalp regression trivially fit |
| PPG optics (VNS clip) | 808–830nm PPG for HRV | Proves NeurOne already does NIR photoplethysmetric sensing |

The sources, a detector class, a switchable-gain TIA, and abundant compute already exist. The gaps are **wavelength choice, detector geometry, far-field SNR, and coupling** — not the absence of optics.

---

## 4. The four blockers

**Risk A — Wavelength pair (chromophore separation). CENTRAL CAVEAT.**
Good fNIRS wants one wavelength **below** and one **above** the ~805 nm isosbestic point (e.g. 690–760 nm and 830–850 nm) so HbO₂ and HbR separate cleanly. NeurOne's native pair is **660 nm + 808–830 nm**:
- 808–830 nm sits **on/near the isosbestic point** — the *worst* place for one of the two wavelengths, because HbO₂ and HbR have near-equal extinction there → poor oxy/deoxy separation.
- 660 nm is at the short edge of the optical window (higher blood/water absorption, shallower penetration).
→ The stock pair supports **total-hemoglobin / blood-volume** trends (both chromophores rise/fall together with perfusion) but gives a **poorly conditioned** HbO₂/HbR split. Clean oximetry wants either **660 + 1064 nm** (smart-module only) or a **new ~850 nm emitter** added to the module spec — a BOM/mold change, not free.

**Risk B — Source–detector geometry. Requires cross-zone reads.**
PD1/PD2 are **co-located with the LED array** to measure near-field backscatter (mm depth) for dose metering — exactly the wrong geometry for fNIRS, which needs cm-scale separation to reach cortex. A detector directly under a source samples skin/skull, not brain. Path forward: read a **detector on an adjacent/further zone module** while a source zone drives (6 mm pitch × N LEDs, or ~30 mm cross-zone). A **short + long** separation pair additionally enables scalp-signal regression (superficial-hemodynamics removal — standard fNIRS practice). This is a firmware routing + calibration change; no new detector part if reusing zone PDs.

**Risk C — Far-field SNR.**
Detected power at 3 cm through scalp/skull is orders of magnitude below the near-field dose signal the current TIAs (47/22 kΩ) are tuned for. Recovering it needs a higher-gain TIA path and **modulated / time-multiplexed detection** to reject ambient and separate wavelengths. The DG2788A per-slot gain switch is a foothold; a dedicated high-gain fNIRS acquisition mode would likely be needed.

**Risk D — Hair coupling.**
Scalp fNIRS at cm separation must get photons out and back through hair-gapped skin — the classic scalp-fNIRS failure mode. NeurOne's plasma-activated PDMS windows couple well for pressed near-field dose metering; far-field coupling through hair is materially harder and is the largest **empirical** unknown.

---

## 5. Recommended build scope (if pursued)

**Tier 1 — "Brain activity visualization" (marketing-grade, matches Neurode's claim):** single-wavelength or total-Hb blood-volume trend, cross-zone long-separation read, mBLL on M7. Firmware + calibration only. Delivers the real-time visualization differentiator, pairs with the existing EEG closed loop (electrical + hemodynamic fusion — which Neurode *cannot* do; it has no EEG).

**Tier 2 — Quantitative HbO₂/HbR oximetry:** requires resolving Risk A — either restrict to smart-module (660 + 1064 nm) headsets, or add a ~850 nm emitter to the zone-module spec. Module BOM/mold change; regulatory scope expansion.

---

## 6. Suggested next steps (bench, before any spec)

1. **Coupling/SNR sanity bench (GitHub Issue #193):** existing zone modules — drive one zone's LED, read an **adjacent** zone's scalp-facing PD (~30 mm separation), measure detectable ΔOD during a **breath-hold / Valsalva** (global CO₂ → large hemodynamic swing). If a breath-hold signal is not recoverable through hair, Tier 1 is not viable on stock optics — kill early.
2. **Firmware spike:** time-multiplexed source-encoding mode + mBLL; reuse the DG2788A gain path in a high-gain acquisition profile.
3. **Regulatory scope (RISK-03 counsel):** confirm "brain-activity / blood-flow visualization" as a **wellness** claim; any oximetry/clinical framing is a separate gate.
4. **Data classification (before any storage code):** fNIRS raw optical + derived hemodynamic time series describe the **person** → **UHDR**. LED/PD calibration drift → **SHDR**. Add rows to the NP-FW-EMMC-001 §12 session-data classification table if built.

---

## 7. Bottom line

fNIRS is the only Neurode feature category NeurOne lacks, and NeurOne is unusually well-positioned to add it because the **sources, a detector class, a switchable-gain front-end, and the compute already ship**. A blood-volume-trend "brain visualization" is a firmware-plus-geometry effort; true oximetry needs a wavelength fix (660 + 1064 nm, or add ~850 nm). The gating unknown is empirical — **far-field coupling through hair (Risk D)** — and is answerable with a one-afternoon breath-hold bench on existing modules before any spec is written.
