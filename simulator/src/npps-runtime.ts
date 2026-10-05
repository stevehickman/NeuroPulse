/**
 * npps-runtime.ts — the simulator's protocol library, read at RUN TIME.
 *
 * This module is bundled to `simulator/js/vendor/npps-runtime.js` by
 * `scripts/build-simulator-runtime.ts`. **What is bundled is CODE, not
 * protocol content:** the real parser (`common/lib/nppsParser.ts`) plus the
 * transform below. Every zone, condition and protocol is fetched and parsed
 * from `protocols/predefined/` when the simulator loads.
 *
 * It replaced `simulator/js/protocols.generated.js` and the `ZONES` export of
 * `sockets.generated.js`, which baked the parsed library into JS at build time.
 * NP-NPPS-REF-001 §1.6 (*No build-time cache of protocol content*) prohibits
 * that on every runtime, and the simulator had already demonstrated why: its
 * committed output still carried the `side` field ZONE-1 removed a month
 * earlier, so the simulator was quietly showing a lattice the repository had
 * stopped describing. Editing a `.npps` file must change what the simulator
 * shows, and now does.
 *
 * The cost, accepted deliberately: `fetch()` is blocked at a `file://` origin,
 * so the simulator must be served over HTTP. See HOWTO §3.
 */

import { parseNPPSFile, buildNamespace, validateNamespaceReferences } from '../../common/lib/nppsParser';
import { DEFAULT_WAVELENGTH_RULES, mapWavelength, parsePbmWavelength } from '../../common/lib/wavelengthRules';
import type {
  NPProtocolDefinition,
  NPZoneDefinition,
  NPModalityParams,
} from '../../common/types/protocol';

/** Where the shipped library lives, relative to `simulator/index.html`. */
const DEFAULT_BASE_URL = '../protocols/predefined/';

// T2-only modality types (CLAUDE.md §3 "T2 additions") — used to derive a display
// tier for protocols, since .npps files don't carry an explicit tier field.
const T2_ONLY_MODALITY_TYPES = new Set([
  'qeeg_21ch', 'tms', 'pbm_deep_1170nm', 'clinical_tacs', 'hd_tdcs', 'cervical_vns',
]);

export interface SimZone {
  name: string;
  sockets: number[];
  description: string;
}

export interface SimProtocol {
  id: string;
  name: string;
  description?: string;
  tier: 'T1' | 'T2';
  duration: number;
  modalities: Record<string, any>;
  phases: Array<{ label: string; start: number; end: number; color: string }>;
  conditions: string[];
}

// ── Live bindings ─────────────────────────────────────────────────────────────
//
// Exported as `let` so consumers can keep a plain `import { PROTOCOLS }` and see
// the loaded values through the ES live binding. They are EMPTY until
// `loadLibrary()` resolves, so nothing may read them at module top level —
// `loadLibrary()` is awaited in app.js before any consumer is constructed.

export let PROTOCOLS: Record<string, SimProtocol> = {};
export let PROTOCOL_IDS: string[] = [];
export let ZONES: SimZone[] = [];

/** Composite protocols skipped, and any namespace problems found while loading. */
export let LOAD_REPORT: {
  compositesSkipped: number;
  duplicateDefinitions: string[];
  unresolvedReferences: string[];
} = { compositesSkipped: 0, duplicateDefinitions: [], unresolvedReferences: [] };

// ── Transform ─────────────────────────────────────────────────────────────────

function slugFromFilename(filename: string): string {
  return filename.replace(/\.npps$/, '');
}

/** The emitter channel a block's wavelength drives, as the simulator names it ('808nm'). */
function channelLabel(wl: string): string | null {
  const w = parsePbmWavelength(wl);
  if (w.kind !== 'single') return null;
  const el = mapWavelength(w.nm, DEFAULT_WAVELENGTH_RULES);
  return el ? el.replace('led_', '') + 'nm' : null;
}

