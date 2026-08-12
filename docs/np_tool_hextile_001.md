# Universal Hex-Tile Module Shell — Tooling Specification

**Project:** NeurOne
**Document:** NP-TOOL-HEXTILE-001
**Revision:** 1
**Date:** 2026-08-11
**Status:** DRAFT — Pre-Tooling. **Must be signed off before mould design commences.**
**Effective Date:** —
**Author:** NeurOne Mechanical Engineering
**Approved By:** —
**References:** NP-HEX-ZM-001 Rev 2 §3 (geometry), §4a (type taxonomy, SMART-1), §5.4a (cluster clamps), §6 (one universal mould), §7 (GATE-1, GATE-2, MECH-2); NP-HW-HEXTILE-001 Rev 3 §3–§7 (bezel, emitter layout, socket interface), D-3, D-5, OI-HEXTILE-01…17; NP-DRV-SHELL-002 Rev 2 §5.1 (19-contact array, REQ-SKT-01), §8.1–8.2 (clamp decoupling, zero dynamic flex); NP-THERM-BEZEL-001 Rev 1 §4.5 (bezel 1.0 mm — the only calculated figure); NP-TOOL-ZM-001 Rev 1 (superseded — feature-checklist pattern and the four features that carry forward); NP-TOOL-ZM-SM-001 Rev 1 (superseded — F-SM-03 confirmed unnecessary); NP-RISK-003 Rev 1; NP-FAI-001 Rev 1 §4–§5; NP-ART-001 Rev 1 §2.1; NP-PROC-SUP-001 Rev 1; ISO 10993-5; ISO 17049:2013
**Related Issues:** —
**Gate:** NP-COORD-001 G2 (pre-tooling) — mould design review, §4
**IEC 62304 Class:** N/A
**Supersedes:** **NP-TOOL-ZM-001 Rev 1** and **NP-TOOL-ZM-SM-001 Rev 1** — the position-unique mould family and its smart-module variant. Feature disposition in §1.2.
**Parent Document:** None — this document establishes the base hex-tile mould specification

---

> **The one-mould claim is the point of this document.** `NP-HEX-ZM-001` §6 states it plainly:
> *"one universal module-shell mould; element population is FPC/loadings only. This is the
> inventory/tooling win."* The retired family was five position-unique moulds plus a smart-module
> variant — six tools, six first-article inspections, six sets of features that could each be
> omitted before steel was cut, and a keying scheme (`NP-TOOL-ZM-001` F-01/F-02/F-03) whose entire
> purpose was preventing insertion into the wrong one.
>
> **This specification must not reintroduce a variant.** Every T1-A / T1-B / T1-C / T2-D difference
> is an FPC and element-population difference on an identical moulded body. Any proposed feature that
> differs by tile type is a request for a second tool, and it is refused here unless it comes with
> the tooling cost.

---

## 1. Scope

### 1.1 What this covers

The moulded thermoplastic body of the universal 40 mm hex tile, its co-moulded silicone gasket, and
every feature the mould must carry. One tool, all tile types.

**Not in scope:** the tile FPC, emitter placement and element population (`NP-HW-HEXTILE-001`); the
socket, contact array and L1 laminate (`NP-DRV-SHELL-002`); the cluster clamp plate and actuator
(`NP-HEX-ZM-001` §5.4a, MECH-2 — a different part on the opposite side of the same footprint,
deliberately decoupled per `NP-DRV-SHELL-002` §8.1); PDMS window bonding process
(`NP-FAI-001` §4).

### 1.2 Feature disposition from the retired moulds

Stated per `NP-CONV-001` §7 — what it was, what replaced it, and why.

