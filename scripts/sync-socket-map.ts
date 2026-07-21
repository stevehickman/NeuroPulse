#!/usr/bin/env bun
/**
 * sync-socket-map.ts — Canonical helmet socket geometry → shared JSON + typed tables.
 *
 * Source of truth: the SCAN-MEASURED ROW STRUCTURE below, validated against
 * protocols/predefined/00-zones.npps on every run.
 * Targets:
 *   - Shared:  hardware/np_socket_map.json   (all platforms read or embed this)
 *   - Web:     app/web/src/lib/socketMap.generated.ts
 *
 * ── The lattice is MEASURED FROM A 3D SCAN, not idealized (2026-07-20) ────────
 *
 * PROVISIONAL — v1, scan-grounded. Pending gates REG-1 (10-20 registration vs
 * shell CAD) and ACT-1 (active-surface boundary). NOT locked/final.
 *
 * Earlier revisions DERIVED the lattice from an ellipsoid: a skull, then a
 * Neuronic LIGHT *exterior* envelope minus a GUESSED 3 mm wall. Two Scaniverse
 * LiDAR scans of a physical reference helmet replaced that idealization with
 * measurement (NP-HEX-ZM-001 §3.4):
 *
 *   1. The exterior scan confirmed the metric scale (footprint 272 x 234 mm vs
 *      published 273 x 226, ±1.5%) and falsified the published 207 mm height —
 *      that was packaging; the helmet is ~157 mm rim-to-crown.
 *   2. The interior scan captured the actual tiling surface (the clear inner
 *      window), single-sided, radius 1-4 mm inside the exterior (= the wall).
 *
 * Measured over-vertex geodesics on the real interior surface:
 *
 *   nasion->inion arc   465 mm   (was 404 mm on the envelope-minus-wall ellipsoid)
 *   ear-to-ear peak arc  419 mm   (was 367 mm)
 *   rim->crown height    157 mm
 *   vault surface area  ~1370 cm² (full mesh; was 939 cm²)
 *
 * The real surface is ~15% larger in every arc than the old ellipsoid, because
 * that ellipsoid shrank a published EXTERIOR by a guessed wall. Laying 40 mm
 * hexes on the measured surface gives ~80 sockets, not 54. The tiling surface is
 * the INNERMOST emitting-face (clear-window) surface: on a concave bowl, rigid
 * hex prisms presenting a continuous emitting field tessellate at their
 * scalp-facing faces, so module + clear-layer thickness set mechanical depth and
 * dose distance, NOT the socket count (verified: an outward offset increases area
 * ~2%/mm — thickness does not reduce the count on a concave interior).
 *
 * All the ellipsoid machinery — SHELL_WALL_MM, the cephalic-index/skull-axes
 * derivation, ellipsePerimeter, and the arc-from-envelope calculation — is
 * DELETED per §3.4. The row widths below are MEASURED, not computed from an
 * ellipsoid; the arc lengths, rim height, module width and row pitch are the
 * scan constants that replaced the derivation.
 *
 * ── The v1 row structure (measured) ──────────────────────────────────────────
 *
 * Twelve coronal rows front->back, widths MEASURED off the scan:
 *
 *     3 6 7 8 9 8 9 8 7 6 5 4   (sum 80)
 *
 * Row r sits at arc fraction arcFrac(r) = 0.08 + (r/11)*0.86 of the
 * nasion->inion arc — the 10-20 system's own longitudinal ruler.
 *
 * ── Socket lattice ≠ active surface (architecture, §3.4) ─────────────────────
 *
 * The physical socket lattice is a FULL regular parity-alternating tessellation
 * over the scalp vault (~80 sockets). The ACTIVE surface is software-defined and
 * enforced per-element through the existing two-level (socket:element)
 * addressing — a boundary tile keeps its socket and the firmware disables the
 * low-order element_ids outside the chosen surface. That descriptor and its
 * firmware element-mask API are separate work (ACT-2) and are NOT emitted here;
 * this generator produces only the physical 80-socket lattice.
 *
 * ── A 40 mm tile cannot resolve adjacent 10-20 lines (gate REG-1) ────────────
 *
 * The row pitch is 34.6 mm and the 10-20 lines are ~10% of the nasion->inion arc
 * apart (~47 mm on this 465 mm surface, so the 34.6 mm rows over-sample the
 * longitudinal ruler while a single 40 mm tile still cannot split two adjacent
 * lateral 10-20 columns). The socket-to-10-20 registration is the open REG-1
 * gate and must be confirmed against shell CAD before any clinical placement
 * claim; the row/lobe boundaries here are the v1 proposal, not a final map.
 *
 * ── Why a derived structure and not a hand-written table ─────────────────────
 *
 * Socket ids run front-to-back by row, left-to-right within each row. Within a
 * row the leftmost sockets are left-hemisphere, the rightmost are
 * right-hemisphere, and an odd-width row's exact centre socket is MIDLINE -- it
 * belongs to BOTH hemispheres, which is what produces the duplicated sockets in
 * 00-zones.npps (2, 13, 29, 46, 62, 74).
 *
 * `validateAgainstZoneFile` regenerates all eight lobe zones AND every aggregate
 * union from the derived lobe/side assignment and fails the build if they do not
 * match 00-zones.npps. That check passing is the evidence the model is the real
 * addressing scheme and not a guess.
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

// ─── Scan-measured geometry constants (NP-HEX-ZM-001 §3.4) ────────────────────
//
// These REPLACE the ellipsoid-from-envelope constants. Every value here is
// measured from a Scaniverse LiDAR scan of the reference helmet interior, or is
// the locked module design point. PROVISIONAL pending REG-1 / ACT-1.

/** Module flat-to-flat width, mm. NP-HEX-ZM-001 §3 design point (ISA ISC-1). */
const MODULE_WIDTH_MM = 40;

