#!/usr/bin/env bun
/**
 * check-thermal-sink.ts — the rejection specification at the via terminus (OI-N1-02).
 *
 * `NP-THERM-CFD-N1-001` §5 finds that the whole N-dependence of the lattice sits
 * in one term: the external heatsink the BN-boss via exports ~90 % of each tile's
 * heat into. It sweeps `R_sink` from "perfect" to 2 K/W, gets ceilings from 0 to
 * >80 tiles, and raises `OI-N1-02` BLOCKING because NO DOCUMENT IN THE TREE GIVES
 * THAT COMPONENT A NUMBER. N1-D-1 then forbids quoting any tile count until it does.
 *
 * This script gives it a number, and the number is not chosen — it is recovered,
 * twice, from quantities already in the tree.
 *
 *   bun scripts/check-thermal-sink.ts             # full report
 *   bun scripts/check-thermal-sink.ts --validate  # anchors only, exit 1 on drift
 *
 * ── The three things it establishes ──────────────────────────────────────────
 *
 *  1. THE TERMINUS IS THE SHELL, NOT THE HUB. R1's via is a ~32 mm radial
 *     conductor through the layer stack; the hub heatsink `NP-TOOL-HUB-001` §2
 *     infers sits a median 188 mm away across that shell. Nothing in the tree
 *     specifies a part that connects them, and neither candidate survives its own
 *     arithmetic: a solid collection trunk inside a head-worn mass budget drops
 *     tens of kelvin in transport alone, and a heatsink bolted on at the occiput
 *     adds zero tiles to the ceiling even when its own resistance is zero (§1).
 *
 *  2. R_sink IS NOT A FREE PARAMETER. The via terminates on the outer bowl, so
 *     its rejection resistance is the EXTERNAL FILM of R1's own outward path —
 *     the last term of R_OUT_BASE = 0.41 — which the via path and the cavity path
 *     must SHARE. R1 pins it at ambient (its "perfect sink"), setting to zero a
 *     resistance its own model spends once. Recovered from R1: 0.115 m²K/W over
 *     the tile footprint. Recovered independently from a sphere natural-convection
 *     correlation plus linearised radiation over the helmet's own exterior: the
 *     same coefficient to 4.7 %. Aggregated: ~1.08 K/W (§2, §3).
 *
 *  3. WHAT IS MISSING IS A SPREADER, NOT A HEATSINK. A lumped R_sink assumes an
 *     isothermal terminus; the bare CFRP bowl spreads over ~63 mm, so it is 80
 *     hot spots rather than one sink (§5, §7). And the fan is not on this path at
 *     all: what degrades it is OCCLUSION, which no fan-RPM signal observes (§9).
 *
 * Units: area-normalised R" in m^2*K/W ("R"), absolute conductance G in W/K.
 * Convention per NP-THERM-CFD-C2-001 §7.
 *
 * CI-Kind: report
 */
import { readFileSync } from "fs";
import {
  R_IN, R_OUT_BASE, R_GAP_STAGNANT, R_CAV_AMB, R_JF, R_FS, R_SC, R_VIA,
  T_CORE, FACE_LIMIT, AMB_NOMINAL,
  TILE_AREA, HEX_EDGE_M, CENTRE_PITCH_M,
  SOCKETS, NEIGH, N_SOCKETS, distributed,
  heatW, OP, G_LAT_SCALP, G_LAT_FACE, steady as steadyN1,
} from "./check-thermal-multitile";

const VALIDATE_ONLY = process.argv.includes("--validate");
const A = TILE_AREA;

// ---------------------------------------------------------------------------
// §1  The layer stack, decomposed — where R_OUT_BASE's last term actually is
// ---------------------------------------------------------------------------
// NP-HELMET-GEOM-001 §2 gives the radial stack outboard of the module body.
// Each layer's through-thickness R" is thickness/k. Everything except the last
// line is SOLID; the last line is the boundary layer on the helmet's exterior.

type Layer = { name: string; mm: number; k: number; note: string };

/** Outward stack from the cavity face of the absorber to the exterior skin.
 *  Thicknesses: NP-HELMET-GEOM-001 §2 (L2 + L3). Conductivities: material class. */
export const OUT_STACK: Layer[] = [
  { name: "carbon-loaded absorber foam", mm: 3.0, k: 0.05, note: "EMF L4, open-cell carbon foam" },
  { name: "Pd-polyester liner", mm: 0.1, k: 0.20, note: "EMF L3" },
  { name: "mu-metal", mm: 0.2, k: 30.0, note: "EMF L2" },
  { name: "PETG laminate", mm: 0.3, k: 0.20, note: "EMF L2 encapsulation" },
  { name: "CFRP shell", mm: 2.5, k: 0.80, note: "EMF L1 + structure, through-thickness" },
];

/** Solid part of the outward path, m^2K/W. */
export const R_SOLID_OUT = OUT_STACK.reduce((a, l) => a + l.mm / 1000 / l.k, 0);

/** THE EXTERNAL FILM. R1's outward path minus its own solid stack. This is the
 *  term R1 sets to zero when it pins the via terminus at ambient — and the same
 *  term its cavity path already spends once. m^2K/W over the TILE FOOTPRINT. */
export const R_FILM_R1 = R_CAV_AMB - R_SOLID_OUT;
/** Implied external coefficient, W/m^2K, referred to the tile footprint. */
export const H_FILM_R1 = 1 / R_FILM_R1;

// ---------------------------------------------------------------------------
// §2  The same coefficient, recovered independently from the helmet's outside
// ---------------------------------------------------------------------------

const MAP = JSON.parse(readFileSync("hardware/np_socket_map.json", "utf8"));
const AX = MAP.ellipsoidSemiAxesMm as { foreAft: number; lateral: number; vertical: number };
/** Module-face plane to exterior skin: gap + L2 + L3 per NP-HELMET-GEOM-001 §2. */
const SHELL_OFFSET_MM = 12;

/** Thomsen approximation, better than 1.1 % for any ellipsoid. */
function ellipsoidArea(a: number, b: number, c: number): number {
  const p = 1.6075;
  return 4 * Math.PI * (((a * b) ** p + (a * c) ** p + (b * c) ** p) / 3) ** (1 / p);
}

