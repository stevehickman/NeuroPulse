#!/usr/bin/env bun
/**
 * check-thermal-bowl.ts — the outer-bowl heat budget after the Layer 4 deletion
 * (OI-EMCCAV-07, GitHub #403), and what it does to the Helmholtz actuator.
 * Owning document: NP-THERM-BOWL-001 (docs/np_therm_bowl_001.md).
 *
 * REQ-CAV-04 (taken 2026-09-23, GitHub #391) deleted the Layer 4 absorber and
 * re-lofted the outer bowl 3 mm, taking the outward path 0.410 -> 0.335 m^2K/W
 * (NP-EMC-CAV-001 §8.2). Less outward resistance is good for the scalp and puts
 * more heat into the bowl that carries the mu-metal (L2) and the Helmholtz
 * coils. REQ-EMI-11 calibrates a coil-drive -> field transfer function, and
 * NP-ENV-OPRANGE-001 §2 makes the active-cancellation envelope SOFT on drift.
 * Nobody had budgeted the coupling. This script does, and asserts no
 * measurement.
 *
 *   bun scripts/check-thermal-bowl.ts             # full report
 *   bun scripts/check-thermal-bowl.ts --validate  # anchors only, exit 1 on drift
 *
 * ── What it establishes ──────────────────────────────────────────────────────
 *
 *  1. THE DELETION BARELY MOVES THE BOWL AT EQUAL DRIVE. With the BN-boss via
 *     fitted, ~90 % of tile heat already lands on the outer bowl through the
 *     via, not through the absorber station. Removing the station re-routes only
 *     the cavity leg, so at a fixed drive the bowl warms by a fraction of a
 *     kelvin (§2). What moves the bowl is the drive the lower resistance
 *     ADMITS, and the face-42 C governor bounds that (§3).
 *
 *  2. THE BOWL IS BOUNDED BY THE SAFETY CEILING, NOT BY THE SUPPLY. The T1/T2
 *     peak rows of CLAUDE.md §4.5 are electrical peaks the assembly cannot
 *     spend thermally (NP-PWR-THERM-001 §3). Treated literally they put the
 *     face far past 42 C, so the interlocks throttle before the bowl gets
 *     there. The steady-state bowl maximum under PBM is therefore the drive
 *     that holds the face at 42 C — computed at +25 C and at the +35 C PBM block.
 *
 *  3. THE DRIFT QUESTION IS DECIDED BY TWO THINGS THE RECORD DOES NOT SAY: the
 *     coil DRIVE TOPOLOGY and the calibration TEMPERATURE. Voltage-mode drive
 *     moves the transfer gain at copper's 0.393 %/K; current-mode drive does
 *     not move it at first order. REQ-EMI-11 triggers recalibration on
 *     np_module_map rebuild only, so the calibration temperature can be
 *     anywhere in the operating envelope, and the drift is set by the whole
 *     coil-temperature span, not by one session's rise (§4, §5).
 *
 * ── Two allocations of the same outward path, both carried ───────────────────
 *
 * NP-THERM-COOL-001 §2 and NP-EMC-CAV-001 §8.2 allocate R_OUT_BASE = 0.41 as
 * gap 0.23 + foam 0.075 + shell 0.005 + film 0.10; that is the allocation whose
 * post-deletion total is the 0.335 GitHub #403 names. NP-THERM-SINK-001 §3.1
 * allocates it as gap 0.23 + solid 0.065 (foam 0.060 at k 0.05) + film 0.115
 * by balance, and SPEC-SINK-01 then fixes the film from a correlation (h 7.81
 * over the real exterior), which is 0.120 over the tile footprint. The two
 * disagree on the foam by 0.015 m^2K/W. Post-deletion the SPEC allocation
 * reads 0.355, not 0.335. For the BOWL, SPEC is the conservative one (a larger
 * film holds the bowl hotter), so both are reported and the SPEC case bounds.
 *
 * Coupled work: GitHub #407 (OI-THCOOL-21) re-baselines the same network. It
 * had not merged to main when this was written; the 0.335 case is computed
 * here, locally, and #407 will need to reconcile its constants with these.
 *
 * CI-Kind: report
 */
