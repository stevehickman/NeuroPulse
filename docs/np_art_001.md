# Manufactured Artifact Register and Documentation Readiness

**Project:** NeurOne
**Document:** NP-ART-001
**Revision:** 6
**Date:** 2026-09-23
**Status:** ACTIVE
**Effective Date:** 2026-08-11
**Author:** NeurOne Systems Engineering
**Approved By:** — (new document)
**References:** NP-HEX-ZM-001 Rev 2; NP-HW-HEXTILE-001 Rev 3; NP-DRV-SHELL-002 Rev 2; NP-HW-HUB-001 Rev 3; NP-HELMET-GEOM-001 Rev 1; NP-TOOL-HUB-001 Rev 3; NP-TOOL-SHELL-001 Rev 3; NP-TOOL-LENS-001 Rev 2; NP-TOOL-HEXTILE-001 Rev 1; NP-FAI-001 Rev 2 §2.1, §3.1; NP-RISK-002 Rev 2; NP-CONV-001 Rev 6 §4, §6; NP-HW-TCAP-001 Rev 2; **NP-HW-AUDIO-001 Rev 1; NP-HW-NASAL-001 Rev 1; NP-HW-VNSCLIP-001 Rev 1; NP-HW-CVNS-001 Rev 1; NP-HW-TMS-001 Rev 1; NP-HW-TACSDRV-001 Rev 1**; NP-PWR-THERM-001 Rev 1 §8; `docs/superseded/README.md`
**Related Issues:** GitHub Issue #343 (OI-ART-05); **GitHub Issue #332 (OI-ART-04 — owning specifications for A11–A15; OI-TACS-02 — the driver stage, now A16)**
**Gate:** NP-COORD-001 G2 (pre-tooling) — this register is the input list for the G2 completeness check
**IEC 62304 Class:** N/A (documentation register)
**Supersedes:** None — first consolidated statement
**Parent Document:** None

---

> **Why this document exists.** Four documents were retired on 2026-08-11 because they specified,
> inspected, risk-assessed or tooled a part that the 2026-07-15 hex-tile decision deleted. Writing
> their replacements required first answering a question nobody had written down: **what does
> NeurOne actually manufacture?** The retired set answered it implicitly and wrongly — as "five zone
> modules, one shell, one hub" — and every gap below was invisible while that was the working list.
>
> This register is that list. For each artifact it records the governing specification, its maturity,
> and whether the three manufacturing-facing documents — **tooling specification**, **risk register**,
> **FAI checklist** — exist, are blocked, or are not applicable. **Where one is blocked, the blocking
> open item is named by ID**, so the register is a work list rather than an observation.

---

## 1. What counts as an artifact here

An artifact is a **separately manufactured physical part or assembly** that has, or will have, its
own tooling, its own incoming inspection, and its own failure modes. Firmware modules, protocols and
data schemas are out of scope — they are covered by `NP-SW-001` and the IEC 62304 process.

The register deliberately does **not** decompose to component level. `Hirose FH34S`, `STM32G071` and
`ADS1299` are purchased parts governed by `NP-PROC-SUP-001`, not artifacts.

---

## 2. The register

Status column: **✅** exists and current · **⚠** exists but architecture-coupled to the retired
design · **⛔** blocked, with the blocking item named · **—** not applicable.

### 2.1 Cranial assembly

| # | Artifact | Governing spec | Spec maturity | Tooling spec | Risk register | FAI checklist |
|---|---|---|---|---|---|---|
| **A1** | **Hex-tile module shell** — one universal 40 mm mould, all tile types | `NP-HEX-ZM-001` §6, `NP-HW-HEXTILE-001` | DESIGN STUDY | ✅ `NP-TOOL-HEXTILE-001` (DRAFT) | ✅ `NP-RISK-003` | ⛔ **GATE-1, GATE-2, OI-HEXTILE-01** |
| **A2** | **Hex-tile FPC + element population** — T1-A / T1-B / T1-C / T2-D | `NP-HW-HEXTILE-001` §4–§6 | DESIGN STUDY | — (FPC fab drawing, not a mould) | ✅ `NP-RISK-003` | ⛔ **OI-HEXTILE-02, -12, -05** |
| **A3** | **Socket + 19-contact array** on L1 | `NP-DRV-SHELL-002` §5.1, `NP-HW-HEXTILE-001` §7.1–7.2 | contact count CLOSED at 19 | ⛔ **OI-SHELL2-03(b)** | ✅ `NP-RISK-004` | ⛔ **OI-HEXTILE-11, SH2-DRC-05a** |
| **A4** | **Cluster controller board** (18 off, laminated into L1) | `NP-DRV-SHELL-002` §3.2 | DRAFT | ⛔ **OI-SHELL2-04** | ✅ `NP-RISK-004` | ⛔ **OI-SHELL2-04, -05, -11** |
| **A5** | **L1 inner-bowl laminate** (rigid-flex into moulded carrier) | `NP-DRV-SHELL-002` §4.1, `NP-HELMET-GEOM-001` §2 | DESIGN STUDY | ⛔ **MECH-1, OI-SHELL2-02** | ✅ `NP-RISK-004` | ⛔ **MECH-1** |
| **A6** | **L0 outer bowl** — CFRP shell + 5-layer EMF stack | `NP-HEX-ZM-001` §5, `NP-TOOL-SHELL-001` | ⚠ tooling spec written for 5 slots | ⚠ `NP-TOOL-SHELL-001` — see §5 | ✅ `NP-RISK-004` | ⛔ **MECH-1, EMF-1, EMF-3, RISK-20** |
| **A7** | **Cluster clamp plate + over-centre actuator** | `NP-HEX-ZM-001` §5.4a | CLUSTER-1 decided; geometry open | ⛔ **MECH-2** | ✅ `NP-RISK-004` | ⛔ **MECH-2, OI-SHELL2-03(b)** |

### 2.2 Control hub

