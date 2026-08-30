/**
 * NP-THERM-COOL-001 — 1D thermal-network model for the hex-tile cooling options.
 *
 * Reproduces NP-THERM-CFD-R1-001's published resistance network, then evaluates the
 * candidate cooling architectures against it. Every figure in NP-THERM-COOL-001 that
 * is not quoted from another document is produced here.
 *
 *   bun scripts/check-thermal-network.ts
 *
 * Units: area-normalised thermal resistance R" in m^2*K/W throughout ("R" below).
 * Convention follows NP-THERM-CFD-C2-001 §7 (single periodic hex cell, adiabatic walls).
 */

// ---------------------------------------------------------------------------
// §1  Published anchors — NOT derived here. Changing one invalidates the model.
// ---------------------------------------------------------------------------

/** NP-THERM-CFD-R1-001 §2: inward path, junction -> perfused scalp core. */
const R_IN = 0.11;
/** NP-THERM-CFD-R1-001 §2: outward path total, fan off. */
const R_OUT_BASE = 0.41;
/** NP-THERM-CFD-R1-001 §2: the stagnant inter-bowl air gap term within R_OUT_BASE. */
const R_GAP_STAGNANT = 0.23;
/** NP-THERM-CFD-R1-001 §3: face -> 37 C core, low perfusion. Sets the inward flux ceiling. */
const R_FACE_CORE = 0.105;
/** NP-THERM-CFD-R1-001 §3: inward flux ceiling to hold face <= 42 C. */
const Q_INWARD_MAX = 47.4; // W/m^2
/** NP-THERM-CFD-R1-001 §5.1: BN-boss via export fraction at the modelled geometry. */
const VIA_EXPORT_FRACTION = 0.90;
/** NP-HW-HEXTILE-001: hex tile 40 mm flat-to-flat. */
const TILE_F2F_M = 0.040;

const TILE_AREA = (Math.sqrt(3) / 2) * TILE_F2F_M ** 2; // m^2, regular hexagon
const VAULT_AREA = 0.11; // m^2, ~80 sockets x tile area (NP-HELMET-GEOM-001 order)

// ---------------------------------------------------------------------------
// §2  Decomposition of the outward path
// ---------------------------------------------------------------------------
// R_OUT_BASE - R_GAP_STAGNANT = 0.18 is unallocated in the source document.
// Allocated here from the NP-THERM-CFD-C2-001 §2 stack so each term can be
// attacked independently. Sums to 0.18 by construction.

const R_FOAM = 0.075;      // 3 mm carbon-loaded EMI absorber, k ~ 0.04 W/m.K
const R_SHELL = 0.005;     // CFRP 2.5 mm + Pd-polyester 0.1 + mu-metal 0.2
const R_EXT_NATURAL = 0.10; // outer face natural convection, h ~ 10 W/m^2.K

const R_OUT_REMAINDER = R_FOAM + R_SHELL + R_EXT_NATURAL;

// ---------------------------------------------------------------------------
// §3  Option definitions — each names the term it changes and why
// ---------------------------------------------------------------------------

/** Two films in series across a stirred gap: R = 2/h. */
const stirredGap = (h: number) => 2 / h;
/** Forced external convection film. */
const forcedExternal = (h: number) => 1 / h;

interface Option {
  id: string;
  name: string;
  rGap: number;
  rFoam: number;
  rExt: number;
  /** Effective via resistance; Infinity = no via. */
  rVia: number;
  breachesShield: boolean;
  note: string;
}

/**
 * Effective via resistance, back-solved so the network reproduces R1 §5.1's
 * published 90% export rather than asserting an idealised copper number.
 * The ideal 4 mm copper via is ~0.001; the 10x difference is contact,
 * spreading and real-sink resistance, which is where via work must go.
 */