| Retired | What it was | Disposition |
|---|---|---|
| `NP-TOOL-ZM-001` **F-01** | Asymmetric D-section key, **unique geometry per zone position**, preventing insertion into the wrong slot | **RETIRED.** Tiles are type-agnostic: any type inserts into any socket (`NP-HEX-ZM-001` §4a). There is no wrong socket to key against. Replaced by **F-TH-01**, an orientation-only key — the tile must not seat rotated, but it need not identify itself mechanically. |
| `NP-TOOL-ZM-001` **F-02** | Raised numeral 1–5 on the thumb-contact surface | **RETIRED.** The numeral encoded slot position. Position is now a property of the **socket**, not the module; the module reports its **type** by UID. A numeral on the module would name something the module does not have. |
| `NP-TOOL-ZM-001` **F-03** | ISO 17049 braille zone abbreviation (FL/FR/CL/CR/OC) | **RETIRED**, same basis as F-02. **The accessibility intent does not retire with it** — see §3 F-TH-01 note and `NP-RISK-004` RISK-22. |
| `NP-TOOL-ZM-SM-001` **F-SM-03** | Wider mechanical key preventing a smart module entering a non-smart slot | **RETIRED — confirmed 2026-07-28.** Under SMART-1 *every* socket is I2C/TIA-capable. Its dependent shell requirement (`OI-SM-SHELL-01`, dual-slot cutout) is gone with it — the requirement, not just the geometry. |
| `NP-TOOL-ZM-001` **F-04** | PD2 aperture, scalp-facing, 1.2 mm, 15° funnel, at X=33.0 / Y=39.0 mm | **CARRIES FORWARD as F-TH-05, position void.** The coordinates were the geometric centre of a 66 × 78 mm array and were marked *POSITION LOCKED*. They do not transfer to a 40 mm hex. **`OI-RISK3-02` owns the re-derivation.** This is the clearest example in the set of an inherited locked number surviving an architecture change unnoticed. |
| `NP-TOOL-ZM-001` **F-05** | Co-moulded D-section compression gasket, Shore 40–50 A, 2.5 × 2.0 mm, 20 % compression | **CARRIES FORWARD as F-TH-06**, re-dimensioned to the hex perimeter. |
| `NP-TOOL-ZM-001` **F-06** | 0.5 mm undercut retention groove + silicone primer | **CARRIES FORWARD as F-TH-07** unchanged in principle. |
| `NP-TOOL-ZM-001` **F-07** | LED baffle wall isolating PD1 from direct emission | **CARRIES FORWARD as F-TH-04**, and is **harder** — the baffle competes for area on a face roughly a third the size (`NP-RISK-003` §1.2). |
| `NP-TOOL-ZM-001` **F-08** | Sliding eject lever, ≥3:1 advantage, ≤1 N at the tip, for users with reduced grip | **RETIRED — replaced at a different level.** `NP-HEX-ZM-001` §5.4a moves extraction to one over-centre clamp per 3–7 tiles with per-module spring plungers. **The accessibility requirement is unchanged and is now unverified**: F-08 had a specified ≤1 N tip force; the cluster clamp has 34.2–57.0 N plate load and no equivalent figure. `NP-RISK-004` RISK-22 / `OI-SHELL2-03(b)`. |

**Net: three of eight features retired outright, four carry forward, one moved to a different
part.** All three retirements are consequences of the same decision — identity moved from the module
to the socket.

---

## 2. Mandatory feature checklist

Every feature below is **MANDATORY** and must appear in the mould design review (§4) before steel is
cut. Omission after first cut costs $15–40 k and 6–8 weeks per feature — this consolidated list is
the control for `RISK-23`.

