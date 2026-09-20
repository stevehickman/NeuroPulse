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
 * seam, which is a different excitation — and above 3 GHz the foam is no longer
 * electrically thin. `NP-EMC-CAV-001` `OI-EMCCAV-06` carries that gap.
 */

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
const ABSORBER_T_M = 0.003; // EMF L4, the layer under audit

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
const THERM = {
  outwardTotal: 0.41, // m2K/W, NP-THERM-COOL-001 §2 / R1 §2
  stationT: ABSORBER_T_M, // 3.0 mm
  kFoam: 0.04, // carbon-loaded open-cell, NP-THERM-COOL-001 §2
  kAir: 0.026, // stagnant air, same table's 6 mm gap term
  rSubstitution: 0.02, // ceramic-filled elastomer target, §6.3 / OI-THCOOL-04
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

function fieldLimit() {
  const dvUv = EEG_BUDGET_UVPP * RF_DEMOD_SHARE; // uV allowed from demodulation
  const vRfMax = (dvUv * 1e-6) * 10 ** (EMIRR_DB / 20); // volts at the amplifier input
  const hEff = PICKUP_LEN_M / 2; // electrically short: h_e ~ l/2
  const coupling = hEff * 10 ** (-CMRR_AT_RF_DB / 20); // V per (V/m)
  return { dvUv, vRfMax, hEff, coupling, eMax: vRfMax / coupling };
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
    ["delete, outer bowl re-lofted 3 mm", T - rFoam, "<- BEST; a CAD edit, no mould is cut"],
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
  reportQ();
  reportAbsorber();
  reportVerdict();
  reportThermal();
  console.log();
}

if (import.meta.main) main();
