import { NPProtocolEntry, NPNamespace } from '../../../../common/types/protocol';
import { wavelengthRulesStore } from './wavelengthRulesStore';
import {
  parseNPPSFile,
  buildNamespace,
  validateNamespaceReferences,
} from '../../../../common/lib/nppsParser';

const MANIFEST_URL = '/protocols/predefined/manifest.json';
const BASE_URL = '/protocols/predefined/';

interface Manifest {
  protocols: string[];
  composites: string[];
  // Rev 2: zone and condition definition files. All files load into ONE shared
  // namespace (NP-NPPS-REF-001 §1.6); definition files are loaded first so
  // protocol zone/condition references resolve.
  zones?: string[];
  conditions?: string[];
  // PBM wavelength mapping defaults (NP-NPPS-REF-001 §7a).
  wavelength_rules?: string[];
}

let _cached: NPProtocolEntry[] | null = null;
let _cachedNamespace: NPNamespace | null = null;

export async function loadPredefinedProtocols(): Promise<NPProtocolEntry[]> {
  if (_cached) return _cached;

  try {
    const manifest = await fetch(MANIFEST_URL).then(r => r.json()) as Manifest;
    // Definition files first, then protocol/composite files.
    const defFiles = [
      ...(manifest.zones ?? []),
      ...(manifest.conditions ?? []),
      ...(manifest.wavelength_rules ?? []),
    ];
    const protoFiles = [...manifest.protocols, ...manifest.composites];
    const allFiles = [...defFiles, ...protoFiles];

    const parsed = await Promise.all(
      allFiles.map(async (filename) => {
        try {
          const text = await fetch(`${BASE_URL}${filename}`).then(r => r.text());
          return parseNPPSFile(text);
        } catch {
          return { entries: [], zones: [], conditions: [], wavelengthRules: [] };
        }
      })
    );

    const { namespace, errors } = buildNamespace(parsed);
    const refErrors = validateNamespaceReferences(namespace);
    // Duplicate zone/condition names are errors, not warnings: the colliding
    // name is left unbound, so anything referencing it also shows up in
    // refErrors below (NP-NPPS-REF-001 §1.6).
    if (errors.length > 0) console.error('[NPPS] duplicate definitions:', errors);
    if (refErrors.length > 0) console.error('[NPPS] unresolved references:', refErrors);

    // The shipped wavelength defaults. The store keeps its built-in copy if the
    // file is missing or invalid, so eligibility never runs against no rules.
    const shippedRules = parsed.flatMap(p => p.wavelengthRules);
    if (shippedRules.length === 1) wavelengthRulesStore.setDefaults(shippedRules[0]);
    else if (shippedRules.length > 1) console.error('[NPPS] more than one wavelength_rules block ships');

    _cachedNamespace = namespace;
    _cached = namespace.entries;
    return _cached;
  } catch {
    return [];
  }
}

// Synchronous accessor for already-loaded protocols (empty until loadPredefinedProtocols resolves)
export function getCachedPredefinedProtocols(): NPProtocolEntry[] {
  return _cached ?? [];
}

// The loaded namespace (zones + conditions + entries); null until loaded.
export function getPredefinedNamespace(): NPNamespace | null {
  return _cachedNamespace;
}
