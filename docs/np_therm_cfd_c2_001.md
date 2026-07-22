# NP-THERM-CFD-C2-001 Rev A — Case C2 Go/No-Go Run Card (Path-A admissibility)

**Program:** NeurOne chassis / thermal
**Status:** DRAFT run card — fully-pinned parameters for the single worst-fault case that decides
whether NP-REQ-FANHEALTH-001 **Path A** (existing junction throttle suffices) is admissible. Runnable
as a 1D hand-calc first, then a 2D-axisymmetric conjugate-heat confirm.
**Parent:** NP-THERM-CFD-001 (case matrix, §6 row C2) · **Feeds:** OI-FAN-01a decision gate.
**Date:** 2026-07-21

---

## 1. The question, and the elegant formulation

**Question:** with the fan fully stopped (worst fault), does holding the LED **junction** at its
throttle temperature keep the **scalp-facing surface ≤ 42 °C**? If yes → Path A (no new interlock).
If no → Path B interlock required, and the full matrix sizes it.

**Formulation (assumption-light):** do **not** model LED efficiency and solve for junction. Instead
**pin the junction node at the throttle temperature** (the value SW01-M04 actually regulates) and solve
for the face. This sidesteps the wall-plug-efficiency uncertainty entirely and directly answers "does
junction ≤ T guarantee face ≤ 42 °C?". Two sub-runs:

- **C2a — junction Dirichlet T_j = 62 °C** (throttle setpoint, sustained regulated point) — the realistic pass condition.
- **C2b — junction Dirichlet T_j = 65 °C** (cutoff threshold, hottest sustained-before-cutoff) — the conservative robustness check.

A fixed-flux cross-check (T2-peak heat at the LED plane, η_wp = 0.15–0.25 for the 1170 nm laser + TEC)
is optional to visualize the junction rise, but the go/no-go is taken from C2a/C2b.

---

## 2. Fixed geometry (single-hex column, symmetry walls)

| Node (scalp → exterior) | Material | Thickness |
|---|---|---|
| deep tissue BC | — | 37 °C Dirichlet at gray base |
| gray matter | tissue | 20 mm (truncated) |
| CSF | tissue | 2 mm |
| skull | tissue | 7 mm |
| scalp | tissue | 6 mm |
| **bezel gap** | air (conduction) | **0.6 mm** ← C2 worst (thin = max coupling) |
| optical window | PDMS | 1.0 mm |
| **LED / junction plane** | Dirichlet source | T_j = 62 / 65 °C |
| module body + BN boss | glass-filled PBT / BN | 14 mm |
| socket wall | glass-filled PBT | 2.5 mm |
| inter-bowl gap | air (conduction) | 6 mm |
| absorber foam | foam | 3 mm |
| Pd-poly + mu-metal | — | 0.1 + 0.2 mm |
| CFRP shell (outer face) | CFRP | 2.5 mm |

Hexagon 40 mm flat-to-flat; lateral walls adiabatic-symmetry (periodic vault).

## 3. Fixed material properties (pessimistic single values — low outward k, so a PASS is conservative)

| Material | k (W/m·K) | ρ | cp | Note |
|---|---|---|---|---|
| Scalp | 0.34 | 1100 | 3500 | |
| Skull | 0.32 | 1900 | 1700 | |
| CSF | 0.57 | 1000 | 4000 | |
| Gray | 0.51 | 1040 | 3600 | |
| Air (both gaps) | 0.026 | 1.2 | 1005 | conduction only — Ra ≈ 300 at 6 mm < 1708, verified |
| PDMS window | 0.20 | 970 | 1460 | |
| Glass-filled PBT | **0.30** | 1550 | 1300 | low end = pessimistic (traps heat inward) |
| BN boss | **1.0** | 1900 | 800 | low end |
| Absorber foam | **0.05** | 300 | 1000 | low end |
| Mu-metal / Pd | 30 / 0.15 | — | — | negligible |
| CFRP (through-thick) | **0.8** | 1600 | 900 | low end |
| Blood (perfusion) | — | 1050 | 3600 | Pennes |

## 4. Fixed boundary conditions

- **Junction:** Dirichlet 62 °C (C2a) / 65 °C (C2b).
- **Deep tissue:** Dirichlet 37 °C at gray base.
- **Perfusion (Pennes):** `q = ρ_b cp_b w_b (37 − T)` in scalp/skull/brain, **w_b = 0.0005 s⁻¹ (low, pessimistic)**, T_art = 37 °C.
- **Outer shell:** natural convection + radiation to a **43.3 °C (110 °F)** surround; **h_conv = 5 W/m²·K (low end)**, ε = 0.9.
- **Optical co-heating:** +5 mW/cm² volumetric in the 6 mm scalp (run with and without to isolate its share).
- **Lateral:** adiabatic symmetry.
- **Interfaces:** include realistic contact conductances (window↔LED, boss↔socket, socket↔shell); perfect contact is optimistic — flag if used.
- **Regime:** steady-state (the go/no-go is a steady question; τ_face transient lives in case C6).

