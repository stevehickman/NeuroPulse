#!/usr/bin/env bun
/**
 * check-pbm-subgroup.ts — the ICNIRP repetitive-exposure sub-group rule, run over the
 * predefined protocol library.
 *
 * `NP-BIB-PBMIRR-001` §3.1 item 1 and §5.1 (`OI-BIBPBM-05`). ICNIRP 2013 (laser
 * guidelines, "Repetitive pulse exposures", rule 2) requires that the exposure from
 * ANY group of pulses delivered in time T not exceed the exposure limit for time T,
 * with T varied from the pulse duration to the whole exposure. For skin at
 * 400–1400 nm the limit for T ≤ 10 s is 1.1 × C_A × T^0.25 J/cm² and above 10 s it is
 * 200 × C_A mW/cm² (ICNIRP 2013 laser, Table 7; C_A from Table 3). A check that
 * compares only the time average would pass a protocol of long, sparse bursts whose
 * average is inside the limit and whose bursts are not.
 *
 * For each `pbm_transcranial` block this script slides the block's periodic pulse
 * train over every window T from 1 ms to the longest ON run it can have, and scores
 * the worst window as Σ over the wavelengths on the tile of (exposure in the window
 * ÷ the limit for T). A score above 1 breaches the rule. The weighted sum across
 * wavelengths is the form R-4 uses (D-10, `OI-HEXTILE-30`/`-31`); its C_A weights
 * are the unverified part (`OI-BIBPBM-01`), so a score is a reading, not a verdict.
 *
 * It also reports the one thing the parser now refuses (`OI-SESPWR-03`): a block that
 * writes `frequency: 0` with a duty other than 100 %. The shipped library has none,
 * so a hit here means a protocol was added that `nppsParser.ts` will not load.
 *
 * What it does NOT do: it is not the pre-signing check (`OI-HEXTILE-30`/`-31`), it
 * sets no limit, and a pulse train is assumed periodic at the stated frequency and
 * duty. Irradiance is the block's own `irradiance` (NP-NPPS-REF-001 Rev 18), capped at what
 * its channel delivers at full drive (the §4.3 anchor, a design target: `OI-HEXTILE-20`).
 * Blocks that share zones, frequency, duty and start are one exposure, so their
 * wavelengths are summed, as the rule requires.
 *
 *   bun scripts/check-pbm-subgroup.ts            # report, always exits 0
 *   bun scripts/check-pbm-subgroup.ts --strict   # exit 1 if any block scores above 1
 *
 * --strict is not wired into CI: `07-vascular-baseline` scores above 1 today and also
 * breaches R-4's average term (`OI-SESPWR-02`), so a gate would fail from its first run.
 *
 * CI-Kind: report
 */
import { readFileSync, readdirSync } from "fs";
import { join } from "path";

const DIR = "protocols/predefined";

/** Full-drive irradiance per channel, mW/cm², by channel (NP-HW-HEXTILE-001 §4.3.1 for
 *  the T1-A channels, §4.3.2 for CH_C). Design targets — OI-HEXTILE-20, OI-HEXTILE-21. */
const ANCHOR_T1A = 403;
const ANCHOR_CH_C = 28;

/** ICNIRP 2013 laser, Table 3. */
const cA = (nm: number): number =>
  nm < 700 ? 1 : nm < 1050 ? 10 ** (0.002 * (nm - 700)) : 5;

/** Skin exposure limit for a window of T seconds, in mW·s/cm² (ICNIRP 2013 laser, Table 7). */
const limit = (nm: number, T: number): number =>
  T <= 10 ? 1100 * cA(nm) * T ** 0.25 : 200 * cA(nm) * T;

export type Block = {
  file: string; kind: string; wavelengths: number[]; peakMwCm2: number[];
  frequencyHz: number; dutyPct: number; cw: boolean; burstS: number;
  worst: number; worstT: number; refused: string | null;
};

const field = (body: string, name: string): string | undefined => {
  const m = new RegExp(`(?:^|\\n)\\s*${name}:\\s*([^\\n#]+)`).exec(body);
  return m ? m[1].trim() : undefined;
};
const number = (s: string | undefined, d: number): number => {
  if (s === undefined) return d;
  const v = parseFloat(s.replace(/[^\d.]/g, ""));
  return Number.isFinite(v) ? v : d;
};
const seconds = (s: string | undefined): number | null => {
  const m = /^(\d+(?:\.\d+)?)\s*(s|m|h)$/.exec(s ?? "");
  return m ? Number(m[1]) * { s: 1, m: 60, h: 3600 }[m[2] as "s" | "m" | "h"] : null;
};

/** The wavelength a block states and the peak irradiance it delivers there, capped at its channel's full drive. */
function channel(wavelength: string, irradiance: number): { nm: number; peak: number } {
  const nm = parseFloat(wavelength);
  const anchor = nm >= 1050 ? ANCHOR_CH_C : ANCHOR_T1A;
  return { nm, peak: Math.min(irradiance, anchor) };
}

