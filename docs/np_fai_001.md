# First Article Inspection Programme

**Project:** NeurOne
**Document:** NP-FAI-001
**Revision:** 1
**Date:** 2026-08-11
**Status:** ACTIVE
**Effective Date:** 2026-08-11
**Author:** NeurOne Quality (interim: Steve Hickman, CEO)
**Approved By:** — (new document)
**References:** NP-ART-001 Rev 1 (artifact register); NP-FAI-ZM-001 Rev 1 (superseded — method source); NP-RM-001 Rev 1 §4 (severity/probability scales); NP-QMS-DC-001 (record types); NP-PROC-SUP-001 Rev 1 (supplier qualification); NP-COORD-001 (gates); NP-CONV-001 Rev 2 §4; `docs/superseded/README.md`
**Related Issues:** —
**Gate:** NP-COORD-001 G2 (pre-tooling) and G3 (pre-production)
**IEC 62304 Class:** N/A (mechanical/electrical inspection programme)
**Supersedes:** None. `NP-FAI-ZM-001` Rev 1 is superseded by this document **together with** `NP-ART-001` and the per-artifact checklists; no single document replaces it one-for-one, because it conflated a programme, a method and one artifact's checklist.
**Parent Document:** None

---

> **What this document is for.** `NP-FAI-ZM-001` Rev 1 was three things in one file: an FAI *method*,
> an FAI *checklist for the zone-module FPC assembly*, and — through its §9 risk cross-reference —
> the closure evidence for eleven entries in the risk register. When the zone module was retired the
> checklist died with it, but the method did not, and neither did the qualification tests it
> defined. This document separates the three. The method and the surviving tests live here; the
> per-artifact checklists are separate documents; the artifact list and its readiness live in
> `NP-ART-001`.

---

## 1. Scope

This document defines **how** a NeurOne first article inspection is written, run and dispositioned.
It does not inspect anything itself. Every artifact's actual checklist is its own document, named
`NP-FAI-<ARTIFACT>-001` and listed in `NP-ART-001` §3.

It also carries forward, unchanged in substance, the two **process qualifications** that
`NP-FAI-ZM-001` defined and that are not properties of the retired geometry: PDMS–polyimide bond
qualification (§4) and post-service ingress qualification (§5). Both apply to the hex tile exactly
as they applied to the zone module, because both are properties of *materials and process*, not of
part outline.

**Out of scope:** software verification (IEC 62304, `NP-SW-001`), incoming inspection of purchased
components as a supplier control (`NP-PROC-SUP-001`), and routine production inspection after the
first article is accepted.

---

## 2. When an FAI checklist may be written

This is the rule that produced most of `NP-ART-001` §3.2's blocked list, so it is stated plainly:

> **Every accept criterion in an FAI checklist must be traceable to a released document.** A
> criterion with no source is not a conservative placeholder — it is a number that will be measured
> against, argued about and eventually shipped to.

Concretely, an FAI checklist may be issued when **all** of the following hold for the artifact:

| # | Condition | Why |
|---|---|---|
| **F1** | A governing specification exists with status `BASELINED` or `ACTIVE` | `DRAFT` and `DESIGN STUDY` documents state explicitly that their numbers are proposals. Inspecting to a proposal produces a pass record that means nothing. |
| **F2** | Every dimension the checklist inspects is dimensioned in that specification, or is explicitly marked as gated on a named open item | The retired checklist did this correctly: `FAI-TC02` carried `[BLOCKING]` and named its dependency. A gated item is honest; an invented number is not. |
| **F3** | The manufacturing process the checklist inspects has a supplier category in `NP-PROC-SUP-001` | An FAI inspects the output of a process. If nobody has qualified the process, the inspection has no baseline to detect drift from. |
| **F4** | The artifact's failure modes are in a risk register | §7's cross-reference is what makes an FAI a risk control rather than a measurement exercise. |

