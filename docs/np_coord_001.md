# Engineering Gate Definitions and Coordination Item Register

**Project:** NeurOne
**Document:** NP-COORD-001
**Revision:** 12
**Date:** 2026-09-25
**Status:** ACTIVE
**Effective Date:** 2026-09-25
**Author:** NeurOne Systems Engineering + Quality
**Approved By:** — (re-issue pending Quality approval)
**References:** NP-ART-001 Rev 8 §2, §6; NP-FAI-001; NP-RISK-002 §3; NP-RISK-003; NP-RISK-004; NP-HW-HEXTILE-001 Rev 13 D-9; NP-DRV-SHELL-002; NP-REV-SHELL-001; NP-TOOL-HEXTILE-001; NP-PROC-FPC-001 Rev 8; NP-CONV-001 Rev 9 §4.0.4, §4.1, §7.1; NP-PRIV-REM-001 STEP-14; NP-INT-FHIR-001; NP-MOD-ID-001 §10; `docs/superseded/np_coord_001.docx` (Rev 11, retained)
**Related Issues:** GitHub Issue #394 (`OI-ART-02`, `OI-CONV-03`); #330; #13; #7; #5
**Gate:** N/A — this document defines the gates
**IEC 62304 Class:** N/A (plan / gate definitions)
**Supersedes:** NP-COORD-001 Rev 11 (`Rev A.10`, `.docx`, *Zone Module FPC Engineering Coordination Checklist*)
**Change Summary:** Demoted from a per-assembly checklist to a gate-definition document. The gates stay `NP-DP-001`'s phase-exit reviews; what each review checks is now defined per artifact (`NP-ART-001` §2). Every Rev 11 item ID is dispositioned in §4, so every existing citation still resolves. The dotted revision scheme is flattened to integers (§6).
**Review Cadence:** At every gate declaration, and whenever `NP-ART-001` §2 gains or loses a row

---

> **Why this revision exists.** Rev 11 was titled *Zone Module FPC Engineering Coordination
> Checklist*. Its gates were scoped to an assembly that the 2026-07-15 hex-tile decision deleted. Its
> gate *names* are still the release gates of current documents: `NP-ART-001` is "the input list for
> the G2 completeness check", and about thirty headers cite `NP-COORD-001 G1`, `G2` or `G3`.
> Retiring the document would orphan those citations, so `NP-ART-001` §5 kept it. Keeping it as
> written left the gates defined only by reference to a part that no longer exists.
>
> `OI-ART-02` offered two ways out: re-scope the gate items to the artifact set, **or** demote this
> document to gate definitions and move the per-artifact items into the artifacts' own documents.
> **This revision takes the second.** The test that decided it: of Rev 11's 38 item IDs, 24 are carried (§4), and every one of those
> 24 already has a home in an artifact's own specification, risk register or FAI programme. A
> re-scoped checklist here would be a second copy of those controls, which is the duplication
> `OI-CONV-07` exists to remove.

---

## 1. Scope

This document holds four things and nothing else:

1. **What gates G1, G2 and G3 check** (§2). The gates themselves, and when they are held, are
   `NP-DP-001`'s.
2. **The rule for what may be a gate item** (§3).
3. **The disposition of every coordination item ID issued by Rev 11 and earlier** (§4). An item ID
   is an address, and every one of them keeps resolving.
4. **The programme items that belong to no manufactured artifact** (§5), because no artifact
   document can own them.

It does **not** hold per-artifact items. Those live in the artifact's governing specification,
tooling specification, risk register or FAI checklist, as named in `NP-ART-001` §2.

---

## 2. Gate definitions

**The gates belong to `NP-DP-001`, and this document does not move them.** `NP-DP-001` §6.1 and §10
define G1, G2 and G3 as **phase-exit design reviews** under 21 CFR §820.30(e): G1 closes Phase 1
(system architecture), G2 closes Phase 2 (detailed design) and G3 closes Phase 3 (design
verification). The design plan sets when each review is held, who attends (§10.1), the independence
rule (§10.3) and the programme-level conditions (§10.2: for example, G1 is not passed while RISK-03
or RISK-20 is open). **None of that changes here.**

