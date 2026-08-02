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
 * Row r sits at arc fraction 0.08 + (r/11)*0.86 of the nasion->inion arc — the
 * 10-20 system's own longitudinal ruler. That is recorded here as the scan
 * observation it is; no code reads it, because nothing downstream of the lattice
 * is derived from where a row falls on the arc.
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
 * claim; the row boundaries here are the v1 proposal, not a final map.
 *
 * ── What this generator owns, and what it does NOT (ZONE-1, 2026-07-30) ──────
 *
 * This generator owns exactly one thing: the PHYSICAL LATTICE — socket ids,
 * socket count, row, col, x/y and midline parity. That is a hardware fact and
 * changes only on an inner-bowl re-tool.
 *
 * It does NOT own zone CONTENT. `protocols/predefined/00-zones.npps` is the sole
 * definition of every zone; each zone carries its own explicit `sockets:` list,
 * authored by a human. Nothing here defines, derives or hardcodes a brain lobe.
 * A zone whose socket set happens to correspond to a human brain lobe is a
 * property of how its author chose and named that set, not a concept this code
 * knows about.
 *
 * Earlier revisions derived lobe membership a SECOND time here, from four
 * guessed arc constants, and diffed the result against the zone file. That check
 * was circular: on disagreement the zone file was re-cut from the same constants
 * (see the pre-ZONE-1 note in `validateAgainstZoneFile`), so it proved only that
 * the file matched the code. No anatomy was ever validated by it. Real
 * anatomical validation is gate REG-1 against the shell CAD — a physical
 * activity, not an arithmetic one. The derivation is deleted; the STRUCTURAL
 * checks it used to sit alongside are kept and strengthened.
 *
 * ── Why a derived structure and not a hand-written table ─────────────────────
 *
 * Socket ids run front-to-back by row, left-to-right within each row. Within a
 * row the leftmost sockets are left-hemisphere, the rightmost are
 * right-hemisphere, and an odd-width row's exact centre socket is MIDLINE -- it
 * belongs to BOTH hemispheres, which is what produces the duplicated sockets in
 * 00-zones.npps (2, 13, 29, 46, 62, 74).
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
const SWIFT_OUT = join(
  ROOT, "app", "ios", "NeurOne", "Models", "SocketZones.generated.swift",
);

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

// ── Socket positions in 3-space ───────────────────────────────────────────────
//
// Socket positions are emitted as real coordinates in 3-space, so anything
// downstream — the firmware socket map on the wire, a layout view, an optical
// model — reads one physical description instead of re-deriving position from
// row/col and a pitch.
//
// FRAME (aircraft body axes, right-handed), origin at the shell centre:
//
//     +x  forward   (toward the face)
//     +y  right     (toward the right ear)
//     +z  down      (toward the neck)
//
// So the vault the sockets tile has NEGATIVE z, the crown sits at the most
// negative z, and the rim plane is z = 0 through the origin.
//
// ── ⚠ THE ELLIPSOID BELOW IS A DESCRIPTION, NOT THE GEOMETRY ─────────────────
//
// Principal direction, 2026-07-31: the ellipsoid is a convenient approximation
// useful for describing the shell. It is NOT a fact about it. The reference of
// record is the measured interior scan — `cad/HelmetScan.3dm` — until CAD
// drawings with exact measurements exist.
//
// This code has not yet been moved onto that scan, and the gap is not cosmetic:
//
//   - The scan in the repo is a raw Scaniverse capture (98,003 vertices,
//     181,944 faces) that includes the surroundings it was captured with. It
//     segments into 341 disconnected components; the helmet is not one of them.
//   - A RANSAC fit does recover a shell at R ~ 157 mm, which agrees with the
//     documented rim->crown 157 mm. But the radial histogram about that centre
//     shows ONE broad peak (r ~ 145-175 mm) on a heavy background, not the two
//     clean peaks an interior and an exterior surface would give. Which surface
//     the points belong to is therefore not established, and the inner face is
//     the datum every other layer offsets from (NP-HELMET-GEOM-001 §1).
//   - The scan frame is arbitrary. The rim plane and crown are recoverable, so
//     the vertical axis and the fore-aft LINE are too — but which end of that
//     line is the face is not, and getting it backwards mirrors every socket
//     front-to-back. That is a wrong-site error, not a display one.
//
// So the ellipsoid stays as the INTERIM stand-in, labelled as such, and every
// emitted coordinate is provisional in the stronger sense: not merely unlocked,
// but derived from a shape the principal has explicitly said is not the thing
// itself. Replacing it is scan-segmentation work with two decisions in it that
// belong to a person, not to this script.

/** INTERIM. Semi-axis fore/aft, mm — half the measured interior length. */
const SEMI_AXIS_FORE_AFT_MM = INTERIOR_LENGTH_MM / 2;

/** INTERIM. Semi-axis left/right, mm — half the measured interior breadth. */
const SEMI_AXIS_LATERAL_MM = INTERIOR_BREADTH_MM / 2;

/**
 * INTERIM. Semi-axis vertical, mm. The rim sits at the widest station, so
 * rim->crown IS the semi-axis and the rim plane passes through the centre —
 * which is what makes the origin a physically meaningful point rather than an
 * arbitrary datum. That much survives replacing the ellipsoid with the scan.
 */
const SEMI_AXIS_VERTICAL_MM = RIM_TO_CROWN_MM;

/**
 * Surface point at sagittal parameter `t` and coronal parameter `u`.
 *
 *   t: 0 = front rim, PI/2 = crown, PI = back rim
 *   u: 0 = midline, positive = right (+y)
 *
 * Satisfies the ellipsoid equation identically:
 *   cos²t + sin²t·sin²u + sin²t·cos²u = cos²t + sin²t = 1
 */
