#!/usr/bin/env bun
/**
 * check-cavity-q.ts — the Layer 4 requirement NP-BIB-EMF-001 §7.3 says nobody ever wrote.
 *
 * `OI-BIBEMF-08` places a burden on EMC: state Layer 4's requirement **in dB, against
 * a named source and a named frequency band**, or lose the layer. Layer 4 is the only
 * member of the CLAUDE.md §4.3 stack with no dB figure in any document — including
 * `NP-DT-001` `DI-PERF-22`, where every other layer has one — while its cost is known
 * to three significant figures (18 % of the outward thermal path, `NP-THERM-COOL-001`
 * §2) and blocks two fixes to a BLOCKING `OI-SINK-01`.
 *
 * This script is the arithmetic behind `NP-EMC-CAV-001`. It answers the burden, and
 * the answer is not the one §7.3 expected.
 *
 *   bun scripts/check-cavity-q.ts             # full report
 *   bun scripts/check-cavity-q.ts --validate  # published anchors only, exit 1 on drift
 *
 * CI-Kind: report
 *
 * (Declared up here rather than at the foot of the banner: check-gate-coverage.ts
 * reads only the first 80 lines, and this banner outgrew that window at Rev 2.)
 *
 * ── The five things it establishes ───────────────────────────────────────────
 *
 *  1. THE BAND IS 420 MHz – 3 GHz, AND ITS LOWER EDGE MOVES WITH THE WEARER.
 *     The cavity is the shell between the scalp and the Pd-polyester liner
 *     (`NP-HELMET-GEOM-001` §2: L1 18–22 mm + gap 5–7 mm + absorber 3.0 mm). Its
 *     lowest supported mode fits one guided wavelength around the mean
 *     circumference, so it is a function of head size — 506 MHz at a 52 cm head,
 *     422 MHz at 62 cm. No document records that the resonant frequency of the
 *     enclosure is a property of the person wearing it, which is why a
 *     single-frequency requirement would have been wrong even if one existed.
 *
 *  2. THE SOURCE IS REAL AND NAMEABLE. `NP-DRV-SHELL-002` §3.2 puts 18 STM32G071
 *     cluster controllers on L1 INSIDE the envelope, with a 400 kHz I2C tree
 *     (§3.4) and 80 PWM LED drivers (§9.2, ≈20 kHz carrier). §9.6 states the
 *     consequence in terms: "A Faraday cage does not protect the EEG electrodes
 *     and fluxgates that share the enclosure with the source." §7.3's premise —
 *     that the radios live in the hub, so nothing excites the cavity — is wrong.
 *     Digital edges, not radios, are the driver.
 *
 *  3. A 3 mm NON-MAGNETIC ABSORBER AGAINST A CONDUCTOR IS ELECTROMAGNETICALLY
 *     INVISIBLE AT THESE FREQUENCIES, AND THAT RESULT IS STRUCTURAL. For a lossy
 *     slab of thickness d on a PEC backing, Z_in = j·eta·tan(k·d); in the thin
 *     limit tan(kd) -> kd and eta·k = eta0·k0 EXACTLY, because eta ∝ 1/sqrt(e_r)
 *     and k ∝ sqrt(e_r). So Z_in -> j·eta0·k0·d — purely reactive, and INDEPENDENT
 *     OF THE LOADING. Loss is second order in (d/lambda), and at the lowest mode
 *     3 mm is lambda/217. Sweeping the foam's permittivity from lightly to
 *     implausibly heavily loaded moves its surface resistance from 1 mOhm to
 *     37 mOhm against a Pd-polyester wall of ~100 mOhm/sq: between 0.08 dB and
 *     2.7 dB of Q, against the 26 dB the requirement needs. This
 *     is why every thin commercial RF absorber is iron- or ferrite-loaded — and
 *     `REQ-EMI-10` plus NP-BIB-EMF-001 §7.8 forbid putting magnetic loading in
 *     this enclosure.
 *
 *  4. THE WEARER'S HEAD IS THE ABSORBER, BY ~50 dB. Skin at the lowest mode (e_r 44,
 *     sigma 0.44 S/m — the LEAST lossy tissue that could bound the cavity, chosen
 *     to be conservative) presents Re(eta) ≈ 54 Ohm/sq over ~37 % of the cavity
 *     boundary. That takes the bare-wall Q of ~409 to a loaded Q of ~1.3: not a
 *     damped resonance, an absent one. Every state in which the named sources of
 *     (2) are energised is a state in which the head is inside the cavity.
 *
 *  5. REMOVING IT IS THE RIGHT CALL, BUT THE 3 mm RE-LOFT IS BINDING. Vacating a
 *     3 mm station does not delete its resistance — it fills it with stagnant
 *     air, and air is a WORSE insulator per mm than the foam (0.115 vs 0.075
 *     m2K/W, a 54 % regression). So deletion alone takes the outward path
 *     0.410 -> 0.450, while deletion WITH the re-loft reaches 0.335, the best
 *     figure available, against a ceramic-filled substitution's 0.355. The
 *     deletion and the re-loft are ONE change, not two.
 *
 *     The re-loft costs nothing today: no mould is cut (`NP-REV-SHELL-001` is
 *     DRAFT, no item signed) and `OI-ART-01` already owes a re-scope of
 *     `NP-TOOL-SHELL-001`. Nor is "5-layer" a published claim — nothing is
 *     externally published. An earlier revision charged both to deletion and
 *     concluded "substitute"; neither cost exists.
 *
 * ── What it does NOT establish ───────────────────────────────────────────────
 *
 * It measures nothing. `EMF-1` has still never run. The head model is a
 * homogeneous sphere with a skin surface impedance, and the enclosure is treated
 * as a spherical shell; both are first-order. Every simplification was taken in
 * the conservative direction (least lossy tissue, lossless head for the bare-wall
 * figure, no credit for the L1 module bodies that also sit in the cavity), so the
 * margin is a floor rather than an estimate.
 *
 * The band stops at 3 GHz because the INTERNAL source rolls off there. That
 * argument does not cover EXTERNAL 6 GHz Wi-Fi ingress through the parting-plane
 * seam, which is a different excitation. §6.5 (report block 10, `OI-EMCCAV-06`)
 * works that case against the post-deletion geometry: at 5.925-7.125 GHz the
 * cavity is OVERMODED (modal overlap ~25), so the head bounds a diffuse field
 * at composite Q 34-41 rather than removing a mode — and the internal field is
 * decided by the SEAM (a 36 dB swing between §5.3a's discrete apertures and a
 * continuous slot), not by the deleted layer (2 dB). Unmeasured; EMF-1a/1b must
 * be swept to 7.125 GHz to confirm it.
 */

import * as TOP from "./thermal-outward-path";

const VALIDATE_ONLY = process.argv.includes("--validate");

// ── Physical constants ───────────────────────────────────────────────────────
const C0 = 299_792_458; // m/s
const MU0 = 4 * Math.PI * 1e-7; // H/m
const EPS0 = 8.8541878128e-12; // F/m
const ETA0 = Math.sqrt(MU0 / EPS0); // ~376.73 Ohm

// ── Minimal complex arithmetic ───────────────────────────────────────────────
type Cx = { re: number; im: number };
const cx = (re: number, im = 0): Cx => ({ re, im });
const cmul = (a: Cx, b: Cx): Cx => cx(a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re);
const cscale = (a: Cx, s: number): Cx => cx(a.re * s, a.im * s);
const cdiv = (a: Cx, b: Cx): Cx => {
  const d = b.re * b.re + b.im * b.im;
  return cx((a.re * b.re + a.im * b.im) / d, (a.im * b.re - a.re * b.im) / d);
};
const cabs = (a: Cx): number => Math.hypot(a.re, a.im);
const csqrt = (a: Cx): Cx => {
  const m = Math.sqrt(cabs(a));
  const th = Math.atan2(a.im, a.re) / 2;
  return cx(m * Math.cos(th), m * Math.sin(th));
};
/** tan(z) = sin(z)/cos(z) for complex z, via the real/imag expansions. */
const ctan = (z: Cx): Cx => {
  const { re: x, im: y } = z;
  const den = Math.cos(2 * x) + Math.cosh(2 * y);
  return cx(Math.sin(2 * x) / den, Math.sinh(2 * y) / den);
};

// ── Geometry: NP-HELMET-GEOM-001 §2 radial stack, scalp datum -> exterior ────
// The cavity is bounded inside by the scalp and outside by the first continuous
// conductor, which is the Pd-polyester liner (EMF L3). The absorber (EMF L4) is
// the last 3.0 mm before that conductor, laid directly against it.
const HEAD_CIRC_M = { min: 0.52, max: 0.62 }; // CLAUDE.md §4.4, 1 adult SKU
const L1_SUBTOTAL_M = { min: 0.018, max: 0.022 }; // module body + socket wall + clamp
const INTERBOWL_GAP_M = { min: 0.005, max: 0.007 }; // clamp travel + boss + labyrinth
const ABSORBER_T_M = TOP.ABSORBER_T_M; // EMF L4, the layer under audit (deleted 2026-09-23)

/** Scalp -> absorber inner face: the air/dielectric part of the cavity. */
const airThickness = (which: "min" | "max") => L1_SUBTOTAL_M[which] + INTERBOWL_GAP_M[which];
/** Scalp -> conductive boundary: the air part plus the absorber itself. */
const cavityThickness = (which: "min" | "max") => airThickness(which) + ABSORBER_T_M;

const headRadius = (circ: number) => circ / (2 * Math.PI);

/**
 * Lowest supported mode: one guided wavelength around the mean circumference of
 * the shell. A shell of mean radius r_m thinner than lambda/20 cannot support a
 * radial mode at that frequency, so the circumferential family is the lowest.
 */
function lowestMode(circ: number, which: "min" | "max") {
  const aHead = headRadius(circ);
  const rMean = aHead + airThickness(which) / 2;
  const cMean = 2 * Math.PI * rMean;
  return { aHead, rMean, cMean, f: C0 / cMean };
}

/**
 * First RADIAL mode of the air layer. The outer boundary is a conductor (short);
 * the inner is lossy tissue of high permittivity, which lies between a short
 * (t = lambda/2) and an open (t = lambda/4). The quarter-wave case is the lower
 * frequency and therefore the binding one.
 */
const firstRadialMode = (which: "min" | "max") => ({
  quarterWave: C0 / (4 * airThickness(which)),
  halfWave: C0 / (2 * airThickness(which)),
});

// ── Source: NP-DRV-SHELL-002 §3.2, §3.4, §9.2 ────────────────────────────────
// The emission envelope of a trapezoidal edge is flat, then -20 dB/dec past
// 1/(pi*tau), then -40 dB/dec past the knee 1/(pi*t_r). Only the knee matters
// here: it sets how fast the source falls across the band.
const EDGE_TIME_S = { fast: 4e-9, slow: 8e-9 }; // STM32G071-class GPIO into a short trace
const knee = (tr: number) => 1 / (Math.PI * tr);
/** dB below the knee amplitude at frequency f, on the -40 dB/decade asymptote. */
const sourceRolloffDb = (f: number, tr: number) => {
  const fk = knee(tr);
  return f <= fk ? 0 : -40 * Math.log10(f / fk);
};

// ── Cavity Q ─────────────────────────────────────────────────────────────────
// Q = G * omega*mu0*V / (R_s * S). G = 1 reproduces the textbook 2V/(delta*S) for
// a good conductor; G = 0.5 is used throughout because tangential H at a real
// mode's wall exceeds the volume RMS, and halving Q is the conservative direction
// for a requirement.
const G_GEOM = 0.5;