| ID | Feature | Risk ref | Spec ref | Confirmed in mould design? |
|---|---|---|---|---|
| **F-TH-01** | **Orientation key** — asymmetric feature on the insertion face preventing the tile seating in any of the six rotations but one. Must engage before the pad array contacts the socket. Identical on every tile type. **Must be tactile**, discriminable by a user who cannot see the tile in place on their own head — this is what carries `NP-TOOL-ZM-001` F-03's accessibility intent forward. | RISK-12 | `NP-HEX-ZM-001` §4a | ☐ |
| **F-TH-02** | **Back-face pad-array pocket** — recess and datum for the 19-contact array in **two staggered rows** (REQ-SKT-01, binding: a 19-contact single row spans 38 mm and does not fit a 40 mm hex at 2.00 mm pitch). Mis-key asymmetry per F-TH-01; `SEAT#` at a mechanical extreme so it asserts last. | RISK-SHELL-01 | `NP-DRV-SHELL-002` §5.1.6 | ☐ |
| **F-TH-03** | **Spring-plunger landing** — the clamp plate's per-module plunger bears here to supply socket contact force. Must transmit load into the tile body and **not** into the pad array, the window, or a formed bend. The plate carries no conductors and is a separate part (`NP-DRV-SHELL-002` §8.1). | RISK-22 | `NP-HEX-ZM-001` §5.4a | ☐ |
| **F-TH-04** | **PD1 baffle wall** — internal moulded wall, height ≥ LED stack + 0.3 mm, isolating PD1 from direct emission so PD1 measures only light exiting through the PDMS window. Aperture aligned to PD1's field of view. **Omission invalidates the dose-metering claim**, which is the primary product differentiator. | RISK-06 | `NP-TOOL-ZM-001` F-07 (carried) | ☐ |
| **F-TH-05** | **PD2 aperture** — circular aperture in the scalp-facing window for backscatter measurement, 15° inward funnel entry, free of flash and burr (Ra ≤ 1.6 µm interior). **Position and diameter TBD — `OI-RISK3-02`.** Must be co-located in XY with PD1 on the emitting face; the retired X/Y coordinates are void. | RISK-14 | `NP-TOOL-ZM-001` F-04 (position void) | ☐ **BLOCKED** |
| **F-TH-06** | **Co-moulded perimeter gasket** — Shore 40–50 A UV-stable medical-grade silicone (ISO 10993-5), D-section, continuous around the full hex perimeter, compressed ~20 % when seated. No user re-sealing action. Must hold IPX4 after ≥ 10 swap cycles per tile. **Section dimensions TBD pending the bezel decision** — the gasket and the bezel compete for the same perimeter band. | RISK-16, RISK-19 | `NP-TOOL-ZM-001` F-05 (carried) | ☐ **BLOCKED** |
| **F-TH-07** | **Gasket retention groove** — 0.5 mm undercut around the hex perimeter accepting the gasket D-section heel; silicone primer applied to the groove before co-moulding. Mechanical retention plus chemical bond against delamination over the swap cycle count. | RISK-19 | `NP-TOOL-ZM-001` F-06 (carried) | ☐ |
| **F-TH-08** | **NTC pocket** — location for the per-tile thermistor that drives the 62 °C junction hardware throttle, plus the scalp-facing NTC co-located with PD2 required by `NP-REQ-FANHEALTH-001` Path B1. **Two thermal sensing locations, not one** — the retired module had only the junction-side sensor, and FMEA-G07-01 is the record of why that was insufficient. | RISK-26 | `NP-REQ-FANHEALTH-001` §4a | ☐ |
| **F-TH-09** | **Bezel** — the non-emitting perimeter band between the active field and the tile edge. **1.0 mm** (principal direction 2026-08-11; `NP-THERM-BEZEL-001` §4.5 is the only calculated value — module-face ceiling ~45.5 °C). **`NP-HW-HEXTILE-001` §3 still carries 2.5 mm as `OI-HEXTILE-01` and must be co-revised before this feature is dimensioned.** The difference is 14.5 % of active field area. | RISK-HEX-02 | `NP-THERM-BEZEL-001` §4.5 | ☐ **BLOCKED** |

---

## 3. Materials

