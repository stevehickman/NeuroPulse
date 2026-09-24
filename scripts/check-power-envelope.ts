#!/usr/bin/env bun
/**
 * check-power-envelope.ts — the whole power envelope, against a specified
 * rejection path and a specified TMS coil (issue #336).
 *
 *   bun scripts/check-power-envelope.ts             # full report
 *   bun scripts/check-power-envelope.ts --validate  # anchors only, exit 1 on drift
 *
 * NP-PWR-THERM-001 is the document. Five threads were open, and every one of
 * them was blocked on a quantity nobody had written down:
 *
 *   OI-PWR-01 / OI-N1-02   the tile ceiling rests on a heatsink no document
 *                          specifies                    -> NP-THERM-SINK-001
 *   OI-POE-09              a Rev 10 claim withdrawn     -> §8 here
 *   T1 power source        decided against a ceiling that has since moved
 *   OI-PWR-04              a second inlet nobody assessed
 *   TMS                    no electrical specification ANYWHERE
 *
 * The first is closed: `SPEC-SINK-01` R_sink = 1.08 K/W, recovered twice —
 * 1.14 K/W since the 3 mm outer-bowl re-loft (OI-THCOOL-21, NP-THERM-SINK-001 §3a).
 * This script takes that as an input and does the four things that follow
 * from it, plus the one thing that was never blocked on it at all.
 *
 * ── What it establishes ──────────────────────────────────────────────────────
 *
 *  §1  THE PBM CEILING IN WATTS, which is the unit both D-4 and N1-D-4 insist
 *      on. NP-PWRSRC-001 §4.1 put the sealed-cavity ceiling at 27.6-49.1 W from
 *      a lumped one-node model. Under SPEC-SINK-01 the number is N-dependent
 *      and ambient-dependent, and at every montage the library actually
 *      authors it is BELOW the bottom of that band.
 *
 *  §2  THE PER-PROTOCOL TABLE, RE-RUN (OI-SINK-05). N1 §6a was computed at
 *      R_sink 0.5 with the external film counted twice, and split the library
 *      four-inadmissible / fourteen-heatsink-recoverable. "Recoverable on the
 *      heatsink alone" is no longer an available move, because there is no
 *      heatsink. The split does not survive.
 *
 *  §3  THE TMS COIL, SPECIFIED ELECTRICALLY. From a coil geometry rather than
 *      from E proportional-to B^2 scaling (OI-PWR-03), and it lands on top of
 *      NP-PWR-BUDGET-001 §4.1's scaling estimate — two derivations sharing no
 *      input.
 *
 *  §4  WHICH END OF 0.1-0.5 T IS A DESIGN POINT (OI-PWR-02). The cortical
 *      field goes as B*omega, not B, and a small coil rings faster than the
 *      1.5-2.0 T clinical reference it keeps being compared to. The top of the
 *      range is plausibly usable. The bottom is not usable at all.
 *
 *  §5  THE COIL'S OWN THERMAL WALL, which is the T2 mirror of §1: a 3000-pulse
 *      train at the design point puts more energy into the coil than a
 *      head-worn thermal mass can hold under the same 42 C applied-part limit.
 *      EVERY thread in #336 terminates in heat.
 *
 *  §6  CLAUDE.md §4.5's rows, re-derived to include what §3 specifies.
 *  §7  The four power-path options, priced against a real number for the first
 *      time - and OI-PWR-11's arithmetic, which is decidable now.
 *  §8  OI-POE-09(b): the efficacy-floor band, denominated in dose.
 *
 * Units: energy J, power W, field T, length m unless a suffix says otherwise.
 *
 * CI-Kind: report
 */
import {
  distributed, heatW, OP, FACE_LIMIT, AMB_NOMINAL, N_SOCKETS, TILE_AREA, R_SINK_SPECIFIED,
} from "./check-thermal-multitile";
import {
  steadySink, SPREADERS, R_SINK_SPEC, R_SINK_BAND, H_EXT_SPEC, A_EXT_EFF,
  R_SINK_SPEC_R1, A_EXT_EFF_R1, OCCLUSION, type SinkOpts,
} from "./check-thermal-sink";
import { R_OUT_CURRENT } from "./thermal-outward-path";
import { analyse } from "./check-pbm-power";

const VALIDATE_ONLY = process.argv.includes("--validate");

const f0 = (x: number) => x.toFixed(0);
const f1 = (x: number) => x.toFixed(1);
const f2 = (x: number) => x.toFixed(2);
const f3 = (x: number) => x.toFixed(3);
const rule = (s: string) => console.log(`\n${s}\n${"-".repeat(s.length)}`);

/** q vector for an active set at a per-tile ELECTRICAL draw. Four lines, and
 *  re-implemented rather than imported because neither parent exports it. */
function drive(set: number[], elecW: number): number[] {
  const q = new Array(N_SOCKETS).fill(0);
  for (const i of set) q[i] = heatW(elecW);
  return q;
}
const maxFace = (f: { f: number[] }) => Math.max(...f.f);

/** The spreader SPEC-SINK-04 requires: >= 0.18 W/K of exterior kt. S3 is the
 *  row NP-THERM-SINK-001 §7 recommends and the only one that clears it. */
const S3 = SPREADERS[3];
const WITH_SPREADER: SinkOpts = { ktSpreader: S3.kt };
const BARE: SinkOpts = {};

// ---------------------------------------------------------------------------
// §1  The PBM ceiling, in watts
// ---------------------------------------------------------------------------

/** Largest admissible per-tile electrical draw at N tiles, distributed.
 *  Monotone in drive at fixed N, so bisection; 30 halvings of [0, 60] is well
 *  past the precision any figure here is quoted to. */
const driveCache = new Map<string, number>();
function maxDrivePerTile(n: number, o: SinkOpts): number {
  const key = `${n}|${o.ktSpreader ?? 0}|${o.amb ?? AMB_NOMINAL}|${o.occludedFrac ?? 0}|${o.perfectSink ? 1 : 0}|${o.asWas ? 1 : 0}`;
  const hit = driveCache.get(key);
  if (hit !== undefined) return hit;
  let lo = 0, hi = 60;
  const set = distributed(n);
  if (maxFace(steadySink(drive(set, lo), o)) > FACE_LIMIT) { driveCache.set(key, 0); return 0; }
  for (let k = 0; k < 30; k++) {
    const mid = (lo + hi) / 2;
    if (maxFace(steadySink(drive(set, mid), o)) <= FACE_LIMIT) lo = mid; else hi = mid;
  }
  driveCache.set(key, lo);
  return lo;
}
/** Total admissible PBM electrical watts at N tiles. */
const admissibleW = (n: number, o: SinkOpts) => n * maxDrivePerTile(n, o);
/** The aggregate ceiling. NP-THERM-SINK-001 §8.1: the admissible TOTAL rises
 *  monotonically with N — spreading the same watts is thermally free and then
 *  some — so the best N is the full lattice. Asserted rather than assumed:
 *  `--validate` checks the sequence is increasing. */