**What this document defines is what each review checks.** `NP-DP-001` §10.1 says each review
"uses the NP-COORD-001 checklist for the relevant gate". Rev 11 answered with one checklist for one
assembly, which no longer exists. **Rev 12 answers per artifact.** At each gate, the review
dispositions every `NP-ART-001` §2 row against the criterion below, using that artifact's own
documents. The gate passes when every row passes or is recorded in the review minutes as out of
scope for this gate, with the reason. The "G2 completeness check" that `NP-ART-001` and its
per-artifact specifications cite is this disposition, taken at G2.

| Gate | `NP-DP-001` phase exit | Criterion for an artifact released as a **PCB/FPC fabrication package** (today A2, A4, A5, A9, A16) | Criterion for an artifact with a **tooling specification** (`NP-ART-001` §2 "Tooling spec" column) |
|---|---|---|---|
| **G1** | Phase 1, system architecture (`NP-DP-001` §6.3) | Every decision that fixes the layout is frozen: parts whose footprint, pinout or rating fix it are selected, and the artifact's G1 items are closed. **Layout may begin** | The tooling specification is **BASELINED** ("tooling specs frozen", `NP-DP-001` §6.1). **No steel may be cut** before this (`NP-DP-001` §6.3) |
| **G2** | Phase 2, detailed design (`NP-DP-001` §6.4) | Layout complete and released for fabrication ("PCB layouts complete", §6.1) | The tooling specification is **released to a toolmaker** and the artifact's design review record is signed (for example `NP-REV-SHELL-001` for A3–A7) |
| **G3** | Phase 3, design verification (`NP-DP-001` §6.5) | The artifact's FAI checklist is executed and passes under `NP-FAI-001` (F1–F4 satisfied), and every §5 programme item naming the artifact or its tier is closed | The same |

An artifact with neither a fabrication package nor a tooling specification (for example A10, whose
tooling specification is also its governing specification) is checked on the column that matches
how it is released. `NP-ART-001` §2 records which that is.

**Two properties Rev 11 had, and this keeps:**

- **An open BLOCKING item at a passed gate is not possible** (`NP-DP-001` §10.2). A conditional pass
  carries written closure criteria and a re-verification date.
- **No gate item may be waived.** It may be closed, retired under §3, or moved to another gate by a
  revision of the document that owns it.

**One property Rev 11 had, and this does not.** Rev 11's checklist was one list for *the* assembly,
so one item's status stood for all of it. A per-artifact disposition shows the state the programme
is actually in: the hub mould (A8) has a BASELINED tooling specification and an issued FAI
checklist, while the hex-tile mould (A1) is DRAFT and blocked on GATE-1.

---

## 3. What may be a gate item

A gate item is a requirement on a release, so `NP-CONV-001` §7.1 (CLAUDE.md §18) applies to it in
full. Before an item enters any artifact's gate list it must answer **what fails if the gate opens
with this item open** and **where that is traceable** (a hazard control in `NP-RISK-003`/`-004`, a
standard, a measurement or a derivation).

An item whose subject no longer exists is **retired in place** (§4), never deleted. An item whose
subject survives but whose control changed form is **carried**, and its row names the successor
control. **An item this document cannot trace is raised as an open item (§7), never retired.**

---

## 4. Disposition of every Rev 11 coordination item

**Legend.** **CARRIED** means the subject survives, and the successor column names where the item now
lives and what its control is. **CLOSED** means it was closed at Rev 11 or earlier, and the record
stands. **RETIRED** means the subject no longer exists in the design. A retired row stays here so
that its ID resolves. **PROGRAMME** means the item belongs to no artifact and is carried in §5.

The *risk* column is the Rev 11 RISK-nn, followed by its `NP-RISK-002` §3 disposition. That table is
the authority on whether a hazard was carried or retired, and this table follows it. Where they
disagree, `NP-RISK-002` wins, and the disagreement is a defect in this table.

### 4.1 Gate G1 (Rev 11: pre-layout of the zone-module FPC)