/** Exterior area of the vault dome, m^2 — half the offset ellipsoid. */
export const A_EXT_GROSS =
  ellipsoidArea(AX.foreAft + SHELL_OFFSET_MM, AX.lateral + SHELL_OFFSET_MM, AX.vertical + SHELL_OFFSET_MM)
  / 2 / 1e6;

/** Deductions: what the vault exterior is not free to reject through.
 *  Areas are design-stage estimates and are the softest input in this file. */
export const A_EXT_DEDUCTIONS: { name: string; cm2: number }[] = [
  { name: "ear cut-outs (2 x ear-cup mount)", cm2: 100 },
  { name: "hub enclosure footprint (occipital arch)", cm2: 54 },
  { name: "Boa occipital dial", cm2: 20 },
];
export const A_EXT_EFF = A_EXT_GROSS - A_EXT_DEDUCTIONS.reduce((a, d) => a + d.cm2, 0) / 1e4;

/** Air properties at a ~305 K film. */
const AIR = { nu: 1.6e-5, alpha: 2.25e-5, k: 0.0263, Pr: 0.71, beta: 1 / 305 };
/** Characteristic sphere diameter of the helmeted head, m. */
const D_SPHERE = 2 * ((AX.foreAft + AX.lateral + AX.vertical) / 3 + SHELL_OFFSET_MM) / 1000;

/** Churchill-Chu sphere natural convection. Returns h in W/m^2K. */
export function hConvSphere(dT: number): number {
  const Ra = (9.81 * AIR.beta * dT * D_SPHERE ** 3) / (AIR.nu * AIR.alpha);
  const Nu = 2 + (0.589 * Ra ** 0.25) / (1 + (0.469 / AIR.Pr) ** (9 / 16)) ** (4 / 9);
  return (Nu * AIR.k) / D_SPHERE;
}

const EMISSIVITY = 0.90;   // painted polymer / CFRP
const VIEW_FACTOR = 0.85;  // the rest of the view is the wearer's own shoulders
/** Linearised radiation coefficient about T_m (K), W/m^2K. */
export const hRad = (tmK: number) => 4 * EMISSIVITY * VIEW_FACTOR * 5.67e-8 * tmK ** 3;

/** Independent external coefficient over the REAL exterior area, W/m^2K. */
export function hExtCorrelation(dT = 10, tmK = 303): number {
  return hConvSphere(dT) + hRad(tmK);
}

// ---------------------------------------------------------------------------
// §3  Lateral conduction in the exterior skin — the spreader trade
// ---------------------------------------------------------------------------
// Per neighbour pair, G = sum(k*t) * w / L, w = hex edge, L = centre pitch —
// the same form NP-THERM-CFD-N1-001 §2.1 uses, applied to the SHELL, which is
// where the one large lateral term in the stack lives.

const lateralG = (kt: number) => (kt * HEX_EDGE_M) / CENTRE_PITCH_M;

/** Bare outer bowl: CFRP in-plane + mu-metal. This is N1's G_LAT_CAV, relocated
 *  from the cavity air (which cannot conduct) to the shell (which does). */
export const KT_SHELL_BARE = 10 * 0.0025 + 30 * 0.0002;

export type Spreader = { id: string; name: string; kt: number; gramsPerM2: number; note: string };
export const SPREADERS: Spreader[] = [
  { id: "S0", name: "none — bare outer bowl", kt: 0, gramsPerM2: 0, note: "as adopted" },
  { id: "S1", name: "PGS graphite film 25 um", kt: 1900 * 25e-6, gramsPerM2: 0.025e-3 * 2100 * 1000, note: "k 1900 in-plane" },
  { id: "S2", name: "PGS graphite film 70 um", kt: 1500 * 70e-6, gramsPerM2: 0.070e-3 * 2100 * 1000, note: "k 1500 in-plane" },
  { id: "S3", name: "PGS graphite film 100 um", kt: 1500 * 100e-6, gramsPerM2: 0.100e-3 * 2100 * 1000, note: "k 1500 in-plane" },
  { id: "S4", name: "aluminium foil 300 um", kt: 205 * 300e-6, gramsPerM2: 0.300e-3 * 2700 * 1000, note: "k 205" },
];

// ---------------------------------------------------------------------------
// §4  The refined network — R_OUT_BASE re-partitioned, not re-estimated
// ---------------------------------------------------------------------------
// N1's network: J -> F -> S -> core (inward); J -> C -> ambient (cavity);
//               J -> K -> ambient (via, through the unspecified R_sink).
//
// The correction is one node. The cavity path and the via path do not reach two
// different ambients: they reach the SAME exterior skin, and the film on that
// skin is the last term of R_OUT_BASE. So:
//
//   J -> C          stagnant inter-bowl gap        0.230   (unchanged)
//   C -> X          absorber + liners + CFRP       0.065   (was inside R_CAV_AMB)
//   X -> ambient    external film                  0.115   (was inside R_CAV_AMB)
//   J -> X          the BN-boss via                0.0059  (was J -> K)
//   X(i) <-> X(j)   shell + optional spreader
//
// 0.230 + 0.065 + 0.115 = 0.410 = R_OUT_BASE EXACTLY. No number is invented; the
// via simply lands where a 30 mm radial conductor lands, and stops being credited
// with a rejection surface the cavity path is already paying for.

const NJ = (i: number) => i;
const NF = (i: number) => N_SOCKETS + i;
const NS = (i: number) => 2 * N_SOCKETS + i;
const NC = (i: number) => 3 * N_SOCKETS + i;
const NX = (i: number) => 4 * N_SOCKETS + i;
const N_NODES = 5 * N_SOCKETS;

/** Exterior skin area belonging to one socket, m^2. */
export const A_EXT_TILE = A_EXT_EFF / N_SOCKETS;

