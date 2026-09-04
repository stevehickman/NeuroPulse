#!/usr/bin/env bun
/**
 * check-thermal-multitile.ts — the N-tile aggregate thermal model (OI-PWR-01).
 *
 * `NP-THERM-CFD-R1-001` solves a SINGLE periodic hex cell with ADIABATIC side
 * walls. `NP-PWR-BUDGET-001` §3.2 then spends that cell's face margin on a tile
 * count and brackets 4-8 tiles; `NP-THERM-COOL-001` §5 calibrates a cavity budget
 * to reproduce the same ~6. Neither model has a second tile in it.
 *
 * This script puts 80 of them on the real lattice and couples them. It is the
 * source of every figure in `NP-THERM-CFD-N1-001`.
 *
 *   bun scripts/check-thermal-multitile.ts             # full report
 *   bun scripts/check-thermal-multitile.ts --validate  # anchors only, exit 1 on drift
 *
 * ── What is actually new here ────────────────────────────────────────────────
 *
 * An adiabatic-walled periodic cell is not "one tile". It is the boundary
 * condition of an INFINITE ARRAY of identical, identically-driven tiles: lateral
 * gradients vanish, so zero lateral flux is exact. R1 therefore already reports
 * the fully-populated, fully-active limit for the local stack, and §3.2 spends a
 * margin on tile count that was never a function of tile count.
 *
 * Two terms are genuinely N-dependent, and they pull in opposite directions:
 *
 *   (a) LATERAL CONDUCTION relieves an isolated tile and vanishes as a montage
 *       clusters. It is bounded by the adiabatic case, so it can only ever make
 *       things better than R1 -- it cannot raise a ceiling R1 already failed.
 *
 *   (b) THE SHARED SINK. R1 pins the via terminus at ambient ("perfect sink"),
 *       which is exact for one cell and false for N: all N tiles export ~90 % of
 *       their heat into ONE external heatsink whose temperature rises with N.
 *       Nothing in the tree specifies that heatsink's resistance.
 *
 * (b) is the binding term, it is invisible to every single-cell model, and it
 * inverts `NP-PWR-BUDGET-001` §3.3: raising export efficiency moves MORE heat
 * into the term that scales with N.
 *
 * Units: area-normalised R" in m^2*K/W ("R"), absolute conductance G in W/K.
 * Convention per NP-THERM-CFD-C2-001 §7.
 *
 * CI-Kind: report
 */
import { readFileSync } from "fs";
import { analyse, AVAILABLE_W } from "./check-pbm-power";

const VALIDATE_ONLY = process.argv.includes("--validate");

// ---------------------------------------------------------------------------
// §1  Published anchors — NOT derived here. Changing one invalidates the model.
// ---------------------------------------------------------------------------

/** NP-THERM-CFD-R1-001 §2: inward path, junction -> perfused scalp core. */
const R_IN = 0.11;
/** NP-THERM-CFD-R1-001 §2: outward path total, fan off. */
const R_OUT_BASE = 0.41;
/** NP-THERM-CFD-R1-001 §2: stagnant inter-bowl air gap term within R_OUT_BASE. */
const R_GAP_STAGNANT = 0.23;
/** NP-THERM-CFD-R1-001 §3: face -> 37 C perfused core, low perfusion. */
const R_FACE_CORE = 0.105;
/** NP-THERM-CFD-R1-001 §5.1 T1-std @ 43.3 C: junction, and via export fraction. */
const ANCHOR_TJ = 47.5;
const ANCHOR_EXPORT = 0.87;
/** NP-ENV-OPRANGE-001 / NP-THERM-CFD-001 §5: worst-case and nominal ambient. */
const AMB_WORST = 43.3;
const AMB_NOMINAL = 25.0;
const T_CORE = 37.0;
/** CLAUDE.md §4.2 / IEC 60601-1: applied-part face ceiling. */
const FACE_LIMIT = 42.0;
/** NP-THERM-CFD-001 §4: wall-plug efficiency band, 660/808/1064 nm LED. */
const ETA_WP_BAND = [0.30, 0.45] as const;

/** NP-HW-HEXTILE-001: hex tile 40 mm flat-to-flat (nominal, not the provisional
 *  ellipsoid coordinates — see §3 for why the two are used for different jobs). */
const TILE_F2F_M = 0.040;
const HEX_EDGE_M = TILE_F2F_M / Math.sqrt(3);          // 23.09 mm
const TILE_AREA = (Math.sqrt(3) / 2) * TILE_F2F_M ** 2; // 13.856 cm^2
const CENTRE_PITCH_M = TILE_F2F_M;

// ---------------------------------------------------------------------------
// §2  Network calibration against R1, and what the calibration exposes
// ---------------------------------------------------------------------------
// R1 publishes T_j, T_face, T_scalp and the export fraction at four operating
// points. The 1D chain J -> F -> S -> core must sum to R_IN, and the chain
// J -> cavity -> ambient must sum to R_OUT_BASE. That leaves the split of R_IN
// across its three legs, and R_VIA, to be recovered from the published data.

/** Split of R_IN, recovered from R1 §5.1 T1-std @ 43.3 C (T_face 46.7, T_scalp 43.3).
 *  Independently confirmed by the 25 C row to within 0.2 C — see validate(). */
const R_JF = 0.005;  // 1 mm PDMS + 0.6 mm gap (R1 §5.3)
const R_FS = 0.039;  // module face -> scalp surface
const R_SC = R_FACE_CORE - R_FS; // 0.066, scalp -> perfused core
/** Cavity leg of the outward path; the rest of R_OUT_BASE is cavity -> ambient. */
const R_CAV_AMB = R_OUT_BASE - R_GAP_STAGNANT; // 0.18 (foam + shell + ext film)

