// Protocol eligibility against the fitted helmet.
//
// Answers two questions the configuration UI asks constantly:
//
//   1. Can this predefined protocol run on the helmet as currently fitted?
//   2. If not, WHICH sockets are wrong and WHAT module does each one need?
//
// (2) is the requirement that shapes everything here. A boolean "unavailable"
// is useless to someone holding a helmet and a box of modules, so every failure
// carries the socket ids and the candidate part numbers that would fix them.
//
// Coverage policy: a zone with SOME of the required modules is offered, with a
// coverage figure, not hidden. Hiding a zone whose modules the user can plainly
// see fitted reads as a bug. Only ZERO coverage disqualifies.

import type {
  NPProtocolEntry,
  NPProtocolDefinition,
  NPProtocolModality,
  NPModalityTypeId,
  NPZoneDefinition,
} from '../types/protocol';
import {
  MODULE_TYPES,
  ELEMENT_TYPE_LABEL,
  modulesProviding,
  type NPElementType,
  type NPHelmetInventory,
  type NPModuleType,
} from './helmetInventory';
import { toSocketSet, unionSockets, unionZoneSockets } from './socketSet';

// ─── Modality requirements ─────────────────────────────────────────────────────

/**
 * What a modality needs from a helmet socket.
 *
 * `anyOf` groups are alternatives-within, AND-ed across: a modality needing
 * `[[led_660], [led_808]]` requires a socket providing 660 AND a socket
 * providing 808. A dual Ag/AgCl electrode appears in both the EEG and tES
 * groups because one part satisfies either need.
 *
 * `socketBased: false` marks modalities delivered by accessories rather than
 * helmet sockets (intranasal probe, auricular clip, goggles, audio cups, TMS
 * coil). They are never a reason to disable a protocol on socket grounds.
 */
export interface NPModalityRequirement {
  socketBased: boolean;
  requires: NPElementType[][];
}

export const MODALITY_REQUIREMENTS: Record<NPModalityTypeId, NPModalityRequirement> = {
  pbm_transcranial: { socketBased: true, requires: [['led_660'], ['led_808']] },
  pbm_deep_1170nm: { socketBased: true, requires: [['led_1170']] },
  eeg_neurofeedback: { socketBased: true, requires: [['eeg_electrode', 'dual_electrode']] },
  qeeg_21ch: { socketBased: true, requires: [['eeg_electrode', 'dual_electrode']] },
  bes_tacs: { socketBased: true, requires: [['tes_electrode', 'dual_electrode']] },
  tdcs: { socketBased: true, requires: [['tes_electrode', 'dual_electrode']] },
  clinical_tacs: { socketBased: true, requires: [['tes_electrode', 'dual_electrode']] },
  // HD-tDCS specifically needs the dual-rated Ag/AgCl part: the 4x1 ring records
  // and stimulates through the same electrode (CLAUDE.md §3, T2 additions).
  hd_tdcs: { socketBased: true, requires: [['dual_electrode']] },

  // Accessory-delivered — not fitted into helmet sockets.
  pbm_intranasal: { socketBased: false, requires: [] },
  vns_hrv: { socketBased: false, requires: [] },
  cervical_vns: { socketBased: false, requires: [] },
  audio_entrainment: { socketBased: false, requires: [] },
  visual_stimulation: { socketBased: false, requires: [] },
  tms: { socketBased: false, requires: [] },
  vibrotactile_40hz: { socketBased: false, requires: [] },
};

// ─── Coverage ──────────────────────────────────────────────────────────────────

export interface NPZoneCoverage {
  zoneName: string;
  /** Sockets in the zone that satisfy every requirement group. */
  satisfied: number[];
  /** Sockets in the zone that do not. */
  missing: number[];
  /** Sockets referenced by the zone that are not valid on this helmet. */
  invalid: number[];
  /** satisfied + missing. Excludes `invalid` — an unaddressable socket is not
   *  coverage the user can do anything about. */
  total: number;
}

function makeCoverage(
  zoneName: string,
  satisfied: number[],
  missing: number[],
  invalid: number[],
): NPZoneCoverage {
  return { zoneName, satisfied, missing, invalid, total: satisfied.length + missing.length };
}

export function coverageFraction(c: NPZoneCoverage): number {
  return c.total === 0 ? 0 : c.satisfied.length / c.total;
}

/** `7/11 sockets` — the string the zone dropdown shows beside a partial zone. */
export function coverageLabel(c: NPZoneCoverage): string {
  return `${c.satisfied.length}/${c.total} sockets`;
}

/**
 * How well `zone` supports `modality` given the fitted inventory.
 *
 * Zones name sockets; sockets hold modules; modules provide elements. A socket
 * counts as satisfied only when it covers EVERY requirement group, because a
 * PBM zone needs both wavelengths at the same site, not 660 here and 808 there.
 */
