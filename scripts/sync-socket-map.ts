#!/usr/bin/env bun
/**
 * sync-socket-map.ts — Canonical helmet socket geometry → shared JSON + typed tables.
 *
 * Source of truth: the ROW STRUCTURE below, validated against
 * protocols/predefined/00-zones.npps on every run.
 * Targets:
 *   - Shared:  hardware/np_socket_map.json   (all platforms read or embed this)
 *   - Web:     app/web/src/lib/socketMap.generated.ts
 *
 * ── Why a row structure and not a hand-written table ─────────────────────────
 *
 * The 78 socket ids are not arbitrary: they are assigned front-to-back in rows
 * of a hexagonally close-packed dome, left-to-right within each row. Widths run
 *
 *     frontal    1 2 3 4 5 4
 *     mid        5 4 5 4 5 4 5 4 5      (alternating, hex offset packing)
 *     occipital  4 5 4 3 2
 *
 * summing to exactly 78. Within a row the leftmost sockets are left-hemisphere,
 * the rightmost are right-hemisphere, and an odd-width row's exact centre socket
 * is MIDLINE — it belongs to BOTH hemispheres, which is what produces the ten
 * duplicated sockets in 00-zones.npps (frontal 1/5/13, parietal 22/31/40/49/58,
 * occipital 67/75). In the 5-wide mid rows the two outermost sockets are
 * temporal and the inner three are parietal; the 4-wide mid rows are all
 * parietal.
 *
 * Deriving from this structure rather than transcribing a table means the
 * geometry is checkable: `validateAgainstZoneFile` regenerates all eight lobe
 * zones from the derived lobe/side assignment and fails the build if they do not
 * match 00-zones.npps byte for byte. That check passes today, which is the
 * evidence the row structure is the real addressing scheme and not a guess.
 *
 * ── Coordinate status ────────────────────────────────────────────────────────
 *
 * PROVISIONAL. x/y are unit-hex lattice coordinates for laying out the picker,
 * NOT millimetre positions on the shell. They are correct in TOPOLOGY (which
 * socket neighbours which, which row, which side) because that is derived from
 * the validated structure — but the absolute scale and dome curvature must be
 * replaced from the shell CAD before any physical claim is made. Nothing in the
 * eligibility logic depends on x/y; they only place hexes in the picker.
 *
 * Run:  bun scripts/sync-socket-map.ts   (--check to verify freshness in CI)
 */

import { readFileSync, writeFileSync, mkdirSync, readdirSync } from "fs";
import { join, dirname } from "path";

const ROOT = join(import.meta.dir, "..");
const ZONE_FILE = join(ROOT, "protocols", "predefined", "00-zones.npps");
const JSON_OUT = join(ROOT, "hardware", "np_socket_map.json");
const TS_OUT = join(ROOT, "app", "web", "src", "lib", "socketMap.generated.ts");

type Lobe = "frontal" | "temporal" | "parietal" | "occipital";
type Side = "left" | "right" | "midline";
type Band = "frontal" | "mid" | "occipital";

/** Row widths, front to back. Sums to 78. */
const ROWS: Array<{ band: Band; width: number }> = [
  { band: "frontal", width: 1 },
  { band: "frontal", width: 2 },
  { band: "frontal", width: 3 },
  { band: "frontal", width: 4 },
  { band: "frontal", width: 5 },
  { band: "frontal", width: 4 },
  { band: "mid", width: 5 },
  { band: "mid", width: 4 },
  { band: "mid", width: 5 },
  { band: "mid", width: 4 },
  { band: "mid", width: 5 },
  { band: "mid", width: 4 },
  { band: "mid", width: 5 },
  { band: "mid", width: 4 },
  { band: "mid", width: 5 },
  { band: "occipital", width: 4 },
  { band: "occipital", width: 5 },
  { band: "occipital", width: 4 },
  { band: "occipital", width: 3 },
  { band: "occipital", width: 2 },
];

export interface SocketGeometry {
  /** 1-based socket id, matching the `sockets:` lists in .npps zone files. */
  id: number;
  lobe: Lobe;
  /** `midline` sockets belong to BOTH hemisphere zones of their lobe. */
  side: Side;
  row: number;
  col: number;
  /** Provisional unit-hex lattice coordinates — see file header. */
  x: number;
  y: number;
}