| ID | Rev 11 item (abbreviated) | Rev 11 status | Risk → `NP-RISK-002` | Disposition | Successor: where it lives now |
|---|---|---|---|---|---|
| G1-01 | Regulatory opinion letter, 400 mW/cm² peak pulsed | BLOCKING | RISK-03 → OPEN, `NP-RISK-003` | **CARRIED** (A2) | RISK-03 in `NP-RISK-003`; `pending-decisions.md` §13.1 and §13.1a; GitHub #5. Still BLOCKING and still uncommissioned |
| G1-02 | Set RSET values for the approved current | OPEN, awaiting G1-01 | RISK-03 | **CARRIED, control changed form** (A2) | The drive current is now bounded by **`REQ-TDRV-01`'s `I_cap`**, a hardware reference that no firmware can raise (`NP-HW-HEXTILE-001` Rev 13 D-9). No resistor ladder is specified. `OI-HEXTILE-25` owns the sizing |
| G1-03 | LED PWM carrier-frequency EDR | OPEN | RISK-13 → CARRIED, `NP-RISK-004` | **CARRIED, control changed form** (A2, A4) | `NP-DRV-SHELL-002` `REQ-EMI-03` (sense-quiet windows) and `REQ-EMI-04` (spread-spectrum PWM prohibited), verified by `SH2-DRC-16` (< 5 µVpp) and recorded in `NP-REV-SHELL-001` |
| G1-04 | 50 µs EEG blanking window in firmware | OPEN | RISK-13 | **CARRIED** (A2, A4) | As G1-03. The fixed 50 µs figure is Rev 11's. `REQ-EMI-03` is the governing requirement |
| G1-05 | ≤ 25 % LED duty cycle as a safety-MCU limit | OPEN | RISK-02 → CARRIED, `NP-RISK-003` | **CARRIED, control changed form** (A2) | `NP-HW-HEXTILE-001` Rev 13 D-9 `REQ-TDRV-02` puts the average-power bound in a gate-path circuit outside U1. **The firmware duty limit that CLAUDE.md §3 states is not retired by this row.** Its enforcement is `NP-FW-PBM1064-001`'s and the hub's. See §7 `OI-COORD-01` for the miscitation of this ID |
| G1-06 | Single PD with two measurements vs a second PD | **CLOSED** (Option B, dual PD) | RISK-14 → CARRIED, `NP-RISK-003` | **CLOSED**. The decision carries | The hex tile keeps dual PD in every type. **Rev 11's pin-19 `PD2_CATHODE` assignment and the X = 33.0 / Y = 39.0 mm aperture do not transfer** (`NP-RISK-002` RISK-14) |
| G1-07 | LED emitter part numbers confirmed | BLOCKING | RISK-03 / OI-03 | **CARRIED** (A2) | **`OI-HEXTILE-02`** (no 660/808 nm emitter selected). It also blocks `OI-HUB-C08` and the uncosted term U (CLAUDE.md §2.1) |
| G1-08 | Hirose FH34S 1,000-cycle rating from datasheet | OPEN | RISK-01 → **RETIRED** | **RETIRED** | Hex tiles have no connector. `NP-DRV-SHELL-002` §8.2 replaced the tail with a back-face compression pad array. The socket contact array is A3, and its own items are `OI-HEXTILE-11` and `SH2-DRC-05a` |
| G1-09 | LED V_f ±0.10 V bin control confirmed by supplier | OPEN | RISK-08 → CARRIED, `NP-RISK-003` | **CARRIED** (A2) | `NP-PROC-FPC-001` §2.1, which is **load-bearing for string construction** (Rev 4). It cannot close while `OI-HEXTILE-02` has no emitter to bin |
| G1-10 | BOM updated to BCR421W, no BCR421U | OPEN | RISK-07 → **RETIRED** | **RETIRED** | D-3 put a driver on every tile. *A driver operated beyond its rating* is now inside `OI-HEXTILE-07` |
| G1-11 | Photodiode part, baffle geometry, integrated-TIA decision | OPEN, mould-blocking | RISK-06 → CARRIED, `NP-RISK-003` | **CARRIED** (A1, A2) | Baffle: `NP-TOOL-HEXTILE-001` F-TH-04. PD selection: `NP-HW-HEXTILE-001`. The 40 mm face makes it harder (`NP-RISK-002` RISK-06) |
| G1-12 | SiO₂ sputtering process house confirmed | OPEN | RISK-04 → CARRIED, `NP-RISK-003` | **CARRIED** (A1, A2) | RISK-04 in `NP-RISK-003`. The qualification is `NP-FAI-001` `OI-FAI-01`, and GitHub #7 |
| G1-13 | ZONE_ID firmware debounce | **CLOSED** 2026-05-10 | RISK-18 → CARRIED, re-scoped | **CLOSED**. Its subject is retired | The ZONE_ID pin is gone. The hazard shape transfers to `SEAT#` (`NP-RISK-004` RISK-18; `NP-DRV-SHELL-002` `SH2-DRC-10b`) |
| G1-14 | PDMS thermal-cycling qualification (FAI-TC02) | OPEN, BLOCKING | RISK-04 | **CARRIED** (A1, A2) | As G1-12. **FAI-TC02 has never been run** (`NP-RISK-002` RISK-04) |
| G1-15 | FPC layout freeze and PD2 aperture position | **CLOSED** 2026-05-09 | — | **CLOSED**. Its subject is retired | Closed against `NP-HW-FPC-001` (superseded). The PD2 coordinates do not transfer (see G1-06). The hex-tile PD2 position is re-derived in `NP-HW-HEXTILE-001` |
| G1-16 | eMMC partition architecture and dual-bank OTA spec | **CLOSED** 2026-05-11 | — | **CLOSED** | `NP-FW-EMMC-001` (now Rev 3). Firmware, not an artifact |