An artifact failing any of F1–F4 gets an entry in `NP-ART-001` §3.2 naming the blocking item, **not**
a checklist with `TBD` in the accept column.

---

## 3. Checklist structure

Every `NP-FAI-*` checklist uses the section pattern established by `NP-FAI-ZM-001` and retained
because it worked — the ordering front-loads the cheap checks that invalidate the expensive ones:

| § | Section | Purpose |
|---|---|---|
| 1 | Pre-inspection document check | Confirm the specification revisions, BOM revision and drawing notes on file are the ones being inspected to. Catches the whole-inspection-invalid case for the cost of a document review. |
| 2 | Incoming component inspection | Purchased parts, before assembly consumes them. |
| 3 | Assembled artifact inspection | Dimensional, visual, and the process qualifications of §4–§5 below. |
| 4 | Installation and interface verification | The artifact in its mating assembly. |
| 5 | Lifecycle validation | Cycle counts against the rated life. |
| 6 | Functional test | The artifact doing its job in a complete system. |
| 7 | Non-conformance summary | Every FAIL, with disposition. |
| 8 | Result and sign-off | Named signatories. |
| 9 | Risk cross-reference | Each risk this FAI is the control for, and the item numbers that evidence it. |

**Item numbering** is `FAI-<GROUP><NN>` — append-only per `NP-CONV-001` §6. A retired item is struck
through and retained; its number is never reused.

**Item markers:**

| Marker | Meaning |
|---|---|
| `[BLOCKING]` | Must pass before any other item in the checklist proceeds, and before production starts. |
| `[GATED: OI-xxx]` | The accept criterion is not yet derivable. The item exists, is numbered, and cannot be signed until the named open item closes. **New at Rev 1** — `NP-FAI-ZM-001` had no way to express this and used prose. |
| `[QUAL]` | One-time process qualification, not a per-lot check. |

**Disposition:** any FAIL triggers a disposition review. No waiver without Engineering **and**
Quality sign-off — carried forward from `NP-FAI-ZM-001` §Scope.

---

## 4. PDMS–polyimide bond qualification [QUAL] — carried forward

**Applies to:** every artifact with a PDMS optical window bonded to polyimide — currently the hex
tile (A1/A2), and any future module carrying an emitting face.

**Why it survives the architecture change.** The hazard is a 15:1 CTE mismatch (PDMS ≈ 300 ppm/°C
against polyimide ≈ 18 ppm/°C) driving fatigue at the bond line over the ~1,800 thermal cycles of
device life. That is a materials property. The hex tile has a smaller window than the retired
66 × 78 mm module, which changes the absolute strain at the perimeter but not the mechanism, and
**does not relax the qualification** — the bond process is identical (75 nm SiO₂ RF-magnetron
interlayer, O₂ plasma activation, 120 °C/2 h/50 g·cm⁻² cure).

