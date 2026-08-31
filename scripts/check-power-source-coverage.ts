#!/usr/bin/env bun
/**
 * check-power-source-coverage.ts — which power source unlocks which protocol.
 *
 * `NP-SES-PWR-001` D-1: *the audit is a committed script, not a table in a
 * document.* This is the same principle applied to the question `NP-PWRSRC-001`
 * exists to answer — given a candidate power source, how many of the authored
 * `pbm_transcranial` protocols can run **single-pass** (every commanded socket
 * lit at once, no cascading)?
 *
 * It shares `analyse()` with `scripts/check-pbm-power.ts` deliberately, so the
 * two cannot diverge. That script owns the per-protocol demand model; this one
 * owns only the source envelopes and the arithmetic of subtraction.
 *
 * ── Two things this script reports that a single number cannot ───────────────
 *
 * 1. **The demand distribution is a cliff, not a slope.** Coverage does not rise
 *    smoothly with watts. Sorting the library by demand shows a dense band and
 *    then a long gap, and the study's recommendation turns on where the gap sits.
 * 2. **`OI-SESPWR-03` is worth up to 4× on a fifth of the library.** Protocols
 *    setting both `frequency: 0Hz` and `duty_cycle:` are undefined; `analyse()`
 *    reports the CW reading (higher) per `NP-SES-PWR-001` D-3. This script
 *    reports coverage under BOTH readings, so a source can only ever be sized
 *    against a figure whose uncertainty is visible on the same line.
 *
 *   bun scripts/check-power-source-coverage.ts
 *
 * CI-Kind: report
 */
import { analyse, type Row } from "./check-pbm-power";

/** Non-PBM overhead subtracted from every source envelope before comparing it to
 *  emitter demand: NP-HW-HEXTILE-001 §9.1 gives ~6–8 W (processor stack, EEG
 *  front end, safety MCU, fan, hub logic). The conservative end is used, which
 *  reproduces `check-pbm-power.ts`'s 40 W from the R-10 mid-envelope of 48 W. */
const OVERHEAD_W = 8.0;

type Source = { id: string; label: string; envelopeW: number; mode3: boolean };

/** Candidate sources. Envelope is what the source delivers to the helmet inlet,
 *  before OVERHEAD_W. USB-C figures are PD 3.x fixed contracts; the mains figures
 *  are the two bracketing points NP-PWRSRC-001 §3 costs. */
const SOURCES: Source[] = [
  { id: "R-10", label: "R-10 today (45–50 W, 45 W brick)", envelopeW: 48, mode3: true },
  { id: "PD-65", label: "USB-C PD 65 W (20 V/3.25 A)", envelopeW: 65, mode3: true },
  { id: "PD-100", label: "USB-C PD 100 W (20 V/5 A)", envelopeW: 100, mode3: true },
  { id: "PD-140", label: "USB-C PD 140 W EPR (28 V/5 A)", envelopeW: 140, mode3: true },
  { id: "PD-240", label: "USB-C PD 240 W EPR (48 V/5 A)", envelopeW: 240, mode3: true },
  { id: "PD-240x2", label: "2 x USB-C PD 240 W EPR", envelopeW: 480, mode3: true },
  { id: "MAINS-500", label: "Mains base station, 500 W DC out", envelopeW: 500, mode3: false },
  { id: "MAINS-1800", label: "Mains base station, 1,800 W DC out", envelopeW: 1800, mode3: false },
];

/** Per-tile draw under the *other* reading of OI-SESPWR-03: where a protocol sets
 *  frequency 0 Hz (CW) AND a duty cycle, apply the duty instead of ignoring it. */
function perTileDutyReading(r: Row): number {
  const ambiguous = r.notes.some((n) => n.startsWith("CW+duty"));
  if (!ambiguous || r.duty === null) return r.perTileW;
  return (r.perTileW * r.duty) / 100;
}

