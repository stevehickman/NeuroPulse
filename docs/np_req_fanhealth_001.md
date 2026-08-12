# NP-REQ-FANHEALTH-001 — Forced-Convection (Fan) Health Thermal Interlock Requirement

**Program:** NeurOne
**Status:** DRAFT for change-control review — proposed addition to **NP-SW-001 §6.2** (SW-01 Class C
safety requirements) with allocations to SW01-M04, a SW-02 telemetry element, and SW03-M05.
Drafted, not merged: NP-SW-001 is an ACTIVE controlled document; §7 lists the exact insertions for
the Quality Lead to accept under change control.
**Path selection RESOLVED (2026-07-22, §4a):** THERM-1a first-pass analysis (NP-THERM-CFD-R1-001)
rejected Path A and selected **Path B1** (scalp-facing NTC at PD2); provisional SR-FAN-03/04 constants
published, pending verification-grade CFD + THERM-1b to baseline.
**Origin:** FMEA-G07-01 (NP-FMEA-GEOM-001) / OI-GEOM-FMEA-01, arising from the THERM-1 coupling
(NP-THERM-BEZEL-001 §4).
**References:** NP-SW-001 §5.1/§6.2/§7.2, NP-FMEA-001 §3.4 (SW01-M04), NP-FMEA-GEOM-001 §4 (G07),
NP-THERM-BEZEL-001, NP-RM-001 §4/§7, CLAUDE.md §4.2 (interlocks) / §4.5 (power) / §5.1 (SHDR fan RPM),
IEC 60601-1 (42 °C applied-part limit), IEC 62304 §7.1.
**Date:** 2026-07-21

---

## 1. Hazard being closed

The existing thermal interlock (**SW01-M04**, NP-FMEA-001 FMEA-M04) throttles PBM zone current on
**junction** NTC temperature: 50 % throttle at 62 °C, cutoff at 65 °C. THERM-1 (NP-THERM-BEZEL-001 §4)
established that patient scalp safety is a **scalp-facing surface (face) temperature** limit (42 °C,
IEC 60601-1), and that the split of module heat between "outward to ambient" and "inward to scalp"
depends on **forced convection** (the hub fan; RPM already logged to SHDR per CLAUDE.md §5.1).

**The gap (FMEA-G07-01):** on loss/degradation of forced convection, outward thermal resistance rises
and more heat is forced scalp-ward. With cooling reduced, the module face rides **up toward the
junction temperature** — but the junction NTC is *held at 62 °C by its own throttle* and does not cut
off until 65 °C, so it reports "safe/throttling" while the face (and scalp) approach unsafe
temperature. A fan failure is therefore a scalp-burn pathway (NP-RM-001 severity **S3**, moderate,
reversible) that the current junction-only interlock does **not** independently bound. Physics note:
because fan loss shifts heat *toward* the face, the junction-throttle case is the adverse one — the
existing interlock is least protective exactly when this fault occurs.

---

## 2. Requirement statements (proposed SR-FAN-01 … SR-FAN-06)

IDs follow the NP-SW-001 §6.2 safety-requirement style (statement + safety class + response bound +
verification). Class per IEC 62304 §4.3; response bounds are **thermal-class** (seconds), not the
ms-class of the electrical interlocks, and are justified in §3.

