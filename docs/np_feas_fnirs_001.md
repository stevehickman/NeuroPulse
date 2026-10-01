# fNIRS on Existing NeurOne NIR Optics — Feasibility Assessment

**Project:** NeurOne
**Document:** NP-FEAS-FNIRS-001
**Revision:** 2
**Date:** 2026-09-27
**Status:** DRAFT — exploratory; feasibility assessment only; creates no locked decision. Feeds a go/no-go on adding an fNIRS brain-monitoring modality.
**Author:** SmartyPants (competitive analysis, Neurode Labs comparison)
**Approved By:** — (pending Steve Hickman review)
**References:** CLAUDE.md §3 (modality 1 PBM Transcranial, modality 6 PPG/HRV), §9 (competitive position — Neurode gap note); NP-HW-FPC-001 Rev 5 (dual-PD, 1064nm InGaAs); NP-FW-PBM1064-001 Rev 1 (per-channel dose metering, InGaAs PD coefficients); NP-HW-HUB-001 Rev 2 (DG2788A per-slot TIA gain switch); RISK-03 (NP-REG-PBM1064-001 — marketing/regulatory gate); NP-HW-HEXTILE-001 §4.1/§5.1/§8.1 (emitter lattice, PD1/PD2 at site 0, per-channel strings); NP-HEX-ZM-001 §3 (40 mm tile); NP-HW-EEGNET-001 §1.9.4 (four-pod separation table)
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
| Transcranial LEDs | 660nm + 808–830nm on each T1-A base PBM tile, tiled across the hex-socket lattice (NP-HEX-ZM-001) — count scales with tiles populated, not a fixed 5-zone total; +1064nm on T1-C smart tiles | Light **sources** — already scalp-coupled and per-channel PWM-addressable |
| Dual photodiodes / zone | PD1 (behind PDMS, forward emission) + PD2 (scalp-facing, backscatter). Smart module: InGaAs Hamamatsu G12180-010A | Candidate **detectors** — but co-located with source (see Risk B) |
| Hub TIA + gain switch | Per-socket TIA (47 kΩ; 22 kΩ for InGaAs via DG2788A) — every socket I2C/TIA-capable per SMART-1 (NP-HEX-ZM-001 §4a) | Detector front-end — gain path already switchable (extensible) |
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

**Risk B — Source–detector geometry. Requires cross-tile reads.**
PD1/PD2 are **co-located with the LED array** to measure near-field backscatter (mm depth) for dose metering — exactly the wrong geometry for fNIRS, which needs cm-scale separation to reach cortex. A detector directly under a source samples skin/skull, not brain. Path forward: read a detector on a **neighbouring tile** while a source tile drives. *(Rev 2 correction: Rev 1 gave this as "~30 mm cross-zone", a figure from the retired five-zone layout. On the hex-tile lattice it does not hold.)* The geometry is:

- **Centre to centre is 40.0 mm.** The tile is 40 mm flat-to-flat (NP-HEX-ZM-001 §3), so neighbouring tile centres are 40.0 mm apart. PD1/PD2 sit at site 0, the tile centre (NP-HW-HEXTILE-001 D-2, §5.1).
- **The source is the whole tile, not a point.** Each wavelength is driven as one channel of series strings through one FET (NP-HW-HEXTILE-001 §8.1), so a single emitter cannot be lit alone. The emitters of one channel span the 91-site lattice, out to ring 5 at 19.0 mm (5 × 3.80 mm, §4.1). Seen from the neighbour's PD, they sit **~21–59 mm** away, with the centroid at 40.0 mm.
- **So the cross-tile read overshoots the 25–35 mm window at its centroid, and is not a single separation at all.** Diffuse-light falloff weights the detected signal toward the nearest emitters, so the measurement mixes depths. No one separation figure describes it.

An in-window separation exists elsewhere: the **diagonal of a four-pod T1-B tile, 29.0 mm** (NP-HW-EEGNET-001 §1.9.4). That needs a *third* PD, because D-2 keeps PD1/PD2 co-located, and a third PD needs socket contacts the closed 19-contact budget does not have (`OI-EEGNET-22`). **A third option: per-string switching.** Lighting single emitters is not practical, because each channel is series strings on a 24 V rail (NP-HW-HEXTILE-001 §8.1). A string is the finest practical unit: 4 × 11 at 660 nm and 3 × 14 at 808 nm on T1-A, at §8.1.1's worked example. On the 91-site lattice, **22–24 of the 90 emitter sites** sit 25–35 mm from a given neighbour's PD2, depending on how the lattice is oriented to the tile. That is about 12 per wavelength, roughly one string's worth, in a band on the facing side. A string laid out as that band and lit alone would be an in-window source for the neighbour's existing PD2, with **no third PD and no socket contacts**. Three conditions apply:

- **The strings must be laid out as bands**, not in the alternating rows NP-HW-HEXTILE-001 §4.2 describes. A hexagon's 6 neighbours cannot each get a band from 4 + 3 strings. The layout stays open until emitter selection (`OI-HEXTILE-02`).
- **It needs a gate per string** and a larger tile MCU than the listed 8-pin part. The gates must sit downstream of the `REQ-TDRV-02` duty limiter.
- **It is safe only if current is regulated per string.** Under a per-channel `I_cap`, switching strings off forces the channel current through the strings left on: about 4× per emitter with 1 of 4 CH_A strings on. That exceeds the emitter rating and the 400 mW/cm² peak. NP-HW-HEXTILE-001 is inconsistent on which applies (`OI-HEXTILE-27`). Under per-channel regulation this option is a hazard-control change, not a firmware one.

Risks A, C and D apply to this option unchanged.