export type SinkOpts = {
  amb?: number;
  /** External coefficient over the exterior skin, W/m^2K. */
  hExt?: number;
  /** Extra series R" on the exterior skin over `occludedFrac` of it (m^2K/W). */
  occlusionR?: number;
  occludedFrac?: number;
  /** Spreader kt added to the bare shell, W/K. */
  ktSpreader?: number;
  /** Pin the exterior skin at ambient. */
  perfectSink?: boolean;
  /** R1-compatibility mode: pin the skin AND charge the cavity path the full
   *  R_CAV_AMB (film included). That pair IS R1's configuration — and the fact
   *  that it takes both is the double count this document is about. */
  r1Compat?: boolean;
  /** Bolt a heatsink to the shell at the occipital arch (the §1b test).
   *  `rHub` is its own resistance to ambient, K/W; 0 = a perfect one. */
  hubSink?: { rHub: number; gContact?: number };
};

/** Socket whose exterior skin the occipital hub would bolt to. */
export const OCCIPUT_IDX = (() => {
  const target = { xMm: -AX.foreAft, yMm: 0, zMm: 0 };
  let best = 0, bd = Infinity;
  SOCKETS.forEach((s, i) => {
    const d = Math.hypot(s.xMm - target.xMm, s.yMm - target.yMm, s.zMm - target.zMm);
    if (d < bd) { bd = d; best = i; }
  });
  return best;
})();

function solveLinear(M: number[][], b: number[]): number[] {
  const n = b.length;
  for (let c = 0; c < n; c++) {
    let p = c;
    for (let r = c + 1; r < n; r++) if (Math.abs(M[r][c]) > Math.abs(M[p][c])) p = r;
    [M[c], M[p]] = [M[p], M[c]];
    [b[c], b[p]] = [b[p], b[c]];
    const d = M[c][c];
    for (let r = c + 1; r < n; r++) {
      const f = M[r][c] / d;
      if (f === 0) continue;
      for (let k = c; k < n; k++) M[r][k] -= f * M[c][k];
      b[r] -= f * b[c];
    }
  }
  const x = new Array(n).fill(0);
  for (let r = n - 1; r >= 0; r--) {
    let s = b[r];
    for (let k = r + 1; k < n; k++) s -= M[r][k] * x[k];
    x[r] = s / M[r][r];
  }
  return x;
}

export type SinkField = {
  j: number[]; f: number[]; s: number[]; c: number[]; x: number[];
  qHub: number; qTotal: number;
};

export function steadySink(qTile: number[], o: SinkOpts = {}): SinkField {
  const amb = o.amb ?? AMB_NOMINAL;
  const hExt = o.hExt ?? H_EXT_SPEC;
  const ktLat = KT_SHELL_BARE + (o.ktSpreader ?? 0);
  const gLatX = lateralG(ktLat);
  const occlFrac = o.occludedFrac ?? 0;
  const occlR = o.occlusionR ?? 0;

  const M: number[][] = Array.from({ length: N_NODES }, () => new Array(N_NODES).fill(0));
  const b = new Array(N_NODES).fill(0);
  const link = (a1: number, a2: number, g: number) => {
    M[a1][a1] += g; M[a2][a2] += g; M[a1][a2] -= g; M[a2][a1] -= g;
  };
  const toRes = (a1: number, g: number, T: number) => { M[a1][a1] += g; b[a1] += g * T; };

  // Per-socket exterior conductance: (1-phi) clear + phi occluded, in parallel.
  const gClear = (1 - occlFrac) * A_EXT_TILE * hExt;
  const gOccl = occlFrac * A_EXT_TILE / (1 / hExt + occlR);
  const gXA = gClear + gOccl;

  for (let i = 0; i < N_SOCKETS; i++) {
    link(NJ(i), NF(i), A / R_JF);
    link(NF(i), NS(i), A / R_FS);
    toRes(NS(i), A / R_SC, T_CORE);
    link(NJ(i), NC(i), A / R_GAP_STAGNANT);
    link(NC(i), NX(i), A / (o.r1Compat ? R_CAV_AMB : R_SOLID_OUT));
    link(NJ(i), NX(i), A / R_VIA);
    if (!o.perfectSink && !o.r1Compat) toRes(NX(i), gXA, amb);
    b[NJ(i)] += qTile[i];
    for (const j of NEIGH[i]) {
      if (j <= i) continue;
      link(NS(i), NS(j), G_LAT_SCALP);
      link(NF(i), NF(j), G_LAT_FACE);
      link(NX(i), NX(j), gLatX);
    }
  }
  if (o.perfectSink || o.r1Compat) for (let i = 0; i < N_SOCKETS; i++) { M[NX(i)] = new Array(N_NODES).fill(0); M[NX(i)][NX(i)] = 1; b[NX(i)] = amb; }
  // Hub sink: shell -> contact -> hub sink -> ambient, in series.
  const gHubPath = o.hubSink ? 1 / (1 / (o.hubSink.gContact ?? 2.0) + o.hubSink.rHub) : 0;
  if (o.hubSink) toRes(NX(OCCIPUT_IDX), gHubPath, amb);

  const T = solveLinear(M, b);
  const qHub = o.hubSink ? (T[NX(OCCIPUT_IDX)] - amb) * gHubPath : 0;
  return {
    j: SOCKETS.map((_, i) => T[NJ(i)]),
    f: SOCKETS.map((_, i) => T[NF(i)]),
    s: SOCKETS.map((_, i) => T[NS(i)]),
    c: SOCKETS.map((_, i) => T[NC(i)]),
    x: SOCKETS.map((_, i) => T[NX(i)]),
    qHub, qTotal: qTile.reduce((a, q) => a + q, 0),
  };
}

const maxFace = (f: SinkField) => Math.max(...f.f);

function drive(set: number[], elecW: number): number[] {
  const q = new Array(N_SOCKETS).fill(0);
  for (const i of set) q[i] = heatW(elecW);
  return q;
}

// ---------------------------------------------------------------------------
// §5  THE SPECIFICATION
// ---------------------------------------------------------------------------

/** Specified external coefficient over the exterior skin, W/m^2K.
 *  The conservative of the two independent recoveries (§2 of the report). */
export const H_EXT_SPEC = Math.min(
  H_FILM_R1 * (A / A_EXT_TILE),   // R1's film, re-referred to the real exterior area
  hExtCorrelation(10, 303),        // correlation, over the same area
);

