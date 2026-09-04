# N-Tile Aggregate Thermal Analysis — The Concurrency Ceiling on the Real Lattice

**Project:** NeurOne
**Document:** NP-THERM-CFD-N1-001
**Revision:** 1
**Date:** 2026-09-03
**Status:** DESIGN STUDY — **coupled N-tile resistance network on the delivered 80-socket lattice, validated against every published `NP-THERM-CFD-R1-001` figure.** It is **not** a mesh CFD and does not close `OI-R1-01`'s mesh-independence requirement. It closes the *question* `OI-PWR-01` was opened to answer, and finds that a mesh CFD could not have answered it — see §11.
**Effective Date:** —
**Author:** NeurOne Thermal / Systems Engineering
**Approved By:** — (pending design review)
**References:** NP-THERM-CFD-R1-001 Rev 1 (§2 resistance network, §3 inward-flux ceiling, §4 τ_face, §5 BN-boss export study, §5.3 findings, OI-R1-01…05 — the anchor document); NP-THERM-CFD-001 (§4 heat-source model and η_wp, §5 BC spec, §9 decision logic); NP-THERM-CFD-C2-001 (§7 the 1D network, §2 stack-up); NP-THERM-COOL-001 (§5 aggregate cavity ceiling, OI-THCOOL-08); NP-PWR-BUDGET-001 Rev 3 (§3.2 the 4–8 tile estimate, §3.3 export efficiency, §3.5 the N = 80 extrapolation, D-4, OI-PWR-01/08/10); NP-PWRSRC-001 Rev 1 (§4.1 the cavity wall, §5 thermal dose, §7.0 coverage 2/23, D-1/D-2, OI-PWRSRC-05/07); NP-SES-PWR-001 Rev 1 (§2.1 the tile-count governor, §4 cascading); NP-HW-HEXTILE-001 Rev 8 (§9.1–9.3 concurrency, OI-HEXTILE-06/09/21); NP-ENV-OPRANGE-001 (ambient/duty envelope); NP-REQ-FANHEALTH-001 (SR-FAN-03/04); NP-HELMET-GEOM-001 (§2 radial stack, §8 THERM-1a); NP-HW-HUB-001 Rev 5 (OI-HUB-C19 hub thermal budget); NP-DT-001 Rev 2 (DI-REG-01 IEC 60601-1, DI-SAFE-13); NP-CONV-001 Rev 6 (§4 identifiers, §8 a convention worth writing down is worth a script); CLAUDE.md §3 (PBM ceilings), §4.2 (42/62 °C interlocks), §4.3 (EMF stack), §4.5 (power); IEC 60601-1 (42 °C applied part); `hardware/np_socket_map.json`; `scripts/check-thermal-multitile.ts`; `scripts/check-pbm-power.ts`; `scripts/check-thermal-dose.ts`
**Related Issues:** —
**Gate:** Does not close THERM-1a. Raises one BLOCKING item (`OI-N1-02`) that gates any quoted tile count.
**IEC 62304 Class:** — (analysis document; no code)
**Supersedes:** None — extends NP-THERM-CFD-R1-001 to N tiles
**Parent Document:** NP-PWR-BUDGET-001 §3.2 (`OI-PWR-01`)

---

> **⚠ READ FIRST — the one-paragraph version.**
>
> `NP-THERM-CFD-R1-001` solves a single periodic hex cell with **adiabatic side walls**. That is not
> the boundary condition of one tile; it is the boundary condition of an **infinite array of
> identical, identically-driven tiles**. R1 therefore already reports the fully-active limit, and
> `NP-PWR-BUDGET-001` §3.2 spends its face margin on a tile count that the margin was never a
> function of. Putting 80 tiles on the real lattice and coupling them shows the two genuinely
> N-dependent terms, and **neither is the one the tree has been arguing about**. Lateral conduction —
> the montage-clustering effect `OI-PWR-10` exists for — is worth **≤ 0.22 K** across every authored
> montage and can be dropped. What actually scales with N is the **external heatsink at the via
> terminus**, which R1 pins at ambient as a modelling idealisation and which **no document in the
> tree specifies**. The concurrency ceiling is a property of an unspecified component. **`OI-N1-02`
> is therefore BLOCKING, and no tile count may be quoted until it is closed.**

---

## 1. Headline

1. **The adiabatic identity (§3).** Driven uniformly, the 80-tile coupled model reproduces R1's
   single cell to **6 × 10⁻¹⁴ K**. It must: a uniform array has no lateral gradient, so zero lateral
   flux is *exact*. **R1's "single-tile" result is the fully-active result.** Its face margin is not
   a budget that tile count spends, and `NP-PWR-BUDGET-001` §3.2 spends it anyway.
2. **The model reproduces R1 and finds a factor of 0.65 in it (§2).** From **one** calibration point
   the network reproduces all four published temperatures to **≤ 0.45 K** and all three export
   fractions to **≤ 0.8 pp** across a 2.4× flux range. But the heat flux that produces those
   temperatures is a constant **0.646–0.676** of the flux R1's own table labels — **η_wp applied a
   second time** to an `NP-THERM-CFD-001` §4 figure that is already `q_heat = P_elec(1 − η_wp)`.
   The error is **non-conservative**: corrected, R1's T1-std face margin at nominal ambient falls
   from **11.3 K to 9.2 K**, and that 11.3 K is the exact number §3.2 divides to get 4–8 tiles.
3. **The shared sink is the whole N-dependence (§5).** R1's perfect sink is exact for one cell and
   false for N: all N tiles export ~90 % of their heat into **one** heatsink whose temperature rises
   with N. With the sink pinned, the face moves **0.2 K from N = 1 to N = 80**. At a plausible
   0.5 K/W it moves **17.6 K**. R1 assumed the flat row, and it is the only row that is not real.