function shellVolume(aHead: number, aShield: number) {
  return (4 / 3) * Math.PI * (aShield ** 3 - aHead ** 3);
}
const sphereArea = (a: number) => 4 * Math.PI * a * a;
const headVolume = (a: number) => (4 / 3) * Math.PI * a ** 3;

/** Q contribution of one boundary of area S with surface resistance R_s. */
function qFromBoundary(f: number, V: number, S: number, Rs: number) {
  return (G_GEOM * 2 * Math.PI * f * MU0 * V) / (Rs * S);
}

/** Surface resistance of tissue: the real part of its intrinsic impedance. */
function tissueSurfaceR(f: number, epsR: number, sigma: number) {
  const omega = 2 * Math.PI * f;
  const epsC = cx(epsR, -sigma / (omega * EPS0));
  const eta = cdiv(cx(ETA0), csqrt(epsC));
  return { Rs: eta.re, eta, epsC };
}

/**
 * Surface resistance presented by a lossy slab of thickness d on a PEC backing:
 * Z_in = j * eta * tan(k*d). This is the whole Layer 4 question in one line.
 */
function slabSurfaceR(f: number, d: number, epsR: Cx) {
  const k0 = (2 * Math.PI * f) / C0;
  const sq = csqrt(epsR);
  const eta = cdiv(cx(ETA0), sq); // eta0 / sqrt(e_r)
  const k = cscale(sq, k0); // k0 * sqrt(e_r)
  const kd = cscale(k, d);
  const zIn = cmul(cx(0, 1), cmul(eta, ctan(kd))); // j * eta * tan(kd)
  return { Rs: zIn.re, Xs: zIn.im, kd, thinLimitX: ETA0 * k0 * d };
}

// ── Tissue and material assumptions, all sweepable ───────────────────────────
// Dry skin is the OUTERMOST tissue and the least lossy candidate for the cavity's
// inner boundary (muscle and grey matter are both lossier). Using it understates
// the head's damping, which is the conservative direction for this finding.
const SKIN_500MHZ = { epsR: 44, sigma: 0.44 };
// Metallized-fabric surface resistance. 0.1 Ohm/sq is the design assumption;
// 0.02-0.5 brackets the commercial range for Pd/Ag-polyester.
const PD_FABRIC_RS = { low: 0.02, nominal: 0.1, high: 0.5 };
// Carbon-loaded open-cell foam. The span is deliberately absurd at the top end:
// 6 - j12 is far beyond any real open-cell carbon foam, and is included to show
// the conclusion does not depend on the loading.
const FOAM_EPS: Array<[string, Cx]> = [
  ["light  (1.5 - j0.3)", cx(1.5, -0.3)],
  ["design (2.0 - j1.0)", cx(2.0, -1.0)],
  ["heavy  (3.0 - j3.0)", cx(3.0, -3.0)],
  ["absurd (6.0 - j12 )", cx(6.0, -12.0)],
];

// ── Thermal, for §7 — the half that decides what REPLACES the layer ──────────
// NP-THERM-COOL-001 §2. The station is 3.0 mm; what occupies it decides the term.
// The trap: vacating it does not delete the resistance, it fills it with stagnant
// air, and air is a WORSE insulator per mm than the foam.
// Constants shared with every thermal script since OI-THCOOL-21 (GitHub #407),
// so this table and the network models cannot fork.
const THERM = {
  outwardTotal: TOP.R_OUT_R1, // m2K/W, AS-WAS (foam in) — §7/§8.5 argue from it; HISTORICAL
  outwardCurrent: TOP.R_OUT_CURRENT, // 0.335 — station deleted, 3 mm re-loft (REQ-CAV-04)
  stationT: TOP.ABSORBER_T_M, // 3.0 mm
  kFoam: TOP.K_ABSORBER, // carbon-loaded open-cell, NP-THERM-COOL-001 §2
  kAir: TOP.K_AIR, // stagnant air, same table's 6 mm gap term
  rSubstitution: TOP.R_STATION_SUBSTITUTED, // ceramic-filled elastomer target, §6.3 / OI-THCOOL-04
};
const rStation = (k: number) => THERM.stationT / k;

// ── Clamp load path, for §8.3 — NP-HELMET-GEOM-001 §2 tolerances ─────────────
// The absorber sits at station L2, OUTBOARD of the Gap the clamps live in, so
// the re-loft takes no clamp space. But the foam is also a +/-0.5 tolerance
// CONTRIBUTOR, so deleting it SHRINKS the stack the spring plungers must cover.
const CLAMP_TOL = {
  features: 0.3, // cluster-clamp + lever features on L1's outer face
  gap: 0.5, // inter-bowl clamp travel + blind-mate boss + labyrinth lip
  absorber: 0.5, // the station under audit — a contributor, not just a filler
};
// ── The Gap itself, for §8.5 — FLUSH-1 (NP-HEX-ZM-001 §5.4a, 2026-09-20) ─────
// The lever throws only with the bowls separated (§5.2: "reached by unclamping
// the bowls"), so "clamp travel" is not an assembled-state requirement and the
// 5-7 mm was never derived. The gap is stagnant air and the LARGEST outward term.
const GAP = { nominalMm: 6, rangeMm: [5, 7] as const, kAir: 0.026 };
const rGap = (mm: number) => mm / 1000 / GAP.kAir;
/** What one millimetre of gap is worth on the outward path. */
const gapPerMm = () => rGap(1);
/** Outward total with the gap re-dimensioned to `mm`, everything else held. */
/** Outward total at a given gap, AS-WAS (foam in). HISTORICAL: §8.5/§8.7's
 *  published figures were stated on this baseline before the deletion. */
const outwardAtGap = (mm: number) => THERM.outwardTotal - rGap(GAP.nominalMm) + rGap(mm);
/** The same, CURRENT (station deleted). OI-THCOOL-21. */
const outwardAtGapNow = (mm: number) => THERM.outwardCurrent - rGap(GAP.nominalMm) + rGap(mm);

// ── Dimensioning the Gap, for §8.7 ───────────────────────────────────────────
// FLUSH-1 removed travel, §8.6.1 removed the fluxgates, BOSS-1 removed the boss.
// What was NEVER on the contributor list is what actually sets the floor:
// NP-DRV-SHELL-002 §4.1 puts the CLUSTER CONTROLLER COMPONENTS on L1's
// GAP-FACING face. Package heights below are JEDEC maxima for the package types
// those documents name; the two `assumed` entries are NOT stated anywhere.
const GAP_FLOOR = {
  // sourced: package types named in NP-DRV-SHELL-002 §10.1 / NP-HW-HUB-001 §8.1
  parts: [
    ["STM32G071 UFQFPN32", 0.6],
    ["PCA9548A TSSOP-24", 1.2],
    ["TMUX1308-class mux, TSSOP", 1.2],
    ["zero-drift op-amp, SOT-23-5", 1.45],
    ["passives 0402/0603", 0.55],
  ] as Array<[string, number]>,
  // PCB-1 (principal, 2026-09-21, NP-DRV-SHELL-002 §3.2): 0.80 mm ±0.08, 4-layer
  // rigid-flex. Was an unstated assumption of 0.8-1.0; now a decision.
  pcb: 0.8,
  pcbTol: 0.08,
  clearanceAssumed: [0.5, 1.0] as const, // assembly/tolerance — STILL NOT stated
};
const tallestPart = () => Math.max(...GAP_FLOOR.parts.map(([, h]) => h));
/** Component plane on L1's gap-facing face: PCB-1 plus the tallest named package. */
const componentPlane = () => GAP_FLOOR.pcb + tallestPart();
/**
 * PLATE-1 (principal, 2026-09-21): the plate POCKETS, so the closed-state stack is
 * max(component plane, plate plane) — not their sum. Only the clearance term is
 * still an assumption.
 */
const gapFloorPocketed = () =>
  GAP_FLOOR.clearanceAssumed.map((c) => componentPlane() + c);
/** What the plate would cost if it sat OVER the parts instead of pocketing. */
const gapFloorStacked = (plateMm: number) =>
  GAP_FLOOR.clearanceAssumed.map((c) => componentPlane() + plateMm + c);

// ── PACK-1, for §8.8 — boss vs board on the cluster mirror axis ──────────────
// The cluster-clamp boss wants an INTERNAL vertex (mid-span stiffness,
// NP-HELMET-GEOM-001 §3); BOARD-1 wants the PAN-facing PERIMETER (minimum
// bending moment under PLATE-1). On a hex lattice those are one hex edge apart,
// both on the cluster's mirror axis — so both keep SYM-1 and neither moves.
// Hex edge a = W/sqrt(3); NP-HEX-ZM-001 §5.4a calls 23.09 mm "one hex edge".
//
// ⚠ DO NOT MEASURE THIS LATTICE WITH 3-SPACE DISTANCES. The socket map gives
// xMm/yMm/zMm on a DOUBLY-CURVED scanned surface, so a straight-line chord
// between two socket centres UNDERSTATES their on-surface spacing — badly where
// the surface turns over. Row 11's z swings 21.6 mm across four sockets, and its
// chords read 24-25 mm; a 25 mm chord for a 40 mm geodesic implies a local
// radius of ~13 mm, which is the rim fold, not a compressed lattice.
// Rev 11 of NP-EMC-CAV-001 made exactly that error and published a "lattice
// compresses to 13.9 mm at the rim" caveat. Rev 12 withdrew it. An earlier pass
// made the same mistake a different way (naive nearest-neighbour across rows of
// differing width). Same root cause both times.
//
// The governing constraint is not a measurement at all, it is the PART:
// the tile is ONE UNIVERSAL 40 mm MOULD, type-agnostic, identical shape
// (NP-ART-001 A1, NP-HEX-ZM-001 §627, NP-DT-001 DI-USE-05). Modules are
// interchangeable, so every hexagon is the same size BY CONSTRUCTION — and
// adjacent socket centres are therefore 40 mm apart ON THE SURFACE everywhere.
// They cannot be closer: the parts would overlap.
//
// ⚠ AND THE MODULE SURFACE IS NOT FLAT. NP-HEX-ZM-001 §3.1 committed
// "Option A (rigid, MEDIAN-CURVED 40 mm hexagon)": one compromise curvature,
// R_m ~ 87 mm, 3.1 mm dome depth at W = 40 — chosen to minimise the mismatch
// between module and site curvature across ALL socket positions, which is what
// its "worst-case mismatch" column tabulates. The residual is already bounded
// and already allocated: 1.04 mm worst case, ~0.25 mm over most of the vault,
// "absorbed by the PDMS window standoff + a <=0.8 mm compliant gasket".
// Rev 12 of NP-EMC-CAV-001 argued from FLAT hexagons and concluded the gaps
// widen with curvature, giving PACK-1 free clearance. Wrong premise, and the
// conclusion does not survive it: the mismatch is taken up RADIALLY at L0, not
// in-plane as gap width. Rev 13 withdrew it.
const HEX = {
  nominalWmm: 40, // hardware/np_socket_map.json geometry.moduleWidthMm
  moduleRadiusMm: 87, // R_m, the compromise curvature (NP-HELMET-GEOM-001 §1)
  domeDepthMm: 3.1, // at W = 40 (NP-HEX-ZM-001 §3.1)
  mismatchWorstMm: 1.04, // §3.1, absorbed by standoff + <=0.8 mm gasket
  mismatchTypicalMm: 0.25, // §3.1, "most of the vault"
};
/** The hex edge as a CHORD across the module's own curved surface. */
const hexEdgeChord = (wMm: number, rMm: number) =>
  2 * rMm * Math.sin(hexEdge(wMm) / (2 * rMm));