import {
  N_SOCKETS, TILE_AREA, FACE_LIMIT, AMB_NOMINAL, distributed, heatW, ETA_WP,
} from "./check-thermal-multitile";
import {
  steadySink, OUT_STACK, R_SOLID_OUT, H_EXT_SPEC, A_EXT_TILE, A_EXT_EFF,
  R_SINK_SPEC, SPREADERS, type SinkOpts,
} from "./check-thermal-sink";

const VALIDATE_ONLY = process.argv.includes("--validate");
const A = TILE_AREA;

// ---------------------------------------------------------------------------
// §1  The two allocations, before and after REQ-CAV-04
// ---------------------------------------------------------------------------

const R_GAP = 0.23;
/** NP-THERM-COOL-001 §2: foam 3 mm at k ~0.04. */
const R_FOAM_COOL = 0.075;
/** NP-THERM-COOL-001 §2: CFRP + Pd-polyester + mu-metal. */
const R_SHELL_COOL = 0.005;
/** NP-THERM-COOL-001 §2: external natural convection, h ~10 over the tile footprint. */
const R_FILM_COOL = 0.10;

/** NP-THERM-SINK-001 §3.1's solid stack with the absorber row struck. */
export const R_SHELL_SPEC = R_SOLID_OUT - OUT_STACK[0].mm / 1000 / OUT_STACK[0].k;
/** SPEC-SINK-01's film, referred to the tile footprint. */
export const R_FILM_SPEC = A / (A_EXT_TILE * H_EXT_SPEC);

export type Alloc = { id: string; name: string; rCX: number; hExt: number; rFilm: number };
const hFor = (rFilm: number) => A / (A_EXT_TILE * rFilm);

export const ALLOC = {
  coolPre: { id: "COOL-pre", name: "COOL §2, absorber in", rCX: R_FOAM_COOL + R_SHELL_COOL, hExt: hFor(R_FILM_COOL), rFilm: R_FILM_COOL },
  coolPost: { id: "COOL-post", name: "COOL §2, deleted + re-loft", rCX: R_SHELL_COOL, hExt: hFor(R_FILM_COOL), rFilm: R_FILM_COOL },
  specPre: { id: "SPEC-pre", name: "SPEC-SINK-01, absorber in", rCX: R_SOLID_OUT, hExt: H_EXT_SPEC, rFilm: R_FILM_SPEC },
  specPost: { id: "SPEC-post", name: "SPEC-SINK-01, deleted + re-loft", rCX: R_SHELL_SPEC, hExt: H_EXT_SPEC, rFilm: R_FILM_SPEC },
} satisfies Record<string, Alloc>;

export const rOut = (a: Alloc) => R_GAP + a.rCX + a.rFilm;

const KT_S3 = SPREADERS[3].kt;
/** Spreader states: the bare bowl that ships, and SPEC-SINK-04's S3 (OI-SINK-01, BLOCKING). */
const SPREAD = [
  { id: "bare", kt: 0 },
  { id: "S3", kt: KT_S3 },
] as const;

// ---------------------------------------------------------------------------
// §2  The bowl and coil temperatures for a drive
// ---------------------------------------------------------------------------

export type BowlResult = {
  faceMax: number; bowlMax: number; bowlMean: number; coilMax: number; qW: number;
};

/** Steady state for a per-socket heat vector. The coils sit on the INNER face
 *  of the outer bowl, so each is charged the heat crossing the shell solid
 *  (conservative: every watt leaving through that socket's film crosses it). */