function calibrateVia(exportFraction: number): number {
  const gOther = 1 / R_IN + 1 / R_OUT_BASE;
  const gVia = (exportFraction * gOther) / (1 - exportFraction);
  return 1 / gVia;
}
const R_VIA_EFF = calibrateVia(VIA_EXPORT_FRACTION);

const NO_VIA = Infinity;

const options: Option[] = [
  {
    id: "BASE", name: "As-modelled, fan off, no via",
    rGap: R_GAP_STAGNANT, rFoam: R_FOAM, rExt: R_EXT_NATURAL, rVia: NO_VIA,
    breachesShield: false, note: "NP-THERM-CFD-R1-001 §2 baseline",
  },
  {
    id: "V", name: "BN-boss conductive export (ADOPTED)",
    rGap: R_GAP_STAGNANT, rFoam: R_FOAM, rExt: R_EXT_NATURAL, rVia: R_VIA_EFF,
    breachesShield: false, note: "Cavity stagnant, shield intact",
  },
  {
    id: "X", name: "External cavity ventilation (REJECTED)",
    rGap: 0.02, rFoam: R_FOAM, rExt: R_EXT_NATURAL, rVia: NO_VIA,
    breachesShield: true, note: "Requires inlet+outlet through mu-metal",
  },
  {
    id: "R", name: "Sealed recirculation (stir the cavity)",
    rGap: stirredGap(30), rFoam: R_FOAM, rExt: R_EXT_NATURAL, rVia: R_VIA_EFF,
    breachesShield: false, note: "Closed loop, no gas exchange",
  },
  {
    id: "RF", name: "Recirculation + thermally-specified absorber",
    rGap: stirredGap(30), rFoam: 0.02, rExt: R_EXT_NATURAL, rVia: R_VIA_EFF,
    breachesShield: false, note: "Absorber chosen for k as well as dB",
  },
  {
    id: "RFE", name: "Recirculation + absorber + forced external",
    rGap: stirredGap(30), rFoam: 0.02, rExt: forcedExternal(30), rVia: R_VIA_EFF,
    breachesShield: false, note: "Full stack, shield untouched",
  },
];

// ---------------------------------------------------------------------------
// §4  Solver — parallel conductances from the junction node
// ---------------------------------------------------------------------------

interface Split { rOut: number; inward: number; outward: number; exported: number }

function solve(o: Option): Split {
  const rOut = o.rGap + o.rFoam + R_SHELL + o.rExt;
  const gIn = 1 / R_IN;
  const gOut = 1 / rOut;
  const gVia = o.rVia === Infinity ? 0 : 1 / o.rVia;
  const gTot = gIn + gOut + gVia;
  return { rOut, inward: gIn / gTot, outward: gOut / gTot, exported: gVia / gTot };
}

// ---------------------------------------------------------------------------
// §5  Aggregate cavity ceiling — the capability question
// ---------------------------------------------------------------------------
// The cavity rejects its residual through rOut over the vault area. Holding the
// allowed cavity rise fixed, the tolerable tile count scales as 1/rOut.

const P_TILE_HEAT = 3.0;      // W/tile dissipated, order from NP-PWR-BUDGET-001 §3.2
const CAVITY_RESIDUAL = 0.10; // R1 §5.1: fraction not exported by the via
const CAVITY_DT_BUDGET = 6.7; // K, calibrated below to reproduce the published ~6-tile rule

function tileCeiling(rOut: number, dtBudget = CAVITY_DT_BUDGET): number {
  const rAbs = rOut / VAULT_AREA;                    // K/W
  return dtBudget / (P_TILE_HEAT * CAVITY_RESIDUAL * rAbs);
}

// ---------------------------------------------------------------------------
// §6  Ambient crossover — max ambient holding face <= 42 C, healthy state
// ---------------------------------------------------------------------------
// Two published points per R1 §5.1 (T1-std): face 30.7 C at 25 C ambient,
// face 46.7 C at 43.3 C ambient. Linear in ambient between them.