/** SPEC-SINK-01: the aggregate rejection resistance of the vault exterior, K/W. */
export const R_SINK_SPEC = 1 / (H_EXT_SPEC * A_EXT_EFF);

/** Band on SPEC-SINK-01, from the corners of the two soft inputs: h_ext over
 *  [correlation at dT 5 in a 25 C room, R1's own implied film] and A_ext over
 *  [-25 %, +25 %] of the §2 deduction estimate. */
export const R_SINK_BAND: [number, number] = [
  1 / (H_FILM_R1 * (A / A_EXT_TILE) * A_EXT_EFF * 1.25),
  1 / (hExtCorrelation(5, 298) * A_EXT_EFF * 0.75),
];

/** SPEC-SINK-02: the degraded (occluded) case. A hood or bedding over ~70 % of
 *  the vault, 3 mm of fabric at k 0.05, with radiation to a surface that warms. */
export const OCCLUSION = { fracRef: 0.70, rAdd: 0.003 / 0.05 + 0.02 };

// ---------------------------------------------------------------------------
// §6  Report
// ---------------------------------------------------------------------------

const f1 = (x: number) => x.toFixed(1);
const f2 = (x: number) => x.toFixed(2);
const f3 = (x: number) => x.toFixed(3);
const rule = (s: string) => console.log(`\n${s}\n${"-".repeat(s.length)}`);

/** Straight-line socket -> occipital hub distances, mm. */
function hubDistances(): number[] {
  const t = { x: -AX.foreAft, y: 0, z: 0 };
  return SOCKETS.map((s) => Math.hypot(s.xMm - t.x, s.yMm - t.y, s.zMm - t.z)).sort((a, b) => a - b);
}
/** Median socket-to-hub distance, mm — a LOWER bound: it is straight-line, while
 *  a real conductor would follow the shell. */
const MEDIAN_HUB_MM = (() => { const d = hubDistances(); return d[Math.floor(d.length / 2)]; })();

function reportTerminus() {
  rule("§1  Where the via terminus is, and what is not attached to it");
  const rStack = OUT_STACK.reduce((a, l) => a + l.mm / 1000 / l.k, 0);
  const viaLenMm = 14 + 2.25 + 3.5 + 6 + OUT_STACK.reduce((a, l) => a + l.mm, 0);
  console.log(`  R1 §5's via is a solid conductor down the boss centreline. Against the`);
  console.log(`  NP-HELMET-GEOM-001 §2 radial stack it is ~${viaLenMm.toFixed(0)} mm long: module body 14,`);
  console.log(`  socket wall 2.25, clamp 3.5, inter-bowl gap 6, then the ${rStack > 0 ? OUT_STACK.length : 0}-layer outer bowl.`);
  console.log(`  It therefore terminates ON THE OUTER BOWL. It does not reach the hub.`);
  const d = hubDistances();
  console.log();
  console.log(`  Straight-line socket -> occipital hub: min ${d[0].toFixed(0)}  median ${d[Math.floor(d.length / 2)].toFixed(0)}` +
    `  max ${d[d.length - 1].toFixed(0)} mm  (${N_SOCKETS} sockets)`);
  console.log();
  const q = drive(distributed(6), OP.libMin);
  console.log(`  (a) A SOLID COLLECTION TRUNK to the hub. A conductor carrying Q watts over`);
  console.log(`      L metres drops dT = Q L / (k A); as a trunk of mass m = rho L A that is`);
  console.log(`      dT = rho Q L^2 / (k m). Mass is the design variable. What it buys at the`);
  console.log(`      MEDIAN socket-to-hub distance (${MEDIAN_HUB_MM.toFixed(0)} mm, itself a lower bound):`);
  console.log();
  console.log(`      copper trunk    dT at ${f1(6 * heatW(OP.libMin))} W (N=6 floor)   dT at ${f1(6 * heatW(OP.r4))} W (N=6 R-4)   dT at 30 W`);
  for (const g of [50, 100, 250, 500, 1000]) {
    const L = MEDIAN_HUB_MM / 1000;
    const Acu = g / 1000 / (8960 * L);
    const cells = [6 * heatW(OP.libMin), 6 * heatW(OP.r4), 30].map((Q) =>
      `${((Q * L) / (400 * Acu)).toFixed(0)} K`.padStart(13));
    console.log(`      ${(g + " g").padEnd(16)}${cells.join("      ")}`);
  }
  console.log();
  console.log(`      The whole face budget at 25 C ambient is ${f1(FACE_LIMIT - AMB_NOMINAL)} K, for every leg of the`);
  console.log(`      path together. A trunk inside a head-worn mass budget (CLAUDE.md §4.4`);
  console.log(`      rates the entire spring-decoupled electrode pod at 80-120 g) spends that`);
  console.log(`      budget several times over on transport alone. REJECTED — and branching`);
  console.log(`      does not rescue it, because every branch still spans the distance.`);
  console.log();
  console.log(`  (b) THE SHELL AS THE COLLECTOR, with a heatsink bolted on at the occiput.`);
  console.log(`      Fraction of tile heat the hub intercepts, N=6 at the library floor:`);
  console.log();
  console.log(`      spreader      R_hub=0 (perfect)   1.0 K/W   2.0 K/W   ceiling@1.3W: none -> R_hub 1.0`);
  for (const [id, kt] of [["bare shell", 0], ["PGS 100 um", SPREADERS[3].kt]] as const) {
    const cells = [0, 1.0, 2.0].map((rHub) => {
      const fld = steadySink(q, { amb: AMB_NOMINAL, ktSpreader: kt, hubSink: { rHub } });
      return `${(100 * fld.qHub / fld.qTotal).toFixed(1)} %`.padStart(12);
    });
    const c0 = ceiling(OP.libMin, { amb: AMB_NOMINAL, ktSpreader: kt });
    const c1 = ceiling(OP.libMin, { amb: AMB_NOMINAL, ktSpreader: kt, hubSink: { rHub: 1.0 } });
    console.log(`      ${id.padEnd(14)}${cells.join("  ")}       ${String(c0)} -> ${String(c1)}`);
  }
  console.log();
  console.log(`      A hub sink is not useless — but it is not "the heatsink" either. It is a`);
  console.log(`      SECOND rejection surface of the same order as the shell's own ${f2(R_SINK_SPEC)} K/W,`);
  console.log(`      reachable only through the spreader the shell would already need, and on`);
  console.log(`      a bare shell it intercepts a small share even when perfect. So it cannot`);
  console.log(`      be what R1's "perfect sink" stood for, and OI-HUB-C19's hub thermal`);
  console.log(`      budget is DECOUPLED from the tile field: it sizes for hub electronics`);
  console.log(`      (the ~1.8 W boost loss, RT1062, radios), which is the useful half of`);
  console.log(`      this finding for that item.`);
}

