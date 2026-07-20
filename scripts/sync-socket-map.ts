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

import { readFileSync, writeFileSync, mkdirSync } from "fs";
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

if (checkOnly && stale > 0) {
  console.error(`\n${stale} generated file(s) out of date — run: bun scripts/sync-socket-map.ts`);
  process.exit(1);
}

console.log(
  `${sockets.length} sockets — all 8 lobe zones reproduced exactly from 00-zones.npps.`,
);
