# NP-THERM-CFD-001 — THERM-1a Conjugate-Heat CFD Boundary Conditions

**Program:** NeurOne chassis / thermal
**Status:** DRAFT analysis spec — defines the boundary conditions, property set, case matrix, and
decision logic for the THERM-1a conjugate-heat / bioheat simulation. Closes the analysis half of
THERM-1 and supplies the constants that NP-REQ-FANHEALTH-001 (SR-FAN-03/04) is parameterized on.
**Parent gates:** THERM-1a (np_helmet_geom_001 §8), OI-FAN-01 (np_req_fanhealth_001 §8).
**Sources:** np_therm_bezel_001 (first-order model + the values this CFD must reproduce/replace),
np_helmet_geom_001 §2 (radial stack-up), np_opt_psf_001 §2.1 (tissue optical/thermal layers),
CLAUDE.md §4.2 (42 °C / 62 °C), §4.5 (power configs), pbm_neuro_protocols.md (irradiance).
**Date:** 2026-07-21

---

## 1. Objective — what this simulation must decide

1. **Path A admissibility.** Does the existing SW01-M04 junction throttle (62 °C) conservatively bound
   the **scalp-facing surface ≤ 42 °C** under the worst credible fault — fan fully stopped, ambient
   43.3 °C (110 °F)? If **yes**, NP-REQ-FANHEALTH-001 Path A is admissible (no new interlock). If
   **no** (expected — §1 of that doc), Path B is required and this CFD sizes it.
2. **Natural-convection-safe duty ceiling** per power config = the PBM duty at which the **fan-off,
   43 °C-ambient steady-state face temperature = 42 °C**. → SR-FAN-03 config constants.
3. **Face thermal time constant τ_face** and the time-to-42 °C after a fan-stall step from full-power
   steady state. → SR-FAN-04 response-time bound (`T_resp < k·τ_face`).
4. **Heat-split fraction** (scalp-bound vs outward) vs bezel height and convection state — validates
   np_therm_bezel_001 §4 and confirms the "scalp-as-adiabatic" design intent.

---

## 2. Domain — representative unit cell

Model a **single-hex column** with symmetry side walls (the vault is locally periodic; a full-helmet
model is not needed for the face/junction/scalp questions and is deferred). Axisymmetric-equivalent or
a 40 mm-hex prism; either is acceptable if the lateral BC is adiabatic-symmetry.

Radial layer stack (from np_helmet_geom_001 §2), scalp → exterior:

| # | Layer | Thickness | Notes |
|---|-------|-----------|-------|
| tissue | scalp / skull / CSF / gray | 6 / 7 / 2 / ≥20 mm | Pennes bioheat; gray truncated at ≥20 mm with 37 °C deep BC |
| **gap** | air (bezel standoff) | **sweep 0.6 / 1.0 mm** | conduction-dominated (Ra≪1708 at ≤1 mm — verify, no convection cell) |
| L0 | module optical window (PDMS) | 1.0 mm | over the LED plane |
| src | **LED plane (heat source)** | — | §4 |
| L1 | module body + socket (glass-filled PBT) + BN boss | 12–16 + 2.5 mm | BN boss at the heat pickup |
| gap2 | inter-bowl air + clamp | 5–7 mm | |
| L2 | absorber foam / Pd-poly / mu-metal | 3 / 0.1 / 0.2 mm | |
| L3 | CFRP shell | 2.5 mm | outer convective face |

---

## 3. Material thermal properties

Populate and **cite at wavelength/temperature**; values below are planning mid-ranges — flag each as
literature vs measured. `k` W/m·K, `ρ` kg/m³, `cp` J/kg·K.