function aggregateW(o: SinkOpts): { w: number; n: number } {
  return { w: admissibleW(N_SOCKETS, o), n: N_SOCKETS };
}
/** Largest N holding face <= 42 C at a fixed per-tile drive. Monotone
 *  decreasing in N at fixed drive, so binary search rather than a walk. */
function ceilingTiles(elecW: number, o: SinkOpts): number {
  const holds = (n: number) => maxFace(steadySink(drive(distributed(n), elecW), o)) <= FACE_LIMIT;
  if (!holds(1)) return 0;
  if (holds(N_SOCKETS)) return N_SOCKETS;
  let lo = 1, hi = N_SOCKETS;
  while (hi - lo > 1) { const mid = (lo + hi) >> 1; if (holds(mid)) lo = mid; else hi = mid; }
  return lo;
}

/** NP-PWRSRC-001 §4.1's one-node inversion, reproduced so the two can be
 *  compared rather than asserted to differ. P_total = dT_margin / R. */
const PWRSRC_41 = { margin: 11.3, rLo: 0.23, rHi: 0.41, rNow: R_OUT_CURRENT };
const pwrsrc41Ceiling = (r: number) => PWRSRC_41.margin / r;

function reportCeiling() {
  rule("§1  The PBM ceiling in watts — SPEC-SINK-01 against NP-PWRSRC-001 §4.1");
  console.log(`  NP-PWRSRC-001 §4.1 inverts NP-PWR-BUDGET-001 §3.2 into watts and gets a`);
  console.log(`  single number for the whole assembly:`);
  console.log(`    R" ${f2(PWRSRC_41.rHi)} m2K/W (conservative)  ->  ${f1(pwrsrc41Ceiling(PWRSRC_41.rHi))} W`);
  console.log(`    R" ${f2(PWRSRC_41.rLo)} m2K/W (optimistic)    ->  ${f1(pwrsrc41Ceiling(PWRSRC_41.rLo))} W`);
  console.log(`    (at the CURRENT ${PWRSRC_41.rNow.toFixed(3)}, Layer 4 deleted, its conservative end would be ${f1(pwrsrc41Ceiling(PWRSRC_41.rNow))} W —`);
  console.log(`     §4.1 is NP-PWRSRC-001's to restate; OI-THCOOL-21. The comparison below is unaffected in sign.)`);
  console.log(`  That model has no N in it. SPEC-SINK-01's does, and N is the whole story:`);
  console.log();
  console.log("    N     bare shell S0        with spreader S3      (25 C ambient)");
  console.log("          W/tile   total       W/tile   total");
  for (const n of [1, 2, 6, 12, 20, 37, 80]) {
    const b = maxDrivePerTile(n, BARE), s = maxDrivePerTile(n, WITH_SPREADER);
    console.log(`   ${String(n).padStart(3)}   ${f2(b).padStart(6)}  ${f1(n * b).padStart(6)} W     ` +
      `${f2(s).padStart(6)}  ${f1(n * s).padStart(6)} W`);
  }
  const aggS = aggregateW(WITH_SPREADER), aggB = aggregateW(BARE);
  const agg35 = aggregateW({ ...WITH_SPREADER, amb: 35 });
  console.log();
  console.log(`  Aggregate ceiling, reached at the full lattice: ${f1(aggS.w)} W at ${f0(AMB_NOMINAL)} C, ${f1(agg35.w)} W at`);
  console.log(`  the +35 C top of the ambient envelope. The two spreader columns CONVERGE`);
  console.log(`  there — at N = ${aggS.n} the exterior is uniformly loaded and there is nothing left`);
  console.log(`  to spread — so the spreader's entire value is at the low N sessions run at.`);
  console.log();
  console.log(`  THE COMPARISON THAT MATTERS. §4.1's band is ${f1(pwrsrc41Ceiling(PWRSRC_41.rHi))}-${f1(pwrsrc41Ceiling(PWRSRC_41.rLo))} W and reads as a`);
  console.log(`  property of the assembly. It is a property of the assembly ONLY when the`);
  console.log(`  whole lattice is lit. NP-THERM-CFD-N1-001 §6 puts authored montages at`);
  console.log(`  N = 5-37, and at N = 6 the admissible total is ${f1(admissibleW(6, WITH_SPREADER))} W with a spreader and`);
  console.log(`  ${f1(admissibleW(6, BARE))} W without — ${f1(pwrsrc41Ceiling(PWRSRC_41.rHi) / admissibleW(6, WITH_SPREADER))}x and ${f1(pwrsrc41Ceiling(PWRSRC_41.rHi) / admissibleW(6, BARE))}x below the BOTTOM of §4.1's band.`);
  console.log();
  console.log(`  §4.1's conclusion is unchanged and strengthened: it argued no source above`);
  console.log(`  ~57 W buys anything. The ceiling did not rise when it was specified. It`);
  console.log(`  fell, and it acquired two arguments (N and ambient) that a charger keyed`);
  console.log(`  to "peak draw of configuration" cannot see.`);
  console.log();
  const occ = admissibleW(6, { ...WITH_SPREADER, occludedFrac: OCCLUSION.fracRef, occlusionR: OCCLUSION.rAdd });
  console.log(`  Occluded (SPEC-SINK-02, phi ${f2(OCCLUSION.fracRef)}), N = 6: ${f1(occ)} W.`);
}

function reportT1Peak() {
  rule("§1b  What this does to CLAUDE.md §4.5's T1-peak row");
  const OVERHEAD_W = 7.0;  // NP-HW-HEXTILE-001 §9.1 non-PBM overhead, 6-8 W
  const agg = aggregateW(WITH_SPREADER).w;
  const at6 = admissibleW(6, WITH_SPREADER);
  console.log(`  §4.5 reads "T1 peak ~45-50 W, min 20 V/3 A (65 W)". Net of the ~${f0(OVERHEAD_W)} W`);
  console.log(`  non-PBM overhead (NP-HW-HEXTILE-001 §9.1) that is ~38-43 W to emitters, and`);
  console.log(`  check-pbm-power.ts's AVAILABLE_W takes it as 40.0.`);
  console.log();
  console.log(`    thermally admissible, fully distributed (N = 80):   ${f1(agg)} W  -> ${f1(agg + OVERHEAD_W)} W device`);
  console.log(`    thermally admissible at an authored montage (N = 6): ${f1(at6)} W  -> ${f1(at6 + OVERHEAD_W)} W device`);
  console.log();
  console.log(`  So the T1-peak row is an ELECTRICAL peak the assembly cannot spend. A 45 W`);
  console.log(`  brick already covers the fully-distributed thermal ceiling with ${f1(45 - agg - OVERHEAD_W)} W over.`);
  console.log(`  The 65 W rung buys nothing thermally at T1 — which is NP-PWRSRC-001 §12.4's`);
  console.log(`  "a 65 W source unlocks zero protocols", now with a resistance behind it.`);
}