const FACE_LIMIT = 42.0;
const A1 = 25.0, F1 = 30.7, A2 = 43.3;
const SLOPE = (46.7 - F1) / (A2 - A1);

/** faceAt433: published face temperature at 43.3 C ambient for this config. */
function maxAmbient(faceAt433: number): number {
  return A2 - (faceAt433 - FACE_LIMIT) / SLOPE;
}

const configs = [
  { name: "T1-std  (125 mW/cm^2)", face433: 46.7 },
  { name: "T1-peak (190 mW/cm^2)", face433: 48.9 },
  { name: "T2-peak (300 mW/cm^2)", face433: 52.6 },
];

// ---------------------------------------------------------------------------
// §7  Pneumatic loop sizing — remote blower, tubes through the existing boss
// ---------------------------------------------------------------------------

const RHO_AIR = 1.15, CP_AIR = 1005, NU_AIR = 1.6e-5;

interface Loop { boreMm: number; velocity: number; reynolds: number; dpPa: number }

function sizeLoop(watts: number, deltaT: number, boreMm: number, lengthM: number): Loop {
  const mdot = watts / (CP_AIR * deltaT);        // kg/s
  const q = mdot / RHO_AIR;                       // m^3/s
  const d = boreMm / 1000;
  const a = (Math.PI * d ** 2) / 4;
  const v = q / a;
  const re = (v * d) / NU_AIR;
  // Blasius for turbulent, 64/Re for laminar
  const f = re < 2300 ? 64 / re : 0.316 * re ** -0.25;
  const dp = f * (lengthM / d) * ((RHO_AIR * v ** 2) / 2);
  return { boreMm, velocity: v, reynolds: re, dpPa: dp };
}

/** TE11 cutoff for a circular bore acting as waveguide-below-cutoff. */
const cutoffGHz = (boreMm: number) =>
  (1.8412 * 3e8) / (2 * Math.PI * (boreMm / 2000)) / 1e9;

/** Below-cutoff attenuation, ~32 dB per length/diameter for a round bore. */
const bcoAttenDb = (lengthMm: number, boreMm: number) => 32 * (lengthMm / boreMm);

// ---------------------------------------------------------------------------
// §8  PCM sizing — the only sub-ambient option without a heat pump
// ---------------------------------------------------------------------------

const PCM_LATENT = 200_000; // J/kg, paraffin order
function pcmMassKg(watts: number, minutes: number): number {
  return (watts * minutes * 60) / PCM_LATENT;
}

// ---------------------------------------------------------------------------
// Report
// ---------------------------------------------------------------------------

const f2 = (n: number) => n.toFixed(2);
const pc = (n: number) => (n * 100).toFixed(1) + "%";

console.log("NP-THERM-COOL-001 — thermal network model\n");

console.log("§2  Outward path decomposition (m^2K/W)");
console.log(`  stagnant inter-bowl gap   ${f2(R_GAP_STAGNANT)}   <- published, dominant`);
console.log(`  EMI absorber foam         ${f2(R_FOAM)}   <- shield part, never thermally specified`);
console.log(`  shell (CFRP+Pd+mu)        ${f2(R_SHELL)}`);
console.log(`  external natural conv.    ${f2(R_EXT_NATURAL)}`);
console.log(`  total                     ${f2(R_GAP_STAGNANT + R_OUT_REMAINDER)}  (published ${f2(R_OUT_BASE)})`);
console.log(`  inward path R_in          ${f2(R_IN)}   <- FLOOR: 1.6 mm to a perfused sink\n`);

console.log(`§3  Via calibration: R_via_eff = ${R_VIA_EFF.toFixed(4)} m^2K/W`);
console.log(`     (reproduces the published ${pc(VIA_EXPORT_FRACTION)} export; an ideal 4 mm`);
console.log(`      copper via is ~0.001, so ~90% of via resistance is contact/spreading/sink)\n`);