export function bowl(q: number[], a: Alloc, amb: number, kt: number, extraSocketW = 0): BowlResult {
  const o: SinkOpts = { amb, hExt: a.hExt, rCX: a.rCX, ktSpreader: kt };
  const f = steadySink(q, o);
  // Coil self-heat is dissipated ON the bowl. steadySink injects only at the
  // junctions, so add it by superposition as a uniform rise charged to the film
  // alone — an upper bound, since the inward leak through the gap is ignored.
  const dx = extraSocketW / (A_EXT_TILE * a.hExt);
  const x = f.x.map((v) => v + dx);
  const gFilm = A_EXT_TILE * a.hExt;
  const rShellAbs = (a.id.startsWith("COOL") ? R_SHELL_COOL : R_SHELL_SPEC) / A;
  const coil = x.map((tx) => tx + (tx - amb) * gFilm * rShellAbs);
  return {
    faceMax: Math.max(...f.f),
    bowlMax: Math.max(...x),
    bowlMean: x.reduce((s, v) => s + v, 0) / x.length,
    coilMax: Math.max(...coil),
    qW: q.reduce((s, v) => s + v, 0),
  };
}

/** Uniform per-socket heat vector for a total emitter-electrical wattage. */
const uniformHeat = (elecTotalW: number, eta = ETA_WP) =>
  new Array(N_SOCKETS).fill((elecTotalW / N_SOCKETS) * (1 - eta));
const zeros = () => new Array(N_SOCKETS).fill(0);

/** Largest fully-distributed emitter-electrical total holding face <= 42 C. */
export function admissibleTotalW(a: Alloc, amb: number, kt: number): number {
  const face = (w: number) => bowl(uniformHeat(w), a, amb, kt).faceMax;
  if (face(0) > FACE_LIMIT) return 0;
  let lo = 0, hi = 400;
  for (let k = 0; k < 60; k++) { const m = (lo + hi) / 2; if (face(m) <= FACE_LIMIT) lo = m; else hi = m; }
  return lo;
}

// ---------------------------------------------------------------------------
// §3  Power: CLAUDE.md §4.5 peaks, net of the non-PBM overhead
// ---------------------------------------------------------------------------
// NP-HW-HEXTILE-001 §9.1 puts non-PBM overhead at 6–8 W; check-power-envelope.ts
// takes 7.0 and sites it in the hub (NP-PWRSRC-001 §4.4's hub row).
const OVERHEAD_W = 7.0;
export const PEAKS = [
  { id: "T1 peak lo", w: 45 }, { id: "T1 peak hi", w: 50 },
  { id: "T2 peak lo", w: 70 }, { id: "T2 peak hi", w: 74 },
];

// ---------------------------------------------------------------------------
// §4  Coil resistance and the REQ-EMI-11 transfer function
// ---------------------------------------------------------------------------

/** Annealed copper, IEC 60028: 0.00393 /K referred to 20 C. The coil
 *  conductor is not stated anywhere in the record; copper is ASSUMED. */
export const ALPHA_CU_20 = 0.00393;
export const rRatio = (t: number, t0: number) => (1 + ALPHA_CU_20 * (t - 20)) / (1 + ALPHA_CU_20 * (t0 - 20));

/** Voltage-mode drive, resistive limit (omega*L << R is the worst case:
 *  |d ln Z / d ln R| = R^2/|Z|^2 <= 1). Gain error vs calibration at t0. */
export const gainErrV = (t: number, t0: number) => 1 / rRatio(t, t0) - 1;

/** Feed-forward cancellation ceiling for a fractional actuator-gain error:
 *  the residual is |eps| of the field being subtracted. */
export const ceilingDb = (eps: number) => -20 * Math.log10(Math.abs(eps));

/** Active share of the ELF target, from CLAUDE.md §4.3: combined 35–45 dB with
 *  L2 supplying 15–25 dB leaves the active loop 10 (35−25) to 30 (45−15) dB.
 *  DESIGN TARGETS — EMF-1 has never run. */
export const ACTIVE_SHARE_DB = { lo: 35 - 25, mid: 40 - 20, hi: 45 - 15 };

/** Largest calibration-to-use temperature difference a voltage-driven coil
 *  tolerates for a given dB ceiling (at a 25 C calibration). */