// ---------------------------------------------------------------------------
// §2  The per-protocol table, re-run (OI-SINK-05)
// ---------------------------------------------------------------------------

function reportProtocols() {
  rule("§2  Per-protocol thermal ceilings under SPEC-SINK-01 (OI-SINK-05)");
  console.log(`  N1 §6a was computed at R_sink 0.5 K/W with the external film double-counted,`);
  console.log(`  and reported "4 inadmissible at any heatsink / 14 heatsink-recoverable" over`);
  console.log(`  23 authored protocols. Re-run against SPEC-SINK-01, on the network that`);
  console.log(`  restores the film to both paths.`);
  console.log();
  console.log(`  Columns: "power" is NP-SES-PWR-001 §2.1's concurrency at that per-tile`);
  console.log(`  draw; "S0" is the bare outer bowl as adopted; "S3" is with SPEC-SINK-04's`);
  console.log(`  100 um graphite spreader; "perfect" pins the exterior skin at ambient.`);
  console.log();
  console.log("  W/tile  power    S0    S3  perfect  binds             protocol");
  const rows = analyse().filter((r) => r.sockets !== null)
    .sort((x, y) => y.perTileW - x.perTileW);
  let inadmissibleAny = 0, spreaderRecovered = 0, admissibleBare = 0, powerBound = 0;
  for (const r of rows) {
    const s0 = ceilingTiles(r.perTileW, BARE);
    const s3 = ceilingTiles(r.perTileW, WITH_SPREADER);
    const id = ceilingTiles(r.perTileW, { perfectSink: true });
    if (id === 0) inadmissibleAny++;
    else if (s0 === 0 && s3 > 0) spreaderRecovered++;
    if (s0 > 0) admissibleBare++;
    const binds = id === 0 ? "THERMAL any R_sink"
      : s3 === 0 ? "THERMAL spreader-proof"
      : s3 < r.maxConcurrent ? "THERMAL"
      : "power";
    if (binds === "power") powerBound++;
    console.log(`  ${f2(r.perTileW).padStart(6)}  ${String(r.maxConcurrent).padStart(5)}` +
      `  ${String(s0).padStart(4)}  ${String(s3).padStart(4)}  ${String(id >= N_SOCKETS ? ">80" : id).padStart(7)}` +
      `  ${binds.padEnd(23)}${r.name}`);
  }
  console.log();
  console.log(`  ${rows.length} authored protocols carry a fixed zone set (the 23rd is`);
  console.log(`  clinician-selected, so its montage is not a property of the protocol).`);
  console.log();
  console.log(`    inadmissible at ANY rejection resistance   ${inadmissibleAny}`);
  console.log(`    recovered by the SPREADER, not a heatsink  ${spreaderRecovered}`);
  console.log(`    hold >= 1 tile on the bare shell as adopted ${admissibleBare}`);
  console.log(`    power-bound rather than thermally bound    ${powerBound}`);
  console.log();
  console.log(`  TWO DIFFERENCES FROM N1 §6a, both stated rather than reconciled away.`);
  console.log(`  (i) "perfect" here pins the exterior skin at ambient, which is STRICTLY`);
  console.log(`  more generous than N1's ideal — N1 zeroed R_sink but still charged the`);
  console.log(`  cavity leg its full 0.18 with the film in it. So this column is an upper`);
  console.log(`  bound on N1's, and a protocol inadmissible HERE at a perfect exterior is`);
  console.log(`  inadmissible under any reading. (ii) N1 counted 23 rows including the`);
  console.log(`  clinician-selected one.`);
  console.log();
  console.log(`  WHAT DOES NOT SURVIVE IS THE SPLIT'S NAME. N1 §6a's "recoverable on the`);
  console.log(`  heatsink alone" has no referent after SINK-D-2: there is no heatsink to`);
  console.log(`  improve, and R_sink is not a purchasable parameter. What those protocols`);
  console.log(`  are recoverable ON is SPEC-SINK-04 — a lamination process and a BOM line`);
  console.log(`  against a margin-negative T1, which is OI-SINK-01, still BLOCKING. And on`);
  console.log(`  the bare shell that ships today, ${rows.length - admissibleBare} of ${rows.length} hold no tiles at all.`);
}

// ---------------------------------------------------------------------------
// §3  The TMS coil, specified electrically
// ---------------------------------------------------------------------------
//
// NP-PWR-BUDGET-001 §4.1 scaled clinical stimulator energies by E ∝ B^2 and
// got 1-2 J at 0.1 T and 16-40 J at 0.5 T, flagging OI-PWR-03 for "an actual
// capacitor-bank calculation". This is that calculation, from a coil geometry
// chosen to fit the applicator CAD_PARTS_LIST already carries.

const MU0 = 4 * Math.PI * 1e-7;
const RHO_CU = 1.68e-8;          // Ohm-m at 20 C

/** SPEC-TMS-01 — the coil. A figure-8 of two series wings. */
const COIL = {
  wingMeanRadiusM: 0.025,   // 25 mm — focal, and inside the TMS-WINDOW envelope
  turnsPerWing: 10,
  wingCount: 2,
  conductorRadiusM: 0.0015, // 6 x 1.5 mm strip, equivalent radius
  conductorAreaM2: 9e-6,    // 6 x 1.5 mm
  skinFactor: 2.0,          // R_ac / R_dc at the ring frequency, 1.5 mm strip
};
/** Single-wing inductance, round multi-turn loop:
 *  L = mu0 * N^2 * a * (ln(8a/r_w) - 2). Wings in series; wing-to-wing mutual
 *  coupling is opposing and small at this separation and is NOT credited. */
const L_WING = MU0 * COIL.turnsPerWing ** 2 * COIL.wingMeanRadiusM *
  (Math.log(8 * COIL.wingMeanRadiusM / COIL.conductorRadiusM) - 2);
const L_COIL = COIL.wingCount * L_WING;

/** Peak axial field at a wing centre: B = mu0 * N * I / (2a). */
const fieldPerAmp = MU0 * COIL.turnsPerWing / (2 * COIL.wingMeanRadiusM);
const currentForField = (bT: number) => bT / fieldPerAmp;
/** Stored energy for a peak coil current: E = 1/2 L I^2. */
const energyForField = (bT: number) => 0.5 * L_COIL * currentForField(bT) ** 2;

/** SPEC-TMS-02 — the bank. Bank voltage is the free variable; 1600 V is the
 *  clinical norm and sets C from the design-point energy. */
const V_BANK = 1600;
const DESIGN_FIELD_T = 0.5;
const C_BANK = 2 * energyForField(DESIGN_FIELD_T) / V_BANK ** 2;
/** Series LC ring. Monophasic pulse ~ half a period; biphasic ~ one period. */
const T_RING = 2 * Math.PI * Math.sqrt(L_COIL * C_BANK);
const OMEGA = 2 * Math.PI / T_RING;