const hexEdge = (wMm: number) => wMm / Math.sqrt(3);
/** Boss at a from centre, board at 2a — separation is exactly one hex edge. */
const packSeparation = (wMm: number) => hexEdge(wMm);

const clampStack = (withAbsorber: boolean) => {
  const t = [CLAMP_TOL.features, CLAMP_TOL.gap, ...(withAbsorber ? [CLAMP_TOL.absorber] : [])];
  return { worst: t.reduce((a, b) => a + b, 0), rss: Math.sqrt(t.reduce((a, b) => a + b * b, 0)) };
};

// ── The allocation ───────────────────────────────────────────────────────────
// SH2-DRC-16 holds EEG artifact below 5 uVpp with all LEDs at full PWM load.
// RF demodulation is allocated 20 % of that budget because, unlike the
// therapeutic-band self-field, REQ-EMI-05's feed-forward model cannot predict it:
// it moves with posture, hand proximity and configuration, so it is not
// subtractable and must be held by the enclosure instead.
const EEG_BUDGET_UVPP = 5.0; // SH2-DRC-16
const RF_DEMOD_SHARE = 0.2;
const EMIRR_DB = 60; // ADS1299 has no EMIRR spec; 60 dB is the design assumption
const PICKUP_LEN_M = 0.04; // in-tile electrode lead; N4 beyond it is guarded (REQ-EMI-02)
const CMRR_AT_RF_DB = 20; // the 110 dB CMRR spec is a 50/60 Hz figure, not an RF one
const Q_CEILING = 20; // REQ-CAV-02 allocation

function fieldLimit(emirrDb = EMIRR_DB) {
  const dvUv = EEG_BUDGET_UVPP * RF_DEMOD_SHARE; // uV allowed from demodulation
  const vRfMax = (dvUv * 1e-6) * 10 ** (emirrDb / 20); // volts at the amplifier input
  const hEff = PICKUP_LEN_M / 2; // electrically short: h_e ~ l/2
  const coupling = hEff * 10 ** (-CMRR_AT_RF_DB / 20); // V per (V/m)
  return { dvUv, vRfMax, hEff, coupling, eMax: vRfMax / coupling };
}

// OI-EMCCAV-01 (#398) — how REQ-CAV-01, and the margin behind REQ-CAV-04, move
// with the one input nobody has measured. The ADS1299 datasheet gives no EMIRR;
// NP-EMC-EMIRR-001 is the bench procedure that will. Until it runs, this is a
// sensitivity statement over candidate values, not a result.
//
// REQ-CAV-01 is linear in 10^(EMIRR/20): one decade per 20 dB. REQ-CAV-02 is an
// ALLOCATION of that field limit to the enclosure, as a Q ceiling. The scaling
// below HOLDS THE SOURCE/LAYOUT SHARE (REQ-EMI-01..07) FIXED, so the whole change
// in field limit lands on the enclosure's Q ceiling, dB for dB — resonant peak
// field is proportional to Q. That is the conservative reading (the enclosure
// absorbs all of it); whether to re-allocate instead is an EMC/principal call and
// is not made here. Evaluated at the mid-range head's lowest mode, like every
// other single-number anchor in this file.
const EMIRR_SWEEP_DB = [40, 50, 60, 70];

function emirrSensitivity(emirrDb: number) {
  const m = midCavity();
  const fl = fieldLimit(emirrDb);
  const qCeil = Q_CEILING * 10 ** ((emirrDb - EMIRR_DB) / 20);
  const q = qLoaded(m.f, PD_FABRIC_RS.nominal);
  const foam = slabSurfaceR(m.f, ABSORBER_T_M, FOAM_EPS[1][1]);
  const qWithFoam = qLoaded(m.f, PD_FABRIC_RS.nominal, foam.Rs);
  return {
    eMax: fl.eMax,
    qCeil,
    headMarginDb: 20 * Math.log10(qCeil / q.loaded),
    // What the deleted absorber would add ON TOP of the head, at design loading.
    // Independent of EMIRR — the point of carrying it in the sweep.
    foamWithHeadDb: 20 * Math.log10(q.loaded / qWithFoam.loaded),
  };
}

// EMIRR at which the head's margin against the scaled Q ceiling reaches zero.
const emirrBreakEven = () => {
  const q = qLoaded(midCavity().f, PD_FABRIC_RS.nominal);
  return EMIRR_DB - 20 * Math.log10(Q_CEILING / q.loaded);
};
// ── §6.5 — EXTERNAL 6 GHz Wi-Fi ingress through the parting-plane seam ──────
// (`OI-EMCCAV-06`.) §4.2's upper edge is set by the INTERNAL source's roll-off
// and says nothing about an external source that does not roll off. This block
// works that case to §6's first-order standard, against the POST-DELETION
// geometry (REQ-CAV-04 taken: the liner sits on the air region, no absorber).

// Band: FCC 20-51 (ET Docket 18-295) opened U-NII-5 .. U-NII-8, 5.925-7.125 GHz.
const WIFI6E_HZ = { lo: 5.925e9, mid: 6.525e9, hi: 7.125e9 };

// Incident field. The EIRP ceilings are the regulatory maxima for a CLIENT
// (a phone or laptop near the wearer) under 47 CFR §15.407 as amended by
// FCC 20-51: low-power-indoor client 24 dBm, standard-power client 30 dBm
// (6 dB under the 36 dBm AP). 0.3 m is IEC 60601-1-2:2014+A1:2020 Table 9's
// proximity distance; 0.1 m is a phone held against the headset.
const WIFI_CLIENT = {
  lpiDbm: 24, // FCC 20-51 LPI client ceiling
  spDbm: 30, // FCC 20-51 standard-power client ceiling
  refDistM: 0.3, // IEC 60601-1-2 Table 9 proximity distance
  nearDistM: 0.1, // handset against the helmet — bounding case
  table9Vpm: 9, // IEC 60601-1-2 Table 9, 5100-5800 MHz (the band it stops at)
};
const dbmToW = (dbm: number) => 10 ** (dbm / 10) / 1000;
/** Far-field rms E from EIRP: E = sqrt(30 * EIRP) / d. */
const incidentE = (eirpDbm: number, dM: number) => Math.sqrt(30 * dbmToW(eirpDbm)) / dM;

// Dry skin, Gabriel et al. 1996 (Phys. Med. Biol. 41, part III) 4-Cole-Cole
// parameters. §6.3 uses a spot value at 460 MHz; at 6-7 GHz the dispersion is
// steep enough that the model, not a spot value, is the honest input.
const GABRIEL_SKIN_DRY = {
  epsInf: 4.0,
  sigmaIonic: 0.0002,
  poles: [
    { dEps: 32.0, tau: 7.234e-12, alpha: 0.0 },
    { dEps: 1100, tau: 32.481e-9, alpha: 0.2 },
  ],
};
function gabrielSkin(f: number) {
  const w = 2 * Math.PI * f;
  let e = cx(GABRIEL_SKIN_DRY.epsInf);
  for (const p of GABRIEL_SKIN_DRY.poles) {
    // dEps / (1 + (j w tau)^(1 - alpha))
    const mag = (w * p.tau) ** (1 - p.alpha);
    const ph = ((1 - p.alpha) * Math.PI) / 2;
    const den = cx(1 + mag * Math.cos(ph), mag * Math.sin(ph));
    const t = cdiv(cx(p.dEps), den);
    e = cx(e.re + t.re, e.im + t.im);
  }
  // e is eps' - j eps'' in the e^{jwt} convention; add the ionic term.
  const epsC = cx(e.re, e.im - GABRIEL_SKIN_DRY.sigmaIonic / (w * EPS0));
  return { epsC, epsR: epsC.re, sigma: -epsC.im * w * EPS0 };
}

/** Post-deletion geometry: the conductor sits directly on the air region. */
function postDeletionCavity(circ = (HEAD_CIRC_M.min + HEAD_CIRC_M.max) / 2) {
  const aHead = headRadius(circ);
  const t = (airThickness("min") + airThickness("max")) / 2;
  const aShield = aHead + t;
  return {
    aHead, aShield, t,
    V: shellVolume(aHead, aShield),
    sHead: sphereArea(aHead),
    sShield: sphereArea(aShield),
    rimPerimeter: 2 * Math.PI * aShield, // the parting-plane seam runs round the mouth
  };
}

/**
 * Plane-wave power absorptivity of a surface at incidence theta, TE and TM,
 * given its input impedance for each polarisation. Free-space reference
 * impedances are eta0/cos (TE) and eta0*cos (TM).
 */
function absorptivity(zTE: Cx, zTM: Cx, th: number) {
  const c = Math.cos(th);
  const a = (z: Cx, z0: number) => {
    const g = cdiv(cx(z.re - z0, z.im), cx(z.re + z0, z.im));
    return 1 - (g.re * g.re + g.im * g.im);
  };
  return (a(zTE, ETA0 / c) + a(zTM, ETA0 * c)) / 2;
}
/** Wave impedances in a medium of relative permittivity e for a given sin(theta) in air. */
function mediumZ(f: number, e: Cx, s: number) {
  const k0 = (2 * Math.PI * f) / C0;
  const kz = cscale(csqrt(cx(e.re - s * s, e.im)), k0); // k0 sqrt(e - sin^2)
  const w = 2 * Math.PI * f;
  const zTE = cdiv(cx(w * MU0), kz);
  const zTM = cdiv(kz, cscale(e, w * EPS0));
  return { kz, zTE, zTM };
}
/** Diffuse-field (cosine-weighted, 2*sin*cos) average of a theta-dependent absorptivity. */
function diffuseAverage(fn: (th: number) => number, n = 400) {
  let acc = 0;
  for (let i = 0; i < n; i++) {
    const th = ((i + 0.5) / n) * (Math.PI / 2);
    acc += fn(th) * Math.sin(2 * th) * (Math.PI / 2 / n);
  }
  return acc;
}
/** A lossy half-space (the head, first-order: homogeneous dry skin). */
function halfSpaceAbsorptivity(f: number, e: Cx, th?: number) {
  const one = (t: number) => {
    const m = mediumZ(f, e, Math.sin(t));
    return absorptivity(m.zTE, m.zTM, t);
  };
  return th === undefined ? diffuseAverage(one) : one(th);
}
/** A lossy slab of thickness d on a conductor (the deleted Layer 4, for comparison). */
function slabOnPecAbsorptivity(f: number, e: Cx, d: number, th?: number) {
  const one = (t: number) => {
    const m = mediumZ(f, e, Math.sin(t));
    const tn = ctan(cscale(m.kz, d));
    const j = cx(0, 1);
    return absorptivity(cmul(j, cmul(m.zTE, tn)), cmul(j, cmul(m.zTM, tn)), t);
  };
  return th === undefined ? diffuseAverage(one) : one(th);
}
/** A good-conductor wall of surface resistance Rs: a ~ 4 Rs / eta0 at normal incidence (the larger, so the wall gets more credit than it earns). */
const wallAbsorptivity = (rs: number) => (4 * rs) / ETA0;

