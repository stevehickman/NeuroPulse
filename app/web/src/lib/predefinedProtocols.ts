import { NPProtocolEntry, NPNamespace } from '../../../../common/types/protocol';
import { wavelengthRulesStore } from './wavelengthRulesStore';
import { initNppsCore } from '../../../../common/lib/nppsCore';
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

/**
 * The predefined library did not load whole (REQ-NPPS-34). `failures` names each file that could not be
 * fetched or parsed, with the reason. Nothing of the attempt is cached or offered, and the next call to
 * `loadPredefinedProtocols` starts again.
 */
export class PredefinedLoadError extends Error {
  constructor(readonly failures: readonly string[]) {
    super(`the predefined protocol library did not load: ${failures.join('; ')}`);
    this.name = 'PredefinedLoadError';
  }
}

let _cached: NPProtocolEntry[] | null = null;
let _cachedNamespace: NPNamespace | null = null;
// The load in progress, so a second caller (a StrictMode re-run, a second component) joins it instead
// of starting another. Cleared when it settles: a failed load is never the answer to a later call.
let _loading: Promise<NPProtocolEntry[]> | null = null;

const reason = (e: unknown): string => (e instanceof Error ? e.message : String(e));

// A response that is not 2xx is a failure, never text to parse: a server that answers a missing file
// with its index page would otherwise hand the parser HTML.
async function fetchOk(url: string): Promise<Response> {
  const response = await fetch(url);
  if (!response.ok) throw new Error(`HTTP ${response.status}`);
  return response;
}

/**
 * Load the shipped library into one namespace (NP-NPPS-REF-001 §1.6). It loads whole or not at all
 * (REQ-NPPS-34): a file that cannot be fetched or parsed rejects the load with a `PredefinedLoadError`,
 * because a definitions file read as empty leaves every zone and condition unbound while the protocols
 * that name them are still offered. A rejected load is reported here and not cached; call again to retry.
 */
export function loadPredefinedProtocols(): Promise<NPProtocolEntry[]> {
  if (_cached) return Promise.resolve(_cached);
  if (_loading) return _loading;
  const attempt: Promise<NPProtocolEntry[]> = loadOnce()
    .catch((e: unknown) => {
      console.error('[NPPS] predefined protocol library failed to load:', e instanceof PredefinedLoadError ? e.failures : reason(e));
      throw e;
    })
    .finally(() => { if (_loading === attempt) _loading = null; });
  _loading = attempt;
  return attempt;
}

async function loadOnce(): Promise<NPProtocolEntry[]> {
  // The parser is the WebAssembly core. Nothing is parsed until it is loaded: a parse attempted
  // earlier throws, and that throw is a load failure, not an empty file (REQ-NPPS-34 (a)).
  let manifest: Manifest;
  try {
    [manifest] = await Promise.all([
      fetchOk(MANIFEST_URL).then(r => r.json() as Promise<Manifest>),
      initNppsCore(),
    ]);
  } catch (e) {
    throw new PredefinedLoadError([`manifest.json or the NPPS core: ${reason(e)}`]);
  }

  // Definition files first, then protocol/composite files.
  const defFiles = [
    ...(manifest.zones ?? []),
    ...(manifest.conditions ?? []),
    ...(manifest.wavelength_rules ?? []),
  ];
  const protoFiles = [...(manifest.protocols ?? []), ...(manifest.composites ?? [])];
  const allFiles = [...defFiles, ...protoFiles];

  const settled = await Promise.allSettled(
    allFiles.map(async (filename) => parseNPPSFile(await (await fetchOk(`${BASE_URL}${filename}`)).text())),
  );
  const failures = settled.flatMap((s, i) => (s.status === 'rejected' ? [`${allFiles[i]}: ${reason(s.reason)}`] : []));
  if (failures.length > 0) throw new PredefinedLoadError(failures);
  const parsed = settled.map(s => (s as PromiseFulfilledResult<ReturnType<typeof parseNPPSFile>>).value);

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
}

// Synchronous accessor for already-loaded protocols (empty until loadPredefinedProtocols resolves)
export function getCachedPredefinedProtocols(): NPProtocolEntry[] {
  return _cached ?? [];
}

// The loaded namespace (zones + conditions + entries); null until loaded.
export function getPredefinedNamespace(): NPNamespace | null {
  return _cachedNamespace;
}

/** For tests only: forget the loaded library, so the next call loads again. */
export function resetPredefinedProtocolsForTest(): void {
  _cached = null;
  _cachedNamespace = null;
  _loading = null;
}