function reportBudget() {
  rule("§2  The rejection budget — one coefficient, recovered twice");
  console.log(`  (a) FROM R1'S OWN OUTWARD PATH. R_OUT_BASE = ${f2(R_OUT_BASE)} m2K/W, of which the`);
  console.log(`      stagnant gap is ${f2(R_GAP_STAGNANT)}, leaving R_cav->amb = ${f2(R_CAV_AMB)}. Decompose it:`);
  console.log();
  console.log(`        layer                          mm      k      R"      note`);
  for (const l of OUT_STACK) {
    console.log(`        ${l.name.padEnd(28)} ${String(l.mm).padStart(4)}  ${String(l.k).padStart(5)}  ` +
      `${(l.mm / 1000 / l.k).toFixed(4)}   ${l.note}`);
  }
  console.log(`        ${"solid subtotal".padEnd(28)}                ${R_SOLID_OUT.toFixed(4)}`);
  console.log(`        ${"EXTERNAL FILM (by balance)".padEnd(28)}                ${R_FILM_R1.toFixed(4)}   h = ${f2(H_FILM_R1)} W/m2K`);
  console.log();
  console.log(`      That film is referred to the tile footprint (${(A * 1e4).toFixed(2)} cm2). Re-referred to`);
  console.log(`      the real exterior area it is h = ${f2(H_FILM_R1 * (A / A_EXT_TILE))} W/m2K.`);
  console.log();
  console.log(`  (b) FROM THE OUTSIDE OF THE HELMET, with no R1 input at all.`);
  console.log(`      Vault dome, semi-axes (${AX.foreAft}, ${AX.lateral}, ${AX.vertical}) + ${SHELL_OFFSET_MM} mm:` +
    `  ${(A_EXT_GROSS * 1e4).toFixed(0)} cm2 gross`);
  for (const d of A_EXT_DEDUCTIONS) console.log(`        less ${d.name.padEnd(38)} ${String(d.cm2).padStart(4)} cm2`);
  console.log(`        effective rejecting area                       ${(A_EXT_EFF * 1e4).toFixed(0)} cm2 = ${f3(A_EXT_EFF)} m2`);
  console.log();
  console.log(`        dT(K)   h_conv (sphere, D ${f2(D_SPHERE)} m)   h_rad (eps ${EMISSIVITY}, F ${VIEW_FACTOR})   h_total`);
  for (const dT of [5, 10, 15]) {
    console.log(`        ${String(dT).padStart(4)}    ${f2(hConvSphere(dT)).padStart(22)}   ` +
      `${f2(hRad(303)).padStart(28)}   ${f2(hConvSphere(dT) + hRad(303)).padStart(6)}`);
  }
  console.log();
  const hA = H_FILM_R1 * (A / A_EXT_TILE), hB = hExtCorrelation(10, 303);
  console.log(`      (a) gives ${f2(hA)} W/m2K, (b) gives ${f2(hB)} W/m2K over the same area —` +
    ` ${(100 * Math.abs(hA - hB) / hB).toFixed(1)} % apart.`);
  console.log(`      Two derivations sharing no input agree. R_sink is NOT a free parameter:`);
  console.log(`      it is the outside of the helmet, and it was already in R_OUT_BASE.`);
}

function reportSpec() {
  rule("§3  THE SPECIFICATION");
  console.log(`  SPEC-SINK-01  h_ext        = ${f2(H_EXT_SPEC)} W/m2K over ${f3(A_EXT_EFF)} m2 of vault exterior`);
  console.log(`                R_sink       = ${f2(R_SINK_SPEC)} K/W aggregate  (band ${f2(R_SINK_BAND[0])} - ${f2(R_SINK_BAND[1])} K/W)`);
  const rOcc = 1 / (((1 - OCCLUSION.fracRef) * A_EXT_EFF * H_EXT_SPEC) +
    (OCCLUSION.fracRef * A_EXT_EFF / (1 / H_EXT_SPEC + OCCLUSION.rAdd)));
  console.log(`  SPEC-SINK-02  R_sink,occl  = ${f2(rOcc)} K/W  (hood/bedding over ${(100 * OCCLUSION.fracRef).toFixed(0)} % of the vault,`);
  console.log(`                               3 mm fabric at k 0.05 plus suppressed radiation)`);
  console.log(`  SPEC-SINK-03  R_sink,fan-lost = ${f2(R_SINK_SPEC)} K/W — UNCHANGED. The hub fan is not`);
  console.log(`                               on this path (§1b). See §6.`);
  console.log();
  console.log(`  N1 §5 swept 0.25 / 0.50 / 1.00 / 2.00 K/W and called 0.50 "a small fan-cooled`);
  console.log(`  extruded sink". The specified value is ${f2(R_SINK_SPEC / 0.5)}x that, and no fan appears in it.`);
}