| Material | k | ρ | cp | Source status |
|----------|----|----|----|---------------|
| Scalp | 0.34 | 1100 | 3500 | literature (Bashkatov/IT'IS) — **sweep** |
| Skull | 0.32 | 1900 | 1700 | literature |
| CSF | 0.57 | 1000 | 4000 | literature |
| Gray matter | 0.51 | 1040 | 3600 | literature |
| Air (gap) | 0.026 | 1.2 | 1005 | standard |
| PDMS window | 0.20 | 970 | 1460 | literature — **measure on hardware** |
| Glass-filled PBT | 0.30–0.7 | 1550 | 1300 | grade-dependent — **from datasheet of selected grade** |
| BN-filled boss | 1.0–3.0 | 1900 | 800 | grade-dependent |
| Absorber foam | 0.05–0.10 | 300 | 1000 | vendor |
| Pd-polyester | 0.15 | 1400 | 1100 | estimate |
| Mu-metal | 30 | 8700 | 460 | standard |
| CFRP shell | through-thick 0.8; in-plane 5–10 | 1600 | 900 | **anisotropic — use tensor** |
| Blood (perfusion) | — | 1050 | 3600 | Pennes |

---

## 4. Heat-source model

Heat at the LED plane per zone: **q_heat = P_elec · (1 − η_wp)**, with wall-plug efficiency
**η_wp = 0.30–0.45** (660/808/1064 nm LED) and **0.15–0.25** (1170 nm laser diode + TEC — include TEC
hot-side load). Distribute total device power per the zone power map; run the **worst-case (highest
flux) zone** as the design case.

Config electrical envelopes (CLAUDE.md §4.5), less ~5–10 W hub/processor that does **not** sit at the
scalp:

| Config | Device draw | At-module (est.) | Worst-zone LED-plane heat flux (est.) |
|--------|------------|------------------|----------------------------------------|
| T1 standard | 17–20 W | ~12–15 W | ~0.10–0.15 W/cm² (1064 zone) |
| T1 peak | 45–50 W | ~38–42 W | ~0.18–0.20 W/cm² |
| T2 peak | 70–74 W | ~60–66 W | ~0.25–0.35 W/cm² (1170 laser zone) |

Apply the firmware duty cap (≤25 % pulsed / CW limits) as a **time-averaged** flux for steady-state
cases; use the true pulse train only if a transient junction check is needed (the face τ is far longer
than the pulse period, so time-averaging is valid for the face question).

---

## 5. Boundary conditions

**5.1 Scalp side — Pennes bioheat (the dominant sink, and the weakest input).**
- Deep-tissue Dirichlet: **T = 37 °C** at the gray-matter truncation depth.
- Perfusion source term `q_perf = ρ_b·cp_b·w_b·(T_art − T)`, `T_art = 37 °C`, applied in scalp (and
  skull/brain). **Sweep scalp perfusion `w_b` = 0.0005 / 0.002 / 0.008 s⁻¹** (rest → hyperemic). This
  is np_therm_bezel_001 §6's flagged ±30 %-plus axis; the safe-ceiling result must hold at the **low**
  perfusion end.
- Optional optical co-heating: deposit ~5 mW/cm² in the 6 mm scalp (np_opt_psf_001 §4.2 reflectance)
  as a volumetric term; keep separable to see its share.

**5.2 Outer shell (L3) — convection to ambient. Two mandatory cases:**
- **Fan nominal:** forced convection `h = 25–100 W/m²·K` (sweep; set from the vent CFD or a measured
  hub-airflow correlation).
- **Fan OFF (single-fault):** natural convection `h = 5–10 W/m²·K` + radiation (ε ≈ 0.9 CFRP) to a
  43.3 °C surround.

**5.3 Ambient:** two levels — **nominal 25 °C** and **worst-case 43.3 °C (110 °F)** per the
CLAUDE.md environmental envelope. The safety decisions (§1.1–1.2) are taken at 43.3 °C.

**5.4 Lateral:** adiabatic symmetry (periodic vault).

**5.5 Contact/interface:** include realistic interface conductances (window-to-LED, boss-to-socket,
socket-to-shell); a perfect-contact idealization is optimistic — flag if used.

---

## 6. Case matrix

Steady-state unless noted. Bezel {0.6, 1.0 mm} × power {T1-std, T1-peak, T2-peak} × convection {fan
nominal, fan OFF} × ambient {25, 43.3 °C} × perfusion {low, mid, high}. Prune to the **decision-
critical subset** first:

| # | Purpose | Bezel | Power | Conv | Ambient | Perfusion |
|---|---------|-------|-------|------|---------|-----------|
| C1 | Nominal baseline | 1.0 | T1-std | nominal | 25 | mid |
| C2 | **Path A test** (worst fault) | 0.6 | T2-peak | **OFF** | 43.3 | **low** |
| C3 | **Safe-ceiling sizing** | 1.0 | sweep duty | **OFF** | 43.3 | low |
| C4 | Peak, fan working | 1.0 | T2-peak | nominal | 43.3 | mid |
| C5 | Bezel-sensitivity | 0.6 vs 1.0 | T1-peak | OFF | 43.3 | low |
| C6 | **τ_face transient** | 1.0 | T1-peak→same | step OFF | 43.3 | low |

---

## 7. Transient protocol for τ_face (case C6)

Initialize at the fan-nominal full-power steady state; at t = 0 **step convection to fan-OFF**; record
face temperature T_face(t). Extract **τ_face** (63 % rise) and **t₄₂** (time for T_face to reach
42 °C). SR-FAN-04 sets `T_resp < t₄₂` with margin (target ≤ ½·t₄₂). If Path B1 (direct scalp NTC) is
chosen, also report the sensor-node lag vs the true peak-face node.

---

## 8. Required outputs (per case)

1. Scalp-facing surface (face) peak temperature.
2. LED junction temperature (to test Path A: is face ≤ 42 whenever junction ≤ 62?).
3. Heat-split fraction: scalp-bound flux ÷ total module heat.
4. (C3) The duty at which fan-OFF face = 42 °C → **natural-convection-safe ceiling** per config.
5. (C6) τ_face and t₄₂.

---

## 9. Decision logic (feeds NP-REQ-FANHEALTH-001)

- **Path A admissible ⇔** in C2 (and the low-perfusion sweep), `T_face ≤ 42 °C` for all states with
  `T_junction ≤ 62 °C`. Then the junction throttle suffices; add only SR-FAN-05 monitoring.
- **Else Path B:** publish the C3 duty ceilings as SR-FAN-03 constants and t₄₂/τ_face as the SR-FAN-04
  bound. Recommend **Path B1** (scalp NTC) if C6 shows the junction node lags the face node under
  fan-off (i.e., junction is not a timely proxy).
- **Cross-check:** the fan-nominal peak case (C4) must also pass ≤ 42 °C — if it doesn't, the base
  thermal design (BN boss / vent) is under-sized independent of the fault, a separate finding.

---

## 10. Validation

- **Mesh independence** on face and junction nodes (< 0.2 °C between refinements).
- **Sanity vs first-order model:** np_therm_bezel_001 predicts a ~44 °C face ceiling at 0.6 mm and
  ~45.5 °C at 1.0 mm under its assumptions; the CFD C5 pair should land in that neighborhood or the
  discrepancy must be explained (curvature, contact resistance, anisotropic CFRP).
- **Bench correlation:** reconcile against THERM-1b (scalp-phantom fan-stall) before any result is
  treated as verification-grade.

## 11. Assumptions / limits

Literature tissue + PDMS properties (measure on hardware — np_opt_psf_001 §6 flags the same);
single-cell periodicity ignores rim/ear/Boa edge effects (conservative for a mid-vault hot zone);
steady perfusion (real perfusion rises with heating — omitting that is conservative); TEC hot-side
load for 1170 nm must be included or T2-peak is under-stated.

## 12. Cross-references

np_therm_bezel_001 (THERM-1) · np_req_fanhealth_001 (SR-FAN-03/04, OI-FAN-01) · np_helmet_geom_001 §2/§8 ·
np_fmea_geom_001 (FMEA-G07-01) · np_opt_psf_001 §2.1 · CLAUDE.md §4.2/§4.5.