| ID | Requirement | Class |
|----|-------------|-------|
| **SR-FAN-01** | The device shall limit the **scalp-facing module surface temperature to ≤ 42 °C** (IEC 60601-1 applied-part limit) during normal operation **and under a single-fault loss or degradation of forced convection** (fan stall, fan failure, or vent occlusion). | **C** |
| **SR-FAN-02** | The safety function of SR-FAN-01 shall rest on a measurement that **bounds the face temperature independently of forced-convection state**, by one of the two approved paths in §4 (direct scalp-facing NTC, or fan-health-gated derate), selected by the THERM-1a CFD outcome and the hardware FMEA review. | **C** |
| **SR-FAN-03** | Under a face-temperature or forced-convection fault, SW-01 shall limit PBM zone duty to the **natural-convection-safe ceiling** — the duty at which steady-state face temperature is ≤ 42 °C **with the fan fully stopped** — the value(s) established by THERM-1a CFD per power configuration and stored as safety-MCU config constants. | **C** |
| **SR-FAN-04** | The transition from fault detection to the SR-FAN-03 safe state shall complete within **T_resp**, where T_resp is bounded by the module thermal time constant τ_face such that the face temperature **cannot exceed 42 °C at any point** during the transition (nominal T_resp ≤ 10 s; τ_face and the exact bound fixed by THERM-1a). | **C** |
| **SR-FAN-05** | SW-02 shall sample hub fan RPM, log it to SHDR (existing), compute a forced-convection headroom trend, and raise a **predictive-maintenance alert** (SW03-M05) *before* degradation reaches the SR-FAN-03 safety derate — an availability/comfort layer above, and independent of, the Class C safety function. | **B** |
| **SR-FAN-06** | The SR-FAN-01/03 decision shall be made in the Class C domain (SW-01) and shall **not depend on SW-02 correctness** (NP-SW-001 §7.2). Any hub-provided fan-health indication used as an input shall be **fail-safe**: absent, stale, or invalid → treated as "forced convection NOT confirmed" → SR-FAN-03 derate. | **C** |

---

## 3. Response-time rationale (why seconds, not milliseconds)

The electrical interlocks are ms-class because charge/current hazards are instantaneous. A thermal
hazard develops over the module's thermal time constant τ_face (mass × specific heat / conductance),
expected to be **tens of seconds**. Two design consequences:

1. **The derate ceiling is inherently safe**, so a slow response cannot overshoot: SR-FAN-03 caps duty
   at the level whose *steady-state* face temperature is ≤ 42 °C with the fan off. Even if detection
   lags, the system is settling toward a safe point, not an unsafe one.
2. **T_resp must still beat the rise** from the operating point to 42 °C at full power with the fan
   just failed. THERM-1a CFD yields τ_face and that rise time; SR-FAN-04's ≤ 10 s nominal is a
   placeholder to be replaced by `< k·τ_face` with margin. The direct-NTC path (§4, Path B1) makes
   this a non-issue by measuring the face temperature itself and throttling at the threshold, exactly
   as SW01-M04 already does for the junction.

---

## 4. Two approved implementation paths (gated by THERM-1a)

| Path | Description | When selected | Cost |
|------|-------------|---------------|------|
| **A — junction NTC proven sufficient** | Keep SW01-M04 unchanged. Requires THERM-1a CFD to **prove** the 62 °C junction throttle conservatively bounds face ≤ 42 °C **even at zero forced convection**. Add only SR-FAN-05 (Class B fan monitoring + maintenance alert). | Only if the CFD margin proves out — **physically unlikely** (fan loss drives face toward the throttled 62 °C junction, §1), so treat Path A as the exception, not the plan. **→ REJECTED by NP-THERM-CFD-R1-001 §2 (see §4a).** | Lowest — no new sensor/Class C change |
| **B1 — scalp-facing NTC (recommended)** | Co-locate an NTC on the **scalp-facing surface with the existing PD2** (already on that surface for backscatter dose metering, CLAUDE.md §3 PBM RISK-14). SW01-M04 reads face temperature directly and throttles/cuts on a face threshold with margin below 42 °C. Directly measures the safety parameter; robust to fan state, bezel variation, and hair. | **SELECTED (NP-THERM-CFD-R1-001 §4a).** | +1 NTC/zone BOM; SW01-M04 Class C change (adds one ADC channel + threshold, mirrors existing NTC handling incl. FMEA-M04-02 open-circuit rule) |
| **B2 — fan-health-gated derate** | SW-01 receives an independent fan-health indication (tach to the Safety MCU, or a hub-asserted fan-OK GPIO cross-checked against SHDR RPM) and enforces SR-FAN-03 when forced convection is not confirmed. Infers the hazard from airflow rather than measuring face temperature. | Complement to B1, or fallback if a scalp-side NTC is not mechanically feasible. | Fan tach routing to STM32G071; Class C logic |