/** Coil resistance at the ring frequency. */
const COND_LEN_M = COIL.wingCount * COIL.turnsPerWing * 2 * Math.PI * COIL.wingMeanRadiusM;
const R_COIL_DC = RHO_CU * COND_LEN_M / COIL.conductorAreaM2;
const R_COIL_AC = R_COIL_DC * COIL.skinFactor;
const Q_COIL = OMEGA * L_COIL / R_COIL_AC;
/** Fraction of stored energy dissipated in the coil per full ring cycle. */
const LOSS_FRAC = 2 * Math.PI / Q_COIL;

/** SPEC-TMS-04 — supply. eta_net is the fraction of stored energy that must be
 *  resupplied per pulse: 1.0 for a monophasic stimulator (nothing returns),
 *  ~0.40 for a biphasic one with bank recovery. eta_psu is the charger. */
const ETA_NET = { monophasic: 1.00, biphasicRecovered: 0.40 };
const ETA_PSU = 0.85;
const rechargeW = (bT: number, repHz: number, etaNet: number) =>
  energyForField(bT) * repHz * etaNet / ETA_PSU;

function reportTms() {
  rule("§3  SPEC-TMS — the first electrical specification of the TMS coil");
  console.log(`  SPEC-TMS-01  figure-8, ${COIL.wingCount} wings in series, wing mean radius`);
  console.log(`               ${f0(COIL.wingMeanRadiusM * 1000)} mm, ${COIL.turnsPerWing} turns/wing, ${f0(COIL.conductorAreaM2 * 1e6)} mm2 copper strip`);
  console.log(`               L = ${(L_COIL * 1e6).toFixed(1)} uH   R_ac = ${(R_COIL_AC * 1e3).toFixed(1)} mOhm   Q = ${f0(Q_COIL)}`);
  console.log(`               B per amp-turn at the wing centre: ${(fieldPerAmp * 1e3).toFixed(3)} mT/A`);
  console.log();
  console.log(`  SPEC-TMS-02  bank ${f0(C_BANK * 1e6)} uF at ${V_BANK} V  (E = ${f1(energyForField(DESIGN_FIELD_T))} J at the ${DESIGN_FIELD_T} T design point)`);
  console.log(`  SPEC-TMS-03  series ring T = ${f0(T_RING * 1e6)} us; monophasic pulse ~${f0(T_RING * 1e6 / 2)} us,`);
  console.log(`               biphasic ~${f0(T_RING * 1e6)} us; I_peak = ${f0(currentForField(DESIGN_FIELD_T))} A; dB/dt = ${(DESIGN_FIELD_T * OMEGA / 1e3).toFixed(1)} kT/s`);
  console.log();
  console.log("  Energy per pulse across the stated field range, and the check against");
  console.log("  NP-PWR-BUDGET-001 §4.1's E-proportional-to-B^2 scaling estimate:");
  console.log();
  console.log("    B (T)   I_peak (A)   E_pulse (J)   §4.1 scaling estimate");
  const scalingEstimate: Record<string, string> = { "0.10": "~1-2 J", "0.50": "~16-40 J" };
  for (const b of [0.1, 0.2, 0.3, 0.4, 0.5]) {
    const k = b.toFixed(2);
    console.log(`    ${b.toFixed(2)}   ${f0(currentForField(b)).padStart(10)}   ${f1(energyForField(b)).padStart(11)}   ${scalingEstimate[k] ?? ""}`);
  }
  console.log();
  console.log(`  Both §4.1 rows are reproduced from a coil geometry that shares no input`);
  console.log(`  with the scaling argument. OI-PWR-03 asked for exactly this substitution.`);
  console.log();
  console.log("  SPEC-TMS-04  average supply power during an active train:");
  console.log();
  console.log("    protocol                rate      monophasic   biphasic+recovery");
  const trains: { name: string; hz: number }[] = [
    { name: "rTMS 10 Hz (3000 pulses)", hz: 10 },
    { name: "rTMS 1 Hz (LF)", hz: 1 },
    { name: "iTBS (600 pulses / 190 s)", hz: 600 / 190 },
  ];
  for (const t of trains) {
    console.log(`    ${t.name.padEnd(24)}${(t.hz.toFixed(2) + " Hz").padStart(8)}` +
      `   ${(f0(rechargeW(DESIGN_FIELD_T, t.hz, ETA_NET.monophasic)) + " W").padStart(10)}` +
      `   ${(f0(rechargeW(DESIGN_FIELD_T, t.hz, ETA_NET.biphasicRecovered)) + " W").padStart(17)}`);
  }
  console.log();
  console.log(`  DESIGN FIGURE: ${f0(rechargeW(DESIGN_FIELD_T, 10, ETA_NET.biphasicRecovered))} W average, the biphasic 10 Hz row. Monophasic`);
  console.log(`  at the same point is ${f0(rechargeW(DESIGN_FIELD_T, 10, ETA_NET.monophasic))} W, which is why SPEC-TMS-02 specifies biphasic`);
  console.log(`  with bank recovery rather than leaving the topology open.`);
}

// ---------------------------------------------------------------------------
// §4  Which end of 0.1-0.5 T is a design point (OI-PWR-02)
// ---------------------------------------------------------------------------
//
// NP-PWR-BUDGET-001 §4.2 compares 0.1-0.5 T against the ~1.5-2.0 T a clinical
// figure-8 needs and concludes the target "may be too low". That comparison is
// not like-for-like: what depolarises an axon is the induced E field, which
// goes as dB/dt = B * omega, and omega is set by L*C. A small coil with a small
// bank rings roughly twice as fast as a clinical one, so it buys back a factor
// the field comparison charges it for.

const CLINICAL_REF = { bT: 1.75, periodS: 250e-6, wingRadiusM: 0.045, depthM: 0.020 };
/** Fraction of a clinical stimulator's MAXIMUM output at which resting motor
 *  threshold is typically reached. 50-70 % is the usual clinical range; 0.60 is
 *  the midpoint and the single softest input in this section. */
const RMT_FRACTION_OF_MAX = 0.60;
const RMT_MULTIPLE = 1.20;   // the evidence base's "120 % RMT"
/** Off-axis falloff of a loop's induced field, first order: (1 + (z/a)^2)^-3/2. */
const falloff = (z: number, a: number) => (1 + (z / a) ** 2) ** -1.5;
/** Induced cortical field at depth z, FIRST ORDER AND UNNORMALISED:
 *  E ~ dB/dt * a * falloff, dB/dt = B * 2*pi/T. Used only as a RATIO between
 *  two coils evaluated the same way, so the missing geometry factors — which
 *  are common to both — cancel. No absolute V/m is quoted from it. */
function eFigure(bT: number, periodS: number, aM: number, zM: number): number {
  return bT * (2 * Math.PI / periodS) * aM * falloff(zM, aM);
}
const Z = CLINICAL_REF.depthM;
const eRefMax = eFigure(CLINICAL_REF.bT, CLINICAL_REF.periodS, CLINICAL_REF.wingRadiusM, Z);
/** What the NeurOne coil has to reach, as a fraction of the reference coil at
 *  its own maximum output. */
