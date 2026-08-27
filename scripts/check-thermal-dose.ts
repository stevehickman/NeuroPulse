#!/usr/bin/env bun
/**
 * check-thermal-dose.ts — would this protocol accumulate a harmful thermal dose?
 *
 * The design has an INSTANTANEOUS ceiling and no CUMULATIVE-DOSE check. The 42 °C
 * face limit (DI-SAFE-08), the scalp-facing NTC at PD2 (DI-SAFE-13, Path B1), the
 * 62 °C junction throttle and SR-FAN-03's fan-loss derate are all real-time
 * reactive interlocks: they answer *"is the face too hot right now?"*
 *
 * Nothing answers *"will this protocol, over its stated duration, accumulate a
 * harmful thermal dose?"*, and nothing considers repeated or back-to-back
 * sessions. A repo-wide search for CEM43 or any cumulative thermal-dose treatment
 * returns nothing outside NP-PWRSRC-001 itself.
 *
 * This script is that missing arithmetic, run over the authored library rather
 * than over a hypothetical — the thermal-dose sibling of check-pbm-power.ts. It
 * imports analyse() from that script so the demand model cannot fork
 * (NP-SES-PWR-001 D-1).
 *
 * ── The metric ───────────────────────────────────────────────────────────────
 *
 *   CEM43 = Σ Δt · R^(43 − T),   R = 0.25 for T < 43 °C,  R = 0.5 for T ≥ 43 °C
 *
 * (Sapareto & Dewey.) The breakpoint matters enormously: BELOW 43 °C every extra
 * degree quadruples dose; above it, doubles. At the 42 °C interlock ceiling the
 * accumulation rate is exactly 0.25 CEM43 per minute — small per minute, and
 * unbounded in a long session, because the interlock caps TEMPERATURE and says
 * nothing about DURATION.
 *
 * ── ⚠ EVERY THERMAL CONSTANT BELOW IS PROVISIONAL ────────────────────────────
 *
 * NP-THERM-CFD-R1-001 is a 1D closed-form plus an explicitly NON-verification-
 * grade axisymmetric FD model, and OI-PWR-01's verification-grade CFD has not
 * run. The transfer model here is first-order lumped and calibrated to reproduce
 * ONE published point exactly (25 °C ambient, single T1-std tile → 30.7 °C face).
 *
 *   >>> THIS IS A FLAG-FOR-REVIEW TOOL, NOT A PASS/FAIL GATE. <<<
 *
 * Every constant is a named export so it can be replaced from the CFD without
 * rewriting the check. None of them is a derived figure and none should be
 * quoted as one.
 *
 *   bun scripts/check-thermal-dose.ts
 *   bun scripts/check-thermal-dose.ts --ambient 35
 */
import { analyse, type Row } from "./check-pbm-power";

// ── Provisional thermal constants ────────────────────────────────────────────
// Replace from OI-PWR-01's verification-grade CFD. Sources named per constant.

/** Nominal ambient. NP-ENV-OPRANGE-001 gates PBM full ≤ +35 °C, derate +35→+43. */
export const T_AMBIENT_NOMINAL_C = 25.0;

/** Face-temperature time constant, minutes. NP-THERM-CFD-R1-001 §4:
 *  "τ_face ≈ 35–45 min (order tens of minutes)". */
export const TAU_FACE_MIN = 35.0;
export const TAU_FACE_MAX = 45.0;

/** Sealed-cavity thermal resistance seen by aggregate emitter power, °C/W.
 *  NP-PWR-BUDGET-001 §3.2: 10 % residual over ~0.1 m² of vault at
 *  0.23–0.41 m²K/W, so ΔT_cavity = P_total × R with R in °C/W numerically equal. */
export const R_CAVITY_CONSERVATIVE = 0.41;
export const R_CAVITY_OPTIMISTIC = 0.23;