| Item | Test | Method | Accept criterion |
|---|---|---|---|
| **FAI-M01** | Visual inspection of the PDMS–PI bond line | Visual, 10× magnifier | No delamination, no air bubble > 0.5 mm, no discolouration. Any delamination ≥ 1 mm → reject. |
| **FAI-M02** | 90° peel test on a witness coupon from the same bonding batch, processed through the full sputter/plasma/cure sequence identically to production | Instron or equivalent, IPC-TM-650 2.4.9 | ≥ **150 N/m**; target ≥ 400 N/m. 100–149 N/m → conditional accept, review process, re-run 3 coupons. < 100 N/m → **reject batch, halt production**, escalate to Manufacturing Eng. |
| **FAI-M03** | 5-cycle accelerated screen, −20 °C to +70 °C, 1 witness coupon per production batch | Thermal chamber | No new delamination. Passing FAI-M03 alone does **not** evidence long-term reliability — FAI-TC02 must already be on file. |
| **FAI-TC01** | Bonding process documentation check before any qualification sample is made: SiO₂ 75 nm RF magnetron; O₂ plasma 100 W / 30 sccm / 60 s; cure 120 °C / 2 h / 50 g·cm⁻²; < 10 min plasma-to-contact delay | Process control plan review | All five parameters documented. Any deviation approved in writing by HW EE before qualification proceeds. |
| **FAI-TC02** | **[BLOCKING] [QUAL]** Full thermal-cycling qualification: IEC 60068-2-14 condition Na, 10 bonded coupons, 200 cycles −10 °C to +65 °C, 15 min dwell, ramp ≤ 3 °C/min; interim inspection of 5 coupons at 100 cycles | IEC 60068-2-14 chamber | Zero delamination at the 100-cycle interim **and** at 200 cycles on all 10. Any delamination → **HALT**; SiO₂ interlayer process non-conformant. **Production cannot start until this passes.** |
| **FAI-TC03** | Post-cycling 90° peel on all 10 coupons immediately after FAI-TC02 | As FAI-M02 | ≥ 150 N/m on all 10 after 200 cycles. Record min/max/mean in the qualification report. |
| **FAI-TC04** | Post-cycling optical transmittance at 660 nm and 808 nm, same instrument as the pre-cycling baseline | Spectrophotometer | ≥ 90 % of pre-cycling baseline at both wavelengths; reject below 85 % absolute. |
| **FAI-TC04a** | **[GATED: OI-HEXTILE-02]** Post-cycling transmittance at **1064 nm** for T1-C tiles | Spectrophotometer | **New at Rev 1.** The retired checklist tested 660 and 808 nm only, because 1064 nm arrived with the smart module after it was written. The criterion cannot be set until the 1064 nm emitter and window stack are selected. |
| **FAI-TC05** | LED-junction cycling simulation (recommended, not mandatory for release): 3 complete tiles, 500 LED on/off cycles at 25 % duty, rated peak current, PD1 monitored each cycle | Bench supply + tile | PD1 output within 5 % of initial after 500 cycles. Required before claiming > 2-year PDMS reliability. |
| **FAI-TC06** | Qualification report sign-off | Document review | Signed `NP-QR-PDMS-001` on file, containing supplier, process control plan reference, dates, all peel and transmittance data, and the chamber calibration certificate. Retained for device lifetime + 2 years. |

**FAI-TC02 remains BLOCKING and remains unmet.** It gated zone-module production and it gates
hex-tile production. It is the oldest unresolved production blocker in the programme.

---

## 5. Post-service ingress qualification [QUAL] — carried forward, and materially harder

**Applies to:** A1 hex tile (perimeter gasket), A6 shell, A8 hub enclosure.

