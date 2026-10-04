#!/usr/bin/env bun
/**
 * check-pbm-power.ts — what every predefined protocol actually asks the PBM rail for.
 *
 * `NP-HW-HEXTILE-001` §9 states a concurrency ceiling of "~6 tiles" and §9.3 raises
 * OI-HEXTILE-09: nothing in the delivered v2 wire format stops a protocol naming
 * more sockets than the USB-C PD contract can feed. `app/web/src/lib/hubCompiler.ts`
 * has no power or budget check of any kind, so every over-budget protocol in
 * `protocols/predefined/` compiles clean today.
 *
 * This script is the missing arithmetic, run over the authored library rather than
 * over a hypothetical. It is the source of the tables in `NP-SES-PWR-001`.
 *
 * ── The "~6 tiles" figure is itself the thing to distrust ─────────────────────
 *
 * §9.2's ~6 assumes 6.25 W/tile — 100 % intensity, 25 % duty, both channels. No
 * authored protocol runs there. Real per-tile draw across the library spans
 * 1.2 W to 20.0 W, so the true concurrency limit spans 2 to 32 tiles. A governor
 * expressed as a tile count is wrong in both directions; it must be watts
 * (NP-PWR-BUDGET-001 D-4). This script therefore reports watts and derives the
 * tile count, never the reverse.
 *
 *   bun scripts/check-pbm-power.ts            # report, always exits 0
 *   bun scripts/check-pbm-power.ts --strict   # exit 1 if any protocol is over budget
 *
 * --strict is NOT wired into CI yet, deliberately: 18 of 20 protocols are over
 * budget as authored, so a gate would fail from the first commit and be disabled.
 * It is turned on once the OI-SESPWR-01..03 remediation lands (OI-SESPWR-05).
 *
 * CI-Kind: report
 */
import { readFileSync, readdirSync } from "fs";
import { join } from "path";
import { TILE_W, AVAILABLE_W } from "./pbm-model";
import { PBM_FULL_SCALE_MW_CM2 } from "../app/web/src/lib/pbmDrive";
import { DEFAULT_WAVELENGTH_RULES, mapWavelength, parsePbmWavelength } from "../app/web/src/lib/wavelengthRules";

const DIR = "protocols/predefined";

/** Per-tile electrical draw at 100 % intensity and full 150 mA drive, by wavelength
 *  set, and the watts available to emitters (R-10 T1 peak envelope less the ~6–8 W
 *  non-PBM overhead of NP-HW-HEXTILE-001 §9.1). Both now live in
 *  hardware/np_pbm_model.json, the one source the C governor is emitted from too
 *  (OI-PWRSRC-13); each value's derivation is recorded beside it there. They still
 *  inherit OI-HEXTILE-02 (no emitter is selected) and OI-HEXTILE-20. Re-exported so
 *  the scripts that import them from here keep working. */
export { TILE_W, AVAILABLE_W };

export type Row = {
  file: string; name: string; sockets: number | null; wavelength: string;
  irradiance: string; cw: boolean; duty: number | null; perTileW: number;
  requiredW: number | null; maxConcurrent: number; groups: number | null;
  durationS: number | null; zoneLabel: string; notes: string[];
};

function field(body: string, name: string): string | undefined {
  // Strip trailing `# comment` — several protocols carry long rationale comments.
  const m = new RegExp(`${name}:\\s*([^\\n#]+)`).exec(body);
  return m ? m[1].trim() : undefined;
}
const num = (s: string | undefined, d: number): number => {
  if (s === undefined) return d;
  const v = parseFloat(s.replace(/[^\d.]/g, ""));
  return Number.isFinite(v) ? v : d;
};

/** The number at the start of a value, ignoring its unit suffix (`36mW_cm2` → 36). `num()` strips
 *  every non-digit and would read the 2 of `cm2` as part of the number. */
const leadingNumber = (s: string | undefined): number => {
  const v = s === undefined ? NaN : parseFloat(s);
  return Number.isFinite(v) ? v : 0;
};