/** The one published calibration point: NP-THERM-CFD-R1-001 §5.1, single T1-std
 *  tile, 25 °C ambient → face 30.7 °C, i.e. a 5.7 °C total rise. */
export const CALIB_FACE_RISE_C = 5.7;
/** Per-tile draw the calibration point corresponds to (NP-HW-HEXTILE-001 §9.2). */
export const CALIB_TILE_W = 6.25;

/** Applied-part limit, hardware-enforced. NP-DT-001 DI-SAFE-08 / DI-SAFE-13.
 *  NOT changed by anything in NP-PWRSRC-001. */
export const FACE_LIMIT_C = 42.0;

/** Watts to emitters under the R-10 envelope — the cascading budget. */
export const BUDGET_W = 40.0;

/** ⚠ INJURY THRESHOLDS ARE LITERATURE VALUES, NOT PROJECT FIGURES, AND ARE
 *  PLACEHOLDERS PENDING A SOURCED CLINICAL INPUT (NP-PWRSRC-001 OI-PWRSRC-03).
 *  They are printed as reference lines only. The DERIVED quantity this script
 *  produces is the dose; the thresholds are an overlay on it. */
export const CEM43_REVIEW_LINE = 2.0;   // conservative device-safety practice
export const CEM43_CONCERN_LINE = 40.0; // region where skin injury is discussed

/** Course length for the repeated-exposure case: the Alzheimer's dosing pattern
 *  cited in the protocol library — 108 sessions over 56 days. */
export const COURSE_SESSIONS = 108;

// ── Model ────────────────────────────────────────────────────────────────────

/** Steady-state face rise above ambient for an aggregate emitter load.
 *  Two terms: a LOCAL term scaling with per-tile drive, and a CAVITY term
 *  scaling with total dissipated power. The local coefficient is back-solved so
 *  the calibration point is reproduced exactly at each R. */
export function faceRiseSteady(totalW: number, tileW: number, rCav: number): number {
  const localAtCalib = CALIB_FACE_RISE_C - rCav * CALIB_TILE_W;
  return localAtCalib * (tileW / CALIB_TILE_W) + rCav * totalW;
}

/** CEM43 accumulated per minute at a face temperature. */
export function cem43Rate(tC: number): number {
  return tC >= 43 ? Math.pow(0.5, 43 - tC) : Math.pow(0.25, 43 - tC);
}

/** Integrate CEM43 over one session, first-order transient, clamped by the
 *  hardware interlock at FACE_LIMIT_C. Returns dose and the temperature the
 *  face ends the session at (for the back-to-back case). */
export function sessionDose(
  totalW: number, tileW: number, durationMin: number,
  rCav: number, tauMin: number, ambientC: number, startRiseC = 0,
): { cem43: number; peakUnclampedC: number; endRiseC: number; trips: boolean } {
  const riseSS = faceRiseSteady(totalW, tileW, rCav);
  const stepMin = 1 / 60; // one-second steps
  let cem = 0;
  let endRise = startRiseC;
  for (let t = 0; t < durationMin; t += stepMin) {
    // First-order approach from startRiseC toward riseSS.
    const rise = riseSS + (startRiseC - riseSS) * Math.exp(-t / tauMin);
    const face = Math.min(ambientC + rise, FACE_LIMIT_C);
    cem += stepMin * cem43Rate(face);
    endRise = rise;
  }
  const peakUnclamped = ambientC + riseSS + (startRiseC - riseSS) * Math.exp(-durationMin / tauMin);
  return {
    cem43: cem,
    peakUnclampedC: peakUnclamped,
    endRiseC: Math.min(endRise, FACE_LIMIT_C - ambientC),
    trips: peakUnclamped > FACE_LIMIT_C,
  };
}

// ── Report ───────────────────────────────────────────────────────────────────

const argAmbient = process.argv.indexOf("--ambient");
const ambientC = argAmbient >= 0 ? Number(process.argv[argAmbient + 1]) : T_AMBIENT_NOMINAL_C;