**Recommendation:** **Path B1** — measuring the actual safety parameter (face temperature) is more
robust and defensible than inferring it from fan RPM, and it reuses the SW01-M04 NTC-handling pattern
already verified (including the open-circuit-as-fault rule, FMEA-M04-02). B2 may be added as defense
in depth. Path A is retained only so the CFD is allowed to discharge the requirement cheaply if it
genuinely can.

---

## 4a. THERM-1a outcome — path selection RESOLVED (NP-THERM-CFD-R1-001, 2026-07-22)

First-pass THERM-1a analysis (1D closed-form + rough non-verification-grade axisymmetric FD;
**NP-THERM-CFD-R1-001**) has run cases C2/C3/C6. Outcome:

- **Path A REJECTED.** At the worst fault (fan off, 43.3 °C ambient, low perfusion) the 62 °C junction
  throttle leaves the scalp-facing face ≈ 60 °C and scalp skin ≈ 52–55 °C — **14–21 °C over the 42 °C
  limit** (1D and 2D agree, fails even at the 62 °C setpoint). The junction throttle regulates the wrong
  node; it does **not** bound the face. This holds with the BN-boss conductive-export design too (fault
  face 60.3 °C with or without the via — the via gives zero benefit once the external sink is lost).
- **Path B1 SELECTED** — direct scalp-facing NTC co-located with PD2, per the §4 recommendation. Path B2
  (fan-health-gated derate) retained as optional defense-in-depth.
- **SR-FAN-03 constant (natural-convection-safe ceiling):** fan-off safe LED-plane flux ≈ **45 W/m²
  (4.5 mW/cm²) at 43.3 °C ambient**, ≈ 90 W/m² (9 mW/cm²) at 25 °C — single-digit % of any config's rated
  flux. Operationally **halt/trickle PBM on fan loss**, ceiling → 0 as ambient → 42 °C. Store per-config
  as safety-MCU constants, **marked TBD-per-datasheet + TBD-per-THERM-1b**.
- **SR-FAN-04 constant (response bound):** τ_face ≈ **35–45 min** (tens of minutes; diffusion cross-check
  ≈ 22 min); t₄₂ after fan loss ≈ **minutes**. The ≤ 10 s nominal T_resp is met with a 2+ order-of-
  magnitude margin — the **steady ceiling (SR-FAN-03), not response speed, is the binding parameter.**
- **Envelope backstop:** the face ≤ 42 °C ceiling at extreme ambient is fundamentally ambient-bounded
  (no passive path rejects below ambient, and 43.3 °C > 42 °C), so it is owned by the firmware
  ambient/duty gate (NP-ENV-OPRANGE-001 / NP-FW-POE-001), which SR-FAN complements, not replaces.

**Provisional status:** the constants above come from 1D + non-verification-grade FD. The
verification-grade 2D CFD (NP-THERM-CFD-001 §9 step 2) and THERM-1b bench (OI-FAN-03) must confirm before
they are baselined as verified. Path **selection** is firm: Path A cannot be rescued.

---

## 5. Allocation to software items

| Software item / module | Added responsibility |
|---|---|
| **SW01-M04** (Thermal interlock, Class C) | Read the scalp-facing NTC per zone (Path B1) and/or the fail-safe fan-health input (B2/SR-FAN-06); enforce SR-FAN-01/03/04; apply the existing open-circuit/out-of-range = fault rule (FMEA-M04-02) to the new channel(s). |
| **SW-02 hub telemetry** (e.g., SW02-M11 session log / SW02-M15) Class B | Sample fan RPM, log to SHDR, compute headroom trend, expose fan-health to SW-01 as a *fail-safe advisory only* (never the sole safety basis, per SR-FAN-06). |
| **SW03-M05** (Predictive maintenance alerts) Class B | Surface the SR-FAN-05 maintenance alert (fan/vent service) before the safety derate engages; measurement-triggered per the CLAUDE.md §5.2 reminder rules. |