/**
 * Composite Q of an OVERMODED cavity (diffuse field): Q = 8 pi V / (lambda * sum S_i a_i).
 * Hill 1994 / IEC 61000-4-21 Annex. At 6 GHz this cavity has hundreds of modes
 * and they overlap (below), so a per-mode Q is not defined; this is.
 */
function diffuseQ(f: number, absorptionAreaM2: number, V: number) {
  return (8 * Math.PI * V) / ((C0 / f) * absorptionAreaM2);
}
/** Weyl mode density and the modal-overlap factor M = (dN/df) * (f/Q). */
function modalOverlap(f: number, V: number, Q: number) {
  const dNdf = (8 * Math.PI * V * f * f) / C0 ** 3;
  return { dNdf, modesBelow: (8 * Math.PI * V * f ** 3) / (3 * C0 ** 3), M: dNdf * (f / Q) };
}

/**
 * Seam transmission cross-sections, diffuse-averaged.
 *  - continuous residual slot of width w along the whole rim: <sigma_t> = P*w/2
 *    (the electrically-large-aperture limit, half the area over 4 pi incidence);
 *  - n discrete circular apertures of diameter L: Hill 1994's small-aperture
 *    <sigma_t> = 16 k^4 a^6 / (9 pi) each, a = L/2.
 */
const SEAM = {
  residualMm: 2.5, // NP-HEX-ZM-001 §5.3a: lambda/20 at 6 GHz
};
const sigmaContinuousSlot = (P: number, wM: number) => (P * wM) / 2;
const sigmaSmallAperture = (f: number, dM: number) => {
  const k = (2 * Math.PI * f) / C0;
  const a = dM / 2;
  return (16 * k ** 4 * a ** 6) / (9 * Math.PI);
};
/** Internal rms field ratio: E_c / E_inc = sqrt(lambda * Q * sigma_t / (2 pi V)). Hill 1994. */
const internalFieldRatio = (f: number, Q: number, sigma: number, V: number) =>
  Math.sqrt(((C0 / f) * Q * sigma) / (2 * Math.PI * V));
/**
 * Peak single-component field at the electrode, from the rms total: sqrt(2)
 * for the carrier crest, x sqrt(ln 100) / sqrt(3) for the 99th percentile of a
 * Rayleigh-distributed rectangular component of a diffuse field.
 */
const PEAK_FACTOR = Math.SQRT2 * Math.sqrt(Math.log(100)) / Math.sqrt(3);

function wifiCase(f = WIFI6E_HZ.mid, headVisible = 1) {
  const c = postDeletionCavity();
  const skin = gabrielSkin(f);
  const aHead = halfSpaceAbsorptivity(f, skin.epsC);
  const aHeadNormal = halfSpaceAbsorptivity(f, skin.epsC, 0);
  const aWall = wallAbsorptivity(PD_FABRIC_RS.nominal);
  const aFoam = slabOnPecAbsorptivity(f, FOAM_EPS[1][1], ABSORBER_T_M);
  const areaHead = headVisible * c.sHead * aHead;
  const areaWall = c.sShield * aWall;
  const areaFoam = c.sShield * aFoam; // what the deleted layer WOULD add
  const Q = diffuseQ(f, areaHead + areaWall, c.V);
  const Qfoam = diffuseQ(f, areaHead + areaWall + areaFoam, c.V);
  const Qwall = diffuseQ(f, areaWall, c.V);
  // §6's surface-impedance formula, for comparison (G = 0.5, linearised).
  const qSurf = qFromBoundary(f, c.V, c.sHead, tissueSurfaceR(f, skin.epsR, skin.sigma).Rs);
  return { c, skin, aHead, aHeadNormal, aWall, aFoam, Q, Qfoam, Qwall, qSurf, areaHead, areaWall, areaFoam };
}

/**
 * Fraction of the head that must be RF-visible through the module field for the
 * design-intent seam to hold REQ-CAV-01 at incident field eInc. Q goes as
 * 1/(visible head area) once the head dominates, so field goes as 1/sqrt(f_v).
 */
function visibleFractionForLimit(eInc: number, f = WIFI6E_HZ.mid) {
  const w = wifiCase(f, 1);
  const fr = wifiField(f, w.Q, w.c.V, w.c.rimPerimeter);
  const eMax = fieldLimit().eMax;
  const ePk = fr.holes * eInc * PEAK_FACTOR;
  // solve (areaHead*fv + areaWall) = (areaHead + areaWall) * (ePk/eMax)^2
  const need = (w.areaHead + w.areaWall) * (ePk / eMax) ** 2;
  return (need - w.areaWall) / w.areaHead;
}

/**
 * EMIRR at which the 6E internal field just meets REQ-CAV-01 (§5.4's sensitivity,
 * applied to §6.5). REQ-CAV-01 is linear in 10^(EMIRR/20), so the break-even is
 * the 60 dB design assumption minus the margin in dB.
 */
function emirrBreakEven6E(ePkVpm: number) {
  return EMIRR_DB - 20 * Math.log10(fieldLimit().eMax / ePkVpm);
}

function wifiField(f: number, Q: number, V: number, P: number) {
  const slot = internalFieldRatio(f, Q, sigmaContinuousSlot(P, SEAM.residualMm / 1000), V);
  const nPath = Math.floor(P / (SEAM.residualMm / 1000)); // pathological: a hole every 2.5 mm
  const holes = internalFieldRatio(f, Q, nPath * sigmaSmallAperture(f, SEAM.residualMm / 1000), V);
  return { slot, holes, nPath };
}

/** Visible head fraction at which the head's absorption equals what the deleted foam's would have been. */
function visibleFractionFoamParity(f = WIFI6E_HZ.mid) {
  const w = wifiCase(f, 1);
  return w.areaFoam / w.areaHead;
}

// ── Reporting ────────────────────────────────────────────────────────────────
const f3 = (n: number, d = 1) => n.toFixed(d);
const mhz = (f: number) => `${f3(f / 1e6)} MHz`;

function midCavity() {
  // The mid-range head, used for every single-number anchor below.
  const circ = (HEAD_CIRC_M.min + HEAD_CIRC_M.max) / 2;
  const aHead = headRadius(circ);
  const tAir = (airThickness("min") + airThickness("max")) / 2;
  const tCav = (cavityThickness("min") + cavityThickness("max")) / 2;
  const aShield = aHead + tCav;
  const V = shellVolume(aHead, aShield);
  const sShield = sphereArea(aShield);
  const sHead = sphereArea(aHead);
  const f = C0 / (2 * Math.PI * (aHead + tAir / 2));
  return { circ, aHead, aShield, tAir, tCav, V, sShield, sHead, f };
}

function qLoaded(f: number, fabricRs: number, foamRs = 0) {
  const m = midCavity();
  const wallQ = qFromBoundary(f, m.V, m.sShield, fabricRs + foamRs);
  const skin = tissueSurfaceR(f, SKIN_500MHZ.epsR, SKIN_500MHZ.sigma);
  const headQ = qFromBoundary(f, m.V, m.sHead, skin.Rs);
  return { wallQ, headQ, loaded: 1 / (1 / wallQ + 1 / headQ), skinRs: skin.Rs };
}

function reportBand() {
  console.log(`\n=== 1. THE BAND — and it is a function of the wearer =======================\n`);
  console.log(`  Cavity = scalp -> Pd-polyester liner (NP-HELMET-GEOM-001 §2):`);
  console.log(`    L1 subtotal        ${L1_SUBTOTAL_M.min * 1e3}-${L1_SUBTOTAL_M.max * 1e3} mm`);
  console.log(`    inter-bowl gap      ${INTERBOWL_GAP_M.min * 1e3}-${INTERBOWL_GAP_M.max * 1e3} mm`);
  console.log(`    absorber (EMF L4)   ${ABSORBER_T_M * 1e3} mm  <- the layer under audit`);
  console.log(`    -> air region       ${f3(airThickness("min") * 1e3)}-${f3(airThickness("max") * 1e3)} mm`);
  console.log(`    -> to conductor     ${f3(cavityThickness("min") * 1e3)}-${f3(cavityThickness("max") * 1e3)} mm\n`);

  const small = lowestMode(HEAD_CIRC_M.min, "min");
  const large = lowestMode(HEAD_CIRC_M.max, "max");
  console.log(`  Lowest supported mode — one wavelength round the mean circumference:`);
  console.log(`    52 cm head: r_m ${f3(small.rMean * 1e3)} mm, C ${f3(small.cMean * 1e3)} mm -> ${mhz(small.f)}`);
  console.log(`    62 cm head: r_m ${f3(large.rMean * 1e3)} mm, C ${f3(large.cMean * 1e3)} mm -> ${mhz(large.f)}`);
  console.log(`\n    The enclosure's resonant frequency is a property of the PERSON WEARING IT.`);
  console.log(`    One SKU covers 52-62 cm (CLAUDE.md §4.4), so the lower edge sweeps 84 MHz`);
  console.log(`    across the population. No document records this. It is why a requirement`);
  console.log(`    at a single frequency would have been wrong even if one had been written.\n`);

  const rMin = firstRadialMode("max");
  const rMax = firstRadialMode("min");
  console.log(`  First RADIAL mode of the air layer (the next family up):`);
  console.log(`    quarter-wave bound  ${f3(rMin.quarterWave / 1e9, 2)}-${f3(rMax.quarterWave / 1e9, 2)} GHz  <- binding`);
  console.log(`    half-wave bound     ${f3(rMin.halfWave / 1e9, 2)}-${f3(rMax.halfWave / 1e9, 2)} GHz\n`);

  console.log(`  Upper edge, set by the SOURCE rather than the cavity:`);
  for (const [name, tr] of Object.entries(EDGE_TIME_S)) {
    console.log(
      `    t_r ${tr * 1e9} ns -> knee ${mhz(knee(tr))};  at 500 MHz ${f3(sourceRolloffDb(500e6, tr))} dB,` +
        `  at 3 GHz ${f3(sourceRolloffDb(3e9, tr))} dB`,
    );
  }
  const drop =
    sourceRolloffDb(3e9, EDGE_TIME_S.slow) - sourceRolloffDb(500e6, EDGE_TIME_S.slow);
  console.log(`\n    The source falls ${f3(-drop)} dB between the lowest mode and 3 GHz, which`);
  console.log(`    exceeds any plausible resonant enhancement differential. BAND: 420 MHz - 3 GHz.`);
  console.log(`    Revisit if a faster part or a switching converter lands inside the envelope`);
  console.log(`    (OI-HUB-C19 provisionally sites the 24 V boost on the Hub PCB — outside).`);
}

function reportSource() {
  console.log(`\n=== 2. THE SOURCE — named, and it is not a radio ===========================\n`);
  console.log(`  NP-DRV-SHELL-002 places all of this INSIDE the envelope, on L1:`);
  console.log(`    18x STM32G071 cluster controllers, 64 MHz core            §3.2`);
  console.log(`    400 kHz I2C tree, 32 segments, ~18 ms per 100 ms tick     §3.4, NP-HW-HUB-001`);
  console.log(`    80 PWM LED drivers, ~20 kHz carrier, 24 V rail            §9.2, §5.4`);
  console.log(`    ADS1299 bank + SPI at the posterior aggregation node      §3.5`);
  console.log(`\n  §9.6 already states the consequence in terms:`);
  console.log(`    "A Faraday cage does not protect the EEG electrodes and fluxgates that`);
  console.log(`     share the enclosure with the source."`);
  console.log(`\n  So NP-BIB-EMF-001 §7.3's premise — the radios live in the hub, therefore`);
  console.log(`  nothing excites the cavity — is WRONG, and §7.3 said so itself in its own`);
  console.log(`  caveat. The exciting source is digital edges, and it was always there.`);
}