4. **This inverts `NP-PWR-BUDGET-001` §3.3.** That section names export efficiency as "the actual
   lever". Raising export efficiency moves *more* heat into the term that scales with N. Above the
   crossover it makes the face **hotter**, not cooler. §3.3's lever is pointed the wrong way.
5. **`OI-PWR-10` is answerable and the answer is no (§6).** Clustered-vs-distributed at equal N and
   equal power is worth **≤ 0.22 K** at the hottest condition and **≤ 0.05 K** at the design point,
   across every authored zone. The face is within ~1 K of the junction (R1 §5.3's own finding), the
   junction is owned by the via, and lateral tissue conduction is an order of magnitude too weak to
   matter. **The flat rule does not need a clustering term.** It needs a different unit.
6. **"How many tiles" was the wrong question (§6b).** With an ideal sink the tiles are thermally
   decoupled and there is **no tile-count limit at all** — only a per-tile **drive** limit: 6.63 W/tile
   at 25 °C ambient, falling ~0.38 W/tile per °C and reaching zero at ~42.5 °C. With a real sink a
   count limit appears, and **the heatsink sets it**. This is `NP-PWR-BUDGET-001` **D-4** ("the
   governor must be watts") arriving independently from the thermal side.
7. **18 of 23 authored protocols are thermally bound, not power bound (§6a)** — the reverse of the
   framing in `NP-HW-HEXTILE-001` §9. **Four are inadmissible at any heatsink** (≥ 10 W/tile:
   Alzheimer's 1064 nm, Memory Boost, Vascular Baseline, Stroke rehab), which is robust to every
   unspecified parameter here. **Fourteen become achievable on the heatsink alone** — which makes
   `OI-N1-02` the highest-leverage open item in the thermal file.
8. **`OI-PWRSRC-07`'s premise is wrong (§7).** It infers "~39 % of steady rise" in a 6–30 min session
   from τ_face 35–45 min. That τ is R1 §4's **fan-off** value. In the healthy state the via is in
   parallel, giving τ ~15× shorter; the slow node is the scalp, not the module. Measured on the
   model, a 30-min session reaches **75–91 %** of steady rise. **The transient does not buy a power
   rung.**
9. **`OI-THCOOL-08`: the cooling-option ratios shrink by ~2.4× (§8).** `NP-THERM-COOL-001` §5's
   headline 3.28× for RFE becomes **1.36×**, because §5 attributes the whole ceiling to a cavity leg
   that carries ~10 % of the heat. **Its qualitative result survives** — the shield-safe stack still
   beats the shield-breaching one (1.36× vs 1.17×) — and that was always the load-bearing claim.
10. **`OI-PWR-08`: §3.5's ~100 °C is the right sign and order (§9).** A model valid at N = 80 gives
    **50.2 °C** at the library floor and **129.9 °C** at the R-4 point, both at 25 °C ambient. The
    bound "the 42 °C limit binds long before full population" holds. What §3.5 could not say, and
    this can: full population is bounded by **per-tile drive**, not by population.

---

## 2. Method, and what validating it exposed

### 2.1 The network

Per socket: junction `J`, module face `F`, scalp `S`, cavity `C`. One shared sink node `K`. Fixed
reservoirs at ambient and at the 37 °C perfused core. 321 nodes, solved directly.

The topology is R1's, and every resistance is R1's or is recovered from R1's published data:

| Leg | R" (m²K/W) | Source |
|---|---:|---|
| `J → F` | 0.005 | R1 §5.3 (1 mm PDMS + 0.6 mm gap) |
| `F → S` | 0.039 | recovered from R1 §5.1's published `T_face`/`T_scalp` |
| `S → core` | 0.066 | balance of `R_FACE_CORE` = 0.105 (R1 §3) |
| `J → C` | 0.23 | R1 §2 stagnant inter-bowl gap |
| `C → ambient` | 0.18 | balance of `R_OUT_BASE` = 0.41 (R1 §2) |
| `J → K` (via) | 0.00594 | back-solved from R1 §5.1's 87 % export at `T_j` 47.5 |

The two chains sum to R1's published totals by construction (0.110 and 0.41). Everything else is a
consequence.

**Lateral conduction — the term the adiabatic wall sets to zero.** Per neighbour pair,
`G = Σ(k·t)·w/L` with `w` the hex edge (23.09 mm) and `L` the centre pitch (40 mm):

| Plane | Σ k·t | G (W/K) | vs the vertical path it competes with |
|---|---:|---:|---|
| Scalp/tissue | 0.35 × 6 mm | 1.21 × 10⁻³ | 5.8 % of `S → core` per neighbour |
| Module face | 0.25 × 2 mm | 2.89 × 10⁻⁴ | negligible; the modules are discrete parts |
| Cavity/shell | CFRP 2.5 mm + µ-metal 0.2 mm | 1.79 × 10⁻² | **12.8× `C → ambient`** — the shell is near-isothermal |

That last row already settles half of `OI-PWR-10` before a single case is run: **the shell smears
cavity residual heat laterally 13× faster than it rejects it**, so clustering cannot concentrate the
cavity term. `NP-PWR-BUDGET-001` §3.2's caveat — that clustering concentrates residual cavity heat —
is **not what happens**.

### 2.2 Topology from the delivered lattice, magnitudes from the nominal hex

`hardware/np_socket_map.json` supplies 80 measured socket positions. Its own header marks the 3-space
coordinates *"PROVISIONAL, replace from shell CAD before any physical claim"*, and their median
nearest-neighbour chord (32.0 mm) is 20 % under the nominal pitch that the same file's
`tiledAreaCm2` (80 × 13.856 cm²) assumes. So the coordinates are used for **topology and clustering
metrics**, where that compression is harmless, and **never for conductance magnitudes**, where it
would propagate a coordinate artefact into every temperature. The neighbour graph cuts at 1.35× the
median NN distance and gives a mean degree of **4.65** — a hex sheet with a rim, as expected.

### 2.3 The validation, and the factor of 0.65 it exposes

`bun scripts/check-thermal-multitile.ts --validate` reproduces R1 from the single T1-std anchor:

| Config | Ambient | `T_j` | `T_face` model/pub | `T_scalp` model/pub | Export model/pub | q_heat ÷ label |
|---|---:|---:|---|---|---|---:|
| T1-std | 43.3 | 47.5 | 47.0 / 46.7 (+0.32) | 43.3 / 43.3 (+0.00) | 87.0 / 87 | 0.650 |
| T1-peak | 43.3 | 49.8 | 49.2 / 48.9 (+0.32) | 44.7 / 44.7 (−0.02) | 89.2 / 90 | 0.646 |
| T2-peak | 43.3 | 53.8 | 53.0 / 52.6 (+0.44) | 47.1 / 47.0 (+0.08) | 90.8 / 91 | 0.649 |
| T1-std | 25.0 | 30.3 | 30.6 / 30.7 (−0.10) | 33.0 / 32.8 (+0.18) | — | 0.676 |

Four temperatures to ≤ 0.45 K and three export fractions to ≤ 0.8 pp, from one calibration point,
across a 2.4× flux range and both ends of the ambient envelope. **The topology is right.**

**But the last column is a constant.** The heat flux that produces R1's published temperatures is
0.646–0.676 of the flux R1's own table names — a ratio of `1 − 0.339`, sitting squarely in
`NP-THERM-CFD-001` §4's η_wp band of 0.30–0.45. The natural reading is that **η_wp was applied twice**:
once to produce §4's "worst-zone LED-plane heat flux", and again inside R1 to a figure that was
already `q_heat`.

**Why this matters more than a bookkeeping note.** It is non-conservative, and it lands on the exact
number the tile bracket is built from. R1 §5.1's T1-std row at 25 °C ambient gives face 30.7 °C, a
**11.3 K** margin to the 42 °C limit — and `NP-PWR-BUDGET-001` §3.2 divides precisely that 11.3 K by
a per-tile cavity rise to get 4–8 tiles. Driven at the flux R1's table labels, the same row gives
face **32.8 °C** and a margin of **9.2 K**. The bracket's numerator is ~18 % too large before any
other consideration. **`OI-N1-01`, and it is R1's to resolve, not this document's** — the ratio is
reported here, the cause is inferred, and a document may not silently rewrite another's inputs.

> **Everything downstream in this study is stated in W/tile of dissipated heat**, which is unambiguous,
> with `q_heat = P_elec(1 − η_wp)` at η_wp = 0.35 as the stated bridge to the W/tile of electrical
> draw that `NP-SES-PWR-001` §2.1 and `scripts/check-pbm-power.ts` work in. No result below depends
> on the flux labels.

---

## 3. The adiabatic identity — what R1 actually computed

Drive all 80 sockets identically, perfect sink, lateral conduction **on**:

| | `T_j` | `T_face` |
|---|---:|---:|
| Single adiabatic cell (R1's configuration) | 47.5 | 47.0 |
| 80-tile coupled model | 47.5 | 47.0 |
| Difference | 4.3 × 10⁻¹⁴ | 6.4 × 10⁻¹⁴ K |

They agree to machine precision, and **they must**. A uniformly-driven array has no lateral gradient;
the lateral conductances carry nothing; adiabatic side walls are exact. This is not a numerical
coincidence, it is an identity, and it is the single most consequential sentence in this document:

> **A periodic cell with adiabatic walls is the N = ∞ boundary condition, not the N = 1 one.**

Three things follow immediately.

1. **R1's face margin is not a tile budget.** It is the margin of a *fully active vault* at that
   flux. `NP-PWR-BUDGET-001` §3.2 treats it as headroom to be consumed by adding tiles, then adds an
   aggregate cavity rise on top. The 11.3 K already has every tile in it.
2. **R1 is the clustered bound, for free.** A maximally clustered montage approaches the periodic
   case from below, so R1's numbers *are* the worst-case-clustering numbers. Lateral conduction can
   only ever make things better than R1 — it can never rescue a ceiling R1 already fails, which is
   why §6's answer to `OI-PWR-10` was bounded before it was computed.
3. **R1 §2's "2D spreading does not rescue the face" is correct and is being over-read.** It is
   correct *for the periodic case it was solved in*, and it has been carried through the tree as a
   general statement about the architecture. It is not one.

---

## 4. Face temperature vs N

At the R-4 point (`NP-HW-HEXTILE-001` §9.2: 6.25 W/tile electrical = 4.06 W/tile heat), R_sink
0.5 K/W, distributed and clustered montages grown from the same interior seed:

| N | clustered `T_face` | distributed `T_face` | sink `T` | clustering penalty |
|---:|---:|---:|---:|---:|
| 1 | 45.3 | 45.3 | 29.7 | +0.00 |
| 4 | 48.8 | 48.7 | 33.3 | +0.02 |
| 6 | 51.0 | 51.0 | 35.7 | +0.01 |
| 8 | 53.2 | 53.1 | 38.1 | +0.10 |
| 12 | 57.6 | 57.3 | 42.8 | +0.21 |
| 20 | 66.1 | 65.9 | 52.3 | +0.19 |
| 40 | 87.3 | 87.3 | 76.1 | +0.04 |
| 80 | 129.9 | 129.9 | 123.7 | +0.00 |

*(25 °C ambient. The 43.3 °C table is in the script output; it shifts every row up ~11.8 K and leaves
the penalty column within 0.01 K.)*

Two features. The **sink column tracks the face column** almost exactly — a 94 K sink rise carries an
84.6 K face rise. And the **clustering penalty peaks at 0.21 K in mid-range and vanishes at both
ends**, which is the correct shape: at N = 1 there is no neighbour to differ about, at N = 80 every
tile is active and the clustered and distributed sets are the same set.

### 4a. The tile ceiling the 42 °C limit actually permits

Largest N (distributed) holding max face ≤ 42.0 °C. `>80` = the whole lattice fits; `0` = one tile
already fails.

| Drive | Ambient | perfect | 0.25 | 0.50 | 1.00 | 2.00 K/W |
|---|---:|---:|---:|---:|---:|---:|
| library floor 1.3 W/tile | 25 | >80 | 78 | **42** | 25 | 16 |
| | 35 | >80 | 24 | 14 | 8 | 6 |
| | 43.3 | 0 | 0 | 0 | 0 | 0 |
| R-4 point 6.25 W/tile | 25 | >80 | 0 | 0 | 0 | 0 |
| | 35 | 0 | 0 | 0 | 0 | 0 |
| library ceiling 20.0 W/tile | 25 | 0 | 0 | 0 | 0 | 0 |

**The number 6 does not appear anywhere in this table.** At the library floor the thermal ceiling is
16–78 tiles depending on a heatsink nobody has chosen; at the R-4 point it is 0 unless the heatsink
is near-ideal *and* the room is at 25 °C. The coincidence `NP-PWR-BUDGET-001` §3.2 records — that a
thermal bracket of 4–8 straddles the power-derived ~6 — **does not survive a model with a heatsink in
it**, and it was a coincidence of two numbers that were both answering the wrong question.

`NP-ENV-OPRANGE-001` blocks PBM above +43 °C ambient, so the 43.3 °C rows are the envelope edge, not
an operating case. **The 25 °C rows are the design case.**

---

## 5. The shared sink — the term no single-cell model can see

Same total power in every column; distributed montage; 25 °C ambient; 1.3 W/tile:

| R_sink | N=1 | N=4 | N=6 | N=8 | N=20 | N=80 |
|---|---:|---:|---:|---:|---:|---:|
| perfect | 29.2 | 29.3 | 29.3 | 29.3 | 29.3 | **29.4** |
| 0.25 | 31.3 | 31.8 | 32.1 | 32.3 | 34.0 | 42.2 |
| 0.50 | 32.6 | 33.3 | 33.8 | 34.2 | 36.9 | **50.2** |
| 1.00 | 34.2 | 35.2 | 35.8 | 36.5 | 40.3 | 59.7 |
| 2.00 | 35.6 | 36.9 | 37.8 | 38.6 | 43.6 | 68.6 |

**The perfect-sink row is flat.** From N = 1 to N = 80 it moves 0.2 K — the entire lateral-spreading
effect. Every other row rises steeply. R1 assumed the flat row; it is an upper bound on heatsink
quality that no hardware attains, and it is the only row that is not real.

**Nothing in the document tree specifies this heatsink.** A repo-wide search for a thermal resistance
at the via terminus returns `NP-THERM-COOL-001` §5's references to *"the via's hub heatsink"*,
`NP-PWR-BUDGET-001` §3.3's *"external, fan-cooled heatsink (idealized as a 'perfect sink')"*, and
`NP-HW-HUB-001` `OI-HUB-C19`'s open residual to *"confirm hub thermal headroom … against the
`NP-TOOL-HUB-001` F-04 fan/heatsink path"* — a **placement** question with no resistance attached.
The component that sets the concurrency ceiling has never been given a number. **`OI-N1-02`,
BLOCKING.**

### 5.1 What this does to `NP-PWR-BUDGET-001` §3.3

§3.3 concludes: *"the only way to raise the aggregate ceiling … is improve via export efficiency
beyond ~90 %"*, and *"a bigger battery alone does not raise PBM concurrency — the bottleneck by this
estimate is the sealed cavity's residual-heat export path."*

The first half of that is now **backwards**. Export efficiency routes heat away from the cavity and
into the shared sink. At the design point ~90 % of the heat is already in the term that scales with
N, and only ~10 % is in the term §3.3 proposes to attack. Improving export past 90 % moves the
remaining tenth across too — and above the crossover it raises the face temperature. The correct
statement of §3.3's lever is **not** "export more" but **"reject better"**: `R_sink`, not the export
fraction. The second half of §3.3 stands, for a reason §3.3 does not give: a bigger battery does not
help because the bottleneck is rejection, and rejection is where the design has no specification.

---

## 6. `OI-PWR-10` — montage clustering, answered

Every authored zone in `protocols/predefined/00-zones.npps`, driven at the library floor, against the
most-spread montage of the **same tile count**. "Penalty" is what the geometry costs.

| Zone | Tiles | Compactness | `T_face` | Penalty |
|---|---:|---:|---:|---:|
| Frontal Left | 20 | 32.5 mm | 36.9 | +0.05 K |
| Frontal Right | 20 | 31.8 mm | 36.9 | +0.05 K |
| Parietal Left / Right | 13 | 34.8 mm | 35.4 | +0.04 K |
| Posterior | 33 | 31.3 mm | 39.8 | +0.03 K |
| Temporal / Occipital (either side) | 5 | 24.6–39.9 mm | 33.6 | +0.02 K |
| Frontal | 37 | 31.7 mm | 40.7 | +0.01 K |
| Vault (excl. Occipital) | 71 | 32.8 mm | 48.2 | +0.00 K |
| Motor / SMA | 7 | 39.2 mm | 34.0 | −0.00 K |

**Worst authored montage penalty: 0.05 K at the design point, 0.22 K at 43.3 °C ambient and the R-4
drive.** Both are inside every other uncertainty in this document by more than an order of magnitude.

**`OI-PWR-10` asks whether the flat "~6 tiles, anywhere" rule should distinguish clustered from
distributed montages. It should not.** The mechanism is visible in §2.1's table: the face sits
0.005 m²K/W from the junction and tracks it to within ~1 K (R1 §5.3's own finding), the junction is
owned by the via at 0.00594, and lateral tissue conduction offers 1.21 × 10⁻³ W/K per neighbour
against 0.233 W/K down the via. The face is not thermally its own node, so its temperature is set by
what the junction is attached to — and the junction is attached to a **global** node.

> **The bilateral-DLPFC worry that motivated `OI-PWR-10` was a real question with a small answer.**
> Recommend **closing `OI-PWR-10`** with the clustering term explicitly dropped rather than carried
> forward. What is wrong with the flat rule is its **unit**, not its lack of a geometry term.

### 6a. Per-protocol thermal ceilings — the table that matters

Against `analyse()` from `scripts/check-pbm-power.ts`, so demand and thermal cannot fork
(`NP-PWRSRC-001` D-1). 25 °C ambient. "power" is `NP-SES-PWR-001` §2.1's concurrency at that per-tile
draw; "thermal" is this model at R_sink 0.5 K/W; **"ideal"** repeats it with a perfect sink — where
*ideal* is already 0, the verdict does not depend on `OI-N1-02`.

| Protocol | W/tile | power | thermal | ideal | Binds |
|---|---:|---:|---:|---:|---|
| PBM — Alzheimer's 1064 nm | 25.0 | 1 | 0 | **0** | **THERMAL — at any heatsink** |
| Memory Boost | 25.0 | 1 | 0 | **0** | **THERMAL — at any heatsink** |
| Vascular Baseline | 20.0 | 2 | 0 | **0** | **THERMAL — at any heatsink** |
| PBM — Stroke (chronic rehab) | 10.0 | 4 | 0 | **0** | **THERMAL — at any heatsink** |
| Focus Prime / Gamma Focus / Flow State | 5.0 | 8 | 0 | >80 | THERMAL — heatsink-recoverable |
| PBM — Anxiety (Wang), Alpha Calm, Full T1 Immersive, Gamma+Theta, Alzheimer's (Chun) | 4.7–4.8 | 8 | 1 | >80 | THERMAL — heatsink-recoverable |
| PBM — MCI, ADHD Focus | 4.4 | 9 | 2 | >80 | THERMAL — heatsink-recoverable |
| Anxiety Relief, PBM — Depression (Schiffer) | 3.9–4.1 | 9–10 | 4 | >80 | THERMAL — heatsink-recoverable |
| Deep Sleep | 3.8 | 10 | 5 | >80 | THERMAL — heatsink-recoverable |
| PBM — Parkinson's | 3.1 | 12 | 9 | >80 | THERMAL — heatsink-recoverable |
| PBM — Depression (Cassano) | 2.2 | 17 | 19 | >80 | power |
| PBM — Alzheimer's 40 Hz (Woźniak), Anxiety (Maiello) | 1.9 | 21 | 25 | >80 | power |
| PBM — TBI + Intranasal | 1.4 | 29 | 39 | >80 | power |
| PBM — Autism (pediatric 40 Hz) | 1.3 | 32 | 45 | >80 | power |

**18 of 23 are thermally bound, not power bound.** `NP-HW-HEXTILE-001` §9 frames concurrency as a
power question and §9.3 consequence 2 states the aggregate thermal risk *"cannot"* bind because
"the power envelope permits ~6 tiles". That is the wrong way round for most of the library.

The split is the actionable part. **Four protocols are inadmissible regardless of heatsink** — and
all four are the high-dose ones, which connects directly to `OI-HEXTILE-21`'s emitter wall and
`NP-PWR-BUDGET-001` §3.4's efficacy floor: the protocols the evidence base most wants are the ones
the thermal envelope most refuses. **Fourteen turn on the heatsink alone.** That is the return on
`OI-N1-02`, and it is larger than the return on any other open thermal item.

### 6b. The per-tile drive limit — the ceiling that survives an ideal heatsink

A perfect sink decouples the tiles, so face temperature depends on per-tile drive and ambient and
**not on N at all**:

| Ambient | Max W/tile electrical | = W/tile heat |
|---:|---:|---:|
| 25 | 6.63 | 4.31 |
| 30 | 4.74 | 3.08 |
| 35 | 2.84 | 1.85 |
| 40 | 0.95 | 0.62 |
| 43.3 | none — 42 °C unreachable | — |

Linear in ambient at ~−0.38 W/tile per °C, reaching zero at ~42.5 °C — the same wall R1 §3 and §5.3
describe ("a passive path cannot reject below the ambient it dumps to"), now denominated per tile.

The R-4 point (6.25 W/tile) sits just under the 25 °C row, which is why §4a's R-4 perfect-sink cell
reads `>80` and every other R-4 cell reads 0: **the firmware's own maximum permitted PBM setting is
admissible only in a cool room and only with a heatsink that does not exist.** The library spans
1.3–25.0 W/tile, so this wall sits inside the library at every ambient — it **partitions** the
authored set rather than clearing or condemning it, and it is the same partition §6a tabulates.

---

## 7. `OI-PWRSRC-07` — the transient, answered and refused

`OI-PWRSRC-07` reasons: τ_face is 35–45 min, sessions are 6–30 min, so ~39 % of steady rise is
reached, implying ~70–125 W transiently for one isolated session — *"would make the 92–132 W rung
defensible; does not rescue mains."*

**The premise is wrong.** τ_face 35–45 min is R1 §4's **fan-off** figure, built on
`R_in ∥ R_out(off) ≈ 0.087 m²K/W`. In the healthy state the via is in parallel too, giving
**0.0056 m²K/W** — 15× lower. The module equilibrates in ~3 min; the slow node is the scalp
(`C_S/G_SC` ≈ 12 min), not the module.

Measured on the model, from an all-off start at 25 °C ambient and the library floor — percentages are
the fraction of the steady-state **rise** reached:

| N | cold | 6 min | 12 min | 20 min | 30 min | steady |
|---:|---:|---|---|---|---|---:|
| 6 | 29.3 | 32.1 (63 %) | 32.8 (77 %) | 33.1 (86 %) | 33.4 (91 %) | 33.8 |
| 20 | 29.3 | 32.6 (43 %) | 33.8 (60 %) | 34.8 (72 %) | 35.6 (83 %) | 36.9 |
| 80 | 29.3 | 34.5 (25 %) | 38.3 (43 %) | 41.9 (60 %) | 44.9 (75 %) | 50.2 |

**A 30-minute session reaches 75–91 % of steady rise, not 39 %.** The ~39 % figure is approached only
by a 6-minute session at N = 80 — the shortest session on the largest montage, which is not the case
the open item was arguing for.

> **Disposition: close `OI-PWRSRC-07` as NOT SUPPORTED.** The transient credit is real but small, it
> shrinks as sessions lengthen, and it must not be spent on a power rung. Its one durable
> contribution is that the credit is *larger at high N* — so if a transient allowance is ever taken,
> it must be a function of N and session length, never a flat multiplier.

---

## 8. `OI-THCOOL-08` — the cooling-option ratios, re-run

`NP-THERM-COOL-001` §5 evaluates the cooling architectures on a cavity model calibrated to reproduce
the ~6-tile rule, and marks its own tile counts as not-a-result (*"read it as 'the same order as a
threefold improvement,' never as twenty tiles"*). Re-run here at 25 °C ambient and the library floor,
ceiling = largest N holding face ≤ 42 °C:

| ID | Option | R_out | §5 ceiling | §5 ratio | **N1 ceiling** | **N1 ratio** | Shield |
|---|---|---:|---:|---:|---:|---:|---|
| BASE/V | As adopted | 0.410 | 6.0 | 1.00× | 42 | **1.00×** | ok |
| X | External ventilation | 0.200 | 12.3 | 2.05× | 49 | **1.17×** | **BREACH** |
| R | Sealed recirculation | 0.247 | 10.0 | 1.66× | 46 | **1.10×** | ok |
| RF | R + thermal absorber | 0.192 | 12.8 | 2.14× | 49 | **1.17×** | ok |
| RFE | RF + forced external | 0.125 | 19.7 | 3.28× | 57 | **1.36×** | ok |
| GFE | Gap bridge + absorber + forced ext | 0.060 | — | — | >80 | **≥1.90×** | ok |

**The ratios shrink by roughly 2.4×**, and the reason is structural: §5's model routes the entire
concurrency question through the cavity leg, which carries ~10 % of the heat. Improving a path that
carries a tenth of the load cannot triple the ceiling.

**`NP-THERM-COOL-001` §5's central claim survives, and should be restated rather than withdrawn.**
Its result was never the absolute counts — it says so itself — but the ordering: *"we do not have to
choose between the EMF claim and the thermal ceiling."* **RFE still beats X, 1.36× against 1.17×.**
The shield-safe stack still outperforms the shield-breaching one, by 16 % rather than 60 %. The
recommendation is unchanged; its margin is smaller and its stated basis needs correcting.

**And `GFE` — the static gap bridge, `NP-THERM-COOL-001`'s own D-2 comparator — is now the strongest
row**, clearing the whole lattice at the library floor with no blower, no tubes and no aperture. On
this model the pneumatic loop has no case left.

---

## 9. `OI-PWR-08` — the N = 80 extrapolation

`OI-PWR-08` records that `NP-PWR-BUDGET-001` §3.2's lumped model was never exercised beyond N ≈ 8,
that §3.5 extrapolates it to N = 80, and that the resulting ~100 °C *"is stated only to fix the sign
and order … outside the model's validity and must not be quoted as a temperature."*

This model is a node network with no N-range of validity to exceed: N = 80 is the same solve as N = 1
with a different source vector. At 25 °C ambient it gives max face **50.2 °C** at the library floor
and **129.9 °C** at the R-4 point (R_sink 0.5 K/W), and **29.4 °C** at the library floor with a
perfect sink.

**§3.5's sign and order are confirmed, and its caveat can be lifted — but the conclusion it supports
should be re-derived.** §3.5's point is that the 42 °C limit binds long before full population. True.
But the spread across those three figures — 29.4 to 129.9 °C, all at N = 80 — shows the figure is
**not a property of N = 80**. It is a property of per-tile drive and of `R_sink`. **A full lattice is
thermally unremarkable at the library floor with a good heatsink.** `OI-HEXTILE-06`'s population
decision therefore has less thermal content than §3.5 implies, and its cost content is untouched:
**populate for placement freedom and govern in watts** — which is where `NP-PWR-BUDGET-001` D-4 and
`OI-HEXTILE-09` already point.

> One consequence worth stating separately, because it is new and it is not obvious. In the
> **fully-populated** lattice at high ambient, the 80 vias are conductive paths between the shared
> sink and the 37 °C head. When ambient exceeds body temperature the head becomes the coldest
> reservoir in the assembly, and **idle populated tiles carry heat into the scalp**. This is R1 §2's
> *"the junction throttle regulates the wrong node"* generalised to the array, it scales with
> population rather than with activity, and no interlock in `CLAUDE.md` §4.2 observes an idle tile.
> **`OI-N1-04`.**

---

## 10. `OI-PWRSRC-05` — the thermal-dose constants

`OI-PWRSRC-05` holds `scripts/check-thermal-dose.ts` at flag-for-review rather than a CI gate, with
its constants as named exports, *"until `OI-PWR-01` lands"*.

**It should stay flag-for-review, and the reason has changed.** The original reason was that a
verification-grade CFD would replace provisional constants with real ones. This study does not supply
that: it re-derives the *network*, not the *tissue* constants (CEM43 reference, perfusion bounds,
ambient sensitivity) that `check-thermal-dose.ts` actually carries. Those are still provisional and
still need THERM-1b bench correlation (`OI-R1-02`).

What has changed is that the dose check now has a **worse** unresolved input than its tissue
constants. `NP-PWRSRC-001` §5's finding — cascading holds the cavity at the interlock ceiling and
puts Vascular Baseline at 292 CEM43 — was computed against the 42 °C interlock as the temperature
bound. §6a finds **Vascular Baseline is thermally inadmissible at one tile at any heatsink**, so the
cascade that generates the 292 CEM43 exposure is a cascade of sessions that cannot run as authored.
The dose finding's *direction* is unaffected and its *magnitude* is now unquotable.

> **Disposition: `OI-PWRSRC-05` stays open and is re-pointed at `OI-R1-02` (bench correlation) rather
> than at `OI-PWR-01`.** Add `R_sink` to the constants the dose model must carry once `OI-N1-02`
> closes. **`NP-PWRSRC-001` §5's 292 CEM43 figure should be re-run** against the admissible drive
> set from §6b rather than against the authored one — **`OI-N1-05`**.

---

## 11. What this does *not* close

**This is not a mesh CFD, and `OI-R1-01` remains open.** It has no mesh, so it cannot claim
mesh-independence; it resolves nothing inside a tile; contact conductances are R1's; tissue
properties are literature; and no bench has correlated any of it (`OI-R1-02`).

**But `OI-PWR-01` should not be left as written.** It asks for a "verification-grade multi-tile
aggregate CFD" to establish the concurrency ceiling. The finding of §5 is that the ceiling is not a
field-resolution question at all: it is set by a lumped external component with no specification. A
mesh CFD of the helmet would have resolved the cavity and the stack beautifully and returned an
answer parameterised on the same unknown `R_sink`. **Spending a CFD to learn that would have been the
expensive way to learn it.**

> **Recommended disposition of `OI-PWR-01`: NARROWED, not closed.** The aggregate-concurrency and
> montage-clustering questions it was opened for are answered (§4, §4a, §6). What remains of its
> original scope is the intra-tile field and the mesh-independence requirement, which belong to
> `OI-R1-01` where they started. **It should not gate anything that `OI-N1-02` does not gate first.**

---

## 12. Decisions

| Ref | Decision | Basis | Reversible |
|---|---|---|---|
| **N1-D-1** *(principal)* | **No tile count may be quoted as a thermal ceiling until `R_sink` is specified.** The ceiling ranges 16–78 tiles at the library floor across a plausible heatsink range, and 0 at the R-4 point | §4a, §5 | Yes — on `OI-N1-02` |
| **N1-D-2** | **Drop the montage-clustering term.** Close `OI-PWR-10` rather than carrying it | §6: ≤ 0.22 K worst case across every authored montage | Yes |
| **N1-D-3** | **Restate `NP-PWR-BUDGET-001` §3.3's lever as rejection (`R_sink`), not export fraction** | §5.1: ~90 % of heat is already in the N-scaling term | Yes |
| **N1-D-4** | **The governor is watts per tile, and the thermal side now says so independently** — §6b's wall is a per-tile drive limit with no N in it | §6b, and `NP-PWR-BUDGET-001` D-4 from the power side | No — two independent derivations |
| **N1-D-5** | **`OI-PWRSRC-07` closed as NOT SUPPORTED**; no transient allowance against a power rung | §7: 75–91 % of steady rise at 30 min, not 39 % | Yes — on bench data |
| **N1-D-6** | **`NP-THERM-COOL-001` §5's recommendation stands; its ratios are superseded by §8** and its stated basis corrected | §8: RFE 1.36× still beats X 1.17× | Yes |
| **N1-D-7** *(principal)* | **Four authored protocols are thermally inadmissible at any heatsink and must not be presented as deliverable** pending a drive-level or emitter change | §6a "ideal" column = 0 | Yes — on emitter selection (`OI-HEXTILE-02`) |

---

## 13. Risk rows

| Ref | Hazard | Current control | Verification |
|---|---|---|---|
| RISK-N1-01 | Concurrency ceiling quoted from a model with no heatsink in it; a montage is authorised that exceeds the face limit | N1-D-1 — no count quoted until `OI-N1-02` | Heatsink spec + THERM-1b bench (`OI-R1-02`) |
| RISK-N1-02 | Four protocols in the shipped library are thermally inadmissible at one tile | N1-D-7; `scripts/check-thermal-multitile.ts` §6a is the standing check | **No verification defined** — needs a compile-time gate, `OI-N1-03` |
| RISK-N1-03 | R1's flux labels carry η_wp twice; downstream margins are ~18 % optimistic | §2.3 reports the ratio; no document rewritten | `OI-N1-01` on R1 |
| RISK-N1-04 | Idle populated tiles conduct heat into the scalp above ~37 °C ambient; no interlock observes an idle tile | `NP-ENV-OPRANGE-001` blocks > +43 °C ambient (partial — it gates PBM, not conduction) | **No verification defined** — `OI-N1-04` |

---

## 14. Open items

| Ref | Item | Owner |
|---|---|---|
| **OI-N1-01** | **R1's §5.1 flux labels reproduce at 0.649 × their stated value**, consistent with η_wp applied twice to an `NP-THERM-CFD-001` §4 figure that is already `q_heat`. Non-conservative; lands on the 11.3 K that `NP-PWR-BUDGET-001` §3.2 divides. Resolve on the owning document | Thermal |
| **OI-N1-02** | **BLOCKING — specify the external heatsink at the via terminus (`R_sink`, K/W, and its fan-loss case).** It sets the concurrency ceiling, it is the difference between 0 and >80 tiles for 14 of 23 protocols, and no document in the tree gives it a number. Route with `NP-HW-HUB-001` `OI-HUB-C19` and `NP-TOOL-HUB-001` F-04 | Thermal + ME |
| **OI-N1-03** | **No gate stops a thermally inadmissible protocol compiling.** `hubCompiler.ts` has no thermal check, as it had no power check before `OI-HEXTILE-09`. The two should be one check in watts per tile | FW + App |
| **OI-N1-04** | **Idle populated tiles are conductive paths from the shared sink into the scalp** when ambient exceeds body temperature. Scales with population, not activity; no interlock observes an idle tile. Decide with `OI-HEXTILE-06` | Thermal + FW |
| **OI-N1-05** | **Re-run `NP-PWRSRC-001` §5's 292 CEM43 cascade** against §6b's admissible drive set rather than the authored one. Direction unaffected; magnitude currently unquotable | Thermal |
| **OI-N1-06** | Lateral conductances are literature `k·t` estimates. §6's conclusion is robust (it survives an order of magnitude either way, because the competing via path is 200× larger), but the values should be replaced from the shell laminate datasheet with `OI-R1-04` | Thermal |
| **OI-N1-07** | Socket 3-space coordinates are provisional; topology is used, magnitudes are not (§2.2). Re-run when shell CAD replaces the interim ellipsoid | Thermal + ME |

**Carried, re-pointed or recommended for closure elsewhere:** `OI-PWR-01` NARROWED (§11) · `OI-PWR-08`
answered, caveat liftable (§9) · `OI-PWR-10` recommended CLOSED (§6) · `OI-PWRSRC-05` re-pointed at
`OI-R1-02` (§10) · `OI-PWRSRC-07` recommended CLOSED, not supported (§7) · `OI-THCOOL-08` answered,
`NP-THERM-COOL-001` §5 ratios superseded (§8).

---

## 15. Reproduction

```
bun scripts/check-thermal-multitile.ts             # full report — every figure above
bun scripts/check-thermal-multitile.ts --validate  # anchors only; exit 1 on drift from R1
```

The script imports `analyse()` from `scripts/check-pbm-power.ts` so that demand, coverage, dose and
thermal cannot fork (`NP-PWRSRC-001` D-1). Every published R1 anchor is a named constant; changing one
invalidates the model, and `--validate` says so. Per `NP-CONV-001` §8, a convention worth writing down
is worth a script — and an anchor worth quoting is worth a check that it still reproduces.

---

## 16. Revision history

| Rev | Date | Author | Change |
|---|---|---|---|
| 1 | 2026-09-03 | NeurOne Thermal / Systems Engineering | **Initial release, against `OI-PWR-01`.** Coupled 321-node N-tile network on the delivered 80-socket lattice, validated against all four published `NP-THERM-CFD-R1-001` figures to ≤ 0.45 K and all three export fractions to ≤ 0.8 pp from a single calibration point. **Central finding: an adiabatic-walled periodic cell is the N = ∞ boundary condition, not N = 1**, so R1 already reported the fully-active limit and `NP-PWR-BUDGET-001` §3.2 spends a margin that was never a function of tile count (§3). **The two genuinely N-dependent terms are not the ones under discussion:** lateral conduction (the `OI-PWR-10` clustering effect) is worth ≤ 0.22 K across every authored montage and is dropped (§6, N1-D-2), while the **shared external heatsink** — pinned at ambient by R1 as an idealisation and **specified nowhere in the tree** — sets the entire ceiling, ranging it from 16 to 78 tiles at the library floor and to 0 at the R-4 point (§4a, §5). **`OI-N1-02` BLOCKING; N1-D-1 forbids quoting any tile count until it closes.** Inverts `NP-PWR-BUDGET-001` §3.3: export efficiency routes heat *into* the N-scaling term, so the lever is rejection, not export (§5.1, N1-D-3). **18 of 23 authored protocols are thermally bound rather than power bound**, four of them inadmissible at one tile at any heatsink and fourteen recoverable on the heatsink alone (§6a). **With an ideal sink there is no tile-count limit at all, only a per-tile drive wall** of 6.63 W/tile at 25 °C falling ~0.38 W/tile per °C to zero at ~42.5 °C — the thermal side arriving independently at `NP-PWR-BUDGET-001` D-4's governor-in-watts (§6b, N1-D-4). **Folded-in items answered:** `OI-PWR-08` — §3.5's sign and order confirmed from a model with no N-range of validity, but N = 80 spans 29.4–129.9 °C so the figure is a property of drive and `R_sink`, not of population (§9); `OI-PWRSRC-07` — premise refuted, its τ is R1's fan-off value and a 30-min session reaches 75–91 % of steady rise, not 39 %, recommended closed as not supported (§7, N1-D-5); `OI-THCOOL-08` — `NP-THERM-COOL-001` §5's ratios shrink ~2.4× (RFE 3.28× → 1.36×) because §5 routes the ceiling through a cavity leg carrying ~10 % of the heat, **but its central ordering survives** — the shield-safe stack still beats the shield-breaching one, and the static gap bridge `GFE` is now the strongest row, leaving the pneumatic loop with no case (§8, N1-D-6); `OI-PWRSRC-05` — stays flag-for-review, re-pointed at bench correlation `OI-R1-02` rather than at `OI-PWR-01`, and `NP-PWRSRC-001` §5's 292 CEM43 figure needs re-running against the admissible drive set (§10, `OI-N1-05`). **One defect found in the anchor document:** R1's §5.1 temperatures reproduce at **0.649 ×** the flux its own table labels, consistent with η_wp applied twice, which is non-conservative and lands on the exact 11.3 K margin §3.2 divides — reported, not silently corrected (`OI-N1-01`). **`OI-PWR-01` recommended NARROWED, not closed:** the concurrency and clustering questions are answered, the intra-tile field and mesh-independence return to `OI-R1-01`, and a mesh CFD could not have answered the question because the binding term is lumped and unspecified (§11). Adds `scripts/check-thermal-multitile.ts`. **No locked section modified; no firmware, app or protocol changed.** |