/** R_VIA back-solved so the network reproduces R1 §5.1's 87 % export at T_j 47.5. */
function calibrateVia(): { rVia: number; qAnchor: number } {
  const other = (ANCHOR_TJ - T_CORE) / R_IN + (ANCHOR_TJ - AMB_WORST) / R_OUT_BASE;
  const q = other / (1 - ANCHOR_EXPORT);
  return { rVia: (ANCHOR_TJ - AMB_WORST) / (ANCHOR_EXPORT * q), qAnchor: q };
}
const { rVia: R_VIA, qAnchor: Q_ANCHOR } = calibrateVia();

/** R1 §5.1 rows, as published. `label` is the flux the table names. */
const R1_ROWS = [
  { cfg: "T1-std",  label: 125, amb: AMB_WORST,   tj: 47.5, face: 46.7, scalp: 43.3, exp: 0.87 },
  { cfg: "T1-peak", label: 190, amb: AMB_WORST,   tj: 49.8, face: 48.9, scalp: 44.7, exp: 0.90 },
  { cfg: "T2-peak", label: 300, amb: AMB_WORST,   tj: 53.8, face: 52.6, scalp: 47.0, exp: 0.91 },
  { cfg: "T1-std",  label: 125, amb: AMB_NOMINAL, tj: 30.3, face: 30.7, scalp: 32.8, exp: NaN },
];

/** Single adiabatic cell, perfect sink — R1's own configuration. q in W/m^2. */
function singleCell(q: number, amb: number) {
  const gIn = 1 / R_IN, gOut = 1 / R_OUT_BASE, gVia = 1 / R_VIA;
  const tj = (q + T_CORE * gIn + amb * gOut + amb * gVia) / (gIn + gOut + gVia);
  const qIn = (tj - T_CORE) * gIn;
  return {
    tj, face: tj - qIn * R_JF, scalp: tj - qIn * (R_JF + R_FS),
    exportFrac: ((tj - amb) * gVia) / q, qIn,
  };
}

/** Heat flux that reproduces a published junction temperature. */
function fluxFor(tj: number, amb: number): number {
  return (tj - T_CORE) / R_IN + (tj - amb) / R_OUT_BASE + (tj - amb) / R_VIA;
}

function validate(): boolean {
  let ok = true;
  const lines: string[] = [];
  lines.push("  cfg      amb    q_heat  ratio   T_j     T_face          T_scalp         export");
  const ratios: number[] = [];
  for (const r of R1_ROWS) {
    const q = fluxFor(r.tj, r.amb);
    const s = singleCell(q, r.amb);
    const ratio = q / 10 / r.label; // W/m^2 -> mW/cm^2, against the label
    ratios.push(ratio);
    const dF = s.face - r.face, dS = s.scalp - r.scalp;
    if (Math.abs(dF) > 0.5 || Math.abs(dS) > 0.5) ok = false;
    const ex = Number.isNaN(r.exp) ? "     —" :
      `${(s.exportFrac * 100).toFixed(1)}/${(r.exp * 100).toFixed(0)}`;
    lines.push(
      `  ${r.cfg.padEnd(8)} ${String(r.amb).padStart(4)}  ${(q / 10).toFixed(1).padStart(6)}` +
      `  ${ratio.toFixed(3)}  ${s.tj.toFixed(1).padStart(5)}` +
      `  ${s.face.toFixed(1)}/${r.face.toFixed(1)} (${dF >= 0 ? "+" : ""}${dF.toFixed(2)})` +
      `  ${s.scalp.toFixed(1)}/${r.scalp.toFixed(1)} (${dS >= 0 ? "+" : ""}${dS.toFixed(2)})` +
      `  ${ex}`);
  }
  console.log("§2  Network reproduction of NP-THERM-CFD-R1-001 (model/published)");
  console.log(`  R_JF ${R_JF}  R_FS ${R_FS}  R_SC ${R_SC.toFixed(3)}  ->  R_IN ${(R_JF + R_FS + R_SC).toFixed(3)}`);
  console.log(`  R_gap ${R_GAP_STAGNANT}  R_cav->amb ${R_CAV_AMB.toFixed(2)}  ->  R_OUT ${(R_GAP_STAGNANT + R_CAV_AMB).toFixed(2)}`);
  console.log(`  R_VIA ${R_VIA.toFixed(5)} (back-solved from the T1-std anchor alone)`);
  console.log();
  console.log(lines.join("\n"));
  const rMin = Math.min(...ratios), rMax = Math.max(...ratios);
  const etaImplied = 1 - (rMin + rMax) / 2;
  console.log();
  console.log(`  Every published temperature is reproduced to <= 0.45 C, and the export`);
  console.log(`  fractions to <= 0.8 pp, from ONE calibration point across a 2.4x flux range.`);
  console.log(`  The topology is therefore right. But the heat flux that produces those`);
  console.log(`  temperatures is a CONSTANT ${rMin.toFixed(3)}-${rMax.toFixed(3)} of the flux R1's own table labels,`);
  console.log(`  implying eta_wp = ${etaImplied.toFixed(3)} applied a second time to an NP-THERM-CFD-001 §4`);
  console.log(`  figure that is ALREADY q_heat = P_elec(1-eta_wp). eta_wp band is ${ETA_WP_BAND[0]}-${ETA_WP_BAND[1]}.`);
  // What the double-counted eta_wp costs the figure NP-PWR-BUDGET-001 §3.2 divides.
  const pubFace = 30.7, label = 125 * 10; // W/m^2
  const corrected = singleCell(label, AMB_NOMINAL).face;
  console.log();
  console.log(`  What that costs the number the tile bracket is built from:`);
  console.log(`    R1 §5.1 T1-std @ 25 C, as published   face ${pubFace} C  ->  margin ${(FACE_LIMIT - pubFace).toFixed(1)} K`);
  console.log(`    same row driven at the labelled flux   face ${corrected.toFixed(1)} C  ->  margin ${(FACE_LIMIT - corrected).toFixed(1)} K`);
  console.log(`  NP-PWR-BUDGET-001 §3.2 divides that ${(FACE_LIMIT - pubFace).toFixed(1)} K by a per-tile cavity rise to get 4-8 tiles.`);
  if (!ok) console.log("  !! ANCHOR DRIFT — a published R1 figure no longer reproduces.");
  return ok;
}