function reportAllocation() {
  const fl = fieldLimit();
  console.log(`\n=== 3. THE REQUIREMENT — derived, then allocated ===========================\n`);
  console.log(`  Victim: the ADS1299 front end and the fluxgates, inside the cavity with the`);
  console.log(`  source. The mechanism is RF DEMODULATION — cavity RF rectified in the PGA`);
  console.log(`  input to a DC/LF offset in a uV front end. Unlike the therapeutic-band self`);
  console.log(`  field, it is NOT subtractable: REQ-EMI-05's feed-forward model predicts`);
  console.log(`  commanded current, not a demodulated offset that moves with posture and hand`);
  console.log(`  proximity. So the enclosure has to hold it.\n`);
  console.log(`    SH2-DRC-16 budget                 ${f3(EEG_BUDGET_UVPP)} uVpp`);
  console.log(`    allocated to RF demodulation      ${RF_DEMOD_SHARE * 100} %  -> ${f3(fl.dvUv)} uVpp`);
  console.log(`    EMIRR (design assumption)         ${EMIRR_DB} dB`);
  console.log(`    -> V_RF at amplifier input        ${f3(fl.vRfMax * 1e3, 2)} mV`);
  console.log(`    in-tile pickup ${PICKUP_LEN_M * 1e3} mm -> h_e       ${f3(fl.hEff * 1e3)} mm`);
  console.log(`    CM->DM conversion at RF           ${CMRR_AT_RF_DB} dB`);
  console.log(`    -> coupling                       ${f3(fl.coupling * 1e3, 2)} mV per (V/m)`);
  console.log(`\n    REQ-CAV-01:  E <= ${f3(fl.eMax, 2)} V/m peak in band, at the electrode plane.\n`);
  console.log(`  REQ-CAV-02 allocates the ENCLOSURE's share of that as a Q ceiling, because Q`);
  console.log(`  is the only term the enclosure controls — source and layout are already owned`);
  console.log(`  by REQ-EMI-01..07. Q_L <= ${Q_CEILING} caps resonant enhancement at`);
  console.log(`  ${f3(20 * Math.log10(Q_CEILING))} dB.`);
  console.log(`\n  This IS an allocation and is labelled one. The finding below does not depend`);
  console.log(`  on it: at Q_L <= 5 or Q_L <= 50 the head still meets it and the foam still`);
  console.log(`  does not move it.`);
}

function reportEmirr() {
  console.log(`\n=== 3a. EMIRR SENSITIVITY — OI-EMCCAV-01, unmeasured ======================\n`);
  console.log(`  ${EMIRR_DB} dB is a design assumption. Candidate values, source/layout share held fixed:\n`);
  console.log(`    EMIRR   REQ-CAV-01     Q ceiling   head margin   foam on top of head`);
  for (const e of EMIRR_SWEEP_DB) {
    const r = emirrSensitivity(e);
    console.log(
      `    ${String(e).padStart(3)} dB  ${f3(r.eMax, 3).padStart(7)} V/m  ${f3(r.qCeil, 2).padStart(9)}  ${f3(r.headMarginDb).padStart(8)} dB  ${f3(r.foamWithHeadDb, 3).padStart(9)} dB`,
    );
  }
  console.log(`\n  Break-even: the head's margin reaches 0 dB at EMIRR = ${f3(emirrBreakEven())} dB.`);
  console.log(`  Below that the allocation, not Layer 4, is what has to move: the absorber adds`);
  console.log(`  ${f3(emirrSensitivity(EMIRR_DB).foamWithHeadDb, 3)} dB with the head fitted at every EMIRR.`);
}

function reportQ() {
  const m = midCavity();
  console.log(`\n=== 4. Q — bare, then with a head in it ====================================\n`);
  console.log(`  Mid-range head (${f3(m.circ * 100)} cm), at ${mhz(m.f)}:`);
  console.log(`    a_head ${f3(m.aHead * 1e3)} mm · a_shield ${f3(m.aShield * 1e3)} mm`);
  console.log(`    V ${f3(m.V * 1e3, 2)} L · S_shield ${f3(m.sShield * 1e4, 1)} cm2 · S_head ${f3(m.sHead * 1e4, 1)} cm2`);
  console.log(`    head is ${f3((m.sHead / (m.sHead + m.sShield)) * 100)} % of the cavity boundary area\n`);

  console.log(`  Bare-wall Q (Pd-polyester only, head treated as LOSSLESS):`);
  for (const [name, rs] of Object.entries(PD_FABRIC_RS)) {
    console.log(`    R_s ${String(rs).padStart(5)} Ohm/sq  ->  Q ${f3(qFromBoundary(m.f, m.V, m.sShield, rs), 0).padStart(6)}   (${name})`);
  }
  const skin = tissueSurfaceR(m.f, SKIN_500MHZ.epsR, SKIN_500MHZ.sigma);
  console.log(`\n  Now put the wearer in it. Dry skin at ${mhz(m.f)} (e_r ${SKIN_500MHZ.epsR},`);
  console.log(`  sigma ${SKIN_500MHZ.sigma} S/m — the LEAST lossy tissue that could bound this cavity):`);
  console.log(`    eta = ${f3(skin.eta.re, 1)} + j${f3(skin.eta.im, 1)} Ohm  ->  R_s = ${f3(skin.Rs, 1)} Ohm/sq`);
  console.log(`    that is ${f3(skin.Rs / PD_FABRIC_RS.nominal, 0)}x the fabric wall, over ${f3((m.sHead / (m.sHead + m.sShield)) * 100)} % of the boundary\n`);

  const q = qLoaded(m.f, PD_FABRIC_RS.nominal);
  console.log(`    Q_wall  ${f3(q.wallQ, 0).padStart(6)}`);
  console.log(`    Q_head  ${f3(q.headQ, 2).padStart(6)}`);
  console.log(`    Q_load  ${f3(q.loaded, 2).padStart(6)}   <- REQ-CAV-02 needs <= ${Q_CEILING}`);
  console.log(`\n    Met with ${f3(20 * Math.log10(Q_CEILING / q.loaded))} dB of margin, by the head, in every state where`);
  console.log(`    the §2 sources are energised. That is not a damped resonance. It is an`);
  console.log(`    ABSENT one: at Q ~ ${f3(q.loaded, 1)} the mode is over-damped and does not form.`);
}

function reportAbsorber() {
  const m = midCavity();
  console.log(`\n=== 5. WHAT LAYER 4 ACTUALLY CONTRIBUTES ==================================\n`);
  console.log(`  A lossy slab of thickness d laid against a conductor presents`);
  console.log(`    Z_in = j * eta * tan(k*d)`);
  console.log(`  and in the thin limit tan(kd) -> kd, where eta*k = eta0*k0 EXACTLY, because`);
  console.log(`  eta goes as 1/sqrt(e_r) and k goes as sqrt(e_r). So`);
  console.log(`    Z_in -> j * eta0 * k0 * d`);
  console.log(`  — purely reactive, and INDEPENDENT OF THE LOADING. Absorption is second`);
  console.log(`  order in (d/lambda), and at ${mhz(m.f)} the 3 mm foam is lambda/${f3(C0 / m.f / ABSORBER_T_M, 0)}.\n`);

  for (const f of [m.f, 1e9, 3e9]) {
    console.log(`  at ${mhz(f)}:`);
    for (const [name, eps] of FOAM_EPS) {
      const s = slabSurfaceR(f, ABSORBER_T_M, eps);
      const q0 = qLoaded(f, PD_FABRIC_RS.nominal).wallQ;
      const q1 = qLoaded(f, PD_FABRIC_RS.nominal, s.Rs).wallQ;
      console.log(
        `    ${name}  R_s ${f3(s.Rs * 1e3, 1).padStart(7)} mOhm/sq` +
          `   X_s ${f3(s.Xs, 1).padStart(6)} Ohm` +
          `   ->  wall Q ${f3(q0, 0).padStart(5)} -> ${f3(q1, 0).padStart(5)}` +
          `  (${f3(20 * Math.log10(q0 / q1), 2).padStart(5)} dB)`,
      );
    }
    console.log();
  }
  console.log(`  Against a Pd-polyester wall at ${PD_FABRIC_RS.nominal} Ohm/sq, 3 mm of carbon foam is`);
  console.log(`  worth a fraction of a dB at the lowest mode, and only becomes useful above`);
  console.log(`  ~3 GHz — where the source is already ${f3(-sourceRolloffDb(3e9, EDGE_TIME_S.slow) + sourceRolloffDb(500e6, EDGE_TIME_S.slow))} dB down and the head has`);
  console.log(`  taken the mode out regardless.\n`);
  console.log(`  This is WHY every thin commercial RF absorber is iron- or ferrite-loaded: a`);
  console.log(`  magnetic absorber works in the H-field maximum at a conductor wall, where a`);
  console.log(`  dielectric one sits in the E-field null. And magnetic loading is exactly what`);
  console.log(`  REQ-EMI-10 and NP-BIB-EMF-001 §7.8 forbid in this enclosure.`);
}

function reportVerdict() {
  const m = midCavity();
  const q = qLoaded(m.f, PD_FABRIC_RS.nominal);
  const need = 20 * Math.log10(q.wallQ / Q_CEILING);
  const foam = slabSurfaceR(m.f, ABSORBER_T_M, FOAM_EPS[1][1]);
  const qFoam = qLoaded(m.f, PD_FABRIC_RS.nominal, foam.Rs).wallQ;
  const foamDb = 20 * Math.log10(q.wallQ / qFoam);
  const headDb = 20 * Math.log10(q.wallQ / q.loaded);

  console.log(`\n=== 6. THE VERDICT ========================================================\n`);
  console.log(`  OI-BIBEMF-08 asked for a requirement in dB against a named source and band.`);
  console.log(`  It exists, and it is met by something other than Layer 4:\n`);
  console.log(`    requirement (bare Q ${f3(q.wallQ, 0)} -> Q_L ${Q_CEILING})   ${f3(need).padStart(6)} dB needed`);
  console.log(`    Layer 4 supplies (design loading)      ${f3(foamDb, 2).padStart(6)} dB`);
  console.log(`    the wearer's head supplies             ${f3(headDb).padStart(6)} dB`);
  console.log(`\n  Layer 4 delivers ${f3((foamDb / need) * 100, 1)} % of its own stated job, in the band where`);
  console.log(`  that job exists, against 18 % of the outward thermal path and two blocked`);
  console.log(`  fixes to a BLOCKING OI-SINK-01.`);
  console.log(`\n  NP-BIB-EMF-001 §7.3 offered two outcomes. Neither is quite right:`);
  console.log(`    2a (substitute, keep the RF function) — there is no RF function to keep.`);
  console.log(`    2b (delete because EMC cannot state a requirement) — EMC CAN state one.`);
  console.log(`  The real outcome is 2b for a better reason: the requirement is stated, and`);
  console.log(`  Layer 4 is not what meets it. See NP-EMC-CAV-001 §8.`);
}