| Property | Requirement | Standard |
|---|---|---|
| Body material | PC/ABS alloy or equivalent, UL94-V0 | UL 94 |
| **Colour** | **Single colour across all tile types.** The retired spec assigned one colour per zone position (ZM-01 Blue … ZM-05 Purple) to disambiguate slots. There are no slots to disambiguate; a per-type colour would imply a type-keyed socket that does not exist, and would reintroduce a variant against §1's one-mould rule. | — |
| Gasket | Shore 40–50 A UV-stable medical-grade silicone, skin-adjacent | ISO 10993-5 |
| Gasket primer | Dow Corning 1200 or equivalent, applied to the F-TH-07 groove | — |
| Window interface | PDMS bonded to polyimide via 75 nm SiO₂ interlayer + O₂ plasma; **qualification per `NP-FAI-001` §4, FAI-TC02 BLOCKING** | IEC 60068-2-14 |
| Socket rim finish | Ra ≤ 1.6 µm on gasket mating surfaces. **Whether moulded CFRP achieves this without secondary operations is `RISK-20`, open since 2026-05-06** and now applies to ~80 rims rather than 5 | — |

---

## 4. Mould design review checklist

All twelve items must be closed before steel is cut. Pattern retained from `NP-TOOL-ZM-001` §5 and
`NP-TOOL-HUB-001` §4.

| Item | Check | Status |
|---|---|---|
| THEX-MDR-01 | One tool, all tile types — no type-specific cavity, insert or colour | Open |
| THEX-MDR-02 | F-TH-01 orientation key engages before pad-array contact; tactilely discriminable in situ | Open |
| THEX-MDR-03 | F-TH-02 two staggered rows fit inside the tile inradius with ±0.4 mm blind-mate tolerance across a full 6-tile cluster (**cross-check SH2-DRC-05a**) | Open |
| THEX-MDR-04 | F-TH-03 plunger load path bypasses pad array, window and any formed bend | Open |
| THEX-MDR-05 | F-TH-04 baffle height ≥ LED stack + 0.3 mm **and** the baffle footprint does not push emitter count below the dose requirement at the final bezel | Open — depends on OI-HEXTILE-01, OI-HEXTILE-04 |
| THEX-MDR-06 | F-TH-04/F-TH-05 optical isolation: first-article opaque-tape test reads < 1 % of the normal LED-on signal at PD1 | Open |
| THEX-MDR-07 | F-TH-05 PD2 aperture position derived for the 40 mm face and co-located in XY with PD1 | Open — **BLOCKED, `OI-RISK3-02`** |
| THEX-MDR-08 | F-TH-06 gasket section and F-TH-09 bezel jointly fit the available perimeter band | Open — **BLOCKED, `OI-HEXTILE-01`** |
| THEX-MDR-09 | F-TH-06 perimeter length against the per-tile seam-length budget | Open — **BLOCKED, `OI-THEX-03`** |
| THEX-MDR-10 | F-TH-08 both NTC locations present — junction-side and scalp-facing co-located with PD2 | Open |
| THEX-MDR-11 | Draft, gate and ejector positions do not intersect the F-TH-04 baffle, F-TH-05 aperture or F-TH-07 groove | Open |
| THEX-MDR-12 | Steel-cut approval: THEX-MDR-01…11 all closed, and **GATE-1 and GATE-2 both PASS** | Open — **BLOCKED** |

---

## 5. FAI cross-reference

Per `NP-FAI-001` §2, **no FAI checklist can yet be issued for this artifact**; the blockers are
recorded in `NP-ART-001` §3.2 under `NP-FAI-HEXTILE-001`. The process qualifications that *are*
defined and that apply to this part regardless:

| Test | Where | Status |
|---|---|---|
| PDMS–PI bond, FAI-M01…M03 + FAI-TC01…TC06 | `NP-FAI-001` §4 | **FAI-TC02 BLOCKING, never run** |
| Post-service ingress, FAI-IPX-01…IPX-05 | `NP-FAI-001` §5 | FAI-IPX-02 BLOCKING; FAI-IPX-05 gated on OI-THEX-03 |

---

## 6. Tooling economics