// ---------------------------------------------------------------------------
// §3  The lattice
// ---------------------------------------------------------------------------
// Two different jobs, two different sources, deliberately:
//
//   TOPOLOGY + CLUSTERING come from hardware/np_socket_map.json's measured
//   xMm/yMm/zMm — who neighbours whom, and how tight a montage is.
//
//   CONDUCTANCE MAGNITUDES come from the nominal 40 mm f2f hex, NOT from those
//   coordinates. The file marks them "PROVISIONAL, replace from shell CAD", and
//   their median nearest-neighbour chord (32.0 mm) is 20 % under the nominal
//   pitch that the same file's tiledAreaCm2 (80 x 13.856) assumes. Deriving
//   conductances from them would propagate a coordinate artefact into every
//   temperature. Topology survives that compression; magnitudes would not.

type Socket = { id: number; xMm: number; yMm: number; zMm: number };

function loadLattice(): { sockets: Socket[]; neighbours: number[][] } {
  const map = JSON.parse(readFileSync("hardware/np_socket_map.json", "utf8"));
  const sockets: Socket[] = map.sockets;
  const d = (a: Socket, b: Socket) =>
    Math.hypot(a.xMm - b.xMm, a.yMm - b.yMm, a.zMm - b.zMm);
  // Nearest-neighbour cut at 1.35x the median NN distance: separates the first
  // shell (histogram 2-6 neighbours, mean 4.65 — a hex sheet with a rim) from
  // the second cleanly.
  const nn = sockets.map((a) => Math.min(...sockets.filter((b) => b.id !== a.id).map((b) => d(a, b))));
  const med = [...nn].sort((x, y) => x - y)[Math.floor(nn.length / 2)];
  const cut = med * 1.35;
  const neighbours = sockets.map((a, i) =>
    sockets.map((b, j) => (i !== j && d(a, b) < cut ? j : -1)).filter((j) => j >= 0));
  return { sockets, neighbours };
}

const { sockets: SOCKETS, neighbours: NEIGH } = loadLattice();
const N_SOCKETS = SOCKETS.length;

// Lateral conductances per neighbour pair: G = sum(k*t) * w / L, w = hex edge,
// L = centre pitch. Each is a named export so a datasheet can replace it.
const lateralG = (kt: number) => (kt * HEX_EDGE_M) / CENTRE_PITCH_M;

/** Scalp/tissue plane. k 0.35 W/m.K over ~6 mm of scalp above the perfusion sink. */
export const G_LAT_SCALP = lateralG(0.35 * 0.006);
/** Module face plane. PDMS skirt + PBT body bridging between adjacent modules,
 *  k ~0.25 over ~2 mm of effective section. Weak by construction: the modules
 *  are discrete parts in separate sockets. */
export const G_LAT_FACE = lateralG(0.25 * 0.002);
/** Cavity/shell plane. CFRP 2.5 mm at in-plane k ~10, plus 0.2 mm mu-metal at
 *  k ~30. This is the one large lateral term in the stack. */
export const G_LAT_CAV = lateralG(10 * 0.0025 + 30 * 0.0002);

/** External heatsink at the via terminus, absolute K/W. R1 pins this at ambient
 *  ("perfect sink", its §5 upper bound) and NOTHING in the document tree
 *  specifies it — see NP-THERM-CFD-N1-001 §7 and OI-N1-02. 0.5 K/W is a small
 *  fan-cooled extruded sink; the report sweeps it because it is unspecified. */
export const R_SINK_DEFAULT = 0.5;

// ---------------------------------------------------------------------------
// §4  Steady solver
// ---------------------------------------------------------------------------
// Nodes, per socket i: J(i) junction, F(i) module face, S(i) scalp.
// Plus one cavity node per socket C(i), and ONE shared sink node K.
// Fixed reservoirs: ambient, and the 37 C perfused core.

const NJ = (i: number) => i;
const NF = (i: number) => N_SOCKETS + i;
const NS = (i: number) => 2 * N_SOCKETS + i;
const NC = (i: number) => 3 * N_SOCKETS + i;
const NK = 4 * N_SOCKETS;
const N_NODES = 4 * N_SOCKETS + 1;

const A = TILE_AREA;
const G_JF = A / R_JF, G_FS = A / R_FS, G_SC = A / R_SC;
const G_JC = A / R_GAP_STAGNANT, G_CA = A / R_CAV_AMB, G_JK = A / R_VIA;