function reportThermal() {
  const rFoam = rStation(THERM.kFoam);
  const rAir = rStation(THERM.kAir);
  const T = THERM.outwardTotal;

  console.log(`\n=== 7. WHAT REPLACES IT — and why the re-loft is BINDING ==================\n`);
  console.log(`  §6 says Layer 4 does not earn its place on RF grounds. It does NOT follow`);
  console.log(`  that simply removing it recovers the 18 % OI-BIBEMF-08 assumed, because`);
  console.log(`  vacating a 3 mm station does not delete its resistance — it fills it with`);
  console.log(`  stagnant air, and air is a WORSE insulator per mm than the foam:\n`);
  console.log(`    3 mm foam (k ${THERM.kFoam})   ${rFoam.toFixed(4)} m2K/W`);
  console.log(`    3 mm air  (k ${THERM.kAir})  ${rAir.toFixed(4)} m2K/W` +
    `   <- ${((rAir / rFoam - 1) * 100).toFixed(0)} % WORSE\n`);
  const rows: Array<[string, number, string]> = [
    ["today (foam in place)", T, ""],
    ["delete, gap NOT closed", T - rFoam + rAir, "<- a REGRESSION"],
    ["delete, outer bowl re-lofted 3 mm", T - rFoam, "<- BEST; TAKEN (REQ-CAV-04, #391)"],
    ["substitute (ceramic-filled elastomer)", T - rFoam + THERM.rSubstitution, "fallback if OI-EMCCAV-08 says so"],
  ];
  console.log(`  Outward path (NP-THERM-COOL-001 §2 total ${T}):\n`);
  for (const [label, v, note] of rows) {
    console.log(`    ${label.padEnd(38)} ${v.toFixed(3)}   ${note}`);
  }
  const deltaSub = (T - rFoam + THERM.rSubstitution) - (T - rFoam);
  console.log(`\n  So DELETION AND THE RE-LOFT ARE ONE CHANGE. Deleting without closing the`);
  console.log(`  gap is a regression; deleting with it reaches the best figure available.`);
  console.log(`  The re-loft costs nothing today — no mould is cut (NP-REV-SHELL-001 is`);
  console.log(`  DRAFT, no item signed) and OI-ART-01 already owes a NP-TOOL-SHELL-001`);
  console.log(`  re-scope — and "5-layer" is not a published claim, because nothing is`);
  console.log(`  externally published. An earlier revision charged both to deletion.`);
  console.log(`  REQ-CAV-04 TAKEN 2026-09-23 (GitHub #391): station deleted, re-loft binding.`);
  console.log(`\n  Substitution trails by only ${deltaSub.toFixed(3)} m2K/W, but that gap is CONTACT`);
  console.log(`  resistance, not bulk: a ceramic-filled station is ~${rStation(1.5).toFixed(3)} in bulk, so the`);
  console.log(`  0.020 is the price of pressing a compliant pad onto two curved faces.`);
  console.log(`  Deletion removes the interface, not just the material.`);
  console.log(`\n  The compliant member is NOT the foam: NP-HEX-ZM-001 §5.4a puts preload on`);
  console.log(`  over-center lever-throw cluster clamps with per-module spring plungers`);
  console.log(`  (MECH-2). The foam is incidentally compressible, which is why it obstructs`);
  console.log(`  OI-THCOOL-15's pad — an obstruction, not a function. Whether the clamps`);
  console.log(`  still take up the +/-0.5 stack without it is OI-EMCCAV-08, and it is the`);
  console.log(`  one question left before the re-loft is cut into CAD.`);
}

// ── Published anchors ────────────────────────────────────────────────────────
// Every figure NP-EMC-CAV-001 quotes, re-derived here. --validate fails on drift.
function reportGap() {
  console.log(`\n=== 8. THE GAP — re-opened by FLUSH-1, and it dwarfs the absorber ==========\n`);
  console.log(`  FLUSH-1 (NP-HEX-ZM-001 §5.4a, 2026-09-20): the cluster lever is flush when`);
  console.log(`  closed and throws ONLY with the bowls separated — §5.2 reaches the levers`);
  console.log(`  "by unclamping the bowls". So "inter-bowl clamp TRAVEL" is not an`);
  console.log(`  assembled-state requirement, and the 5-7 mm was never derived from it.\n`);
  console.log(`  The gap is stagnant air and the LARGEST single outward term:`);
  console.log(`    ${rGap(GAP.nominalMm).toFixed(3)} m2K/W at ${GAP.nominalMm} mm = ` +
    `${((rGap(GAP.nominalMm) / THERM.outwardTotal) * 100).toFixed(0)} % of the outward path\n`);
  console.log(`    gap(mm)   R_gap    outward   recovered`);
  for (const mm of [7, 6, 5, 4, 3, 2]) {
    const rec = rGap(GAP.nominalMm) - rGap(mm);
    console.log(`      ${String(mm).padStart(2)}     ${rGap(mm).toFixed(3)}    ${outwardAtGap(mm).toFixed(3)}  (now ${outwardAtGapNow(mm).toFixed(3)})` +
      `     ${rec >= 0 ? "+" : ""}${rec.toFixed(3)}${mm === GAP.nominalMm ? "   (today)" : ""}`);
  }
  console.log(`\n  Each mm is worth ${gapPerMm().toFixed(4)} m2K/W, so narrowing the gap 2 mm recovers`);
  console.log(`  MORE than deleting the entire 3 mm Layer 4 absorber ` +
    `(${(2 * gapPerMm()).toFixed(3)} vs ${rStation(THERM.kFoam).toFixed(3)}).`);
  console.log(`\n  Against NP-THERM-COOL-001 §6.1's sealed recirculation (0.231 -> 0.067,`);
  console.log(`  recovery 0.164), narrowing to 3 mm reaches ~70 % of the prize with NO motor,`);
  console.log(`  no power draw and no moving part inside the sealed cavity. The two INTERACT`);
  console.log(`  rather than compose — a narrower gap is less volume at higher flow`);
  console.log(`  resistance to stir — so they must be traded, not stacked.`);
  console.log(`\n  What sets the assembled gap is the CLOSED lever footprint, the labyrinth`);
  console.log(`  lip, the fluxgates, and the blind-mate boss — and the boss is a STANDALONE`);
  console.log(`  POSTERIOR-CENTRE feature (§5.3c), so it constrains the gap LOCALLY, not`);
  console.log(`  across the vault. A locally-relieved gap is available. Dimensioning it is`);
  console.log(`  MECH-2's, and it is the largest unclaimed thermal lever in the document set.`);
}

function reportGapFloor() {
  console.log(`\n=== 9. DIMENSIONING THE GAP — and the lever is not what sets it ===========\n`);
  console.log(`  Three contributors have left the Gap's requirement list: FLUSH-1 took out`);
  console.log(`  TRAVEL, §8.6.1 took out the FLUXGATES (never in it), BOSS-1 took out the`);
  console.log(`  BOSS. What was never ON the list is what actually sets the floor:`);
  console.log(`  NP-DRV-SHELL-002 §4.1 puts the CLUSTER CONTROLLER COMPONENTS on L1's`);
  console.log(`  gap-facing face — "Cluster controller components, incl. the STM32G071".\n`);
  console.log(`  Package heights (JEDEC maxima for the package types those docs name):`);
  for (const [n, h] of GAP_FLOOR.parts) {
    console.log(`    ${n.padEnd(30)} ${h.toFixed(2)} mm${h === tallestPart() ? "   <- tallest" : ""}`);
  }
  const [lo, hi] = gapFloorPocketed();
  console.log(`\n  Closed-state stack, under PCB-1 and PLATE-1 (both principal, 2026-09-21):`);
  console.log(`    PCB ${GAP_FLOOR.pcb} +/-${GAP_FLOOR.pcbTol}   <- PCB-1, DECIDED (was an assumption)`);
  console.log(`    + tallest part ${tallestPart()}`);
  console.log(`    = component plane ${componentPlane().toFixed(2)} mm`);
  console.log(`    + clearance ${GAP_FLOOR.clearanceAssumed[0]}-${GAP_FLOOR.clearanceAssumed[1]} (STILL ASSUMED, not stated)`);
  console.log(`    = ${lo.toFixed(2)}-${hi.toFixed(2)} mm   against 5-7 mm today\n`);
  console.log(`  PLATE-1 pockets, so this is max(component, plate) and NOT their sum. A`);
  console.log(`  0.8 mm plate sitting OVER the parts would give ${gapFloorStacked(0.8)[0].toFixed(2)} mm instead —`);
  console.log(`  pocketing is worth ${(rGap(gapFloorStacked(0.8)[0]) - rGap(lo)).toFixed(3)} m2K/W, on top of PCB-1's ` +
    `${(rGap(1.0 + tallestPart() + 0.5) - rGap(lo)).toFixed(4)}.\n`);
  console.log(`  Thermal value of landing in that band:`);
  for (const mm of [3.5, 3.0, 2.75]) {
    console.log(`    ${mm.toFixed(2)} mm -> outward ${outwardAtGap(mm).toFixed(3)} as-was, ${outwardAtGapNow(mm).toFixed(3)} now` +
      `   (recovers ${(rGap(GAP.nominalMm) - rGap(mm)).toFixed(3)})`);
  }
  console.log(`\n  TWO of the four missing inputs are now closed: PCB-1 fixes the board at`);
  console.log(`  0.80 mm and PLATE-1 makes the plate pocket. TWO REMAIN, both MECH-2's:`);
  console.log(`  the plate's own structural thickness (it only governs if it exceeds the`);
  console.log(`  ${tallestPart()} mm component plane), and the outer bowl's inner-surface PROFILE`);
  console.log(`  tolerance, which is nowhere in the record and sets the clearance term.`);
}

