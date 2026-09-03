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
 *
 * This prints a model; it asserts nothing and gates nothing, so no workflow runs
 * it — the same shape as its sibling check-thermal-dose.ts. Its outputs are
 * checked by being quoted into NP-THERM-COOL-001, where each figure is traceable
 * to the section that produced it.
 *
 * CI-Kind: report
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
/**
 * Compliant thermally-conductive gap pad bridging the 6 mm inter-bowl gap.
 * k = 3 W/m.K is mid-range for a ceramic-filled silicone pad; such pads are
 * electrically insulating and non-magnetic, which the fluxgate/Helmholtz split
 * across this gap requires. R = L/k, plus a contact allowance both faces.
 */
const GAP_PAD = 0.006 / 3.0 + 0.0002;
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
  // D-2 test (principal, 2026-08-30): the pneumatic loop is in scope only if its
  // benefit is real AND not obtainable otherwise. These rows are the "otherwise".
  // A compliant thermally-conductive, electrically-insulating gap pad shorts the
  // same stagnant-air term the loop stirs — no blower, no tubes, no aperture.
  {
    id: "G", name: "Conductive gap bridge (pad), no loop",
    rGap: GAP_PAD, rFoam: R_FOAM, rExt: R_EXT_NATURAL, rVia: R_VIA_EFF,
    breachesShield: false, note: "Static, no moving parts",
  },
  {
    id: "GF", name: "Gap bridge + thermally-specified absorber",
    rGap: GAP_PAD, rFoam: 0.02, rExt: R_EXT_NATURAL, rVia: R_VIA_EFF,
    breachesShield: false, note: "Two materials changes, no subsystem",
  },
  {
    id: "GFE", name: "Gap bridge + absorber + forced external",
    rGap: GAP_PAD, rFoam: 0.02, rExt: forcedExternal(30), rVia: R_VIA_EFF,
    breachesShield: false, note: "Full static stack — the D-2 comparator",
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

// ---------------------------------------------------------------------------
// §9  Remote-sink accessories — where the heat is, and what removing it costs
// ---------------------------------------------------------------------------
// Rev 3. Two optional, power-source-keyed accessories. The chiller belongs on the
// via's hub heatsink, NOT the recirculated cavity air: with the via fitted the
// cavity path carries ~2 % of tile heat and the via ~90 % (§4).

/** Duty-averaged per-tile draw, NP-PWR-BUDGET-001 §3.2 (R-4 pulsed, 25 % duty). */
const P_TILE_DUTY = 6.25;
const L_ICE = 334_000;   // J/kg latent heat of fusion — ice
const L_PARAFFIN = 200_000;
const TEC_COP = 0.6;     // typical at dT ~25 K

const viaLoad = (n: number) => n * P_TILE_DUTY * VIA_EXPORT_FRACTION;
const cavityLoad = (n: number) => n * P_TILE_DUTY * (1 - VIA_EXPORT_FRACTION);
const iceGrams = (watts: number, minutes: number) => (watts * minutes * 60) / L_ICE * 1000;

// ---------------------------------------------------------------------------
// §10  The governor bind — min(electrical, thermal, dose), NP-PWRSRC-001 §11
// ---------------------------------------------------------------------------
// Cooling raises ONLY the thermal term. If electrical binds first, cooling buys
// no concurrency and therefore no session-length reduction.

/** Emitter watts available after overhead, per source. NP-PWRSRC-001 §4.1. */
const SOURCES: Array<[string, number]> = [
  ["45 W brick (Home Standard)", 40],
  ["65 W brick", 60],
  ["100 W EPR", 95],
  ["mains base station", 235],
];
/** Sealed-cavity aggregate thermal ceiling, NP-PWRSRC-001 §4.1. */
const THERMAL_UNCOOLED: [number, number] = [27.6, 49.1];
const COOLING_GAIN = 3.28; // §5 row RFE

const tilesFrom = (watts: number) => watts / P_TILE_DUTY;

// ---------------------------------------------------------------------------
// Report — Rev 3 additions
// ---------------------------------------------------------------------------

console.log("§9  Remote-sink accessories — the load, and what removes it");
console.log("  tiles   total W   via path   cavity     ice @30min(via)   TEC W_elec(via)");
for (const n of [6, 10, 20, 30]) {
  const v = viaLoad(n);
  console.log(
    `  ${String(n).padStart(5)}   ${(n * P_TILE_DUTY).toFixed(1).padStart(7)}   ` +
    `${v.toFixed(1).padStart(8)}   ${cavityLoad(n).toFixed(1).padStart(6)}   ` +
    `${(iceGrams(v, 30)).toFixed(0).padStart(13)} g   ${(v / TEC_COP).toFixed(0).padStart(12)} W`,
  );
}
console.log(`\n  Ice (334 kJ/kg) beats paraffin (${L_PARAFFIN / 1000} kJ/kg) by ${(L_ICE / L_PARAFFIN).toFixed(2)}x — use ice.`);
console.log(`  A TEC removing the 6-tile via load draws ${(viaLoad(6) / TEC_COP).toFixed(0)} W and rejects ${(viaLoad(6) * (1 + 1 / TEC_COP)).toFixed(0)} W:`);
console.log(`  mains only. The ice pack draws ~1-2 W (pump), so it alone preserves Mode 3.\n`);

console.log("§10 The governor bind — cooling raises ONLY the thermal term");
console.log("  Concurrency = min(electrical, thermal). Cooling moves thermal; the source moves electrical.");
console.log("\n  source                        elec tiles   thermal tiles      min      cooled min");
for (const [name, w] of SOURCES) {
  const e = tilesFrom(w);
  const tLo = tilesFrom(THERMAL_UNCOOLED[0]), tHi = tilesFrom(THERMAL_UNCOOLED[1]);
  const cLo = tLo * COOLING_GAIN, cHi = tHi * COOLING_GAIN;
  console.log(
    `  ${name.padEnd(28)} ${e.toFixed(1).padStart(10)}   ${(tLo.toFixed(1) + "-" + tHi.toFixed(1)).padStart(13)}   ` +
    `${Math.min(e, tHi).toFixed(1).padStart(6)}   ${Math.min(e, cHi).toFixed(1).padStart(10)}`,
  );
}
console.log("\n  THE FINDING: on the 45 W brick — Home Standard, the config the ice pack targets —");
console.log("  the cooled min is UNCHANGED at 6.4 tiles, because electrical binds first.");
console.log("  Higher-wattage sources do gain (65 W: 7.9 -> 9.6; 100 W EPR: 7.9 -> 15.2), so the");
console.log("  gain is bought by WATTS first and cooling second; cooling alone buys nothing at 45 W.");
console.log("  So: ice pack on a 45 W brick -> ambient envelope only, no session-length gain.");
console.log("      TEC chiller in a mains base station -> both, because the station brings watts too.\n");

console.log("§11 Cascade length and thermal dose — what concurrency is worth");
const bindBase = Math.min(tilesFrom(40), tilesFrom(THERMAL_UNCOOLED[1]));
const bindStation = Math.min(tilesFrom(235), tilesFrom(THERMAL_UNCOOLED[1] * COOLING_GAIN));
const ratio = bindStation / bindBase;
console.log(`  Cascade time scales as 1/concurrency (groups = ceil(sockets / maxConcurrent)).`);
console.log(`  ${bindBase.toFixed(1)} tiles -> ${bindStation.toFixed(1)} tiles is a ${ratio.toFixed(1)}x reduction.`);
console.log(`  Applied to NP-PWRSRC-001 §5.5's worst case (Vascular Baseline, 40 groups, 20.0 h, 292 CEM43):`);
console.log(`    groups ~${Math.ceil(40 / ratio)}, ~${(20 / ratio).toFixed(1)} h, CEM43 ~${(292 / ratio).toFixed(0)} at an unchanged plateau.`);
console.log(`  CEM43 uses R = 0.25 below 43 C, so each 1 C the chiller removes cuts it a further 4x.`);
console.log(`  Directional: NP-PWRSRC-001 owns the dose model (scripts/check-thermal-dose.ts).\n`);

console.log("§12 The anti-fog clamp — what actually limits the cold end");
const dewpt = (T: number, rh: number) => {
  const a = Math.log(rh) + (17.27 * T) / (237.7 + T);
  return (237.7 * a) / (17.27 - a);
};
for (const [label, T, rh] of [["room 25 C / 60 %", 25, 0.60], ["room 30 C / 70 %", 30, 0.70],
                              ["scalp gap ~33 C / 90 %", 33, 0.90], ["scalp gap ~35 C / 95 %", 35, 0.95]] as const) {
  console.log(`  ${label.padEnd(24)} dew point ${dewpt(T, rh).toFixed(1).padStart(5)} C`);
}
console.log("  The scalp gap governs, not the room: a head makes it warm and near-saturated.");
console.log("  Face must stay ABOVE ~31 C or condensation forms on the scalp-facing PDMS window");
console.log("  and the PD2 aperture — the surfaces the J/cm^2 dose claim depends on.");
console.log("  Usable band is therefore ~32 C (anti-fog) to 42 C (IEC 60601-1). Narrow, and it needs");
console.log("  a tempering valve. NP-ENV-001 §5 provides no live RH sensor to compute the clamp from.\n");

console.log("§13 Sealed-loop condensation is bounded and one-time");
const loopVolM3 = 0.006 * VAULT_AREA + 0.0003;
const satGm3 = (T: number) => (216.7 * (6.112 * Math.exp((17.67 * T) / (T + 243.5)))) / (T + 273.15);
for (const [T, rh] of [[25, 0.60], [30, 0.70]] as const) {
  console.log(`  ${(loopVolM3 * 1000).toFixed(2)} L sealed at ${T} C / ${(rh * 100).toFixed(0)} % RH -> ${(satGm3(T) * rh * loopVolM3 * 1000).toFixed(0)} mg water, once`);
}
console.log("  A desiccant pad handles this permanently. The EXTERNAL chilled lines are the opposite");
console.log("  case — unlimited room air, continuous sweating — so they need insulation.\n");

console.log("§14 Accessory BOM (ESTIMATE — no chiller costing exists anywhere in the document set)");
const boms: Array<[string, Array<[string, number, number]>]> = [
  ["Hip ice/PCM loop (USB-C, Mode 3 preserved)", [
    ["2x PCM/ice pack, 500 g, HDPE shell", 8, 16], ["insulated hip pouch + belt clip", 6, 12],
    ["insulated silicone tubing, ~2 m", 5, 11], ["dry-break quick-disconnect pair", 7, 15],
    ["micropump, 5 V brushless, ~1-2 W", 9, 18], ["coolant, fill port, expansion volume", 3, 6],
    ["cold-plate on existing hub heatsink", 5, 10], ["thermostatic tempering valve", 6, 14],
    ["coolant thermistors + control", 3, 5]]],
  ["TEC base-station chiller (mains)", [
    ["TEC module 40x40, 30-60 W Qmax", 8, 20], ["hot-side heatsink + fan", 6, 12],
    ["cold-side exchanger", 5, 10], ["TEC driver, H-bridge + current ctl", 4, 8],
    ["thermistors + control", 2, 3], ["desiccant pad + condensate wick", 3, 8],
    ["duct / manifold / seals", 5, 10]]],
];
for (const [title, items] of boms) {
  let lo = 0, hi = 0;
  console.log(`  ${title}`);
  for (const [k, a, b] of items) { console.log(`    ${k.padEnd(40)} $${String(a).padStart(3)}-${b}`); lo += a; hi += b; }
  console.log(`    ${"TOTAL".padEnd(40)} $${String(lo).padStart(3)}-${hi}`);
}
console.log("  Optional RH sensor (SHT4x / HDC3020 / BME280 class)  $  2-5");
console.log("  Vapour compression, for contrast                     $105-240 — wrong technology at 10-30 W");
console.log("\n  Both are ACCESSORIES, not base BOM: Home Standard is already $897-959 at -41% to -51% GM.\n");

// ---------------------------------------------------------------------------
// §15  D-2 test — is the pneumatic loop's benefit obtainable by other means?
// ---------------------------------------------------------------------------

console.log("§15 D-2 test — pneumatic loop vs a static conductive gap bridge");
console.log("  D-2 (principal): the loop is in scope only if its benefit is real AND not");
console.log("  obtainable otherwise. Both attack the SAME term — the 0.23 stagnant inter-bowl gap.\n");
console.log("  ID    R_out   tiles   vs base   moving parts   aperture   option");
for (const id of ["V", "R", "RFE", "G", "GF", "GFE"]) {
  const o = options.find((x) => x.id === id)!;
  const r = solve(o);
  const n = tileCeiling(r.rOut);
  const loop = id === "R" || id === "RFE";
  console.log(
    `  ${id.padEnd(5)} ${r.rOut.toFixed(3).padStart(5)}   ${n.toFixed(1).padStart(5)}   ` +
    `${(n / tileCeiling(solve(options[0]).rOut)).toFixed(2)}x`.padStart(7) +
    `   ${(loop ? "blower+tubes" : "none").padEnd(13)}  ${"none".padEnd(9)}  ${o.name}`,
  );
}
const rfe = tileCeiling(solve(options.find((o) => o.id === "RFE")!).rOut);
const gfe = tileCeiling(solve(options.find((o) => o.id === "GFE")!).rOut);
console.log(`\n  A gap pad is R = ${GAP_PAD.toFixed(4)} m^2K/W against ${(2 / 30).toFixed(3)} for a stirred gap`);
console.log(`  and 0.230 stagnant — conduction through a solid beats convection across a gap.`);
console.log(`  GFE reaches ${gfe.toFixed(1)} tiles vs RFE's ${rfe.toFixed(1)} — ${(gfe / rfe).toFixed(1)}x BETTER with no loop at all.`);
console.log(`\n  CONCLUSION for D-2: the loop's benefit is NOT unique to it. A static pad attacks`);
console.log(`  the same term harder, with no blower, no tubes, no acoustic path beside the audio`);
console.log(`  modality, and no penetration — so OI-THCOOL-06's ELF measurement is not needed.`);
console.log(`  Subject to OI-THCOOL-15: real two-face contact across a curved 5-7 mm gap with`);
console.log(`  tolerance stack, and compression set over repeated bowl separations.`);

// ---------------------------------------------------------------------------
// §16  Gap-pad coverage sensitivity (§6.9.1)
// ---------------------------------------------------------------------------
// §6.9's GAP_PAD assumes a FULL-AREA pad. The cluster clamps, fluxgates and the
// bowl-separation requirement force a DISCRETE pattern instead. Pads at area
// fraction phi sit in PARALLEL with stagnant air over the remaining (1 - phi).

/** Effective gap resistance for pads covering area fraction phi. */
const gapAtCoverage = (phi: number) =>
  1 / (phi / GAP_PAD + (1 - phi) / R_GAP_STAGNANT);

/** Circular pad of diameter d on a 40 mm hex tile -> coverage fraction. */
const coverageFromPadDia = (dMm: number) =>
  (Math.PI * (dMm / 2) ** 2) / (TILE_AREA * 1e6);

console.log("§16 Gap-pad coverage sensitivity — §6.9's figures assume FULL AREA");
console.log("  Pads at fraction phi are in parallel with stagnant air over (1 - phi).\n");
console.log("  pad dia   phi     R_gap    R_out*   tiles   vs base   note");
for (const d of [10, 12, 16, 20, 25, 30]) {
  const phi = coverageFromPadDia(d);
  const rGap = gapAtCoverage(phi);
  // full static stack (GFE) with this gap term
  const rOut = rGap + 0.02 + R_SHELL + forcedExternal(30);
  const n = tileCeiling(rOut);
  const note = d <= 12 ? "fits beside clamps easily" : d >= 30 ? "likely clashes" : "";
  console.log(
    `  ${String(d).padStart(4)} mm  ${(phi * 100).toFixed(1).padStart(4)}%  ` +
    `${rGap.toFixed(4)}   ${rOut.toFixed(3)}   ${n.toFixed(1).padStart(5)}   ` +
    `${(n / tileCeiling(solve(options[0]).rOut)).toFixed(2)}x`.padStart(7) + `   ${note}`,
  );
}
const full = tileCeiling(solve(options.find((o) => o.id === "GFE")!).rOut);
const phi20 = coverageFromPadDia(20);
const real = tileCeiling(gapAtCoverage(phi20) + 0.02 + R_SHELL + forcedExternal(30));
console.log(`\n  Full-area (§6.9 as written): ${full.toFixed(1)} tiles.`);
console.log(`  A realistic 20 mm pad per tile (${(phi20 * 100).toFixed(0)}% coverage): ${real.toFixed(1)} tiles.`);
console.log(`  So §6.9's headline is optimistic by ${(full / real).toFixed(1)}x — but even the`);
console.log(`  low-coverage rows beat the stirred gap (${(2 / 30).toFixed(3)}), so D-2's conclusion holds.`);
console.log(`  Coverage, not pad conductivity, is the design variable. OI-THCOOL-15.`);


// ---------------------------------------------------------------------------
// §17  Derate semantics and the efficacy-floor clamp (§7.4) — DECIDED, D-4
// ---------------------------------------------------------------------------
// NP-ENV-OPRANGE-001 §1 specifies "linear duty derate T_f -> T_max". Duty
// scales; nothing said whether the session extends to compensate, and the two
// readings are not interchangeable. D-4 (2026-09-02) settles it:
//
//   SESSION LENGTH IS FIXED. Duty scales, so delivered dose scales with it —
//   and the ramp is CLAMPED at NP-PWR-BUDGET-001 §3.4's efficacy floor, below
//   which the session is REFUSED rather than derated further.
//
// §17.1-§17.4 below produce the figures that decided it. The load-bearing one
// is §17.2: the dose-preserving reading buys efficacy with time-at-ceiling,
// and time-at-ceiling is the whole of NP-PWRSRC-001 §5.5's finding.

/** Efficacy floor, NP-PWR-BUDGET-001 §3.4 (10-120 J/cm^2 band, lower edge). */
const EFFICACY_FLOOR_J = 10;
/** The shared band, NP-ENV-OPRANGE-001 §2 footnote ‖. */
const T_FULL = 30, T_BLOCK = 35;

/** CEM43 per minute at a face temperature. Model owned by
 *  check-thermal-dose.ts (NP-PWRSRC-001 §5); duplicated rather than imported
 *  because that script prints its full audit at module scope. */
const cem43Rate = (tC: number) => (tC >= 43 ? 0.5 ** (43 - tC) : 0.25 ** (43 - tC));
/** CLAUDE.md §4.2 / IEC 60601-1 applied-part limit, and the temperature the
 *  derate band exists to hold the face AT — so it is the right rate to use
 *  inside the band, not a pessimistic overlay. */
const FACE_LIMIT_C = 42.0;
/** NP-PWRSRC-001 §5.5's reference lines. */
const CEM43_REVIEW_LINE = 2.0, CEM43_CONCERN_LINE = 40.0;

/** Linear duty derate as specified: 1.0 at T_full, 0.0 at T_block. */
function dutyLinear(ambC: number): number {
  if (ambC <= T_FULL) return 1;
  if (ambC >= T_BLOCK) return 0;
  return (T_BLOCK - ambC) / (T_BLOCK - T_FULL);
}

/** Duty at which a protocol's derated dose reaches the efficacy floor. */
const dutyFloor = (fullDoseJ: number) => EFFICACY_FLOOR_J / fullDoseJ;
/** Ambient at which that duty is reached — the protocol's effective block. */
const clampAmbient = (fullDoseJ: number) =>
  T_BLOCK - dutyFloor(fullDoseJ) * (T_BLOCK - T_FULL);

/** Representative protocols, all inside NP-PWR-BUDGET-001 §3.4's published
 *  band (10-120 J/cm^2, 6-30 min). Length is declared by the protocol, not
 *  derived from dose — which is exactly the property D-4 preserves. */
const PROTOCOLS: { doseJ: number; min: number }[] = [
  { doseJ: 40, min: 13.3 }, { doseJ: 60, min: 20 },
  { doseJ: 90, min: 25 }, { doseJ: 120, min: 30 },
];
/** The 60 J/cm^2 / 20 min row is the worked example used throughout §7.4. */
const REF = PROTOCOLS[1];

console.log("\n§17 Derate semantics — DECIDED (D-4): fixed length + efficacy-floor clamp\n");

console.log("  17.1  The reading as written — fixed length, so dose scales with duty");
console.log(`        (${REF.doseJ} J/cm^2 protocol, ${REF.min} min)\n`);
console.log("  ambient   duty     dose   verdict");
for (const amb of [30, 31, 32, 33, 34, 34.5]) {
  const d = dutyLinear(amb), j = REF.doseJ * d;
  const verdict = j < EFFICACY_FLOOR_J ? "*** SUB-THRESHOLD — a null session ***"
    : d === 1 ? "full dose" : "under-dosed, but inside the 10-120 J/cm^2 band";
  console.log(`  ${amb.toFixed(1).padStart(6)}C  ${(d * 100).toFixed(0).padStart(4)}%  ${j.toFixed(1).padStart(6)}   ${verdict}`);
}
console.log(`\n  Most of the band lands INSIDE the therapeutic window, so under-dosing there`);
console.log(`  is a weaker session, not a null one. Only the top sliver is pathological:`);
console.log(`  the ${REF.doseJ} J/cm^2 protocol crosses the floor at ${clampAmbient(REF.doseJ).toFixed(1)} C, leaving ${(T_BLOCK - clampAmbient(REF.doseJ)).toFixed(1)} C of`);
console.log(`  band in which the device runs to completion and cannot work.\n`);

console.log("  17.2  The dose-preserving reading — extend the session — and its price");
console.log(`        CEM43 upper bound at the ${FACE_LIMIT_C} C interlock (${cem43Rate(FACE_LIMIT_C).toFixed(2)} CEM43/min).`);
console.log(`        The derate band is DEFINED as the region where the face is held at`);
console.log(`        that limit, so this is the design condition, not a worst case.\n`);
console.log("  ambient   duty   session   CEM43   vs NP-PWRSRC-001 §5.5 lines");
for (const amb of [30, 31, 32, 33, 34, 34.5]) {
  const d = dutyLinear(amb), mins = REF.min / d, cem = mins * cem43Rate(FACE_LIMIT_C);
  const flag = cem >= CEM43_CONCERN_LINE ? "*** past the 40 concern line, ONE session ***"
    : cem >= CEM43_REVIEW_LINE ? "above the 2.0 review line" : "";
  console.log(`  ${amb.toFixed(1).padStart(6)}C  ${(d * 100).toFixed(0).padStart(4)}%  ${mins.toFixed(0).padStart(6)}m  ${cem.toFixed(1).padStart(6)}   ${flag}`);
}
const cemFixed = REF.min * cem43Rate(FACE_LIMIT_C);
const cemExt34 = (REF.min / dutyLinear(34)) * cem43Rate(FACE_LIMIT_C);
console.log(`\n  Fixed length holds this at ${cemFixed.toFixed(1)} CEM43 at every ambient in the band.`);
console.log(`  Extension multiplies it by 1/duty — ${(cemExt34 / cemFixed).toFixed(0)}x at 34 C — and at 34.5 C a routine`);
console.log(`  ${REF.min}-minute protocol passes the 40 CEM43 line in a SINGLE session. That is the`);
console.log(`  §5.5 mechanism exactly: the interlock caps temperature and says nothing about`);
console.log(`  how long the face may stay there. THIS is why fixed length wins.\n`);

console.log("  17.3  The decided clamp — derate to the duty floor, then REFUSE");
console.log("        The duty curve is unchanged and still shared by every module;");
console.log("        only where a protocol stops walking down it is per-protocol.\n");
console.log("  dose   length   duty floor   blocks at   (was)   CEM43 at own length");
for (const p of PROTOCOLS) {
  const cem = p.min * cem43Rate(FACE_LIMIT_C);
  console.log(
    `  ${String(p.doseJ).padStart(3)} J  ${p.min.toFixed(1).padStart(6)}m  ` +
    `${(dutyFloor(p.doseJ) * 100).toFixed(0).padStart(9)}%  ` +
    `${clampAmbient(p.doseJ).toFixed(1).padStart(9)}C  ${T_BLOCK.toFixed(1)}C  ${cem.toFixed(1).padStart(15)}`,
  );
}
const worstClamp = Math.max(...PROTOCOLS.map((p) => p.min * cem43Rate(FACE_LIMIT_C)));
const lost = T_BLOCK - Math.min(...PROTOCOLS.map((p) => clampAmbient(p.doseJ)));
console.log(`\n  Cost: at most ${lost.toFixed(1)} C off the top of an envelope §7.2 already records as`);
console.log(`  ~8 C more conservative than the physics requires. Worst CEM43 under the`);
console.log(`  clamp is ${worstClamp.toFixed(1)} against the extension reading's ${cemExt34.toFixed(1)} at 34 C.\n`);

console.log("  17.4  The inversion the clamp creates, stated because it will be misread");
const lo = PROTOCOLS[0], hi = PROTOCOLS[PROTOCOLS.length - 1];
console.log(`        A ${hi.doseJ} J/cm^2 protocol survives to ${clampAmbient(hi.doseJ).toFixed(1)} C; a ${lo.doseJ} J/cm^2 one blocks at ${clampAmbient(lo.doseJ).toFixed(1)} C.`);
console.log("        The HEAVIER protocol runs in the HOTTER room. That is correct — the");
console.log("        floor is on delivered dose and the derate is multiplicative — but it");
console.log("        inverts the intuition, and it means the app's advice at a refusal is");
console.log(`        \"a higher-dose protocol may still run\", never \"try a shorter one\".`);
console.log(`        It is not free: the heavier protocol earns its extra ${(clampAmbient(hi.doseJ) - clampAmbient(lo.doseJ)).toFixed(1)} C by spending`);
console.log(`        ${(hi.min / lo.min).toFixed(1)}x the time at the ceiling (${(hi.min * cem43Rate(FACE_LIMIT_C)).toFixed(1)} vs ${(lo.min * cem43Rate(FACE_LIMIT_C)).toFixed(1)} CEM43). The clamp therefore`);
console.log("        inherits §17.2's objection in BOUNDED form: the time it spends is the");
console.log("        protocol's own declared length, which check-thermal-dose.ts already");
console.log("        audits, not a new ambient-dependent length nothing has assessed.");
console.log("        OI-THCOOL-18 (HFE), OI-THCOOL-19 (dose input), OI-THCOOL-20 (T2).");


// ---------------------------------------------------------------------------
// §18  Hysteresis on the ambient hard edges (§7.5, OI-THCOOL-16)
// ---------------------------------------------------------------------------
// The block edge is a discrete transition: below it a session is admitted,
// above it denied. Any discrete transition driven by a noisy, drifting input
// chatters unless the re-entry threshold is separated from the exit threshold.
// This section sizes that separation from three independent bounds, and shows
// what it costs in availability.

/** Junction-interlock precedent, firmware/safety_mcu/include/np_safety_config.h. */
const JUNCTION_CUTOFF_C = 62, JUNCTION_REARM_C = 55;

/**
 * ADC counts per degree from the shipped NTC table
 * (firmware/safety_mcu/src/np_thermal_interlock.c, k_ntc_adc, 10k B=3950,
 * 10k divider, 12-bit). Quoted, not re-derived: three entries spanning the band.
 */
const NTC_ADC_AT = { 30: 2140, 34: 1846, 35: 1776, 36: 1708 } as const;

/** Room thermostats carry their own differential; ambient oscillates by it. */
const THERMOSTAT_DIFFERENTIAL_C = [0.5, 1.0] as const;

/** Room recovery rates after a hot spell (still-air building envelope). */
const DRIFT_RATES_C_PER_H = [1, 2, 3] as const;

console.log("\n§18 Hysteresis on the ambient hard edges (OI-THCOOL-16)");

const countsPerDeg = NTC_ADC_AT[34] - NTC_ADC_AT[35];
console.log("\n  (a) What the sense chain can represent");
console.log(`    ADC slope at the block edge: ${countsPerDeg} counts/K`
  + `  -> 1 LSB = ${(1 / countsPerDeg).toFixed(3)} K`);
console.log(`    So the ANALOG chain resolves ~${(3 / countsPerDeg).toFixed(2)} K at 3 LSB of noise`
  + " — it is not the limit.");
console.log("    The limit is adc_to_celsius(), which returns whole degrees (uint8_t).");
console.log("    MINIMUM REPRESENTABLE HYSTERESIS TODAY = 1.0 K. Anything finer needs the");
console.log("    ambient path specified at 0.1 K, which POE's i16 dC encoding already assumes.");

console.log("\n  (b) What the environment demands");
for (const d of THERMOSTAT_DIFFERENTIAL_C) {
  console.log(`    thermostat differential ${d.toFixed(1)} K -> ambient itself cycles by ~${d.toFixed(1)} K;`
    + ` a band < ${d.toFixed(1)} K sits inside the room's own oscillation`);
}
console.log("    A hysteresis band suppresses cycling only when it exceeds the input's");
console.log("    peak-to-peak excursion, so 1.0 K is the floor the room sets too.");

console.log("\n  (c) What availability can pay — wait to re-arm after a block");
console.log("    band    " + DRIFT_RATES_C_PER_H.map(r => `${r} K/h`.padStart(9)).join(""));
for (const band of [0.5, 1.0, 2.0, JUNCTION_CUTOFF_C - JUNCTION_REARM_C]) {
  const row = DRIFT_RATES_C_PER_H.map(r => `${(band / r * 60).toFixed(0)} min`.padStart(9)).join("");
  const tag = band === JUNCTION_CUTOFF_C - JUNCTION_REARM_C
    ? "   <- the junction interlock's band, unusable for an admission gate" : "";
  console.log(`    ${band.toFixed(1)} K ${row}${tag}`);
}

console.log("\n  => 1.0 K is simultaneously the representation floor, the environment floor,");
console.log("     and near the availability ceiling. Three independent bounds, one number.");

console.log("\n  (d) Re-arm points, anchored on the EFFECTIVE block — not on a constant");
const HYST_C = 1.0;
console.log(`    flat block (today, §7.2):        block ${T_BLOCK.toFixed(1)} C`
  + `  -> re-arm ${(T_BLOCK - HYST_C).toFixed(1)} C`);
console.log("    per-protocol block (OI-THCOOL-17 closed; the efficacy clamp IS adopted):");
for (const full of [40, 60, 120]) {
  const a = clampAmbient(full);
  console.log(`      ${String(full).padStart(3)} J/cm^2 protocol:      block ${a.toFixed(1)} C`
    + `  -> re-arm ${(a - HYST_C).toFixed(1)} C`);
}
console.log("    The rule was written against the anchor, so it held when 17 landed — which is");
console.log("    why 16 did not have to wait for 17, and needed no revision when it closed.");