console.log("§4  Heat split at the junction");
console.log("  ID    R_out   inward   outward  exported  shield  option");
for (const o of options) {
  const s = solve(o);
  const flag = o.breachesShield ? "BREACH" : "  ok  ";
  console.log(
    `  ${o.id.padEnd(5)} ${f2(s.rOut)}   ${pc(s.inward).padStart(6)}   ` +
    `${pc(s.outward).padStart(6)}   ${pc(s.exported).padStart(7)}  ${flag}  ${o.name}`,
  );
}

const base = solve(options[0]);
const vent = solve(options[2]);
console.log(`\n  Model reproduces the published ~83% inward at ${pc(base.inward)} (network vs FD: within 4 pp).`);
console.log(`  Perfect external ventilation reaches only ${pc(vent.inward)} inward — it does NOT`);
console.log(`  fix the face, and the adopted via already reaches ${pc(solve(options[1]).inward)}.\n`);

console.log("§5  Aggregate cavity ceiling (equal cavity temperature rise)");
console.log("  ID    R_out   tiles   vs baseline   option");
for (const o of options) {
  const s = solve(o);
  const n = tileCeiling(s.rOut);
  const ratio = n / tileCeiling(base.rOut);
  console.log(
    `  ${o.id.padEnd(5)} ${f2(s.rOut)}   ${n.toFixed(1).padStart(5)}   ` +
    `${ratio.toFixed(2)}x`.padStart(11) + `   ${o.name}`,
  );
}
console.log(`\n  Baseline calibrated to the published ~6-tile concurrency rule.`);
console.log(`  Ceiling scales as 1/R_out — this is the protocol-coverage lever.\n`);

console.log("§6  Max ambient holding face <= 42 C, healthy state, via fitted");
console.log(`  (linear fit through R1 §5.1's two published points, slope ${SLOPE.toFixed(3)} K/K)`);
for (const c of configs) {
  console.log(`  ${c.name}   face ${c.face433} C @ 43.3 C  ->  max ambient ${maxAmbient(c.face433).toFixed(1)} C`);
}
console.log(`\n  Compare NP-ENV-OPRANGE-001 as published: T1-A full <= +35, T2-D full <= +30.`);
console.log(`  The provisional dagger-marked bounds already sit close to what the thermal design supports.\n`);

console.log("§7  Pneumatic loop — 20 W removed, 12 K rise, 1.5 m each way");
console.log("  bore   velocity    Re      dP/tube   TE11 cutoff   BCO atten @ L/D=3");
for (const bore of [10, 12, 16, 20]) {
  const l = sizeLoop(20, 12, bore, 1.5);
  console.log(
    `  ${String(bore).padStart(2)} mm  ${l.velocity.toFixed(1).padStart(5)} m/s  ` +
    `${l.reynolds.toFixed(0).padStart(6)}  ${l.dpPa.toFixed(0).padStart(5)} Pa   ` +
    `${cutoffGHz(bore).toFixed(1).padStart(5)} GHz     ${bcoAttenDb(bore * 3, bore).toFixed(0)} dB`,
  );
}
console.log(`\n  Every bore is below cutoff far above the 6 GHz Wi-Fi 6 concern in NP-HEX-ZM-001 §5.3a.`);
console.log(`  Velocity, not pressure drop, is the binding constraint — the audio modality is adjacent.\n`);

console.log("§8  PCM mass for sub-ambient operation");
for (const [w, m] of [[10, 20], [20, 20], [20, 30], [40, 20]] as const) {
  console.log(`  ${String(w).padStart(2)} W for ${m} min  ->  ${(pcmMassKg(w, m) * 1000).toFixed(0)} g latent mass`);
}
console.log(`\n  Sized on latent heat only; a real pack is 2-3x this with matrix, shell and`);
console.log(`  sensible heat. Bounded and small — but it is a capacity, not a rate.`);