/** Vertical spacing of a unit hex lattice: sqrt(3)/2. */
const HEX_ROW_PITCH = 0.8660254037844386;

function buildSockets(): SocketGeometry[] {
  const sockets: SocketGeometry[] = [];
  let id = 1;

  ROWS.forEach((row, r) => {
    const { band, width } = row;
    for (let col = 0; col < width; col++) {
      const isCentre = width % 2 === 1 && col === (width - 1) / 2;
      const side: Side = isCentre ? "midline" : col < width / 2 ? "left" : "right";

      // In the 5-wide mid rows the outer pair is temporal and the inner three
      // are parietal; 4-wide mid rows are entirely parietal.
      const lobe: Lobe =
        band === "frontal" ? "frontal"
        : band === "occipital" ? "occipital"
        : width === 5 && (col === 0 || col === 4) ? "temporal"
        : "parietal";

      sockets.push({
        id,
        lobe,
        side,
        row: r,
        col,
        x: Number((col - (width - 1) / 2).toFixed(4)),
        y: Number((r * HEX_ROW_PITCH).toFixed(4)),
      });
      id++;
    }
  });

  return sockets;
}

// ─── Validation against the locked zone file ───────────────────────────────────

const LOBE_LABEL: Record<Lobe, string> = {
  frontal: "Frontal",
  temporal: "Temporal",
  parietal: "Parietal",
  occipital: "Occipital",
};

/** Regenerate the eight lobe zones from the derived geometry. */
function zonesFromGeometry(sockets: SocketGeometry[]): Map<string, number[]> {
  const zones = new Map<string, number[]>();
  const push = (name: string, id: number) => {
    const list = zones.get(name) ?? [];
    list.push(id);
    zones.set(name, list);
  };

  for (const s of sockets) {
    const lobe = LOBE_LABEL[s.lobe];
    if (s.side === "midline") {
      push(`${lobe} Left`, s.id);
      push(`${lobe} Right`, s.id);
    } else {
      push(`${lobe} ${s.side === "left" ? "Left" : "Right"}`, s.id);
    }
  }
  return zones;
}

function parseZoneFile(source: string): Map<string, number[]> {
  const zones = new Map<string, number[]>();
  const blockRe = /zone\s+"((?:[^"\\]|\\.)*)"\s*\{([\s\S]*?)\n\}/g;

  for (const block of source.matchAll(blockRe)) {
    const socketsMatch = /sockets:\s*\[([^\]]*)\]/.exec(block[2]);
    if (!socketsMatch) continue;
    const ids = [...socketsMatch[1].matchAll(/\d+/g)].map(m => Number(m[0]));
    zones.set(block[1], ids.sort((a, b) => a - b));
  }
  return zones;
}

/**
 * Fail loudly if the derived geometry no longer reproduces the shipped zone
 * definitions. This is the check that makes the row structure trustworthy: the
 * zone files are the locked artefact, and the geometry must explain them.
 */
function validateAgainstZoneFile(sockets: SocketGeometry[]): string[] {
  const errors: string[] = [];
  const actual = parseZoneFile(readFileSync(ZONE_FILE, "utf-8"));
  const derived = zonesFromGeometry(sockets);

  if (sockets.length !== 78) {
    errors.push(`expected 78 sockets, derived ${sockets.length}`);
  }

  // Only the eight lobe zones are derivable from geometry. The zone file also
  // carries aggregate zones (All / Frontal / Posterior — unions of lobe zones,
  // added as legacy-migration targets), which by construction have no
  // lobe+side derivation. Validate the lobe zones; verify aggregates are
  // unions of real sockets rather than silently skipping them.
  for (const [name, derivedIds] of derived) {
    const actualIds = actual.get(name);
    if (!actualIds) {
      errors.push(`derived lobe zone "${name}" is missing from the zone file`);
      continue;
    }
    const sorted = [...derivedIds].sort((a, b) => a - b);
    if (JSON.stringify(sorted) !== JSON.stringify(actualIds)) {
      errors.push(
        `zone "${name}" mismatch\n    derived: [${sorted}]\n    zone file: [${actualIds}]`,
      );
    }
  }

  const validIds = new Set(sockets.map(s => s.id));
  for (const [name, ids] of actual) {
    if (derived.has(name)) continue;
    const unknown = ids.filter(id => !validIds.has(id));
    if (unknown.length > 0) {
      errors.push(`aggregate zone "${name}" references non-existent sockets: [${unknown}]`);
    }
  }
  return errors;
}

