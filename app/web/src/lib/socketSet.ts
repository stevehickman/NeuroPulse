// socketSet.ts — the one place socket-id validity and socket-set algebra live.
//
// Three rules kept turning up open-coded, subtly differently, in the parser, the
// eligibility engine and the config UI:
//
//   1. A socket id is valid only if it names a real socket on THIS helmet.
//      Ids are 1-based (NP_SOCKET_NUMBERING_BASE) and the count is derived from
//      tile geometry, so nothing may hardcode a bound — see socketMap.generated.
//   2. A socket set is a SET. Midline sockets belong to both hemispheres of
//      their lobe, so unioning "Frontal Left" with "Frontal Right" double-counts
//      socket 2 unless the union dedups. Duplicates silently inflate coverage
//      denominators ("4/5 sockets" when the zone has 4) and double-dose a site
//      in any per-socket iteration.
//   3. Order is ascending id, so two equal sets compare equal and UI listings
//      are stable.
//
// Everything below is pure and total: bad input is reported, never thrown at
// and never silently dropped, because the callers differ in what they do with
// it (the parser errors, the UI annotates).

import {
  NP_SOCKET_ID_MAX,
  NP_SOCKET_ID_MIN,
  isValidSocketId,
} from './socketMap.generated';

export {
  NP_SOCKET_COUNT,
  NP_SOCKET_ID_MAX,
  NP_SOCKET_ID_MIN,
  NP_SOCKET_NUMBERING_BASE,
  isValidSocketId,
} from './socketMap.generated';

/** The inclusive id range (e.g. `1–27`), for error messages and UI hints. */
export function socketRangeLabel(): string {
  return `${NP_SOCKET_ID_MIN}–${NP_SOCKET_ID_MAX}`;
}

/** Plain decimal integer, optionally signed. No hex, no exponent, no decimal point. */
const DECIMAL_INTEGER = /^[+-]?\d+$/;

/**
 * Coerce one raw value to a number for validation, STRICTLY.
 *
 * Bare `Number()` is not a gate — it maps `true`→1, `[5]`→5, `'0x10'`→16,
 * `'1e1'`→10, `null`→0 and `''`→0. Every one of those is reachable from a
 * `.npps` file, because the lexer admits booleans, identifiers and nested
 * arrays into a generic array. A socket id is a plain decimal integer or it is
 * not a socket id.
 */
function coerce(value: unknown): number {
  if (typeof value === 'number') return value;
  if (typeof value === 'string' && DECIMAL_INTEGER.test(value.trim())) {
    return Number(value.trim());
  }
  return NaN;
}

/**
 * Why a given value is not a usable socket id. `out-of-range` and `not-fitted`
 * are distinct: the first means the number cannot name a socket on any helmet
 * of this revision, the second means the lattice has a gap there. Today the
 * lattice is contiguous so `not-fitted` never fires, but the row structure is
 * allowed to leave holes and callers should not have to care.
 */
export type NPSocketIdProblem = 'not-an-integer' | 'out-of-range' | 'not-fitted';

/**
 * Classify a raw value, or `undefined` when it is a perfectly good socket id.
 * Uses the same strict coercion as `toSocketSet`, so the two never disagree
 * about the same input.
 */
export function socketIdProblem(value: unknown): NPSocketIdProblem | undefined {
  const n = coerce(value);
  if (!Number.isInteger(n)) return 'not-an-integer';
  if (n < NP_SOCKET_ID_MIN || n > NP_SOCKET_ID_MAX) return 'out-of-range';
  if (!isValidSocketId(n)) return 'not-fitted';
  return undefined;
}

/** One rejected input, with the value as written so the error can quote it. */
export interface NPInvalidSocketId {
  /** The original value, before coercion — what the author actually typed. */
  raw: unknown;
  problem: NPSocketIdProblem;
}

/** Render a rejected id the way it was written, for error messages. */
export function describeInvalid(entry: NPInvalidSocketId): string {
  return typeof entry.raw === 'string' ? JSON.stringify(entry.raw) : String(entry.raw);
}

export interface NPSocketSetResult {
  /** Valid ids, deduplicated, ascending. */
  sockets: number[];
  /**
   * Inputs that are not usable socket ids, in first-seen order. The ORIGINAL
   * value is preserved: coercing first and reporting the coerced number told
   * the author "socket NaN is not on this helmet", which names nothing they
   * can find in their file.
   */
  invalid: NPInvalidSocketId[];
  /** Valid ids that appeared more than once, in first-seen order. */
  duplicates: number[];
}

/**
 * Canonicalise raw socket ids into a set: validated, deduplicated, sorted.
 *
 * This is the single implementation behind zone parsing, zone unions and the
 * socket picker. Decimal-integer strings are accepted so callers parsing text
 * (`"4, 6, 21"`) and callers holding numbers share one path; everything else is
 * rejected with the original value preserved for the error message.
 */
export function toSocketSet(raw: Iterable<unknown>): NPSocketSetResult {
  const seen = new Set<number>();
  const invalidSeen = new Set<string>();
  const sockets: number[] = [];
  const invalid: NPInvalidSocketId[] = [];
  const duplicates: number[] = [];

  for (const value of raw) {
    const problem = socketIdProblem(value);
    if (problem !== undefined) {
      // Key on the rendered original so two distinct bad tokens stay distinct —
      // collapsing them to one `NaN` hid half the errors in a file.
      const key = `${typeof value}:${String(value)}`;
      if (!invalidSeen.has(key)) {
        invalidSeen.add(key);
        invalid.push({ raw: value, problem });
      }
      continue;
    }
    const n = coerce(value);
    if (seen.has(n)) {
      if (!duplicates.includes(n)) duplicates.push(n);
      continue;
    }
    seen.add(n);
    sockets.push(n);
  }

  sockets.sort((a, b) => a - b);
  return { sockets, invalid, duplicates };
}

/**
 * Union of any number of socket lists — deduplicated and sorted.
 *
 * THE call to reach for whenever more than one zone contributes sockets. A
 * bilateral protocol referencing "Frontal Left" + "Frontal Right" must dose the
 * midline socket once, not twice.
 */
export function unionSockets(...lists: ReadonlyArray<Iterable<number>>): number[] {
  const merged: number[] = [];
  for (const list of lists) merged.push(...list);
  return toSocketSet(merged).sockets;
}

/** Union of the socket lists of several zones. Convenience over `unionSockets`. */
export function unionZoneSockets(
  zones: Iterable<{ readonly sockets: readonly number[] }>,
): number[] {
  return unionSockets(...[...zones].map(z => z.sockets));
}