function findParam<T extends NPModalityParams['type']>(
  modalities: NPProtocolDefinition['modalities'],
  type: T
): Extract<NPModalityParams, { type: T }>['params'] | undefined {
  const m = modalities.find(m => m.modalityParams.type === type);
  return m ? (m.modalityParams.params as any) : undefined;
}

function deriveTier(def: NPProtocolDefinition): 'T1' | 'T2' {
  return def.modalities.some(m => T2_ONLY_MODALITY_TYPES.has(m.modalityParams.type))
    ? 'T2' : 'T1';
}

function derivePhases(durationSeconds: number) {
  if (durationSeconds < 120) {
    return [{ label: 'Session', start: 0, end: durationSeconds, color: '#334466' }];
  }
  const ramp = Math.min(60, Math.round(durationSeconds * 0.05));
  return [
    { label: 'Ramp', start: 0, end: ramp, color: '#334466' },
    { label: 'Main', start: ramp, end: durationSeconds - ramp, color: '#446688' },
    { label: 'Ramp-down', start: durationSeconds - ramp, end: durationSeconds, color: '#334466' },
  ];
}

export function buildModalities(def: NPProtocolDefinition, durationSeconds: number) {
  const out: Record<string, any> = {};

  // One block per wavelength: the display is the union of the blocks, and the
  // dose is what the tissue receives from all of them (the Σ term of CLAUDE.md §3).
  const pbmBlocks = def.modalities
    .filter(m => m.modalityParams.type === 'pbm_transcranial')
    .map(m => m.modalityParams.params as Extract<NPModalityParams, { type: 'pbm_transcranial' }>['params']);
  if (pbmBlocks.length > 0) {
    const first = pbmBlocks[0];
    const dose_jcm2 = pbmBlocks.reduce(
      (sum, b) => sum + (b.irradianceMWcm2 * (b.dutyCyclePercent / 100) * durationSeconds) / 1000, 0);
    out.pbm = {
      active: true,
      zones: [...new Set(pbmBlocks.flatMap(b => b.zoneRefs ?? []))],
      wavelengths: [...new Set(pbmBlocks.map(b => channelLabel(b.wavelength)).filter((x): x is string => x !== null))],
      frequency: first.frequencyHz,
      dutyCycle: first.dutyCyclePercent / 100,
      dose_jcm2: Math.round(dose_jcm2 * 10) / 10,
    };
  }

  const eeg = findParam(def.modalities, 'eeg_neurofeedback');
  if (eeg) {
    out.eeg = {
      active: true,
      channels: ['Fp1', 'Fp2', 'F3', 'F4', 'C3', 'C4', 'P3', 'P4'],
      band: eeg.band,
    };
  }

  const bes = findParam(def.modalities, 'bes_tacs');
  if (bes) {
    out.bes = { active: true, frequency: bes.frequencyHz, intensity_ma: bes.intensityMilliamps };
  }

  const tdcs = findParam(def.modalities, 'tdcs');
  if (tdcs) {
    out.tdcs = { active: true, intensity_ma: tdcs.intensityMilliamps };
  }

  const vns = findParam(def.modalities, 'vns_hrv');
  if (vns) {
    out.vns = {
      active: true,
      frequency: vns.frequencyHz,
      intensity_ma: vns.intensityMilliamps,
      hrv_sync: vns.hrvProtocol !== 'standalone',
    };
  }

  const audio = findParam(def.modalities, 'audio_entrainment');
  if (audio) {
    out.audio = {
      active: true,
      binaural_hz: audio.binauralBeatsHz ?? audio.isochronicTonesHz ?? 10,
      base_hz: audio.carrierHz,
      type: audio.binauralBeatsHz != null ? 'binaural' : 'isochronic',
      breathing_pacer: audio.boneConductionPacer,
      breathing_bpm: vns?.resonanceBreathingRate ?? 6,
    };
  }

  const visual = findParam(def.modalities, 'visual_stimulation');
  if (visual) {
    out.visual = { active: true, frequency: visual.frequencyHz, mode: visual.mode };
  }

  const tms = findParam(def.modalities, 'tms');
  if (tms) {
    out.tms = { active: true, frequency: tms.frequencyHz, target: tms.target, pattern: tms.tmsProtocol };
  }

  const hdTdcs = findParam(def.modalities, 'hd_tdcs');
  if (hdTdcs) {
    out.hd_tdcs = { active: true, target: hdTdcs.target, intensity_ma: hdTdcs.intensityMilliamps };
  }

  return out;
}