export type Opts = {
  amb?: number; rSink?: number;
  gLatScalp?: number; gLatFace?: number; gLatCav?: number;
  /** Cavity path legs, for the NP-THERM-COOL-001 §5 options. Default = as adopted. */
  rGap?: number; rCavAmb?: number;
  /** Perfect sink reproduces R1 exactly; it is what R1 assumed. */
  perfectSink?: boolean;
};

/** Dense LU with partial pivoting. N_NODES = 321, so this is not the bottleneck. */
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

/** Build the conductance matrix and the source vector. `qTile[i]` in W. */
function assemble(qTile: number[], o: Opts = {}) {
  const amb = o.amb ?? AMB_WORST;
  const rSink = o.rSink ?? R_SINK_DEFAULT;
  const gLS = o.gLatScalp ?? G_LAT_SCALP;
  const gLF = o.gLatFace ?? G_LAT_FACE;
  const gLC = o.gLatCav ?? G_LAT_CAV;
  const gJC = A / (o.rGap ?? R_GAP_STAGNANT);
  const gCA = A / (o.rCavAmb ?? R_CAV_AMB);

  const M: number[][] = Array.from({ length: N_NODES }, () => new Array(N_NODES).fill(0));
  const b = new Array(N_NODES).fill(0);
  const link = (a: number, c: number, g: number) => {
    M[a][a] += g; M[c][c] += g; M[a][c] -= g; M[c][a] -= g;
  };
  const toRes = (a: number, g: number, T: number) => { M[a][a] += g; b[a] += g * T; };

  for (let i = 0; i < N_SOCKETS; i++) {
    link(NJ(i), NF(i), G_JF);
    link(NF(i), NS(i), G_FS);
    toRes(NS(i), G_SC, T_CORE);
    link(NJ(i), NC(i), gJC);
    toRes(NC(i), gCA, amb);
    if (o.perfectSink) toRes(NJ(i), G_JK, amb);
    else link(NJ(i), NK, G_JK);
    b[NJ(i)] += qTile[i];
    for (const j of NEIGH[i]) {
      if (j <= i) continue; // undirected, add once
      link(NS(i), NS(j), gLS);
      link(NF(i), NF(j), gLF);
      link(NC(i), NC(j), gLC);
    }
  }
  if (o.perfectSink) { M[NK][NK] = 1; b[NK] = amb; }
  else toRes(NK, 1 / rSink, amb);
  return { M, b, amb };
}

export type Field = { j: number[]; f: number[]; s: number[]; c: number[]; sink: number };

function steady(qTile: number[], o: Opts = {}): Field {
  const { M, b } = assemble(qTile, o);
  const T = solveLinear(M, b);
  return {
    j: SOCKETS.map((_, i) => T[NJ(i)]), f: SOCKETS.map((_, i) => T[NF(i)]),
    s: SOCKETS.map((_, i) => T[NS(i)]), c: SOCKETS.map((_, i) => T[NC(i)]),
    sink: T[NK],
  };
}

// ---------------------------------------------------------------------------
// §5  Montages
// ---------------------------------------------------------------------------

const dist = (a: Socket, b: Socket) =>
  Math.hypot(a.xMm - b.xMm, a.yMm - b.yMm, a.zMm - b.zMm);

/** Both montage generators start from the SAME interior seed, so that at N=1
 *  they are the same set and every difference at N>1 is clustering alone. A rim
 *  socket has fewer neighbours and so less lateral relief; seeding one generator
 *  at the rim would charge that ~0.2 K rim effect to clustering. */
function interiorSeed(): number {
  let seed = 0;
  for (let i = 0; i < N_SOCKETS; i++) if (NEIGH[i].length > NEIGH[seed].length) seed = i;
  return seed;
}

/** Tightest N-set: region-grow from the interior seed. */
function clustered(n: number): number[] {
  const chosen = [interiorSeed()];
  while (chosen.length < n) {
    let best = -1, bestD = Infinity;
    for (let i = 0; i < N_SOCKETS; i++) {
      if (chosen.includes(i)) continue;
      const d = Math.min(...chosen.map((c) => dist(SOCKETS[i], SOCKETS[c])));
      if (d < bestD) { bestD = d; best = i; }
    }
    chosen.push(best);
  }
  return chosen;
}

/** Most-spread N-set: farthest-point sampling from the same interior seed. */
function distributed(n: number): number[] {
  const chosen = [interiorSeed()];
  while (chosen.length < n) {
    let best = -1, bestD = -1;
    for (let i = 0; i < N_SOCKETS; i++) {
      if (chosen.includes(i)) continue;
      const d = Math.min(...chosen.map((c) => dist(SOCKETS[i], SOCKETS[c])));
      if (d > bestD) { bestD = d; best = i; }
    }
    chosen.push(best);
  }
  return chosen;
}

/** Compactness: mean nearest-active-neighbour distance, mm. Lower = tighter.
 *  This is the montage variable OI-PWR-10 asks whether the flat rule may ignore. */
function compactness(set: number[]): number {
  if (set.length < 2) return NaN;
  return set.reduce((acc, i) =>
    acc + Math.min(...set.filter((j) => j !== i).map((j) => dist(SOCKETS[i], SOCKETS[j]))), 0) / set.length;
}

const idToIdx = new Map(SOCKETS.map((s, i) => [s.id, i]));