const TARGET_FRACTION = RMT_FRACTION_OF_MAX * RMT_MULTIPLE;
const eNeurOne = (bT: number) => eFigure(bT, T_RING, COIL.wingMeanRadiusM, Z);

function reportField() {
  rule("§4  OI-PWR-02 — does 0.1-0.5 T reach the protocols the evidence base cites?");
  console.log(`  NP-PWR-BUDGET-001 §4.2 compares 0.1-0.5 T against the ~1.5-2.0 T a clinical`);
  console.log(`  figure-8 reaches and says the target "may be too low". That comparison is`);
  console.log(`  not like-for-like in EITHER direction, and both corrections matter:`);
  console.log();
  console.log(`   + what depolarises an axon is dB/dt = B * omega, and omega is set by L*C.`);
  console.log(`     SPEC-TMS-03 rings in ${f0(T_RING * 1e6)} us against the reference's ${f0(CLINICAL_REF.periodS * 1e6)} us, so this`);
  console.log(`     coil converts ${(CLINICAL_REF.periodS / T_RING).toFixed(1)}x more of each tesla into induced field. §4.2 charges`);
  console.log(`     it for a factor it does not owe.`);
  console.log(`   - a ${f0(COIL.wingMeanRadiusM * 1000)} mm wing falls off far faster with depth than a ${f0(CLINICAL_REF.wingRadiusM * 1000)} mm one:`);
  console.log(`     ${f2(falloff(Z, COIL.wingMeanRadiusM))} against ${f2(falloff(Z, CLINICAL_REF.wingRadiusM))} at ${f0(Z * 1000)} mm. §4.2 does not charge it for that at all,`);
  console.log(`     and it is the larger of the two.`);
  console.log();
  console.log(`  Both coils through the same first-order expression, so the geometry factors`);
  console.log(`  common to both cancel and only the RATIO is quoted. Target: the reference`);
  console.log(`  coil at ${RMT_MULTIPLE.toFixed(1)} x RMT, RMT being ~${(RMT_FRACTION_OF_MAX * 100).toFixed(0)} % of its maximum output -> ${TARGET_FRACTION.toFixed(2)} of maximum.`);
  console.log();
  console.log("    NeurOne B (T)   fraction of reference max   vs the 0.72 target");
  for (const b of [0.1, 0.2, 0.3, 0.4, 0.5]) {
    const frac = eNeurOne(b) / eRefMax;
    console.log(`    ${b.toFixed(2).padStart(13)}   ${frac.toFixed(3).padStart(25)}   ` +
      `${frac >= TARGET_FRACTION ? "clears" : `${(TARGET_FRACTION / frac).toFixed(1)}x SHORT`}`);
  }
  const fracTop = eNeurOne(DESIGN_FIELD_T) / eRefMax;
  const bNeeded = DESIGN_FIELD_T * TARGET_FRACTION / fracTop;
  console.log();
  console.log(`  §4.2's CONCERN IS CONFIRMED, for a reason §4.2 did not give. The top of`);
  console.log(`  the stated range reaches ${(fracTop * 100).toFixed(0)} % of the reference coil's maximum and ${(TARGET_FRACTION / fracTop).toFixed(1)}x`);
  console.log(`  short of ${RMT_MULTIPLE.toFixed(1)} x RMT. The shortfall is DEPTH FALLOFF, not peak field —`);
  console.log(`  which is why raising the field is the expensive lever:`);
  console.log();
  console.log(`    field needed at this geometry:  ${f2(bNeeded)} T  (outside the stated range)`);
  console.log(`    E_pulse there (E goes as B^2):  ${f0(energyForField(bNeeded))} J  vs ${f1(energyForField(DESIGN_FIELD_T))} J at ${DESIGN_FIELD_T} T`);
  console.log(`    supply at 10 Hz, biphasic:      ${f0(rechargeW(bNeeded, 10, ETA_NET.biphasicRecovered))} W  vs ${f0(rechargeW(DESIGN_FIELD_T, 10, ETA_NET.biphasicRecovered))} W`);
  console.log();
  console.log(`  ${(rechargeW(bNeeded, 10, ETA_NET.biphasicRecovered) / rechargeW(DESIGN_FIELD_T, 10, ETA_NET.biphasicRecovered)).toFixed(0)}x, and there is no PD class, no mains accessory and no head-worn`);
  console.log(`  thermal path in the programme that reaches it. The other lever — a larger`);
  console.log(`  wing — is a coil-design decision that belongs to OI-PWR-02, not here.`);
  console.log();
  console.log(`  CONSEQUENCE, AND IT IS THE POINT OF THIS SECTION. The TMS supply question`);
  console.log(`  has TWO answers ${(rechargeW(bNeeded, 10, ETA_NET.biphasicRecovered) / rechargeW(DESIGN_FIELD_T, 10, ETA_NET.biphasicRecovered)).toFixed(0)}x apart, and which one is right is OI-PWR-02's to`);
  console.log(`  settle, not a power question at all. SPEC-TMS-04's ${f0(rechargeW(DESIGN_FIELD_T, 10, ETA_NET.biphasicRecovered))} W is the figure`);
  console.log(`  IF CLAUDE.md §3's 0.1-0.5 T stands. §6's table carries it with that`);
  console.log(`  condition attached, which is the only honest way to carry it.`);
  console.log();
  console.log(`  SOFTEST INPUT: RMT at ${(RMT_FRACTION_OF_MAX * 100).toFixed(0)} % of maximum output. At the 50-70 % ends the`);
  console.log(`  target moves to ${(0.5 * RMT_MULTIPLE).toFixed(2)}-${(0.7 * RMT_MULTIPLE).toFixed(2)} and the shortfall to ` +
    `${(0.5 * RMT_MULTIPLE / fracTop).toFixed(1)}-${(0.7 * RMT_MULTIPLE / fracTop).toFixed(1)}x. The verdict does not turn on it.`);
}

// ---------------------------------------------------------------------------
// §5  The coil's own thermal wall
// ---------------------------------------------------------------------------

/** Head-worn coil assembly: copper + potting + housing. */
const COIL_MASS_KG = 0.20;
const COIL_C_JPKGK = 500;      // copper-dominated, epoxy-potted
const COIL_C_JPK = COIL_MASS_KG * COIL_C_JPKGK;
const APPLIED_PART_LIMIT_C = FACE_LIMIT;   // IEC 60601-1, same 42 C as PBM
const lossPerPulseJ = (bT: number) => LOSS_FRAC * energyForField(bT);