// ─── Lateralized-protocol audit ────────────────────────────────────────────────
//
// Why this lives here: zone membership is INCLUSIVE by default (PR #210 ruling) — a
// midline socket belongs to BOTH hemisphere zones of its lobe, and the only way to
// narrow a zone is to author a narrower one. There is no runtime exclusion filter.
//
// That rule is free for a protocol that targets both hemispheres, a whole lobe, or the
// whole vault: the midline sockets belong there anyway. It has a real cost for a
// protocol that targets ONE hemisphere, because the midline modules push roughly half
// their cortical energy across the midline (50% exactly, by symmetry — NP-OPT-PSF-001 §4), which
// is a confound in exactly the protocols whose evidence base is lateralized.
//
// A survey on 2026-07-19 found exactly one such protocol among 72 files. That survey is
// a point-in-time grep nobody will remember to re-run, so it is encoded here instead:
// add a new single-hemisphere-zone protocol and this check fails until someone decides
// whether it needs a narrowed zone.
//
// ── The survey method this replaces ─────────────────────────────────────────────
//
//   1. Zone-using protocols:  grep -l "zones:" protocols/predefined/*.npps | grep -v 00-zones
//   2. For each, resolve its zone names against 00-zones.npps and check whether any zone
//      is a single hemisphere ("* Left" or "* Right" alone). A bilateral pair, a
//      whole-lobe zone, or a whole-region zone (All, Frontal, Posterior, Vault (excl.
//      Occipital), Motor / SMA) is not boundary-sensitive.
//   3. Any NEW single-hemisphere zone protocol re-opens the partial-module question.
//
// ── Modalities deliberately NOT checked ─────────────────────────────────────────
//
//   tDCS   — targets 10-20 `electrode_pairs` labels, not sockets. NOTE: no 10-20 →
//            socket mapping exists in the repo yet. When it is built, a named electrode
//            resolves to ONE element on ONE module, which the existing (socket:element)
//            address already expresses — so it introduces no new zone-boundary question.
//   TMS    — `target: DLPFC_L` etc., positioned by its own focal figure-8 coil.
//   tACS / VNS / audio / visual — carry no socket-zone keys at all.
//
// Only PBM addresses sockets by zone, so only PBM is audited.

/** Zone names that are a single hemisphere: "<Lobe> Left" or "<Lobe> Right", nothing else. */
const SINGLE_HEMISPHERE_RE = /^(Frontal|Temporal|Parietal|Occipital) (Left|Right)$/;

/**
 * Legal non-list values for `zones:`. Anything else is a typo and must fail the audit
 * rather than being waved through — an unrecognised scalar previously passed silently,
 * which is the exact failure mode this check exists to prevent.
 * Mirrors the selector handling in app/web/src/lib/protocolEligibility.ts.
 */
const LEGAL_ZONE_TOKENS = new Set([
  "named", "all", "front", "rear", "custom", "clinician_selected",
]);

/**
 * Protocols already reviewed and accepted as single-hemisphere. Keyed by protocol file
 * basename → the reason it is allowed to stay that way. Adding an entry here is a
 * design decision and should carry a doc reference.
 *
 * Note the granularity: accepting a file suppresses the check for EVERY zone list in it.
 * Keep protocols single-purpose so that stays a safe trade.
 */
const ACCEPTED_LATERALIZED: Record<string, string> = {
  // Intentionally empty. clinical-03 was the only member and was narrowed to
  // "Frontal Right (excl. midline)" on 2026-07-20 — see docs/np_opt_psf_001.md §4 and
  // docs/status/completed-decisions.md.
};