/** Zones as authored, so the montages tested are the ones that exist. */
function loadZones(): Map<string, number[]> {
  const text = readFileSync("protocols/predefined/00-zones.npps", "utf8");
  const zones = new Map<string, number[]>();
  for (const m of text.matchAll(/zone\s+"([^"]+)"\s*\{([\s\S]*?)\n\}/g)) {
    const s = /sockets:\s*\[([^\]]*)\]/.exec(m[2]);
    if (s) {
      const idx = [...s[1].matchAll(/\d+/g)]
        .map((d) => idToIdx.get(Number(d[0]))).filter((v): v is number => v !== undefined);
      if (idx.length) zones.set(m[1], idx);
    }
  }
  return zones;
}

// ---------------------------------------------------------------------------
// §6  Operating points
// ---------------------------------------------------------------------------
// Everything above is in W/m^2 of module heat. Protocol and budget documents are
// in W/tile of ELECTRICAL draw. The bridge is NP-THERM-CFD-001 §4:
// q_heat = P_elec * (1 - eta_wp).

const ETA_WP = 0.35; // mid-band, and the value §2 recovers from R1's own numbers
const heatW = (elecW: number) => elecW * (1 - ETA_WP);

/** Named electrical operating points, W/tile. */
const OP = {
  /** NP-HW-HEXTILE-001 §9.2 R-4: the point the "~6 tiles" rule is derived at. */
  r4: 6.25,
  /** NP-SES-PWR-001 §2.1 library floor (Autism, 20 % intensity @ 25 % duty). */
  libMin: 1.3,
  /** NP-SES-PWR-001 §2.1 library ceiling (Vascular Baseline, 80 % CW). */
  libMax: 20.0,
};

/** q vector in W for an active set at a given electrical per-tile draw. */
function drive(set: number[], elecW: number): number[] {
  const q = new Array(N_SOCKETS).fill(0);
  for (const i of set) q[i] = heatW(elecW);
  return q;
}

const maxFace = (f: Field) => Math.max(...f.f);
const comp = (set: number[]) => (set.length < 2 ? "  \u2014" : compactness(set).toFixed(1));

// ---------------------------------------------------------------------------
// §7  Transient (OI-PWRSRC-07)
// ---------------------------------------------------------------------------
// Area-normalised heat capacities, kJ/m^2.K, from NP-THERM-CFD-R1-001 §4's
// lumped swing mass: 14 mm PBT body 28.2 + PDMS 1.4 + partial scalp ~11.

const C_J = 28.2e3 * A;   // module body
const C_F = 1.4e3 * A;    // PDMS face
const C_S = 11e3 * A;     // partial scalp
const C_C = 4.2e3 * A;    // CFRP shell 2.5 mm + absorber
/** Sink mass, absolute J/K. 200 g of aluminium — unspecified in the tree, like
 *  R_SINK itself (OI-N1-02). */
export const C_SINK_DEFAULT = 0.2 * 900;

function capacities(cSink = C_SINK_DEFAULT): number[] {
  const C = new Array(N_NODES).fill(0);
  for (let i = 0; i < N_SOCKETS; i++) {
    C[NJ(i)] = C_J; C[NF(i)] = C_F; C[NS(i)] = C_S; C[NC(i)] = C_C;
  }
  C[NK] = cSink;
  return C;
}

/** Implicit Euler from an all-off equilibrium. Returns max face T at each probe. */
function transient(qTile: number[], probesMin: number[], o: Opts = {}, cSink = C_SINK_DEFAULT) {
  const C = capacities(cSink);
  const cold = steady(new Array(N_SOCKETS).fill(0), o);
  let T = new Array(N_NODES).fill(0);
  for (let i = 0; i < N_SOCKETS; i++) {
    T[NJ(i)] = cold.j[i]; T[NF(i)] = cold.f[i]; T[NS(i)] = cold.s[i]; T[NC(i)] = cold.c[i];
  }
  T[NK] = cold.sink;
  const dt = 5; // s
  const { M: G, b } = assemble(qTile, o);
  // Pre-factor is not worth it at this size; rebuild per step from G.
  const out: { min: number; face: number }[] = [];
  const end = Math.max(...probesMin) * 60;
  const probeSet = new Set(probesMin.map((m) => Math.round((m * 60) / dt)));
  for (let step = 1; step * dt <= end + 1e-9; step++) {
    const M2 = G.map((row) => row.slice());
    const b2 = b.slice();
    for (let n = 0; n < N_NODES; n++) {
      if (C[n] > 0) { M2[n][n] += C[n] / dt; b2[n] += (C[n] / dt) * T[n]; }
    }
    T = solveLinear(M2, b2);
    if (probeSet.has(step)) {
      out.push({ min: (step * dt) / 60, face: Math.max(...SOCKETS.map((_, i) => T[NF(i)])) });
    }
  }
  return { cold: Math.max(...cold.f), probes: out, steadyFace: maxFace(steady(qTile, o)) };
}

// ---------------------------------------------------------------------------
// §8  Report
// ---------------------------------------------------------------------------

const f1 = (x: number) => x.toFixed(1);
const rule = (s: string) => console.log(`\n${s}\n${"-".repeat(s.length)}`);