**Why it survives, and why the criterion tightened.** IPX4 after *user* field replacement — not
IPX4 as-built — is the property that matters, because every module is user-swappable. The retired
test removed and re-inserted 5 modules 10 times each: **50 swaps, 5 seals**. The hex lattice has
~30 populated sockets on a ~80-socket lattice, so the same per-module cycle count is **~300 swaps
over ~30 independent perimeter seals**, and total seam length rises correspondingly
(`NP-HEX-ZM-001` §6: *"IPX4 rides on 30 co-moulded gaskets — a per-tile seam-length budget is
required"*).

| Item | Test | Method | Accept criterion |
|---|---|---|---|
| **FAI-IPX-01** | As-built IPX4 baseline on the assembled headset, all tiles factory-installed | IEC 60529 IPX4, 10 min, all directions | No ingress to Hub PCB, any connector, any tile PCB. All EEG channels < 10 kΩ after test. |
| **FAI-IPX-02** | **[BLOCKING]** Post-service IPX4: remove and re-insert **every populated tile 10 times** at user pace, no tools, no re-sealing; then repeat the full IPX4 spray | 10 swaps/tile, then IEC 60529 IPX4 | Same criterion as FAI-IPX-01, with **zero** re-sealing. Any ingress → **HALT production**; gasket geometry or socket rim tolerance non-conformant. |
| **FAI-IPX-03** | Gasket visual and dimensional check after cycling, 10× magnification; cross-section height on a sample | 10× magnifier; calibrated comparator | No tears, cuts or embedded debris. Compression set < 10 %. |
| **FAI-IPX-04** | **[QUAL]** Gasket ageing: 5 sealed tiles at 40 °C / 75 % RH for 90 days (≈ 2 years ambient, Arrhenius), then IPX4 and dimensional check | IEC 60068-2-66 damp heat, then IPX4 | IPX4 pass; dimensions within 5 % of nominal; compression set < 15 %. |
| **FAI-IPX-05** | **[GATED: NP-TOOL-HEXTILE-001 OI-THEX-03]** Per-tile seam-length budget: total sealed perimeter across the populated lattice, against the leak rate the gasket section can hold | Calculation + IPX4 | **New at Rev 1.** ~30 seals in series is a different reliability problem from 5, and no budget has been set. |

---

## 6. Sign-off

An FAI is complete when §8 of the artifact's checklist carries a signature from each discipline that
owns an item in it, plus Quality. The retired checklist required eight signatories against a
document (`NP-DRV-SHELL-001` §8) that no longer exists; signatory lists are now per-checklist and
derived from the item owners, not fixed programme-wide.

---

## 7. Risk cross-reference

Each checklist's §9 lists the risks it is the control for and the item numbers that evidence
closure. The mapping for the tests in this document:

| Risk | Register | Controlled by |
|---|---|---|
| **RISK-04** — PDMS–PI CTE mismatch delamination | `NP-RISK-003` | FAI-M01…M03, FAI-TC01…TC06 |
| **RISK-16** — IPX4 after user field replacement | `NP-RISK-003` | FAI-IPX-01…IPX-05 |

---

## 8. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **OI-FAI-01** | **FAI-TC02 has never been run.** It has been the stated blocking production gate since 2026-05-06, across two architectures. It needs a coupon supplier, chamber time and ~4 weeks elapsed for 200 cycles at ≤ 3 °C/min. Nothing about the hex-tile decision relaxes it. | Manufacturing Eng | **Production start (any tile)** |
| **OI-FAI-02** | Set the FAI-TC04a 1064 nm transmittance criterion once **OI-HEXTILE-02** selects the emitter and window stack. | HW EE / Optical | T1-C release |
| **OI-FAI-03** | Confirm the §5 ingress qualification scales to ~30 seals, or re-derive it. FAI-IPX-02's cycle count grows 6× and the seals are in series for the enclosure claim but independent for the tile claim; the retired test never had to distinguish those. | ME + Quality | IPX4 claim |
| **OI-FAI-04** | Decide whether `NP-QR-PDMS-001` (the qualification report the retired checklist required by number) is still the record of file, or whether it is superseded alongside its parent. It is cited by number and has never been created. | Quality | DHF consistency |

---

## 9. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 1 | 2026-08-11 | NeurOne Quality | Initial release. Separates the FAI *programme and method* from the per-artifact *checklists* and from the *artifact register*, which `NP-FAI-ZM-001` Rev 1 conflated. §2 states the four conditions (F1–F4) under which a checklist may be issued at all — the rule that produced the blocked list in `NP-ART-001` §3.2. §3 retains the nine-section checklist pattern and adds the `[GATED: OI-xxx]` marker so an item can exist without an invented accept criterion. §4 and §5 carry forward the two process qualifications that are properties of materials and process rather than of the retired geometry: PDMS–PI bond (FAI-M01…M03, FAI-TC01…TC06) and post-service ingress (FAI-IPX-01…04). **Two criteria tightened by the architecture change rather than relaxed**: FAI-IPX-02 grows from 50 swaps over 5 seals to ~300 over ~30, and FAI-TC04a adds a 1064 nm transmittance check the original predates. Records that **FAI-TC02 remains BLOCKING and unmet** — the programme's oldest open production gate, unaffected by the hex-tile decision. Raises OI-FAI-01…04. |