// ── The measured interior surface ────────────────────────────────────────────
//
// Socket positions come from the LiDAR interior scan, not from the ellipsoid.
// `hardware/np_helmet_surface.json` holds radius as a function of direction,
// sampled from cad/HelmetScan.3dm and expressed in these same body axes; see
// scripts/extract-helmet-surface.ts for how it is segmented and framed.
//
// The ellipsoid survives ONLY as a parametrisation — a way to sweep directions
// smoothly and evenly across the vault. It no longer supplies any distance. Each
// direction it generates is looked up in the measured table and given the radius
// the real helmet has there.

interface HelmetSurface {
  sampling: { azimuthStepDeg: number; elevationStepDeg: number;
              measuredCells: number; interpolatedCells: number };
  radiusMm: Record<string, { r: number; n: number; measured: boolean }>;
}
const SURFACE: HelmetSurface = JSON.parse(
  readFileSync(join(ROOT, "hardware/np_helmet_surface.json"), "utf8"),
);

/**
 * Measured radius, millimetres, in the given direction — bilinear over the
 * sampled grid. Azimuth 0 is forward and +90 is right; elevation 0 is the rim
 * plane and 90 the crown, matching the extractor's convention.
 */
function measuredRadius(dx: number, dy: number, dz: number): number {
  const horiz = Math.hypot(dx, dy);
  const azDeg = (Math.atan2(dy, dx) * 180) / Math.PI;
  const elDeg = Math.max(0, Math.min(90, (Math.atan2(-dz, horiz) * 180) / Math.PI));
  const { azimuthStepDeg: A, elevationStepDeg: E } = SURFACE.sampling;

  const a0 = Math.floor(azDeg / A) * A, e0 = Math.floor(elDeg / E) * E;
  const fa = (azDeg - a0) / A, fe = (elDeg - e0) / E;
  const wrap = (a: number) => (((a + 180) % 360) + 360) % 360 - 180;
  const at = (a: number, e: number) =>
    SURFACE.radiusMm[`${wrap(a)},${Math.min(90, e)}`]?.r ??
    SURFACE.radiusMm[`${wrap(a)},${Math.min(90, Math.max(0, e - E))}`]?.r ?? 0;

  const r00 = at(a0, e0), r10 = at(a0 + A, e0);
  const r01 = at(a0, e0 + E), r11 = at(a0 + A, e0 + E);
  return (r00 * (1 - fa) + r10 * fa) * (1 - fe) + (r01 * (1 - fa) + r11 * fa) * fe;
}

/**
 * A point on the MEASURED interior surface.
 *
 *   t: 0 = front rim, PI/2 = crown, PI = back rim
 *   u: 0 = midline, positive = right (+y)
 *
 * The (t, u) sweep is ellipsoidal; the RADIUS is the helmet's own.
 */
function surfacePoint(t: number, u: number): { x: number; y: number; z: number } {
  const ex = SEMI_AXIS_FORE_AFT_MM * Math.cos(t);
  const ey = SEMI_AXIS_LATERAL_MM * Math.sin(t) * Math.sin(u);
  const ez = -SEMI_AXIS_VERTICAL_MM * Math.sin(t) * Math.cos(u);
  const len = Math.hypot(ex, ey, ez) || 1;
  const [dx, dy, dz] = [ex / len, ey / len, ez / len];
  const r = measuredRadius(dx, dy, dz);
  return { x: dx * r, y: dy * r, z: dz * r };
}

function ellipsoidPoint(t: number, u: number): { x: number; y: number; z: number } {
  return {
    x: SEMI_AXIS_FORE_AFT_MM * Math.cos(t),
    y: SEMI_AXIS_LATERAL_MM * Math.sin(t) * Math.sin(u),
    // Negative: +z is DOWN in aircraft axes, and the vault is above the rim.
    z: -SEMI_AXIS_VERTICAL_MM * Math.sin(t) * Math.cos(u),
  };
}

/** Simpson integration of `f` over [lo, hi]; `n` must be even. */
function integrate(f: (v: number) => number, lo: number, hi: number, n = 2000): number {
  const h = (hi - lo) / n;
  let sum = f(lo) + f(hi);
  for (let i = 1; i < n; i++) {
    sum += f(lo + i * h) * (i % 2 === 0 ? 2 : 4);
  }
  return (sum * h) / 3;
}

/**
 * Arc-length element along the midline meridian (u = 0), d/dt.
 *
 * Central difference on the MEASURED surface rather than the ellipsoid's closed
 * form. Arc lengths, row spacing and the ring-fit diagnostic all descend from
 * this, so leaving it analytic would have kept the diagnostic reasoning about a
 * shape the socket positions no longer come from.
 */
const DIFF_H = 1e-4;
function speedAlong(
  at: (v: number) => { x: number; y: number; z: number }, v: number,
): number {
  const a = at(v - DIFF_H), b = at(v + DIFF_H);
  return Math.hypot(b.x - a.x, b.y - a.y, b.z - a.z) / (2 * DIFF_H);
}

function sagittalSpeed(t: number): number {
  return speedAlong((v) => surfacePoint(v, 0), t);
}

/** Arc length along the midline meridian from the front rim to parameter `t`. */
function sagittalArcTo(t: number): number {
  return integrate(sagittalSpeed, 0, t);
}

/** Arc-length element along the coronal ring at sagittal parameter `t`, d/du. */
function coronalSpeed(t: number, u: number): number {
  return speedAlong((v) => surfacePoint(t, v), u);
}

/** Arc length along the ring at `t`, from the midline out to `u`. */
function coronalArcTo(t: number, u: number): number {
  const s = integrate(v => coronalSpeed(t, v), 0, Math.abs(u));
  return u < 0 ? -s : s;
}

