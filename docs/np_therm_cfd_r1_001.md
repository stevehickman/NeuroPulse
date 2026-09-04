# NP-THERM-CFD-R1-001 — THERM-1a First-Pass Analysis Results (C2 / C3 / C6) + BN-Boss Conductive-Export Study

**Program:** NeurOne chassis / thermal
**Status:** DESIGN STUDY — **1D closed-form + rough non-verification-grade axisymmetric FD.** Advances
THERM-1a but does **not** close it: the verification-grade 2D-axisymmetric CFD (NP-THERM-CFD-001 §9
step 2 — mesh-independence < 0.2 °C, contact conductances, curved geometry) and the THERM-1b
scalp-phantom bench remain OPEN. Numbers are directional; multi-°C margins are robust to the modelling.
**Parent:** NP-THERM-CFD-001 (BC spec, case matrix) · NP-THERM-CFD-C2-001 (C2 run card).
**Extended by:** NP-THERM-CFD-N1-001 Rev 1 (N-tile, non-adiabatic, on the real 80-socket lattice —
`OI-PWR-01`). **Two things it establishes about this document, and both change how it should be read:**
(i) the single periodic cell with adiabatic side walls is the **N = ∞** boundary condition, not the
N = 1 one, so §5.1 already reports the fully-active limit and §2's *"2D spreading does not rescue the
face"* is correct **for the periodic case only** and must not be read as a general statement about the
architecture; (ii) §5.1's flux labels reproduce §5.1's own temperatures at 0.649 × their stated value
(`OI-R1-06` below).
**Feeds:** NP-REQ-FANHEALTH-001 (SR-FAN-03/04 path selection + constants) · NP-FMEA-GEOM-001
(FMEA-G07-01) · NP-ENV-OPRANGE-001 (PBM high-temp bound) · a candidate base-thermal design input.
**Method:** 1D radial resistance network (per NP-THERM-CFD-C2-001 §7) + cell-centred finite-volume
axisymmetric (r,z) conjugate-heat + Pennes-perfusion solver, SOR to ≤ 1e-6 residual. Pessimistic
low-k outward property set (NP-THERM-CFD-C2-001 §3). Single periodic hex cell (40 mm f2f → 21 mm
area-equiv radius), adiabatic side walls. **Not** mesh-independence-verified.
**Date:** 2026-07-22

---

## 1. Headline

- **C2 (Path-A admissibility): NO-GO — decisively.** The 62 °C junction throttle does **not** bound the
  scalp-facing face ≤ 42 °C under the worst fault. Confirmed 1D and 2D. **Path B interlock required.**
- **C3 (fan-off safe-duty ceiling):** at worst-case ambient the safe LED-plane flux is ~4.5 mW/cm² —
  single-digit % of any config's rated flux. Operationally: **PBM must halt (or trickle) on fan loss,**
  and the ceiling shrinks toward zero as ambient rises to the 43.3 °C envelope edge.
- **C6 (transient):** τ_face is **tens of minutes**; t₄₂ after fan loss is **minutes**. SR-FAN-04's
  ≤ 10 s nominal response target has a 2+ order-of-magnitude margin. The binding parameter is the
  **steady ceiling (SR-FAN-03)**, not detection/response speed.
- **BN-boss conductive-export study:** a solid via to an external heatsink exports ~90 % of module heat
  and keeps the junction throttle-free **without ventilating the shielded interior** — a good base-thermal
  (junction) design — but it does **not** clear face ≤ 42 °C at worst-case ambient, and gives **zero**
  benefit in the fault. The face ceiling at high ambient is an **ambient/duty envelope** control, not a
  heat-path problem.

## 2. C2 — Path-A admissibility (fan OFF, 43.3 °C, low perfusion)

Junction pinned at the regulated point; solve for the face. 1D per NP-THERM-CFD-C2-001 §7; FD confirm.

| Sub-run | 1D T_face | FD T_face | 1D T_scalp | FD T_scalp | Verdict |
|---|---:|---:|---:|---:|---|
| C2a — T_j = 62 °C (throttle setpoint) | 60.9 | **60.2** | 55.6 | **52.2** | NO-GO |
| C2b — T_j = 65 °C (cutoff) | 63.7 | **63.0** | 57.9 | **54.0** | NO-GO |
| C2a + optical co-heat (+5 mW/cm²) | — | **60.3** | — | **52.8** | NO-GO |

Face and scalp exceed 42 °C by **14–21 °C**, failing even the easier C2a condition. Root cause
(NP-THERM-CFD-C2-001 §7): fan-off, the outward path (≈ 0.41 m²K/W, dominated by the **internal stagnant
inter-bowl air gap**, ≈ 0.23) is ~3.7× more resistive than the inward path to the perfused scalp
(≈ 0.11), so ~83 % of junction heat dumps into the patient — **the junction throttle regulates the wrong
node.** 2D spreading does not rescue the face (periodic vault → adiabatic side walls → no lateral relief);
FD face = mean = peak, confirming the 1D. **Decision (per NP-THERM-CFD-001 §9): Path A dead → Path B.**