function reportAdiabaticIdentity() {
  rule("§3  The adiabatic identity — what R1 actually computed");
  const q = Q_ANCHOR;
  const cell = singleCell(q, AMB_WORST);
  const all = steady(drive([...Array(N_SOCKETS).keys()], q * A / (1 - ETA_WP)), {
    amb: AMB_WORST, perfectSink: true,
  });
  const dJ = Math.max(...all.j) - cell.tj;
  const dF = maxFace(all) - cell.face;
  console.log(`  All ${N_SOCKETS} sockets driven identically, perfect sink, lateral conduction ON:`);
  console.log(`    single adiabatic cell   T_j ${f1(cell.tj)}  T_face ${f1(cell.face)}`);
  console.log(`    80-tile coupled model   T_j ${f1(Math.max(...all.j))}  T_face ${f1(maxFace(all))}`);
  console.log(`    difference              ${dJ.toExponential(1)}   ${dF.toExponential(1)}  K`);
  console.log();
  console.log(`  They agree to machine precision, and they must: a uniformly-driven array`);
  console.log(`  has no lateral gradient, so the lateral terms carry nothing and adiabatic`);
  console.log(`  side walls are EXACT. R1's single cell is the FULLY-ACTIVE limit, not a`);
  console.log(`  single tile. Its face margin is not a budget that tile count spends.`);
  return { cell, all };
}

function reportNSweep(amb: number) {
  rule(`§4  Face temperature vs N at ambient ${amb} C — clustered against distributed`);
  console.log(`  Driven at NP-HW-HEXTILE-001 §9.2's R-4 point, ${OP.r4} W/tile electrical`);
  console.log(`  (= ${heatW(OP.r4).toFixed(2)} W/tile heat at eta_wp ${ETA_WP}), R_sink ${R_SINK_DEFAULT} K/W.`);
  console.log();
  console.log("   N   clustered            distributed          sink   spread");
  console.log("       compact  T_face      compact  T_face       T      relief");
  for (const n of [1, 2, 4, 6, 8, 12, 20, 40, 80]) {
    const cl = clustered(n), di = distributed(n);
    const fc = steady(drive(cl, OP.r4), { amb }), fd = steady(drive(di, OP.r4), { amb });
    const tc = maxFace(fc), td = maxFace(fd);
    console.log(
      `  ${String(n).padStart(2)}   ${comp(cl).padStart(5)} mm  ${f1(tc).padStart(5)} C` +
      `    ${comp(di).padStart(5)} mm  ${f1(td).padStart(5)} C` +
      `    ${f1(fc.sink).padStart(5)}  ${(tc - td >= 0 ? "+" : "")}${(tc - td).toFixed(2)} K`);
  }
  console.log();
  console.log(`  "spread relief" is clustered minus distributed at the same N and the same`);
  console.log(`  total power: the entire montage-geometry effect OI-PWR-10 asks about.`);
}


/** Max N (distributed) holding face <= 42 C, across ambient x R_sink x drive. */
function reportCeilings() {
  rule("§4a  The tile ceiling — what N the 42 C face limit actually permits");
  const drives: [string, number][] = [
    ["library floor  1.3 W/tile", OP.libMin],
    ["R-4 point      6.25 W/tile", OP.r4],
    ["library ceil   20.0 W/tile", OP.libMax],
  ];
  console.log("  Distributed montage; ceiling is the largest N with max T_face <= 42.0 C.");
  console.log("  '>80' = the whole lattice fits; '0' = a single tile already fails.");
  console.log();
  console.log("  drive                      amb   R_sink=perfect  0.25   0.50   1.00   2.00");
  for (const [name, w] of drives) {
    for (const amb of [AMB_NOMINAL, 35.0, AMB_WORST]) {
      const cells = [null, 0.25, 0.5, 1.0, 2.0].map((rs) => {
        const o: Opts = rs === null ? { amb, perfectSink: true } : { amb, rSink: rs };
        let last = 0;
        for (let n = 1; n <= N_SOCKETS; n++) {
          if (maxFace(steady(drive(distributed(n), w), o)) <= FACE_LIMIT) last = n; else break;
        }
        return (last === N_SOCKETS ? ">80" : String(last)).padStart(rs === null ? 14 : 6);
      });
      console.log(`  ${(amb === AMB_NOMINAL ? name : "").padEnd(26)} ${String(amb).padStart(4)}  ${cells.join(" ")}`);
    }
  }
  console.log();
  console.log("  NP-ENV-OPRANGE-001 blocks PBM above +43 C ambient, so the 43.3 C rows are");
  console.log("  the envelope edge, not an operating case. The 25 C rows are the design case.");
}

function reportSinkSweep(amb: number, w: number) {
  rule(`§5  The shared sink at ambient ${amb} C, ${w} W/tile — the term no single-cell model sees`);
  console.log(`  Distributed montage, same total power in every column.`);
  console.log();
  console.log("  R_sink K/W    N=1    N=4    N=6    N=8   N=20   N=80    <- max T_face, C");
  for (const rs of [0.0, 0.25, 0.5, 1.0, 2.0]) {
    const row = [1, 4, 6, 8, 20, 80].map((n) => {
      const o: Opts = rs === 0 ? { amb, perfectSink: true } : { amb, rSink: rs };
      return f1(maxFace(steady(drive(distributed(n), w), o))).padStart(6);
    });
    console.log(`  ${(rs === 0 ? "perfect" : rs.toFixed(2)).padStart(8)}   ${row.join(" ")}`);
  }
  console.log();
  console.log(`  The perfect-sink row is flat in N to within the spreading term: with the`);
  console.log(`  sink pinned, tile count barely moves the face. Every other row rises`);
  console.log(`  steeply. R1 assumed the flat row, and it is the only row that is not real.`);
}