/** Worst Σ E/EL over windows, for a periodic train of ON width w every p seconds. */
function score(nm: number[], peak: number[], cw: boolean, p: number, w: number, tMax: number) {
  let worst = 0;
  let worstT = 0;
  for (let k = -300; k <= 450; k++) {
    const T = 10 ** (k / 50);          // 1 ms … ~1e9 s, trimmed to the longest ON run
    if (T > tMax) break;
    let onS: number;
    if (cw) onS = T;
    else {
      const n = Math.floor(T / p);
      onS = n * w + Math.min(w, T - n * p);
    }
    const r = nm.reduce((sum, l, i) => sum + (peak[i] * onS) / limit(l, T), 0);
    if (r > worst) { worst = r; worstT = T; }
  }
  return { worst, worstT };
}

export function analyse(): Block[] {
  const out: Block[] = [];
  for (const file of readdirSync(DIR).sort()) {
    if (!file.endsWith(".npps") || file.startsWith("00-")) continue;
    const text = readFileSync(join(DIR, file), "utf8");
    const sessionS = seconds(field(text, "duration")) ?? 1800;
    const groups = new Map<string, { nm: number[]; peak: number[]; body: string; freq: number; duty: number; cw: boolean; refused: string | null }>();
    for (const m of text.matchAll(/(pbm_transcranial)\s*\{([\s\S]*?)\n {4}\}/g)) {
      const body = m[2].replace(/^\s*#.*$/gm, "");
      const freq = number(field(body, "frequency"), 40);
      const dutyRaw = field(body, "duty_cycle");
      const cw = freq === 0;
      const duty = cw ? 100 : number(dutyRaw, 25);
      const { nm, peak } = channel(
        (field(body, "wavelength") ?? "").replace(/"/g, ""),
        parseFloat(field(body, "irradiance") ?? "0") || 0,
      );
      if (!Number.isFinite(nm) || peak <= 0) continue;
      const refused =
        cw && dutyRaw !== undefined && number(dutyRaw, 100) !== 100
          ? `frequency: 0 with duty_cycle ${dutyRaw}: nppsParser.ts refuses it (OI-SESPWR-03)`
          : null;
      const key = [field(body, "zones"), freq, duty, field(body, "start"), field(body, "interval_on")].join("|");
      const g = groups.get(key) ?? { nm: [], peak: [], body, freq, duty, cw, refused };
      g.nm.push(nm); g.peak.push(peak);
      groups.set(key, g);
    }
    for (const g of groups.values()) {
      const p = g.cw ? 0 : 1 / g.freq;
      const w = g.cw ? 0 : (g.duty / 100) * p;
      const burstS = g.cw ? seconds(field(g.body, "interval_on")) ?? sessionS : w;
      const { worst, worstT } = score(g.nm, g.peak, g.cw, p, w, g.cw ? burstS : sessionS);
      out.push({
        file, kind: "pbm_transcranial", wavelengths: g.nm, peakMwCm2: g.peak, frequencyHz: g.freq,
        dutyPct: g.duty, cw: g.cw, burstS, worst, worstT, refused: g.refused,
      });
    }
  }
  return out.sort((a, b) => b.worst - a.worst);
}

if (import.meta.main) {
  const rows = analyse();
  const over = rows.filter((r) => r.worst > 1);
  console.log("PBM sub-group audit — ICNIRP 2013 repetitive-exposure rule 2 (NP-BIB-PBMIRR-001 §5.1)");
  console.log("score = worst Σ over wavelengths of (exposure in window ÷ skin limit for the window); > 1 breaches\n");
  console.log(
    "file".padEnd(44) + "mode".padEnd(14) + "peak mW/cm²".padStart(12) +
    "ON run".padStart(10) + "score".padStart(8) + "  worst T",
  );
  console.log("-".repeat(100));
  for (const r of rows) {
    const mode = r.cw ? "CW" : `${r.frequencyHz}Hz ${r.dutyPct}%`;
    const run = r.burstS < 1 ? `${(r.burstS * 1000).toFixed(1)} ms` : `${r.burstS.toFixed(0)} s`;
    console.log(
      r.file.slice(0, 43).padEnd(44) + mode.padEnd(14) +
      r.peakMwCm2.map((x) => x.toFixed(0)).join("+").padStart(12) +
      run.padStart(10) + r.worst.toFixed(2).padStart(8) +
      `  ${r.worstT < 1 ? (r.worstT * 1000).toFixed(0) + " ms" : r.worstT.toFixed(1) + " s"}` +
      (r.worst > 1 ? "   ⚠ BREACH" : ""),
    );
    if (r.refused) console.log(`      ⚠ ${r.refused}`);
  }
  console.log(`\n${rows.length} pbm_transcranial block(s); ${over.length} above 1.`);
  console.log("Inherits OI-HEXTILE-20 (the 403 mW/cm² anchor) and OI-BIBPBM-01 (C_A weights as applied).");
  console.log("A score is a reading of ICNIRP's wording, not a pre-signing check (OI-HEXTILE-30/-31).");
  if (process.argv.includes("--strict") && over.length) process.exit(1);
}
