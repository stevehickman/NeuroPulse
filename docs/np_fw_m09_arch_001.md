# NP-FW-M09-ARCH-001 — Architecture Decision: Operating-Envelope Gate as SW01-M09 vs. extending SW01-M04

**Program:** NeurOne firmware / Safety MCU
**Status:** DRAFT decision note (settles **OI-POE-02**). Recommends a home for the predictive
operating-envelope admission/derate function defined in NP-FW-POE-001.
**Sources:** NP-FW-POE-001, NP-FMEA-001 §3 (SW01-M01…M08 module set + interlock pattern), NP-SW-001 §5.1,
NP-REQ-FANHEALTH-001 (SR-FAN), NP-ENV-OPRANGE-001.
**Decision:** **New module SW01-M09** (not an extension of SW01-M04).
**Date:** 2026-07-21

---

## 1. Context

NP-FW-POE-001 adds a **predictive, ambient-based operating-envelope gate**: read ambient (start-of-session
baseline), consult the per-modality envelope table + the descriptor's signed POE block, compute a duty
ceiling / admission decision (`min(TABLE, POE, SR-FAN)`), deny enable when out of range. Where should this
Class C logic live — folded into **SW01-M04** (the existing junction-NTC thermal interlock), or a **new
SW01-M09**?

## 2. Decision drivers

| Driver | Weight | Why it matters here |
|--------|--------|---------------------|
| Re-verification blast radius | **high** | M04 is the *certified junction-throttle/cutoff* — the scalp-burn interlock (FMEA-M04-01…04). Editing it re-opens that unit's Class C verification. |
| Independent-backstop preservation | **high** | The reactive junction cutoff must stay independent of any predictive-gate bug. |
| Single responsibility / cohesion | high | "Hold junction ≤ limit in real time" vs. "admit/deny + set ambient ceiling" are different responsibilities that merely share the word *temperature*. |
| Consistency with the existing architecture | high | SW-01 is already a **family of independent interlock checkers** (M03 charge, M04 thermal, M05 cardiac, M06 impedance), each owning one concern and feeding M01/M08. |
| Temporal character | med | M04 is a **reactive** 10 Hz control loop; the envelope gate is **predictive/admission** (baseline + coarse re-checks). |
| New-module cost | low | A new unit adds a test spec, FMEA section, NP-SW-001 row, traceability — modest and one-time. |

## 3. Options

**A — Extend SW01-M04.** *Pros:* shares the "thermal" topic and NTC HAL; one place composes the final
thermal duty. *Cons:* pulls descriptor/POE parsing + table lookup + SR-FAN state into a unit whose current
job is a clean sensor→throttle loop; **re-opens the certified junction interlock**; mixes a real-time loop
with admission logic (more state, larger stack/MISRA surface); breaks the "one checker, one concern"
pattern.