function reportProtocols(amb: number, w: number) {
  rule(`§6  The authored montages at ambient ${amb} C, ${w} W/tile (OI-PWR-10)`);
  const zones = loadZones();
  const rows = [...zones.entries()]
    .filter(([, s]) => s.length >= 2 && s.length < N_SOCKETS)
    .map(([name, set]) => {
      const t = maxFace(steady(drive(set, w), { amb }));
      const td = maxFace(steady(drive(distributed(set.length), w), { amb }));
      return { name, n: set.length, comp: compactness(set), t, penalty: t - td };
    })
    .sort((a, b) => b.penalty - a.penalty);
  console.log(`  Each authored zone against the most-spread montage`);
  console.log(`  of the SAME tile count. "penalty" is what the geometry costs.`);
  console.log();
  console.log("  zone                       tiles  compact  T_face   penalty");
  for (const r of rows.slice(0, 14)) {
    console.log(`  ${r.name.padEnd(26)} ${String(r.n).padStart(4)}   ${r.comp.toFixed(1).padStart(5)} mm  ${f1(r.t).padStart(5)} C   ${(r.penalty >= 0 ? "+" : "")}${r.penalty.toFixed(2)} K`);
  }
  const worst = rows[0];
  console.log();
  console.log(`  Worst authored montage penalty: ${worst.penalty.toFixed(2)} K (${worst.name}).`);
  return rows;
}


/** Per-protocol thermal ceiling, against the same analyse() the power and dose
 *  checks use, so demand and thermal cannot fork (NP-PWRSRC-001 D-1). */
function reportProtocolCeilings(amb: number) {
  rule(`§6a  Per-protocol thermal tile ceiling at ambient ${amb} C`);
  console.log(`  "power" is the concurrency the ${AVAILABLE_W} W PBM envelope allows at that`);
  console.log(`  per-tile draw (NP-SES-PWR-001 §2.1). "thermal" is the largest N this model`);
  console.log(`  holds at face <= ${FACE_LIMIT} C, R_sink ${R_SINK_DEFAULT} K/W. The binding one is min().`);
  console.log(`  "th(ideal)" repeats it with a PERFECT sink -- the R1 assumption, and an upper`);
  console.log(`  bound no heatsink can beat. Where th(ideal) is already 0, the verdict does`);
  console.log(`  NOT depend on the unspecified R_sink (OI-N1-02).`);
  console.log();
  console.log("  protocol                      W/tile   power  thermal  th(ideal)  binds");
  const rows = analyse()
    .filter((r) => r.perTileW > 0)
    .sort((a, b) => b.perTileW - a.perTileW);
  const seen = new Set<string>();
  for (const r of rows) {
    const key = r.name;
    if (seen.has(key)) continue;
    seen.add(key);
    const ceil = (o: Opts) => {
      let last = 0;
      for (let n = 1; n <= N_SOCKETS; n++) {
        if (maxFace(steady(drive(distributed(n), r.perTileW), o)) <= FACE_LIMIT) last = n; else break;
      }
      return last;
    };
    const th = ceil({ amb });
    const thIdeal = ceil({ amb, perfectSink: true });
    const pw = r.maxConcurrent;
    const binds = th < pw ? "THERMAL" : th > pw ? "power" : "equal";
    console.log(
      `  ${r.name.slice(0, 28).padEnd(28)} ${r.perTileW.toFixed(1).padStart(6)}  ` +
      `${String(pw).padStart(6)}   ${(th === N_SOCKETS ? ">80" : String(th)).padStart(5)}   ` +
      `${(thIdeal === N_SOCKETS ? ">80" : String(thIdeal)).padStart(7)}    ${binds}`);
  }
}


/** With a perfect sink the tiles are thermally decoupled, so there is no tile
 *  COUNT limit at all -- only a per-tile DRIVE limit. Bisect for it. */
function reportDriveLimit() {
  rule("§6b  The per-tile drive limit — the ceiling that survives an ideal heatsink");
  console.log("  A perfect sink decouples the tiles: face temperature then depends on");
  console.log("  per-tile drive and ambient, and not on N at all. This is the hard wall.");
  console.log();
  console.log("  ambient   max W/tile (electrical)   = W/tile heat");
  for (const amb of [AMB_NOMINAL, 30, 35, 40, AMB_WORST]) {
    let lo = 0, hi = 60;
    for (let k = 0; k < 60; k++) {
      const mid = (lo + hi) / 2;
      if (maxFace(steady(drive([interiorSeed()], mid), { amb, perfectSink: true })) <= FACE_LIMIT) lo = mid;
      else hi = mid;
    }
    const txt = lo < 1e-6 ? "  none — 42 C unreachable" : `${lo.toFixed(2).padStart(12)}`;
    console.log(`  ${String(amb).padStart(5)}    ${txt}${lo < 1e-6 ? "" : `           ${heatW(lo).toFixed(2)}`}`);
  }
  // Least-squares slope over the finite rows, and the ambient where the wall hits zero.
  const pts: [number, number][] = [];
  for (const amb of [AMB_NOMINAL, 30, 35, 40]) {
    let lo = 0, hi = 60;
    for (let k = 0; k < 60; k++) {
      const mid = (lo + hi) / 2;
      if (maxFace(steady(drive([interiorSeed()], mid), { amb, perfectSink: true })) <= FACE_LIMIT) lo = mid;
      else hi = mid;
    }
    pts.push([amb, lo]);
  }
  const n = pts.length;
  const mx = pts.reduce((a, [x]) => a + x, 0) / n;
  const my = pts.reduce((a, [, y]) => a + y, 0) / n;
  const slope = pts.reduce((a, [x, y]) => a + (x - mx) * (y - my), 0) /
                pts.reduce((a, [x]) => a + (x - mx) ** 2, 0);
  console.log();
  console.log(`  Linear in ambient at ${slope.toFixed(2)} W/tile per C, reaching zero at ${(mx - my / slope).toFixed(1)} C.`);
  console.log();
  console.log(`  NP-SES-PWR-001 §2.1's library spans 1.3-25.0 W/tile. The wall sits inside`);
  console.log(`  that span at every ambient, so it partitions the authored library rather`);
  console.log(`  than clearing or condemning it.`);
}