/** Nasion->inion over-vertex arc on the measured interior surface, mm. */
const SAGITTAL_ARC_MM = 465;

/** Ear-to-ear over-vertex arc at the widest station, mm. */
const CORONAL_ARC_MM = 419;

/** Rim->crown height on the measured interior surface, mm (falsifies the 207 mm packaging figure). */
const RIM_TO_CROWN_MM = 157;

/** Measured front-to-back row spacing, mm. Coincides with the 40 mm offset-hex pitch (rowPitchMm() below). */
const ROW_PITCH_MM_MEASURED = 34.6;

/** Full-mesh vault surface area on the measured interior, cm² — the containment bound. */
const VAULT_SURFACE_CM2 = 1370;

/**
 * Clearance between the helmet interior and the largest skull the single adult
 * SKU covers (62 cm), mm. Carried from the interim model as a documented floor:
 * the scanned interior is LARGER than that ellipsoid in every arc, so the true
 * standoff is at least this. PROVISIONAL — confirm against shell CAD (SCAN-1).
 */
const STANDOFF_MM = 23;

/** Largest head circumference the single adult SKU covers, mm (CLAUDE.md §4.4). */
const HEAD_CIRCUMFERENCE_MM = 620;

/** Interior footprint, mm — exterior scan footprint (272 x 234) less the ~2.5 mm wall. */
const INTERIOR_LENGTH_MM = 267;
const INTERIOR_BREADTH_MM = 229;

// ── The measured row structure ────────────────────────────────────────────────
//
// Widths MEASURED off the scan, front->back. These are not computed from an
// ellipsoid: the interior surface was tiled with 40 mm hexes and the whole tiles
// counted per coronal band. Parity ALTERNATES o e o e o e o e o e o e — required
// (see the packing note below), and satisfied by construction here.
const ROW_WIDTHS: readonly number[] = [3, 6, 7, 8, 9, 8, 9, 8, 7, 6, 5, 4];

/** Position of row r along the nasion->inion arc (= the 10-20 coordinate). */
function arcFrac(r: number): number {
  return 0.08 + (r / 11) * 0.86;
}

// ── Lobe boundaries, in nasion->inion arc fraction (skull geography) ──────────
//
//   central sulcus       ~= the C line  = 0.50  -> frontal | parietal
//   parieto-occipital    ~= the PO line = 0.80  -> parietal | occipital
//
// Temporal is a LATERAL lobe, below the Sylvian fissure: the outermost socket of
// a row, only where the row is wide enough to reach the temporal line, and only
// within the lobe's own front-to-back extent (F7/T3 junction to T5/P5).
const CENTRAL_SULCUS_ARC = 0.5;
const PARIETO_OCCIPITAL_ARC = 0.8;
const TEMPORAL_ARC_RANGE: [number, number] = [0.35, 0.78];
const TEMPORAL_MIN_ROW_WIDTH = 7;