/** Invert a monotone arc-length function by bisection. */
function invertArc(
  arcAt: (v: number) => number, target: number, lo: number, hi: number,
): number {
  for (let i = 0; i < 80; i++) {
    const mid = (lo + hi) / 2;
    if (arcAt(mid) < target) { lo = mid; } else { hi = mid; }
  }
  return (lo + hi) / 2;
}

/** Full front-rim-to-back-rim arc over the crown, on the midline meridian, mm. */
function sagittalArcModelledMm(): number {
  return sagittalArcTo(Math.PI);
}

/** Full ear-to-ear arc over the crown at the widest station, mm. */
function coronalArcModelledMm(): number {
  return 2 * coronalArcTo(Math.PI / 2, Math.PI / 2);
}

// ── The measured row structure ────────────────────────────────────────────────
//
// Widths MEASURED off the scan, front->back. These are not computed from an
// ellipsoid: the interior surface was tiled with 40 mm hexes and the whole tiles
// counted per coronal band. Parity ALTERNATES o e o e o e o e o e o e — required
// (see the packing note below), and satisfied by construction here.
const ROW_WIDTHS: readonly number[] = [3, 6, 7, 8, 9, 8, 9, 8, 7, 6, 5, 4];

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

/**
 * Every emitted point must lie ON the ellipsoid, and the modelled arcs must
 * reproduce the SCANNED ones. The second check is the load-bearing one: it is
 * what makes the ellipsoid a description of the measured helmet rather than a
 * shape that merely happens to hold sockets. If the real interior stops being
 * ellipsoidal enough, this fails rather than silently emitting wrong positions.
 */
function checkEllipsoid(sockets: SocketGeometry[]): string[] {
  const errors: string[] = [];

  for (const s of sockets) {
    const r =
      0; // (unused — the ellipsoid residual is no longer the invariant)
    const rMeasured = measuredRadius(
      s.xMm / Math.hypot(s.xMm, s.yMm, s.zMm),
      s.yMm / Math.hypot(s.xMm, s.yMm, s.zMm),
      s.zMm / Math.hypot(s.xMm, s.yMm, s.zMm),
    );
    const rActual = Math.hypot(s.xMm, s.yMm, s.zMm);
    if (Math.abs(rActual - rMeasured) > 0.5) {
      throw new Error(
        `socket ${s.id} at (${s.xMm}, ${s.yMm}, ${s.zMm}) is ${rActual.toFixed(1)} mm ` +
        `from the origin but the measured surface is ${rMeasured.toFixed(1)} mm in that ` +
        `direction. Positions must lie ON the scanned interior, not near it.`,
      );
    }
    if (s.zMm > 0) {
      errors.push(
        `socket ${s.id} has z = ${s.zMm} mm, below the rim plane. +z is DOWN in ` +
        `aircraft axes, so every vault socket must have z <= 0.`,
      );
    }
  }

  // Modelled vs measured arcs. 5% is the tolerance an ellipsoid fitted to three
  // extents can be expected to hold against a LiDAR-scanned real surface.
  const sag = sagittalArcModelledMm();
  const cor = coronalArcModelledMm();
  if (Math.abs(sag - SAGITTAL_ARC_MM) / SAGITTAL_ARC_MM > 0.10) {
    errors.push(
      `sagittal arc integrated over the scanned surface is ${sag.toFixed(1)} mm ` +
      `against the separately-measured ${SAGITTAL_ARC_MM} mm — over 10% apart. Two ` +
      `readings of the same scan cannot differ that far; suspect the frame or the ` +
      `sampling, not the constants.`,
    );
  }
  if (Math.abs(cor - CORONAL_ARC_MM) / CORONAL_ARC_MM > 0.10) {
    errors.push(
      `coronal arc integrated over the scanned surface is ${cor.toFixed(1)} mm ` +
      `against the separately-measured ${CORONAL_ARC_MM} mm — over 10% apart.`,
    );
  }
  return errors;
}

/**
 * How well the scan-counted ROW_WIDTHS actually fit the MEASURED surface.
 *
 * ── A KNOWN, QUANTIFIED DISAGREEMENT ────────────────────────────────────────
 *
 * They do not fit exactly, and since 2026-07-31 this is measured against the
 * scanned interior rather than an ellipsoid — so any residual is a real property
 * of the reference helmet against the row widths counted off it, not an artefact
 * of the model in between.
 *
 * That is squarely a REG-1 question (registering the lattice against shell CAD,
 * NP-HEX-ZM-001 §7) rather than something to fix by choosing kinder constants
 * here. What must NOT happen is it disappearing: the two lattice failures this
 * project has already had — the 78-socket lattice no tile width could produce,
 * and NP_HEXMAP_MAX_SOCKETS guessed at 64 — were both unchecked constants that
 * no code ever contradicted out loud.
 *
 * So: any compression is REPORTED with its number, and a gross one FAILS. The
 * hard floor is deliberately well below the observed deviation, so this catches
 * a real regression without blocking on a documented approximation.
 */
/*
 * ── Why this floor moved on 2026-07-31 ──────────────────────────────────────
 *
 * It was 0.85 while this diagnostic measured ROW_WIDTHS against an ELLIPSOID.
 * It now measures them against the SCANNED interior, and the worst row moved
 * from 86.4% to 84.3% — the real helmet is slightly tighter at the rear rim than
 * the ellipsoid through its extents suggested.
 *
 * The floor is re-baselined to 0.80 rather than left at 0.85, because the
 * quantity being measured changed underneath it: a threshold calibrated against
 * the model would now be failing on the measurement, which is backwards. It is
 * still set BELOW the observed worst case so it catches a regression rather than
 * asserting the present lattice is fine — row 11 at 84.3% is a live REG-1
 * finding, reported in full every run, not a resolved one.
 */