function reportValidation(): boolean {
  rule("§0  Validation — the refined network IS R1's, re-partitioned");
  const sum = R_GAP_STAGNANT + R_SOLID_OUT + R_FILM_R1;
  const okSum = Math.abs(sum - R_OUT_BASE) < 1e-9;
  console.log(`  Leg sum   ${f3(R_GAP_STAGNANT)} + ${f3(R_SOLID_OUT)} + ${f3(R_FILM_R1)} = ${sum.toFixed(6)}` +
    `   vs R_OUT_BASE ${f3(R_OUT_BASE)}   ${okSum ? "EXACT" : "DRIFT"}`);
  // R1-compat mode must reproduce R1's own single adiabatic cell.
  const gIn = 1 / R_IN, gOut = 1 / R_OUT_BASE, gVia = 1 / R_VIA;
  /** R1 §5.1 T1-std @ 25 C, at the flux the N1 model recovers: 84.5 mW/cm^2. */
  const qm2 = 845;
  const tj = (qm2 + T_CORE * gIn + AMB_NOMINAL * gOut + AMB_NOMINAL * gVia) / (gIn + gOut + gVia);
  const faceCell = tj - (tj - T_CORE) * gIn * R_JF;
  const q = new Array(N_SOCKETS).fill(qm2 * A);
  const compat = Math.max(...steadySink(q, { amb: AMB_NOMINAL, r1Compat: true }).f);
  const dPin = Math.abs(compat - faceCell);
  console.log(`  R1-compat, all ${N_SOCKETS} driven: T_face ${f1(compat)} C` +
    `   vs R1's single adiabatic cell ${f1(faceCell)} C   delta ${dPin.toExponential(1)} K`);
  console.log();
  console.log(`  R1-compat takes TWO settings: pin the skin at ambient, AND charge the`);
  console.log(`  cavity path the full ${f2(R_CAV_AMB)} m2K/W with the film still in it. That it takes`);
  console.log(`  both is the finding — R1 spends the external film on the cavity path and`);
  console.log(`  simultaneously sets it to zero for the via path. Every other resistance,`);
  console.log(`  node and lateral term here is R1's, unchanged.`);
  console.log();
  const spec = Math.max(...steadySink(q, { amb: AMB_NOMINAL }).f);
  console.log(`  Same drive, film restored on BOTH paths: T_face ${f1(spec)} C  (+${f1(spec - compat)} K)`);
  console.log(`  — that drive is R1's T1-std flux on ALL 80 sockets (${f1(qm2 * A * N_SOCKETS)} W of heat), far`);
  console.log(`  outside any operating point. It isolates the term; it is not an operating`);
  console.log(`  claim. §6 gives the admissible drives.`);
  const ok = okSum && dPin < 0.05;
  console.log(`  ${ok ? "PASS" : "FAIL"}`);
  return ok;
}

function reportSpreader() {
  rule("§5  The spreader trade — what actually buys tiles");
  console.log(`  Bare shell lateral kt = ${f3(KT_SHELL_BARE)} W/K (CFRP 2.5 mm in-plane + mu-metal).`);
  console.log(`  Per-socket exterior conductance = ${f3(A_EXT_TILE * H_EXT_SPEC)} W/K, so the bare shell`);
  console.log(`  spreads over barely one ring. Adding an exterior spreader film:`);
  console.log();
  console.log(`  id  spreader                    kt(W/K)  lat/rej   added mass   ceiling@1.3W  ceiling@6.25W`);
  for (const sp of SPREADERS) {
    const kt = KT_SHELL_BARE + sp.kt;
    const ratio = lateralG(kt) / (A_EXT_TILE * H_EXT_SPEC);
    const mass = sp.gramsPerM2 * A_EXT_GROSS;
    const c1 = ceiling(OP.libMin, { amb: AMB_NOMINAL, ktSpreader: sp.kt });
    const c2 = ceiling(OP.r4, { amb: AMB_NOMINAL, ktSpreader: sp.kt });
    console.log(`  ${sp.id}  ${sp.name.padEnd(26)} ${f3(kt).padStart(6)}   ${f1(ratio).padStart(5)}x   ` +
      `${mass.toFixed(0).padStart(7)} g     ${String(c1).padStart(9)}     ${String(c2).padStart(9)}`);
  }
  console.log();
  console.log(`  "lat/rej" is lateral conductance to a neighbour over the socket's own`);
  console.log(`  rejection conductance. Below ~1 the exterior is 80 hot spots; the aggregate`);
  console.log(`  ${f2(R_SINK_SPEC)} K/W is only realised as it goes large. §7 gives the residual gap to`);
  console.log(`  the isothermal limit — S3 does not close it, it gets most of the way.`);
  console.log();
  console.log(`  Mass is over the gross dome (${(A_EXT_GROSS * 1e4).toFixed(0)} cm2); a graphite film also needs a`);
  console.log(`  dielectric overwrap, and it is a new BOM line against an already`);
  console.log(`  margin-negative T1 (CLAUDE.md §2.1) — costed in the owning document, not here.`);
}

/** Largest distributed N holding max face <= 42 C. */
function ceiling(elecW: number, o: SinkOpts): number | string {
  let last = 0;
  for (let n = 1; n <= N_SOCKETS; n++) {
    if (maxFace(steadySink(drive(distributed(n), elecW), o)) <= FACE_LIMIT) last = n; else break;
  }
  return last === N_SOCKETS ? ">80" : last;
}

/** Largest total PBM electrical draw holding the face limit, W, maximised over
 *  N. N and W/tile trade against each other and only their product is a budget
 *  (NP-PWR-BUDGET-001 D-4, NP-THERM-CFD-N1-001 N1-D-4).
 *
 *  Every conductance here is constant, so max face temperature is AFFINE in the
 *  per-tile drive: two solves per N give the line exactly, and no search is
 *  needed. `admissibleWatts` is that line inverted at the 42 C limit. */
function admissibleWatts(n: number, o: SinkOpts): number {
  const set = distributed(n);
  const f0 = maxFace(steadySink(drive(set, 0), o));
  if (f0 >= FACE_LIMIT) return 0;
  const f1v = maxFace(steadySink(drive(set, 1), o));
  return (n * (FACE_LIMIT - f0)) / (f1v - f0);
}

function maxTotalWatts(o: SinkOpts): number {
  let best = 0;
  for (let n = 1; n <= N_SOCKETS; n++) best = Math.max(best, admissibleWatts(n, o));
  return best;
}

