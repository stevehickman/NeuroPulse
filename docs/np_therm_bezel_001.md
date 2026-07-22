# NP-THERM-BEZEL-001 Rev A — THERM-1 / BEZEL-1 Coupling Analysis

**Program:** NeurOne chassis / mechanical stack
**Status:** ANALYSIS — first-order coupled thermal/optical model, not a measurement.
Closes the *scaling and trade direction* for THERM-1 and BEZEL-1; absolute scalp/junction
temperatures still require the CFD + bench work named in §7.
**Parent:** `docs/np_helmet_geom_001.md` (§2 stack-up, §5 impact strategy, §8 gates)
**Sources:** `CLAUDE.md` §4.2 (42 °C scalp / 62 °C junction), §4.5 (power), `docs/np_fmea_001.md`
(FMEA-M04: 62 °C throttle 50 %, 65 °C cutoff, PTC ~70 °C), `docs/pbm_neuro_protocols.md`
(0.02–0.31 W/cm² scalp, ~2 % to cortex), `docs/np_opt_psf_001.md` (~51 % diffuse reflectance),
`docs/np_bib_1064_001.md` (100 mW/cm² average at 25 % duty).
**Date:** 2026-07-21

---

## 1. The coupling in one sentence

The sacrificial bezel height `h_b` sets the **module-face-to-scalp air gap `s`**, and that one
geometric parameter simultaneously moves (a) PBM optical dose, (b) scalp temperature, (c) module
junction/throttle margin, and (d) EEG pod-travel budget — so it cannot be chosen on impact grounds
alone. This note models all four against `h_b` and lands a value.

**Type-dependent from the start.** The gap only exists for **rigid optical modules** (T1-A/C, T2-D),
which the bezel holds off the scalp. **Electrode modules (T1-B) sit on ±12 mm spring pods**; the
electrode protrudes *past* the bezel plane into skin contact, so for them `s = 0` by design. This
split turns out to be exactly what the thermal analysis wants (§4.4), because electrode modules are
low-power and optical modules are the heat sources.

---

## 2. Effect on PBM dose — negligible, and metered regardless

Two mechanisms, both small at `h_b` ≤ ~1.5 mm:

**Geometric spreading.** A module is a broad, near-uniform 40 mm emitter (600 LEDs, ±15–25 % field).
On-axis irradiance from a uniform disk of radius `a` = 20 mm at axial gap `s` falls as
`a²/(a²+s²)`. At `s` = 0.6 mm that is `400/400.36` = **0.9991 → 0.09 % loss**. At `s` = 2 mm it is
`400/404` = **1.0 % loss**. Geometric loss is in the noise until the gap is many millimetres.

**Fresnel reflection is independent of gap thickness.** Transcranial scalp PBM is delivered across
hair, so the baseline is already window → air → scalp — the optical PSF model assumes exactly this
(2.8 % specular + ~48.8 % diffuse reflectance at n = 1.4). The two interfaces (window/air, air/scalp)
exist whether the gap is 0.1 mm or 2 mm; air absorbs nothing across sub-mm paths. So the bezel adds
**no** new interface loss. (Only if the baseline were index-matched *contact* would a gap cost the
~5.5 % two-interface Fresnel penalty — and contact is not achievable through hair.)

**And it is closed-loop.** Per-tile PD2 backscatter metering + the J/cm² dose loop compensate any
residual coupling change within the 25 % duty / 42 °C limits. The dose *claim* is metered, not
open-loop, so a sub-mm standoff shift is corrected in firmware, not lost.

> **Optical verdict: PBM dose is insensitive to the bezel over 0–1.5 mm (<1 % geometric, zero added
> Fresnel), and metered besides.** Optics does not constrain `h_b`.

---

## 3. Effect on EEG contact — negligible

Electrode pods travel **±12 mm** at 80–120 g. The bezel offsets the pod's neutral point by `h_b`,
so it consumes `h_b/12` = **5 %** of one-sided travel at 0.6 mm (8 % at 1.0 mm). The electrode still
seats with full preload. Constraint: the bezel must **not** cap the electrode — the pod protrudes
through its own aperture, bezel land around it (§4.4). No dose/contact penalty.