interface LateralizedHit {
  file: string;
  zones: string[];
}

/** Strip `#`-to-end-of-line comments. */
function stripComments(src: string): string {
  return src.replace(/#[^\n]*/g, "");
}

/**
 * Extract the bodies of PBM blocks only. Other modality blocks do not address sockets by
 * zone, and matching the whole file would let a `description:` string or a commented-out
 * example trip the gate.
 */
function pbmBlocks(src: string): string[] {
  const out: string[] = [];
  const re = /pbm_\w+\s*\{/g;
  for (const m of src.matchAll(re)) {
    let depth = 1;
    let i = m.index! + m[0].length;
    const from = i;
    while (i < src.length && depth > 0) {
      if (src[i] === "{") depth++;
      else if (src[i] === "}") depth--;
      i++;
    }
    out.push(src.slice(from, i - 1));
  }
  return out;
}

/**
 * Find protocols whose PBM blocks name a bare single-hemisphere zone.
 *
 * Fails CLOSED: every `zones:` key is located first and then classified, so a value shape
 * this function does not recognise is reported as unresolvable rather than producing no
 * match and silently passing.
 *
 * Hemisphere pairing is evaluated across ALL PBM blocks in a file, not per block, because
 * a protocol may legitimately drive each hemisphere from its own block (e.g. different
 * intensity per side) and that is still bilateral. It also matches the file-level
 * granularity of ACCEPTED_LATERALIZED.
 */
function auditLateralizedProtocols(
  dir: string,
): { hits: LateralizedHit[]; unresolved: LateralizedHit[] } {
  const known = parseZoneFile(readFileSync(ZONE_FILE, "utf-8"));
  const hits: LateralizedHit[] = [];
  const unresolved: LateralizedHit[] = [];

  for (const name of readdirSync(dir).sort()) {
    if (!name.endsWith(".npps") || name.startsWith("00-zones")) continue;
    const src = stripComments(readFileSync(join(dir, name), "utf-8"));

    const namesInFile: string[] = [];
    const badInFile: string[] = [];

    for (const block of pbmBlocks(src)) {
      for (const m of block.matchAll(/zones:\s*([^\n]+)/g)) {
        const raw = m[1].trim();

        if (raw.startsWith("[")) {
          const closed = /\[([^\]]*)\]/.exec(raw.includes("]") ? raw : block.slice(m.index!));
          if (!closed) { badInFile.push(raw); continue; }
          const zoneNames = [...closed[1].matchAll(/"((?:[^"\\]|\\.)*)"/g)].map(z => z[1]);
          if (zoneNames.length === 0) { badInFile.push(raw); continue; }
          for (const z of zoneNames) {
            if (!known.has(z)) badInFile.push(z);
            else namesInFile.push(z);
          }
          continue;
        }

        // Bare token: `zones: clinician_selected` and friends. The montage is chosen
        // elsewhere, so there is nothing to laterality-check — but the token must be one
        // we actually recognise.
        if (/^[A-Za-z_][A-Za-z0-9_]*$/.test(raw)) {
          if (!LEGAL_ZONE_TOKENS.has(raw)) badInFile.push(raw);
          continue;
        }

        // Anything else (quoted scalar, number, unrecognised shape) fails closed.
        badInFile.push(raw);
      }
    }

    if (badInFile.length > 0) unresolved.push({ file: name, zones: [...new Set(badInFile)] });

    const lateral = namesInFile.filter(z => SINGLE_HEMISPHERE_RE.test(z));
    const lobes = new Set(lateral.map(z => z.replace(/ (Left|Right)$/, "")));
    const unpaired = [...lobes].filter(
      lobe => !(namesInFile.includes(`${lobe} Left`) && namesInFile.includes(`${lobe} Right`)),
    );
    if (unpaired.length > 0) {
      hits.push({
        file: name,
        zones: [...new Set(lateral.filter(z => unpaired.includes(z.replace(/ (Left|Right)$/, ""))))],
      });
    }
  }
  return { hits, unresolved };
}

// ─── Emit ──────────────────────────────────────────────────────────────────────