function reportCeilings() {
  rule("§6  The concurrency ceiling under the specification — N1-D-1's answer");
  console.log(`  Distributed montage, max T_face <= ${f1(FACE_LIMIT)} C, spreader S3 (PGS 100 um).`);
  console.log();
  console.log(`  drive (W/tile elec)     amb 25    amb 30    amb 35    | occluded, amb 25`);
  for (const [name, w] of [["library floor  1.3", OP.libMin], ["R-4 point     6.25", OP.r4],
  ["library ceil  20.0", OP.libMax]] as const) {
    const cells = [25, 30, 35].map((amb) =>
      String(ceiling(w, { amb, ktSpreader: SPREADERS[3].kt })).padStart(6));
    const occ = String(ceiling(w, {
      amb: 25, ktSpreader: SPREADERS[3].kt,
      occludedFrac: OCCLUSION.fracRef, occlusionR: OCCLUSION.rAdd,
    })).padStart(6);
    console.log(`  ${name.padEnd(22)} ${cells.join("    ")}    | ${occ}`);
  }
  console.log();
  console.log(`  Same table on the BARE shell (S0), which is the design as currently adopted:`);
  console.log();
  console.log(`  drive (W/tile elec)     amb 25    amb 30    amb 35    | occluded, amb 25`);
  for (const [name, w] of [["library floor  1.3", OP.libMin], ["R-4 point     6.25", OP.r4],
  ["library ceil  20.0", OP.libMax]] as const) {
    const cells = [25, 30, 35].map((amb) => String(ceiling(w, { amb })).padStart(6));
    const occ = String(ceiling(w, {
      amb: 25, occludedFrac: OCCLUSION.fracRef, occlusionR: OCCLUSION.rAdd,
    })).padStart(6);
    console.log(`  ${name.padEnd(22)} ${cells.join("    ")}    | ${occ}`);
  }
  console.log();
  console.log(`  The same ceiling in the unit NP-PWR-BUDGET-001 D-4 and N1-D-4 both say the`);
  console.log(`  governor must be — TOTAL PBM ELECTRICAL WATTS admissible on the face limit:`);
  console.log();
  console.log(`  spread over N tiles      N=1     N=6    N=12    N=20    N=80   | best N`);
  for (const [id, kt] of [["bare shell S0", 0], ["PGS 100 um S3", SPREADERS[3].kt]] as const) {
    for (const amb of [25, 35]) {
      const cells = [1, 6, 12, 20, 80].map((n) =>
        `${f1(admissibleWatts(n, { amb, ktSpreader: kt }))}`.padStart(6));
      console.log(`  ${(id + ", " + amb + " C").padEnd(22)}${cells.join("  ")}   |  ${f1(maxTotalWatts({ amb, ktSpreader: kt }))} W`);
    }
  }
  console.log();
  console.log(`  Two things this unit shows that a tile count hides. The budget RISES with N`);
  console.log(`  — spreading the same watts over more tiles is thermally free and then some,`);
  console.log(`  the exact opposite of a concurrency ceiling. And the spreader's whole value`);
  console.log(`  is at LOW N: at N=80 the exterior is uniformly loaded and there is nothing`);
  console.log(`  to spread, so the two rows converge on the aggregate ${f1(maxTotalWatts({ amb: 25 }))} W. Sessions run`);
  console.log(`  at N = 5-37 (NP-THERM-CFD-N1-001 §6), which is where the spreader is worth`);
  console.log(`  ${(admissibleWatts(6, { amb: 25, ktSpreader: SPREADERS[3].kt }) / admissibleWatts(6, { amb: 25 })).toFixed(1)}x at N=6. For scale, CLAUDE.md §4.5 puts Standard T1 at ~17-20 W TOTAL`);
  console.log(`  device draw, of which PBM is a part.`);
}

/** A spreader stiff enough that the exterior skin is one isothermal node — the
 *  condition a LUMPED R_sink silently assumes. */
const KT_ISOTHERMAL = 100;

function reportBaseline() {
  rule("§4  The idle baseline — the exterior is not at ambient before anything runs");
  const zero = new Array(N_SOCKETS).fill(0);
  console.log(`  Every populated socket is a conduction path between the ${f1(T_CORE)} C perfused`);
  console.log(`  core and the exterior skin. With no tile driven at all, at 25 C ambient:`);
  console.log();
  console.log(`  ambient   T_skin    T_face    heat the WEARER pushes into the shell`);
  for (const amb of [20, 25, 30, 35]) {
    const f = steadySink(zero, { amb, ktSpreader: SPREADERS[3].kt });
    const qBody = f.x.reduce((a, tx) => a + (tx - amb) * (A_EXT_TILE * H_EXT_SPEC), 0);
    console.log(`  ${(amb + " C").padEnd(10)}${f1(Math.max(...f.x)).padStart(6)} C  ${f1(Math.max(...f.f)).padStart(6)} C` +
      `        ${f1(qBody)} W`);
  }
  console.log();
  console.log(`  The spreader does not move this row: with every tile idle the exterior is`);
  console.log(`  uniformly loaded, so there is nothing to spread.`);
  console.log();
  console.log(`  So ~${f1(steadySink(zero, { amb: AMB_NOMINAL, ktSpreader: KT_ISOTHERMAL }).x[0] - AMB_NOMINAL)} K of the ${f1(FACE_LIMIT - AMB_NOMINAL)} K face budget at 25 C is spent before the first LED lights,`);
  console.log(`  by the head the helmet is on. R1's perfect sink pins that away by fiat, so`);
  console.log(`  this term appears in no prior document. It is the same mechanism as`);
  console.log(`  NP-THERM-CFD-N1-001's OI-N1-04, running in the direction that always applies.`);
}