export function toleratedDeltaT(db: number, t0 = 25): number {
  const eps = 10 ** (-db / 20);
  // |1/r − 1| = eps with r = rRatio(t0+d, t0) > 1  =>  r = 1/(1−eps)
  const r = 1 / (1 - eps);
  return (r - 1) * (1 + ALPHA_CU_20 * (t0 - 20)) / ALPHA_CU_20;
}

// ---------------------------------------------------------------------------
// Report
// ---------------------------------------------------------------------------

const f1 = (x: number) => x.toFixed(1);
const f2 = (x: number) => x.toFixed(2);
const f3 = (x: number) => x.toFixed(3);
const pct = (x: number) => `${(100 * x).toFixed(1)} %`;
const rule = (s: string) => console.log(`\n${s}\n${"-".repeat(s.length)}`);

/** Coil self-heating: dissipation not stated anywhere. Swept, not asserted. */
const COIL_W = [0, 0.5, 1.0, 2.0];

function reportAllocations() {
  rule("§1  The outward path, before and after REQ-CAV-04, in both allocations");
  console.log(`  alloc        C->X     film     R_out    R_sink(agg)`);
  for (const a of Object.values(ALLOC)) {
    console.log(`  ${a.id.padEnd(11)} ${f3(a.rCX)}   ${f3(a.rFilm)}   ${f3(rOut(a))}    ${f2(1 / (a.hExt * A_EXT_EFF))} K/W`);
  }
  console.log();
  console.log(`  COOL-post is the 0.335 GitHub #403 names. SPEC-post reads ${f3(rOut(ALLOC.specPost))} because`);
  console.log(`  NP-THERM-SINK-001 §3.1 puts the foam at ${f3(OUT_STACK[0].mm / 1000 / OUT_STACK[0].k)} (k 0.05) where NP-THERM-COOL-001 §2`);
  console.log(`  puts it at ${f3(R_FOAM_COOL)} (k 0.04), and SPEC-SINK-01's film is the correlation's, not`);
  console.log(`  the balance's. For the BOWL, the larger film is the conservative one.`);
}

function reportEqualDrive() {
  rule("§2  At equal drive the deletion barely moves the bowl");
  console.log(`  Fully distributed, 25 C ambient, 20 W emitter-electrical (${f1(20 * (1 - ETA_WP))} W heat):`);
  console.log();
  console.log(`  alloc        spreader   face max   bowl max   bowl mean   coil max`);
  for (const a of Object.values(ALLOC)) for (const s of SPREAD) {
    const r = bowl(uniformHeat(20), a, AMB_NOMINAL, s.kt);
    console.log(`  ${a.id.padEnd(11)}  ${s.id.padEnd(8)} ${f1(r.faceMax).padStart(8)}   ${f1(r.bowlMax).padStart(8)}   ${f1(r.bowlMean).padStart(9)}   ${f1(r.coilMax).padStart(8)}`);
  }
  const d = (pre: Alloc, post: Alloc) =>
    bowl(uniformHeat(20), post, AMB_NOMINAL, 0).bowlMean - bowl(uniformHeat(20), pre, AMB_NOMINAL, 0).bowlMean;
  console.log();
  console.log(`  Bowl-mean shift from the deletion at this drive: COOL ${f3(d(ALLOC.coolPre, ALLOC.coolPost))} K,` +
    ` SPEC ${f3(d(ALLOC.specPre, ALLOC.specPost))} K.`);
  const dw = (pre: Alloc, post: Alloc) => admissibleTotalW(post, 25, 0) - admissibleTotalW(pre, 25, 0);
  console.log(`  Admissible-drive gain from the deletion (25 C, bare): COOL ${f3(dw(ALLOC.coolPre, ALLOC.coolPost))} W,` +
    ` SPEC ${f3(dw(ALLOC.specPre, ALLOC.specPost))} W.`);
  console.log(`  The via (${f3(0.0059)} m2K/W) sits in PARALLEL with gap + station (0.30 -> 0.24) between`);
  console.log(`  the junction and the bowl, so the station changes J->X by < 1 %. With the via`);
  console.log(`  fitted, the 0.410 -> 0.335 change is a cavity-leg change the bowl cannot see,`);
  console.log(`  and it buys no admissible drive on this network either — a hand-off to`);
  console.log(`  OI-THCOOL-21 / #407, whose premise is that capability figures are understated.`);
}