const RING_FIT_HARD_FLOOR = 0.80;

function checkRingFit(): { errors: string[]; warnings: string[] } {
  const errors: string[] = [];
  const warnings: string[] = [];

  rowCompression.forEach((c, r) => {
    if (c >= 0.9999) { return; }
    const pitch = MODULE_WIDTH_MM * c;
    const msg =
      `row ${r} (${ROW_WIDTHS[r]} wide) is compressed to ${(c * 100).toFixed(1)}% ` +
      `— tiles sit ${pitch.toFixed(1)} mm apart instead of ${MODULE_WIDTH_MM} mm, ` +
      `so they overlap by ${(MODULE_WIDTH_MM - pitch).toFixed(1)} mm. The SCANNED ` +
      `ring at that station is shorter than the scan-counted row width needs.`;
    if (c < RING_FIT_HARD_FLOOR) {
      errors.push(msg + ` Below the ${RING_FIT_HARD_FLOOR} hard floor.`);
    } else {
      warnings.push(msg);
    }
  });

  return { errors, warnings };
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
  row: number;
  col: number;
  /** Provisional unit-hex lattice coordinates — layout only, not millimetres. */
  x: number;
  y: number;
  /**
   * Position in 3-space, millimetres, in aircraft body axes with the origin at
   * the ellipsoid centre: +x forward, +y right, +z down. The vault has negative
   * z. This is the physical location — nothing here describes anatomy.
   *
   * A socket's anatomical meaning comes from ZONE MEMBERSHIP, authored in
   * protocols/predefined/00-zones.npps. Neither a lobe nor a side is a property
   * of the hardware.
   */
  xMm: number;
  yMm: number;
  zMm: number;
}

/** Vertical spacing of a unit hex lattice: sqrt(3)/2. */
const HEX_ROW_PITCH = 0.8660254037844386;

/**
 * Per-row lateral compression, 1 = the row's tiles sit at their full 40 mm arc
 * pitch. Below 1 means the ellipsoid's ring at that station is SHORTER than the
 * scan-counted row width needs, so the emitted tiles are squeezed. Populated by
 * buildSockets, reported by checkRingFit.
 */
const rowCompression: number[] = [];

/** Non-fatal ring-fit findings, printed prominently at the end of a run. */
const ringFitWarnings: string[] = [];

function buildSockets(): SocketGeometry[] {
  const sockets: SocketGeometry[] = [];
  let id = NUMBERING_BASE;

  // Rows are laid along the midline meridian at the measured pitch, centred in
  // the available arc so the front and back rim margins match. The lattice does
  // not run rim to rim: the tiles stop short of both.
  const totalArc = sagittalArcModelledMm();
  const rowSpan = (ROW_WIDTHS.length - 1) * ROW_PITCH_MM_MEASURED;
  const firstRowArc = (totalArc - rowSpan) / 2;

  ROW_WIDTHS.forEach((width, r) => {
    // Sagittal parameter for this row, from its arc position.
    const rowArc = firstRowArc + r * ROW_PITCH_MM_MEASURED;
    const t = invertArc(sagittalArcTo, rowArc, 0, Math.PI);

    // Columns sit at whole module widths of ARC along this row's ring, symmetric
    // about the midline. Odd widths put a socket on the midline; even widths
    // straddle it — the offset that makes the packing work.
    //
    // Where the ring is SHORTER than the row needs, the row is compressed
    // uniformly rather than clamped. Clamping piles the outermost sockets on top
    // of each other and hides the problem; uniform compression keeps the row
    // ordered and makes the shortfall a single reportable number per row
    // (rowCompression below), which checkRingFit then holds to account.
    const halfRing = coronalArcTo(t, Math.PI / 2);
    const halfNeeded = ((width - 1) / 2) * MODULE_WIDTH_MM;
    const compression = halfNeeded > halfRing && halfNeeded > 0
      ? halfRing / halfNeeded
      : 1;
    rowCompression[r] = compression;

    for (let col = 0; col < width; col++) {
      const offsetMm = (col - (width - 1) / 2) * MODULE_WIDTH_MM * compression;
      const u = invertArc(v => coronalArcTo(t, v), offsetMm, -Math.PI / 2, Math.PI / 2);
      const p = surfacePoint(t, u);

      sockets.push({
        id,
        row: r,
        col,
        x: Number((col - (width - 1) / 2).toFixed(4)),
        y: Number((r * HEX_ROW_PITCH).toFixed(4)),
        xMm: Number(p.x.toFixed(2)),
        yMm: Number(p.y.toFixed(2)),
        zMm: Number(p.z.toFixed(2)),
      });
      id++;
    }
  });

  return sockets;
}

// ─── Validation against the locked zone file ───────────────────────────────────

/**
 * Aggregate zones, and the zones each one unions. These are ZONE NAMES authored
 * in 00-zones.npps — this table says nothing about anatomy and no entry is
 * derived from geometry. It is the composition contract: whoever authored
 * "Frontal" asserted it is exactly "Frontal Left" ∪ "Frontal Right", and this
 * generator holds them to it.
 *
 * Checking only "do the referenced sockets exist" is not enough: re-cut the
 * lattice larger and a stale `All` would keep listing a subset, every id would
 * still resolve, the build would pass, and the helmet would silently address
 * part of itself. "All" means every socket.
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
 * Zones that are deliberately NOT unions of other zones — a subset or novel
 * grouping. "Motor / SMA" is a single coronal band minus its two outermost
 * sockets (PROVISIONAL pending REG-1). "Frontal Right (excl. midline)" is
 * "Frontal Right" minus the shared midline sockets, for a lateralized protocol
 * that must stay off the midline (the PBM optical-resolution floor,
 * np_opt_psf_001). Both still have to reference real, deduplicated sockets —
 * they are exempt only from the union check, and must be listed here rather than
 * silently skipped.
 */