### 4.2 Gate G2 (Rev 11: pre-tooling of the zone-module shell)

| ID | Rev 11 item (abbreviated) | Rev 11 status | Risk → `NP-RISK-002` | Disposition | Successor: where it lives now |
|---|---|---|---|---|---|
| G2-01 | Multi-FPC and EEG cable separation | NEEDS RE-SCOPE | RISK-17 → CARRIED, `NP-RISK-004`; RISK-11 → **RETIRED** | **RETIRED as scoped; its hazard is CARRIED** (A3–A5) | `NP-DRV-SHELL-002` (requirements and the 33-item DRC) and `NP-REV-SHELL-001` (the record that gates first cut) |
| G2-02 | Universal orientation-key geometry | OPEN | RISK-15 → **RETIRED** | **CARRIED as a feature**, not as a hazard control (A1) | `NP-TOOL-HEXTILE-001` F-TH-01, `THEX-MDR-02`, and `OI-THEX-05` (in-situ tactile discrimination) |
| G2-03 | Stiffener datum features on the FPC drawing for CMM | OPEN | RISK-12 → CARRIED, re-scoped, `NP-RISK-004` | **CARRIED** (A2, A3) | The alignment tolerance is now `REQ-SKT-01` and `SH2-DRC-05a` (±0.4 mm across a full cluster). The FPC datum features belong to A2's FAI, `NP-FAI-HEXFPC-001`, which is a named absence (`NP-ART-001` §3.2) |
| G2-04 | `NP-DRV-SHELL-001` DRC sign-off, CAD items | OPEN | RISK-11 → **RETIRED** | **RETIRED**. Its record function is CARRIED | `NP-DRV-SHELL-001` is superseded. Its sign-off function is `NP-REV-SHELL-001` (SH2-DRC-01…28) |
| G2-05 | IPX4 field-replacement seal design decision | OPEN | RISK-16 → CARRIED, **severity increased**, `NP-RISK-003`; RISK-19 | **CARRIED** (A1) | RISK-16 and RISK-19 in `NP-RISK-003`. The seal is a moulded feature of `NP-TOOL-HEXTILE-001`, with about 30 perimeter seals where there were 5 |
| G2-06 | Approved LED current ceiling as a safety-MCU constant | OPEN, awaiting G1-01 | RISK-03 | **CARRIED, control changed form** (A2) | As G1-02. `REQ-TDRV-01` puts the ceiling in a hardware reference, which is stronger than a firmware constant |
| G2-07 | PWM carrier and blanking locked as constants | OPEN, awaiting G1-03 | RISK-13 | **CARRIED** (A2, A4) | As G1-03 |
| G2-08 | PDMS-fouling vs LED-ageing algorithm | OPEN | RISK-14 | **CARRIED** (A2) | RISK-14 in `NP-RISK-003`. Dual-PD dose metering, `NP-HW-HEXTILE-001` |
| G2-09 | PWM carrier harmonic analysis | OPEN | RISK-13 | **CARRIED** (A2, A4) | As G1-03. `REQ-EMI-04` changes the analysis: a fixed carrier is *required*, because dither smears energy into the acquisition band |
| G2-10 | Audio zone-ID firmware | PARTIAL | RISK-15 → **RETIRED** | **RETIRED** | `NP-FW-ZA-001` is superseded (`docs/superseded/np_fw_za_001.md`). Module identity is `NP-HEX-ZM-001` UID auto-inventory |
| G2-11 | EEG cable routing in the shell | **CLOSED** 2026-05-10 | RISK-21 → CARRIED, re-scoped; RISK-24 → CLOSED-CONFIRMED | **CLOSED**. Its subject is retired | There are no EEG cables to route. Electrodes live in T1-B tiles on L1 (`NP-DRV-SHELL-002` §3.5, §9.1) |
| G2-12 | HRV biofeedback firmware | **CLOSED** 2026-05-11 | — | **CLOSED** | `NP-FW-HRV-001` §10 |
| G2-13 | Lens and goggle assembly as a G2 deliverable | OPEN | — | **CARRIED** (A10) | `NP-TOOL-LENS-001`, and its `OI-01…OI-10` |
| G2-14 | Hub control program | PARTIAL | — | **PROGRAMME** (§5) | `NP-FW-HUB-001` |