export function zoneCoverageFor(
  zone: NPZoneDefinition,
  modality: NPModalityTypeId,
  inventory: NPHelmetInventory,
): NPZoneCoverage {
  const requirement = MODALITY_REQUIREMENTS[modality];
  const satisfied: number[] = [];
  const missing: number[] = [];

  // Zones parsed from .npps are already canonical, but a zone can also be built
  // in memory (the config UI, tests, migrations), so canonicalise here rather
  // than trusting the caller — a repeated socket would otherwise count twice
  // against `total`. Ids that name no socket on this helmet are REPORTED as
  // invalid, never dropped: an unaddressable id is exactly what the operator
  // needs told about.
  const canonical = toSocketSet(zone.sockets);
  const invalid = canonical.invalid
    .map(e => Number(e.raw))
    .filter(n => Number.isFinite(n));

  for (const socketId of canonical.sockets) {
    // Distinct from the above: this id IS a socket, it just has no module
    // fitted. Both end up in `invalid` for the UI, but the id is real.
    if (inventory.at(socketId) === undefined) {
      invalid.push(socketId);
      continue;
    }
    const ok = requirement.requires.every(group => inventory.satisfies(socketId, group));
    (ok ? satisfied : missing).push(socketId);
  }

  return makeCoverage(zone.name, satisfied, missing, invalid);
}

/**
 * The sockets a modality actually addresses: the DEDUPLICATED union of every
 * zone it targets.
 *
 * Per-zone coverage is reported separately because the operator needs to see
 * which zone is short. But the set of sockets that will be driven is a union,
 * and it must not repeat: "Frontal Left" + "Frontal Right" share midline socket
 * 2, and any left/right zone pair that spans the centre column shares its
 * midline sockets the same way. Dosing, socket counts and per-socket iteration
 * all read this, never a concatenation.
 */
export function targetSocketsFor(
  modality: NPProtocolModality,
  namespace: ReadonlyMap<string, NPZoneDefinition>,
): number[] {
  return unionZoneSockets(targetZones(modality, namespace).zones);
}

/**
 * Zones offerable for `modality`, each with its coverage.
 *
 * Per the coverage policy, a zone appears whenever at least one socket supports
 * the modality. Zones with zero support are excluded — offering "Occipital Left
 * (0/10)" for PBM when no LED is fitted there is noise, not information.
 */
export function zonesForModality(
  zones: Iterable<NPZoneDefinition>,
  modality: NPModalityTypeId,
  inventory: NPHelmetInventory,
): NPZoneCoverage[] {
  const out: NPZoneCoverage[] = [];
  for (const zone of zones) {
    const coverage = zoneCoverageFor(zone, modality, inventory);
    if (coverage.satisfied.length > 0) out.push(coverage);
  }
  return out.sort(
    (a, b) => coverageFraction(b) - coverageFraction(a) || a.zoneName.localeCompare(b.zoneName),
  );
}

// ─── Eligibility ───────────────────────────────────────────────────────────────

/** One socket that blocks a protocol, and what would unblock it. */
export interface NPSocketShortfall {
  socketId: number;
  /** Part number currently fitted, or undefined when the socket is empty. */
  fitted?: string;
  /** Element groups this socket fails to provide. */
  missingElements: NPElementType[][];
  /** Modules that would satisfy every missing group at once, if any single one can. */
  candidateModules: string[];
}

export interface NPModalityShortfall {
  modality: NPModalityTypeId;
  /** Zones the protocol asked for that have no usable socket at all. */
  unsupportedZones: string[];
  /** Zone references that name a zone absent from the NPPS namespace. */
  unresolvedZones: string[];
  /** Per-socket detail, capped by the caller for display. */
  sockets: NPSocketShortfall[];
  /** Coverage per requested zone, including partially-covered ones. */
  coverage: NPZoneCoverage[];
}

export interface NPEligibility {
  eligible: boolean;
  /** True when the protocol runs, but on fewer sockets than it asks for. */
  degraded: boolean;
  /**
   * Modalities whose target is patient-specific and still UNCHOSEN. Non-empty
   * means the protocol is blocked pending targeting — a different state from
   * "the hardware is wrong", which is what `shortfalls` describes.
   */
  requiresTargeting: NPModalityTypeId[];
  /**
   * Modalities that use clinician-selected targeting at all, whether or not a
   * selection has been made yet. The UI keeps the target visible and editable
   * once chosen — a perilesional site is a clinical decision the operator must
   * be able to review, not a one-way gate that disappears on first click.
   */
  clinicianTargeted: NPModalityTypeId[];
  shortfalls: NPModalityShortfall[];
  /** One-line reason for the disabled state, or '' when eligible. */
  summary: string;
}

