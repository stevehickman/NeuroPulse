// ─── PBM wavelength mapping rules ─────────────────────────────────────────────
//
// A protocol states the wavelength its source used (NP-NPPS-REF-001 §4.1a):
// `wavelength: "810nm"`. The helmet has a small, fixed set of emitter channels
// (element types led_660 / led_808 / led_1064). These rules say which channel,
// if any, may deliver a requested wavelength. They are CONFIGURATION, not
// physics: the defaults ship in protocols/predefined/00-wavelength-rules.npps,
// and a user may loosen or tighten them (a researcher may want 850 nm accepted
// on the 808 channel; another may want only exact matches).
//
// Two rules the whole feature rests on, both principal decisions (2026-09-29):
//   - Each wavelength is independently controlled, but not independent for
//     safety (CLAUDE.md §3). Mapping chooses a channel; it never relaxes a
//     safety term, which is evaluated on the channel actually driven.
//   - A PBM ceiling refuses a protocol and never reshapes it. Here that means a
//     wavelength no rule accepts is NOT delivered on the nearest channel: the
//     protocol is ineligible, and the compiler refuses it.
//
// Privacy: nothing here concerns a person. Rules are device/user configuration.

import type { NPElementType } from './helmetInventory';

/** The emitter element types a transcranial PBM block can be mapped onto. */
export type NPPBMChannelElement = 'led_660' | 'led_808' | 'led_1064';

export const PBM_CHANNEL_ELEMENTS: readonly NPPBMChannelElement[] = ['led_660', 'led_808', 'led_1064'];

/** One channel's acceptance window: requested wavelengths in [minNm, maxNm] may be delivered on it. */
export interface NPWavelengthChannelRule {
  element: NPPBMChannelElement;
  /** The channel's nominal emission wavelength. Used only to break a tie between two accepting channels. */
  nominalNm: number;
  minNm: number;
  maxNm: number;
}

export type NPWavelengthRulesLevel = 'global' | 'user';

export interface NPWavelengthRules {
  name: string;
  level: NPWavelengthRulesLevel;
  description?: string;
  /** At most one rule per element. A channel with no rule accepts nothing. */
  channels: NPWavelengthChannelRule[];
}

/**
 * The shipped defaults. Mirrors protocols/predefined/00-wavelength-rules.npps,
 * which is the authored copy; wavelengthRules.test.ts fails if the two drift.
 *
 * Each window is the channel's band in CLAUDE.md §3 (660–670 nm, 808–830 nm,
 * 1064 nm) widened by 10 nm each side. 10 nm is roughly half the FWHM of a red
 * or NIR LED, so a requested centroid inside the window still falls within the
 * emitter's half-maximum spectrum. It is an UNVALIDATED DEFAULT, editable by
 * design, and it is not a safety parameter.
 */
export const DEFAULT_WAVELENGTH_RULES: NPWavelengthRules = {
  name: 'NeurOne default wavelength mapping',
  level: 'global',
  channels: [
    { element: 'led_660', nominalNm: 660, minNm: 650, maxNm: 680 },
    { element: 'led_808', nominalNm: 808, minNm: 798, maxNm: 840 },
    { element: 'led_1064', nominalNm: 1064, minNm: 1054, maxNm: 1074 },
  ],
};

/**
 * Merge user rules over defaults, channel by channel: a user rule for an element
 * replaces the default rule for that element; elements the user did not touch
 * keep the default. Returns a new object; never mutates either input.
 */
export function resolveWavelengthRules(
  defaults: NPWavelengthRules,
  user?: NPWavelengthRules | null,
): NPWavelengthRules {
  if (!user) return { ...defaults, channels: defaults.channels.map(c => ({ ...c })) };
  const byElement = new Map(defaults.channels.map(c => [c.element, { ...c }]));
  for (const c of user.channels) byElement.set(c.element, { ...c });
  return {
    name: user.name,
    level: 'user',
    description: user.description,
    channels: PBM_CHANNEL_ELEMENTS.filter(e => byElement.has(e)).map(e => byElement.get(e)!),
  };
}