const rows = analyse();
const fmt = (n: number) => (n >= 100 ? n.toFixed(0) : n >= 1 ? n.toFixed(1) : n.toExponential(1));

console.log(`Thermal-dose audit — CEM43 over ${rows.length} pbm_transcranial protocols`);
console.log(`(NP-PWRSRC-001 §5. Ambient ${ambientC} °C. Interlock ${FACE_LIMIT_C} °C.`);
console.log(` ⚠ ALL THERMAL CONSTANTS PROVISIONAL — flag-for-review, NOT a pass/fail gate.)\n`);

console.log(`At the ${FACE_LIMIT_C} °C interlock ceiling the accumulation rate is exactly`);
console.log(`${cem43Rate(FACE_LIMIT_C).toFixed(2)} CEM43/min, so ${CEM43_CONCERN_LINE} CEM43 is reached in`);
console.log(`${(CEM43_CONCERN_LINE / cem43Rate(FACE_LIMIT_C)).toFixed(0)} minutes of contact at the limit.\n`);

// ── Case A: single-pass, only for protocols that fit the envelope ────────────
console.log("── CASE A — single-pass, protocols that fit the 40 W envelope ──");
console.log(
  "protocol".padEnd(34) + "W".padStart(7) + "min".padStart(6) +
  "peakC".padStart(8) + "CEM43".padStart(10) + "course".padStart(10),
);
console.log("-".repeat(75));
let caseAmax = 0;
for (const r of rows) {
  if (r.requiredW === null || r.requiredW > BUDGET_W || r.durationS === null) continue;
  const d = sessionDose(r.requiredW, r.perTileW, r.durationS / 60,
    R_CAVITY_CONSERVATIVE, TAU_FACE_MIN, ambientC);
  caseAmax = Math.max(caseAmax, d.cem43 * COURSE_SESSIONS);
  console.log(
    r.name.slice(0, 34).padEnd(34) +
    r.requiredW.toFixed(0).padStart(7) +
    (r.durationS / 60).toFixed(0).padStart(6) +
    d.peakUnclampedC.toFixed(1).padStart(8) +
    fmt(d.cem43).padStart(10) +
    fmt(d.cem43 * COURSE_SESSIONS).padStart(10),
  );
}
console.log(`\nWorst ${COURSE_SESSIONS}-session course in Case A: ${fmt(caseAmax)} CEM43.\n`);

// ── Case B: cascaded to fit the budget — the case that changes the answer ────
console.log("── CASE B — cascaded to fit 40 W (NP-SES-PWR-001 §4's remedy) ──");
console.log("Every over-budget protocol runs at the budget ceiling for groups × duration.");
console.log(
  "protocol".padEnd(34) + "grps".padStart(5) + "hours".padStart(7) +
  "peakC".padStart(8) + "CEM43".padStart(10) + "flag".padStart(8),
);
console.log("-".repeat(75));
const caseB: { name: string; cem: number }[] = [];
for (const r of rows) {
  if (r.requiredW === null || r.requiredW <= BUDGET_W || r.durationS === null || r.groups === null) continue;
  const mins = (r.durationS / 60) * r.groups;
  // Cascaded: the emitter load is held at the governor budget; per-tile drive is
  // the protocol's own, since cascading changes WHICH tiles are lit, not how hard.
  const d = sessionDose(BUDGET_W, r.perTileW, mins,
    R_CAVITY_CONSERVATIVE, TAU_FACE_MIN, ambientC);
  caseB.push({ name: r.name, cem: d.cem43 });
  console.log(
    r.name.slice(0, 34).padEnd(34) +
    r.groups.toString().padStart(5) +
    (mins / 60).toFixed(1).padStart(7) +
    d.peakUnclampedC.toFixed(1).padStart(8) +
    fmt(d.cem43).padStart(10) +
    (d.cem43 >= CEM43_CONCERN_LINE ? "CONCERN" : d.cem43 >= CEM43_REVIEW_LINE ? "review" : "—").padStart(8),
  );
}
const overConcern = caseB.filter((c) => c.cem >= CEM43_CONCERN_LINE).length;
const overReview = caseB.filter((c) => c.cem >= CEM43_REVIEW_LINE).length;
console.log(
  `\n${overConcern} of ${caseB.length} cascaded protocols reach the ${CEM43_CONCERN_LINE} CEM43 reference line ` +
  `in a SINGLE session; ${overReview} reach ${CEM43_REVIEW_LINE}.`,
);