const rows = analyse();
const demandCW = (r: Row) => r.requiredW;
const demandDuty = (r: Row) => (r.sockets === null ? null : r.sockets * perTileDutyReading(r));

const valued = rows.filter((r) => r.requiredW !== null);
const indeterminate = rows.filter((r) => r.requiredW === null);

console.log(`Power-source coverage — ${rows.length} protocols carrying pbm_transcranial`);
console.log(`(NP-PWRSRC-001 §3; demand model from scripts/check-pbm-power.ts; overhead ${OVERHEAD_W} W)\n`);

// ── 1. the demand distribution, sorted, so the cliff is visible ───────────────
const sortedCW = valued.map(demandCW).filter((w): w is number => w !== null).sort((a, b) => a - b);
console.log("Demand distribution, W to emitters, CW reading (ascending):");
console.log("  " + sortedCW.map((w) => Math.round(w)).join("  "));
let biggestGap = { from: 0, to: 0, ratio: 0 };
for (let i = 1; i < sortedCW.length; i++) {
  const ratio = sortedCW[i] / sortedCW[i - 1];
  if (ratio > biggestGap.ratio) biggestGap = { from: sortedCW[i - 1], to: sortedCW[i], ratio };
}
console.log(
  `  largest single step: ${Math.round(biggestGap.from)} W -> ${Math.round(biggestGap.to)} W ` +
    `(${biggestGap.ratio.toFixed(1)}x) — nothing in the library asks for anything between.\n`,
);

// ── 2. coverage per source, both readings ────────────────────────────────────
console.log(
  "source".padEnd(38) + "to emit".padStart(9) + "CW rdg".padStart(9) +
    "duty rdg".padStart(10) + "Mode 3".padStart(9),
);
console.log("-".repeat(75));
for (const s of SOURCES) {
  const avail = s.envelopeW - OVERHEAD_W;
  const cw = valued.filter((r) => (demandCW(r) as number) <= avail).length;
  const du = valued.filter((r) => (demandDuty(r) as number) <= avail).length;
  console.log(
    s.label.slice(0, 38).padEnd(38) +
      `${Math.round(avail)} W`.padStart(9) +
      `${cw}/${rows.length}`.padStart(9) +
      `${du}/${rows.length}`.padStart(10) +
      (s.mode3 ? "yes" : "no").padStart(9),
  );
}
console.log(
  `\n${indeterminate.length} protocol(s) are indeterminate under every source ` +
    `(operator-selected sockets), so no row above can reach ${rows.length}/${rows.length}: ` +
    indeterminate.map((r) => r.name).join(", ") + ".",
);

// ── 3. what each step up actually buys ───────────────────────────────────────
console.log("\nMarginal protocols unlocked by each step (CW reading):");
let prev = 0;
for (const s of SOURCES) {
  const avail = s.envelopeW - OVERHEAD_W;
  const gained = valued.filter(
    (r) => (demandCW(r) as number) <= avail && (demandCW(r) as number) > prev,
  );
  console.log(
    `  ${s.id.padEnd(11)} +${gained.length}` +
      (gained.length
        ? `  ${gained.map((r) => `${r.name.slice(0, 30)} (${Math.round(demandCW(r) as number)} W)`).join("; ")}`
        : ""),
  );
  prev = Math.max(prev, avail);
}

// ── 4. the OI-SESPWR-03 exposure, named ──────────────────────────────────────
const amb = rows.filter((r) => r.notes.some((n) => n.startsWith("CW+duty")));
console.log(`\nOI-SESPWR-03 exposure — ${amb.length} of ${rows.length} protocols are CW+duty ambiguous:`);
for (const r of amb) {
  const hi = demandCW(r), lo = demandDuty(r);
  console.log(
    `  ${r.name.slice(0, 40).padEnd(42)}` +
      (hi === null || lo === null
        ? "indeterminate (operator-selected sockets)"
        : `${Math.round(lo)} W … ${Math.round(hi)} W  (${(hi / lo).toFixed(1)}x)`),
  );
}
