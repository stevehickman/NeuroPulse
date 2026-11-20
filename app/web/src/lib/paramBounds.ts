/*
 * paramBounds.ts — what a bound MEANS for a web control. The bounds themselves are not here.
 *
 * Every range any app applies to a parameter, a clinician limit or a timing field is in ONE file,
 * common/npps/bounds.json, and scripts/sync-constants.ts writes it into common/lib/bounds.generated.ts (this
 * app), NppsBounds.generated.kt (Compose, Android) and NppsBounds.generated.swift (iOS). To find or change a
 * bound, edit that JSON and run `bun scripts/sync-constants.ts`; `--check` fails CI if an output is stale.
 * This file re-exports the generated table under the names the controls use and holds the logic that turns a
 * bound into a control (controlKind, fineRange) and a check (inBounds, limitsOutOfRange).
 *
 * What a bound may rest on (the principal, 2026-11-15; docs/status/completed-decisions.md):
 *   natural        cannot be otherwise: a frequency or a time is not below 0.
 *   hardware       set by the hardware or its wire format: a driver ceiling, a descriptor field's width.
 *   practical      set by what a person can perceive or do: the ear does not respond above 20 kHz.
 *   safety         a health or safety limit, with its source.
 *   unestablished  carried over with no source on record, each one a question for the principal (UC-191).
 * A bound with none of those reasons is arbitrary and is not in the file: that side is left open and its
 * `unbounded` note says why. A bound is only ever shown or checked. Nothing in the controls clamps a typed value,
 * and the validator, the compiler and the hub stay the places a value is refused.
 *
 * Which control a range gets follows from its bounds alone (controlKind below):
 *   bounded on both sides        a slider, with an editable value field and a magnifier for fine changes.
 *   bounded on one side or none  a typed field only. A range with no end has no track, even a logarithmic
 *                                one (the principal chose a typed field, 2026-11-15).
 */
import {
  INTERVAL_BOUNDS,
  PARAM_BOUNDS,
  PROTOCOL_BOUNDS,
  LIMIT_BOUNDS,
  type NPBound,
  type NPBoundBasis,
  type NPBoundEdge,
} from '../../../../common/lib/bounds.generated';

export { INTERVAL_BOUNDS, PARAM_BOUNDS, PROTOCOL_BOUNDS, LIMIT_BOUNDS };
export type ParamBound = NPBound;
export type Edge = NPBoundEdge;
export type Basis = NPBoundBasis;

// ─── Slider geometry ──────────────────────────────────────────────────────────

/** Width of a slider track in pixels. A range with more steps than this cannot be set to every step by
 *  dragging, so its slider offers the magnifier. */
export const SLIDER_TRACK_PX = 200;

// ─── What a bound means for a control ─────────────────────────────────────────

export type ControlKind = 'slider' | 'field';

/** Both sides bounded: a slider. Anything else: a typed field (a range with no end has no track). */
export function controlKind(b: ParamBound): ControlKind {
  return b.min !== undefined && b.max !== undefined ? 'slider' : 'field';
}

/** How many steps a slider spans. Infinity for a range with an open side. */
export function stepsAcross(b: ParamBound): number {
  if (b.min === undefined || b.max === undefined) return Infinity;
  return Math.round((b.max.value - b.min.value) / b.step);
}

/** True when dragging the slider cannot reach every step: more steps than pixels on the track. */
export function needsMagnifier(b: ParamBound, trackPx: number = SLIDER_TRACK_PX): boolean {
  return stepsAcross(b) > trackPx;
}

/** The window the magnifier shows: `trackPx` steps centred on `value`, kept inside the bounds and on the
 *  step grid. At one step per pixel every step is reachable. A range that already fits returns whole. */
export function fineRange(value: number, b: ParamBound, trackPx: number = SLIDER_TRACK_PX): { min: number; max: number } {
  const lo = b.min!.value;
  const hi = b.max!.value;
  if (!needsMagnifier(b, trackPx)) return { min: lo, max: hi };
  const span = trackPx * b.step;
  let start = value - span / 2;
  start = Math.min(Math.max(start, lo), hi - span);
  start = lo + Math.round((start - lo) / b.step) * b.step;
  const end = Math.min(start + span, hi);
  return { min: round(start, b.step), max: round(end, b.step) };
}

/** True when `v` is inside the bounds the range states (a side left open never fails). */
export function inBounds(v: number, b: ParamBound): boolean {
  if (b.min !== undefined && v < b.min.value) return false;
  if (b.max !== undefined && v > b.max.value) return false;
  return true;
}

/** The clinician limits whose value is outside the range its field states, as `modality.field` keys. A set with any
 *  is refused when it is saved or imported, never changed: the limit a clinician typed is the limit they meant, and
 *  holding it inside its range would save one they did not choose. An absent limit is "no limit" and never fails;
 *  a value that is not a finite number always does. */
export function limitsOutOfRange(limits: object): string[] {
  const bad: string[] = [];
  for (const [m, fields] of Object.entries(LIMIT_BOUNDS)) {
    const block = (limits as Record<string, unknown>)[m] as Record<string, unknown> | undefined;
    if (!block || typeof block !== 'object') continue;
    for (const [f, b] of Object.entries(fields)) {
      const v = block[f];
      if (v === undefined) continue;
      if (typeof v !== 'number' || !Number.isFinite(v) || !inBounds(v, b as ParamBound)) bad.push(`${m}.${f}`);
    }
  }
  return bad;
}

function round(v: number, step: number): number {
  const decimals = Math.max(0, Math.ceil(-Math.log10(step)) + 1);
  return Number(v.toFixed(decimals));
}

/** Every bound in the file, flattened, for the tests and for anyone auditing the list. */
export function allBounds(): { key: string; bound: ParamBound }[] {
  const out: { key: string; bound: ParamBound }[] = [];
  for (const [k, b] of Object.entries(INTERVAL_BOUNDS)) out.push({ key: `interval.${k}`, bound: b });
  for (const [m, fields] of Object.entries(PARAM_BOUNDS)) {
    for (const [f, b] of Object.entries(fields)) out.push({ key: `${m}.${f}`, bound: b as ParamBound });
  }
  for (const [k, b] of Object.entries(PROTOCOL_BOUNDS)) out.push({ key: `protocol.${k}`, bound: b });
  for (const [m, fields] of Object.entries(LIMIT_BOUNDS)) {
    for (const [f, b] of Object.entries(fields)) out.push({ key: `limit.${m}.${f}`, bound: b as ParamBound });
  }
  return out;
}