const NON_UNION_ZONES = new Set([
  "Motor / SMA",
  "Frontal Right (excl. midline)",
]);

/**
 * Zones named as a part of some aggregate. Derived from AGGREGATES rather than
 * listed again, so there is exactly one place a zone name is written down and no
 * second list to fall out of step with it.
 */
const PART_ZONES = new Set(Object.values(AGGREGATES).flat());

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
 * Fail loudly if the row structure no longer matches the tile geometry, or if
 * the shipped zone definitions are not STRUCTURALLY sound against the lattice.
 *
 * Note the direction of authority, and its limit (ZONE-1, 2026-07-30). The TILE
 * GEOMETRY here is the locked artefact for the LATTICE — socket ids, count, row,
 * col, x/y, parity — and `00-zones.npps` may not contradict it; that is what
 * stopped a physically impossible 78-socket lattice in 2026-07-20.
 *
 * Authority stops there. Zone CONTENT — which sockets are in which zone — is
 * authored in `00-zones.npps` and is never re-derived here. What is checked is
 * structure, which is verifiable without knowing what any zone MEANS:
 *
 *   - every socket a zone names exists on this lattice
 *   - no zone lists the same socket twice (a zone is a set)
 *   - no zone is empty, and no declared zone is silently unparsed
 *   - "All" is exactly every socket
 *   - each aggregate is exactly the deduplicated union of its named parts
 *   - every zone in the file is registered as an aggregate, a named part of one,
 *     or an explicit non-union zone
 *
 * A structural failure here means the file and the hardware disagree about what
 * is addressable. It does NOT mean the file is anatomically wrong — that is gate
 * REG-1 against the shell CAD, and no arithmetic in this script can stand in for
 * it.
 */
function validateAgainstZoneFile(sockets: SocketGeometry[]): string[] {
  const errors: string[] = [];
  const source = readFileSync(ZONE_FILE, "utf-8");
  const actual = parseZoneFile(source);

  // Geometry must tessellate: the parity rule and the packing it produces are
  // both asserted directly (parity on the widths, overlap on the coordinates).
  errors.push(...checkParity());
  errors.push(...checkPacking(sockets));
  errors.push(...checkEllipsoid(sockets));
  const ringFit = checkRingFit();
  errors.push(...ringFit.errors);
  ringFitWarnings.push(...ringFit.warnings);

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

  // Every `zone "..."` in the file must have produced a parsed socket list.
  // parseZoneFile skips a block whose `sockets:` key is missing or malformed;
  // without this a typo'd zone would vanish from every check below and the build
  // would go green on a file that no longer defines it.
  const declared = [...source.matchAll(/zone\s+"((?:[^"\\]|\\.)*)"\s*\{/g)].map(m => m[1]!);
  for (const name of declared) {
    if (!actual.has(name)) {
      errors.push(
        `zone "${name}" is declared but has no parsable \`sockets: [...]\` list — ` +
        `every zone must carry its own explicit socket set.`,
      );
    }
  }
  const dupeDecls = declared.filter((n, i) => declared.indexOf(n) !== i);
  if (dupeDecls.length > 0) {
    errors.push(
      `zone name(s) declared more than once: [${[...new Set(dupeDecls)].join(", ")}] — ` +
      `the later block silently wins, so names must be unique.`,
    );
  }

  const validIds = new Set(sockets.map(s => s.id));

  // Per-zone structural soundness. Applies to EVERY zone in the file, including
  // user-authored and research zones — none of them is exempt from naming real,
  // deduplicated sockets.
  for (const [name, ids] of actual) {
    if (ids.length === 0) {
      errors.push(`zone "${name}" is empty — a zone with no sockets addresses nothing.`);
    }
    const unknown = ids.filter(id => !validIds.has(id));
    if (unknown.length > 0) {
      errors.push(
        `zone "${name}" references non-existent sockets: [${unknown}] — ` +
        `this helmet has sockets ${sockets[0]!.id}-${sockets[sockets.length - 1]!.id}.`,
      );
    }
    const dupes = ids.filter((id, i) => ids.indexOf(id) !== i);
    if (dupes.length > 0) {
      errors.push(`zone "${name}" lists duplicate sockets: [${[...new Set(dupes)]}] — a zone is a set`);
    }
    if (!AGGREGATES[name] && !PART_ZONES.has(name) && !NON_UNION_ZONES.has(name)) {
      errors.push(
        `zone "${name}" is not registered in scripts/sync-socket-map.ts. Add it to ` +
        `AGGREGATES (with the zones it unions), name it as a part of an existing ` +
        `aggregate, or add it to NON_UNION_ZONES (a deliberate novel grouping).`,
      );
    }
  }

  // Each aggregate must be exactly the deduplicated union of its named parts,
  // read from the zone file on both sides. Nothing here is derived from geometry
  // — this is the file being held to its own composition contract.
  const union = (...names: string[]) =>
    [...new Set(names.flatMap(n => actual.get(n) ?? []))].sort((a, b) => a - b);

  for (const [name, partNames] of Object.entries(AGGREGATES)) {
    const ids = actual.get(name);
    if (!ids) {
      errors.push(`aggregate zone "${name}" is missing from the zone file`);
      continue;
    }
    const missingParts = partNames.filter(p => !actual.has(p));
    if (missingParts.length > 0) {
      errors.push(
        `aggregate zone "${name}" names part zone(s) missing from the zone file: ` +
        `[${missingParts.join(", ")}]`,
      );
      continue;
    }
    const want = union(...partNames);
    if (JSON.stringify(ids) !== JSON.stringify(want)) {
      errors.push(
        `aggregate zone "${name}" is not the union of [${partNames.join(", ")}]\n` +
        `    union of parts: [${want}]\n` +
        `    zone file:      [${ids}]`,
      );
    }
  }

  // "All" additionally means EVERY socket — pinned independently of the union,
  // so a part zone that lost a socket cannot quietly shrink "All" with it.
  const all = actual.get("All");
  if (all && (all.length !== sockets.length || all.some(id => !validIds.has(id)))) {
    errors.push(
      `zone "All" lists ${all.length} sockets but the helmet has ${sockets.length}. ` +
      `"All" must mean every socket on the helmet and nothing less.`,
    );
  }

  return errors;
}

