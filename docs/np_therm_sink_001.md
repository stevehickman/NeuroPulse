# Rejection Specification at the Via Terminus — What `R_sink` Is, and What Is Attached To It

**Project:** NeurOne
**Document:** NP-THERM-SINK-001
**Revision:** 2
**Date:** 2026-09-23
**Status:** DESIGN STUDY — **specification of the term `NP-THERM-CFD-N1-001` `OI-N1-02` declares BLOCKING.** It is a resistance-network study on the delivered 80-socket lattice, not a mesh CFD and not a bench measurement; `OI-R1-01` (mesh independence) and `OI-R1-02` (THERM-1b correlation) both remain open. It closes `OI-N1-02` by *recovering* the number rather than choosing one, and it changes the answer to `N1-D-1`.
**Effective Date:** —
**Author:** NeurOne Thermal / Systems Engineering
**Approved By:** — (pending design review)
**References:** NP-THERM-CFD-N1-001 Rev 1 (§2.1 the network and its lateral terms, §4a/§5 the `R_sink` sweep, §5.1 the export-vs-rejection inversion, §6a per-protocol ceilings, §6b the per-tile drive wall, N1-D-1…7, `OI-N1-01…08` — the parent document); NP-THERM-CFD-R1-001 Rev 1 (§2 the outward path `R_OUT_BASE`, §3 the inward-flux ceiling, §5 the BN-boss export study and its "perfect sink", §5.2 the fault case, §5.3 findings, `OI-R1-01…06`); NP-THERM-CFD-001 (§4 heat-source model and η_wp, §5 BC spec); NP-THERM-CFD-C2-001 (§7 the 1D network and its sign convention); NP-THERM-COOL-001 Rev 11 (§4 the 90 %/2 % split, §5 the cooling options, §6.4 `OI-R1-03` — the fan cools the outer shell only, §6.7 the remote-sink accessories, D-2/D-4 — Rev 11's withdrawal is confined to §7.4.4's Class B/C latch ownership and touches none of these); NP-HELMET-GEOM-001 (§2 the radial stack-up, §3.2 the L1 BN bosses, §3.3 the outer bowl and its unbroken 4-layer inner face, §8 THERM-1a); NP-HEX-ZM-001 (§5.1 the outer bowl as the complete EMF envelope, §5.3 the parting plane and labyrinth lip); NP-PWR-BUDGET-001 Rev 3 (§3.2 the 4–8 tile estimate, §3.3 export efficiency, §3.5 the N = 80 extrapolation, D-4, `OI-PWR-01/08/10`); NP-SES-PWR-001 Rev 1 (§2.1 the tile-count governor, the library floor and ceiling); NP-PWRSRC-001 Rev 1 (§5 thermal dose, §11 the min() governor, `OI-PWRSRC-05`); NP-HW-HUB-001 Rev 5 (`OI-HUB-C19` hub thermal budget, HUB-REQ-C04); NP-TOOL-HUB-001 Rev 1 (§2 the inferred occipital-arch placement, §3 F-04 the fan/heatsink access door, `OI-HTOOL-03/04`); NP-REQ-FANHEALTH-001 (SR-FAN-01…06, Path B1); NP-FMEA-GEOM-001 (FMEA-G07-01); NP-ENV-OPRANGE-001 (§2 the ambient/duty envelope); NP-DT-001 Rev 2 (DI-SAFE-13, DI-REG-01 IEC 60601-1); NP-CONV-001 Rev 6 (§4 identifiers, §8 a convention worth writing down is worth a script); CLAUDE.md §3 (PBM ceilings), §4.2 (42/62 °C interlocks), §4.3 (the 4-layer EMF stack), §4.4 (fit system mass), §4.5 (power), §5.1 (SHDR fan RPM); IEC 60601-1 (42 °C applied part); `hardware/np_socket_map.json`; `scripts/check-thermal-sink.ts`; `scripts/check-thermal-multitile.ts` · **Rev 2:** NP-EMC-CAV-001 §7, §9 item 4, §10 (`OI-EMCCAV-02`); NP-BIB-EMF-001 §7.2, §7.6; NP-HEX-ZM-001 §5.3, §5.3.1; NP-DRV-SHELL-002 `REQ-EMI-10`, `REQ-EMI-11`; `hardware-detail.md` §4.3 (D1); NP-HW-TMS-001 `REQ-TMS-08`; NP-REG-UPG-001 `OI-UPG-05`; WHO EHC 238 (ELF, 0–300 Hz); NP-THERM-BOWL-001 §5.2–§5.3 (the active-loop share and the thermal drift of the same transfer function); NP-HEX-ZM-001 §5.6.1 (the `EMF-1` test plan)
**Related Issues:** GitHub Issue #399 (`OI-EMCCAV-02`, Rev 2); GitHub Issue #391
**Gate:** Does not close THERM-1a. Closes `OI-N1-02` and **supersedes `N1-D-1`'s prohibition with a number**; raises one BLOCKING item of its own (`OI-SINK-01`).
**IEC 62304 Class:** — (analysis document; no code ships from it)
**Supersedes:** None — specifies a term `NP-THERM-CFD-N1-001` left open
**Parent Document:** NP-THERM-CFD-N1-001 §5 (`OI-N1-02`)

---

> **⚠ READ FIRST — the one-paragraph version.**
>
> `OI-N1-02` asks for a heatsink specification. There is no heatsink to specify. R1's BN-boss via is
> a **~32 mm radial conductor** through the layer stack: it terminates on the **outer bowl**, not at
> the hub a median 188 mm away, and nothing in the tree specifies a part that would connect the two
> — nor can one exist inside the mass budget. So the "external fan-cooled heatsink" is the outside
> of the helmet, its resistance is the **external film that is already the last term of R1's own
> `R_OUT_BASE = 0.41`**, and R1 pins that film at ambient for the via while charging it in full to
> the cavity. **Recovered from R1's own numbers, and independently from a natural-convection
> correlation over the helmet's exterior — two derivations sharing no input, agreeing to 4.7 % —
> `R_sink` is 1.08 K/W** (band 0.83–1.58). That validates N1's 1.00 K/W row and makes its 0.50
> "plausible default" 2.2× optimistic. **But a lumped `R_sink` presumes an isothermal terminus, and
> the bare CFRP bowl is not one:** it spreads over ~63 mm, so the exterior is eighty hot spots
> rather than one sink. **The missing component is a spreader, not a heatsink** — and with the best
> one costed here the ceiling at the library floor is **10 tiles at 25 °C**, against N1's
> 16–78. **`N1-D-1` can be lifted; what replaces it is worse news than the prohibition was.**

---

## 1. Headline

1. **The terminus is the shell (§2).** Against `NP-HELMET-GEOM-001` §2's radial stack, R1's via
   spans module body → socket wall → clamp → inter-bowl gap → the outer bowl: **~32 mm** as computed here, **~29 mm** since the
   2026-09-23 Layer 4 absorber deletion took 3 mm off the outer bowl (`REQ-CAV-04`), ending on the outer bowl. `NP-TOOL-HUB-001` §2 *infers* a hub heatsink at the occipital arch from
   R1 §5's phrase "external fan-cooled heatsink". The sockets are **20–262 mm (median 188)** from
   that point and **no document specifies a conductor between them.**
2. **Neither collection architecture survives its own arithmetic (§2.1, §2.2).** A solid trunk drops
   `ρQL²/km`: at the median distance, **100 g of copper costs 40 K at the N = 6 library floor and
   193 K at the N = 6 R-4 point**, against a total face budget of 17.0 K at 25 °C. And a heatsink
   bolted to the shell at the occiput — even at **`R_hub` = 0** — **adds zero tiles to the ceiling**,
   because the shell rejects locally faster than it conducts 188 mm.
3. **`R_sink` is not a free parameter; it is the last term of `R_OUT_BASE` (§3).** Decomposing R1's
   own `R_cav→amb = 0.18` into its solid layers leaves **0.115 m²K/W of external film** — h = 8.71
   W/m²K over the tile footprint, **8.18 over the real exterior area**. A Churchill sphere
   correlation plus linearised radiation over that same exterior gives **7.81**. The two share no
   input and agree to **4.7 %**.
4. **Specified: `R_sink` = 1.08 K/W aggregate (§4).** Band 0.83–1.58 K/W from the corners of the two
   soft inputs. **N1's sweep bracketed the truth**; its 1.00 K/W row is the real one and its 0.50
   "small fan-cooled extruded sink" was **2.2× optimistic**.