function loadZones(): Map<string, number[]> {
  const text = readFileSync(join(DIR, "00-zones.npps"), "utf8");
  const zones = new Map<string, number[]>();
  for (const m of text.matchAll(/zone\s+"([^"]+)"\s*\{([\s\S]*?)\n\}/g)) {
    const s = /sockets:\s*\[([^\]]*)\]/.exec(m[2]);
    if (s) zones.set(m[1], [...s[1].matchAll(/\d+/g)].map((d) => Number(d[0])));
  }
  return zones;
}

export function analyse(): Row[] {
  const zones = loadZones();
  const all = zones.get("All") ?? [];
  const rows: Row[] = [];

  for (const file of readdirSync(DIR).sort()) {
    if (!file.endsWith(".npps") || file.startsWith("00-")) continue;
    const text = readFileSync(join(DIR, file), "utf8");
    const nameM = /protocol\s+"([^"]+)"/.exec(text);
    // One block per wavelength (NP-NPPS-REF-001 §4.1a): read ALL of them. Reading
    // only the first would count a 660 nm block and miss its 808 nm partner.
    const blocks = [...text.matchAll(/pbm_transcranial\s*\{([\s\S]*?)\n {4}\}/g)].map((m) => m[1].replace(/^\s*#.*$/gm, ""));   // comments may quote field names
    if (!nameM || blocks.length === 0) continue;
    const notes: string[] = [];

    // Electrical draw of ONE channel at full drive: the dual tile's 25 W is two
    // channels, so half each; CH_C is the 1064-only tile's 6.3 W.
    const chanW = (ch: string): number =>
      ch === "led_1064" ? TILE_W["1064nm"] : TILE_W["660_808nm"] / 2;

    const perSocketW = new Map<number, number>();
    let unknownSockets = 0;
    let anyClinician = false;
    const wls: string[] = [];
    const irrs: string[] = [];
    let cw = false;
    let duty: number | null = null;
    for (const body of blocks) {
      const wl = (field(body, "wavelength") ?? "").replace(/"/g, "");
      const w = parsePbmWavelength(wl);
      const ch = w.kind === "single" ? mapWavelength(w.nm, DEFAULT_WAVELENGTH_RULES) : null;
      const irr = leadingNumber(field(body, "irradiance"));
      wls.push(wl); irrs.push(`${irr}`);
      if (ch === null) { notes.push(`${wl || "no wavelength"}: no channel delivers it — not counted`); continue; }
      const frac = Math.min(1, irr / PBM_FULL_SCALE_MW_CM2[ch]);
      const freqHz = num(field(body, "frequency"), 0);
      const dutyRaw = field(body, "duty_cycle");
      const d = dutyRaw === undefined ? null : num(dutyRaw, 100);
      const blockCw = freqHz === 0;
      cw = cw || blockCw;
      if (d !== null) duty = d;
      if (blockCw && d !== null) {
        // NP-NPPS-REF-001 §4.1: `frequency: 0` selects CW, and CW means 100 % duty.
        // The compiler emits freq_code 0x00 and the duty register independently
        // (hubCompiler.ts freqCode/dutyReg), so which one wins is unspecified.
        // Reported at the CW reading — the higher draw — and flagged. OI-SESPWR-03.
        if (!notes.includes("CW+duty ambiguous")) notes.push("CW+duty ambiguous");
      }
      const blockW = chanW(ch) * frac * (blockCw ? 1 : (d ?? 100) / 100);

      const zoneSpec = field(body, "zones") ?? "all";
      let socketList: number[];
      if (zoneSpec.startsWith("[")) {
        const names = [...zoneSpec.matchAll(/"([^"]+)"/g)].map((m) => m[1]);
        const missing = names.filter((n) => !zones.has(n));
        if (missing.length) notes.push(`unknown zone: ${missing.join(", ")}`);
        socketList = names.flatMap((n) => zones.get(n) ?? []);
      } else if (zoneSpec.includes("clinician")) {
        anyClinician = true; socketList = [];
        if (!notes.includes("operator-selected — budget depends on the selection")) {
          notes.push("operator-selected — budget depends on the selection");
        }
      } else {
        socketList = all;
      }
      for (const sk of new Set(socketList)) perSocketW.set(sk, (perSocketW.get(sk) ?? 0) + blockW);
      if (anyClinician) unknownSockets++;
    }

    const sockets: number | null = anyClinician ? null : perSocketW.size;
    const zoneLabel = anyClinician ? "clinician_selected" : `${blocks.length} block(s)`;
    const wavelength = [...new Set(wls)].join("+");
    const irradiance = irrs.join("/");
    // The hottest tile sets the concurrency; the library is summed per socket.
    const perTileW = anyClinician
      ? chanW("led_808") * Math.min(1, leadingNumber(field(blocks[0], "irradiance")) / PBM_FULL_SCALE_MW_CM2.led_808) * (cw ? 1 : (duty ?? 100) / 100)
      : Math.max(0, ...perSocketW.values());
    const requiredWsum = [...perSocketW.values()].reduce((x, y) => x + y, 0);
    // Nothing deliverable (every wavelength unmapped): there is no draw to audit.
    if (!anyClinician && perSocketW.size === 0) {
      console.error(`check-pbm-power: ${nameM[1]}: no block is deliverable (${notes.join("; ")}) — skipped`);
      continue;
    }
    const maxConcurrent = Math.max(1, Math.floor(AVAILABLE_W / perTileW));
    const durM = /\n {4}duration:\s*(\d+)([ms])/.exec(text);
    const durationS = durM ? Number(durM[1]) * (durM[2] === "m" ? 60 : 1) : null;

    rows.push({
      file, name: nameM[1], sockets, wavelength, irradiance, cw, duty, perTileW,
      requiredW: sockets === null ? null : requiredWsum,
      maxConcurrent,
      groups: sockets === null ? null : Math.ceil((sockets || 0) / maxConcurrent),
      durationS, zoneLabel, notes,
    });
  }
  return rows.sort((a, b) => (b.requiredW ?? 0) - (a.requiredW ?? 0));
}

const fmtDur = (s: number | null): string =>
  s === null ? "—" : s < 5400 ? `${Math.round(s / 60)}m` : `${(s / 3600).toFixed(1)}h`;

if (import.meta.main) {
const rows = analyse();
const over = rows.filter((r) => r.requiredW !== null && r.requiredW > AVAILABLE_W);

console.log(`PBM transcranial power audit — ${AVAILABLE_W} W available to emitters`);
console.log(`(R-10 envelope 45–50 W less NP-HW-HEXTILE-001 §9.1 overhead ~6–8 W)\n`);
console.log(
  "protocol".padEnd(38) + "skt".padStart(4) + "W/tile".padStart(8) +
  "needs".padStart(8) + "fits".padStart(6) + "grps".padStart(6) +
  "authored".padStart(10) + "cascaded".padStart(10),
);
console.log("-".repeat(90));
for (const r of rows) {
  const casc = r.durationS !== null && r.groups !== null ? r.durationS * r.groups : null;
  console.log(
    r.name.slice(0, 38).padEnd(38) +
    (r.sockets ?? "—").toString().padStart(4) +
    r.perTileW.toFixed(1).padStart(8) +
    (r.requiredW === null ? "—" : Math.round(r.requiredW).toString()).padStart(8) +
    r.maxConcurrent.toString().padStart(6) +
    (r.groups ?? "—").toString().padStart(6) +
    fmtDur(r.durationS).padStart(10) +
    fmtDur(casc).padStart(10),
  );
  for (const n of r.notes) console.log(`      ⚠ ${n}`);
}

const perTile = rows.map((r) => r.perTileW);
const indeterminate = rows.filter((r) => r.requiredW === null);
const within = rows.length - over.length - indeterminate.length;
console.log(
  `\n${rows.length} protocols with pbm_transcranial · ` +
  `${within} within budget · ${over.length} over · ` +
  `${indeterminate.length} indeterminate (operator-selected sockets)`,
);
console.log(
  `per-tile draw spans ${Math.min(...perTile).toFixed(1)}–${Math.max(...perTile).toFixed(1)} W, ` +
  `so max concurrent tiles spans ${Math.min(...rows.map((r) => r.maxConcurrent))}–` +
  `${Math.max(...rows.map((r) => r.maxConcurrent))} — not a constant. ` +
  `A tile-count governor cannot express this (NP-PWR-BUDGET-001 D-4).`,
);

if (process.argv.includes("--strict") && over.length) {
  console.error(`\nFAIL: ${over.length} protocol(s) exceed the PBM power envelope.`);
  process.exit(1);
}
}