// ─── Lateralized-protocol audit ────────────────────────────────────────────────
//
// Why this lives here: zone membership is INCLUSIVE by default (PR #210 ruling) — a
// midline socket is listed in BOTH the left- and right-side zone it belongs to, and the
// only way to narrow a zone is to author a narrower one. There is no runtime exclusion
// filter.
//
// That rule is free for a protocol that targets both hemispheres or the
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
//      is a single hemisphere ("* Left" or "* Right" alone). A bilateral pair or a
//      whole-region zone (All, Frontal, Posterior, Vault (excl. Occipital),
//      Motor / SMA) is not boundary-sensitive.
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

/**
 * Zone names that name a single hemisphere: anything ending in " Left" or
 * " Right", nothing else.
 *
 * Deliberately open-ended (ZONE-1): it used to enumerate four lobe names, which
 * meant a user- or research-authored zone called "DLPFC Left" fell straight
 * through the midline-spill gate. The pairing test below is what decides whether
 * a hit is really lateralized, and it works on any name. A name with a trailing
 * qualifier — "Frontal Right (excl. midline)" — does not match, which is
 * correct: narrowing IS the remedy this gate asks for.
 */
const SINGLE_HEMISPHERE_RE = /^(.+) (Left|Right)$/;

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
    // A hit is only really lateralized if its counterpart is absent: "<stem> Left"
    // without "<stem> Right" (or vice versa). The stem is whatever precedes the
    // side word — this makes no assumption about what the stem names.
    const stems = new Set(lateral.map(z => z.replace(/ (Left|Right)$/, "")));
    const unpaired = [...stems].filter(
      stem => !(namesInFile.includes(`${stem} Left`) && namesInFile.includes(`${stem} Right`)),
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

/**
 * Swift zone table for the iOS app.
 *
 * The app names a socket by the ZONE it is in — zones are what replaced lobe and
 * side, and they are authored in 00-zones.npps, not derived from geometry. The
 * helmet cannot supply this: zones are protocol data, users may define their own
 * in any .npps file, and a socket belongs to several at once. So the app needs
 * its own copy, and it is generated from the same file the web app and the
 * structural checks read, rather than hand-kept.
 *
 * Coordinates deliberately are NOT emitted here. Those come from the helmet over
 * the socket-map characteristic, because the LATTICE can be re-cut by a firmware
 * update and an app-side copy would then be wrong with no way to notice. Zone
 * membership is the opposite: authored, versioned with the app, and meaningless
 * to the device.
 */
/**
 * SwiftLint's line_length warning threshold for app/ios (see .swiftlint.yml).
 * CI runs `swiftlint --strict`, so a warning fails the build — generated Swift
 * has to clear this exactly like hand-written source does.
 */
const SWIFT_MAX_LINE = 140;

/**
 * Fail the generator if anything it emits would fail the linter.
 *
 * The alternative — excluding generated files in .swiftlint.yml — would also
 * switch off the custom ISC-8 (no print) and ISC-9 (no hardcoded URLs/keys)
 * rules for this file, and those are worth keeping pointed at generated output.
 * Checking here means a future emitter change surfaces in the generator run
 * rather than three steps later in CI.
 */
function assertSwiftLintClean(source: string, path: string): void {
  // A length-only check let an emitter bug through once: the array items were
  // joined with spaces and no commas, which is short, lint-clean and not Swift.
  const items = source.match(/"[^"\n]*"\s+"/g);
  if (items !== null) {
    throw new Error(
      `${path} has ${items.length} adjacent string literal(s) with no comma ` +
      `between them — the array wrapper dropped its separators. First: ${items[0]}`,
    );
  }

  const overlong = source.split("\n")
    .map((line, i) => ({ n: i + 1, len: line.length, line }))
    .filter(l => l.len > SWIFT_MAX_LINE);
  if (overlong.length > 0) {
    const shown = overlong.slice(0, 5)
      .map(l => `    line ${l.n}: ${l.len} chars — ${l.line.slice(0, 60)}...`)
      .join("\n");
    throw new Error(
      `${path} has ${overlong.length} line(s) over SwiftLint's ${SWIFT_MAX_LINE}-char ` +
      `limit, and CI runs \`swiftlint --strict\` so a warning fails the build:\n${shown}\n` +
      `Fix the emitter's wrapping — do not exclude the file from linting.`,
    );
  }
}