A **short + long** separation pair additionally enables scalp-signal regression (superficial-hemodynamics removal — standard fNIRS practice). The cross-tile read is a firmware routing + calibration change with no new detector part. An in-window read needs either a third PD or per-string switching.

**Risk C — Far-field SNR.**
Detected power at 3 cm through scalp/skull is orders of magnitude below the near-field dose signal the current TIAs (47/22 kΩ) are tuned for. Recovering it needs a higher-gain TIA path and **modulated / time-multiplexed detection** to reject ambient and separate wavelengths. The DG2788A per-slot gain switch is a foothold; a dedicated high-gain fNIRS acquisition mode would likely be needed.

**Before gain, responsivity (`OI-HEXTILE-26`, open).** NP-HW-HEXTILE-001 §5.1 fits the Hamamatsu G12180-010A InGaAs PD to every tile as PD1 and PD2, on the premise that InGaAs is broadband. The part's published range is **0.9–1.7 µm**, which excludes 660 nm and 808 nm. If the datasheet's spectral-response curve confirms negligible responsivity there, no gain setting recovers a signal: the fitted PDs cannot detect either stock fNIRS wavelength, only 1064 nm on T1-C. The §6 step 1 bench then cannot run on T1-A tiles as written, and reusing the fitted detectors would need the silicon or extended-range PD that `OI-HEXTILE-26` raises for dose metering. Nothing here selects a part.

**Risk D — Hair coupling.**
Scalp fNIRS at cm separation must get photons out and back through hair-gapped skin — the classic scalp-fNIRS failure mode. NeurOne's plasma-activated PDMS windows couple well for pressed near-field dose metering; far-field coupling through hair is materially harder and is the largest **empirical** unknown.

---

## 5. Recommended build scope (if pursued)

**Tier 1 — "Brain activity visualization" (marketing-grade, matches Neurode's claim):** single-wavelength or total-Hb blood-volume trend, cross-tile long-separation read, mBLL on M7. Firmware + calibration only, **at the cross-tile geometry**, which is outside the 25–35 mm window (§4 Risk B). An in-window read needs a third PD and socket contacts (`OI-EEGNET-22`). Delivers the real-time visualization differentiator, pairs with the existing EEG closed loop (electrical + hemodynamic fusion — which Neurode *cannot* do; it has no EEG).

**Tier 2 — Quantitative HbO₂/HbR oximetry:** requires resolving Risk A — either restrict to smart-module (660 + 1064 nm) headsets, or add a ~850 nm emitter to the zone-module spec. Module BOM/mold change; regulatory scope expansion.

---

## 6. Suggested next steps (bench, before any spec)

1. **Coupling/SNR sanity bench (GitHub Issue #193):** existing tiles — drive one tile's channel, read a **neighbouring** tile's scalp-facing PD2 (40.0 mm centre to centre, ~21–59 mm per emitter; §4 Risk B), measure detectable ΔOD during a **breath-hold / Valsalva** (global CO₂ → large hemodynamic swing). If a breath-hold signal is not recoverable through hair, Tier 1 is not viable on stock optics — kill early. A **positive** result is weaker than it looks: the read is not at an in-window separation, so it shows coupling through hair, not cortical sensitivity.
2. **Firmware spike:** time-multiplexed source-encoding mode + mBLL; reuse the DG2788A gain path in a high-gain acquisition profile.
3. **Regulatory scope (RISK-03 counsel):** confirm "brain-activity / blood-flow visualization" as a **wellness** claim; any oximetry/clinical framing is a separate gate.
4. **Data classification (before any storage code):** fNIRS raw optical + derived hemodynamic time series describe the **person** → **UHDR**. LED/PD calibration drift → **SHDR**. Add rows to the NP-FW-EMMC-001 §12 session-data classification table if built.

---

## 7. Bottom line

fNIRS is the only Neurode feature category NeurOne lacks, and NeurOne is unusually well-positioned to add it because the **sources, a detector class, a switchable-gain front-end, and the compute already ship**. A blood-volume-trend "brain visualization" is a firmware-plus-geometry effort; true oximetry needs a wavelength fix (660 + 1064 nm, or add ~850 nm). The gating unknown is empirical — **far-field coupling through hair (Risk D)** — and is answerable with a one-afternoon breath-hold bench on existing modules before any spec is written.

---

## 8. Revision History

| Rev | Date | Author | Change |
|---|---|---|---|
| 1 | 2026-07-13 | SmartyPants | First issue |
| 2 | 2026-09-27 | NeurOne Systems Engineering | **Separation corrected (§4 Risk B, §6 step 1).** Rev 1 gave the cross-zone read as "~30 mm", a figure from the retired five-zone layout, and `NP-HW-EEGNET-001` §1.9.4 gives it as 40.0 mm. On the hex-tile lattice it is 40.0 mm centre to centre, and each emitter sits ~21–59 mm from the neighbour's PD, because a channel lights the whole tile. The in-window 29.0 mm four-pod diagonal and its third-PD cost (`OI-EEGNET-22`) are now cited. The §6 bench is restated for tiles, and a positive result there no longer implies cortical sensitivity. §5 Tier 1 now states that "firmware + calibration only" holds only at the out-of-window cross-tile geometry. **Risk C now cites `OI-HEXTILE-26`:** the fitted InGaAs PD's published 0.9–1.7 µm range excludes 660 and 808 nm, so the stock detectors may not see either fNIRS wavelength. **Risk B adds per-string switching** as an in-window option that needs no third PD or socket contacts. It is safe only under per-string current regulation, which NP-HW-HEXTILE-001 does not settle (`OI-HEXTILE-27`). No other conclusion changed |