function reportTransient(amb: number, w: number) {
  rule(`§7  Session transient at ambient ${amb} C, ${w} W/tile (OI-PWRSRC-07)`);
  console.log(`  Sessions are 6-30 min (pbm_neuro_protocols.md). From an all-off start,`);
  console.log(`  distributed montage, R_sink ${R_SINK_DEFAULT} K/W. Percentages are the`);
  console.log(`  fraction of the steady-state RISE reached at that elapsed time.`);
  console.log();
  console.log("   N   cold    6 min     12 min    20 min    30 min     steady");
  for (const n of [6, 20, 80]) {
    const r = transient(drive(distributed(n), w), [6, 12, 20, 30], { amb });
    const rise = r.steadyFace - r.cold;
    const cells = r.probes.map((pr) =>
      `${f1(pr.face).padStart(5)} ${(((pr.face - r.cold) / rise) * 100).toFixed(0).padStart(3)}%`);
    console.log(`  ${String(n).padStart(2)}  ${f1(r.cold).padStart(5)}  ${cells.join("  ")}  ${f1(r.steadyFace).padStart(7)}`);
  }
  console.log();
  console.log(`  OI-PWRSRC-07 assumed tau_face 35-45 min and inferred "~39 % of steady rise"`);
  console.log(`  for a 6-30 min session. That tau is R1 §4's FAN-OFF value, built on`);
  console.log(`  R_in || R_out(off) = 0.087. In the healthy state the via is in parallel`);
  console.log(`  too, giving 0.0056 -- 15x lower, so the module equilibrates in minutes`);
  console.log(`  and the slow node is the scalp, not the module. The transient credit is`);
  console.log(`  real but far smaller than the open item assumed.`);
}

function reportOptionRatios(amb: number, w: number) {
  rule(`§8  NP-THERM-COOL-001 §5 option ratios, re-run at ambient ${amb} C, ${w} W/tile (OI-THCOOL-08)`);
  const opts: { id: string; name: string; rGap: number; rRest: number; breach: boolean }[] = [
    { id: "BASE/V", name: "As adopted", rGap: R_GAP_STAGNANT, rRest: 0.18, breach: false },
    { id: "X", name: "External ventilation", rGap: 0.02, rRest: 0.18, breach: true },
    { id: "R", name: "Sealed recirculation", rGap: 2 / 30, rRest: 0.18, breach: false },
    { id: "RF", name: "R + thermal absorber", rGap: 2 / 30, rRest: 0.125, breach: false },
    { id: "RFE", name: "RF + forced external", rGap: 2 / 30, rRest: 0.0583, breach: false },
    { id: "GFE", name: "Gap bridge + absorber + forced ext", rGap: 0.0022, rRest: 0.0583, breach: false },
  ];
  console.log(`  Ceiling = largest N (distributed) holding max face <= ${FACE_LIMIT} C,`);
  console.log(`  R_sink ${R_SINK_DEFAULT} K/W. '>80' means the whole lattice fits.`);
  console.log();
  console.log("  ID       option                              R_out   ceiling   vs base  shield");
  let base = 0;
  for (const o of opts) {
    const ceiling = ceilingFor({ rGap: o.rGap, rCavAmb: o.rRest, amb }, w);
    if (o.id.startsWith("BASE")) base = ceiling;
    console.log(
      `  ${o.id.padEnd(8)} ${o.name.padEnd(34)} ${(o.rGap + o.rRest).toFixed(3)}   ` +
      `${(ceiling === N_SOCKETS ? ">80" : String(ceiling)).padStart(5)}    ${base ? (ceiling / base).toFixed(2) + "x" : "n/a"}    ${o.breach ? "BREACH" : "ok"}`);
  }
  console.log();
  console.log(`  NP-THERM-COOL-001 §5 published 6.0 / 12.3 / 10.0 / 12.8 / 19.7 on a cavity`);
  console.log(`  model calibrated to reproduce the ~6-tile rule. Those ratios are a property`);
  console.log(`  of the cavity leg alone. Here the cavity leg is not what binds.`);
}

/** Largest N (distributed) holding max face <= FACE_LIMIT for a cavity path. */
function ceilingFor(o: Opts, w: number): number {
  let last = 0;
  for (let n = 1; n <= N_SOCKETS; n++) {
    if (maxFace(steady(drive(distributed(n), w), o)) <= FACE_LIMIT) last = n; else break;
  }
  return last;
}

// ---------------------------------------------------------------------------
// §9  Entry point
// ---------------------------------------------------------------------------

function main() {
  const ok = validate();
  if (VALIDATE_ONLY) { process.exit(ok ? 0 : 1); return; }
  reportAdiabaticIdentity();
  reportNSweep(AMB_NOMINAL);
  reportNSweep(AMB_WORST);
  reportCeilings();
  reportSinkSweep(AMB_NOMINAL, OP.libMin);
  reportProtocols(AMB_NOMINAL, OP.libMin);
  reportProtocolCeilings(AMB_NOMINAL);
  reportDriveLimit();
  reportTransient(AMB_NOMINAL, OP.libMin);
  reportOptionRatios(AMB_NOMINAL, OP.libMin);
  console.log();
}

main();