### 4.3 Gate G3 (Rev 11: pre-production)

| ID | Rev 11 item (abbreviated) | Rev 11 status | Risk → `NP-RISK-002` | Disposition | Successor: where it lives now |
|---|---|---|---|---|---|
| G3-01 | PDMS thermal-cycling qualification, 200 cycles | OPEN | RISK-04 | **CARRIED** (A1, A2) | As G1-14 |
| G3-02 | Physical-prototype FPC routing path test (DRC-14) | OPEN | RISK-11 → **RETIRED** | **RETIRED** | The interconnect has no dynamic-flex path (`NP-DRV-SHELL-002` §8.2) |
| G3-03 | IPX4 test after 10 field-replacement cycles | OPEN | RISK-16 | **CARRIED** (A1) | `NP-FAI-001` §4–§5 (IPX4 after service; about 300 swaps rather than 50). The checklist is `NP-FAI-HEXTILE-001`, not yet writable (GitHub #330) |
| G3-04 | PWM blanking oscilloscope validation on prototype | OPEN | RISK-13 | **CARRIED** (A2, A4) | `SH2-DRC-16` |
| G3-05 | Incoming inspection on first production lots | OPEN | RISK-08, -07, -01, -10 | **CARRIED, narrowed** (A2, A5) | `NP-PROC-FPC-001` §7, for the emitter and flex-laminate lots only. **The FH34S and BCR421W lines are retired** with RISK-01 and RISK-07. See §7 `OI-COORD-01` for the miscitation of this ID |
| G3-06 | `NP-FAI-ZM-001` full FAI pass | OPEN | all | **RETIRED**. Its function is CARRIED | `NP-FAI-ZM-001` is retired. G3 is now "the artifact's FAI checklist passes" (§2), and the checklists are enumerated in `NP-ART-001` §3 |
| G3-07 | sLORETA-guided HD-tDCS, T2 | SOFTWARE BASELINED | RISK-03 | **CARRIED** (cap artifact, no row yet) | `NP-FW-HD-001` §12–§13. The hardware half inspects the T2 clinical electrode cap, which has **no `NP-ART-001` row** (`OI-ART-08`) |
| G3-08 | Cervical VNS, T2 | SOFTWARE BASELINED | RISK-25 → CARRIED, §4 | **CARRIED** (A14) | `NP-FW-CVNS-001` §12 (firmware) and `NP-HW-CVNS-001` (artifact). RISK-25 is in `NP-RISK-002` §4 |
| G3-09 | FHIR ImplementationGuide approved before the first T2 EHR integration pilot | **Never issued.** Required at "Rev 1.9" by `NP-PRIV-REM-001` STEP-14, and cited by `NP-INT-FHIR-001`, but absent from the Rev 11 `.docx` | — | **PROGRAMME** (§5). **Issued at this revision** | `NP-INT-FHIR-001`; `NP-PRIV-REM-001` STEP-14 |

**Count, from the three tables and not asserted:** 39 IDs. G1 has 16, G2 has 14 and G3 has 9. Rev 11
issued 38 of them, and G3-09 is issued here for the first time.

| Disposition | G1 | G2 | G3 | Total |
|---|---|---|---|---|
| CARRIED (including *control changed form*, *narrowed* and *as a feature*) | 10 | 8 | 6 | **24** |
| CLOSED | 4 | 2 | 0 | **6** |
| RETIRED (subject gone; hazard or function carried where one exists) | 2 | 3 | 2 | **7** |
| PROGRAMME (§5) | 0 | 1 | 1 | **2** |
| **Total** | **16** | **14** | **9** | **39** |

Rev 11 and its citations never agreed on the count: "27 coordination items" (Rev 1 record), "G1 15 ·
G2 14 · G3 6" (`NP-DT-001` DO-TEST-02 and `NP-DP-001` §10.1), and "G1 16 · G2 14 · G3 8"
(`document-register.md`). **The authority is the three tables above.**

---

## 5. Programme items

These belong to no manufactured artifact. Each has a source that requires it (§3).

| ID | Gate | Item | Required by | Owner | Status |
|---|---|---|---|---|---|
| G2-14 | G2 (T1 and T2, all artifacts carrying SW-02) | The hub control program's design outputs are documented and its HAL stubs are connected to prototype hardware | Rev 11 G2-F: every modality FAI item needs the session runner to dispatch commands, so the runner must work before prototype validation starts | Firmware Lead | **PARTIAL.** Specified in `NP-FW-HUB-001`; the HAL stubs wait on prototype hardware |
| G3-09 | G3 (T2) | `NP-INT-FHIR-001` is approved before the first T2 EHR integration pilot | `NP-PRIV-REM-001` STEP-14 | Clinical informatics + Privacy Lead | **OPEN** |

**Proposed and not adopted: `MOD-ID-1`** (`NP-MOD-ID-001` §10). It is proposed as a gate on
`NP-COORD-001` and it is a verification plan, not a gate item. It names no artifact and no gate.
Adopting it means answering which gate and which artifacts, and that is `OI-COORD-03`.

---

## 6. Revision labels: the dotted scheme resolved (`OI-CONV-03`)

Rev 11 carried a two-part scheme, `A`, `A.1` … `A.10`, that no other document used and
`NP-CONV-001` §4.1 did not describe. The 2026-08-11 conversion mapped only the major, which gave
`Rev 1.10`. `OI-CONV-03` held that flattening would assert an ordering the history did not
establish, because it was not recorded whether `A` and `A.1` were distinct issues.

**Resolved by reading the eleven change records**, which is what `OI-CONV-03` asked for. Git cannot
answer the question: the `.docx` enters history whole at the 2026-09-14 import (`79539c3`), so no
earlier blob exists. The document can answer it:

- **The ordering is established by the labels themselves.** `A < A.1 < … < A.10` is an order, and
  every record's content is consistent with it: A.2 closes G2-11, A.3 adds G2-13, A.8 adds G2-14,
  and so on. No record references content from a later one.
- **`A.1` is a distinct change set.** Its record (carried in the `A` row's fifth cell, with no date
  of its own) extends G2-02 and adds G2-10. Both were absent from `A`'s "27 coordination items".
  Whether it was *issued* separately is unknowable, and the flat number does not claim it was. **A
  flat revision number names a change set, not an issue event.** `A.1`'s date is recorded below as
  unknown rather than inferred.

The mapping is positional and total:

| Rev 11 label | Corpus citation after 2026-08-11 | Flat revision | Date |
|---|---|---|---|
| A | Rev 1 | **1** | 2026-05-06 |
| A.1 | Rev 1.1 | **2** | unknown (recorded in the `A` row, no date) |
| A.2 | Rev 1.2 | **3** | 2026-05-10 |
| A.3 | Rev 1.3 | **4** | 2026-05-10 |
| A.4 | Rev 1.4 | **5** | 2026-05-11 |
| A.5 | Rev 1.5 | **6** | 2026-05-11 |
| A.6 | Rev 1.6 | **7** | 2026-05-11 |
| A.7 | Rev 1.7 | **8** | 2026-05-11 |
| A.8 | Rev 1.8 | **9** | 2026-05-16 |
| A.9 | Rev 1.9 | **10** | 2026-06-28 |
| A.10 | Rev 1.10 | **11** | 2026-07-14 |
| — | — | **12** (this document) | 2026-09-25 |

**Existing citations are not rewritten.** `Rev 1.8` in `NP-DT-001` and `NP-DHF-001` §7 resolves
through this table to Rev 9. That follows `NP-CONV-001` §4.1: a published mapping keeps history
resolvable without editing it. The three sources that disagreed on the current revision were the
DHF (`A.8`), `document-register.md` (`Rev 1.9`) and the `.docx` itself (`A.10`). **The `.docx` was
right: Rev 11.** The DHF row is corrected with this issue.

**Where Rev 11 went.** It moves to `docs/superseded/np_coord_001.docx`, unedited. It is a design
record under `NP-QMS-001` §Records (life of device + 2 years, 21 CFR §820.180), and it is referenced,
so `NP-CONV-001` §4.0.4 retains it. The revision history of Rev 1–11 is that file's §6. It is not
copied here: one copy of a history cannot disagree with another.

---

## 7. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **OI-COORD-01** | **Two `NP-DP-001` §6 activity rows cite item IDs that name something else.** §6.3 cites **G1-05** for the zone-module mould design review, but G1-05 is the LED duty-cycle limit. §6.6 cites **G3-05** for T1 first-in-human tolerability (n ≥ 10), but G3-05 is incoming inspection. The first row's subject is retired: its successor is A1's mould design review (`NP-TOOL-HEXTILE-001` §4, GitHub #330). The second row's subject is live, and **it has no gate item anywhere**. It is a Phase 4 validation activity, and `NP-DP-001` gives Phase 4 no gate, so whether it needs one is a design-plan decision. It is not invented here. Resolve in the `NP-DP-001` revision that `OI-COORD-02` needs | Quality + Clinical Lead | `NP-DP-001` gate plan |
| **OI-COORD-02** | **`NP-DP-001` still describes the Rev 11 checklist.** §6.3's G1 exit criterion reads *"All 15 items in NP-COORD-001 G1 checklist"*, and §10.1 sizes the three checklists at 15 / 14 / 6 items. §6.3 also lists the zone-module mould review and the ZONE_ID debounce as Phase 1 activities, and §11.2 names `NP-FAI-ZM-001` as the home of FAI-TC02 and FAI-IPX-02. Under §2 of this revision the gate checks are a per-artifact disposition over `NP-ART-001` §2. `NP-DP-001` is the design plan (21 CFR §820.30(b)), so it is revised by its owner and not edited in passing from here. **Nothing in `NP-DP-001` §10 conflicts with §2**: the phase gates, attendees, independence rule and the RISK-03 / RISK-20 condition stand as written | Quality (design plan owner) | Next G1 review |
| **OI-COORD-03** | **Adopt, place or decline `MOD-ID-1`** (`NP-MOD-ID-001` §10). It names neither a gate nor an artifact | Firmware + Data Architecture | None |
| **OI-COORD-04** | **Per-artifact gate lists do not yet exist as named lists.** §4 names where each carried item lives, but no artifact document yet carries a section headed *G1/G2/G3 items*. `NP-TOOL-HUB-001` §5 and `NP-TOOL-HEXTILE-001` §5 (mould design reviews) are the nearest. Add the section to each artifact's governing document as it is next revised, and make G2 completeness in `NP-ART-001` checkable against those lists | Systems | G2 completeness (programme-level) |

---

## 8. Revision history

Revisions 1–11 are recorded in `docs/superseded/np_coord_001.docx` §6, under the labels `A` … `A.10`.
§6 above maps them.

| Rev | Date | Author | Description |
|---|---|---|---|
| **12** | **2026-09-25** | NeurOne Systems Engineering + Quality | **Re-issued in Markdown as a gate-definition document (`OI-ART-02`, GitHub #394), with the dotted revision scheme resolved (`OI-CONV-03`).** The gates themselves stay `NP-DP-001`'s phase-exit reviews (§6.1, §10) and are not moved. **What each review checks** is re-defined as a per-artifact disposition over `NP-ART-001` §2, with separate criteria for fabrication-package and tooled artifacts, reconciling Rev 11's release semantics with `NP-DP-001` §6.1's phase deliverables (§2). The rule for gate items is tied to `NP-CONV-001` §7.1 (§3). **Every Rev 11 item ID dispositioned** against `NP-RISK-002` §3 (§4): 24 carried, 6 closed, 7 retired, 2 programme. G3-09 is issued for the first time: `NP-PRIV-REM-001` STEP-14 and `NP-INT-FHIR-001` had required it since 2026-06-03 and cited it as if it existed. Revision labels flattened `A`…`A.10` → 1…11, with the reasoning and the one unknown (`A.1`'s date) stated (§6). Rev 11 retained unedited at `docs/superseded/np_coord_001.docx`. Raises `OI-COORD-01…04`. |