/**
 * Turn parsed NPPS files into the simulator's view of the library.
 *
 * Exported separately from the fetching so the whole transform is testable in
 * the web suite against the real shipped files, with no browser involved.
 */
export function buildLibrary(
  files: Array<{ filename: string; text: string }>
): { protocols: Record<string, SimProtocol>; zones: SimZone[]; report: typeof LOAD_REPORT } {
  const parsedFiles = files.map(f => ({ filename: f.filename, parsed: parseNPPSFile(f.text) }));
  const { namespace, errors } = buildNamespace(parsedFiles.map(p => p.parsed));

  const zones: SimZone[] = [...namespace.zones.values()].map((z: NPZoneDefinition) => ({
    name: z.name,
    sockets: z.sockets,
    description: z.description ?? '',
  }));

  const protocols: Record<string, SimProtocol> = {};
  let compositesSkipped = 0;

  for (const { filename, parsed } of parsedFiles) {
    if (parsed.entries.length === 0) continue; // 00-zones.npps / 00-conditions.npps
    const entry = parsed.entries[0];
    if (entry.kind === 'composite') {
      // Composite protocols reference other protocols by name with time-offset
      // layers. Visualising them needs cross-referencing + merging constituent
      // modality configs by layer, which is real additional work — out of scope.
      // Flagged, not silently dropped.
      compositesSkipped++;
      continue;
    }
    const def = entry.protocol;
    const slug = slugFromFilename(filename);
    const duration = def.timingMode.type === 'duration' ? def.timingMode.seconds : 1200;
    protocols[slug] = {
      id: slug,
      name: def.name,
      description: def.description,
      tier: deriveTier(def),
      duration,
      modalities: buildModalities(def, duration),
      phases: derivePhases(duration),
      conditions: def.conditions ?? [],
    };
  }

  return {
    protocols,
    zones,
    report: {
      compositesSkipped,
      duplicateDefinitions: errors,
      unresolvedReferences: validateNamespaceReferences(namespace),
    },
  };
}

/**
 * Fetch and parse the shipped library, then publish it through the live
 * bindings above. Await this before constructing anything that reads them.
 *
 * @param baseUrl directory holding manifest.json and the .npps files.
 */
export async function loadLibrary(baseUrl: string = DEFAULT_BASE_URL): Promise<void> {
  const base = baseUrl.endsWith('/') ? baseUrl : `${baseUrl}/`;

  const manifest = await fetch(`${base}manifest.json`).then(r => {
    if (!r.ok) throw new Error(`manifest.json: HTTP ${r.status}`);
    return r.json();
  });

  // Definition files first, matching §1.6's load order. Order does not affect
  // resolution — the whole set is parsed before references resolve — but it
  // keeps the simulator's behaviour identical to the apps'.
  const names: string[] = [
    ...(manifest.zones ?? []),
    ...(manifest.conditions ?? []),
    ...(manifest.protocols ?? []),
    ...(manifest.composites ?? []),
  ];

  const files = await Promise.all(names.map(async filename => {
    const r = await fetch(`${base}${filename}`);
    if (!r.ok) throw new Error(`${filename}: HTTP ${r.status}`);
    return { filename, text: await r.text() };
  }));

  const { protocols, zones, report } = buildLibrary(files);
  PROTOCOLS = protocols;
  PROTOCOL_IDS = Object.keys(protocols);
  ZONES = zones;
  LOAD_REPORT = report;

  if (report.duplicateDefinitions.length) {
    console.error('[NP-SIM] duplicate definitions:', report.duplicateDefinitions);
  }
  if (report.unresolvedReferences.length) {
    console.error('[NP-SIM] unresolved references:', report.unresolvedReferences);
  }
}