// ─── Derivations ──────────────────────────────────────────────────────────────

/** Area of a regular hexagon of flat-to-flat width `w` mm, in cm². */
function hexAreaCm2(w: number): number {
  return (Math.sqrt(3) / 2) * w * w / 100;
}

/**
 * Row pitch for offset-packed pointy-top hexes: height is 2w/sqrt(3)
 * vertex-to-vertex and rows advance by 3/4 of that (= 34.64 mm for a 40 mm
 * tile). The scan measured 34.6 mm (ROW_PITCH_MM_MEASURED) — the same pitch,
 * corroborating the offset packing on the real surface.
 *
 * NOTE this pitch is ONLY valid for offset rows. Two rows sharing a column
 * alignment need the full height, 2w/sqrt(3) -- stacking them at 0.75 of it
 * overlaps by a quarter of a tile. That is why the row widths alternate parity,
 * and why `checkPacking` exists to prove it did.
 */
function rowPitchMm(w = MODULE_WIDTH_MM): number {
  return 0.75 * (2 * w / Math.sqrt(3));
}

/** Area of the region the lattice actually occupies, cm² (row integration). */
function tiledAreaCm2(): number {
  return ROW_WIDTHS.reduce((sum, w) => sum + w * MODULE_WIDTH_MM * rowPitchMm(), 0) / 100;
}

/**
 * Prove the lattice tessellates: no two sockets closer than one tile width.
 *
 * In an offset hex lattice every neighbour sits at exactly 1.0 tile width, so
 * anything below that is an overlap. This is a direct geometric assertion on
 * the emitted coordinates rather than a restatement of the parity rule, so it
 * catches a bad rule as well as a bad row.
 */
function checkPacking(sockets: SocketGeometry[]): string[] {
  const errors: string[] = [];
  for (let i = 0; i < sockets.length; i++) {
    for (let j = i + 1; j < sockets.length; j++) {
      const dx = sockets[i]!.x - sockets[j]!.x;
      const dy = sockets[i]!.y - sockets[j]!.y;
      const d = Math.hypot(dx, dy);
      if (d < 0.999) {
        errors.push(
          `sockets ${sockets[i]!.id} and ${sockets[j]!.id} OVERLAP — centres ` +
          `${d.toFixed(3)} tile widths apart, minimum is 1.000. Row widths must ` +
          `alternate parity; widths are [${ROW_WIDTHS.join(", ")}].`,
        );
      }
    }
  }
  return errors;
}

/** Consecutive row widths must alternate parity or same-column tiles collide. */
function checkParity(): string[] {
  const errors: string[] = [];
  for (let r = 1; r < ROW_WIDTHS.length; r++) {
    if (ROW_WIDTHS[r]! % 2 === ROW_WIDTHS[r - 1]! % 2) {
      errors.push(
        `rows ${r - 1} and ${r} share width parity (${ROW_WIDTHS[r - 1]}, ${ROW_WIDTHS[r]}) — ` +
        `offset hex packing needs consecutive rows shifted half a tile, so the ` +
        `widths must alternate odd/even. Widths are [${ROW_WIDTHS.join(", ")}].`,
      );
    }
  }
  return errors;
}

