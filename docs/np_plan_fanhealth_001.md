# NP-PLAN-FANHEALTH-001 — Fan-Health Interlock Sequenced Work Plan

**Program:** NeurOne chassis / thermal-safety
**Status:** DRAFT plan — sequences the four open items (OI-FAN-01…04) from NP-REQ-FANHEALTH-001 §8 to
close **FMEA-G07-01** and land **SR-FAN-01…06** with verified constants.
**References:** NP-REQ-FANHEALTH-001 (SR-FAN-01…06), NP-THERM-CFD-001 (THERM-1a BCs), NP-FMEA-GEOM-001
(FMEA-G07-01), NP-THERM-BEZEL-001 (THERM-1), NP-SW-001 §6.2, NP-RM-001 / NP-RISK-004 (RISK-26).
**Date:** 2026-07-21

---

## 1. Owners

| Role | Scope here |
|------|-----------|
| **Thermal** | CFD (NP-THERM-CFD-001), phantom-bench thermal design, perfusion sweep |
| **ME** | Scalp-NTC-at-PD2 feasibility, module/FPC change, phantom-bench fixture |
| **FW** | SW01-M04 Class C change, unit/integration tests, requirement text |
| **EE** | NTC ADC channel / fan-tach routing (if Path B2) |
| **Quality** | NP-RISK-004 hazard entry (RISK-26), NP-SW-001 change control, traceability |

## 2. Dependency shape

```
Phase 0 (parallel, start now)
  OI-FAN-04a  Quality  ── baseline hazard + provisional requirement
  OI-FAN-01   Thermal  ── run CFD  (C2 first = Path-A go/no-go)  ─┐
  OI-FAN-02   ME       ── scalp-NTC feasibility + BOM  ──────────┤
                                                                 ▼
Phase 1                                        ►  DECISION GATE (path A / B1 / B2)
                                                                 │
                                               (Path B) IMPL: SW01-M04 change [FW]
                                                                 │
Phase 2   OI-FAN-03a model-correlation coupon ─┘   OI-FAN-03b full verification bench
                                                                 │
Phase 3                                        ►  OI-FAN-04b finalize constants + close
```

**Critical path:** OI-FAN-01 → DECISION GATE → IMPL (if Path B) → OI-FAN-03b → OI-FAN-04b.
OI-FAN-02 and OI-FAN-04a run off the critical path, in parallel from day 0.

## 3. Sequenced tasks

| Phase | Task | Owner | Depends on | Exit criteria | Est. |
|-------|------|-------|-----------|---------------|------|
| **0** | **OI-FAN-04a** — **DONE: FMEA-G07-01 is RISK-26, carried in NP-RISK-004** (NP-RISK-001 superseded 2026-08-11); accept SR-FAN-01…06 into NP-SW-001 §6.2 **provisionally** (constants marked TBD-per-THERM-1a) | Quality | — | Hazard under QMS control; requirement baselined | 1–2 wk |
| **0** | **OI-FAN-01a** — Run CFD **case C2** first (bezel 0.6, T2-peak, fan OFF, 43.3 °C, low perfusion) — the Path-A go/no-go | Thermal | NP-THERM-CFD-001 | Face-vs-junction result at worst fault → Path A admissible? y/n | 1–2 wk |
| **0** | **OI-FAN-01b** — Full case matrix C1–C6 + perfusion sweep + τ_face transient | Thermal | C2 | τ_face, natural-convection-safe duty ceilings, heat-split | +2–3 wk |
| **0** | **OI-FAN-02** — Scalp-NTC-at-PD2 feasibility + BOM delta (parallel de-risk so Path B1 is ready) | ME (+EE) | — | Go/no-go on the sensor + $/unit | 2–3 wk |
| **1** | **DECISION GATE** — select Path A / B1 / B2; publish SR-FAN-03/04 constants | Thermal + FW + Quality | OI-FAN-01, -02 | Path chosen, constants issued | 1 wk |
| **1** | **IMPL** *(bridge, not an OI — only if Path B)* — SW01-M04 Class C change: read face NTC and/or fail-safe fan-health input; derate to ceiling; open-circuit=fault (per FMEA-M04-02); unit + integration tests | FW (+EE) | DECISION GATE | Code + 100 % branch cov + integration pass | 8–12 wk |
| **2** | **OI-FAN-03a** — Model-correlation coupon: fan-stall on a scalp phantom, no interlock, measure face rise vs CFD | Thermal + ME | OI-FAN-01b | CFD validated (or discrepancy explained) | 1–2 wk |
| **2** | **OI-FAN-03b** — THERM-1b full verification bench: interlock active, phantom face ≤ 42 °C through a fan-stall at T1-peak & T2-peak, 43.3 °C ambient | ME + Thermal | IMPL, OI-FAN-03a | SR-FAN-01 design verification PASS | 2–3 wk |
| **3** | **OI-FAN-04b** — Finalize SR-FAN-03/04 constants in NP-SW-001; close RISK-2x to acceptable residual; update traceability (NP-DT-001) | Quality (+FW) | OI-FAN-03b | Requirement final; hazard closed; trace complete | 1–2 wk |

## 4. Notes on sequencing choices

- **C2 first (cheap go/no-go).** The single most decision-relevant CFD case runs before the full
  matrix, so Path A is confirmed or killed in ~1–2 weeks — before any interlock spend.
- **Baseline the hazard now (OI-FAN-04a).** A known S3 thermal hazard should be in the risk file (now NP-RISK-004) under
  QMS control immediately; the requirement is accepted provisionally with constants as TBD so it does
  not wait on the CFD.
- **OI-FAN-02 in parallel, not after.** Knowing the scalp-NTC is feasible *before* the gate means a
  "Path B1" decision is actionable the day the CFD lands, not the start of a new investigation.
- **Split verification (03a/03b).** The model-correlation coupon validates the CFD right after it
  runs; the full interlock verification follows the firmware prototype. This lets the CFD earn
  verification-grade trust early without waiting on Class C firmware.
- **Path A shortcut.** If C2 shows a large Path-A margin at low perfusion, IMPL collapses to
  SR-FAN-05 monitoring only (Class B) — but per NP-REQ-FANHEALTH-001 §1/§4 this is the unlikely branch
  and the regulatory posture for a Class C burn hazard still favors an active control.

## 5. Cross-references

NP-REQ-FANHEALTH-001 §8 (source OIs) · NP-THERM-CFD-001 (case matrix) · NP-FMEA-GEOM-001 (FMEA-G07-01) ·
NP-SW-001 §6.2 · NP-RM-001 / NP-RISK-004 (index NP-RISK-002) · NP-DT-001 (traceability, planned).
