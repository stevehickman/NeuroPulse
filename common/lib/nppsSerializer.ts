/**
 * NPPS serialization, for the web app and the simulator: a thin layer over the shared NPPS core
 * (common/npps-core, OI-NPPS-CORE-01), loaded as WebAssembly (nppsCore.ts).
 *
 * This file used to be a 430-line hand-written writer, one of four that drifted from the parser (the iOS and
 * Android writers each had their own, and it wrote `1h` and `max_irradiance_m_wcm2`, spellings the parser does
 * not read). There is one serializer now, and it is in Rust: what the text looks like, and what it refuses to
 * write, is decided there and is the same on every runtime. What is left here is the part that is not
 * language: handing the core the app's models in its shape.
 *
 * In a browser, `await initNppsCore()` (nppsCore.ts) once before the first call; Node needs nothing.
 */

import type {
  NPCompositeProtocol,
  NPConditionDefinition,
  NPProtocolDefinition,
  NPProtocolEntry,
  NPZoneDefinition,
} from '../types/protocol';
import type { NPLimitsSet } from '../types/limits';
import type { NPWavelengthRules } from './wavelengthRules';
import { nppsSerialize } from './nppsCore';

/**
 * The core's shape of an entry. A modality is `{type, params, interval, enabled}`, not the app's
 * `{id, modalityParams: {type, params}, interval, enabled}`; ids and timestamps are the model's own.
 */
export function coreEntry(e: NPProtocolEntry): object {
  if (e.kind === 'composite') return { kind: 'composite', composite: e.composite };
  return {
    kind: 'single',
    protocol: {
      ...e.protocol,
      modalities: e.protocol.modalities.map((m) => ({
        type: m.modalityParams.type,
        params: m.modalityParams.params,
        interval: m.interval,
        enabled: m.enabled,
      })),
    },
  };
}

/** One protocol or composite as `.npps` text. Throws NppsRefusal for a model that cannot be written. */
export function serializeProtocol(entry: NPProtocolEntry): string {
  return nppsSerialize([coreEntry(entry)]);
}

/** Protocols and composites as one `.npps` file: blocks separated by a blank line. */
export function serializeNPPS(entries: NPProtocolEntry[]): string {
  return nppsSerialize(entries.map(coreEntry));
}

/**
 * A zone. Its socket list is canonicalised on the way out, and one that cannot be written (an id that is not a
 * socket on this helmet) is refused rather than serialized into a file the parser rejects.
 */
export function serializeZone(zone: NPZoneDefinition): string {
  return nppsSerialize([{ kind: 'zone', zone }]);
}

export function serializeCondition(condition: NPConditionDefinition): string {
  return nppsSerialize([{ kind: 'condition', condition }]);
}

/** A limits set as a `limits` block; `parseNPPSLimits` reads it back unchanged. */
export function serializeNPPSLimits(limits: NPLimitsSet): string {
  return nppsSerialize([{ kind: 'limits', limits }]);
}

/** A wavelength rule set as a `wavelength_rules` block (NP-NPPS-REF-001 §7a). */
export function serializeWavelengthRules(rules: NPWavelengthRules): string {
  return nppsSerialize([{ kind: 'wavelengthRules', wavelengthRules: rules }]);
}

export type { NPProtocolDefinition, NPCompositeProtocol };