/** Problems with a rule set, as human-readable strings (empty = valid). */
export function validateWavelengthRules(rules: NPWavelengthRules): string[] {
  const errors: string[] = [];
  const seen = new Set<NPPBMChannelElement>();
  for (const c of rules.channels) {
    if (!PBM_CHANNEL_ELEMENTS.includes(c.element)) {
      errors.push(`unknown channel '${String(c.element)}'`);
      continue;
    }
    if (seen.has(c.element)) errors.push(`channel '${c.element}' has more than one rule`);
    seen.add(c.element);
    for (const [k, v] of [['nominal_nm', c.nominalNm], ['min_nm', c.minNm], ['max_nm', c.maxNm]] as const) {
      if (!Number.isFinite(v) || v <= 0) errors.push(`channel '${c.element}': ${k} must be a positive number`);
    }
    if (c.minNm > c.maxNm) errors.push(`channel '${c.element}': min_nm ${c.minNm} is above max_nm ${c.maxNm}`);
  }
  return errors;
}

// ─── The `wavelength` field ────────────────────────────────────────────────────

/**
 * The three multi-channel values the language had before per-wavelength blocks.
 * They name channels, not wavelengths, and are kept so existing protocols keep
 * their meaning. New protocols name one wavelength per block.
 */
export const LEGACY_PBM_WAVELENGTHS: Record<string, readonly NPPBMChannelElement[]> = {
  '660_808nm': ['led_660', 'led_808'],
  '1064nm': ['led_1064'],
  '660_808_1064nm': ['led_660', 'led_808', 'led_1064'],
};

export type NPParsedWavelength =
  | { kind: 'legacy'; value: string; elements: readonly NPPBMChannelElement[] }
  | { kind: 'single'; value: string; nm: number }
  | { kind: 'invalid'; value: string };

const SINGLE_NM = /^([0-9]+(?:\.[0-9]+)?)nm$/;

/**
 * Classify a `wavelength` value. `"1064nm"` is a legacy channel name AND looks
 * like a single wavelength; it keeps its legacy meaning (drive CH_C), which is
 * also what a single 1064 nm request maps to under any rule set that accepts it.
 */
export function parsePbmWavelength(value: string): NPParsedWavelength {
  const legacy = LEGACY_PBM_WAVELENGTHS[value];
  if (legacy) return { kind: 'legacy', value, elements: legacy };
  const m = SINGLE_NM.exec(value);
  if (m) {
    const nm = Number(m[1]);
    if (Number.isFinite(nm) && nm > 0) return { kind: 'single', value, nm };
  }
  return { kind: 'invalid', value };
}

/**
 * The channel that delivers `nm` under `rules`, or null when no rule accepts it.
 * Several windows may overlap after a user edit; the channel whose nominal is
 * nearest wins, and a tie goes to the channel listed first in
 * PBM_CHANNEL_ELEMENTS, so the answer never depends on rule order.
 */
export function mapWavelength(nm: number, rules: NPWavelengthRules): NPPBMChannelElement | null {
  let best: NPWavelengthChannelRule | null = null;
  for (const element of PBM_CHANNEL_ELEMENTS) {
    const rule = rules.channels.find(c => c.element === element);
    if (!rule || nm < rule.minNm || nm > rule.maxNm) continue;
    if (!best || Math.abs(nm - rule.nominalNm) < Math.abs(nm - best.nominalNm)) best = rule;
  }
  return best ? best.element : null;
}

export type NPPbmChannelResolution =
  | { ok: true; elements: readonly NPPBMChannelElement[]; requestedNm?: number }
  | { ok: false; reason: 'invalid' | 'unmapped'; value: string; requestedNm?: number };

/** Which channels a `wavelength` value drives under `rules`. */
export function resolvePbmChannels(value: string, rules: NPWavelengthRules): NPPbmChannelResolution {
  const w = parsePbmWavelength(value);
  if (w.kind === 'legacy') return { ok: true, elements: w.elements };
  if (w.kind === 'invalid') return { ok: false, reason: 'invalid', value };
  const element = mapWavelength(w.nm, rules);
  return element
    ? { ok: true, elements: [element], requestedNm: w.nm }
    : { ok: false, reason: 'unmapped', value, requestedNm: w.nm };
}

/**
 * The element requirement groups a PBM block places on each socket it targets
 * (the form protocolEligibility uses: groups AND-ed, alternatives OR-ed within).
 * Null when the wavelength is invalid or no rule accepts it — no module can
 * satisfy it, which is a different failure from "the fitted module is wrong".
 */
export function pbmRequirementGroups(
  value: string,
  rules: NPWavelengthRules,
): NPElementType[][] | null {
  const r = resolvePbmChannels(value, rules);
  return r.ok ? r.elements.map(e => [e as NPElementType]) : null;
}