function reportAgainstN1() {
  rule("§7  Against NP-THERM-CFD-N1-001's own model — where the difference comes from");
  console.log(`  N1's topology is ONE sink node: every via lands on it, it rejects at R_sink,`);
  console.log(`  and the cavity path rejects SEPARATELY at ${f2(R_CAV_AMB)} (film included). Feeding it the`);
  console.log(`  specified ${f2(R_SINK_SPEC)} K/W, against this network at the same drive, 25 C, floor:`);
  console.log();
  console.log(`   N   N1 @ R_sink ${f2(R_SINK_SPEC)}   this, isothermal   this, PGS 100 um   this, bare`);
  for (const n of [1, 2, 6, 12, 20]) {
    const set = distributed(n);
    const q = drive(set, OP.libMin);
    const a = Math.max(...steadyN1(q, { amb: AMB_NOMINAL, rSink: R_SINK_SPEC }).f);
    const b = maxFace(steadySink(q, { amb: AMB_NOMINAL, ktSpreader: KT_ISOTHERMAL }));
    const c = maxFace(steadySink(q, { amb: AMB_NOMINAL, ktSpreader: SPREADERS[3].kt }));
    const d = maxFace(steadySink(q, { amb: AMB_NOMINAL }));
    console.log(`  ${String(n).padStart(2)}    ${f1(a).padStart(12)} C   ${f1(b).padStart(14)} C` +
      `   ${f1(c).padStart(14)} C   ${f1(d).padStart(9)} C`);
  }
  console.log();
  console.log(`  Column 2 vs 3 is small: with an ISOTHERMAL exterior, N1's model at the`);
  console.log(`  specified R_sink is right to about a kelvin, and the residual is the cavity`);
  console.log(`  leg's double-counted film. N1's sweep bracketed the truth; its 0.50 "plausible`);
  console.log(`  default" was ${f1(R_SINK_SPEC / 0.5)}x optimistic, and its 1.00 row is the real one.`);
  console.log();
  console.log(`  Column 3 vs 5 is NOT small, and it is the whole finding: a lumped R_sink`);
  console.log(`  silently assumes an isothermal terminus. The bare outer bowl is not one.`);
  console.log(`  THE MISSING COMPONENT IS A SPREADER, NOT A HEATSINK.`);
  console.log();
  console.log(`  Hand-back to check-thermal-multitile.ts: R_SINK_DEFAULT = ${f2(R_SINK_SPEC)}, valid ONLY`);
  console.log(`  with a spreader specified; without one N1's topology has no valid lumped value`);
  console.log(`  at all and this script is the model to use.`);
}

function reportSensitivity() {
  rule("§8  Sensitivity — which conclusions survive the soft inputs");
  console.log(`  The two softest inputs are h_ext and the effective exterior area. They enter`);
  console.log(`  only as the product h*A, so the sweep varies the product. Ceiling at the`);
  console.log(`  library floor, 25 C, PGS 100 um, over a deliberately wide box:`);
  console.log();
  const areas = [0.090, 0.118, 0.150];
  console.log(`  h_ext \\ A_ext   ` + areas.map((a) => `${f3(a)} m2`.padStart(10)).join("  "));
  for (const h of [6, 7.81, 10, 14]) {
    const cells = areas.map((ae) => {
      const scale = (ae / A_EXT_EFF) * (h / H_EXT_SPEC);
      return String(ceiling(OP.libMin, {
        amb: AMB_NOMINAL, ktSpreader: SPREADERS[3].kt, hExt: H_EXT_SPEC * scale,
      })).padStart(10);
    });
    console.log(`  ${f2(h).padStart(5)} W/m2K   ${cells.join("  ")}`);
  }
  console.log();
  console.log(`  And the R-4 point (${OP.r4} W/tile) over the same box:`);
  for (const h of [6, 7.81, 10, 14]) {
    const cells = areas.map((ae) => {
      const scale = (ae / A_EXT_EFF) * (h / H_EXT_SPEC);
      return String(ceiling(OP.r4, {
        amb: AMB_NOMINAL, ktSpreader: SPREADERS[3].kt, hExt: H_EXT_SPEC * scale,
      })).padStart(10);
    });
    console.log(`  ${f2(h).padStart(5)} W/m2K   ${cells.join("  ")}`);
  }
  console.log();
  console.log(`  ROBUST: the R-4 point is inadmissible across the whole box — that conclusion`);
  console.log(`  does not depend on any soft input here, because the ${f2(R_VIA / A)} K/W via itself`);
  console.log(`  spends more than the face budget at that drive.`);
  console.log(`  SOFT: the library-floor ceiling moves with area and coefficient, so it is a`);
  console.log(`  design target to be verified on the THERM-1b bench (OI-R1-02), not a claim.`);
}

function reportFault() {
  rule("§9  The fault case — and why it is not the fan");
  const q = drive(distributed(6), OP.libMin);
  console.log(`  Occlusion fraction sweep at the library floor, N = 6, 25 C, PGS spreader.`);
  console.log(`  "phi" is the fraction of the vault exterior covered by 3 mm of fabric.`);
  console.log();
  for (const phi of [0, 0.25, 0.5, 0.7, 0.9, 1.0]) {
    const f = steadySink(q, {
      amb: AMB_NOMINAL, ktSpreader: SPREADERS[3].kt, occludedFrac: phi, occlusionR: OCCLUSION.rAdd,
    });
    const t = maxFace(f);
    console.log(`    phi ${phi.toFixed(2)}   T_skin ${f1(Math.max(...f.x))} C   max T_face ${f1(t)} C` +
      `   ${t > FACE_LIMIT ? "OVER 42" : ""}`);
  }
  console.log();
  console.log(`  FMEA-G07-01 / RISK-26 reads "fan/vent fouling or fan failure -> outward`);
  console.log(`  thermal resistance rises -> heat diverted scalp-ward". The mechanism is real`);
  console.log(`  and the hazard is real. The NAMED CAUSE is not: on this architecture the fan`);
  console.log(`  is not in the tile export path (§1b), and what does raise the outward`);
  console.log(`  resistance is a covered vault — a hood, a hat, bedding, a headrest, a pillow`);
  console.log(`  under a supine user. That is a far more probable initiator than fan failure,`);
  console.log(`  and fan RPM (logged to SHDR per CLAUDE.md §5.1, alerted on by SR-FAN-05)`);
  console.log(`  does not observe it. The Path B1 face NTC does, so the SELECTED control`);
  console.log(`  still covers the hazard — the cause list and the predictive layer do not.`);
  console.log(`  Routed to NP-REQ-FANHEALTH-001 / NP-FMEA-GEOM-001 as OI-SINK-04.`);
}

function main() {
  const ok = reportValidation();
  if (VALIDATE_ONLY) { process.exit(ok ? 0 : 1); return; }
  reportTerminus();
  reportBudget();
  reportSpec();
  reportBaseline();
  reportSpreader();
  reportCeilings();
  reportAgainstN1();
  reportSensitivity();
  reportFault();
  console.log();
}

if (import.meta.main) main();