5. **R1 spends the same film twice (§5).** Reproducing R1's cell in this network takes **two**
   settings — pin the skin at ambient *and* charge the cavity leg the full 0.18 with the film still
   in it. Restoring the film on both paths is the entire content of this document; every other
   resistance, node and lateral term is R1's, unchanged, and the leg sum is **0.230 + 0.065 + 0.115
   = 0.410 exactly**.
6. **The exterior starts 6.1 K above ambient, with every tile idle (§6).** Eighty populated sockets
   are conduction paths from the 37 °C perfused core to the shell; the wearer pushes **5.6 W** into
   it at 25 °C ambient. **This term appears in no prior document** because R1's perfect sink pins it
   away. It is `OI-N1-04`'s mechanism running in the direction that is always present.
7. **The missing part is a spreader (§7).** Bare-shell lateral conductance is 1.6× the socket's own
   rejection — the exterior is 80 hot spots. A **100 µm pyrolytic graphite film (28 g over the
   dome)** takes it to 9.1× and is worth **1.9× the admissible watts at N = 6**. It is a new BOM
   line against an already margin-negative T1 (CLAUDE.md §2.1).
8. **`N1-D-1` answered, and the answer is small (§8).** Distributed montage, 42 °C face limit, 25 °C
   ambient, library floor: **10 tiles with the spreader, 2 without.** N1's range was 16–78. In the
   unit `NP-PWR-BUDGET-001` D-4 and N1-D-4 both insist on: **31.4 W total PBM electrical, fully
   distributed; 8.9 W at N = 6 with the spreader, 4.7 W without.**
9. **The R-4 point is inadmissible, robustly (§10).** Across a deliberately wide box in h_ext and
   exterior area — 6–14 W/m²K × 0.090–0.150 m² — the ceiling at 6.25 W/tile is **0 in every cell**.
   That conclusion needs none of the soft inputs: the 4.29 K/W via alone spends more than the face
   budget at that drive. The library-floor ceiling is *not* robust and moves 4 → 25 tiles across the
   same box.
10. **The fan is not on this path, and `RISK-26`'s named cause is wrong (§11).** `SPEC-SINK-03`:
    `R_sink` is **unchanged by fan loss**. What raises the outward resistance is **occlusion** — a
    hood, a hat, bedding, a headrest, a pillow under a supine user — which fan RPM does not observe
    and `SR-FAN-05` therefore cannot predict. The Path B1 face NTC does observe it, so the selected
    **control** still covers the hazard; the **cause list and the predictive layer** do not.
    `OI-SINK-04`.

---

## 2. Where the via terminus is

### 2.1 It is 32 mm out, not 188 mm back

`NP-THERM-CFD-R1-001` §5 models "a solid conductive via down the boss centreline to an **external**
fan-cooled heatsink", copper, r 4 mm, with the via end **pinned at ambient** — "the best any
fan-cooled heatsink can do". The phrase *external heatsink* is the only description the tree ever
gives that component, and two documents have since read it as a **part at the hub**:

- `NP-TOOL-HUB-001` §2 places the hub enclosure at the occipital arch partly *because* "the BN-boss
  thermal export path terminates at an external fan-cooled heatsink … the hub enclosure is the
  physical home of this fan-cooled heatsink", and specifies **F-04**, a service door over it.
- `NP-THERM-COOL-001` §6.7 sites both remote-sink accessories on "the via's hub heatsink", and §6.4
  settles `OI-R1-03` with "the fan cools the external heatsink at the via terminus, not the cavity
  air".

Against `NP-HELMET-GEOM-001` §2's radial stack, a conductor from the LED junction to the outside is:

| Segment | mm | Source |
|---|---:|---|
| module seated body (junction → outer face) | ~14 | §2 L1, 12–16 mm |
| inner-bowl socket wall + FPC channel | 2.25 | §2 L1 |
| cluster-clamp + lever features | 3.5 | §2 L1 |
| inter-bowl clamp travel + blind-mate boss | 6 | §2 Gap, 5–7 mm |
| absorber foam + Pd liner + mu-metal + PETG + CFRP | 6.1 | §2 L2 + L3 |
| **total** | **~32** | |

> **Since 2026-09-23 the absorber row is gone** (`REQ-CAV-04`, Layer 4 deleted with a binding 3 mm
> re-loft): the last segment is **3.1 mm** (Pd liner + mu-metal + PETG + CFRP) and the total **~29 mm**.
> The table is kept as computed, because the R1 decomposition below was taken against it.

**The via reaches the outer bowl and stops.** Straight-line socket-to-occiput distance across the
delivered lattice is **20 mm minimum, 188 median, 262 maximum** — and straight-line is a lower bound,
because a real conductor follows the shell. Nothing in the document tree specifies a part spanning
that gap.

### 2.2 A solid collection trunk, sized on its own arithmetic

A conductor carrying `Q` watts over `L` metres drops `ΔT = QL/kA`; as a trunk of mass `m = ρLA` that
is `ΔT = ρQL²/km`. Mass is the design variable. At the median 188 mm:

| copper trunk | ΔT at 5.1 W (N = 6, floor) | ΔT at 24.4 W (N = 6, R-4) | ΔT at 30 W |
|---|---:|---:|---:|
| 50 g | 80 K | 386 K | 475 K |
| 100 g | 40 K | 193 K | 237 K |
| 250 g | 16 K | 77 K | 95 K |
| 500 g | 8 K | 39 K | 47 K |
| 1000 g | 4 K | 19 K | 24 K |

The **whole** face budget at 25 °C ambient is 17.0 K, for every leg of the path together, and
CLAUDE.md §4.4 rates the entire spring-decoupled electrode pod — springs, silicone, electrode — at
80–120 g. **A trunk that fits a head-worn mass budget spends the budget several times over on
transport alone.** Branching does not rescue it: every branch still spans the distance, and the
mass scales with `L²` either way. **REJECTED.**

Heat pipes are the obvious counter-proposal and are not assessed here beyond one structural point,
because it decides the question before the thermodynamics: a copper heat pipe running from the tile
field to the hub is a **conductor crossing the Faraday boundary**. `NP-THERM-COOL-001` §6.4 refuses
an *air* duct across that boundary on exactly this reasoning ("both breach a stated requirement"),
and §6.5 credits liquid cooling's one structural advantage as being that "it crosses the envelope as
a dielectric tube, **no conductor**". A metallic penetration is the harder case, not the easier one.
**`OI-SINK-03`** carries it, so that the rejection is recorded as a decision and not as an omission.

### 2.3 The shell as the collector, with a heatsink at the occiput

The remaining possibility is that the outer bowl itself conducts to a hub-mounted sink. Modelled
directly — a heatsink of resistance `R_hub` bolted to the shell's exterior node nearest the occipital
arch, through a generous 2 W/K contact — at N = 6 on the library floor:

| spreader | `R_hub` = 0 (perfect) | 1.0 K/W | 2.0 K/W | ceiling @1.3 W/tile: none → `R_hub` 1.0 |
|---|---:|---:|---:|---|
| bare shell | 15.5 % | 14.3 % | 13.3 % | 2 → 2 |
| + PGS 100 µm | 40.0 % | 32.7 % | 27.7 % | 10 → 10 |

**A perfect hub heatsink adds no tiles.** The fraction it intercepts is not negligible with a
spreader fitted — but a spreader is precisely what makes the *shell* work, and the hub sink is then
a second rejection surface of the same order as the shell's own 1.08 K/W, competing with it rather
than adding to it. It cannot be what R1's "perfect sink" stood for.

> **What this gives `OI-HUB-C19`, which is the routing `OI-N1-02` asks for.** That item's residual
> reads "confirm hub thermal headroom for ~1.8 W against the `NP-TOOL-HUB-001` F-04 fan/heatsink
> path". **It is decoupled from the tile field and can be closed on hub electronics alone** — the
> ~1.8 W boost loss, the RT1062, the radios. A hub sink sized for a few watts is an ordinary part.
> `NP-TOOL-HUB-001` F-04's *door* is unaffected; its stated *rationale* is not (§11).

---

## 3. The rejection budget — one coefficient, recovered twice

### 3.1 From R1's own outward path

`R_OUT_BASE = 0.41 m²K/W`, of which R1 §2 attributes 0.23 to the stagnant inter-bowl gap, leaving
`R_cav→amb = 0.18`. That 0.18 is not one thing. Decomposed against `NP-HELMET-GEOM-001` §2's L2 + L3
thicknesses and material-class conductivities:

| layer | mm | k (W/mK) | R" (m²K/W) | note |
|---|---:|---:|---:|---|
| carbon-loaded absorber foam | 3.0 | 0.05 | 0.0600 | EMF L4 — **deleted 2026-09-23** (`REQ-CAV-04`); row kept because the decomposition below was computed with it |
| Pd-polyester liner | 0.1 | 0.20 | 0.0005 | EMF L3 |
| mu-metal | 0.2 | 30 | 0.0000 | EMF L2 |
| PETG laminate | 0.3 | 0.20 | 0.0015 | EMF L2 encapsulation |
| CFRP shell (through-thickness) | 2.5 | 0.80 | 0.0031 | EMF L1 + structure |
| **solid subtotal** | | | **0.0651** | |
| **external film (by balance)** | | | **0.1149** | **h = 8.71 W/m²K over the tile footprint** |

Referred to the real exterior area rather than the tile footprint, that is **h = 8.18 W/m²K**.

### 3.2 From the outside of the helmet, with no R1 input at all

The vault dome at the socket map's ellipsoid semi-axes + 12 mm (module-face plane to exterior skin):

| | cm² |
|---|---:|
| gross dome | 1354 |
| less ear cut-outs (2 × ear-cup mount) | 100 |
| less hub enclosure footprint | 54 |
| less Boa occipital dial | 20 |
| **effective rejecting area** | **1180 = 0.118 m²** |