function reportPeaks() {
  rule("§3  CLAUDE.md §4.5 peaks — literal, then as the face-42 C governor admits");
  console.log(`  (a) LITERAL. Peak draw less ${f1(OVERHEAD_W)} W non-PBM overhead (sited in the hub), all to`);
  console.log(`      emitters, fully distributed, 25 C ambient, SPEC-post, bare bowl. eta_wp ${ETA_WP}`);
  console.log(`      (light leaves into the wearer), and eta 0 as the everything-is-heat bracket:`);
  console.log();
  console.log(`  row          emitter W   heat W   face max   bowl max   coil max    | eta 0: bowl   coil`);
  for (const p of PEAKS) {
    const e = p.w - OVERHEAD_W;
    const r = bowl(uniformHeat(e), ALLOC.specPost, AMB_NOMINAL, 0);
    const r0 = bowl(uniformHeat(e, 0), ALLOC.specPost, AMB_NOMINAL, 0);
    console.log(`  ${p.id.padEnd(12)} ${f1(e).padStart(8)}   ${f1(r.qW).padStart(6)}   ${f1(r.faceMax).padStart(8)}   ${f1(r.bowlMax).padStart(8)}   ${f1(r.coilMax).padStart(8)}    |        ${f1(r0.bowlMax).padStart(5)}  ${f1(r0.coilMax).padStart(5)}`);
  }
  console.log();
  console.log(`      Every literal row puts the face past ${FACE_LIMIT} C. The PBM zone NTCs and the`);
  console.log(`      Path B1 face NTC throttle first (CLAUDE.md §4.2), so these bowl figures are`);
  console.log(`      an upper bound the device cannot hold in steady state — they are the answer`);
  console.log(`      to the question as asked, not an operating point.`);
  console.log();
  console.log(`  (b) ADMISSIBLE. The largest fully-distributed drive holding the face at 42 C —`);
  console.log(`      the steady-state bowl maximum under PBM on either tier:`);
  console.log();
  console.log(`  alloc        spreader  ambient   admissible W   face    bowl max   bowl mean   coil max`);
  for (const a of [ALLOC.specPre, ALLOC.specPost, ALLOC.coolPre, ALLOC.coolPost]) for (const s of SPREAD) for (const amb of [25, 35]) {
    const w = admissibleTotalW(a, amb, s.kt);
    const r = bowl(uniformHeat(w), a, amb, s.kt);
    console.log(`  ${a.id.padEnd(11)}  ${s.id.padEnd(8)} ${String(amb).padStart(5)} C   ${f1(w).padStart(10)}     ${f1(r.faceMax)}   ${f1(r.bowlMax).padStart(7)}    ${f1(r.bowlMean).padStart(7)}    ${f1(r.coilMax).padStart(7)}`);
  }
}

/** The hottest coil the record admits in steady state, across the envelope. */
export function coilSpan(coilW = 0) {
  const a = ALLOC.specPost;
  const perSocket = coilW / N_SOCKETS;
  const hot35 = Math.max(...SPREAD.map((s) => bowl(uniformHeat(admissibleTotalW(a, 35, s.kt)), a, 35, s.kt, perSocket).coilMax));
  const hot50 = bowl(zeros(), a, 50, 0, perSocket).coilMax;   // PBM blocked above +35
  const idle25 = bowl(zeros(), a, 25, 0, perSocket).coilMax;
  const cold = -10;                                            // off-head, unpowered, at the SOFT low bound
  return { hot35, hot50, idle25, cold, hot: Math.max(hot35, hot50) };
}