| | Retired family | Hex tile |
|---|---|---|
| Moulds | 5 position-unique + 1 smart variant | **1** |
| Features that could be omitted before first cut | 8 × 6 tools | 9 × 1 tool |
| Colour SKUs in moulded stock | 5 | **1** |
| Service inventory | 6 part numbers | 1 body, N element populations |

This is the return `NP-HEX-ZM-001` §6 claimed, and §1's prohibition on variants is what protects it.

---

## 7. Open items

| ID | Description | Owner | Blocking |
|---|---|---|---|
| **OI-THEX-01** | **GATE-1 and GATE-2 must pass before this mould is cut.** GATE-1: curvature-scan bench validates Δκ ≈ 0.0039 across the 5th–95th percentile head map — until it does, a rigid 40 mm tile's seating against a real skull is unmeasured. GATE-2: PBM coupling at the temporal worst case, which is also the Option-A-vs-B go/no-go — a NO-GO changes the part, not the tooling. | ME + Optical | **Steel cut** |
| **OI-THEX-02** | Dimension F-TH-09 once `OI-HEXTILE-01` propagates the 1.0 mm bezel into `NP-HW-HEXTILE-001` §3. The value is directed; only the propagation is outstanding. Blocks F-TH-06 as well, since gasket and bezel share the perimeter band. | HW EE + ME | F-TH-06, F-TH-09 |
| **OI-THEX-03** | **Set the per-tile seam-length budget** (`NP-HEX-ZM-001` §6, `RISK-HEX-01`). ~30 seals in series carry one IPX4 claim and no budget exists. Feeds FAI-IPX-05. | ME + Quality | IPX4 claim; F-TH-06 |
| **OI-THEX-04** | Re-derive the F-TH-05 PD2 aperture position and diameter for the 40 mm face (`OI-RISK3-02`). | Optical + ME | F-TH-05; THEX-MDR-07 |
| **OI-THEX-05** | Confirm the F-TH-01 orientation key is tactilely discriminable **in situ** — on the head, by touch, by a user with tremor. `NP-TOOL-ZM-001` F-02/F-03 solved a version of this with a raised numeral and braille and were validated by an eyes-closed test; the requirement survived their retirement and currently has no HFE evidence behind it. | HFE + ME | Accessibility claim |
| **OI-THEX-06** | Add a **rigid-flex-into-moulded-carrier** and co-moulded-silicone supplier category covering this part to `NP-PROC-SUP-001`. Related to but distinct from `OI-SHELL2-04`, which covers the *active* cluster board. | Procurement | Supplier qualification |

---

## 8. Revision history

| Rev | Date | Author | Description |
|---|---|---|---|
| 1 | 2026-08-11 | NeurOne Mechanical Engineering | Initial release. Replaces `NP-TOOL-ZM-001` Rev 1 and `NP-TOOL-ZM-SM-001` Rev 1 — six position-unique tools become **one universal mould**, which `NP-HEX-ZM-001` §6 names as the tooling win the hex-tile decision was taken for. §1.2 dispositions all eight retired features individually: **F-01 key, F-02 numeral and F-03 braille all retire for one reason** — identity moved from the module to the socket, so a module can no longer name its own position; **F-08's eject lever moves to a different part** (cluster clamp) and its ≤1 N accessibility figure has **no equivalent in the replacement**, which is carried as RISK-22; four features carry forward as F-TH-04…07. Records that **`NP-TOOL-ZM-001` F-04's *POSITION LOCKED* PD2 coordinates are void** — they were the centre of a 66 × 78 mm array and are the clearest instance of a locked number surviving an architecture change unnoticed (`OI-RISK3-02`). Adds four features the hex architecture requires: F-TH-01 orientation-only key, F-TH-02 19-contact staggered pad pocket, F-TH-03 spring-plunger landing, F-TH-08 **two** NTC locations. §1 and §3 prohibit any type-specific mould variant or per-type colour, which is what protects the one-mould return. Three features (F-TH-05, -06, -09) and four review items open BLOCKED with their blockers named. Raises OI-THEX-01…06. |