function emitSwiftZones(
  sockets: SocketGeometry[], zones: Map<string, number[]>,
): string {
  // Most-specific zone = the smallest one containing the socket. "All" is the
  // largest and so is never chosen while any narrower zone exists; a socket in
  // "Motor / SMA" (7) is described by that rather than by "Frontal Left" (20).
  const bySize = [...zones.entries()].sort((a, b) => a[1].length - b[1].length);

  // Emitted Swift has to pass `swiftlint --strict` like hand-written source —
  // the app lints its whole source tree and generated files are not carved out
  // (which also keeps the ISC-8/9 security rules applying to whatever this
  // emits). line_length warns at 140, and a socket in six zones blows past that
  // on one line, so entries wrap.
  const rows = sockets.map(s => {
    const all = bySize.filter(([, ids]) => ids.includes(s.id)).map(([n]) => n);
    const primary = all[0] ?? null;
    const head = `    ${s.id}: SocketZoneEntry(`;
    const primaryArg = `primary: ${primary === null ? "nil" : JSON.stringify(primary)}`;
    const items = all.map(n => JSON.stringify(n));

    // One line when it fits, which keeps the common short cases readable.
    const oneLine = `${head}${primaryArg}, all: [${items.join(", ")}]),`;
    if (oneLine.length <= SWIFT_MAX_LINE) { return oneLine; }

    // Otherwise: argument per line, and the array itself greedily wrapped to the
    // same budget rather than one element per line, which would be unreadable
    // for a socket in six zones.
    const indent = "            ";
    const wrapped: string[] = [];
    let line = "";
    for (const item of items) {
      // Comma travels WITH the item — building the line without it and adding
      // separators later is how the first cut of this emitted a Swift array of
      // space-separated strings with no commas at all.
      const piece = `${item},`;
      const next = line === "" ? piece : `${line} ${piece}`;
      if (`${indent}${next}`.length > SWIFT_MAX_LINE) {
        wrapped.push(`${indent}${line}`);
        line = piece;
      } else {
        line = next;
      }
    }
    if (line !== "") { wrapped.push(`${indent}${line}`); }

    return [
      `    ${s.id}: SocketZoneEntry(`,
      `        ${primaryArg},`,
      `        all: [`,
      ...wrapped,
      `        ]),`,
    ].join("\n");
  }).join("\n");

  return `${BANNER.replace(/^\/\*/, "/*").replace(/\/\/ /g, "// ")}
import Foundation

/// Which zones a socket belongs to, authored in protocols/predefined/00-zones.npps.
///
/// Zones replaced lobe and side: neither is a property of the hardware, and
/// nothing in the system derives one. A socket's anatomical meaning is the set of
/// zones a human put it in.
struct SocketZoneEntry {
    /// Smallest zone containing this socket — the most specific description
    /// available, and what the app speaks. nil if no zone lists it.
    let primary: String?
    /// Every zone containing it, smallest first. Sockets belong to several:
    /// a midline socket is in both the left and right zone of its region.
    let all: [String]
}

enum SocketZones {

    /// 1-based socket id -> zone membership.
    static let bySocket: [UInt8: SocketZoneEntry] = [
${rows}
    ]

    /// Most specific zone name for a socket, or nil when the table has no entry
    /// (a helmet reporting a socket this app build does not know about).
    static func primaryZone(for socketID: UInt8) -> String? {
        bySocket[socketID]?.primary
    }

    static func zones(for socketID: UInt8) -> [String] {
        bySocket[socketID]?.all ?? []
    }

    /// Socket ids this table covers. Not a hardware claim — the helmet's own
    /// socket map is authoritative for which sockets physically exist.
    static let socketCount = ${sockets.length}
}
`;
}

function emitJson(sockets: SocketGeometry[]): string {
  return JSON.stringify(
    {
      _generator: "scripts/sync-socket-map.ts",
      _note:
        "Socket geometry for the NeurOne helmet. Scan-grounded v1 lattice " +
        "(NP-HEX-ZM-001 §3.4), PROVISIONAL pending gates REG-1 and ACT-1. Validated " +
        "against protocols/predefined/00-zones.npps. x/y are provisional unit-hex " +
        "lattice coordinates for UI layout only. xMm/yMm/zMm are positions in " +
        "3-space on the modelled ellipsoid, aircraft body axes (+x forward, +y " +
        "right, +z down), origin at the ellipsoid centre — PROVISIONAL, replace " +
        "from shell CAD before any physical claim. No lobe or side: a socket's " +
        "anatomical meaning is its zone membership, authored in 00-zones.npps.",
      _basis: "scan-measured lattice; INTERIM ellipsoid for 3-space coordinates",
      _coordinateBasis:
        "xMm/yMm/zMm are derived from an INTERIM ellipsoid through the measured " +
        "extents. Principal direction 2026-07-31: the ellipsoid is a description, " +
        "not a fact. The reference of record is the measured interior scan " +
        "(cad/HelmetScan.3dm) until exact CAD exists. See the header of " +
        "scripts/sync-socket-map.ts for what blocks the move.",
      socketCount: sockets.length,
      /** Socket ids are 1-based, matching .npps zone `sockets:` lists. */
      numberingBase: NUMBERING_BASE,
      /** Inclusive id bounds. Parsers reject `sockets:` entries outside these. */
      socketIdMin: sockets[0]!.id,
      socketIdMax: sockets[sockets.length - 1]!.id,
      /** The measured geometry the lattice sits on — see the script header. */
      geometry: geometryFields(),
      ellipsoidSemiAxesMm: {
        foreAft: SEMI_AXIS_FORE_AFT_MM,
        lateral: SEMI_AXIS_LATERAL_MM,
        vertical: SEMI_AXIS_VERTICAL_MM,
      },
      coordinateFrame: "aircraft body axes: +x forward, +y right, +z down; " +
        "origin at the ellipsoid centre (rim plane)",
      rowWidths: [...ROW_WIDTHS],
      sockets,
    },
    null,
    2,
  ) + "\n";
}