function reportCoilThermal() {
  rule("§5  The coil's thermal wall — the T2 mirror of §1");
  console.log(`  Q = ${f0(Q_COIL)} at the ring frequency, so ${(LOSS_FRAC * 100).toFixed(1)} % of the stored energy is`);
  console.log(`  dissipated in the winding per cycle: ${f2(lossPerPulseJ(DESIGN_FIELD_T))} J/pulse at ${DESIGN_FIELD_T} T.`);
  console.log();
  console.log(`  Adiabatic head-room of a ${f0(COIL_MASS_KG * 1000)} g coil assembly (${f0(COIL_C_JPK)} J/K) from ${f0(AMB_NOMINAL)} C to the`);
  console.log(`  same ${f0(APPLIED_PART_LIMIT_C)} C applied-part limit PBM is held to: ${f0(COIL_C_JPK * (APPLIED_PART_LIMIT_C - AMB_NOMINAL))} J.`);
  console.log();
  console.log("    protocol                  pulses   coil energy   vs budget");
  const budgetJ = COIL_C_JPK * (APPLIED_PART_LIMIT_C - AMB_NOMINAL);
  for (const p of [{ n: 3000, name: "rTMS 10 Hz standard" }, { n: 600, name: "iTBS" }, { n: 1200, name: "iTBS x2" }]) {
    const j = p.n * lossPerPulseJ(DESIGN_FIELD_T);
    console.log(`    ${p.name.padEnd(24)}${String(p.n).padStart(7)}   ${(f0(j) + " J").padStart(11)}   ${(j / budgetJ).toFixed(1)}x`);
  }
  console.log();
  console.log(`  Admissible pulse count on the adiabatic budget alone: ${f0(budgetJ / lossPerPulseJ(DESIGN_FIELD_T))}.`);
  console.log();
  console.log(`  THIS IS §1's FINDING AT THE OTHER TIER. The TMS power path was framed as`);
  console.log(`  a supply question — shared rail, mains tether, second inlet — for two`);
  console.log(`  revisions. Given a number, the supply is the easy half: ${f0(rechargeW(DESIGN_FIELD_T, 10, ETA_NET.biphasicRecovered))} W is an`);
  console.log(`  ordinary PD contract. What is NOT ordinary is ${f0(lossPerPulseJ(DESIGN_FIELD_T) * 10)} W of I^2R inside a coil`);
  console.log(`  pressed against a scalp under the same 42 C limit, with no cooling path`);
  console.log(`  specified and the EMF window under it required to be NON-CONDUCTIVE`);
  console.log(`  (CLAUDE.md §4.3) — which forecloses the obvious metallic spreader.`);
  console.log();
  const GATE_OFF_S = 0.055;  // CLAUDE.md §4.2: 5 ms pre-gate + 50 ms hold
  console.log(`  One more consequence that falls out of CLAUDE.md §4.2 alone. The TMS`);
  console.log(`  interlock gates EMF cancellation off 5 ms before each pulse and holds it`);
  console.log(`  ${f0(GATE_OFF_S * 1000 - 5)} ms. At 10 Hz that is ${(GATE_OFF_S * 10 * 100).toFixed(0)} % of the train; above ${f1(1 / GATE_OFF_S)} Hz it is`);
  console.log(`  continuous. iTBS's intra-burst rate is 50 Hz, so for the whole of every`);
  console.log(`  burst the product's primary technical claim is switched off. That is a`);
  console.log(`  statement about the claim, not about safety, and nothing in the set makes`);
  console.log(`  it.`);
}

// ---------------------------------------------------------------------------
// §6  CLAUDE.md §4.5, re-derived
// ---------------------------------------------------------------------------

const OVERHEAD_W = 7.0;

function reportPowerTable() {
  rule("§6  CLAUDE.md §4.5 re-derived — the two T2 rows, and the row that was missing");
  const tmsW = rechargeW(DESIGN_FIELD_T, 10, ETA_NET.biphasicRecovered);
  const tmsMono = rechargeW(DESIGN_FIELD_T, 10, ETA_NET.monophasic);
  const aggS = aggregateW(WITH_SPREADER).w;
  const rows: [string, string, string, string][] = [
    ["Standby", "1 W", "5 V / 0.5 A", "n/a"],
    ["EEG only", "2.5 W", "5 V / 1 A", "n/a"],
    ["Standard T1", "~17-20 W", "15 V / 2 A (45 W)", "inside"],
    ["T1 peak", "~45-50 W", "20 V / 3 A (65 W)",
      `NOT REACHABLE — ${f1(aggS + OVERHEAD_W)} W thermal (SPEC-SINK-01)`],
    ["T2 standard", "~44-46 W", "20 V / 3 A (65 W)", "1170 nm TEC path, separate"],
    ["T2 peak (1170 nm)", "~70-74 W", "20 V / 5 A (100 W EPR)", "1170 nm TEC path, separate"],
    ["T2 + TMS †", `~${f0(tmsW + OVERHEAD_W)} W`, "48 V / 5 A (240 W EPR)",
      `coil-limited to ${f0(aggregatePulseBudget())} pulses (§5)`],
  ];
  console.log("  Mode                 Draw        Min contract             Thermal bound");
  for (const [m, d, c, t] of rows) {
    console.log(`  ${m.padEnd(20)} ${d.padEnd(11)} ${c.padEnd(24)} ${t}`);
  }
  console.log();
  console.log(`  THE T2+TMS ROW IS NEW AND IT IS THE POINT. §4.5's 70-74 W was derived for`);
  console.log(`  the 1170 nm laser zone; NP-PWR-BUDGET-001 §4 found TMS absent from the`);
  console.log(`  electrical model entirely. At ${DESIGN_FIELD_T} T and 10 Hz it is ${f0(tmsW)} W of supply on top`);
  console.log(`  of ~${f0(OVERHEAD_W)} W of device — ${(((tmsW + OVERHEAD_W) / 74)).toFixed(1)}x the T2-peak figure five other documents`);
  console.log(`  cite as an input.`);
  console.log();
  console.log(`  TMS and 1170 nm are NOT summed. They are different applicators and no`);
  console.log(`  authored session runs both; the row is max(), not sum(). If that ever`);
  console.log(`  changes the contract becomes ${f0(tmsW + 74)} W and there is no PD class for it.`);
  console.log();
  console.log(`  A monophasic stimulator would need ${f0(tmsMono + OVERHEAD_W)} W, which no PD class delivers`);
  console.log(`  either — so SPEC-TMS-02's biphasic-with-recovery topology is not a`);
  console.log(`  preference, it is what makes the row expressible as a PD contract at all.`);
  console.log();
  console.log(`  † CONDITIONAL ON OI-PWR-02, and the condition is not a footnote. §4 puts`);
  console.log(`  the ${DESIGN_FIELD_T} T design point ${(TARGET_FRACTION / (eNeurOne(DESIGN_FIELD_T) / eRefMax)).toFixed(1)}x short of the cortical field 120 % RMT needs at`);
  console.log(`  this wing radius. If OI-PWR-02 keeps CLAUDE.md §3's 0.1-0.5 T, this row is`);
  console.log(`  the row. If it raises the field to clinical equivalence at this geometry,`);
  console.log(`  the requirement is ~${f0(rechargeW(DESIGN_FIELD_T * TARGET_FRACTION / (eNeurOne(DESIGN_FIELD_T) / eRefMax), 10, ETA_NET.biphasicRecovered))} W and the modality is not head-worn at all. The`);
  console.log(`  row is written at the lower answer and labelled, never averaged between`);
  console.log(`  them.`);
}
function aggregatePulseBudget(): number {
  return COIL_C_JPK * (APPLIED_PART_LIMIT_C - AMB_NOMINAL) / lossPerPulseJ(DESIGN_FIELD_T);
}