function reportWifi() {
  const f = WIFI6E_HZ.mid;
  const w = wifiCase(f);
  const c = w.c;
  const fl = fieldLimit();
  console.log(`\n=== 10. EXTERNAL 6 GHz Wi-Fi INGRESS (OI-EMCCAV-06, §6.5) ================\n`);
  console.log(`  Post-deletion cavity, 57 cm head: t ${f3(c.t * 1e3)} mm · V ${f3(c.V * 1e3, 2)} L · rim ${f3(c.rimPerimeter * 1e3, 0)} mm\n`);
  console.log(`  Incident field (47 CFR §15.407 / FCC 20-51 client EIRP ceilings):`);
  for (const [lab, dbm, d] of [
    ["LPI client 24 dBm @ 0.3 m (IEC 60601-1-2 Table 9 distance)", WIFI_CLIENT.lpiDbm, WIFI_CLIENT.refDistM],
    ["SP  client 30 dBm @ 0.3 m", WIFI_CLIENT.spDbm, WIFI_CLIENT.refDistM],
    ["LPI client 24 dBm @ 0.1 m (handset against the helmet)", WIFI_CLIENT.lpiDbm, WIFI_CLIENT.nearDistM],
    ["SP  client 30 dBm @ 0.1 m (bounding)", WIFI_CLIENT.spDbm, WIFI_CLIENT.nearDistM],
  ] as Array<[string, number, number]>) {
    console.log(`    ${lab.padEnd(60)} ${f3(incidentE(dbm, d), 1).padStart(6)} V/m rms`);
  }
  console.log(`\n  Dry skin, Gabriel 1996 4-Cole-Cole:`);
  for (const ff of [460e6, WIFI6E_HZ.lo, WIFI6E_HZ.mid, WIFI6E_HZ.hi]) {
    const s = gabrielSkin(ff);
    console.log(`    ${mhz(ff).padStart(12)}  e_r ${f3(s.epsR, 1)}  sigma ${f3(s.sigma, 2)} S/m`);
  }
  console.log(`    (§6.3's 44 / 0.44 at 460 MHz sits BELOW the model's loss — conservative, as it says.)\n`);
  console.log(`  Head absorptivity at ${mhz(f)}: normal ${f3(w.aHeadNormal, 3)}, diffuse-averaged ${f3(w.aHead, 3)}`);
  console.log(`  Fabric wall absorptivity ${w.aWall.toExponential(2)} · deleted foam (design loading) would be ${f3(w.aFoam, 3)}\n`);
  const mo = modalOverlap(f, c.V, w.Q);
  console.log(`  Modes below ${mhz(f)}: ${f3(mo.modesBelow, 0)} · density ${f3(mo.dNdf * 1e9, 0)} /GHz · overlap M ${f3(mo.M, 1)}`);
  console.log(`    M >> 1: the field is DIFFUSE. There is no discrete resonance for a per-mode`);
  console.log(`    Q ceiling to bound, so REQ-CAV-02 has no meaning here even if its band reached.\n`);
  console.log(`  Composite Q (diffuse, Hill 1994):  wall only ${f3(w.Qwall, 0)} · head in ${f3(w.Q, 1)} · + deleted foam ${f3(w.Qfoam, 1)}`);
  console.log(`  §6's surface-impedance formula would give Q_head ${f3(w.qSurf, 1)} — optimistic here (linearised, G = 0.5).`);
  for (const ff of [WIFI6E_HZ.lo, WIFI6E_HZ.hi]) console.log(`    at ${mhz(ff)}: Q ${f3(wifiCase(ff).Q, 1)}`);
  console.log(`  What the deleted foam would have bought in internal field: ${f3(10 * Math.log10(w.Q / w.Qfoam), 2)} dB`);
  console.log(`  Head fraction visible through the module field at which foam = head: ${f3(visibleFractionFoamParity() * 100, 0)} %\n`);

  const fr = wifiField(f, w.Q, c.V, c.rimPerimeter);
  const eRef = incidentE(WIFI_CLIENT.lpiDbm, WIFI_CLIENT.refDistM);
  const eBound = incidentE(WIFI_CLIENT.spDbm, WIFI_CLIENT.nearDistM);
  const pk = (ratio: number, e: number) => ratio * e * PEAK_FACTOR;
  console.log(`  Internal field, screened against REQ-CAV-01's ${f3(fl.eMax, 2)} V/m peak (peak factor ${f3(PEAK_FACTOR, 2)}):`);
  console.log(`    seam as §5.3a intends — discrete <= ${SEAM.residualMm} mm holes, ${fr.nPath} of them (one every ${SEAM.residualMm} mm):`);
  console.log(`      coupling ${f3(20 * Math.log10(fr.holes), 1)} dB -> ${f3(pk(fr.holes, eRef), 3)} V/m @ ${f3(eRef, 1)} · ${f3(pk(fr.holes, eBound), 3)} V/m @ ${f3(eBound, 1)} V/m`);
  console.log(`    seam with the bead absent or lifted — continuous ${SEAM.residualMm} mm slot round the rim:`);
  console.log(`      coupling ${f3(20 * Math.log10(fr.slot), 1)} dB -> ${f3(pk(fr.slot, eRef), 2)} V/m @ ${f3(eRef, 1)} · ${f3(pk(fr.slot, eBound), 1)} V/m @ ${f3(eBound, 1)} V/m`);
  console.log(`\n  Across the band (continuous-slot and discrete-hole coupling both rise with f):`);
  for (const ff of [WIFI6E_HZ.lo, WIFI6E_HZ.mid, WIFI6E_HZ.hi]) {
    const ww = wifiCase(ff);
    const r = wifiField(ff, ww.Q, c.V, c.rimPerimeter);
    console.log(
      `    ${mhz(ff).padStart(12)}  Q ${f3(ww.Q, 1)}  holes ${f3(20 * Math.log10(r.holes), 1)} dB -> ${f3(pk(r.holes, eRef), 3)} / ${f3(pk(r.holes, eBound), 3)} V/m` +
        `   slot ${f3(20 * Math.log10(r.slot), 1)} dB -> ${f3(pk(r.slot, eRef), 2)} V/m`,
    );
  }
  console.log(`    lambda/20 at the band top (${mhz(WIFI6E_HZ.hi)}) is ${f3((C0 / WIFI6E_HZ.hi / 20) * 1e3, 2)} mm, not §5.3a's 2.5 mm.`);
  console.log(`\n  Module-field screening: the head must be RF-visible through the tile field. Fraction needed`);
  console.log(`  for the design-intent seam to hold 0.5 V/m: ${f3(visibleFractionForLimit(eRef) * 100, 1)} % at ${f3(eRef, 1)} V/m · ${f3(visibleFractionForLimit(eBound) * 100, 0)} % at ${f3(eBound, 1)} V/m`);
  console.log(`\n  Against §5.4's EMIRR sensitivity (REQ-CAV-01 moves 20 dB per decade of EMIRR), the`);
  console.log(`  EMIRR at which each case just meets it: holes @ ${f3(eRef, 1)} V/m ${f3(emirrBreakEven6E(pk(wifiField(WIFI6E_HZ.hi, wifiCase(WIFI6E_HZ.hi).Q, c.V, c.rimPerimeter).holes, eRef)), 1)} dB · holes @ ${f3(eBound, 1)} V/m ${f3(emirrBreakEven6E(pk(wifiField(WIFI6E_HZ.hi, wifiCase(WIFI6E_HZ.hi).Q, c.V, c.rimPerimeter).holes, eBound)), 1)} dB · slot @ ${f3(eRef, 1)} V/m ${f3(emirrBreakEven6E(pk(fr.slot, eRef)), 1)} dB`);
  console.log(`\n  The head keeps Q finite (${f3(w.Q, 0)}) but does NOT make the mode absent at 6 GHz: Q scales`);
  console.log(`  with electrical size. What decides REQ-CAV-01 is the seam, by ~${f3(20 * Math.log10(fr.slot / fr.holes), 0)} dB — not the layer.`);
  console.log(`  h_e check: lambda/pi at 7.125 GHz = ${f3((C0 / WIFI6E_HZ.hi / Math.PI) * 1e3, 1)} mm <= §5.1's ${f3(fl.hEff * 1e3)} mm, so 0.5 V/m is conservative here.`);
}