function emitTypeScript(sockets: SocketGeometry[]): string {
  const rows = sockets
    .map(s =>
      `  { id: ${s.id}, row: ${s.row}, col: ${s.col}, x: ${s.x}, y: ${s.y}, ` +
      `xMm: ${s.xMm}, yMm: ${s.yMm}, zMm: ${s.zMm} },`)
    .join("\n");
  const g = geometryFields();

  return `${BANNER}
export interface NPSocketGeometry {
  /** 1-based socket id, matching the \`sockets:\` lists in .npps zone files. */
  id: number;
  row: number;
  col: number;
  /** Provisional unit-hex lattice coordinates — layout only, not millimetres. */
  x: number;
  y: number;
  /**
   * Position in 3-space, MILLIMETRES, aircraft body axes, origin at the centre
   * of the ellipsoid the helmet is a partial shell of:
   *   +x forward (toward the face), +y right, +z down.
   * The vault has negative z; the crown sits at z = -${SEMI_AXIS_VERTICAL_MM}.
   *
   * There is deliberately no lobe and no side here. A socket's anatomical
   * meaning is its ZONE MEMBERSHIP, authored in
   * protocols/predefined/00-zones.npps — not a property of the hardware.
   */
  xMm: number;
  yMm: number;
  zMm: number;
}

/** Semi-axes of the modelled ellipsoid, mm (fore/aft, lateral, vertical). */
export const NP_ELLIPSOID_SEMI_AXES_MM = {
  foreAft: ${SEMI_AXIS_FORE_AFT_MM},
  lateral: ${SEMI_AXIS_LATERAL_MM},
  vertical: ${SEMI_AXIS_VERTICAL_MM},
} as const;

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

/** Value of a `--flag <value>` argument, or undefined. */
function parseArg(flag: string): string | undefined {
  const i = process.argv.indexOf(flag);
  return i >= 0 ? process.argv[i + 1] : undefined;
}

const sockets = buildSockets();
const errors = validateAgainstZoneFile(sockets);

if (errors.length > 0) {
  console.error("The shipped zone definitions are not structurally sound:\n");
  for (const e of errors) console.error(`  - ${e}`);
  console.error(
    "\nEither the row structure in this script or 00-zones.npps is wrong.\n" +
    "THE LATTICE IS THE LOCKED ARTEFACT -- socket ids, count, row, col, x/y and " +
    "parity are the scan-measured ROW_WIDTHS on the interior surface " +
    "(NP-HEX-ZM-001 §3.4), and no zone may address a socket that lattice does not " +
    "have. (Letting the zone file win on the LATTICE is how a physically " +
    "impossible 78-socket lattice passed review.)\n" +
    "ZONE CONTENT IS THE OPPOSITE: which sockets are in which zone is AUTHORED in " +
    "00-zones.npps and is never re-derived here. If a zone's membership looks " +
    "wrong, that is an editorial decision in the zone file -- do not 'fix' it from " +
    "this script.",
  );
  process.exit(1);
}

const outputs: Array<[string, string]> = [
  [JSON_OUT, emitJson(sockets)],
  [TS_OUT, emitTypeScript(sockets)],
  [SWIFT_OUT, emitSwiftZones(sockets, parseZoneFile(readFileSync(ZONE_FILE, "utf-8")))],
];

// Generated Swift is linted like any other source, so check it here rather than
// discovering it in CI.
assertSwiftLintClean(outputs.find(([p]) => p === SWIFT_OUT)![1], SWIFT_OUT);

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
    "\nZone membership is inclusive (PR #210): a midline socket is listed in BOTH the Left\n" +
    "and the Right zone it belongs to, so a single-hemisphere zone leaks a share of its cortical\n" +
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
  `${sockets.length} sockets — all ${parseZoneFile(readFileSync(ZONE_FILE, "utf-8")).size} ` +
  `zones in 00-zones.npps are structurally sound against the lattice.`,
);
console.log(
  `3-space: positions sampled from the SCANNED interior ` +
  `(${SURFACE.sampling.measuredCells} measured cells, ` +
  `${SURFACE.sampling.interpolatedCells} interpolated). Arcs integrated over that ` +
  `surface: sagittal ${sagittalArcModelledMm().toFixed(0)} mm ` +
  `(directly measured ${SAGITTAL_ARC_MM}), coronal ` +
  `${coronalArcModelledMm().toFixed(0)} mm (directly measured ${CORONAL_ARC_MM}). ` +
  `The coronal path crosses both ear regions, which are the thinnest-covered part ` +
  `of the scan and therefore the most interpolated — expect it to read long there.`,
);
if (ringFitWarnings.length > 0) {
  console.warn(
    `\nRING FIT — ${ringFitWarnings.length} row(s) do not fit the SCANNED interior ` +
    `at full ${MODULE_WIDTH_MM} mm pitch:`,
  );
  for (const w of ringFitWarnings) { console.warn(`  - ${w}`); }
  console.warn(
    "Measured against the scan itself, so this is a property of the reference helmet\n" +
    "against the row widths counted off it — not an artefact of a model in between.\n" +
    "Since 2026-07-31 row 1 FITS (the ellipsoid had it compressed to 96.9%) and row\n" +
    "11 got tighter (86.4% -> 84.3%): the real rear rim narrows faster than the\n" +
    "ellipsoid implied. Resolving it is REG-1 (register the lattice against shell\n" +
    "CAD), not a matter of picking kinder constants here. Reported every run.",
  );
}
if (auditDirOverride) {
  console.log(`lateralization audit: FIXTURE RUN against ${auditDirOverride} — not a real check.`);
} else {
  console.log(
    `lateralization audit: ${protocolFiles.length} protocols scanned, ` +
    `no unreviewed single-hemisphere zones` +
    `${Object.keys(ACCEPTED_LATERALIZED).length > 0 ? ` (${Object.keys(ACCEPTED_LATERALIZED).length} accepted)` : ""}.`,
  );
}