const BANNER =
  `// Generated by scripts/sync-socket-map.ts — DO NOT EDIT BY HAND.\n` +
  `// Regenerate with: bun scripts/sync-socket-map.ts\n` +
  `// Geometry is validated on every run against protocols/predefined/00-zones.npps.\n` +
  `// x/y are PROVISIONAL unit-hex lattice coordinates for layout only, not mm.\n`;

function emitJson(sockets: SocketGeometry[]): string {
  return JSON.stringify(
    {
      _generator: "scripts/sync-socket-map.ts",
      _note:
        "Socket geometry for the NeurOne helmet. Validated against " +
        "protocols/predefined/00-zones.npps. x/y are provisional unit-hex lattice " +
        "coordinates for UI layout only — replace from shell CAD before any physical claim.",
      socketCount: sockets.length,
      /** Socket ids are 1-based, matching .npps zone `sockets:` lists. */
      numberingBase: 1,
      rowWidths: ROWS.map(r => r.width),
      sockets,
    },
    null,
    2,
  ) + "\n";
}

function emitTypeScript(sockets: SocketGeometry[]): string {
  const rows = sockets
    .map(s => `  { id: ${s.id}, lobe: '${s.lobe}', side: '${s.side}', row: ${s.row}, col: ${s.col}, x: ${s.x}, y: ${s.y} },`)
    .join("\n");

  return `${BANNER}
export type NPLobe = 'frontal' | 'temporal' | 'parietal' | 'occipital';
export type NPSide = 'left' | 'right' | 'midline';

export interface NPSocketGeometry {
  /** 1-based socket id, matching the \`sockets:\` lists in .npps zone files. */
  id: number;
  lobe: NPLobe;
  /** \`midline\` sockets belong to BOTH hemisphere zones of their lobe. */
  side: NPSide;
  row: number;
  col: number;
  /** Provisional unit-hex lattice coordinates — layout only, not millimetres. */
  x: number;
  y: number;
}

/** Every socket in the helmet, ordered by id. */
export const NP_SOCKETS: readonly NPSocketGeometry[] = [
${rows}
];

export const NP_SOCKET_COUNT = ${sockets.length};

/** Row widths front to back; used by the picker to lay out hex rows. */
export const NP_ROW_WIDTHS: readonly number[] = [${ROWS.map(r => r.width).join(", ")}];

const BY_ID = new Map<number, NPSocketGeometry>(NP_SOCKETS.map(s => [s.id, s]));

export function socketById(id: number): NPSocketGeometry | undefined {
  return BY_ID.get(id);
}

/** True when \`id\` names a real socket on this helmet. */
export function isValidSocketId(id: number): boolean {
  return BY_ID.has(id);
}
`;
}

// ─── Main ──────────────────────────────────────────────────────────────────────

const checkOnly = process.argv.includes("--check");

/** Value of a `--flag <value>` argument, or undefined. */
function parseArg(flag: string): string | undefined {
  const i = process.argv.indexOf(flag);
  return i >= 0 ? process.argv[i + 1] : undefined;
}

const sockets = buildSockets();
const errors = validateAgainstZoneFile(sockets);

if (errors.length > 0) {
  console.error("Socket geometry does not reproduce the shipped zone definitions:\n");
  for (const e of errors) console.error(`  - ${e}`);
  console.error(
    "\nEither the row structure in this script or 00-zones.npps is wrong. " +
    "The zone file is the locked artefact — fix the row structure to match it.",
  );
  process.exit(1);
}

const outputs: Array<[string, string]> = [
  [JSON_OUT, emitJson(sockets)],
  [TS_OUT, emitTypeScript(sockets)],
];

let stale = 0;
for (const [path, content] of outputs) {
  let existing: string | null = null;
  try {
    existing = readFileSync(path, "utf-8");
  } catch {
    existing = null;
  }
  if (existing === content) continue;

  if (checkOnly) {
    console.error(`STALE: ${path}`);
    stale++;
  } else {
    mkdirSync(dirname(path), { recursive: true });
    writeFileSync(path, content, "utf-8");
    console.log(`wrote ${path}`);
  }
}

