import { NPProtocolEntry } from '../types/protocol';
import { parseNPPS } from './nppsParser';

const MANIFEST_URL = '/protocols/predefined/manifest.json';
const BASE_URL = '/protocols/predefined/';

interface Manifest {
  protocols: string[];
  composites: string[];
}

let _cached: NPProtocolEntry[] | null = null;

export async function loadPredefinedProtocols(): Promise<NPProtocolEntry[]> {
  if (_cached) return _cached;

  try {
    const manifest = await fetch(MANIFEST_URL).then(r => r.json()) as Manifest;
    const allFiles = [...manifest.protocols, ...manifest.composites];
    const results = await Promise.all(
      allFiles.map(async (filename) => {
        try {
          const text = await fetch(`${BASE_URL}${filename}`).then(r => r.text());
          return parseNPPS(text);
        } catch {
          return [] as NPProtocolEntry[];
        }
      })
    );
    _cached = results.flat();
    return _cached;
  } catch {
    return [];
  }
}

// Synchronous accessor for already-loaded protocols (empty until loadPredefinedProtocols resolves)
export function getCachedPredefinedProtocols(): NPProtocolEntry[] {
  return _cached ?? [];
}

// Backward-compatibility export — synchronous empty array at module load time.
// Replaced by async loading; kept so any remaining static imports don't break at compile time.
export const PREDEFINED_PROTOCOLS: NPProtocolEntry[] = [];