// ── Case C: back-to-back, the case nothing in the design considers ───────────
console.log("\n── CASE C — back-to-back sessions, τ_face carry-over ──");
console.log(`τ_face is ${TAU_FACE_MIN}–${TAU_FACE_MAX} min. A second session begun soon after the`);
console.log("first starts from an elevated baseline. Worked for a 20-min, 40 W session:\n");
console.log("gap (min)".padEnd(12) + "start rise".padStart(12) + "peakC".padStart(9) + "CEM43 (2nd)".padStart(13));
console.log("-".repeat(46));
const refTileW = 5.0;
const first = sessionDose(BUDGET_W, refTileW, 20, R_CAVITY_CONSERVATIVE, TAU_FACE_MIN, ambientC);
for (const gap of [0, 15, 30, 60, 120]) {
  const carried = first.endRiseC * Math.exp(-gap / TAU_FACE_MIN);
  const second = sessionDose(BUDGET_W, refTileW, 20, R_CAVITY_CONSERVATIVE, TAU_FACE_MIN, ambientC, carried);
  console.log(
    `${gap}`.padEnd(12) + `${carried.toFixed(1)} °C`.padStart(12) +
    second.peakUnclampedC.toFixed(1).padStart(9) + fmt(second.cem43).padStart(13),
  );
}

// ── Sensitivity ──────────────────────────────────────────────────────────────
console.log("\n── SENSITIVITY ──");
const sens = (rCav: number, tau: number) =>
  sessionDose(BUDGET_W, refTileW, 20, rCav, tau, ambientC);
console.log(`R_cav ${R_CAVITY_CONSERVATIVE} / τ ${TAU_FACE_MIN}: peak ${sens(R_CAVITY_CONSERVATIVE, TAU_FACE_MIN).peakUnclampedC.toFixed(1)} °C, ` +
  `CEM43 ${fmt(sens(R_CAVITY_CONSERVATIVE, TAU_FACE_MIN).cem43)}`);
console.log(`R_cav ${R_CAVITY_OPTIMISTIC} / τ ${TAU_FACE_MAX}: peak ${sens(R_CAVITY_OPTIMISTIC, TAU_FACE_MAX).peakUnclampedC.toFixed(1)} °C, ` +
  `CEM43 ${fmt(sens(R_CAVITY_OPTIMISTIC, TAU_FACE_MAX).cem43)}`);
const amb = sessionDose(BUDGET_W, refTileW, 20, R_CAVITY_CONSERVATIVE, TAU_FACE_MIN, 35);
console.log(`Ambient 35 °C (NP-ENV-OPRANGE-001 derate threshold): peak ${amb.peakUnclampedC.toFixed(1)} °C, CEM43 ${fmt(amb.cem43)}`);
console.log(
  `\nOI-SESPWR-03 moves the heat of 6 of ${rows.length} protocols by 4×, exactly as it moves their power ` +
  `(NP-SES-PWR-001 §2.4) — the CW reading is used throughout, per D-3 there.`,
);
console.log(
  `\nSR-FAN-03's fan-loss case is NOT modelled: PBM halts or trickles to ~4.5 mW/cm² on fan loss ` +
  `(NP-THERM-CFD-R1-001 §3), so the fault case is bounded by the interlock, not by dose.`,
);