---

## 6. Verification

| Level | Test | Satisfies |
|-------|------|-----------|
| Unit (SW01-M04) | Face-NTC threshold throttle/cutoff across the range; open-circuit/out-of-range → zone disable; fan-health-invalid → derate (fail-safe) | SR-FAN-02/03/06 |
| Unit (SW01-M04) | T_resp: injected face-temp/fan-loss transient → derate asserted within bound | SR-FAN-04 |
| Integration | Simulated fan stall at T1-peak and T2-peak session → SW-01 derates to natural-convection ceiling; SW-02 raises alert | SR-FAN-01/03/05 |
| System / bench (**THERM-1b**) | Thermal chamber (up to 43 °C ambient) + scalp phantom; stall the fan mid-session at worst-case high-power zone; **phantom face ≤ 42 °C throughout** | SR-FAN-01 (design verification) |
| Analysis (**THERM-1a**) | CFD establishes τ_face, the natural-convection-safe duty ceiling per config, and whether Path A is admissible | SR-FAN-03/04, path selection |

---

## 7. Proposed NP-SW-001 insertions (for change control)

1. **§6.2** — append SR-FAN-01 … SR-FAN-06 to "Currently documented SW-01 safety requirements with
   response times," adding a new bullet: *"Loss of forced convection / scalp-facing surface over-temp
   → PBM duty limited to natural-convection-safe ceiling: **≤ T_resp (thermal-class, ~10 s; < k·τ_face
   per THERM-1a)**, face temperature ≤ 42 °C maintained (NP-REQ-FANHEALTH-001)."*
2. **§5.1** — extend the SW01-M04 row safety-function text: *"…throttles current at 62 °C junction;
   additionally bounds scalp-facing surface ≤ 42 °C under loss of forced convection (NP-REQ-FANHEALTH-001)."*
3. **§5.2** — note the SW-02 fan-RPM telemetry + fail-safe fan-health advisory element (SR-FAN-05/06)
   against the relevant hub-control/telemetry module.
4. **§11** — under §7.1/§7.2, reference NP-FMEA-GEOM-001 as the hardware-side hazard source that this
   requirement closes.

## 8. Open items

| ID | Description | Owner | Blocking |
|----|-------------|-------|----------|
| OI-FAN-01 | THERM-1a analysis: τ_face, natural-convection-safe duty ceiling per config, Path A admissibility. **First-pass DONE → NP-THERM-CFD-R1-001** (1D + rough FD): Path A REJECTED, **Path B1 selected**, provisional SR-FAN-03/04 constants published (§4a). Residual: verification-grade 2D CFD (NP-THERM-CFD-001 §9 step 2) to baseline the constants. | Thermal | Constants baselining (path selection now firm) |
| OI-FAN-02 | Mechanical feasibility of a scalp-facing NTC co-located with PD2 (Path B1); BOM delta. | ME | Path B1 commit |
| OI-FAN-03 | THERM-1b scalp-phantom fan-stall bench (design verification of SR-FAN-01). | ME + Thermal | Design verification |
| OI-FAN-04 | Accept SR-FAN-01…06 into NP-SW-001 under change control; **RISK ID assigned — FMEA-G07-01 is RISK-26**, carried in **NP-RISK-004** (NP-RISK-001 superseded 2026-08-11). | Quality | QMS baseline |

## 9. Cross-references

NP-SW-001 · NP-FMEA-001 (SW01-M04 / FMEA-M04) · NP-FMEA-GEOM-001 (FMEA-G07-01, OI-GEOM-FMEA-01) ·
NP-THERM-BEZEL-001 (THERM-1) · NP-HELMET-GEOM-001 · NP-RM-001 / NP-RISK-004 (RISK-26; index NP-RISK-002) · IEC 60601-1 · IEC 62304 §7.1.