**B — New SW01-M09 "Operating-Envelope Admission Gate."** *Pros:* the envelope gate is *exactly* another
member of the existing interlock-checker family (like M06 impedance — an independent check that feeds M01's
enable and M08's fault latch); **freezes M04**, so the scalp-burn interlock is not re-verified; isolates
predictive-gate failure modes from the reactive cutoff; single responsibility. *Cons:* one more Class C
unit to spec/verify.

## 4. Decision — **B, new SW01-M09**

The deciding factor is that **SW-01 is already structured as independent interlock checkers that each own
one safety concern and feed M01 (enable) + M08 (fault)**. The operating-envelope gate is one more such
concern — ambient operating range — with its own inputs (ambient NTC, signed POE, envelope table,
SR-FAN state). Folding it into M04 would break that pattern and, worse, re-open the certified junction
interlock for re-verification. A sibling module keeps M04 frozen, isolates the new failure modes, and
matches the architecture the whole Class C design already rests on.

## 5. Module responsibilities & interfaces

**SW01-M09 — Operating-Envelope Admission Gate (Class C):**
- Read ambient (session-start baseline + coarse re-reads; no reading → fail-safe worst-case ambient).
- Consult the provisioned envelope **table** (authoritative) + the verified descriptor **POE** block.
- Compute `envelope_ceiling = min( TABLE_clamp(ambient, active_modalities), POE_clamp(ambient) )` and the
  **admission decision** (ceiling == 0 → out of range).
- **Deny path:** feed M01 (enable arbitration) — same pattern as M06 impedance; latch via M08 on out-of-range.
- **Ceiling path:** publish `envelope_ceiling` for the thermal duty arbiter (§5.1).

**SW01-M04 — Thermal Interlock (unchanged responsibility):** junction-NTC real-time throttle (62 °C) /
cutoff (65 °C). Owns the thermal PWM actuator.

### 5.1 Where the min() lands (single actuator authority)

M04 owns the thermal duty actuator, so **M04 applies the composed clamp**:
`effective_duty = min( M04_junction_throttle, M09_envelope_ceiling, SR-FAN_ceiling )`.
M09 *computes and publishes* its ceiling; M04 *consumes* it. This keeps one actuator authority while
separating concerns.

### 5.2 Independence property (defense in depth)

M04's junction **cutoff (65 °C)** does **not** depend on M09: it reads only its own NTC. So a predictive-gate
fault (bad table, POE parse error, ambient-sensor fault) can lower or fail-safe the ceiling but **cannot
disable the reactive cutoff**. Conversely M09's admission deny does not depend on M04. Two independent
layers, as the interlock family intends.

## 6. SR-FAN allocation (refines OI-FAN, keeps the split coherent)

SR-FAN (NP-REQ-FANHEALTH-001) splits along the same reactive/predictive line:
- **Reactive face-temperature throttle** (Path B1, scalp-facing NTC) → **belongs with M04** (real-time, same
  character as the junction throttle; adds one NTC channel to M04, not a new module).
- **Fan-health-driven derate to the natural-convection ceiling** (Path B2) + the ambient envelope → **M09**
  (predictive/admission; the SR-FAN ceiling is one more term M09 folds into its published ceiling).

Net: **M04 = reactive real-time thermal throttles (junction + face NTC); M09 = predictive envelope/admission
+ fan-health-derate.** Clean two-axis split. (Flag this back to OI-FAN so the SR-FAN implementation lands in
the same shape.)

## 7. Consequences

- New **SW01-M09** row in NP-SW-001 §5.1; new unit-test spec (100 % branch on safety paths); new FMEA
  section **FMEA-M09-xx** (import the five POE failure modes from NP-FW-POE-001 §9); traceability entries.
- **M04 stays frozen** except the optional face-NTC channel (a small, separately-verified addition if SR-FAN
  Path B1 is chosen) — the certified junction interlock is not otherwise touched.
- Enable-arbitration (M01) gains one more checker input; fault-latch (M08) gains the out-of-range reason.
- The `min()` composition point is documented in M04; M09 is a pure computer/checker (no actuator ownership).

## 8. Follow-ups

| ID | Description | Owner |
|----|-------------|-------|
| OI-M09-01 | Author the SW01-M09 unit-test spec + FMEA-M09 section under change control (NP-SW-001, NP-FMEA-001) | Quality + FW |
| OI-M09-02 | Confirm the M09→M04 ceiling-publish interface (shared-memory value + validity/stale flag; M09 fault → M04 uses SR-FAN/junction only) | FW |
| OI-M09-03 | Route SR-FAN Path-B1 face-NTC to M04 and Path-B2 fan-derate to M09 per §6; update NP-REQ-FANHEALTH-001 allocation | FW |
| OI-M09-04 | Decide whether M09 admission runs once at start or also on coarse mid-session ambient re-reads (and the re-read cadence) | FW + Thermal |

## 9. Cross-references

NP-FW-POE-001 (the function + POE block) · NP-FMEA-001 §3 (interlock-checker pattern, M04) · NP-SW-001 §5.1 ·
NP-REQ-FANHEALTH-001 (SR-FAN split) · NP-ENV-OPRANGE-001 (envelope table).