| # | Artifact | Governing spec | Spec maturity | Tooling spec | Risk register | FAI checklist |
|---|---|---|---|---|---|---|
| **A8** | **Hub enclosure** (moulded housing, probe dock, fan door) | `NP-TOOL-HUB-001` Rev 3 | **BASELINED** — F-01's probe-contact geometry **GATED** on `NP-HW-NASAL-001` OI-NASAL-09 (Rev 2) | ✅ `NP-TOOL-HUB-001` | ✅ `NP-RISK-004` | ✅ **`NP-FAI-HUB-001`** Rev 3 — delivered, 6 items gated (FAI-HUB-14 on `OI-HTOOL-08`, Rev 3) |
| **A9** | **Hub PCB Rev C** | `NP-HW-HUB-001` Rev 3 | DRAFT | — (PCB fab package) | ✅ `NP-RISK-004` | ⛔ **OI-HUB-C01…C19** |
| **A16** | **21-channel clinical tACS driver stage** (T2) — 21 current sources, their sense paths, compliance supply and cap-side termination. **Added at Rev 3 (GitHub #332, `OI-TACS-02`)** | **`NP-HW-TACSDRV-001` Rev 1** (2026-09-20) | **DRAFT — requirements-grade.** No silicon (`OI-TCAP-03`); **no compliance voltage and no cap electrode impedance exist anywhere** (`OI-TACSDRV-02`) | — (PCB fab package) | ⛔ **none** — which register holds it depends on `OI-TACSDRV-01` | ⛔ **F1, F2, F4** — `NP-FAI-TACSDRV-001` named as an absence in §3.2 |

> **A16's siting is undecided, and the row says so deliberately.** `OI-TACSDRV-01` chooses between
> its own T2 board, Hub PCB Rev C, and the posterior aggregation node. **Rev 6: `NP-HW-TACSDRV-001`
> Rev 2 §4.1.1 recommends, but does not decide, its own T2-only board at the PAN, with the
> compliance-supply magnetics on the Hub PCB.** Under that recommendation the row stays. It is
> conditional on a PAN thermal budget (`OI-TACSDRV-04`). The PAN that would host A16 has no row in
> this register either, which is a third instance of `OI-ART-06` and is recorded against
> `OI-TACSDRV-03`. **If it lands on the Hub PCB,
> A16 folds into A9 and this row is struck** — a legitimate outcome, recorded here so that the fold
> is a decision rather than a disappearance. It is listed under §2.2 rather than §2.4 because two of
> the three candidate sitings are hub-side.
>
> **This is the second artifact found missing from §2 by following a citation rather than by auditing
> §2**, after `OI-ART-08`'s T2 clinical electrode cap — which is `OI-ART-06`'s premise demonstrated
> twice. The cap still has no row; that is `OI-ART-08`'s to add, not this revision's.

### 2.3 Optics and head-worn accessories

| # | Artifact | Governing spec | Spec maturity | Tooling spec | Risk register | FAI checklist |
|---|---|---|---|---|---|---|
| **A10** | **Lens rim + goggle arm + shade assembly** (incl. EC lens) | `NP-TOOL-LENS-001` Rev 2 | Pre-Tooling Draft | ✅ `NP-TOOL-LENS-001` | ✅ `NP-RISK-002` §4 routes to it | ⛔ **NP-TOOL-LENS-001 §OI-01…OI-10** |
| **A11** | **Audio cup assembly** — planar magnetic driver, bone conduction, replaceable mesh frame | **`NP-HW-AUDIO-001` Rev 2** (2026-09-23) | **DRAFT — requirements-grade, no dimensioned geometry** | ⛔ **`OI-AUDIOHW-04`, `-05`** (no driver selected, no geometry) | ⛔ **`OI-RISK2-02` — unblocked 2026-09-20**, inputs at `NP-HW-AUDIO-001` §7 | ⛔ **F1, F2** — see §3.2 |

### 2.4 Applicators and separate accessories

| # | Artifact | Governing spec | Spec maturity | Tooling spec | Risk register | FAI checklist |
|---|---|---|---|---|---|---|
| **A12** | **Intranasal bilateral Y-probe** + hygiene sleeve consumable | **`NP-HW-NASAL-001` Rev 1** (2026-09-20) | **DRAFT — requirements-grade.** Only the 15/20/25 mm depth-stop family is dimensioned, and **`OI-NASAL-02` is BLOCKING**: the probe has no exposure ceiling of its own | ⛔ **`OI-NASAL-01`** — and note this one runs the other way: `NP-TOOL-HUB-001` F-01 is **BASELINED** and dimensioned against probe features A12 does not yet specify | ⛔ **`OI-RISK2-02` — unblocked 2026-09-20**, inputs at `NP-HW-NASAL-001` §7 | ⛔ **F1, F2** — see §3.2 |
| **A13** | **Auricular VNS / HRV clip** | **`NP-HW-VNSCLIP-001` Rev 1** (2026-09-20), with `NP-FW-HRV-001` Rev 1 for the firmware half | **DRAFT — requirements-grade.** The pad area the Class C charge interlock divides by is a PROVISIONAL firmware constant (`OI-VNSCLIP-02`) | ⛔ **`OI-VNSCLIP-03`** (no geometry, no clamping force) | ⛔ **`OI-RISK2-02` — unblocked 2026-09-20**, inputs at `NP-HW-VNSCLIP-001` §7. **Rev 1 and Rev 2 of this register showed ✅ here and that was wrong** — the `NP-RISK-002` §4 row it pointed at is the one stating the clip's hazards are *"not assessed at all"*, which is a **named absence, not a register entry** | ⛔ **F1, F2, F4** — see §3.2 |
| **A14** | **Cervical VNS accessory** (T2) | **`NP-HW-CVNS-001` Rev 1** (2026-09-20), with `NP-FW-CVNS-001` Rev 4 and `NP-REG-CVNS-001` | **DRAFT — requirements-grade.** Four of its requirements are **predicate-coupled** and cannot be relaxed without re-running the substantial-equivalence argument | ⛔ **`OI-CVNSHW-06`** (no geometry) | ✅ `NP-RISK-002` §4 (RISK-25) — and §4's stated reason for holding it *outside* a per-artifact register (`OI-ART-04`, no specification to analyse) is now discharged | ⛔ **F1, F2** — `NP-FAI-CVNS-001` stays a **named absence**, now because its owning specification is DRAFT rather than because none exists; see §3.2 and `NP-FAI-001` §2.1 |
| **A15** | **TMS focal figure-8 coil** (T2) | **`NP-HW-TMS-001` Rev 1** (2026-09-20), adopting `SPEC-TMS-01…05` (`NP-PWR-THERM-001` §8) as carried requirements | **DRAFT — requirements-grade and GATED.** The electrical half is design-study grade on a geometry the study chose; **no mechanical value is set, deliberately** (`NP-HW-TMS-001` §5.1) | ⛔ **`OI-PWR-02`** and **`OI-PWRTH-01`**, both BLOCKING — the two candidate supply answers are 17× apart and a coil sized for either is a different part | ⛔ **A15 has no risk register of any kind**; `RISK-PWRTH-01` and `-03` sit in `NP-PWR-THERM-001` §15, not in a per-artifact register. Inputs at `NP-HW-TMS-001` §7 | ⛔ **F1, F2, F4** — the furthest of the five from F1, for a programme reason: `OI-PWR-02` asks whether the part should exist in this form |

---

## 3. The FAI answer, stated directly

The instruction that produced this register was: replace the retired `NP-FAI-ZM-001` with FAI
checklists for the artifacts that need one **if possible**, and where it is not possible, **name the
checklist and what must be resolved first**. That answer is:

### 3.1 Delivered

| Checklist | Artifact | Why it could be written |
|---|---|---|
| **`NP-FAI-HUB-001`** | A8 hub enclosure | `NP-TOOL-HUB-001` Rev 1 is the only artifact specification in the set with status **BASELINED**. Its four features carry dimensioned geometry (dock saddle, 1.0 mm anchor bosses, ≤20 mm tether, quarter-turn captive door) traceable to a released document, which is the minimum an accept criterion needs. |

Two of its items are gated rather than fabricated: `FAI-HUB-11` (Boa channel bend radius) waits on
**OI-HTOOL-02**, and `FAI-HUB-16` (ingress rating) waits on **OI-HTOOL-03**. Both are marked and
neither carries an invented number.

### 3.2 Blocked — the checklist, and what must be resolved before it can be created

| Checklist that cannot yet be written | Artifact | What must be resolved first |
|---|---|---|
| **`NP-FAI-HEXTILE-001`** | A1 hex-tile module shell | **GATE-1** — curvature-scan bench (5th–95th percentile head map) must validate Δκ ≈ 0.0039 across the vault. Until it passes, the 40 mm rigid tile's seating tolerance against a real skull is unmeasured, so there is no dimension to inspect *to*. **GATE-2** — PBM coupling bench: a rigid 40 mm coupon at the temporal worst case must meet dose spec; this is also the Option-A-vs-B go/no-go, and a NO-GO changes the part. **OI-HEXTILE-01** — bezel width is 2.5 mm in `NP-HW-HEXTILE-001` §3 and 1.0 mm in `NP-THERM-BEZEL-001` §4.5, and 1.0 mm was directed on 2026-08-11 but not yet propagated into HEXTILE. Bezel width sets the active field area (a 14.5 % swing) and therefore every irradiance accept criterion. |
| **`NP-FAI-HEXFPC-001`** | A2 hex-tile FPC + element population | **OI-HEXTILE-02** — the 660–670 nm and 808–830 nm emitters are not selected; §4.3's V_f and radiant-flux figures are design targets, not datasheet values, so there is no incoming-inspection criterion for the dominant BOM line. **OI-HEXTILE-12** — FPC stack-up, trace width/spacing and copper weight for a 24 V / 1.04 A tile are unspecified. **OI-HEXTILE-05** — T1-B's electrode-pod diameter, and hence its depopulated ring count and emitter count, is deferred to a later revision, so T1-B has no LED count to inspect. |
| **`NP-FAI-SOCKET-001`** | A3 socket + contact array | **OI-HEXTILE-11** — pogo contact qualification (≤50 mΩ over ≥500 cycles) has not been run **in the EEG signal path**; contact noise in a µV recording chain is not covered by a resistance figure, and that qualification defines the pass criterion. **SH2-DRC-05a** — the 19-contact two-staggered-row array (REQ-SKT-01) has not been shown in CAD to fit inside the tile inradius with ±0.4 mm blind-mate tolerance across a full cluster. |
| **`NP-FAI-CLUSTER-001`** | A4 cluster controller board | **OI-SHELL2-04** — no supplier category exists for *rigid-flex-into-moulded-carrier* assemblies, and since Rev 2 made the board active it must also cover MCU placement, firmware load, board-level functional test and IEC 62304 Class B traceability on a part laminated into a moulded body. An FAI checklist has to name the process it inspects. **OI-SHELL2-11** — per-controller dissipation is unbudgeted and no CFD case places the source on the gap-facing side of L1, so no thermal accept criterion exists. **OI-SHELL2-05** — BOM unconfirmed. |
| **`NP-FAI-L1-001`** | A5 L1 inner-bowl laminate | **MECH-1** — the four-corner clamp, posterior-centre connector boss and Hall interlock detail are undesigned, and the boss is what the laminate terminates into. **OI-SHELL2-02** — boss contact-group segregation with independent returns is unspecified and explicitly time-boxed *before* MECH-1 cuts the boss. |
| **`NP-FAI-SHELL-001`** | A6 L0 outer bowl | **MECH-1** as above. **EMF-1** — two-layer attenuation has not been shown to meet or beat the single-shell baseline, and the shielding claim is the shell's principal acceptance property. **EMF-3** — the gasket line-pressure map at the back-centre and both ear spans is unmeasured. **RISK-20** — whether CFRP can hold Ra ≤ 1.6 µm on rim contact surfaces without secondary operations is still unanswered by any tooling manufacturer in writing; that is simultaneously an FAI criterion and a supplier-qualification blocker (`NP-PROC-SUP-001` SUP-M-07 / SUP-B-01). |
| **`NP-FAI-CLAMP-001`** | A7 cluster clamp plate | **MECH-2** — actuator geometry, plate seating and one-handed input force at the 122 mm flower span are unverified. **OI-SHELL2-03(b)** — whether 34.2–57.0 N on a 6-tile plate is one-handed-achievable through the §5.4a over-centre actuator at Parkinson's H&Y II–III is an open human-factors question, and it is the plate's governing accept criterion (RISK-22). |
| **`NP-FAI-HUBPCB-001`** | A9 Hub PCB Rev C | **OI-HUB-C01 … OI-HUB-C19** — the board is not designed. `OI-HUB-C07` (whether the safety cut is per-cluster or whole-vault) and `OI-HUB-C19` (siting and sizing the 15–20 V → 24 V boost) both change the board's contents, not merely its layout. |
| **`NP-FAI-LENS-001`** | A10 lens / goggle / shade | The ten open items in `NP-TOOL-LENS-001` §OI. The N42 magnet pocket wall thickness (≥1 mm) is marked BLOCKING in that document and is an FAI dimension. |
| **`NP-FAI-CVNS-001`** | A14 cervical VNS accessory | **Rev 3 (2026-09-20): a hardware specification now exists — `NP-HW-CVNS-001` Rev 1 — and F1 still fails, on its DRAFT status rather than on absence.** `NP-HW-CVNS-001` §8.1 states the before/after against all four conditions and re-scopes `OI-FAI-07`; `REQ-CVNS-12` records that FAI-CV01's placement, impedance and open-detection criteria are now **A14's** criteria carried by a firmware test procedure, and that `NP-FAI-CVNS-001` inherits them when F1 is met. **The row below is Rev 2's text and is retained as the record of why the serial was named**, with its first clause now historical: **No hardware specification exists** (GitHub #332). The electrode assembly, cable and gel-pad consumable have firmware (`NP-FW-CVNS-001`) and a regulatory pathway (`NP-REG-CVNS-001`, gammaCore predicate) but no mechanical or electrical specification to inspect against, so `NP-FAI-001` §2 F1 fails outright. **Corrected at Rev 2:** Rev 1 read *"cited in three places as though the checklist existed"*, and those three citations — a CMake registration, a test binary's own document header, and a note in `NP-FW-CVNS-001` — are now re-pointed at `NP-FW-CVNS-001` §9, which is where FAI-CV01…CV03 actually live. **This row is the remaining citation, and it is meant to be one:** §2 requires an artifact failing F1 to be recorded as a named absence rather than written up with `TBD` accept criteria. It is not evidence of a document. `NP-FAI-001` §2.1, OI-FAI-07 |
| **`NP-FAI-NASAL-001`** · **`NP-FAI-AUDIO-001`** · **`NP-FAI-VNSCLIP-001`** · **`NP-FAI-TMS-001`** | A11–A13, A15 | **Rev 3 (2026-09-20): all four now have an owning specification, and all four checklists remain unwritable.** `NP-HW-AUDIO-001`, `NP-HW-NASAL-001`, `NP-HW-VNSCLIP-001` and `NP-HW-TMS-001` were issued against GitHub #332, and each states its own F1–F4 verdict in its §8.1. **F1 fails on all four (DRAFT), F2 fails on all four (no dimensioned geometry), and F4 fails on three** — A11–A13 are unblocked at `OI-RISK2-02` with hazard inputs supplied, and A15 has no risk register at all. The absence is therefore **narrowed and re-owned, not closed**: it is no longer *"no owning specification document exists"* but *"the owning specification is DRAFT and carries no dimensioned geometry"*, and each document names the open items that would change that. **A15 is the furthest from F1 and for a different reason** — `OI-PWR-02` (BLOCKING) asks whether the coil should exist in this form, so mechanical values are deliberately unset (`NP-HW-TMS-001` §5.1). Rev 1's text — *"no owning specification document exists at all … described only in `CLAUDE.md` §3 prose"* — is retained here as the record of the finding that produced these documents; see §4. |
| **`NP-FAI-TACSDRV-001`** | A16 21-channel clinical tACS driver stage | **Named here for the first time at Rev 3**, as `NP-FAI-001` §2's closing rule requires of an artifact failing F1. `NP-HW-TACSDRV-001` Rev 1 is DRAFT (F1); no silicon is selected and no compliance voltage exists, so there is no dimension or electrical parameter to inspect (F2, `OI-TCAP-03`, `OI-TACSDRV-02`); and which risk register holds its hazards depends on where the stage is sited (F4, `OI-TACSDRV-01`). **No FAI item anywhere inspects the driver today** — FAI-HD01's bench limb, HD03 and HD04 inspect the *cap* (`NP-FW-HD-001` §12), which is itself the subject of `OI-FAI-06` / `OI-ART-08`. |

---

## 4. The finding this register produced

Nine of the fifteen artifacts have **no owning specification document**, and four of those nine are
shipping modalities in the T1 flagship configuration: the intranasal Y-probe (modality 2), the
audio cup assembly (modality 7), and the auricular VNS/HRV clip (modality 6).

This was not visible before, and the reason it was not visible is instructive. The retired document
set was organised around *the zone module* — `NP-HW-FPC-001`, `NP-DRV-SHELL-001`, `NP-TOOL-ZM-001`,
`NP-FAI-ZM-001`, `NP-PROC-FPC-001`, `NP-COORD-001` all took the zone module as their subject. That
is six documents deep on one artifact and zero deep on four others that are in the same box. The
depth read as coverage.

**None of this is a new gap; it is a newly *visible* one.** No decision changes here. The four
missing-specification artifacts are recorded as open items below rather than resolved, because
writing a specification for the intranasal probe is engineering work, not documentation work.

### 4.1 What Rev 3 changed about that finding, and what it did not (2026-09-20, GitHub #332)

**Five owning specifications were issued** — `NP-HW-AUDIO-001` (A11), `NP-HW-NASAL-001` (A12),
`NP-HW-VNSCLIP-001` (A13), `NP-HW-CVNS-001` (A14) and `NP-HW-TMS-001` (A15) — plus
`NP-HW-TACSDRV-001` for the driver stage now registered as A16. **Every artifact in §2 now has an
owning document.**

**§4's closing sentence stands, and is the reason none of them is more than DRAFT.** Writing a
specification for the intranasal probe *is* engineering work, and none of it was done here. What
the five documents contain is (a) every requirement the existing record already binds, carried with
its citation, (b) the interfaces as shipped firmware and baselined tooling already fix them, and
(c) an enumerated statement of what is **not** specified, routed to open items. **No dimension,
material, emitter, exposure limit, force or price is created by any of them.**

So the finding moves from *"nine artifacts have no owning document"* to a precise and smaller
statement:

| | Before Rev 3 | After |
|---|---|---|
| Artifacts with **no owning document** | A11, A12, A13, A15 (and A16, unregistered) | **none** |
| Artifacts whose owning document is **DRAFT with no dimensioned geometry** | — | A11, A12, A13, A14, A15, A16 |
| Artifact FAI checklists writable under `NP-FAI-001` §2 | 1 of 15 (`NP-FAI-HUB-001`) | **1 of 16** — unchanged |
| `OI-RISK2-02` (hazard analysis for A11–A13) | **blocked** for want of a specification | **unblocked**; inputs supplied in each document's §7 |

**Three of the five documents raised a finding their artifact's absence had been hiding**, which is
the register's own thesis — that an unowned artifact is an unexamined one — arriving from the other
direction:

- **A12:** `NP-TOOL-HUB-001` F-01 is **BASELINED** and its saddle radius, insert-pad placement and
  detent force are dimensioned against probe features no document states (`OI-NASAL-01`); and the
  probe has **no exposure ceiling of its own** — `CLAUDE.md` §3's hard-limits table has a scalp row
  and a deep-PBM row and no intranasal row, the duty ceiling is inherited by a firmware comment, and
  the 42 °C limit is enforced by an NTC the probe does not have (`OI-NASAL-02`, BLOCKING).
- **A13:** the **A1/A2 linked-ear reference for the T2 21-channel qEEG sits on a consumable** that
  the auricular VNS current degrades by design, and only the stimulation duty has a replacement
  trigger (`OI-VNSCLIP-01`).
- **A14:** the cardiac interlock — RISK-25's control, and the *"key safety differentiator from
  predicate"* in `NP-REG-CVNS-001` — takes its R-peaks from **A13's PPG sensor**, a cross-artifact
  dependency stated in no device description, risk control or IFU (`OI-CVNSHW-03`).

**A correction of record to §4's first paragraph.** It reads *"four of those nine are shipping
modalities in the T1 flagship configuration"* and then names **three** — intranasal, audio cup,
auricular clip. Three is the correct count; the fourth artifact in the Rev 1 revision-history row
that accompanies it, the **TMS coil, is T2**, not T1. The list was right and the count was wrong.
Corrected here rather than in the Rev 1 text above or in §8's history row, which are records of what
was written (`NP-CONV-001` §1.1).

---

## 5. Architecture-coupled documents retained deliberately

Three active documents are coupled to the retired zone module. Each was left in place rather than
retired, because retiring it would leave a live artifact with **no** governing document — which is
worse than one with a stale section.

| Document | The coupling | Why it stays | Item |
|---|---|---|---|
| `NP-TOOL-SHELL-001` Rev 2 | F-01 is "zone slot plug anchor posts (**×5**, colour-coded)", keyed to the retired ZM-01…05 colour scheme. Both its stated parents — `NP-DRV-SHELL-001` and `NP-TOOL-ZM-001` — are now superseded. **Corrected 2026-08-18: the coupling is deeper than this row stated.** Rev 1 of this register asserted that F-02, F-03 and F-04 are architecture-independent. That claim was made without opening the file and **two-thirds of it is wrong.** **F-03 is not architecture-independent** — its governing rule is that the EEG cables and the zone-module FPC bundle sit on opposite sides of the shell wall, and `NP-DRV-SHELL-002` §9.1 quotes that rule and calls it unavailable; §5.3 absorbs the EEG harness entirely, so the channel has no cable to route. **F-02 is architecture-independent but conflicts** with `NP-TOOL-HUB-001` F-02 on the same hub ports (50 mm tether vs a safety-derived ≤20 mm maximum). Only **F-04 is architecture-independent and clean.** | It is still the **only** tooling specification the headset shell has, which is why it stays. F-04 remains releasable, and the anchor-post/tether pattern, boss-root radius rule, IPX4-at-boss-base requirement, draft-angle floors, §4 materials and the §5 checklist structure all survive as reusable engineering. **2026-09-23: F-02 retired in place (SHELL Rev 3) — hub port covers are `NP-TOOL-HUB-001` F-02 (`OI-ART-07` closed).** | **OI-ART-01** (~~OI-ART-07~~ closed) |
| `NP-COORD-001` Rev 1.10 | Titled *Zone Module FPC Engineering Coordination Checklist*; G1/G2/G3 gate items are scoped to an assembly that no longer exists. | Its gates are cited as the release gates of documents that **are** current, including `NP-TOOL-HUB-001` (G1) and `NP-DRV-SHELL-002` (G2). Retiring it would orphan those gate references. | **OI-ART-02** |
| `NP-PROC-FPC-001` **Rev 4** *(cited here as Rev 1 until 2026-09-21; the document was at Rev 3 behind a stale header)* | Specifies the Hirose FH34S 20-pin ZIF and the RA-copper requirements for a **tailed** FPC. Hex tiles have no tail (`NP-DRV-SHELL-002` §8.2 — module swap actuates zero flex). | The LED V_f binning and RA-copper requirements survive and are the only written procurement controls on the emitter supply. **Rev 4 (GitHub #333) strengthens that survival rather than narrowing it**: §2.1's ±0.10 V within-order bin is now stated as load-bearing for **string construction**, so the surviving half of this document is a premise of `NP-HW-HEXTILE-001` §8.1.1's fixed-N strings and not only a current-matching control. §2.6.2's emitter shortlist is corrected but **still selects nothing** — `OI-HEXTILE-02` is open, so `NP-FAI-HEXFPC-001` (§3.2, artifact A2) stays unwritable. | **OI-ART-03** |

---

## 6. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **OI-ART-01** | Re-scope `NP-TOOL-SHELL-001` to the two-bowl L0/L1 shell, or supersede it with `NP-TOOL-SHELL-002`. F-01's five colour-coded zone-slot plugs have no referent; the hex lattice has ~80 identical sockets, and whether unpopulated sockets need plugs at all is an open design question, not a renumbering. Cannot be completed before **MECH-1** fixes the clamp and boss geometry. | ME Lead | Shell tooling release |
| **OI-ART-02** | Re-scope `NP-COORD-001`'s gate structure from the zone-module FPC assembly to the artifact set in §2, **or** demote it to a gate-definition document and move the per-artifact items into the artifacts' own documents. Resolve alongside **OI-CONV-03** (its revision scheme) since both require re-issuing it. | Quality | G2 completeness |
| **OI-ART-03** | Split `NP-PROC-FPC-001` into the emitter-supply controls that survive (V_f binning, RA copper, certificates) and the tailed-FPC connector controls that do not. `NP-PROC-FPC-1064-001` needs the same treatment — its module-count and BOM figures already carry a supersession banner. | Procurement | Emitter sourcing |
| ~~OI-ART-04~~ | **✅ DISCHARGED 2026-09-20 (GitHub #332) as to its own words, and succeeded by `OI-ART-09`.** The item read: *write specifications for A11, A12, A13 and A15* — four shipping or planned artifacts with no design document, A12 and A13 in the T1 flagship box, and until they exist no tooling specification, risk register or FAI checklist can be written for any of them. **Five were written** — `NP-HW-AUDIO-001`, `NP-HW-NASAL-001`, `NP-HW-VNSCLIP-001`, `NP-HW-CVNS-001` (A14, the same defect, tracked on the same issue) and `NP-HW-TMS-001`, the last written against `OI-PWRTH-01` and `OI-PWRTH-09` as this item required. **A16's `NP-HW-TACSDRV-001` was written alongside them** for `OI-TACS-02`. **What the item claimed would follow does not follow yet, and saying so is the point of discharging it precisely** (§4.1): every one of the five is **DRAFT with no dimensioned geometry**, so `NP-FAI-001` §2's F1 and F2 still fail on all of them and **no artifact FAI became writable** — the count stays at 1 of 16. What *did* change: every artifact has an owning document, `OI-RISK2-02` is unblocked with hazard inputs supplied, and each document enumerates the engineering decisions its artifact needs by ID. **G2 pre-tooling completeness is still not declarable**, for the reason now carried by `OI-ART-09` rather than by this item. | — (discharged) | — |
| **OI-ART-09** | **Bring the six Rev 3 specifications to a maturity that satisfies `NP-FAI-001` §2 F1–F2.** `OI-ART-04`'s successor, and deliberately narrower: the artifacts now have owning documents, and what they need is **engineering**, not authoring. Per document: `NP-HW-AUDIO-001` (`OI-AUDIOHW-01…07`, of which the acoustic and bone-conduction exposure ceilings are absent programme-wide); `NP-HW-NASAL-001` (`OI-NASAL-02…09`, with **`OI-NASAL-02` BLOCKING** and **`OI-NASAL-09` gating an already-BASELINED hub mould** — `OI-NASAL-01` closed 2026-09-23 by recording that gate, Rev 4); `NP-HW-VNSCLIP-001` (`OI-VNSCLIP-01…07`, including the `OI-CHARGE-07` electrode-area measurement); `NP-HW-CVNS-001` (`OI-CVNSHW-01…08`, including the cross-artifact interlock dependency); `NP-HW-TMS-001` (`OI-TMS-01…06`, itself gated behind **`OI-PWR-02`** and **`OI-PWRTH-01`**, both BLOCKING); `NP-HW-TACSDRV-001` (`OI-TACSDRV-01…03` plus `OI-TCAP-03`). **The two items to start with are the ones that block work already in flight:** `OI-NASAL-01`, because a baselined tool is dimensioned against an unspecified part — **closed 2026-09-23 (Rev 4) by recording the gate; the hub mould's release now turns on `NP-TOOL-HUB-001` OI-HTOOL-07, which is the faster route than `OI-NASAL-09`** — and `OI-TACSDRV-01`, because it decides which artifact row and which risk register own A16. | Systems + ME + EE | **G2 completeness; T1 tooling release; every artifact FAI except `NP-FAI-HUB-001`** |
| ~~OI-ART-05~~ | **✅ CLOSED 2026-09-14 (GitHub #343) — four of the five were duplicate serials; the fifth is a real absence and stays named.** The item read: `NP-FAI-CVNS-001`, `NP-FAI-HD-001`, `NP-FAI-SM-001`, `NP-FAI-CV-001` and `NP-FAI-ANON-001` are cited across the document set and in firmware test names (`firmware/cervical_vns/tests/np_cvns_fai_tests.c`, `firmware/sloreta_hdtdcs/tests/np_hd_fai_tests.c`) as though they were controlled documents; **none has ever existed**; either create them or re-point the citations. **Resolution: re-point, for four of them.** FAI-SM-01…11, FAI-HD01…HD04, FAI-CV01…CV03 and FAI-ANON-01…09 are each already specified in a firmware specification's own FAI section — `NP-FW-PBM1064-001` §11, `NP-FW-HD-001` §12, `NP-FW-CVNS-001` §9, `NP-FW-ANON-001` §9 — so those four serials were second names for content that exists, and they are **retired** (`NP-FAI-001` §2.1; `NP-DHF-001` §8 rows struck through). `NP-FAI-CV-001` was also a second name for **this register's own** `NP-FAI-CVNS-001`. **`NP-FAI-CVNS-001` is not created**, because it fails `NP-FAI-001` §2 F1 — A14 has no hardware specification (#332) — and §2 requires that case to be a named absence, which is §3.2's row above. **Each candidate was tested as an artifact checklist in its own right and none passes**, so no checklist was written: the smart-module hardware items belong to `NP-FAI-HEXFPC-001` (OI-FAI-05), the HD-tDCS bench items to a cap with no register row (**OI-ART-08**, OI-FAI-06), and anonymisation is out of the FAI programme's scope per `NP-FAI-001` §1. **Root cause, closed at `NP-FAI-001` §3.1:** an `FAI-<GROUP><NN>` item prefix was read as implying an `NP-FAI-<GROUP>-001`, and `NP-CONV-001` §6 never listed the FAI item family. | — (closed) | — |
| ~~OI-ART-07~~ | **✅ CLOSED 2026-09-23 — `NP-TOOL-HUB-001` owns every tethered cover on the hub; `NP-TOOL-SHELL-001` F-02 is retired in place (SHELL Rev 3, HUB Rev 3), and the retirement surfaced a defect in the survivor that is raised, not papered over.** The item read: **`NP-TOOL-SHELL-001` F-02 and `NP-TOOL-HUB-001` F-02 specify the same hub port covers with incompatible geometry, and both documents are ACTIVE.** SHELL F-02: three anchor posts on the control hub shell (USB-C, left and right hub accessory ports), Ø 4.0 mm × 3.5 mm boss, 50 mm tether. HUB F-02: two anchor posts (USB-C, DFU/service), 1.0 mm dia × 0.5 mm protrusion, **≤20 mm free-length tether**. The tether maximum is a **hazard control**, not a styling choice — it exists so a detached-but-tethered cover cannot reach the F-04 fan intake grille (`FAI-HTOOL-02`, marked BLOCKING). SHELL's 50 mm exceeds it by 2.5×. `NP-TOOL-HUB-001`'s back-reference is also wrong in both halves: it describes SHELL F-02 as covering "the headset's zone-slot and lens-rim accessory ports", but F-02 is on the hub, and lens-rim guards are out of scope in SHELL §1 and belong to `NP-TOOL-LENS-001` F-07. **Decide which document owns hub port covers** (HUB is later, BASELINED, and its limit is safety-derived), delete the duplicate, and correct the back-reference. Independent of MECH-1 — this one can be closed now. **Resolution.** *Owner:* HUB, on three independent grounds — SHELL §1 lists "control hub tooling" as **out of its own scope** and its §9 sends hub anchor posts to "a separate document" (which became HUB), so F-02 was hub geometry in a shell spec that excluded the hub; HUB's ≤ 20 mm maximum is the control for `RISK-HUB-01` verified by `FAI-HUB-23` [BLOCKING], where SHELL has no reach limit; and HUB is BASELINED with an issued FAI checklist. *Duplicate:* retired, not deleted (CLAUDE.md §18 — retire is not delete): §2.2, SH-06…08, FAI-CV-05/06 and the F-02 rows of §2–§4 stay verbatim under a banner (`editscripts/patch_art07_tool_shell_f02_retirement.py`, idempotent). *Back-reference:* already corrected on 2026-08-18 — HUB's purpose paragraph no longer carries it; recorded as done. **Two corrections this item's own text needed.** (i) **"2.5×" is overstated:** SHELL's 50 mm is a loop **circumference** (§2.2), so reach is ≈ 25 mm — about 1.25× the limit. It still exceeds a hazard-control maximum, which is disqualifying at any ratio. (ii) **"Delete the duplicate" would have orphaned real ports.** Of the two documents, SHELL named the right ports and HUB the right tether. HUB F-02's second port, "DFU/service", appears in no other specification (DFU runs over USB-C, `firmware/bootloader/src/np_dfu.c`), while the hub accessory port(s) that A12, A13, A14 and the mastoid pad connect to — whose covers ship as `NP-ACC-PRIORITY-001` rank 11 — are in neither document once SHELL F-02 goes. Raised as **`NP-TOOL-HUB-001` `OI-HTOOL-08`** (port inventory and cover retention method — `docs/reference/service-network.md` §8.3 sells magnetic covers, HUB specifies compression-fit) and **`OI-HTOOL-09`** (HUB's 1.0 × 0.5 mm boss is borrowed from an *alignment* boss and has no derivation against `FAI-HUB-19`'s 10 N tether pull); `FAI-HUB-14` is `[GATED: OI-HTOOL-08]`. **The ≤ 20 mm maximum is not reopened by either** and applies to every tethered cover the hub ends up with. | — (closed) | — |
| **OI-ART-08** | **The T2 clinical electrode cap is a manufactured artifact with no row in §2.** `NP-HW-TCAP-001` (Rev 1 §8, carried at Rev 2) says so itself — *"the cap artifact has no register entry"* — and its absence has a concrete consequence this register is supposed to prevent: FAI-HD01's phantom limb, FAI-HD03 and FAI-HD04 (`NP-FW-HD-001` §12) inspect the cap, and with no register row there is no risk register to satisfy `NP-FAI-001` §2 F4 and no artifact to name a checklist after. Surfaced by the OI-ART-05 disposition rather than by §2, which is `OI-ART-06`'s point exactly: an artifact absent from the source lists does not appear here. Add the row (with A-number, governing spec, maturity, and the three document columns), then name its checklist from the register. Resolve with **OI-TCAP-06** and **OI-ART-06**. | Systems | T2 verification planning; `NP-FAI-001` OI-FAI-06 |
| **OI-ART-06** | Confirm this register is complete. It was assembled from `CLAUDE.md` §3–§4 and the current specification set; an artifact that appears in neither would not appear here. Verify against the design brief BOM and the box-contents list at §2.1. | Systems | Register integrity |

---

## 7. Cross-references

- **FAI method and the items carried forward from `NP-FAI-ZM-001`:** `docs/np_fai_001.md`
- **Delivered FAI checklist:** `docs/np_fai_hub_001.md`
- **Risk file re-baseline and the disposition of RISK-01…26:** `docs/np_risk_002.md`
- **Per-artifact risk registers:** `docs/np_risk_003.md` (A1/A2), `docs/np_risk_004.md` (A3–A9)
- **Hex-tile mould tooling:** `docs/np_tool_hextile_001.md`
- **Shell interconnect design review record:** `docs/np_rev_shell_001.md`
- **What was retired and why:** `docs/superseded/README.md`

---

## 8. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 6 | 2026-09-23 | NeurOne Systems Engineering | **A16 note updated for `NP-HW-TACSDRV-001` Rev 2.** `OI-TACSDRV-01` is analysed and a siting is recommended, not decided: own T2-only board at the posterior aggregation node, with compliance-supply magnetics on the Hub PCB. Under that recommendation A16 keeps its row rather than folding into A9, conditional on `OI-TACSDRV-04` (the PAN's thermal budget). Records that the PAN has no row in §2, a third `OI-ART-06` instance, against `OI-TACSDRV-03`. No row added or struck. |
| 5 | 2026-09-23 | NeurOne Systems Engineering | **`OI-ART-07` closed.** `NP-TOOL-HUB-001` (Rev 3) owns every hub port cover; `NP-TOOL-SHELL-001` F-02 retired in place (Rev 3). §5's SHELL row and §2's A8 row updated. Corrects the item's "2.5×" (loop circumference, ≈ 1.25×) and records that the survivor's port list is unsourced — raised as `OI-HTOOL-08` / `OI-HTOOL-09` in `NP-TOOL-HUB-001`, `FAI-HUB-14` gated on the first. |
| 4 | 2026-09-23 | NeurOne Systems Engineering | **A8 row and `OI-ART-09` updated for `NP-HW-NASAL-001` OI-NASAL-01's closure.** `NP-TOOL-HUB-001` Rev 2 records F-01's probe-contact geometry as gated on `OI-NASAL-09`, and `NP-FAI-HUB-001` Rev 2 gates three more items (2 → 5). The A8 row stays BASELINED and its FAI stays writable — F1 turns on the governing specification's status, and F2 is met by *marking* a gate, which is what was done. The writable-checklist count is unchanged at **1 of 16**. What changed is that A8's hub mould is now visibly blocked on A12, and `NP-TOOL-HUB-001` OI-HTOOL-07 names the design change that would unblock it. No artifact row added or removed. |
| **3** | **2026-09-20** | **NeurOne Systems Engineering** | **Every artifact in §2 now has an owning specification document, and no artifact FAI became writable — both halves stated (GitHub #332, `OI-ART-04`, `OI-TACS-02`).** Six specifications were issued against this register's principal finding: **`NP-HW-AUDIO-001`** (A11), **`NP-HW-NASAL-001`** (A12), **`NP-HW-VNSCLIP-001`** (A13), **`NP-HW-CVNS-001`** (A14), **`NP-HW-TMS-001`** (A15, written against `OI-PWRTH-01` and `OI-PWRTH-09` as `OI-ART-04` required) and **`NP-HW-TACSDRV-001`** for the 21-channel clinical tACS driver stage, **registered here as A16** — the second artifact found missing from §2 by following a citation rather than auditing it, after `OI-ART-08`'s cap, which is `OI-ART-06`'s premise demonstrated twice. **A16's siting is undecided and its row says so: if `OI-TACSDRV-01` puts the stage on Hub PCB Rev C, A16 folds into A9 and the row is struck.** **`OI-ART-04` is discharged as to its own words and succeeded by `OI-ART-09`**, which is narrower: the artifacts need engineering, not authoring. **What did not change is stated as plainly as what did** (§4.1): all six documents are **DRAFT with no dimensioned geometry**, F1 and F2 fail on every one, the writable-FAI count stays at **1 of 16**, and **G2 pre-tooling completeness is still not declarable**. What did: `OI-RISK2-02` is **unblocked** — A11–A13 have hazard-analysis inputs for the first time — `NP-RISK-002` §4's stated reason for holding RISK-25 outside a per-artifact register is discharged, and `NP-FAI-CVNS-001`'s F1 failure changes from *absence* to *DRAFT status*, re-scoping `OI-FAI-07`. **Three findings the artifacts' own documents surfaced** are carried into §4.1: `NP-TOOL-HUB-001` F-01 is **BASELINED against probe features A12 does not specify**; the intranasal probe has **no exposure ceiling of its own** (BLOCKING); the T2 qEEG's A1/A2 reference sits on a **consumable degraded by another modality**; and A14's cardiac interlock depends on **A13's PPG sensor** with that dependency stated nowhere. **Two corrections of record:** A13's risk-register column showed ✅ against a `NP-RISK-002` §4 row that states its hazards are *not assessed*, and §4's *"four of those nine"* miscounts a list of three (the fourth, the TMS coil, is T2). **No engineering value is set or changed by this revision, no figure is invented, and no locked decision moves.** Raises OI-ART-09. |
| **2** | **2026-09-14** | **NeurOne Quality** | **`OI-ART-05` closed (GitHub #343): the five `NP-FAI-*` serials cited as controlled documents are dispositioned, and four of them were duplicate serials rather than missing documents.** FAI-SM-01…11, FAI-HD01…HD04, FAI-CV01…CV03 and FAI-ANON-01…09 are each already specified inside a firmware specification's own FAI section, so `NP-FAI-SM-001`, `NP-FAI-HD-001`, `NP-FAI-CV-001` and `NP-FAI-ANON-001` are retired and their citations re-pointed; the full disposition, including the F1–F4 verdict on each as a checklist in its own right, is `NP-FAI-001` §2.1. **`NP-FAI-CVNS-001` is deliberately still unwritten**: it fails F1 (A14 has no hardware specification, #332), and §3.2's row is what `NP-FAI-001` §2 requires of that case. Both the A14 row in §2.4 and the §3.2 row are reworded so neither reads as a document that exists — and Rev 1's *"cited in three places"* is now stale, because those three citations were re-pointed at `NP-FW-CVNS-001` §9. **Raises OI-ART-08** — the T2 clinical electrode cap has no §2 row, which is why the HD-tDCS bench items have no F4 to satisfy; found by following OI-ART-05 rather than by auditing §2, which is `OI-ART-06`'s premise demonstrated. **No artifact, specification, maturity, tooling or risk-register entry changes, and no engineering value is set or changed by this revision.** |
| 1 | 2026-08-11 | NeurOne Systems Engineering | Initial release. Enumerates the fifteen manufactured artifacts (§2) and records, per artifact, whether its tooling specification, risk register and FAI checklist exist or are blocked — with every blocker named by open-item ID (§3.2). Written to answer the replacement question for the retired `NP-FAI-ZM-001`. **Principal finding (§4): nine of fifteen artifacts have no owning specification document, four of them shipping T1 modalities** — the intranasal Y-probe, audio cup assembly, auricular VNS/HRV clip and TMS coil are described only in `CLAUDE.md` prose. Raises OI-ART-01…06. §5 records the three architecture-coupled documents deliberately retained rather than superseded, each with its reason. No engineering value is set or changed by this document. |