---

## 4. Effect on temperature — the binding coupling

### 4.1 The decoupling insight

The scalp cap (42 °C) and the junction throttle (62 °C) are **20 °C apart on purpose**. That gap is
only survivable if the module face is **thermally decoupled** from the scalp — otherwise a junction
run to 62 °C for dose would drag the scalp up with it. **The air gap is that decoupler.** So the
bezel is not merely impact protection (§5 of the parent brief); it is the element that lets a module
run at therapeutic junction temperature without heating the patient. This reframes THERM-1: the job
is not "keep the junction cool," it is **"keep the scalp cool while letting the junction run warm,"**
and the gap + the outward sink are the two tools.

### 4.2 First-order resistance network (per unit scalp area)

Air conductivity `k_air` = 0.026 W/m·K, so a gap `s` has areal resistance `R_gap = s/k_air`:

| `h_b` = `s` | `R_gap` (m²K/W) |
|---|---|
| 0.3 mm | 0.0115 |
| **0.6 mm** | **0.0231** |
| 1.0 mm | 0.0385 |
| 2.0 mm | 0.0769 |

Heat reaching the scalp = conduction across the gap + optical absorption in scalp tissue. The scalp
sheds heat to core by **perfusion**, the dominant sink; modelling scalp→core as `R_sc` ≈ 0.035 m²K/W
(perfused skin, first-order) with deep tissue at 37 °C, the scalp surface holds ≤ 42 °C only if total
scalp-bound flux `q_scalp` ≤ (42−37)/0.035 ≈ **143 W/m² = 14 mW/cm²**.

Optical absorption in the 6 mm scalp at a high-power zone (100 mW/cm² average, ~50 % enters, ~10–20 %
of that deposits in scalp) ≈ **5 mW/cm²**. That leaves a **conductive budget of ≈ 9 mW/cm²** from the
module face across the gap.

### 4.3 What that budget implies for the module-face temperature

Permitted face-to-scalp rise `ΔT = q_cond · R_gap` at `q_cond` = 9 mW/cm² (= 90 W/m²):

| `h_b` | `R_gap` | Max face-above-scalp ΔT | **Face-temp ceiling** (scalp 42 °C) |
|---|---|---|---|
| 0.3 mm | 0.0115 | 1.0 K | **~43 °C** |
| **0.6 mm** | 0.0231 | **2.1 K** | **~44 °C** |
| 1.0 mm | 0.0385 | 3.5 K | **~45.5 °C** |
| 2.0 mm | 0.0769 | 6.9 K | **~49 °C** |

**Reading:** at 0.6 mm the module *face* must be held ≤ ~44 °C for the scalp to stay ≤ 42 °C — even
though the *junction* is allowed to reach 62 °C. The 18 °C face-to-junction drop is the job of the
**outward** sink (junction → BN-filled boss → socket → inter-bowl → shell → fan/vents). **This is the
real THERM-1 requirement, and it is a face-temperature spec, not a junction spec.**

### 4.4 Why the outward sink must carry ~all module heat (and the scalp none)

The scalp is a patient, not a heat sink: we want `q_scalp` → 0, not merely ≤ budget. Making the gap
*larger* pushes more heat outward and less into the scalp — protective — but only works if the
outward path can accept it. Natural convection off the shell (`h` ≈ 5–10 W/m²K → `R_conv` ≈
0.10–0.20 m²K/W) is **worse** than `R_gap`, so *without airflow the scalp is the lower-resistance
sink* and heat prefers the patient. **Forced convection is therefore load-bearing:** the existing hub
fan (SHDR logs fan RPM) at `h` ≈ 25–100 W/m²K gives `R_conv` ≈ 0.01–0.04 m²K/W, making the outward
path competitive with or better than the gap. THERM-1 passes **iff** the vented/fan-assisted outward
path holds the face ≤ the §4.3 ceiling at peak duty.

The type split falls out for free: **electrode modules (contact, `s`=0) are the low-power ones**
(passive EEG; tES ≤ 4 mA; reduced-LED PBM), so their lack of a decoupling gap is harmless — little
heat to reject. **Optical modules (the heat sources) get the gap.** No conflict.