## 3. C3 — natural-convection-safe duty ceiling (fan OFF, 1D)

Binding constraint is the inward flux: to hold face ≤ 42 °C against the 37 °C perfused core
(R_face→core ≈ 0.105 m²K/W, low perfusion), **q_inward ≤ 47.4 W/m².** At the ceiling the junction floats
to ≈ 42.2 °C, so the outward path carries almost nothing. Safe LED-plane source flux vs ambient:

| Ambient | Safe source flux | ≈ % of rated flux (T1-std / T1-pk / T2-pk)† |
|---|---:|---|
| **43.3 °C** (worst) | **~45 W/m² (4.5 mW/cm²)** | ~3–4.5 % / ~2.4 % / ~1.5 % |
| 25 °C (nominal) | ~90 W/m² (9.0 mW/cm²) | ~6–9 % / ~4.7 % / ~3 % |

† rated fluxes from NP-THERM-CFD-001 §4 — provisional, datasheet-dependent.

**SR-FAN-03 constant:** the fan-off natural-convection-safe ceiling is **~halt/trickle**, ambient-scaled,
→ zero as ambient → 42 °C. Publish per-config as safety-MCU config constants, marked TBD-per-datasheet.

## 4. C6 — τ_face and t₄₂ (1D transient)

Lumped swing mass (14 mm PBT body 28.2 + PDMS 1.4 + partial scalp ~11 ≈ **30–40 kJ/m²·K**) against
R_in ∥ R_out(off) ≈ 0.087 m²K/W → **τ_face ≈ 35–45 min** (order tens of minutes; corroborated by raw
diffusion time L²/α ≈ 22 min through the 14 mm low-k PBT). Steady face rise on fan loss depends on the
still-unpinned fan-airflow model (see §6): **~0.6 °C** if the fan cools only the outer shell,
**up to ~6 °C** (t₄₂ ≈ 16 min) if the fan ventilates the inter-bowl gap. Either way t₄₂ is **minutes.**

**SR-FAN-04 constant:** T_resp ≤ 10 s nominal is met with a 2+ order-of-magnitude margin; τ_face and t₄₂
per config to be finalised by the verification-grade CFD. Thermal response speed is **not** the hard
constraint — the steady ceiling (§3) governs.

## 5. BN-boss conductive-export study (proposed alternative to ventilating the cavity)

**Motivation:** ventilating the inter-bowl cavity to lower the outward resistance would breach the EMF
shield and the sealed interior (apertures through mu-metal + RF liners), the product's differentiators.
Alternative tested: a solid conductive via down the boss centreline to an **external** fan-cooled
heatsink, **cavity left stagnant** (interior never ventilated). Flux-driven FD, copper via (r 4 mm,
k 400), **perfect sink** (via end pinned at ambient — the best any fan-cooled heatsink can do).

**5.1 Healthy state (43.3 °C ambient, perfect sink):**

| Config | Junction | Face | Scalp | vs 42 °C | Heat via export |
|---|---:|---:|---:|---:|---:|
| T1-std (125 mW/cm²) | 47.5 | **46.7** | 43.3 | +4.7 FAIL | 87 % |
| T1-peak (190) | 49.8 | **48.9** | 44.7 | +6.9 FAIL | 90 % |
| T2-peak (300) | 53.8 | **52.6** | 47.0 | +10.6 FAIL | 91 % |
| T2-peak, via 5 mm | 51.1 | 50.1 | 45.4 | +8.1 FAIL | 93 % |
| T1-std, **25 °C ambient** | 30.3 | **30.7** | 32.8 | **PASS** | — |

**5.2 Fault state (external sink / fan airflow LOST, junction throttled to 62 °C):**

| Case | T_face | T_scalp | vs 42 °C |
|---|---:|---:|---:|
| No via (base) | 60.3 | 52.9 | FAIL |
| **Copper via 4 mm** | **60.3** | **52.9** | **FAIL — identical to no-via** |

**5.3 Findings.**
- **The via works as a junction-management strategy:** ~90 % heat exported, junction throttle-free
  (54 < 62 °C) at full T2-peak, **cavity stagnant → shield/IP moat intact.** Recommended as the base
  thermal (C4/junction) design over cavity ventilation.
- **It does not clear face ≤ 42 °C at worst-case ambient,** for two compounding reasons: (a) the face
  sits ~1.6 mm from the LED plane (1 mm PDMS + 0.6 mm gap) and tracks the junction to within ~1 °C —
  ~240 W/m² leaks straight to scalp, removed by perfusion but leaving the tissue hot; (b) a passive path
  cannot reject below the ambient it dumps to, and **ambient 43.3 °C already exceeds the 42 °C limit** —
  even a zero-resistance sink leaves junction (and face) above 42 °C. Same wall as §3.
- **Zero fault benefit:** with the sink dead the bottleneck moves to the natural-convecting outer shell
  and the via's low resistance is wasted — fault face = no-via face exactly. **The fan-health interlock
  is still fully required** with the via design.
- **25 °C ambient passes with margin** (face 31 °C) — the face failure is entirely an extreme-ambient
  effect.