function reportSpan() {
  rule("§4  Coil temperature span, and the resistance change");
  const s = coilSpan();
  console.log(`  NP-ENV-OPRANGE-001 §2 bounds active cancellation on AMBIENT (−10 / +50 C, SOFT).`);
  console.log(`  The coil runs at ambient PLUS the bowl rise, so the envelope never states the`);
  console.log(`  coil temperature it tolerates. SPEC-post, steady state:`);
  console.log();
  console.log(`    cold end: off-head at −10 C ambient, before warm-up          ${f1(s.cold)} C`);
  console.log(`    idle on-head at 25 C (no tile driven)                         ${f1(s.idle25)} C`);
  console.log(`    admissible PBM at the +35 C block (worse spreader state)      ${f1(s.hot35)} C`);
  console.log(`    +50 C ambient, PBM blocked, head below ambient                ${f1(s.hot50)} C`);
  console.log();
  console.log(`  Coil self-heating is NOT specified anywhere (no conductor, turns, current or`);
  console.log(`  resistance is recorded). Swept as a uniform dissipation on the bowl, bounded`);
  console.log(`  by the film alone:`);
  console.log();
  console.log(`    P_coil    hottest coil    R(hot)/R(cold)   R(hot)/R(25 C idle)`);
  for (const w of COIL_W) {
    const c = coilSpan(w);
    console.log(`    ${f1(w)} W    ${f1(c.hot).padStart(8)} C      ${f3(rRatio(c.hot, c.cold)).padStart(8)}        ${f3(rRatio(c.hot, c.idle25)).padStart(8)}`);
  }
  console.log();
  console.log(`  Copper assumed (the record names no conductor): alpha ${ALPHA_CU_20} /K at 20 C.`);
}

function reportDrift() {
  rule("§5  Drift of the REQ-EMI-11 transfer function, and what it costs");
  const s = coilSpan(1.0);
  const cases = [
    { name: "one session: calibrated at 25 C idle, runs to admissible at 25 C", t0: coilSpan(1.0).idle25,
      t: Math.max(...SPREAD.map((sp) => bowl(uniformHeat(admissibleTotalW(ALLOC.specPost, 25, sp.kt)), ALLOC.specPost, 25, sp.kt, 1.0 / N_SOCKETS).coilMax)) },
    { name: "hot room: calibrated at 25 C idle, used at +50 C (PBM blocked)", t0: s.idle25, t: s.hot50 },
    { name: "envelope: calibrated cold (−10 C), used at the hottest state", t0: s.cold, t: s.hot },
  ];
  console.log(`  VOLTAGE-MODE drive (field ∝ V/R): gain error = R(cal)/R(use) − 1. Resistive`);
  console.log(`  limit, which is the worst case at ELF. Feed-forward ceiling = −20 log10|eps|.`);
  console.log(`  P_coil 1.0 W assumed for the hot end.`);
  console.log();
  console.log(`    case                                                          T_cal   T_use    eps      ceiling`);
  for (const c of cases) {
    const e = gainErrV(c.t, c.t0);
    console.log(`    ${c.name.padEnd(62)} ${f1(c.t0).padStart(5)}   ${f1(c.t).padStart(5)}  ${pct(e).padStart(7)}   ${f1(ceilingDb(e)).padStart(5)} dB`);
  }
  console.log();
  console.log(`  Active share the ceiling must clear (CLAUDE.md §4.3, design targets):`);
  console.log(`  ${ACTIVE_SHARE_DB.lo}–${ACTIVE_SHARE_DB.hi} dB (combined 35–45 less L2's 15–25). Tolerated calibration-to-use`);
  console.log(`  coil ΔT under voltage drive: ${f1(toleratedDeltaT(ACTIVE_SHARE_DB.hi))} K for ${ACTIVE_SHARE_DB.hi} dB · ${f1(toleratedDeltaT(ACTIVE_SHARE_DB.mid))} K for ${ACTIVE_SHARE_DB.mid} dB · ${f1(toleratedDeltaT(ACTIVE_SHARE_DB.lo))} K for ${ACTIVE_SHARE_DB.lo} dB.`);
  console.log();
  console.log(`  CURRENT-MODE drive (field ∝ I): coil resistance drops out at first order. What`);
  console.log(`  remains is compliance headroom — the driver must supply I·R(hot), which is`);
  console.log(`  ${pct(rRatio(s.hot, s.cold) - 1)} above I·R at the cold end — plus the second-order terms this`);
  console.log(`  script cannot bound: the mu-metal's permeability vs temperature (D1 makes it`);
  console.log(`  part of the transfer function) and former expansion (CFRP/PETG, < 0.01 %/K).`);
  console.log();
  console.log(`  FEEDBACK vs FEED-FORWARD. A fluxgate-closed loop sees actuator gain error`);
  console.log(`  divided by its loop gain. REQ-EMI-05's feed-forward self-field subtraction and`);
  console.log(`  the "synchronous Helmholtz subtraction from EEG" use the CALIBRATED transfer`);
  console.log(`  directly and take the error in full. The ceilings above are for that path.`);
  console.log();
  console.log(`  RECALIBRATION. REQ-EMI-11 recalibrates on np_module_map rebuild — an insertion`);
  console.log(`  change — and durability-maintenance.md's session-start nulling zeroes fluxgate`);
  console.log(`  OFFSET, not actuator gain. Neither has a temperature term, so the calibration`);
  console.log(`  temperature is whatever the bowl was at the last insertion: the "envelope" row`);
  console.log(`  is reachable, not hypothetical. → OI-THBOWL-02 (raised, not designed).`);
}