// ── Protocol audit ────────────────────────────────────────────────────────────
//
// Deliberately AFTER the emit. The generated socket map derives only from ROWS and
// 00-zones.npps; protocols are downstream consumers of it. Running the audit first would
// mean that adding one lateralized protocol makes the socket map unregenerable until a
// clinical design question is settled — two unrelated artefacts coupled for no reason.
// Failures from both stages are collected and reported together below.
//
// `--audit-dir <path>` points the audit at fixture protocols for testing. It suppresses
// the normal pass line and is refused outside --check, so it can never masquerade as a
// real green run. It is a CLI flag rather than an env var precisely so an ambient value
// in a shell or CI runner cannot silently turn the gate into a no-op.
const auditDirOverride = parseArg("--audit-dir");
if (auditDirOverride && !checkOnly) {
  console.error("--audit-dir is a testing flag and may only be used with --check.");
  process.exit(2);
}
const auditDir = auditDirOverride ?? join(ROOT, "protocols", "predefined");

let protocolFiles: string[];
try {
  protocolFiles = readdirSync(auditDir).filter(f => f.endsWith(".npps"));
} catch {
  console.error(`Protocol directory not readable: ${auditDir}`);
  process.exit(2);
}
if (protocolFiles.length === 0) {
  console.error(`No .npps protocols found in ${auditDir} — the audit would pass vacuously.`);
  process.exit(2);
}

const { hits: lateralized, unresolved } = auditLateralizedProtocols(auditDir);
let auditFailed = false;

if (unresolved.length > 0) {
  auditFailed = true;
  console.error("\nProtocols reference zone names or selectors that are not recognised:\n");
  for (const u of unresolved) console.error(`  - ${u.file}: [${u.zones.join(", ")}]`);
  console.error(
    "\nEvery zone name must exist in 00-zones.npps, and every bare selector must be one of: " +
    `${[...LEGAL_ZONE_TOKENS].join(", ")}.`,
  );
}

const unreviewed = lateralized.filter(h => !(h.file in ACCEPTED_LATERALIZED));
if (unreviewed.length > 0) {
  auditFailed = true;
  console.error(
    "\nSingle-hemisphere-zone protocol(s) found that have not been reviewed for midline spill:\n",
  );
  for (const h of unreviewed) console.error(`  - ${h.file}: [${h.zones.join(", ")}]`);
  console.error(
    "\nZone membership is inclusive (PR #210): the midline sockets of a lobe belong to BOTH\n" +
    "its Left and Right zones, so a single-hemisphere zone leaks a share of its cortical\n" +
    "energy to the other hemisphere. The size of that share depends on how many of the\n" +
    "zone's sockets are midline — for Frontal Right it is 16.3%, and each midline module\n" +
    "contributes exactly 50% of its own output (docs/np_opt_psf_001.md §4). Decide one of:\n" +
    "  (a) accept the spill  — add the file to ACCEPTED_LATERALIZED with a reason;\n" +
    "  (b) narrow the zone   — author an `(excl. midline)` variant in 00-zones.npps and\n" +
    "                          point the protocol at it, as clinical-03 does.\n" +
    "Do NOT reach for partial-module inclusion: it is deferred pending an owner ruling, and\n" +
    "NP-OPT-PSF-001 §4.1 shows it cannot reach zero spill anyway, while (b) can, for free.",
  );
}

if (checkOnly && stale > 0) {
  console.error(`\n${stale} generated file(s) out of date — run: bun scripts/sync-socket-map.ts`);
}

if (auditFailed || (checkOnly && stale > 0)) process.exit(1);

console.log(
  `${sockets.length} sockets — all 8 lobe zones reproduced exactly from 00-zones.npps.`,
);
if (auditDirOverride) {
  console.log(`lateralization audit: FIXTURE RUN against ${auditDirOverride} — not a real check.`);
} else {
  console.log(
    `lateralization audit: ${protocolFiles.length} protocols scanned, ` +
    `no unreviewed single-hemisphere zones` +
    `${Object.keys(ACCEPTED_LATERALIZED).length > 0 ? ` (${Object.keys(ACCEPTED_LATERALIZED).length} accepted)` : ""}.`,
  );
}