### 4.5 Bezel-height recommendation

Optics is indifferent to `h_b` (§2); EEG costs only travel (§3); comfort/fit and the impact function
(parent §5) want it small; **thermal margin wants it larger.** The knee is around **1.0 mm**: it
raises the face ceiling to ~45.5 °C (a ~1.5 K easier cooling target than 0.6 mm) at **~0.2 %** optical
loss and 8 % pod travel — both negligible — while staying low enough not to feel like a point load or
open a fit gap. **Recommend raising the bezel from 0.6 mm to 1.0 mm** on optical modules, keeping the
electrode-pod aperture bezel-free. If comfort testing (parent BEZEL-1 note) rejects 1.0 mm, 0.6 mm is
acceptable but tightens the face ceiling to ~44 °C, i.e. demands a stronger outward sink.

---

## 5. Resolution

- **BEZEL-1 → PASS (with a spec change).** The bezel does not degrade PBM dose (<1 % geometric, no
  added Fresnel, closed-loop metered) or EEG contact (5–8 % of pod travel). Recommended value **1.0 mm**
  on optical modules; **bezel-free electrode-pod aperture**. Its real function is dual: impact
  protection **and** thermal decoupling of scalp from module face.
- **THERM-1 → re-specified, then open on bench.** The acceptance criterion is sharpened from "scalp
  ≤ 42 °C" to a **module-face-temperature ceiling** (~44 °C at `h_b`=0.6 mm, ~45.5 °C at 1.0 mm) that
  the **forced-convection outward path** must hold at T1-peak and T2-peak duty, treating the scalp as
  a near-adiabatic boundary. The junction may run to its 62 °C throttle independently.
- **Design philosophy locked by this analysis:** *scalp is not a heat sink.* Size the gap so
  scalp-bound conduction is negligible; design the fan/vent/BN-boss path to carry essentially all
  module dissipation; let the air gap keep the 42 °C and 62 °C limits independent.

---

## 6. Sensitivity / honesty flags

- `R_sc` (scalp→core perfusion) is the weakest input; it moves the 14 mW/cm² scalp budget nearly
  proportionally. Bench perfusion is subject-variable — treat the face ceilings as central estimates
  with a real ±30 % spread, and design the sink to the pessimistic end.
- The 5 mW/cm² optical-in-scalp term rests on the ~51 % reflectance figure, itself a model
  (np_opt_psf_001 §4.2, unmeasured on NeurOne hardware).
- Natural-vs-forced convection coefficients are textbook ranges; the actual `h` depends on the vent
  geometry that THERM-1's CFD must resolve.
- Plane-parallel, steady-state. Real sessions are duty-cycled and transient; the supercap buffers
  electrical transients but the thermal mass of the module smooths face-temperature excursions —
  transient peaks may undershoot these steady-state ceilings (favourable), but that needs the CFD.

## 7. Open sub-gates (feed back into parent §8)

| Gate | What it must show | Owner |
|------|-------------------|-------|
| THERM-1a | CFD: face ≤ ceiling (§4.3) at T1-peak (45–50 W) and T2-peak (70–74 W) with the fan/vent + BN-boss geometry | ME + Thermal |
| THERM-1b | Bench: measured scalp-phantom surface ≤ 42 °C under a worst-case sustained high-power zone | ME |
| THERM-1c | Confirm `R_sc` perfusion assumption against a perfused scalp phantom (or literature at wavelength) | Thermal |
| BEZEL-1a | Comfort/fit test 0.6 vs 1.0 mm bezel; confirm no point-load discomfort or fit-gap over 52–62 cm | ME + Human factors |
| BEZEL-1b | Confirm electrode-pod aperture stays bezel-free and seats at 80–120 g with the offset | ME |

## 8. Cross-references

np_helmet_geom_001 §2/§5/§8 · CLAUDE.md §4.2/§4.5 · np_fmea_001 (FMEA-M04) ·
pbm_neuro_protocols.md §3 · np_opt_psf_001 §4.2 · np_bib_1064_001.
