# First Article Inspection Programme

**Project:** NeurOne
**Document:** NP-FAI-001
**Revision:** 3
**Date:** 2026-09-20
**Status:** ACTIVE
**Effective Date:** 2026-08-11
**Author:** NeurOne Quality (interim: Steve Hickman, CEO)
**Approved By:** — (new document)
**References:** NP-ART-001 Rev 1 (artifact register); NP-FAI-ZM-001 Rev 1 (superseded — method source); NP-RM-001 Rev 1 §4 (severity/probability scales); NP-QMS-DC-001 (record types); NP-PROC-SUP-001 Rev 1 (supplier qualification); NP-COORD-001 (gates); NP-CONV-001 Rev 6 §1.1, §4, §6; NP-FW-PBM1064-001 Rev 4 §11; NP-FW-HD-001 Rev 6 §12; NP-FW-CVNS-001 Rev 5 §9; NP-FW-ANON-001 Rev 1 §9; NP-HW-TCAP-001 Rev 2; NP-HW-AUDIO-001 Rev 1 §8.1; NP-HW-NASAL-001 Rev 1 §8.1; NP-HW-VNSCLIP-001 Rev 1 §8.1; NP-HW-CVNS-001 Rev 1 §8.1; NP-HW-TMS-001 Rev 1 §8.1; NP-HW-TACSDRV-001 Rev 1; NP-PRIV-REM-001 Rev 5 STEP-22…STEP-24; `docs/superseded/README.md`
**Related Issues:** GitHub Issue #343 (OI-ART-05 disposition); GitHub Issue #332 (A11–A15 hardware specifications — §2.1.1 and OI-FAI-07 re-scoped; §2.2 added)
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

### 2.1 Serials named for checklists nobody wrote — disposition (closes `NP-ART-001` OI-ART-05)

Five `NP-FAI-*` serials were cited across the document set — and two of them were printed by a test
binary as its own document header — as though they were controlled documents. **None had ever been
written.** `NP-ART-001` OI-ART-05 raised it; this section is its disposition. Recorded here rather
than in `NP-ART-001` because four of the five turn out not to be artifact checklists at all, and §2
above is the rule that decides them.

**The finding is not that five documents are missing.** Four of the five named content that already
exists, in full, inside a firmware specification's own FAI section:

| Serial | Cited as holding | Where those items actually are | That document |
|---|---|---|---|
| `NP-FAI-SM-001` | FAI-SM-01…11 (1064 nm smart module) | `NP-FW-PBM1064-001` §11 | Rev 4, BASELINED |
| `NP-FAI-HD-001` | FAI-HD01…HD04 (sLORETA-guided HD-tDCS) | `NP-FW-HD-001` §12 | Rev 6, BASELINED |
| `NP-FAI-CV-001` | FAI-CV01…CV03 (cervical VNS) | `NP-FW-CVNS-001` §9 | Rev 4, BASELINED |
| `NP-FAI-ANON-001` | FAI-ANON-01…09 (research anonymisation) | `NP-FW-ANON-001` §9 | Rev 1, ACTIVE |

Each of those four is therefore a **second serial for one body of text** — which is the failure
`NP-CONV-001` §4.0.2 exists to prevent, arriving from the other direction: not two files under one
serial, but one body of content under two. **All four are retired.** The sections named above are
the record of file, and every citation has been re-pointed at them.

`NP-FAI-CV-001` was doubly redundant: it and `NP-FAI-CVNS-001` are two serials for the **same
artifact** (A14, cervical VNS accessory). `NP-FAI-CVNS-001` is the one retained, because §1 names an
artifact checklist `NP-FAI-<ARTIFACT>-001` from the register — and `NP-FAI-CV-001` was an
independently invented name in `NP-DHF-001` §8's planned-additions list, never derived from
`NP-ART-001` §3 at all.

#### 2.1.1 The question the retirement does not answer