## 5. Outputs

1. **T_face** — window outer / scalp-facing (applied-part) surface temperature.
2. **T_scalp_surf** — scalp skin surface temperature.
3. q_inward, q_outward, and the **heat-split fraction** (inward ÷ total).

## 6. Decision rule

| Result | Verdict | Action |
|--------|---------|--------|
| T_face **and** T_scalp_surf ≤ 42 °C at **T_j = 65 °C** (C2b) | **GO — Path A admissible** | Junction throttle alone protects scalp; add only SR-FAN-05 monitoring (Class B). Still confirm with the fan-nominal peak case C4. |
| T_face **or** T_scalp_surf > 42 °C at **T_j = 62 °C** (C2a) | **NO-GO — Path B required** | Commit the interlock; run C3 (safe-duty ceiling) + C6 (τ_face) to size SR-FAN-03/04. Recommend Path B1 (scalp NTC). |
| Between (pass 62, fail 65) | **MARGINAL** | Treat as NO-GO for a Class C burn hazard; document the thin margin. |

---

## 7. 1D pre-check (do this first — hours, not days)

A 1D radial resistance network (per unit area, m²K/W) gives the expected answer and a validation target
for the CFD. Junction pinned at 62 °C; heat splits to the 37 °C perfused core (inward) and the 43.3 °C
fan-off ambient (outward).

**Inward** (junction→core): window 0.005 + gap(0.6 mm) 0.023 + scalp/core(low perf) ≈ 0.080 → **≈ 0.108**
**Outward** (junction→ambient): body/boss ≈ 0.02 + socket 0.006 + **inter-bowl stagnant air 0.231** + foam 0.043 + CFRP 0.003 + fan-off conv/rad ≈ 0.09 → **≈ 0.393**

- q_inward = (62−37)/0.108 ≈ **231 W/m² (23 mW/cm²)**; q_outward = (62−43.3)/0.393 ≈ **48 W/m² (4.8 mW/cm²)**
- **T_face ≈ 62 − 231·0.005 ≈ 61 °C**; **T_scalp_surf ≈ 37 + 231·0.080 ≈ 55 °C** (≈ 51 °C at mid perfusion)

> **Predicted verdict: NO-GO (Path B required), decisively.** With the fan off, the stagnant inter-bowl
> air gap + fan-off convection make the **outward path ~4× more resistive than the inward path to the
> perfused scalp**, so most junction heat dumps into the patient — the junction throttle regulates the
> wrong node. To make Path A work at the 0.6 mm bezel you would need to roughly **triple the inward
> resistance**, i.e. a **~3.5 mm scalp standoff** (impractical for fit/optics). This is the quantitative
> reason to measure face temperature directly (Path B1) rather than infer safety from the junction NTC.

The CFD's job is now narrow: **confirm** this (curved geometry, contact resistances, 2D spreading) and
either uphold NO-GO or, if it surprisingly passes, explain why the 1D was pessimistic.

## 8. Pessimism ledger (why a PASS would be conservative)

Fan OFF · ambient 43.3 °C · low perfusion · 0.6 mm bezel (max coupling) · low-end outward k on every
outward layer · low convection coefficient · optical co-heat included · junction pinned at the throttle
temperature. Any real operating point is cooler, so **GO under these assumptions is safe; NO-GO is
expected and robust.**

## 9. Effort & path to the gate

1. **1D closed-form** (§7) — ~hours; already predicts NO-GO. *(Thermal)*
2. **2D-axisymmetric conjugate-heat + bioheat** — ~1–2 days; C2a + C2b; mesh <0.1 mm at gap/window, mesh-independence <0.2 °C. *(Thermal)*
3. **Decision gate** — publish GO/NO-GO; if NO-GO (expected), release C3/C6 to size SR-FAN-03/04 and confirm Path B1 with ME (OI-FAN-02). *(Thermal + FW + Quality)*

## 10. Cross-references

NP-THERM-CFD-001 (parent, full matrix) · NP-REQ-FANHEALTH-001 (SR-FAN, Path A/B) · NP-THERM-BEZEL-001
(THERM-1 first-order model this reproduces) · NP-FMEA-GEOM-001 (FMEA-G07-01) · NP-PLAN-FANHEALTH-001
(OI-FAN-01a) · CLAUDE.md §4.2/§4.5.