/**
 * Which zones a modality targets.
 *
 * Only `named` + `zoneRefs` is a real zone reference. `all` means every zone in
 * the namespace. The legacy `front`/`rear`/`custom` numeric forms predate named
 * zones and cannot be resolved against socket ids, so they are reported as
 * unresolved rather than silently treated as full coverage — every zone
 * reference must resolve to an NPPS-defined zone (NP-CFG-UI-001).
 */
function targetZones(
  modality: NPProtocolModality,
  namespace: ReadonlyMap<string, NPZoneDefinition>,
): { zones: NPZoneDefinition[]; unresolved: string[] } {
  // Only some modality param shapes carry zone selection; the rest target the
  // whole helmet implicitly.
  const params = modality.modalityParams.params as {
    zones?: string;
    zoneRefs?: string[];
  };
  const selection = params.zones;

  if (selection === 'named') {
    const zones: NPZoneDefinition[] = [];
    const unresolved: string[] = [];
    for (const name of params.zoneRefs ?? []) {
      const zone = namespace.get(name);
      if (zone) zones.push(zone);
      else unresolved.push(name);
    }
    return { zones, unresolved };
  }

  if (selection === 'all' || selection === undefined) {
    return { zones: [...namespace.values()], unresolved: [] };
  }

  return { zones: [], unresolved: [`legacy '${selection}' selection — not an NPPS zone`] };
}

function shortfallFor(
  socketId: number,
  requirement: NPModalityRequirement,
  inventory: NPHelmetInventory,
): NPSocketShortfall {
  const fitted = inventory.at(socketId);
  const missingElements = requirement.requires.filter(
    group => !inventory.satisfies(socketId, group),
  );

  // A candidate is a module that covers EVERY missing group — swapping one
  // module is the actionable fix; listing parts that each fix half the problem
  // would send someone to buy two modules for one socket.
  const candidateModules = Object.values(MODULE_TYPES)
    .filter((m: NPModuleType) =>
      missingElements.every(group => group.some(e => m.elements.includes(e))),
    )
    .map(m => m.partNumber);

  return {
    socketId,
    fitted: fitted?.present ? fitted.partNumber : undefined,
    missingElements,
    candidateModules,
  };
}

/**
 * Evaluate one protocol against the fitted helmet.
 *
 * Accessory-delivered modalities are skipped: a missing goggle set is a real
 * problem but not a socket problem, and the existing tier-based availability
 * check already covers it.
 */
export function evaluateProtocol(
  protocol: NPProtocolDefinition,
  inventory: NPHelmetInventory,
  namespace: ReadonlyMap<string, NPZoneDefinition>,
  /**
   * Operator-chosen sockets, per modality, for `clinician_selected` targets.
   * A modality only clears targeting once a non-empty selection is supplied.
   */
  targeting?: ReadonlyMap<NPModalityTypeId, readonly number[]>,
): NPEligibility {
  const shortfalls: NPModalityShortfall[] = [];
  const requiresTargeting: NPModalityTypeId[] = [];
  const clinicianTargeted: NPModalityTypeId[] = [];
  let degraded = false;

  for (const modality of protocol.modalities) {
    const type = modality.modalityParams.type;
    const requirement = MODALITY_REQUIREMENTS[type];
    if (!requirement?.socketBased) continue;

    // Patient-specific target: no predefined zone can be correct, so the
    // protocol stays blocked until an operator supplies sockets.
    if (isClinicianSelected(modality)) {
      clinicianTargeted.push(type);
      const chosen = targeting?.get(type) ?? [];
      if (chosen.length === 0) {
        requiresTargeting.push(type);
        continue;
      }
      const coverage = coverageForSockets(`Clinician selection`, chosen, type, inventory);
      if (coverage.satisfied.length === 0 || coverage.missing.length > 0) {
        if (coverage.satisfied.length > 0) degraded = true;
        shortfalls.push({
          modality: type,
          unsupportedZones: coverage.satisfied.length === 0 ? [coverage.zoneName] : [],
          unresolvedZones: [],
          coverage: [coverage],
          sockets: coverage.missing.map(id => shortfallFor(id, requirement, inventory)),
        });
      }
      continue;
    }

    const { zones, unresolved } = targetZones(modality, namespace);
    const coverage = zones.map(z => zoneCoverageFor(z, type, inventory));

    const unsupportedZones = coverage.filter(c => c.satisfied.length === 0).map(c => c.zoneName);
    const partial = coverage.filter(c => c.satisfied.length > 0 && c.missing.length > 0);
    if (partial.length > 0) degraded = true;

    if (unsupportedZones.length === 0 && unresolved.length === 0 && partial.length === 0) {
      continue;
    }

    // Report sockets from the zones that fail outright, plus the gaps in
    // partially-covered zones — those are exactly the sockets to re-fit.
    const blockingSockets = coverage
      .filter(c => c.satisfied.length === 0 || c.missing.length > 0)
      .flatMap(c => c.missing);

    shortfalls.push({
      modality: type,
      unsupportedZones,
      unresolvedZones: unresolved,
      coverage,
      sockets: unionSockets(blockingSockets).map(id =>
        shortfallFor(id, requirement, inventory),
      ),
    });
  }

  const blocking = shortfalls.filter(
    s => s.unsupportedZones.length > 0 || s.unresolvedZones.length > 0,
  );
  const eligible = blocking.length === 0 && requiresTargeting.length === 0;

  return {
    eligible,
    degraded: eligible && degraded,
    requiresTargeting,
    clinicianTargeted,
    shortfalls,
    summary:
      requiresTargeting.length > 0
        ? targetingSummary(requiresTargeting)
        : eligible ? '' : summarize(blocking),
  };
}

