/**
 * NPPS parsing, for the web app and the simulator: a thin layer over the shared NPPS core
 * (common/npps-core, OI-NPPS-CORE-01), loaded as WebAssembly (nppsCore.ts).
 *
 * This file used to be a 1,900-line hand-written lexer and parser, one of five that drifted. There is one
 * parser now, and it is in Rust: what an `.npps` file means, and what refuses it, is decided there and is
 * the same on every runtime. What is left here is the part that is not language: building the app's
 * models from the core's JSON, which means supplying what the core leaves out because it is not the same
 * twice (a random id for a protocol that states none, the clock for `createdAt`).
 *
 * In a browser, `await initNppsCore()` (nppsCore.ts) once before the first parse; Node needs nothing.
 */

import type {
  NPProtocolEntry,
  NPProtocolDefinition,
  NPCompositeProtocol,
  NPProtocolModality,
  NPZoneDefinition,
  NPConditionDefinition,
  NPNamespace,
} from '../types/protocol';
import type { NPLimitsSet } from '../types/limits';
import type { NPWavelengthRules } from './wavelengthRules';
import { NppsRefusal, nppsNamespace, nppsParse, type NppsParsed } from './nppsCore';

// ─── Error ─────────────────────────────────────────────────────────────────────

/** The core's refusal, as the error every caller already catches. `message` is `Line N: …` when it names a line. */
export class NPPSParseError extends Error {
  line?: number;
  constructor(message: string, line?: number) {
    super(line != null ? `Line ${line}: ${message}` : message);
    this.name = 'NPPSParseError';
    this.line = line;
  }
}

const LINE_PREFIX = /^Line (\d+): ([\s\S]*)$/;

function refusal(e: NppsRefusal): NPPSParseError {
  const m = LINE_PREFIX.exec(e.message);
  return m ? new NPPSParseError(m[2]!, Number(m[1])) : new NPPSParseError(e.message);
}

function core(text: string): NppsParsed {
  try {
    return nppsParse(text);
  } catch (e) {
    throw e instanceof NppsRefusal ? refusal(e) : e;
  }
}

// ─── Core JSON → models ────────────────────────────────────────────────────────

/* eslint-disable @typescript-eslint/no-explicit-any */

function protocolOf(p: any, now: string): NPProtocolDefinition {
  const modalities: NPProtocolModality[] = p.modalities.map((m: any) => ({
    id: crypto.randomUUID(),
    modalityParams: { type: m.type, params: m.params },
    interval: m.interval,
    enabled: m.enabled,
  }));
  return { ...p, id: p.id ?? crypto.randomUUID(), createdAt: now, modifiedAt: now, modalities };
}

function compositeOf(c: any, now: string): NPCompositeProtocol {
  return {
    ...c,
    id: c.id ?? crypto.randomUUID(),
    createdAt: now,
    modifiedAt: now,
    layers: c.layers.map((l: any) => ({ id: crypto.randomUUID(), ...l })),
  };
}

function entriesOf(parsed: NppsParsed, now: string): NPProtocolEntry[] {
  return parsed.entries.map((e) =>
    e.kind === 'single'
      ? { kind: 'single', protocol: protocolOf(e.protocol, now) }
      : { kind: 'composite', composite: compositeOf(e.composite, now) },
  );
}

function limitsOf(l: any, now: string): NPLimitsSet {
  return { id: crypto.randomUUID(), createdAt: now, modifiedAt: now, ...l };
}

// ─── Public API ────────────────────────────────────────────────────────────────

/** Parse NPPS text into its protocol and composite entries. */
export function parseNPPS(text: string): NPProtocolEntry[] {
  return entriesOf(core(text), new Date().toISOString());
}

/**
 * Parse a single NPPS file into its entries plus the zone, condition and wavelength-rule definitions it
 * declares. Use `buildNamespace` to fold several files' results into one shared namespace (all files
 * under the protocol directory tree share ONE namespace — see NP-NPPS-REF-001 §1.6).
 */
export function parseNPPSFile(text: string): {
  entries: NPProtocolEntry[];
  zones: NPZoneDefinition[];
  conditions: NPConditionDefinition[];
  wavelengthRules: NPWavelengthRules[];
} {
  const parsed = core(text);
  return {
    entries: entriesOf(parsed, new Date().toISOString()),
    zones: parsed.zones,
    conditions: parsed.conditions,
    wavelengthRules: parsed.wavelengthRules,
  };
}

/** Parse the first `limits` block of the given NPPS text. Returns null if there is none. */
export function parseNPPSLimits(text: string): NPLimitsSet | null {
  const first = core(text).limits[0];
  return first === undefined ? null : limitsOf(first, new Date().toISOString());
}

/** The core's shape of an entry: a modality is `{type, params, …}`, not `{modalityParams: {type, params}, …}`. */
function coreEntry(e: NPProtocolEntry): any {
  if (e.kind === 'composite') return { kind: 'composite', composite: e.composite };
  return {
    kind: 'single',
    protocol: {
      ...e.protocol,
      modalities: e.protocol.modalities.map((m) => ({ type: m.modalityParams.type, params: m.modalityParams.params })),
    },
  };
}

/**
 * Build one namespace from several parsed NPPS files. Zones and conditions are keyed by name and a name is
 * defined exactly once across the whole tree (NP-NPPS-REF-001 §1.6).
 *
 * A name defined by two files is an ERROR, not a last-write-wins warning: the tree is read recursively and
 * nothing guarantees a stable traversal order across platforms, so "later" is not a property this function
 * has. A collision leaves the name UNBOUND — neither definition wins — and is reported in `errors`; any
 * protocol referencing it then fails `validateNamespaceReferences` as if the name had never been defined.
 * That is the only outcome that is the same on every runtime whatever the read order.
 */
export function buildNamespace(
  files: Array<{ entries: NPProtocolEntry[]; zones: NPZoneDefinition[]; conditions: NPConditionDefinition[] }>,
): { namespace: NPNamespace; errors: string[] } {
  const ns = nppsNamespace(
    files.map((f) => ({ entries: f.entries.map(coreEntry), zones: f.zones, conditions: f.conditions })),
  );
  // The core decides which names survive; the namespace holds the caller's own objects for them.
  const original = <T extends { name: string }>(lists: T[][], name: string): T | undefined => {
    for (const list of lists) {
      const found = list.find((d) => d.name === name);
      if (found) return found;
    }
    return undefined;
  };
  const zoneLists = files.map((f) => f.zones);
  const conditionLists = files.map((f) => f.conditions);
  return {
    namespace: {
      entries: files.flatMap((f) => f.entries),
      zones: new Map(ns.zones.map((z: NPZoneDefinition) => [z.name, original(zoneLists, z.name) ?? z])),
      conditions: new Map(
        ns.conditions.map((c: NPConditionDefinition) => [c.name, original(conditionLists, c.name) ?? c]),
      ),
    },
    errors: ns.errors,
  };
}

/**
 * Validate cross-references within a namespace: every protocol/composite `conditions` entry must resolve to
 * a condition definition, and every pbm_transcranial `zoneRefs` entry must resolve to a zone definition.
 * Returns the list of unresolved-reference errors (empty = all references resolve).
 */
export function validateNamespaceReferences(ns: NPNamespace): string[] {
  return nppsNamespace([
    { entries: ns.entries.map(coreEntry), zones: [...ns.zones.values()], conditions: [...ns.conditions.values()] },
  ]).referenceErrors;
}