Retiring a serial does not produce an artifact FAI, and three of the four were cited specifically
for their **hardware-bench** items. So each was tested against §2's F1–F4 as a checklist in its own
right. **None passes, and none can be written today:**

| Candidate checklist | Artifact it would inspect | §2 verdict |
|---|---|---|
| 1064 nm smart module hardware items (FAI-SM-04, -06, -07, -08) | **A2** hex-tile FPC + element population, T1-C | **F1 fails** — `NP-HW-HEXTILE-001` is DESIGN STUDY, and `OI-HEXTILE-02` has selected no emitter, so its §4.3 V_f and radiant-flux figures are design targets rather than datasheet values (`NP-ART-001` §3.2). A2's checklist is already named `NP-FAI-HEXFPC-001` in `NP-ART-001` §3.2; these items belong to it, not to a smart-module serial. |
| HD-tDCS hardware items (FAI-HD01 bench, HD03, HD04) | **T2 clinical electrode cap** | **F1 fails** — `NP-HW-TCAP-001` is DRAFT (Rev 2). **F4 has no register entry to hang on**: the cap is not in `NP-ART-001` §2 at all, which that document raises as `OI-ART-08` (see also `OI-TCAP-06`, `OI-ART-06`). |
| Cervical VNS hardware items (FAI-CV01…CV03) | **A14** cervical VNS accessory | **F1 fails absolutely** — no mechanical or electrical specification exists for the electrode assembly, cable or connector (GitHub #332). This is the one case where the *checklist* is genuinely absent rather than misnamed, and it keeps its serial as a named absence per §2's closing rule. **Superseded in one word at Rev 3, 2026-09-20: not *absolutely*.** `NP-HW-CVNS-001` Rev 1 exists; F1 still fails, on DRAFT status. See §2.2. |
| Research anonymisation (FAI-ANON-01…09) | — | **Out of scope of this programme.** §1 excludes software verification, and `NP-ART-001` §1 excludes firmware modules and data schemas from the artifact register. A pipeline that ships as code has no first article to inspect. `NP-FW-ANON-001` §9 under `NP-SW-001` and IEC 62304 is the correct and sufficient record; `NP-PRIV-REM-001` STEP-22 and STEP-23 already name their verification tests this way, without inventing a serial. STEP-24 was the outlier and is corrected. |

So the standing state after this disposition is: **one named, unwritten artifact checklist
(`NP-FAI-CVNS-001`), blocked on F1**; two item groups that will be absorbed by checklists already
named in `NP-ART-001` §3.2 when those become writable (OI-FAI-05, OI-FAI-06); and one item group
that never needed an FAI serial.

### 2.2 Six artifacts gained an owning specification and none gained a checklist (Rev 3, 2026-09-20)

`NP-ART-001` Rev 3 issued `NP-HW-AUDIO-001` (A11), `NP-HW-NASAL-001` (A12), `NP-HW-VNSCLIP-001`
(A13), `NP-HW-CVNS-001` (A14), `NP-HW-TMS-001` (A15) and `NP-HW-TACSDRV-001` (A16, a new register
row) against GitHub #332. **This section records what that does to §2, which is less than it
sounds and is worth stating precisely.**

| Artifact | F1 | F2 | F3 | F4 |
|---|---|---|---|---|
| A11 audio cup | **fails** — DRAFT | **fails** — no dimension exists | not assessable | **fails**, unblocked (`OI-RISK2-02`) |
| A12 intranasal probe | **fails** — DRAFT | **fails** — only the 15/20/25 mm ring family is dimensioned, without tolerances | not assessable | **fails**, unblocked |
| A13 auricular clip | **fails** — DRAFT | **fails** — the one numeric property with a value (pad area) is PROVISIONAL | not assessable | **fails**, unblocked |
| A14 cervical VNS | **fails** — DRAFT | **fails** | not assessable | **passes** — RISK-25, and `NP-RISK-002` §4's reason for holding it outside a per-artifact register is discharged |
| A15 TMS coil | **fails** — DRAFT **and gated behind two BLOCKING items** | **fails** — `SPEC-TMS` is design-study grade and explicitly re-derivable | not assessable | **fails** — no risk register of any kind |
| A16 tACS driver stage | **fails** — DRAFT | **fails** — no silicon, no compliance voltage | not assessable | **fails** — which register owns it depends on `OI-TACSDRV-01` |

**So the writable-checklist count is unchanged at one** (`NP-FAI-HUB-001`), against sixteen
artifacts rather than fifteen. **That is §2 working, not §2 failing.** F1 exists precisely so that a
`DRAFT` document — which states in its own status line that its numbers are proposals — cannot be
inspected to; six new DRAFT documents create six new owning specifications and zero new accept
criteria.

**What did change is the *shape* of the absence**, and it is the difference between an unanswerable
and an answerable question. Before: *"no owning specification document exists at all"* — nobody
owned the gap and nothing named what would close it. Now each blocked checklist has a document that
enumerates, by open-item ID, the engineering decisions standing between it and F1/F2. `NP-ART-001`
`OI-ART-09` is the successor item that tracks them.

**`NP-FAI-TACSDRV-001` is named here for the first time**, as §2's closing rule requires of an
artifact failing F1. No FAI item anywhere inspects the driver stage today — FAI-HD01's bench limb,
HD03 and HD04 inspect the **cap** (`NP-FW-HD-001` §12), which is `OI-FAI-06`'s subject.

#### 2.1.2 Where these serials may still appear, and where they may not

A sweep that deleted every occurrence of the five strings would delete the records that made the
absence findable in the first place, which is the opposite of what OI-ART-05 asked for. So the rule
is about **what a citation claims**, not about the string:

**Permitted, and each one load-bearing:**

- **§2.1 above** — this disposition, which is the only place the five are enumerated together.
- **`NP-ART-001` §2.4 and §3.2** — the rows recording that A14's checklist has not been written.
  §2 *requires* that entry: an artifact failing F1 gets a named blocker, not a checklist.
- **`NP-DHF-001` §8** — the three planned-addition rows, struck through and marked RETIRED with the
  record of file named. A struck row with its disposition attached is the opposite of a phantom
  document; silently deleting the rows would leave no trace that they were ever planned.
- **Historical records** — revision histories, `docs/status/completed-decisions.md`, and
  `NP-CONV-001` §4.0's worked example. `NP-CONV-001` §1.1 protects records of what was written;
  editing them to make the past tidy is the one thing this disposition must not do.

**Not permitted anywhere, and now absent from the tree:** a firmware banner or CMake registration
naming one; a `Document:` field in a remediation step; a "test specification: …" line in live
specification prose; any citation of one as the source of a criterion. **No file under `firmware/`
mentions any of the five.**

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

### 3.1 An `FAI-` item prefix is not a document serial (added at Rev 2)

> **`FAI-<GROUP><NN>` numbers an inspection or verification item. It says nothing about which
> document holds it, and it never implies that an `NP-FAI-<GROUP>-001` exists.**

This is stated because the opposite inference is what produced §2.1's five phantom serials, and it
was a reasonable inference: `NP-CONV-001` §6 tabulates six identifier families and this is not one
of them, so nothing said where an `FAI-` group lives. Four firmware specifications carry their own
FAI item groups in their own sections — `NP-FW-PBM1064-001` §11 (`FAI-SM-`), `NP-FW-HD-001` §12
(`FAI-HD`), `NP-FW-CVNS-001` §9 (`FAI-CV`), `NP-FW-ANON-001` §9 (`FAI-ANON-`) — and
`NP-PRIV-REM-001` names one-off verification tests the same way (`FAI-ACCEL-01`, `FAI-REVOKE-01`)
with no owning `NP-FAI-*` at all. Each was legitimate; the names were then read as evidence of
documents.

Two consequences:

- **The owning document is named explicitly or it is not cited.** A bare `FAI-HD03` in a test
  banner, a CMake comment or a risk row is not a citation — write `` `NP-FW-HD-001` §12.3 ``.
- **Nothing derives an `NP-FAI-*` serial from an item group.** Artifact checklist serials come from
  the artifact register (`NP-ART-001` §3), which is the only place that decides what NeurOne
  inspects a first article of.

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
| **OI-FAI-05** | **Fold FAI-SM-04, -06, -07 and -08 into `NP-FAI-HEXFPC-001` when that checklist becomes writable.** §2.1 retired `NP-FAI-SM-001`; `NP-FW-PBM1064-001` §11 keeps the items, but §11 is a *firmware* specification and the four hardware-bench items inspect artifact A2. They have no artifact checklist until `OI-HEXTILE-02`, `-12` and `-05` close. Do not re-create a smart-module serial to hold them. | Quality + HW EE | `NP-FAI-HEXFPC-001` issue |
| **OI-FAI-06** | **The T2 clinical electrode cap has no artifact-register entry, so its hardware FAI has nothing to hang on.** FAI-HD01's bench limb, FAI-HD03 and FAI-HD04 inspect the cap; its only specification, `NP-HW-TCAP-001` (Rev 2), is DRAFT (F1) and the cap appears nowhere in `NP-ART-001` §2 (F4). Resolve with `OI-TCAP-06` / `OI-ART-06` by giving the cap a register row, then name its checklist from the register rather than from `NP-FW-HD-001` §12. | Systems + Quality | T2 verification planning |
| **OI-FAI-07** | **`NP-FAI-CVNS-001` stays a named absence — re-scoped 2026-09-20, not closed.** It is the only one of §2.1's five that is a genuinely missing checklist rather than a duplicate serial. **As written it asked for A14 to have a hardware specification; A14 has one** (`NP-HW-CVNS-001` Rev 1, GitHub #332), and F1 still fails because that document is DRAFT — satisfied in its letter, not its substance. The item now runs until `NP-HW-CVNS-001` reaches `ACTIVE`/`BASELINED` with dimensioned criteria, tracked per-decision by `OI-CVNSHW-01…08` and collectively by `NP-ART-001` OI-ART-09. Note `REQ-CVNS-12`: FAI-CV01's placement, impedance and open-detection criteria are **A14's acceptance criteria** carried by a firmware test procedure, and `NP-FAI-CVNS-001` inherits them when F1 is met — nothing moves out of `NP-FW-CVNS-001` §9 meanwhile. Until then `NP-RISK-002` RISK-25's FAI bench cannot be scheduled. | Systems + ME | RISK-25 closure; T2 accessory release |
| **OI-FAI-08** | **`NP-FAI-TACSDRV-001` is a named absence from its first mention (§2.2).** The 21-channel clinical tACS driver stage (A16) has no FAI item anywhere and fails F1, F2 and F4. F4 is the one to sequence first, because `OI-TACSDRV-01`'s siting decision determines **which** risk register and which artifact row own the stage — and if it is sited on Hub PCB Rev C the checklist is `NP-FAI-HUBPCB-001`'s problem, not a new serial's. | Quality + EE | A16 verification planning |

---

## 9. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| **3** | **2026-09-20** | **NeurOne Quality** | **§2.2 added — six artifacts gained an owning specification and no checklist became writable (GitHub #332).** `NP-ART-001` Rev 3 issued hardware specifications for A11, A12, A13, A14, A15 and the newly registered A16; §2.2 tests each against F1–F4 and records that **all six fail F1 on DRAFT status**, five fail F4, and the writable-checklist count stays at **one** (`NP-FAI-HUB-001`) against sixteen artifacts. **That is §2 working**: F1 exists so that a document whose own status line calls its numbers proposals cannot be inspected to. What changed is the *shape* of the absence — from *"no owning specification exists"*, which nobody owned, to a per-document list of the engineering decisions standing between each checklist and F1/F2 (`NP-ART-001` OI-ART-09). **§2.1.1's verdict on A14 is corrected in one word**: *"F1 fails absolutely"* was true when written and is superseded — `NP-HW-CVNS-001` Rev 1 exists and F1 fails on its DRAFT status. **`OI-FAI-07` is re-scoped, not closed**, because its wording is satisfied in the letter and not the substance; it now runs until that document reaches `ACTIVE`/`BASELINED`. **`NP-FAI-TACSDRV-001` is named for the first time** as a named absence, with `OI-FAI-08` raised and its F4 limb sequenced behind the driver's siting decision. **No accept criterion, item number, process qualification or issue condition is created, changed or deleted, and nothing moves out of any firmware specification's own FAI section.** |
| **2** | **2026-09-14** | **NeurOne Quality** | **§2.1 added — disposition of the five `NP-FAI-*` serials that were cited as controlled documents and never written (closes `NP-ART-001` OI-ART-05, GitHub #343).** Principal finding: **four of the five were duplicate serials, not missing documents.** FAI-SM-01…11, FAI-HD01…HD04, FAI-CV01…CV03 and FAI-ANON-01…09 are each already specified in a firmware specification's own FAI section (`NP-FW-PBM1064-001` §11, `NP-FW-HD-001` §12, `NP-FW-CVNS-001` §9, `NP-FW-ANON-001` §9 — three BASELINED, one ACTIVE), so `NP-FAI-SM-001`, `NP-FAI-HD-001`, `NP-FAI-CV-001` and `NP-FAI-ANON-001` are **retired** and every citation is re-pointed at the section that holds the items. `NP-FAI-CV-001` was additionally a second serial for the same artifact as `NP-FAI-CVNS-001`. **No item number is renumbered and no accept criterion is created, changed or deleted** — nothing about the programme's engineering content moves. Each candidate was then tested against §2's F1–F4 **as an artifact checklist in its own right, and none passes**: the smart-module hardware items belong to `NP-FAI-HEXFPC-001` (OI-FAI-05), the HD-tDCS bench items to a cap that has no register row (OI-FAI-06), and `NP-FAI-CVNS-001` fails F1 outright and is **retained as a named absence** in `NP-ART-001` §3.2 exactly as §2's closing rule requires (OI-FAI-07); research anonymisation is out of this programme's scope per §1 and needed no FAI serial. **§3.1 added** — an `FAI-<GROUP><NN>` prefix numbers an item and never implies an `NP-FAI-<GROUP>-001`, which is the inference that produced all five; a bare item number is not a citation. §2.1.2 states the two places a retired serial may still appear (the register that records the absence, and historical records protected by `NP-CONV-001` §1.1) so that a later sweep does not delete the evidence that made the gap findable. Raises OI-FAI-05, -06, -07. |
| 1 | 2026-08-11 | NeurOne Quality | Initial release. Separates the FAI *programme and method* from the per-artifact *checklists* and from the *artifact register*, which `NP-FAI-ZM-001` Rev 1 conflated. §2 states the four conditions (F1–F4) under which a checklist may be issued at all — the rule that produced the blocked list in `NP-ART-001` §3.2. §3 retains the nine-section checklist pattern and adds the `[GATED: OI-xxx]` marker so an item can exist without an invented accept criterion. §4 and §5 carry forward the two process qualifications that are properties of materials and process rather than of the retired geometry: PDMS–PI bond (FAI-M01…M03, FAI-TC01…TC06) and post-service ingress (FAI-IPX-01…04). **Two criteria tightened by the architecture change rather than relaxed**: FAI-IPX-02 grows from 50 swaps over 5 seals to ~300 over ~30, and FAI-TC04a adds a 1064 nm transmittance check the original predates. Records that **FAI-TC02 remains BLOCKING and unmet** — the programme's oldest open production gate, unaffected by the hex-tile decision. Raises OI-FAI-01…04. |