function reportValidation(): boolean {
  rule("§0  Validation — published anchors");
  let ok = true;
  const check = (name: string, got: number, want: number, tol: number, unit = "") => {
    const pass = Math.abs(got - want) <= tol;
    ok &&= pass;
    console.log(`  ${pass ? "ok  " : "FAIL"} ${name.padEnd(54)} ${f3(got)}${unit} vs ${f3(want)}${unit} +/- ${tol}`);
  };
  check("R_out pre, COOL (NP-THERM-COOL-001 §2)", rOut(ALLOC.coolPre), 0.410, 0.001, " m2K/W");
  check("R_out post, COOL (NP-EMC-CAV-001 §8.2)", rOut(ALLOC.coolPost), 0.335, 0.001, " m2K/W");
  check("SPEC-SINK-01 R_sink (NP-THERM-SINK-001 §4)", R_SINK_SPEC, 1.08, 0.02, " K/W");
  check("SINK §3.1 solid stack", R_SOLID_OUT, 0.0651, 0.0005, " m2K/W");
  // rCX = default must reproduce check-thermal-sink.ts exactly (no fork).
  const q = uniformHeat(20);
  const a = steadySink(q, { amb: 25, ktSpreader: KT_S3 });
  const b = steadySink(q, { amb: 25, ktSpreader: KT_S3, rCX: R_SOLID_OUT, hExt: H_EXT_SPEC });
  check("rCX override = default reproduces sink model", Math.max(...a.f) - Math.max(...b.f), 0, 1e-9, " K");
  // NP-THERM-SINK-001 §8.1: 31.4 W fully distributed with S3 at 25 C (absorber in).
  check("SINK §8.1 aggregate W, S3, 25 C", admissibleTotalW(ALLOC.specPre, 25, KT_S3), 31.4, 0.5, " W");
  // NP-THERM-SINK-001 §6: T_skin 31.1 C idle at 25 C (absorber in).
  check("SINK §6 idle T_skin at 25 C", bowl(zeros(), ALLOC.specPre, 25, KT_S3).bowlMax, 31.1, 0.1, " C");
  check("copper alpha (IEC 60028)", ALPHA_CU_20, 0.00393, 1e-6, " /K");
  console.log(`  ${ok ? "PASS" : "FAIL"}`);
  return ok;
}

function main() {
  const ok = reportValidation();
  if (VALIDATE_ONLY) { process.exit(ok ? 0 : 1); return; }
  reportAllocations();
  reportEqualDrive();
  reportPeaks();
  reportSpan();
  reportDrift();
  console.log();
}

if (import.meta.main) main();