// ---------------------------------------------------------------------------
// §7  The four options, and OI-PWR-11's arithmetic
// ---------------------------------------------------------------------------

function reportOptions() {
  rule("§7  OI-PWR-04 — the option set, priced against a real number");
  const tmsW = rechargeW(DESIGN_FIELD_T, 10, ETA_NET.biphasicRecovered);
  const need = tmsW + OVERHEAD_W;
  console.log(`  Requirement, for the first time a number: ${f0(need)} W sustained during a train.`);
  console.log();
  const opts: [string, string, string][] = [
    ["(a) shared rail, one inlet", `240 W EPR covers ${f0(need)} W`,
      "feasible NOW; was not, when the requirement was unbounded"],
    ["(b) mains-tethered", "unbounded", "NeurOne owns IEC 60601-1 isolation; costs Mode 3 device-wide"],
    ["(c) second PD sink on the hub", `2 x 100 W = 200 W`,
      `${((200 / need - 1) * 100).toFixed(0)} % margin; OI-PWR-12 EMF bench inside the shielded assembly`],
    ["(d) inlet on the TMS driver", `${f0(need)} W where the load is`,
      "no second inlet on the head-worn hub at all"],
  ];
  for (const [n, cap, note] of opts) console.log(`  ${n.padEnd(30)} ${cap.padEnd(24)} ${note}`);
  console.log();
  console.log(`  (c) WAS the lead candidate and its whole case was scoping: it "scopes the`);
  console.log(`  Mode 3 loss to the modality that forces it". With ${f0(need)} W in hand that case`);
  console.log(`  evaporates — no power bank sources ${f0(need)} W, so TMS is outside Mode 3 by`);
  console.log(`  arithmetic whatever the inlet count is. What (c) still costs is real:`);
  console.log(`  OI-PWR-12 puts a 5 A switched aggressor inside the assembly whose MEASURED`);
  console.log(`  shielding is the product's primary claim.`);
  console.log();
  console.log(`  (d) pays none of that. §5 puts ${f0(lossPerPulseJ(DESIGN_FIELD_T) * 10)} W of dissipation and a ${f0(C_BANK * 1e6)} uF / ${V_BANK} V`);
  console.log(`  bank in the applicator regardless; the inlet belongs where they are.`);
  console.log();
  rule("§7b  OI-PWR-11 — 130 W against the requirement");
  console.log(`  CLAUDE.md §2.2 ships Pro Full "65 W NeurOne GaN (branded) x 2", $26 BOM,`);
  console.log(`  against a §4.5 row negotiating ONE 100 W EPR contract. Three readings were`);
  console.log(`  on the table and the document set could not distinguish them. It can now:`);
  console.log();
  console.log(`    reading 2 — a dual-inlet architecture was assumed: 2 x 65 = 130 W against`);
  console.log(`      a ${f0(need)} W requirement. Short by ${((need / 130 - 1) * 100).toFixed(0)} %. REFUTED as a TMS provision.`);
  console.log(`    reading 3 — a separate T2 accessory: 65 W covers a 1170 nm applicator`);
  console.log(`      (44-46 W) and covers NOTHING at TMS. Survives only for 1170 nm.`);
  console.log(`    reading 1 — an undocumented spare: survives, and is now the only reading`);
  console.log(`      consistent with both §4.5 and SPEC-TMS-04.`);
  console.log();
  console.log(`  Whatever the second brick is, it is not the TMS supply, and the charger`);
  console.log(`  ladder cannot be made to express TMS by adding 65 W rungs to it.`);
}

// ---------------------------------------------------------------------------
// §8  OI-POE-09(b) — the efficacy-floor band, denominated in dose
// ---------------------------------------------------------------------------

const EFFICACY_FLOOR_J = 10;
const T_FULL = 30, T_BLOCK = 35;
const dutyFloor = (fullDoseJ: number) => EFFICACY_FLOOR_J / fullDoseJ;
const clampAmbient = (fullDoseJ: number) => T_BLOCK - dutyFloor(fullDoseJ) * (T_BLOCK - T_FULL);
/** Dose delivered on re-admission, for a band of `dC` degrees. */
const doseAtReArm = (fullDoseJ: number, dC: number) =>
  fullDoseJ * (dutyFloor(fullDoseJ) + dC / (T_BLOCK - T_FULL));
/** Degrees needed to make re-admission land at `mult` x the floor. */
const bandForMultiple = (fullDoseJ: number, mult: number) =>
  (T_BLOCK - T_FULL) * (mult - 1) * EFFICACY_FLOOR_J / fullDoseJ;

/** POE-D: the band composes by max() with the sense path's resolution, per
 *  NP-FW-POE-001 §6.1's own rule for how margins enter. */
const SENSE_FLOOR_C = 1.0;   // OI-POE-06: adc_to_celsius() returns whole degrees
const TARGET_MULTIPLE = 2.0;
const bandEff = (fullDoseJ: number) =>
  Math.max(SENSE_FLOOR_C, bandForMultiple(fullDoseJ, TARGET_MULTIPLE));

function reportPoe() {
  rule("§8  OI-POE-09(b) — a band chosen in dose instead of inherited in degrees");
  console.log(`  Delta = 1.0 C came from NP-THERM-COOL-001 §7.5.1 on three grounds — ADC`);
  console.log(`  representation, room-thermostat differential, re-arm wait — none about`);
  console.log(`  dose. Carried onto the efficacy-floor edge it buys, per protocol:`);
  console.log();
  console.log("    full dose   blocks at   re-arm @1.0C   dose there   x floor");
  for (const d of [40, 60, 90, 120]) {
    const b = clampAmbient(d);
    console.log(`    ${(String(d) + " J/cm2").padStart(11)}   ${f1(b).padStart(9)}   ${f1(b - 1).padStart(12)}` +
      `   ${(f0(doseAtReArm(d, 1)) + " J").padStart(10)}   ${(doseAtReArm(d, 1) / EFFICACY_FLOOR_J).toFixed(1)}x`);
  }
  console.log();
  console.log(`  Monotone in dose, 1.8x to 3.4x — defensible everywhere, chosen nowhere.`);
  console.log(`  The quantity the edge actually guards is how far above the floor a`);
  console.log(`  re-admitted session must land, which is a DOSE. Denominate it there:`);
  console.log(`  re-admit only at >= ${TARGET_MULTIPLE.toFixed(1)}x the floor (${f0(TARGET_MULTIPLE * EFFICACY_FLOOR_J)} J/cm2).`);
  console.log();
  console.log("    full dose   band for 2.0x   expressible?     POE-D band = max(1.0, .)");
  for (const d of [40, 60, 90, 120]) {
    const raw = bandForMultiple(d, TARGET_MULTIPLE);
    console.log(`    ${(String(d) + " J/cm2").padStart(11)}   ${(f2(raw) + " C").padStart(13)}` +
      `   ${(raw >= SENSE_FLOOR_C ? "yes" : "NO — below 1.0 C").padEnd(16)} ${f2(bandEff(d))} C`);
  }
  console.log();
  console.log(`  The dose rule alone is UNIMPLEMENTABLE above ${f0(EFFICACY_FLOOR_J * TARGET_MULTIPLE * (T_BLOCK - T_FULL) / SENSE_FLOOR_C - EFFICACY_FLOOR_J * (T_BLOCK - T_FULL) / SENSE_FLOOR_C)} J/cm2: OI-POE-06 records`);
  console.log(`  that adc_to_celsius() returns whole degrees, so 1.0 C is the finest band`);
  console.log(`  the sense path can express. Composing by max() — which is how §6.1 already`);
  console.log(`  requires margins to enter — gives a rule that is chosen where it can be`);
  console.log(`  and falls back to the representation floor where it cannot, and that`);
  console.log(`  delivers >= 2.0x the floor on EVERY row.`);
  console.log();
  console.log(`  It changes behaviour only for protocols at or below ${f0(EFFICACY_FLOOR_J * (TARGET_MULTIPLE - 1) * (T_BLOCK - T_FULL) / SENSE_FLOOR_C)} J/cm2, which is a`);
  console.log(`  bounded change to an Efficacy-class refusal in SW-02 and touches no`);
  console.log(`  Class C state. That is the (a) half: TWO latches, one per class.`);
}

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