function reportValidation(): boolean {
  const m = midCavity();
  const small = lowestMode(HEAD_CIRC_M.min, "min");
  const large = lowestMode(HEAD_CIRC_M.max, "max");
  const q = qLoaded(m.f, PD_FABRIC_RS.nominal);
  const fl = fieldLimit();
  const foamDesign = slabSurfaceR(m.f, ABSORBER_T_M, FOAM_EPS[1][1]);
  const foamAbsurd = slabSurfaceR(m.f, ABSORBER_T_M, FOAM_EPS[3][1]);
  const qFoamDesign = qLoaded(m.f, PD_FABRIC_RS.nominal, foamDesign.Rs).wallQ;
  const qFoamAbsurd = qLoaded(m.f, PD_FABRIC_RS.nominal, foamAbsurd.Rs).wallQ;
  const w6 = wifiCase(WIFI6E_HZ.mid);
  const fr6 = wifiField(WIFI6E_HZ.mid, w6.Q, w6.c.V, w6.c.rimPerimeter);
  const w6hi = wifiCase(WIFI6E_HZ.hi);
  const fr6hi = wifiField(WIFI6E_HZ.hi, w6hi.Q, w6hi.c.V, w6hi.c.rimPerimeter);

  const anchors: Array<[string, number, number, number]> = [
    // label, computed, published, tolerance
    ["lowest mode, 52 cm head (MHz)", small.f / 1e6, 506, 2],
    ["lowest mode, 62 cm head (MHz)", large.f / 1e6, 422, 2],
    ["first radial mode, quarter-wave (GHz)", firstRadialMode("max").quarterWave / 1e9, 2.58, 0.05],
    ["source roll-off 500 MHz -> 3 GHz (dB)", -(sourceRolloffDb(3e9, EDGE_TIME_S.slow) - sourceRolloffDb(500e6, EDGE_TIME_S.slow)), 31.1, 0.5],
    ["REQ-CAV-01 field limit (V/m)", fl.eMax, 0.5, 0.02],
    ["skin surface resistance at f_mid (Ohm/sq)", q.skinRs, 53.9, 1.0],
    ["bare-wall Q, 0.1 Ohm/sq fabric", q.wallQ, 409, 10],
    ["Q with the head in it", q.loaded, 1.32, 0.05],
    ["dB needed, bare -> Q_L 20", 20 * Math.log10(q.wallQ / Q_CEILING), 26.2, 0.3],
    ["dB the head supplies", 20 * Math.log10(q.wallQ / q.loaded), 49.8, 0.5],
    ["head share of cavity boundary area (%)", (m.sHead / (m.sHead + m.sShield)) * 100, 36.5, 0.5],
    ["head share of enclosed volume (%)", (headVolume(m.aHead) / (headVolume(m.aHead) + m.V)) * 100, 43.5, 0.5],
    ["Layer 4 R_s, design loading (mOhm/sq)", foamDesign.Rs * 1e3, 3.04, 0.3],
    ["Layer 4 R_s, absurd loading (mOhm/sq)", foamAbsurd.Rs * 1e3, 36.6, 2.0],
    ["dB Layer 4 supplies, design loading", 20 * Math.log10(q.wallQ / qFoamDesign), 0.26, 0.03],
    ["dB Layer 4 supplies, absurd loading", 20 * Math.log10(q.wallQ / qFoamAbsurd), 2.71, 0.1],
    // §7 — what replaces the layer. These decide the recommendation, not §6.
    ["station R, 3 mm foam (m2K/W)", rStation(THERM.kFoam), 0.075, 0.001],
    ["station R, 3 mm stagnant air (m2K/W)", rStation(THERM.kAir), 0.115, 0.001],
    ["air-for-foam penalty (%)", (rStation(THERM.kAir) / rStation(THERM.kFoam) - 1) * 100, 53.8, 1.0],
    ["outward path, delete without re-loft", THERM.outwardTotal - rStation(THERM.kFoam) + rStation(THERM.kAir), 0.450, 0.002],
    ["outward path, delete with 3 mm re-loft", THERM.outwardTotal - rStation(THERM.kFoam), 0.335, 0.002],
    ["outward path, ceramic substitution", THERM.outwardTotal - rStation(THERM.kFoam) + THERM.rSubstitution, 0.355, 0.002],
    // §8.3 — the clamp stack SHRINKS on deletion; the foam is a tolerance contributor.
    ["clamp stack today, worst case (mm)", clampStack(true).worst, 1.30, 0.01],
    ["clamp stack after deletion, worst case (mm)", clampStack(false).worst, 0.80, 0.01],
    ["clamp stack reduction (%)", (1 - clampStack(false).worst / clampStack(true).worst) * 100, 38.5, 0.5],
    // §8.5 — the Gap, re-opened by FLUSH-1. The largest unclaimed thermal lever.
    ["gap R at 6 mm nominal (m2K/W)", rGap(GAP.nominalMm), 0.231, 0.001],
    ["gap share of outward path (%)", (rGap(GAP.nominalMm) / THERM.outwardTotal) * 100, 56.3, 0.5],
    ["per mm of gap (m2K/W)", gapPerMm(), 0.0385, 0.0005],
    ["outward total at 4 mm gap", outwardAtGap(4), 0.333, 0.002],
    ["outward total at 3 mm gap", outwardAtGap(3), 0.295, 0.002],
    ["2 mm of gap vs deleting the absorber", 2 * gapPerMm() - rStation(THERM.kFoam), 0.0019, 0.001],
    // §8.7 — the Gap floor is set by the controller components, not the lever.
    ["tallest named gap-facing package (mm)", tallestPart(), 1.45, 0.01],
    ["component plane, PCB-1 + tallest part (mm)", componentPlane(), 2.25, 0.01],
    ["gap floor, PLATE-1 pocketed, low (mm)", gapFloorPocketed()[0], 2.75, 0.01],
    ["gap floor, PLATE-1 pocketed, high (mm)", gapFloorPocketed()[1], 3.25, 0.01],
    ["PLATE-1 saving vs a 0.8 mm plate sitting over", rGap(gapFloorStacked(0.8)[0]) - rGap(gapFloorPocketed()[0]), 0.031, 0.002],
    ["PCB-1 saving, 1.0 -> 0.80 mm", rGap(1.0 + tallestPart() + 0.5) - rGap(gapFloorPocketed()[0]), 0.0077, 0.0005],
    ["outward total at the 2.75 mm floor", outwardAtGap(gapFloorPocketed()[0]), 0.285, 0.002],
    // §8.8 — PACK-1. The rule is safe at nominal pitch and tight at the rim.
    ["hex edge at nominal W=40 (mm)", hexEdge(HEX.nominalWmm), 23.09, 0.01],
    ["boss->board separation, nominal (mm)", packSeparation(HEX.nominalWmm), 23.09, 0.01],
    // Uniform tiles => this separation is IDENTICAL at all 18 clusters. There is
    // no per-cluster variation to sweep, which is the point of interchangeability.
    ["hex edge is the tile's own edge, so it is constant (mm)", hexEdge(HEX.nominalWmm), 23.09, 0.01],
    // The module is median-curved, so check the curvature does not move PACK-1.
    ["hex edge as chord on the R=87 module surface (mm)", hexEdgeChord(HEX.nominalWmm, HEX.moduleRadiusMm), 23.03, 0.01],
    ["curvature perturbation to PACK-1 (mm)", hexEdge(HEX.nominalWmm) - hexEdgeChord(HEX.nominalWmm, HEX.moduleRadiusMm), 0.068, 0.005],
    ["outward total at a 3 mm gap", outwardAtGap(3), 0.295, 0.002],
    ["recovery, 6 mm -> 3 mm", rGap(GAP.nominalMm) - rGap(3), 0.115, 0.002],
    // §3.1 — OI-EMCCAV-03. REQ-CAV-02 is an allocation against §3's sources, so a
    // new in-envelope source X dB stronger (in field) tightens the Q ceiling to
    // Q_CEILING * 10^(-X/20). The head's margin under the ceiling is what it tolerates.
    ["§3.1 head margin under the Q ceiling (dB)", 20 * Math.log10(Q_CEILING / q.loaded), 23.6, 0.1],
    ["§3.1 margin left, source +10 dB", 20 * Math.log10(Q_CEILING * 10 ** (-10 / 20) / q.loaded), 13.6, 0.1],
    ["§3.1 margin left, source +20 dB (x10 field)", 20 * Math.log10(Q_CEILING * 10 ** (-20 / 20) / q.loaded), 3.6, 0.1],
    // §5.4 — OI-EMCCAV-01 sensitivity. Candidate EMIRR values, NOT measurements.
    ["REQ-CAV-01 at EMIRR 40 dB (V/m)", emirrSensitivity(40).eMax, 0.050, 0.002],
    ["REQ-CAV-01 at EMIRR 50 dB (V/m)", emirrSensitivity(50).eMax, 0.158, 0.002],
    ["REQ-CAV-01 at EMIRR 70 dB (V/m)", emirrSensitivity(70).eMax, 1.58, 0.02],
    ["scaled Q ceiling at EMIRR 40 dB", emirrSensitivity(40).qCeil, 2.0, 0.01],
    ["head margin at EMIRR 40 dB (dB)", emirrSensitivity(40).headMarginDb, 3.6, 0.3],
    ["head margin at EMIRR 50 dB (dB)", emirrSensitivity(50).headMarginDb, 13.6, 0.3],
    ["head margin at EMIRR 60 dB (dB)", emirrSensitivity(60).headMarginDb, 23.6, 0.3],
    ["head margin at EMIRR 70 dB (dB)", emirrSensitivity(70).headMarginDb, 33.6, 0.3],
    ["EMIRR break-even for the head margin (dB)", emirrBreakEven(), 36.4, 0.3],
    // §6.5 — OI-EMCCAV-06: external 6 GHz Wi-Fi ingress, post-deletion geometry.
    ["6E: LPI client 24 dBm @ 0.3 m (V/m rms)", incidentE(WIFI_CLIENT.lpiDbm, WIFI_CLIENT.refDistM), 9.15, 0.05],
    ["6E: SP client 30 dBm @ 0.1 m, bounding (V/m rms)", incidentE(WIFI_CLIENT.spDbm, WIFI_CLIENT.nearDistM), 54.8, 0.2],
    ["6E: Gabriel dry skin e_r at 6.525 GHz", w6.skin.epsR, 34.5, 0.2],
    ["6E: Gabriel dry skin sigma at 6.525 GHz (S/m)", w6.skin.sigma, 4.37, 0.05],
    ["6E: Gabriel dry skin sigma at 460 MHz (S/m)", gabrielSkin(460e6).sigma, 0.71, 0.02],
    ["6E: head absorptivity, diffuse", w6.aHead, 0.498, 0.005],
    ["6E: post-deletion cavity volume (L)", w6.c.V * 1e3, 3.53, 0.02],
    ["6E: modal overlap M at 6.525 GHz", modalOverlap(WIFI6E_HZ.mid, w6.c.V, w6.Q).M, 24.5, 0.5],
    ["6E: composite Q, head in, 5.925 GHz", wifiCase(WIFI6E_HZ.lo).Q, 34.0, 0.5],
    ["6E: composite Q, head in, 6.525 GHz", w6.Q, 37.4, 0.5],
    ["6E: composite Q, head in, 7.125 GHz", wifiCase(WIFI6E_HZ.hi).Q, 40.8, 0.5],
    ["6E: composite Q, empty (fabric wall only)", w6.Qwall, 10633, 100],
    ["6E: composite Q had Layer 4 been kept", w6.Qfoam, 23.8, 0.5],
    ["6E: field Layer 4 would have bought (dB)", 10 * Math.log10(w6.Q / w6.Qfoam), 1.97, 0.05],
    ["6E: head visibility at foam parity (%)", visibleFractionFoamParity() * 100, 58, 1],
    ["6E: seam coupling, discrete 2.5 mm holes (dB)", 20 * Math.log10(fr6.holes), -47.7, 0.2],
    ["6E: seam coupling, continuous 2.5 mm slot (dB)", 20 * Math.log10(fr6.slot), -11.5, 0.2],
    ["6E: E_pk, holes @ 9.2 V/m, 7.125 GHz (V/m)", fr6hi.holes * incidentE(WIFI_CLIENT.lpiDbm, WIFI_CLIENT.refDistM) * PEAK_FACTOR, 0.079, 0.002],
    ["6E: E_pk, holes @ 54.8 V/m, 7.125 GHz (V/m)", fr6hi.holes * incidentE(WIFI_CLIENT.spDbm, WIFI_CLIENT.nearDistM) * PEAK_FACTOR, 0.473, 0.005],
    ["6E: E_pk, slot @ 9.2 V/m (V/m)", fr6.slot * incidentE(WIFI_CLIENT.lpiDbm, WIFI_CLIENT.refDistM) * PEAK_FACTOR, 4.27, 0.05],
    ["6E: E_pk, slot @ 9.2 V/m had Layer 4 been kept (V/m)", fr6.slot * incidentE(WIFI_CLIENT.lpiDbm, WIFI_CLIENT.refDistM) * PEAK_FACTOR * Math.sqrt(w6.Qfoam / w6.Q), 3.40, 0.05],
    ["6E: slot vs holes swing at 6.525 GHz (dB)", 20 * Math.log10(fr6.slot / fr6.holes), 36.2, 0.3],
    ["6E: head visibility needed @ 54.8 V/m (%)", visibleFractionForLimit(incidentE(WIFI_CLIENT.spDbm, WIFI_CLIENT.nearDistM)) * 100, 63, 1],
    ["6E: lambda/20 at 7.125 GHz (mm)", (C0 / WIFI6E_HZ.hi / 20) * 1e3, 2.10, 0.01],
    // §6.5.5 item 5 — the same margins against §5.4's EMIRR sensitivity.
    ["6E: EMIRR break-even, holes @ 9.2 V/m, 7.125 GHz (dB)", emirrBreakEven6E(fr6hi.holes * incidentE(WIFI_CLIENT.lpiDbm, WIFI_CLIENT.refDistM) * PEAK_FACTOR), 44.0, 0.2],
    ["6E: EMIRR break-even, holes @ 54.8 V/m, 7.125 GHz (dB)", emirrBreakEven6E(fr6hi.holes * incidentE(WIFI_CLIENT.spDbm, WIFI_CLIENT.nearDistM) * PEAK_FACTOR), 59.5, 0.2],
    ["6E: EMIRR break-even, slot @ 9.2 V/m (dB)", emirrBreakEven6E(fr6.slot * incidentE(WIFI_CLIENT.lpiDbm, WIFI_CLIENT.refDistM) * PEAK_FACTOR), 78.6, 0.2],
    // OI-THCOOL-21 — the same Gap figures on the CURRENT (station deleted) baseline.
    // The as-was rows above stay: §8.5's "2 mm beats deleting the absorber" is
    // stated against the absorber, and only makes sense with it in.
    ["[current] outward path, station deleted", THERM.outwardCurrent, 0.335, 0.002],
    ["[current] gap share of outward path (%)", (rGap(GAP.nominalMm) / THERM.outwardCurrent) * 100, 68.9, 0.5],
    ["[current] outward total at 4 mm gap", outwardAtGapNow(4), 0.258, 0.002],
    ["[current] outward total at 3 mm gap", outwardAtGapNow(3), 0.220, 0.002],
    ["[current] outward total at the 2.75 mm floor", outwardAtGapNow(gapFloorPocketed()[0]), 0.210, 0.002],
  ];

  console.log(`\nscanned: ${anchors.length} published anchor(s) — NP-EMC-CAV-001\n`);
  let ok = true;
  for (const [label, got, want, tol] of anchors) {
    const pass = Math.abs(got - want) <= tol;
    if (!pass) ok = false;
    const d = Math.abs(want) >= 100 ? 0 : Math.abs(want) >= 10 ? 1 : Math.abs(want) >= 1 ? 2 : 3;
    console.log(
      `  ${pass ? "ok  " : "FAIL"}  ${label.padEnd(44)} ${got.toFixed(d).padStart(9)}  (published ${want.toFixed(d)} +/- ${tol})`,
    );
  }
  console.log(`\n${ok ? "PASS" : "FAIL"} — published anchors reproduce.`);
  return ok;
}

function main() {
  const ok = reportValidation();
  if (VALIDATE_ONLY) {
    process.exit(ok ? 0 : 1);
    return;
  }
  reportBand();
  reportSource();
  reportAllocation();
  reportEmirr();
  reportQ();
  reportAbsorber();
  reportVerdict();
  reportThermal();
  reportGap();
  reportGapFloor();
  reportWifi();
  console.log();
}

if (import.meta.main) main();