## 6. Decision logic (feeds NP-REQ-FANHEALTH-001 and NP-ENV-OPRANGE-001)

1. **Path A is dead** (junction throttle alone), confirmed with and without the export via → **commit
   Path B.** Recommend **Path B1 (direct scalp-facing NTC)**: the junction is not a timely or sufficient
   proxy for the face under fan-off (§2), and adding the via does not change that.
2. **SR-FAN-03** (fan-off safe-duty ceiling) = §3 — halt/trickle, ambient-scaled config constants.
3. **SR-FAN-04** (response time) = §4 — ≤ 10 s trivially satisfied; τ_face/t₄₂ per config from the CFD.
4. **The face ≤ 42 °C ceiling at high ambient is owned by the firmware ambient/duty gate**
   (NP-ENV-OPRANGE-001: PBM full ≤ +35, derate +35→+43, block > +43 °C; enforced NP-FW-POE-001), **not by
   any cooling hardware.** No passive path removes it because it is ambient-bounded.
5. **Base thermal / junction design (C4): BN-boss conductive export to an external heatsink ADOPTED**
   (2026-07-22, completed-decisions.md) — keeps the junction throttle-free at full flux without
   ventilating the shielded interior. Run the verification-grade C4 with this architecture to confirm.

## 7. Open items / follow-ons (tracked; not actioned in this note)

| Ref | Item | Owner |
|---|---|---|
| OI-R1-01 | Verification-grade 2D-axisymmetric CFD (NP-THERM-CFD-001 §9 step 2): mesh independence < 0.2 °C, realistic contact conductances, curved geometry, C2a/C2b/C3/C4/C6 | Thermal |
| OI-R1-02 | THERM-1b scalp-phantom fan-stall bench — correlate before any result is verification-grade | Thermal + ME |
| OI-R1-03 | Pin the fan-airflow path (does the fan ventilate the inter-bowl gap or only cool the outer shell?) — swings C4 and C6; the conductive-export architecture makes it moot for the junction but it must be documented | ME + Thermal |
| OI-R1-04 | Replace literature property placeholders (PBT / BN / foam / CFRP k; PDMS measured) and the fan-nominal convection coefficient with datasheet/measured values | Thermal + EE |
| **OI-R1-06** | **The §5.1 flux labels do not reproduce §5.1's own temperatures.** `NP-THERM-CFD-N1-001` §2.3 rebuilds this document's resistance network and recovers all four published temperatures to ≤ 0.45 K and all three export fractions to ≤ 0.8 pp from a single calibration point — but the heat flux that produces them is a constant **0.649 × (range 0.646–0.676)** of the flux the §5.1 table names. That ratio is `1 − 0.339`, sitting inside `NP-THERM-CFD-001` §4's η_wp band of 0.30–0.45, so the natural reading is that **η_wp was applied a second time to a §4 figure that is already `q_heat = P_elec(1 − η_wp)`**. **It is non-conservative and it is load-bearing:** driven at the labelled flux, the T1-std 25 °C row gives face 32.8 °C rather than 30.7 °C, cutting the margin from **11.3 K to 9.2 K** — and 11.3 K is the exact figure `NP-PWR-BUDGET-001` §3.2 divides to bracket 4–8 tiles. Raised, not corrected: a downstream study may not silently rewrite this document's inputs. Tracked as `OI-N1-01` | Thermal |
| OI-R1-05 | Register outcomes under change control. **DONE 2026-07-22 (DHF Rev 22):** Path B1 + SR-FAN-03/04 constants → NP-REQ-FANHEALTH-001 §4a; FMEA-G07-01 closure path → NP-FMEA-GEOM-001 (feeder); THERM-1a gate → NP-HELMET-GEOM-001 §8; BN-boss export adopted → completed-decisions.md. **Remaining:** NP-RISK-001 `.docx` RISK-2x line (register Rev 2→C, owner-signed); BN-boss export → NP-DT-001 design input; accept SR-FAN-01…06 into NP-SW-001. | Quality |

## 8. Assumptions / limits

1D + rough FD only; SOR to 1e-6 (not mesh-independence-verified); literature tissue + PDMS properties;
single periodic cell (ignores rim/ear/Boa edge effects — conservative for a mid-vault hot zone); steady
perfusion at the low (pessimistic) bound; perfect-sink upper bound in §5 (a real heatsink is worse, so
the §5 FAIL is robust). Directional conclusions and the multi-°C margins are decisive; absolute
constants are provisional pending OI-R1-01/02/04.

## 9. Cross-references

NP-THERM-CFD-001 (BC spec, matrix, decision logic) · NP-THERM-CFD-C2-001 (C2 run card) ·
NP-REQ-FANHEALTH-001 (SR-FAN) · NP-FMEA-GEOM-001 (FMEA-G07-01) · NP-THERM-BEZEL-001 (THERM-1) ·
NP-ENV-OPRANGE-001 / NP-FW-POE-001 (ambient/duty envelope) · NP-HELMET-GEOM-001 §2/§8 (stack, gates) ·
CLAUDE.md §4.2 (42/62 °C interlocks), §4.3 (EMF stack), §4.5 (power/heat).