/** Socket ids start at 1, matching the `sockets:` lists in .npps zone files. */
const NUMBERING_BASE = 1;

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
  let id = NUMBERING_BASE;

  ROW_WIDTHS.forEach((width, r) => {
    const f = arcFrac(r);

    // Temporal is a LATERAL lobe, not a longitudinal band: it sits below the
    // Sylvian fissure, so it can only be the outermost socket of a row, only
    // where the row is wide enough to reach the temporal line, and only within
    // the lobe's own front-to-back extent (F7/T3 junction to T5/P5).
    const temporalRow =
      width >= TEMPORAL_MIN_ROW_WIDTH &&
      f >= TEMPORAL_ARC_RANGE[0] &&
      f <= TEMPORAL_ARC_RANGE[1];

    for (let col = 0; col < width; col++) {
      const isCentre = width % 2 === 1 && col === (width - 1) / 2;
      const side: Side = isCentre ? "midline" : col < width / 2 ? "left" : "right";
      const outermost = col === 0 || col === width - 1;

      const lobe: Lobe =
        temporalRow && outermost ? "temporal"
        : f < CENTRAL_SULCUS_ARC ? "frontal"
        : f < PARIETO_OCCIPITAL_ARC ? "parietal"
        : "occipital";

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
 * Fail loudly if the row structure no longer matches the tile geometry, or no
 * longer reproduces the shipped zone definitions.
 *
 * Note the direction of authority: TILE GEOMETRY is the locked artefact, not the
 * zone file. `00-zones.npps` is a downstream expression of the lattice, so when
 * these disagree the lattice is right and the zone file is re-cut — the reverse
 * of the pre-2026-07-20 rule, which is how a physically impossible 78-socket
 * lattice survived review.
 */
function validateAgainstZoneFile(sockets: SocketGeometry[]): string[] {
  const errors: string[] = [];
  const actual = parseZoneFile(readFileSync(ZONE_FILE, "utf-8"));
  const derived = zonesFromGeometry(sockets);

  // Geometry must tessellate: the parity rule and the packing it produces are
  // both asserted directly (parity on the widths, overlap on the coordinates).
  errors.push(...checkParity());
  errors.push(...checkPacking(sockets));

  // The scan-measured row pitch must match the offset-hex packing pitch a 40 mm
  // tile requires — the corroboration that the measured surface really is tiled
  // by these hexes, not merely fitted to them.
  if (Math.abs(rowPitchMm() - ROW_PITCH_MM_MEASURED) > 0.1) {
    errors.push(
      `measured row pitch ${ROW_PITCH_MM_MEASURED} mm does not match the offset-hex ` +
      `pitch a ${MODULE_WIDTH_MM} mm tile requires (${rowPitchMm().toFixed(2)} mm) — ` +
      `the scan geometry and the module width disagree.`,
    );
  }

  // Containment check. The tiled region is a SUBSET of the helmet interior
  // vault, so its area cannot exceed it. tiledAreaCm2 is the row integration;
  // VAULT_SURFACE_CM2 is the full-mesh area measured off the scan — an
  // independent bound, not the same arithmetic that produced the rows.
  const tiled = tiledAreaCm2();
  if (tiled > VAULT_SURFACE_CM2) {
    errors.push(
      `the tiled region is ${tiled.toFixed(0)} cm² but the measured helmet ` +
      `interior vault is only ${VAULT_SURFACE_CM2} cm² — the lattice does not fit ` +
      `inside the shell. Check ROW_WIDTHS or the measured geometry constants.`,
    );
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
  const union = (...names: string[]) =>
    [...new Set(names.flatMap(n => derived.get(n) ?? []))].sort((a, b) => a - b);

  /**
   * Aggregate zones ARE derivable — each is a union of lobe zones — so derive
   * and diff them like the lobe zones. Checking only "do the referenced sockets
   * exist" is not enough: re-cut the lattice larger and a stale `All` would keep
   * listing a subset, every id would still resolve, the build would pass, and
   * the helmet would silently address part of itself. "All" means every socket.
   */
  const AGGREGATES: Record<string, string[]> = {
    "All": [
      "Frontal Left", "Frontal Right", "Temporal Left", "Temporal Right",
      "Parietal Left", "Parietal Right", "Occipital Left", "Occipital Right",
    ],
    "Frontal": ["Frontal Left", "Frontal Right"],
    "Posterior": [
      "Parietal Left", "Parietal Right", "Occipital Left", "Occipital Right",
    ],
    "Vault (excl. Occipital)": [
      "Frontal Left", "Frontal Right", "Temporal Left", "Temporal Right",
      "Parietal Left", "Parietal Right",
    ],
  };

  /**
   * Zones that are deliberately NOT unions of lobe zones. "Motor / SMA" is a
   * novel cross-lobe grouping (the precentral motor row minus its lateral
   * temporal sockets) and is PROVISIONAL pending REG-1, so it is exempt from the
   * union check — but it still has to reference real sockets, and it has to be
   * listed here rather than silently skipped.
   */
  const NON_UNION_ZONES = new Set(["Motor / SMA"]);

  for (const [name, expectedNames] of Object.entries(AGGREGATES)) {
    const ids = actual.get(name);
    if (!ids) {
      errors.push(`aggregate zone "${name}" is missing from the zone file`);
      continue;
    }
    const want = union(...expectedNames);
    if (JSON.stringify(ids) !== JSON.stringify(want)) {
      errors.push(
        `aggregate zone "${name}" is not the union of [${expectedNames.join(", ")}]\n` +
        `    derived:   [${want}]\n` +
        `    zone file: [${ids}]`,
      );
    }
  }

  // "All" additionally means EVERY socket — pin that independently of the union,
  // so a lobe zone that lost a socket cannot quietly shrink "All" with it.
  const all = actual.get("All");
  if (all && all.length !== sockets.length) {
    errors.push(
      `zone "All" lists ${all.length} sockets but the helmet has ${sockets.length}. ` +
      `"All" must mean every socket on the helmet and nothing less.`,
    );
  }

  for (const [name, ids] of actual) {
    if (derived.has(name)) continue;
    const unknown = ids.filter(id => !validIds.has(id));
    if (unknown.length > 0) {
      errors.push(`zone "${name}" references non-existent sockets: [${unknown}]`);
    }
    const dupes = ids.filter((id, i) => ids.indexOf(id) !== i);
    if (dupes.length > 0) {
      errors.push(`zone "${name}" lists duplicate sockets: [${[...new Set(dupes)]}] — a zone is a set`);
    }
    if (!AGGREGATES[name] && !NON_UNION_ZONES.has(name)) {
      errors.push(
        `zone "${name}" is neither a lobe zone, a known aggregate, nor listed in ` +
        `NON_UNION_ZONES. Add it to AGGREGATES (with the lobe zones it unions) ` +
        `or to NON_UNION_ZONES (if it is a deliberate novel grouping).`,
      );
    }
  }
  return errors;
}

// ─── Emit ──────────────────────────────────────────────────────────────────────

const BANNER =
  `// Generated by scripts/sync-socket-map.ts — DO NOT EDIT BY HAND.\n` +
  `// Regenerate with: bun scripts/sync-socket-map.ts\n` +
  `// Geometry is validated on every run against protocols/predefined/00-zones.npps.\n` +
  `// Scan-grounded v1 lattice (NP-HEX-ZM-001 §3.4) — PROVISIONAL pending REG-1 / ACT-1.\n` +
  `// x/y are PROVISIONAL unit-hex lattice coordinates for layout only, not mm.\n`;

/** The scan-measured geometry object shared by both emitted artifacts. */
function geometryFields() {
  return {
    moduleWidthMm: MODULE_WIDTH_MM,
    /** Nasion->inion over-vertex arc, mm — scan-measured. */
    sagittalArcMm: SAGITTAL_ARC_MM,
    /** Ear-to-ear over-vertex arc at the widest station, mm — scan-measured. */
    coronalArcMm: CORONAL_ARC_MM,
    /** Rim->crown height, mm — scan-measured (falsifies the 207 mm packaging figure). */
    rimToCrownMm: RIM_TO_CROWN_MM,
    /** Front-to-back row spacing, mm — the offset-hex pitch, = the measured 34.6 mm. */
    rowPitchMm: Number(rowPitchMm().toFixed(2)),
    hexAreaCm2: Number(hexAreaCm2(MODULE_WIDTH_MM).toFixed(4)),
    tiledAreaCm2: Number(tiledAreaCm2().toFixed(1)),
    /** Full-mesh interior vault area, cm² — scan-measured containment bound. */
    vaultSurfaceCm2: VAULT_SURFACE_CM2,
    /** Clearance to the largest skull the SKU covers, mm — provisional floor. */
    standoffMm: STANDOFF_MM,
    headCircumferenceMm: HEAD_CIRCUMFERENCE_MM,
    interiorLengthMm: INTERIOR_LENGTH_MM,
    interiorBreadthMm: INTERIOR_BREADTH_MM,
  };
}

function emitJson(sockets: SocketGeometry[]): string {
  return JSON.stringify(
    {
      _generator: "scripts/sync-socket-map.ts",
      _note:
        "Socket geometry for the NeurOne helmet. Scan-grounded v1 lattice " +
        "(NP-HEX-ZM-001 §3.4), PROVISIONAL pending gates REG-1 and ACT-1. Validated " +
        "against protocols/predefined/00-zones.npps. x/y are provisional unit-hex " +
        "lattice coordinates for UI layout only — replace from shell CAD before any " +
        "physical claim.",
      _basis: "scan-measured",
      socketCount: sockets.length,
      /** Socket ids are 1-based, matching .npps zone `sockets:` lists. */
      numberingBase: NUMBERING_BASE,
      /** Inclusive id bounds. Parsers reject `sockets:` entries outside these. */
      socketIdMin: sockets[0]!.id,
      socketIdMax: sockets[sockets.length - 1]!.id,
      /** The measured geometry the lattice sits on — see the script header. */
      geometry: geometryFields(),
      rowWidths: [...ROW_WIDTHS],
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
  const g = geometryFields();

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

/**
 * Socket ids are 1-based. This is the numbering base every consumer must honour
 * — .npps \`sockets:\` lists, the firmware major address, and the UI picker.
 */
export const NP_SOCKET_NUMBERING_BASE = ${NUMBERING_BASE};

/** Inclusive lowest valid socket id. */
export const NP_SOCKET_ID_MIN = ${sockets[0]!.id};

/** Inclusive highest valid socket id. */
export const NP_SOCKET_ID_MAX = ${sockets[sockets.length - 1]!.id};

/**
 * SCAN-MEASURED geometry the lattice sits on (NP-HEX-ZM-001 §3.4). Two Scaniverse
 * LiDAR scans of the reference helmet interior replaced the earlier
 * ellipsoid-from-envelope idealization: the arcs, rim height and vault area are
 * measured off the real interior surface, and the row widths are counted whole
 * tiles on it. PROVISIONAL — pending REG-1 (10-20 registration vs shell CAD) and
 * ACT-1 (active-surface boundary). Rows sit at rowPitchMm along sagittalArcMm;
 * widths must alternate parity or the tiles overlap.
 */
export const NP_TILE_GEOMETRY = {
  moduleWidthMm: ${g.moduleWidthMm},
  /** Nasion->inion over-vertex arc, mm — scan-measured. */
  sagittalArcMm: ${g.sagittalArcMm},
  /** Ear-to-ear over-vertex arc at the widest station, mm — scan-measured. */
  coronalArcMm: ${g.coronalArcMm},
  /** Rim->crown height, mm — scan-measured. */
  rimToCrownMm: ${g.rimToCrownMm},
  /** Front-to-back row spacing, mm — offset-hex pitch, = the measured 34.6 mm. */
  rowPitchMm: ${g.rowPitchMm},
  hexAreaCm2: ${g.hexAreaCm2},
  tiledAreaCm2: ${g.tiledAreaCm2},
  /** Full-mesh interior vault area, cm² — scan-measured containment bound. */
  vaultSurfaceCm2: ${g.vaultSurfaceCm2},
  /** Clearance between the helmet interior and the largest skull, mm (provisional floor). */
  standoffMm: ${g.standoffMm},
  /** Largest head circumference the single adult SKU covers, mm. */
  headCircumferenceMm: ${g.headCircumferenceMm},
  interiorLengthMm: ${g.interiorLengthMm},
  interiorBreadthMm: ${g.interiorBreadthMm},
} as const;

/** Row widths front to back; used by the picker to lay out hex rows. */
export const NP_ROW_WIDTHS: readonly number[] = [${ROW_WIDTHS.join(", ")}];

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
    "\nEither the row structure in this script or 00-zones.npps is wrong.\n" +
    "TILE GEOMETRY IS THE LOCKED ARTEFACT: the row structure is the scan-measured " +
    "ROW_WIDTHS on the interior surface (NP-HEX-ZM-001 §3.4), and 00-zones.npps is " +
    "downstream of it. Fix the zone file to match the lattice -- not the reverse. " +
    "(The reverse was the old rule, and it is how a physically impossible " +
    "78-socket lattice passed review.)",
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