function reportValidation(): boolean {
  rule("§0  Validation — anchors this report must not drift from");
  let ok = true;
  const check = (name: string, got: number, want: number, tol: number, unit = "") => {
    const pass = Math.abs(got - want) <= tol;
    ok &&= pass;
    console.log(`  ${pass ? "ok  " : "FAIL"} ${name.padEnd(46)} ${f2(got)}${unit} vs ${f2(want)}${unit} +/- ${tol}`);
  };
  // The rejection specification this report is built on — CURRENT geometry
  // (Layer 4 deleted, outer bowl re-lofted 3 mm inward; OI-THCOOL-21).
  check("SPEC-SINK-01 R_sink (NP-THERM-SINK-001 §4)", R_SINK_SPEC, 1.14, 0.02, " K/W");
  check("SPEC-SINK-01 h_ext", H_EXT_SPEC, 7.83, 0.05, " W/m2K");
  check("multitile R_SINK_SPECIFIED == SPEC-SINK-01", R_SINK_SPECIFIED, R_SINK_SPEC, 0.005, " K/W");
  check("exterior effective area", A_EXT_EFF, 0.1125, 0.002, " m2");
  // NP-THERM-SINK-001 §8's published ceilings — this script must reproduce the
  // document it takes as an input, or one of the two has moved.
  check("SINK §8 ceiling, library floor, S3, 25 C", ceilingTiles(OP.libMin, WITH_SPREADER), 10, 0, " tiles");
  check("SINK §8 ceiling, library floor, S0, 25 C", ceilingTiles(OP.libMin, BARE), 1, 0, " tiles");
  check("SINK §8.1 admissible W at N = 6, S3", admissibleW(6, WITH_SPREADER), 8.8, 0.3, " W");
  check("SINK §8.1 aggregate W, S3", aggregateW(WITH_SPREADER).w, 30.5, 0.5, " W");
  // HISTORICAL — NP-THERM-SINK-001 Rev 1 as published, foam in and R1's 12 mm
  // exterior. Kept so the as-was figures still reproduce where cited.
  const WAS: SinkOpts = { asWas: true };
  check("[as-was] SPEC-SINK-01 R_sink, Rev 1", R_SINK_SPEC_R1, 1.08, 0.02, " K/W");
  check("[as-was] exterior effective area", A_EXT_EFF_R1, 0.118, 0.002, " m2");
  check("[as-was] ceiling, library floor, S0, 25 C", ceilingTiles(OP.libMin, WAS), 2, 0, " tiles");
  check("[as-was] aggregate W, S3", aggregateW({ ...WITH_SPREADER, ...WAS }).w, 31.4, 0.5, " W");
  // aggregateW() takes the full lattice as the best N. That is NP-THERM-SINK-001
  // §8.1's claim, not an assumption of this script — so it is checked, not held.
  {
    const seq = [1, 2, 6, 12, 20, 37, 80].map((n) => admissibleW(n, WITH_SPREADER));
    const mono = seq.every((w, i) => i === 0 || w > seq[i - 1]);
    ok &&= mono;
    console.log(`  ${mono ? "ok  " : "FAIL"} ${"admissible TOTAL rises monotonically with N".padEnd(46)} ` +
      seq.map((w) => f1(w)).join(" < "));
  }
  // NP-PWR-BUDGET-001 §4.1's scaling estimate, which §3 must land inside.
  check("SPEC-TMS E_pulse at 0.5 T vs §4.1 16-40 J", energyForField(0.5), 28, 12, " J");
  check("SPEC-TMS E_pulse at 0.1 T vs §4.1 1-2 J", energyForField(0.1), 1.5, 0.5, " J");
  check("SPEC-TMS-04 monophasic 10 Hz vs §4.1 ~400 W",
    rechargeW(0.5, 10, ETA_NET.monophasic), 400, 60, " W");
  check("SPEC-TMS-04 monophasic iTBS vs §4.1 ~130 W",
    rechargeW(0.5, 600 / 190, ETA_NET.monophasic), 130, 25, " W");
  // Internal consistency: I_peak from the bank must equal I_peak from the field.
  check("I_peak: V*sqrt(C/L) == B/(mu0*N/2a)",
    V_BANK * Math.sqrt(C_BANK / L_COIL), currentForField(DESIGN_FIELD_T), 1, " A");
  // OI-POE-09(b): the inherited band's published dose multiples.
  check("POE §7.4.4 dose at re-arm, 40 J/cm2", doseAtReArm(40, 1) / 10, 1.8, 0.05, "x");
  check("POE §7.4.4 dose at re-arm, 120 J/cm2", doseAtReArm(120, 1) / 10, 3.4, 0.05, "x");
  console.log();
  console.log(`  ${ok ? "PASS" : "FAIL"} — the first block asserts this report is reading the`);
  console.log(`  rejection specification it claims to; the second asserts SPEC-TMS lands on`);
  console.log(`  NP-PWR-BUDGET-001 §4.1's independent scaling estimate rather than replacing`);
  console.log(`  it with an unrelated number.`);
  return ok;
}

function main() {
  const ok = reportValidation();
  if (VALIDATE_ONLY) { process.exit(ok ? 0 : 1); return; }
  reportCeiling();
  reportT1Peak();
  reportProtocols();
  reportTms();
  reportField();
  reportCoilThermal();
  reportPowerTable();
  reportOptions();
  reportPoe();
  console.log();
}

if (import.meta.main) main();