/** True when this modality defers its target to the operator. */
function isClinicianSelected(modality: NPProtocolModality): boolean {
  return (modality.modalityParams.params as { zones?: string }).zones === 'clinician_selected';
}

/** Coverage over an explicit socket list rather than a named zone. */
function coverageForSockets(
  label: string,
  sockets: readonly number[],
  modality: NPModalityTypeId,
  inventory: NPHelmetInventory,
): NPZoneCoverage {
  const requirement = MODALITY_REQUIREMENTS[modality];
  const satisfied: number[] = [];
  const missing: number[] = [];

  // Operator selections arrive straight from the picker and may repeat a socket;
  // canonicalising first keeps the coverage denominator honest (a site chosen
  // twice is one site) and routes bad ids to `invalid` the same way zone parsing
  // does.
  const canonical = toSocketSet(sockets);
  const invalid = canonical.invalid
    .map(e => Number(e.raw))
    .filter(n => Number.isFinite(n));

  for (const socketId of canonical.sockets) {
    if (inventory.at(socketId) === undefined) {
      invalid.push(socketId);
      continue;
    }
    const ok = requirement.requires.every(group => inventory.satisfies(socketId, group));
    (ok ? satisfied : missing).push(socketId);
  }
  return makeCoverage(label, satisfied, missing, invalid);
}

function targetingSummary(modalities: NPModalityTypeId[]): string {
  const names = modalities.join(', ');
  return `Target must be selected for this patient (${names}) — this protocol treats an individual site, so no preset zone applies`;
}

function summarize(blocking: NPModalityShortfall[]): string {
  const first = blocking[0];

  if (first.unresolvedZones.length > 0) {
    return `References a zone that is not defined: ${first.unresolvedZones[0]}`;
  }

  const socketCount = unionSockets(
    ...blocking.map(s => s.sockets.map(x => x.socketId)),
  ).length;
  const zoneList = first.unsupportedZones.slice(0, 2).join(', ');
  const more = first.unsupportedZones.length > 2 ? ` +${first.unsupportedZones.length - 2} more` : '';

  return `No compatible module in ${zoneList}${more} — ${socketCount} socket${socketCount === 1 ? '' : 's'} need re-fitting`;
}

/** Convenience wrapper for a library entry of either kind. */
export function evaluateEntry(
  entry: NPProtocolEntry,
  inventory: NPHelmetInventory | null,
  namespace: ReadonlyMap<string, NPZoneDefinition>,
): NPEligibility {
  if (!inventory) {
    return {
      eligible: false, degraded: false, requiresTargeting: [], clinicianTargeted: [],
      shortfalls: [], summary: 'No helmet connected',
    };
  }
  if (entry.kind === 'single') {
    return evaluateProtocol(entry.protocol, inventory, namespace);
  }

  // A composite is eligible when every layer it references is eligible. Layers
  // name protocols, so this is resolved by the caller passing single protocols;
  // with no resolver available here the composite is treated as eligible and the
  // per-layer check happens when the layer is expanded.
  return {
    eligible: true, degraded: false, requiresTargeting: [], clinicianTargeted: [],
    shortfalls: [], summary: '',
  };
}

/** Human-readable "Socket 34: fitted ZM-EEG, needs 660nm LED + 808nm LED — fit ZM-PBM-DUAL". */
export function describeShortfall(s: NPSocketShortfall): string {
  const needs = s.missingElements
    .map(group => group.map(e => ELEMENT_TYPE_LABEL[e] ?? e).join(' or '))
    .join(' + ');
  const fitted = s.fitted ? `fitted ${s.fitted}` : 'empty';
  const fix = s.candidateModules.length > 0 ? ` — fit ${s.candidateModules.join(' or ')}` : '';
  return `Socket ${s.socketId}: ${fitted}, needs ${needs}${fix}`;
}

export { modulesProviding };