Churchill sphere natural convection at D = 0.29 m, plus linearised radiation at ε 0.90 and view
factor 0.85 (the remainder of the view being the wearer's own shoulders):

| ΔT (K) | h_conv | h_rad | h_total |
|---:|---:|---:|---:|
| 5 | 2.54 | 4.83 | 7.37 |
| 10 | 2.99 | 4.83 | **7.81** |
| 15 | 3.28 | 4.83 | 8.11 |

**8.18 against 7.81 — 4.7 % apart, from two derivations sharing no input.** One is R1's own network
arithmetic; the other is a textbook correlation over a geometry the socket map already carries. That
agreement is the load-bearing result of this document: it is why the specification below is
*recovered* rather than *chosen*, and why the answer to `OI-N1-02` is not "pick a heatsink".

> **Which of the two is primary.** The correlation. The R1 decomposition depends on the absorber
> foam's conductivity, which is a material-class estimate and the single largest lever in §3.1 — at
> k 0.03 the film would be 0.075, at k 0.08 it would be 0.137. The correlation needs no such input.
> The decomposition is the **corroboration**, and the fact that a figure sensitive to foam k lands
> within 5 % of one that is not is a check on both.

---

## 4. THE SPECIFICATION

| Ref | Quantity | Value | Basis |
|---|---|---|---|
| **SPEC-SINK-01** | `h_ext` over the vault exterior | **7.81 W/m²K over 0.118 m²** | §3.2, corroborated by §3.1 |
| | **`R_sink` aggregate** | **1.08 K/W** (band **0.83–1.58**) | 1/(h·A); band from the corners of both soft inputs |
| **SPEC-SINK-02** | `R_sink`, occluded | **1.48 K/W** | hood/bedding over 70 % of the vault: 3 mm fabric at k 0.05 plus suppressed radiation |
| **SPEC-SINK-03** | `R_sink`, **fan lost** | **1.08 K/W — unchanged** | the hub fan is not on this path (§2.3) |
| **SPEC-SINK-04** | exterior lateral conductance | **≥ 0.18 W/K** (`Σk·t`), i.e. bare shell + a ≥ 100 µm pyrolytic-graphite spreader | §7; the condition under which SPEC-SINK-01 is realised at operating N |

**SPEC-SINK-01 is not a part number and cannot be improved by buying a better one.** It is the
outside of the helmet. The only levers on it are exterior **area** (fixed by head geometry), surface
**emissivity** (already assumed high), and **airflow over the vault** — which no hub-mounted fan
provides and which the product's wearability forbids designing around.

**SPEC-SINK-04 is the part that must actually be procured**, and it is a spreader, not a sink.

---

## 5. The refined network — R1's, re-partitioned

One node changes. `NP-THERM-CFD-N1-001`'s network routes the cavity path to ambient and the via path
to a separate sink node, each rejecting independently. Physically both reach the **same exterior
skin**, and the film on that skin is the last term of `R_OUT_BASE`. So per socket, adding an exterior
node `X`:

| Leg | R" (m²K/W) | Was |
|---|---:|---|
| `J → C` stagnant inter-bowl gap | 0.230 | unchanged |
| `C → X` absorber + liners + CFRP | 0.065 | inside `R_cav→amb` |
| `X → ambient` external film | 0.115 | inside `R_cav→amb` |
| `J → X` the BN-boss via | 0.0059 | was `J → K`, with `K` unspecified |
| `X(i) ↔ X(j)` shell + optional spreader | — | was applied to the cavity air, which cannot conduct |

`0.230 + 0.065 + 0.115 = 0.410 = R_OUT_BASE`, **exactly**. No resistance is invented. The via simply
lands where a 32 mm radial conductor lands, and stops being credited with a rejection surface the
cavity path is already paying for.

**Validation.** Reproducing R1's published cell in this network requires **two** settings — pin the
skin at ambient **and** charge `C → X` the full 0.18 with the film still in it. With both, the
80-socket model reproduces R1's single adiabatic cell to **1.8 × 10⁻¹⁴ K**. That it takes both *is*
the finding: **R1 spends the external film on the cavity path and simultaneously sets it to zero for
the via path.**

> **This is a third accounting defect on the same margin, and they compound.** `OI-N1-01` found R1's
> §5.1 flux labels carry η_wp twice, worth 11.3 K → 9.2 K at nominal ambient. `NP-THERM-CFD-N1-001`
> §3 found the remaining margin was never a function of tile count. This document finds the
> rejection resistance that margin is measured against was set to zero. **None of the three is a
> modelling refinement that makes the design better.**

---

## 6. The idle baseline — the exterior is not at ambient

Every populated socket is a conduction path between the 37 °C perfused core and the exterior skin,
and it conducts whether or not the tile is driven. With **no tile driven at all**:

| ambient | T_skin | T_face | heat the wearer pushes into the shell |
|---:|---:|---:|---:|
| 20 °C | 28.7 | 29.4 | 8.0 W |
| 25 °C | 31.1 | 31.7 | 5.6 W |
| 30 °C | 33.6 | 33.9 | 3.3 W |
| 35 °C | 36.0 | 36.1 | 0.9 W |

The spreader does not move this row — with every tile idle the exterior is uniformly loaded, so
there is nothing to spread.

**~6.1 K of the 17.0 K face budget at 25 °C is spent before the first LED lights**, by the head the
helmet is on. The figures are unremarkable as *physiology* — a scalp under an insulated helmet runs
warm, and 31.7 °C at the module face is nowhere near a hazard — but they are consequential as
*budget*, and they appear in no prior document because R1's perfect sink pins them away by fiat.

This is `OI-N1-04`'s mechanism ("idle populated tiles are conductive paths from the shared sink into
the scalp when ambient exceeds body temperature") running in the direction that is **always**
present, not only above 37 °C ambient. `OI-N1-04` should be restated to cover both signs.

---

## 7. The spreader trade — what actually buys tiles

A lumped `R_sink` silently assumes the terminus is **isothermal**. The bare outer bowl is not:
in-plane `Σk·t` is 0.031 W/K (CFRP 2.5 mm at k ≈ 10 plus mu-metal), against a per-socket rejection
conductance of 0.012 W/K — a ratio of 1.6, and a spreading length of ~63 mm, under two tile pitches.

| id | spreader | `Σk·t` (W/K) | lat/rej | added mass | ceiling @1.3 W | @6.25 W |
|---|---|---:|---:|---:|---:|---:|
| S0 | none — bare outer bowl | 0.031 | 1.6× | 0 g | **2** | 0 |
| S1 | PGS graphite film 25 µm | 0.079 | 3.9× | 7 g | 4 | 0 |
| S2 | PGS graphite film 70 µm | 0.136 | 6.8× | 20 g | 8 | 0 |
| S3 | **PGS graphite film 100 µm** | 0.181 | 9.1× | **28 g** | **10** | 0 |
| S4 | aluminium foil 300 µm | 0.092 | 4.6× | 110 g | 4 | 0 |

Graphite beats aluminium on both axes at once — 2× the lateral conductance at a quarter of the mass
— which is why S4 is listed and not recommended.

**What a spreader is not.** It adds no rejection area and does not change `R_sink`; it makes the
area that already exists reachable from one tile. Its whole value is at **low N** (§8), which is
where sessions run.

**What it costs, which this document does not settle.** A pyrolytic graphite film on the outer bowl
needs a dielectric overwrap and a lamination process, and it is a new BOM line on a configuration set
that is **gross-margin negative at every T1 price in force** (CLAUDE.md §2.1, `NP-COST-001`). It also
sits on the exterior of the EMF shell. Nothing here suggests it perturbs the shielding claim — it is
outboard of an already-conductive CFRP layer, adds no aperture, and is non-magnetic — but that is an
assertion this document is not entitled to make. **`OI-SINK-01`, BLOCKING**, carries both.
*(Rev 2: §7.1 now works that assertion at first order, mechanism by mechanism. It does not clear
`OI-SINK-01`.)*

### 7.1 The spreader against the EMF stack — `RISK-SINK-03` re-scoped (Rev 2, GitHub #399)

**What the film cannot do.** `NP-EMC-CAV-001` §9 item 4: the film sits on the **exterior** of the
outer bowl, outboard of every shield layer, so it cannot change the internal cavity's loaded Q
(`REQ-CAV-02`), and cavity behaviour is dropped from `RISK-SINK-03`. **Nor does `REQ-EMI-10` reach it
by its own text**: that requirement governs conductive additions to the **inner carrier** (L1 in
`NP-DRV-SHELL-002`'s notation, where the fluxgates mount), not the outer bowl's exterior. Two EMF
mechanisms remain, and this section gives each a clearance route. **Nothing below is a measurement.**

**Inputs — which ones the record holds, and which it does not.**

| Input | Value used | Source / why this value |
|---|---|---|
| Outer radius of the shell `a` | **≤ 0.15 m** | Bounds both record figures: 99 mm maximum head radius (`NP-EMC-CAV-001` §2, §4.1) + 32 mm radial stack (CLAUDE.md §4.3) = 131 mm; §3's 1,354 cm² gross exterior taken as a hemisphere = 147 mm. `τ` grows with `a`, so the larger is the conservative one |
| Film sheet conductance `G = σ·t` | **≤ 250 S** at 100 µm | **Not in the record** — `SPEC-SINK-04` specifies `Σk·t`, not σ. Bounded instead by the in-plane resistivity of highly oriented pyrolytic graphite, ≈ 0.4 µΩ·m (σ ≈ 2.5 × 10⁶ S/m) — the ordered limit, which no commercial film exceeds. Replace with the selected film's datasheet under `OI-SINK-01` |
| Film magnetism | diamagnetic, χ of order −10⁻⁴ (SI volume) | Graphite; against L2's µr ≫ 1 in the same stack it is nil, and **no credit or debit is taken for it** below |
| Coupling `κ` of the film's reaction field to the coils' centre field | **≤ 1.40**; **1.00** for a Helmholtz pair inscribed in the shell | Derived below |
| Band | **≤ 300 Hz** | The WHO definition of ELF (EHC 238), the band `NP-BIB-EMF-001` §4 argues in and CLAUDE.md §4.3's ELF figure is stated for. **The active loop's own bandwidth is not stated anywhere in the record** — see the condition in (1) |
| Allowed transfer-function error | **the active loop's share: 10 dB · ~20 dB mid · 30 dB top**, i.e. ε ≤ 0.32 · 0.1 · 0.032 | Taken as `NP-THERM-BOWL-001` §5.2 derives it, so the two documents judge the same transfer function against one budget: CLAUDE.md §4.3's combined ELF 35–45 dB less L2's 15–25 dB leaves **10 (35 − 25) to 30 (45 − 15) dB**. Read open-loop, because `REQ-EMI-05`'s subtraction is feed-forward: an actuator whose field is off by a vector fraction ε leaves a residual of ε, so the path's ceiling is −20 log₁₀ ε. A fluxgate-closed loop divides the error by its loop gain; this is the strict reading. A derivation from a stated target, not a requirement (CLAUDE.md §18). *(Rev 2 as first drafted read the budget as 20 dB at both ends of the range — one pairing of the two ranges, not the span. Corrected before merge, against §5.2)* |

#### (1) ELF eddy-current loading of the Helmholtz actuator — **cleared at first order at the mid share; at the top share, up to ~230 Hz**

**The mechanism.** The calibrated quantity in the active loop is the **coil-drive → field transfer
function** (`hardware-detail.md` §4.3 **D1**; `REQ-EMI-11`). A conducting sheet around the coils
carries eddy currents that react on the coils' field, and so changes that transfer function.

**First-order model.** Treat the film as a thin spherical shell of radius `a` and sheet conductance
`G`. Its dipole (l = 1) mode — the only one whose field is non-zero at the centre, which is where the
brain volume is — has time constant

```
τ₁ = μ₀·G·a / 3 = (4π×10⁻⁷)(250)(0.15)/3 = 1.57 × 10⁻⁵ s
```

and at frequency `f` its reaction field is the perfect-conductor image scaled by
`jωτ₁ / (1 + jωτ₁)`. For a perfect conductor, a coil pair of radius `ρ` and total dipole moment `m = 2NIπρ²`
inside a shell of radius `a` sees a uniform image field `−μ₀m / (2πa³)`, which is what makes
`B_r = 0` at `r = a`.
Against a Helmholtz pair's centre field `0.7155·μ₀NI/ρ` that is `κ = (ρ/a)³ / 0.7155`, **≤ 1.40 for
any `ρ ≤ a`**, and for a pair inscribed in the shell at Helmholtz spacing (`ρ = a/√1.25`) exactly
**1.00**. So the fractional error the film puts on the transfer function is

```
ε(f) ≈ κ · ωτ₁ / √(1 + (ωτ₁)²)
```

| f | ε (film alone) | feed-forward ceiling | against 20 dB mid | against 30 dB top | with `NP-THERM-BOWL-001`'s in-session thermal drift (3.9 %, in phase) |
|---:|---:|---:|---:|---:|---:|
| 50 Hz | 0.0069 | 43.2 dB | +23.2 | +13.2 | 28.0 dB (thermal alone 28.2) |
| 60 Hz | 0.0083 | 41.6 dB | +21.6 | +11.6 | 28.0 dB |
| 100 Hz | 0.014 | 37.2 dB | +17.2 | +7.2 | 27.7 dB |
| **300 Hz** | **0.041** | **27.7 dB** | **+7.7** | **−2.3** | **24.9 dB** |

**The film's error and the thermal one are in quadrature, so they add root-sum-square, not linearly.**
The film's term is `jωτ₁` — a phase shift — while `NP-THERM-BOWL-001` §5.3's coil-resistance drift
under voltage drive is a gain error in phase. At the mains frequencies where ambient ELF is concentrated, the film
costs **0.2 dB** against a thermal term the record already carries; it matters only toward the top of
the band.

**What this takes no credit for, and each is a real one.** (a) **Calibration.** The film is laminated
to the outer bowl before any calibration can run, so it is *in* the transfer function `REQ-EMI-11`
calibrates, and a linear time-invariant change to a calibrated plant is calibrated out. The table
assumes a calibration that ignores the film entirely. (b) **L2.** The record does not state the
coils' radial position relative to the mu-metal. If they sit inboard of it — as a coil meant to drive
the brain volume through a 15–25 dB magnetic shield would have to — the flux the exterior film sees
is what leaks outward through L2, a further 15–25 dB on §4.3's own figure. No credit is taken either
way. (c) **The conductors already present.** CFRP and
mu-metal are both conductors in the characterised plant (D1); the film adds a sheet to a stack that
already carries eddy currents, it does not introduce the mechanism.

**The verdict, and the conditions it rests on — stated as conditions, not as requirements on the part.**
With **no** credit taken, the film clears the **mid share (20 dB) across the whole ELF band**, and
the **top share (30 dB) up to ~230 Hz** — which covers 50 Hz through its 4th harmonic and 60 Hz through
its 3rd. In the form `κ·ω·μ₀·G·a/3 ≤ ε_share`, at `a = 0.15 m`:

| Share | at 300 Hz | at 60 Hz | band edge at `G = 250 S` |
|---|---|---|---|
| 20 dB mid | `G ≤ 603 S` (~240 µm at the graphite ceiling) | `G ≤ 3,016 S` | ~720 Hz |
| 30 dB top | `G ≤ 191 S` (~76 µm — **below** `SPEC-SINK-04`'s 100 µm) | `G ≤ 954 S` | **~230 Hz** |

**The top share above ~230 Hz rests on credit (a)**, and credit (a) holds only if `REQ-EMI-11`'s
calibration resolves the transfer function's **phase** across the band, not a single scalar gain. The
record says *transfer function* and specifies no method, so this is a condition for the calibration's
eventual specification, handed to it rather than imposed here. **Nothing in the record states the
active loop's bandwidth either**; the table above is the answer for whatever it turns out to be.
Outside these conditions the clearance reverts to `EMF-1-SINK-1`.

**The residual that calibration cannot remove** is a *change* to the film after calibration —
delamination, cracking, a gouge. Its effect is bounded by the whole-film figure above (a film that is
entirely gone moves ε by at most 0.041 at 300 Hz), and `REQ-EMI-11` recalibration fires on
`np_module_map` rebuild, not on damage — the same gap `OI-BIBEMF-06` records for mu-metal remanence
and `NP-THERM-BOWL-001` `OI-THBOWL-02` records for temperature. It is inside the mid share without any
trigger, and inside the top share at mains frequencies.

**Ambient direction, for completeness.** The same shell attenuates *external* ELF reaching the
fluxgates by `1/(1 + jωτ₁)` — at 60 Hz, by under 0.002 % in magnitude. It helps, negligibly, and changes no argument.

**Bench confirmation — `EMF-1-SINK-1`** (listed in the `EMF-1` test plan, `NP-HEX-ZM-001` §5.6.1,
and owned here). On the fluxgate/Helmholtz bench, not the VNA set-up: the coil-drive → fluxgate
transfer function, magnitude **and phase**, three axes, 10–300 Hz and to the loop's bandwidth if one
is ever stated, **film fitted vs. not**, at one coil temperature so `NP-THERM-BOWL-001`'s drift does
not enter. The prediction to refute is `|ΔH/H| ≤ 0.041` at 300 Hz, predominantly in quadrature, with
no recalibration between the two states. It confirms the analysis at the mid share; at the top share
above ~230 Hz it is what the clearance waits on, unless the calibration condition above is met.

#### (2) External RF — **narrowed, not cleared**

**Over the continuous area it can only add.** The film is laminated to the CFRP through a dielectric
bond whose thickness is micrometres to tens of micrometres, against λ/4 = 12.5 mm at 6 GHz, the upper
edge `NP-HEX-ZM-001` §5.3(a) bounds the external concern at. Electrically, film and CFRP are two
shunt sheets in contact: their admittances add, and a plane wave's transmission through a shunt sheet
`T = 2/(2 + Z₀Y)` falls as `Y` rises. **An unbroken exterior conductor cannot lower the stack's
through-sheet attenuation**, and it is outside the cavity, so it cannot reach §9 item 4's question
either.

**At its edges it can do something, and the record cannot say what.** `NP-BIB-EMF-001` §7.6: the
stack is **aperture-limited, not layer-limited** — and the film ends at exactly the apertures that set
the floor: the parting-plane labyrinth lip (`NP-HEX-ZM-001` §5.3(a), `RISK-20`), the `BOSS-1` emboss
(§5.3(c)), and the TMS window cut-out that §7.1(3) below now requires. As specified, the film is a
**floating** conductor under a dielectric overwrap: nothing bonds it to shield ground, and a floating
sheet whose largest dimension is of order 0.3 m resonates from roughly c/2L ≈ 0.5 GHz upward — inside
`NP-EMC-CAV-001`'s 420 MHz – 3 GHz band and well below 6 GHz. A resonant floating conductor
terminating at a slot can drive that slot. Whether it does depends on the edge geometry and bonding,
**which `SPEC-SINK-04` does not set** — so there is nothing to compute against.

**Bench step — `EMF-1-SINK-2`** (listed in the `EMF-1` test plan, `NP-HEX-ZM-001` §5.6.1, and owned
here). On the `EMF-1` fixture, over the same range as `EMF-1a` swept to
6 GHz (`NP-EMC-CAV-001` §7, `OI-EMCCAV-04`/`-06`): external illumination, E-field at the electrode
plane, **bare shell vs. film fitted with its edges terminated as built**; plus a radiated-emissions
pre-scan against FCC Part 15B / CISPR 11 in both states, because a floating conductor re-radiates
internal leakage as readily as it admits external RF. **Pass: the film-fitted shell is no worse than
the bare shell at any frequency, within the fixture's uncertainty.** The claim being cleared is
*no degradation*, so the bare shell is the reference — not the 40–60 dB target, which `EMF-1` itself
has yet to measure. **Two edge treatments are available to `OI-SINK-01`'s owner and neither is chosen
here:** bond the film to shield ground at the rim bond points (`NP-HEX-ZM-001` §5.3(b)), or stop it
back from each aperture. Either changes `EMF-1-SINK-2`'s build, not its criterion. It inherits the two limits
§5.6.1 states for the fixture's RF sweeps: 6 GHz does not reach Wi-Fi 6E (5.925–7.125 GHz), and
the external-illumination band is not yet specified (`OI-EMCCAV-06`).

#### (3) Found while scoping (2): the film must not cover the TMS window — `RISK-SINK-07`

CLAUDE.md §4.3 requires a **non-conductive CFRP window at the TMS coil site** to prevent eddy-current
field loss, and `NP-HW-TMS-001` `REQ-TMS-08` records that the window *"forecloses the obvious metallic
heat spreader."* `SPEC-SINK-04` describes a film laminated *"over the dome"* with no exclusion. A
graphite film is a conductor, and laminating it across the window puts back, on the exterior, the
conductor the window exists to remove. **This is not a new constraint** — it is the locked §4.3
control applied to a part that post-dates it — so the film carries a **cut-out at the TMS window on
every bowl laid up with one** (`NP-REG-UPG-001` `OI-UPG-05` decides whether T1 bowls are). It is not
Helmholtz loading and not external RF, so it is not in `RISK-SINK-03`; it is `RISK-SINK-07`. The cut-out
also removes spreader from under the TMS site, a local loss of `Σk·t` that `OI-SINK-01`'s thermal
side now has to carry. **`OI-SINK-09`.**

> **Net effect on `OI-SINK-01`: narrowed, not cleared — it stays BLOCKING.** Mechanism (1) is cleared
> at first order at the 20 dB mid share across the band and at the 30 dB top share up to ~230 Hz; above
> that the top share rests on the calibration resolving phase, or on `EMF-1-SINK-1`. Mechanism (2)
> is cleared over the continuous area and **open at the terminations**, which only `EMF-1-SINK-2` can
> close. And `OI-SINK-01` was never only an EMF item: the BOM line against a margin-negative T1, the
> lamination process and the ingress half (`RISK-SINK-06`) are untouched by this section.

---

## 8. The ceiling under the specification — `N1-D-1` answered

Distributed montage, max `T_face` ≤ 42.0 °C.

**With S3 (PGS 100 µm):**

| drive (W/tile electrical) | amb 25 | amb 30 | amb 35 | occluded, amb 25 |
|---|---:|---:|---:|---:|
| library floor 1.3 | **10** | 4 | 2 | 6 |
| R-4 point 6.25 | 0 | 0 | 0 | 0 |
| library ceiling 20.0 | 0 | 0 | 0 | 0 |

**Bare shell (S0) — the design as currently adopted:**

| drive (W/tile electrical) | amb 25 | amb 30 | amb 35 | occluded, amb 25 |
|---|---:|---:|---:|---:|
| library floor 1.3 | **2** | 0 | 0 | 0 |
| R-4 point 6.25 | 0 | 0 | 0 | 0 |
| library ceiling 20.0 | 0 | 0 | 0 | 0 |

### 8.1 The same result in watts, which is the right unit

`NP-PWR-BUDGET-001` D-4 and `NP-THERM-CFD-N1-001` N1-D-4 both conclude the governor must be watts,
from the power and thermal sides independently. Total admissible PBM **electrical** draw:

| | N = 1 | N = 6 | N = 12 | N = 20 | N = 80 | best N |
|---|---:|---:|---:|---:|---:|---:|
| bare shell S0, 25 °C | 1.3 | 4.7 | 8.7 | 12.3 | 31.4 | 31.4 W |
| bare shell S0, 35 °C | 0.8 | 2.7 | 4.9 | 7.0 | 17.9 | 17.9 W |
| PGS 100 µm S3, 25 °C | 2.5 | **8.9** | 14.4 | 17.6 | 31.4 | 31.4 W |
| PGS 100 µm S3, 35 °C | 1.4 | 5.1 | 8.2 | 10.1 | 17.9 | 17.9 W |

Two things this unit shows that a tile count hides.

- **The budget rises with N.** Spreading the same watts over more tiles is thermally free and then
  some — the exact opposite of a concurrency *ceiling*. A tile-count governor gets the sign of the
  relationship wrong.
- **The spreader's entire value is at low N.** At N = 80 the exterior is uniformly loaded and there
  is nothing to spread, so both rows converge on the aggregate 31.4 W. Authored montages run
  **N = 5–37** (`NP-THERM-CFD-N1-001` §6), where S3 is worth **1.9× at N = 6**.

For scale, CLAUDE.md §4.5 puts Standard T1 at ~17–20 W **total device** draw. The fully-distributed
31.4 W is not an alarming number; the **8.9 W at a realistic montage** is.

### 8.2 What this does to `N1-D-1`

> **`N1-D-1` — "no tile count may be quoted as a thermal ceiling until `R_sink` is specified" — can
> be lifted.** It has been specified. **SINK-D-1** replaces it: a tile count may be quoted only
> alongside the per-tile drive and the spreader state, because the count is not the governed
> quantity — §8.1's watts are.
>
> **The replacement is worse news than the prohibition.** N1 could not quote a number; the numbers it
> could not quote ranged 16–78 tiles. The specified answer is **10**, and **2** on the design as
> currently adopted.

---

## 9. Against `NP-THERM-CFD-N1-001`'s own model

Feeding N1's single-sink topology the specified 1.08 K/W, against this network at the same drive
(library floor, 25 °C, distributed):

| N | N1 @ `R_sink` 1.08 | this, isothermal exterior | this, PGS 100 µm | this, bare |
|---:|---:|---:|---:|---:|
| 1 | 34.3 | 35.2 | 37.1 | 41.7 |
| 2 | 34.7 | 35.6 | 37.4 | 41.9 |
| 6 | 36.1 | 37.2 | 40.7 | 49.0 |
| 12 | 38.1 | 39.5 | 42.8 | 50.3 |
| 20 | 40.8 | 42.6 | 46.9 | 53.4 |

**Columns 2 and 3 differ by about a kelvin**, and that kelvin is the cavity leg's double-counted
film. So N1's model, given the specified `R_sink`, is right — *conditional on an isothermal exterior*.
Its sweep bracketed the truth and its 1.00 K/W row is the real one.

**Columns 3 and 5 do not differ by a kelvin**, and that is the finding. The gap between them is the
spreader, and it is worth **4.6 K at N = 1 and 8.3 K at N = 6**.

> **Hand-back to `scripts/check-thermal-multitile.ts`.** `R_SINK_DEFAULT` should read **1.08**, no
> longer marked unspecified — **valid only where a spreader is specified.** Without one, N1's
> topology has no valid lumped value at any N, and `scripts/check-thermal-sink.ts` is the model to
> use. That change is *not* made in this revision: N1's published figures are its own, and rewriting
> its default would silently move every number in a released document. **`OI-SINK-02`** routes it to
> that document's owner.

---

## 10. Sensitivity — what is robust and what is not

`h_ext` and the effective exterior area are the two softest inputs, and they enter only as the
product. Ceiling at the library floor, 25 °C, S3:

| `h_ext` \ `A_ext` | 0.090 m² | 0.118 m² | 0.150 m² |
|---|---:|---:|---:|
| 6.00 W/m²K | 4 | 6 | 10 |
| **7.81 W/m²K** | 6 | **10** | 14 |
| 10.00 W/m²K | 10 | 14 | 18 |
| 14.00 W/m²K | 14 | 18 | 25 |

The same box at the R-4 point, 6.25 W/tile:

| `h_ext` \ `A_ext` | 0.090 m² | 0.118 m² | 0.150 m² |
|---|---:|---:|---:|
| 6.00 → 14.00 W/m²K | **0** | **0** | **0** |

- **ROBUST — the R-4 point is inadmissible across the whole box.** It needs none of the soft inputs:
  the 4.29 K/W via alone spends more than the face budget at that drive. This is the same wall
  `NP-THERM-CFD-N1-001` §6b reaches from the other side (6.63 W/tile at 25 °C with a *perfect* sink),
  and it is the reason §8's R-4 column is zero everywhere.
- **SOFT — the library-floor ceiling moves 4 → 25 tiles across the box.** It is a design target to be
  confirmed on the THERM-1b bench (`OI-R1-02`), not a claim to build a specification sheet on.
  **The only honest use of "10 tiles" is as the current best estimate under stated assumptions.**

---

## 11. The fault case, and what `RISK-26` names as its cause

Occlusion sweep at the library floor, N = 6, 25 °C, S3. **φ** is the fraction of the vault exterior
covered by 3 mm of fabric:

| φ | T_skin | max T_face | |
|---:|---:|---:|---|
| 0.00 | 37.6 | 40.7 | |
| 0.25 | 38.0 | 41.1 | |
| 0.50 | 38.5 | 41.6 | |
| 0.70 | 39.0 | 42.0 | at the limit |
| 0.90 | 39.5 | 42.4 | **over 42** |
| 1.00 | 39.7 | 42.6 | **over 42** |

`FMEA-G07-01` / `RISK-26` reads *"Fan/vent fouling or fan failure → outward thermal resistance rises
→ heat diverted scalp-ward → scalp face > 42 °C while junction NTC ≤ 62 °C"*. **The mechanism is
real and the hazard is real. The named cause is not.**

- The fan is not in the tile export path (§2.3), so `SPEC-SINK-03` holds: fan loss does not change
  `R_sink`. R1 §5.2 already found the via gives **zero** benefit in its own fault case; the reason is
  that the fault it modelled — "external sink / fan airflow LOST" — is not a fault this architecture
  can have.
- What **does** raise the outward resistance is a covered vault: a hood, a hat, a towel, bedding, a
  headrest, a pillow under a supine user. Sessions run 6–30 minutes and Mode 3 is explicitly designed
  for use away from a desk, so this is a **far more probable initiator than fan failure**.
- **Fan RPM does not observe it.** CLAUDE.md §5.1 logs fan RPM to SHDR and `SR-FAN-05` raises the
  predictive-maintenance alert on it; `NP-TOOL-HUB-001` F-04 exists to let the user act on that
  alert. None of that chain sees a blanket.
- **The selected control still covers the hazard.** `NP-REQ-FANHEALTH-001` Path B1 — the scalp-facing
  NTC co-located with PD2 — reads the face directly and does not care what raised the resistance.
  Path B1 was the right selection; §11's finding is that it was selected for a reason narrower than
  the one that justifies it.

**`OI-SINK-04`** routes the cause-list correction to `NP-FMEA-GEOM-001` and `NP-REQ-FANHEALTH-001`.
It is a **hazard-analysis completeness** question, not a control-selection one, and it is not
corrected here because this document does not own either file.

---

## 12. What this does not close

**It is not a bench measurement, and `OI-R1-02` is what would falsify it.** `h_ext` is a correlation
over an idealised ellipsoid in still air; a real head has hair, a collar, a chair back and a room
with air movement in it. The correlation is as likely to be conservative as optimistic, and §10 shows
the library-floor ceiling is the figure that moves.

**It is not a mesh CFD, and `OI-R1-01` remains open.** No field is resolved inside a tile; contact
conductances are R1's; the exterior node network is a lumped 80-node representation of a continuous
skin, which is adequate where the spreading length spans tile pitches and least adequate exactly
where it does not — the bare-shell case.

**The exterior area is an estimate.** 1354 cm² gross is computed from the socket map's **provisional**
ellipsoid (`OI-N1-07`: replace from shell CAD); the 174 cm² of deductions is a design-stage judgement.
§10's box is wide enough to cover being wrong about it.

**It does not price the spreader, or clear it against the EMF stack.** `OI-SINK-01`. *(Rev 2: §7.1
clears the Helmholtz-loading mechanism at first order and narrows external RF to the film's
terminations; `OI-SINK-01` stays BLOCKING.)*

**It does not re-run the protocol library.** `NP-THERM-CFD-N1-001` §6a's per-protocol table was
computed at `R_sink` = 0.5 with the film double-counted; under this specification it will get worse,
and its four-inadmissible / fourteen-recoverable split will not survive as stated. **`OI-SINK-05`.**

---

## 13. Decisions

| Ref | Decision | Basis | Reversible |
|---|---|---|---|
| **SINK-D-1** *(principal)* | **`N1-D-1` is lifted and replaced.** A tile count may be quoted only alongside the per-tile drive and the spreader state; the governed quantity is watts (§8.1) | §4, §8 | Yes |
| **SINK-D-2** *(principal)* | **There is no hub heatsink for the tile field, and none is to be specified.** The via terminates on the outer bowl; the rejection surface is the vault exterior at **1.08 K/W** | §2, §3 | Yes — on a shell-CAD or bench result |
| **SINK-D-3** | **`OI-HUB-C19`'s hub thermal budget is decoupled from the tile field** and may be closed on hub electronics alone | §2.3 | Yes |
| **SINK-D-4** *(principal)* | **An exterior spreader is required, not optional** (`SPEC-SINK-04`, ≥ 0.18 W/K `Σk·t`). Without one the design does not hold two tiles at the library floor at 25 °C | §7, §8 | No — the bare-shell column is not an operating design |
| **SINK-D-5** | **`SPEC-SINK-03`: fan loss does not degrade `R_sink`.** The fan-loss fault case for the tile export path is **occlusion**, and it is the case to specify against | §2.3, §11 | Yes |
| **SINK-D-6** | **R1 §5.1's "perfect sink" is to be read as an upper bound that was never attainable**, not as an idealisation close to the real part. Every figure resting on it inherits §5's correction | §5 | No — the leg sum is exact |

---

## 14. Risk rows

| Ref | Hazard | Current control | Verification |
|---|---|---|---|
| RISK-SINK-01 | The shipped design has no exterior spreader; a montage authorised on N1's 16–78 range exceeds the face limit at N ≥ 3 | SINK-D-1/D-4; `scripts/check-thermal-sink.ts` §6 is the standing check | Spreader specified + THERM-1b (`OI-R1-02`) |
| RISK-SINK-02 | `RISK-26`'s cause list omits vault occlusion, so the predictive layer (`SR-FAN-05`) cannot alert on the most probable initiator | Path B1 face NTC catches the effect regardless — **the control holds, the analysis does not** | **No verification defined** — `OI-SINK-04` |
| RISK-SINK-03 | **Re-scoped Rev 2 (GitHub #399).** The exterior spreader film perturbs the EMF stack by exactly two mechanisms: **(1) ELF eddy-current loading of the Helmholtz actuator** — the film's eddy currents change `REQ-EMI-11`'s coil-drive → field transfer function, and `REQ-EMI-05`'s feed-forward subtraction runs on it; **(2) external RF** — the film, floating under its overwrap, couples to the apertures it terminates at. *Cavity behaviour is dropped: an exterior conductor cannot change the internal cavity Q (`NP-EMC-CAV-001` §9 item 4). The ingress-seal half of the Rev 1 row is carried unchanged by `RISK-SINK-06`* | (1) §7.1(1) first-order bound: ε ≤ 0.041 at 300 Hz, 0.0083 at 60 Hz, against `NP-THERM-BOWL-001` §5.2's active-loop share (10 / ~20 / 30 dB): clears the 20 dB mid share across the ELF band and the 30 dB top share up to ~230 Hz with no calibration or L2 credit; the top share above ~230 Hz rests on `REQ-EMI-11`'s calibration resolving phase. (2) §7.1(2): cleared over the continuous area only; **no control at the terminations** | (1) **Cleared by analysis** at the mid share, and at the top share to ~230 Hz; `EMF-1-SINK-1` confirms, and closes the top share above ~230 Hz if the calibration condition is not met. (2) **`EMF-1-SINK-2`** — the terminations are open until it runs. Both on the `EMF-1` fixture; `OI-SINK-01` |
| RISK-SINK-04 | `h_ext` is a correlation, never measured; the library-floor ceiling moves 4 → 25 tiles across a plausible box | §10 states the box; §8's figures are labelled estimates | THERM-1b scalp-phantom bench, `OI-R1-02` |
| RISK-SINK-05 | `NP-THERM-CFD-N1-001` §6a's per-protocol admissibility table remains published at `R_sink` 0.5 with the film double-counted | None — that table is not corrected here | `OI-SINK-05` |
| RISK-SINK-06 | *(Rev 2 — split out of Rev 1's `RISK-SINK-03`, wording unchanged in substance.)* The spreader film's lamination and dielectric overwrap perturb the shell's ingress seal | None — this document asserts no clearance | Shell tooling review (`NP-REV-SHELL-001`); `OI-SINK-01` |
| RISK-SINK-07 | *(Rev 2.)* The spreader film, laminated "over the dome" with no exclusion, covers the TMS coil site and puts back the conductor CLAUDE.md §4.3's non-conductive CFRP window exists to remove — eddy-current field loss at the coil | The locked §4.3 window requirement and `REQ-TMS-08`; **not yet carried into `SPEC-SINK-04`** | Film cut-out at the window on every bowl laid up with one; `OI-SINK-09` |

---

## 15. Open items

| Ref | Item | Owner |
|---|---|---|
| **OI-SINK-01** | **BLOCKING — specify, cost and clear the exterior spreader (`SPEC-SINK-04`).** A ≥ 100 µm pyrolytic graphite film with a dielectric overwrap, ~28 g over the dome, laminated to the outer bowl exterior. Needs a BOM line against a margin-negative T1 (`NP-COST-001`, `OI-COST-10`), a lamination process in `NP-TOOL-SHELL-001`, and an EMF/ingress clearance this document is not entitled to assert. **Without it §8's bare-shell column is the design.** **Rev 2 — EMF clearance NARROWED, not cleared (§7.1, GitHub #399):** Helmholtz eddy loading is cleared at first order at the 20 dB mid share across the ELF band (film `G ≤ 603 S`) and at the 30 dB top share up to ~230 Hz; above that the top share needs `REQ-EMI-11`'s calibration to resolve phase, or `EMF-1-SINK-1` (the selected film's σ replaces the graphite-ceiling bound); external RF remains open **at the film's terminations only**, on `EMF-1-SINK-2`; the edge treatment (bonded vs. stopped back) is this item's to choose; and the film now needs a TMS-window cut-out (`OI-SINK-09`). Ingress (`RISK-SINK-06`), cost and process are unchanged | ME + Thermal + EMC |
| **OI-SINK-02** | ~~Update `scripts/check-thermal-multitile.ts` `R_SINK_DEFAULT` 0.5 → **1.08** and mark it specified, with the isothermal-exterior caveat. Not done here: it would silently move every published figure in `NP-THERM-CFD-N1-001`~~ **CLOSED 2026-09-15 by `NP-PWR-THERM-001` §4a — by *pairing*, not by overwrite.** `R_SINK_SPECIFIED = 1.08` is added alongside `R_SINK_DEFAULT = 0.5`, and `rSinkNote()` prints at every report line that names either that 0.5 is N1's published value and is superseded by `SPEC-SINK-01`. The defect this item was raised against is a **superseded number being readable without its label**, not a constant having the wrong value; a rename would have fixed the constant and left N1's tables unreproducible | Thermal (owner of N1). **CLOSED** |
| **OI-SINK-03** | **Record the heat-pipe rejection as a decision.** §2.2 refuses a metallic collection path on the Faraday-boundary argument `NP-THERM-COOL-001` §6.4/§6.5 already applies to air and to liquid. It has never been written down for a conductor, and it is the obvious counter-proposal to SINK-D-2 | Thermal + EMC |
| **OI-SINK-04** | **`FMEA-G07-01` / `RISK-26` name fan failure as the cause of a hazard whose probable initiator is vault occlusion**, which no fan-RPM signal observes. Path B1 covers the effect; the cause list and `SR-FAN-05`'s predictive claim do not. Also re-examine `NP-TOOL-HUB-001` F-04's stated rationale, which rests on the fouling story | Safety + FW |
| **OI-SINK-05** | ~~**Re-run `NP-THERM-CFD-N1-001` §6a's per-protocol table** under `SPEC-SINK-01` + `SPEC-SINK-04`~~ **CLOSED 2026-09-15 by `NP-PWR-THERM-001` §3c.** The split does not survive: **22 of 22** zone-fixed protocols are thermally bound (§6a found five power-bound), **3** are inadmissible at any rejection resistance, **4** are recovered by the *spreader* rather than by a heatsink, and **21 of 22 hold no tiles at all on the bare shell as adopted**. "Recoverable on the heatsink alone" has no referent after `SINK-D-2`; what those protocols are recoverable on is `SPEC-SINK-04`, i.e. `OI-SINK-01`, still BLOCKING | Thermal. **CLOSED** |
| **OI-SINK-06** | **Restate `OI-N1-04` to cover both signs.** §6 shows the core-to-shell conduction path is present at every ambient, not only above 37 °C; at 25 °C it costs 6.1 K of the face budget before any tile is driven | Thermal + FW |
| ~~**OI-SINK-07**~~ | ~~The absorber foam's conductivity (k 0.05) is the largest lever in §3.1's decomposition. Replace from the datasheet with `OI-R1-04`.~~ **✅ MOOT 2026-09-23 — the absorber is deleted** (`REQ-CAV-04`, GitHub #391; `NP-EMC-CAV-001` §8.2), so there is no datasheet to read. §3.1's decomposition is re-run under `NP-THERM-COOL-001` `OI-THCOOL-21`. §3.2 never depended on it | Thermal |
| **OI-SINK-08** | **`NP-HELMET-GEOM-001` §3.2 specifies BN-filled *polymer* bosses (k ~ 1–10) at the module heat pickups; R1 §5 models a *copper* via (k 400).** At the same geometry those differ by ~50× on the leg that carries ~90 % of the heat. Only the copper case has ever been analysed. Reconcile on the owning document | ME + Thermal |
| **OI-SINK-09** | *(Rev 2.)* **Carry the TMS-window cut-out into `SPEC-SINK-04`, and cost it thermally.** §7.1(3): the film may not cover CLAUDE.md §4.3's non-conductive window. Needs the window footprint (`NP-HELMET-GEOM-001` §3.3) and `OI-UPG-05`'s answer on whether T1 bowls carry one; the local `Σk·t` loss under the TMS site is an input to `OI-SINK-01` and to `OI-PWRTH-01` | Thermal + ME |

**Closed since issue:** `OI-SINK-02` and `OI-SINK-05` — `NP-PWR-THERM-001` Rev 1, 2026-09-15 (§4a and §3c).

**Answered or re-pointed elsewhere:** `OI-N1-02` **CLOSED** by §4 · `N1-D-1` lifted and replaced by
SINK-D-1 (§8.2) · `OI-HUB-C19` decoupled from the tile field (§2.3, SINK-D-3) · `OI-R1-03` unaffected
— §2.3 confirms the fan cools the outer shell only, and adds that the outer shell is not the tile
field's rejection route to the hub either.

---

## 16. Reproduction

```
bun scripts/check-thermal-sink.ts             # full report — every figure above
bun scripts/check-thermal-sink.ts --validate  # anchors only; exit 1 on drift from R1
```

The script imports every published anchor from `scripts/check-thermal-multitile.ts`, which in turn
imports `analyse()` from `check-pbm-power.ts`, so demand, coverage and thermal cannot fork
(`NP-PWRSRC-001` D-1). `--validate` asserts two things: that the re-partitioned legs sum to
`R_OUT_BASE` exactly, and that R1-compatibility mode reproduces R1's own single adiabatic cell. Per
`NP-CONV-001` §8, a convention worth writing down is worth a script — and a resistance worth
specifying is worth a check that it still follows from the document it was recovered from.

---

## 17. Revision history

| Rev | Date | Author | Change |
|---|---|---|---|
| 1 | 2026-09-08 | NeurOne Thermal / Systems Engineering | **Initial release, against `NP-THERM-CFD-N1-001` `OI-N1-02` (BLOCKING).** Specifies the rejection resistance at the BN-boss via terminus by recovering it rather than selecting it. **Central finding: there is no heatsink to specify.** R1's via is a ~32 mm radial conductor terminating on the outer bowl, a median 188 mm from the hub `NP-TOOL-HUB-001` §2 infers, and no part connects them: a solid trunk inside a head-worn mass budget drops 40 K at the N = 6 library floor and 193 K at the N = 6 R-4 point, and a heatsink bolted on at the occiput adds **zero** tiles even at `R_hub` = 0 (§2). So the terminus rejects through the helmet's own exterior, and its resistance is **the external film that is already the last term of R1's `R_OUT_BASE`** — recovered from R1's decomposition at h 8.18 W/m²K and independently from a Churchill sphere correlation plus linearised radiation at 7.81, **two derivations sharing no input agreeing to 4.7 %** (§3). **`SPEC-SINK-01`: `R_sink` = 1.08 K/W aggregate**, band 0.83–1.58 (§4). That validates N1's 1.00 K/W row and makes its 0.50 "plausible default" 2.2× optimistic. **R1 spends the same film twice** — reproducing its cell takes both pinning the skin at ambient and charging the cavity leg the full 0.18 — and the re-partitioned legs sum to 0.410 exactly, so nothing is invented (§5). **A lumped `R_sink` presumes an isothermal terminus and the bare CFRP bowl is not one**: 1.6× lat/rej, ~63 mm spreading length, eighty hot spots. **The missing component is a spreader, not a heatsink** (§7, `SPEC-SINK-04`, SINK-D-4). Under the specification the ceiling at the library floor and 25 °C is **10 tiles with a 100 µm graphite film and 2 without**, against N1's 16–78 (§8); in watts, 8.9 W at N = 6 with the spreader against 4.7 W without, converging on 31.4 W fully distributed. **The R-4 point is inadmissible across a 6–14 W/m²K × 0.090–0.150 m² box** and needs none of the soft inputs; the library-floor ceiling moves 4 → 25 across the same box and is a design target for THERM-1b, not a claim (§10). **New term found: the exterior sits 6.1 K above ambient with every tile idle**, because 80 populated sockets conduct 5.6 W from the 37 °C core into the shell — `OI-N1-04`'s mechanism in the direction that is always present (§6, `OI-SINK-06`). **`SPEC-SINK-03`: fan loss does not change `R_sink`**; the fault case is vault **occlusion**, which crosses 42 °C at φ ≥ 0.9 at N = 6 on the library floor and which fan RPM cannot observe — so `FMEA-G07-01`/`RISK-26`'s named cause and `SR-FAN-05`'s predictive claim are both wrong, while Path B1's face NTC still covers the hazard (§11, `OI-SINK-04`). **`N1-D-1` lifted and replaced by SINK-D-1**; `OI-HUB-C19` decoupled from the tile field (SINK-D-3). Eight open items `OI-SINK-01…08` (one BLOCKING), five risk rows, six decisions SINK-D-1…6 (three to principal). Adds `scripts/check-thermal-sink.ts`; exports the published anchors from `scripts/check-thermal-multitile.ts` and guards its `main()` behind `import.meta.main` so the two models cannot fork. **No locked section modified; no firmware, app or protocol changed; no figure in a released document rewritten** — `NP-THERM-CFD-N1-001`'s own `R_SINK_DEFAULT` is left alone and routed as `OI-SINK-02`. |
| 2 | 2026-09-23 | NeurOne Thermal / EMC | **`RISK-SINK-03` re-scoped against `NP-EMC-CAV-001` `OI-EMCCAV-02` (GitHub #399), and `OI-SINK-01`'s EMF clearance NARROWED — not cleared; it stays BLOCKING.** New **§7.1** works Rev 1's unentitled assertion mechanism by mechanism. **Cavity behaviour is dropped** (an exterior conductor cannot change the internal Q), and `REQ-EMI-10` is shown not to reach the film by its own text (it governs the inner carrier). **(1) ELF eddy loading of the Helmholtz actuator — cleared at first order:** a thin-shell dipole-mode model (`τ₁ = μ₀Ga/3`, `a ≤ 0.15 m`, film `G ≤ 250 S` bounded by the in-plane conductivity of oriented graphite because the record holds no σ, coupling `κ ≤ 1.40`) puts **ε ≤ 0.041 (−27.7 dB) at 300 Hz and 0.0083 (−41.6 dB) at 60 Hz** on the coil-drive → field transfer, judged read open-loop (`REQ-EMI-05` is feed-forward) against `NP-THERM-BOWL-001` §5.2's active-loop share (10 / ~20 / 30 dB): clears the 20 dB mid share across the ELF band and the 30 dB top share up to ~230 Hz with no calibration or L2 credit; the top share above ~230 Hz rests on `REQ-EMI-11`'s calibration resolving phase (a quadrature error, so it adds root-sum-square to §5.3's thermal drift: 0.2 dB at mains). At the mid share the clearance holds for `G ≤ 603 S` (~240 µm at the graphite ceiling; `SPEC-SINK-04`'s 100 µm is 2.4× inside); at the top share it holds to ~230 Hz, and above that rests on the calibration resolving phase. **The record states neither the active-loop bandwidth nor the calibration method**; both are named as conditions. *The budget was first drafted as 20 dB at both ends of §4.3's ranges and corrected before merge to `NP-THERM-BOWL-001` §5.2's 10–30 dB span, which merged while this PR was open.* Bench confirmation `EMF-1-SINK-1`. **(2) External RF — narrowed:** over the continuous area two shunt sheets in contact can only add attenuation; at the film's **terminations** (rim lip, `BOSS-1` emboss, TMS cut-out) a floating, resonant-from-~0.5 GHz conductor can drive the apertures that set the floor (`NP-BIB-EMF-001` §7.6), and `SPEC-SINK-04` sets no edge geometry to compute against — bench step `EMF-1-SINK-2` (bare vs. film-fitted, to 6 GHz, plus an emissions pre-scan; pass = no worse than bare). Edge treatment left to `OI-SINK-01`'s owner. **Found while scoping:** the film as specified would cover CLAUDE.md §4.3's non-conductive TMS window — **`RISK-SINK-07`, `OI-SINK-09`** (a cut-out, applying the locked control; not a new requirement). Rev 1's ingress half of `RISK-SINK-03` is split out unchanged as **`RISK-SINK-06`**. Also records, as this revision, `OI-SINK-07`'s 2026-09-23 MOOT mark made in place under #391/#397. **No measurement asserted; no requirement added; no safety control removed; no locked decision changed.** |
