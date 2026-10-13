/*
 * The protocol library's rules as the web app asks them (SWR-PL-02/03/06, SWR-UP-02, SWR-WO-01/02/03, SWR-FM-03…07,
 * SWR-HV-04). Nothing is decided here: every answer is the shared core's (common/npps-core/src/library.rs, reached
 * through nppsLibrary), so the five apps cannot disagree. This file is the typed shape of the answers, the entry
 * summaries the core is given and the calls. protocolLibrary.ts keeps the state and the screens read it.
 */

import type { NPProtocolEntry } from '../../../../common/types/protocol';
import { entryId } from '../../../../common/types/protocol';
import { nppsContentHash, nppsLibrary } from '../../../../common/lib/nppsCore';
import { protocolCoreDef } from './hubCompiler';
import { getPredefinedNamespace } from './predefinedProtocols';

// ── Answers ───────────────────────────────────────────────────────────────────

/** What a save met. */
export type SaveResult =
  | { kind: 'saved' }
  /** A predefined protocol: never saved over; the way to change it is a clone. */
  | { kind: 'read_only' }
  /** The protocol's family has been used: nothing was saved, and a clone of the whole family is offered (SWR-PL-02, SWR-FM-03). */
  | { kind: 'family_used'; familyId: string; memberIds: string[] }
  /** A variant for this hardware version already exists in the family (SWR-FM-06). */
  | { kind: 'variant_exists' };

/** What a delete met. */
export type DeleteResult =
  | { kind: 'deleted' }
  | { kind: 'read_only' }
  | { kind: 'not_found' }
  /** A used family is archived, never deleted (SWR-FM-05). */
  | { kind: 'family_used' };

/** What an upload meets before any byte is sent. A refusal is a local decision: nothing is sent and nothing is marked used. */
export type UploadGate =
  | { kind: 'allow' }
  /**
   * The protocol has no layout for the helmet's hardware version (SWR-HV-04). The counts say how the first of its layouts
   * overlaps the helmet's; they are null while the helmet's layout is not in the table, so no overlap is claimed.
   */
  | {
      kind: 'hardware_mismatch';
      helmetVersion: number;
      protocolVersions: number[];
      sharedSockets: number | null;
      onlyProtocolSockets: number | null;
      onlyHelmetSockets: number | null;
    }
  /** The clone's content is its source's (SWR-PL-06): upload the source, edit the clone, or cancel. */
  | { kind: 'unmodified_clone'; sourceId: string };

/** The outcomes of an upload that was sent, and the two refusals that are not failed uploads (SWR-UP-02). */
export type UploadOutcome = 'confirmed' | 'unknown' | 'confirmed_failed' | 'not_sent' | 'not_a_failed_upload';

// ── Entry helpers ─────────────────────────────────────────────────────────────

/** A protocol's or composite's family id, or undefined when it states none. */
export function entryFamilyId(entry: NPProtocolEntry): string | undefined {
  return entry.kind === 'single' ? entry.protocol.familyId : entry.composite.familyId;
}

/** The key of the family an entry belongs to: its family id, or its own id when it has none (a family of one). */
export function familyKey(entry: NPProtocolEntry): string {
  return entryFamilyId(entry) ?? entryId(entry);
}

function entryVersion(entry: NPProtocolEntry): string {
  return entry.kind === 'single' ? entry.protocol.version : entry.composite.version;
}

// ── The core's questions ──────────────────────────────────────────────────────

/** The zone namespace as the core takes it: every zone's socket list per hardware version. One layout exists, version 1. */
function zonesByVersion(): Record<string, Record<string, number[]>> {
  const out: Record<string, Record<string, number[]>> = {};
  const zones = getPredefinedNamespace()?.zones;
  if (zones) for (const [name, zone] of zones) out[name] = { '1': [...zone.sockets] };
  return out;
}

/** Hardware versions a protocol can be sent to (SWR-HV-04); a composite is a version-1 object until composites carry zones of their own. */
export function supportedVersions(entry: NPProtocolEntry): number[] {
  if (entry.kind === 'composite') return [1];
  return nppsLibrary({ op: 'supportedVersions', def: protocolCoreDef(entry.protocol), zonesByVersion: zonesByVersion() }).versions;
}

/** The summaries the core decides over. `withVersions` costs a core call per protocol, so only the questions that need it ask. */
export function summaries(entries: readonly NPProtocolEntry[], used: ReadonlySet<string>, withVersions: boolean): object[] {
  return entries.map(e => ({
    id: entryId(e),
    ...(entryFamilyId(e) ? { familyId: entryFamilyId(e) } : {}),
    used: used.has(familyKey(e)),
    version: entryVersion(e),
    scope: 'owner',
    ...(withVersions ? { hardwareVersions: supportedVersions(e) } : {}),
  }));
}

export const rules = {
  saveCheck: (entries: object[], id: string) => nppsLibrary({ op: 'saveCheck', entries, id }),
  deleteCheck: (entries: object[], id: string) => nppsLibrary({ op: 'deleteCheck', entries, id }),
  cloneFamily: (entries: object[], id: string, newFamilyId: string, newIds: string[]) =>
    nppsLibrary({ op: 'cloneFamily', entries, id, newFamilyId, newIds }),
  addVariantCheck: (entries: object[], familyId: string, hardwareVersion: number) =>
    nppsLibrary({ op: 'addVariantCheck', entries, familyId, hardwareVersion }),
  variantFor: (entries: object[], familyId: string, helmetVersion: number): string[] =>
    nppsLibrary({ op: 'variantFor', entries, familyId, helmetVersion }).variants,
  offered: (entries: object[], archivedFamilies: readonly string[]): string[] =>
    nppsLibrary({ op: 'offered', entries, archivedFamilies: [...archivedFamilies] }).offered,
  uploadGate: (helmetVersion: number, protocolVersions: number[], contentHash: string, sourceHash: string | null) =>
    nppsLibrary({ op: 'uploadGate', helmetVersion, protocolVersions, contentHash, sourceHash }),
  uploadResult: (req: {
    sentHash: string;
    helmetStatus?: string;
    helmetHash?: string;
    helmetRefusal?: string;
    refusedBeforeSend?: boolean;
  }) => nppsLibrary({ op: 'uploadResult', ...req }),
  mergeImport: (usedHashes: readonly string[], incoming: Array<{ id: string; hash?: string; used: boolean }>) =>
    nppsLibrary({ op: 'mergeImport', usedHashes: [...usedHashes], incoming }).entries as Array<{ id: string; used: boolean }>,
  /** The record a used family is stored as: the family id and nothing else (SWR-WO-02). */
  usedRecord: (familyId: string): { familyId: string } => nppsLibrary({ op: 'usedRecord', familyId }).record,
  isValidUsedRecord: (record: unknown): boolean => nppsLibrary({ op: 'usedRecord', check: record }).valid,
};

/** A protocol's identity (SWR-HV-01) for the one layout there is; null for a composite or a protocol that cannot be hashed yet. */
export function contentHash(entry: NPProtocolEntry): string | null {
  if (entry.kind !== 'single') return null;
  try {
    return nppsContentHash({
      def: protocolCoreDef(entry.protocol),
      hardwareVersion: 1,
      zonesByVersion: zonesByVersion(),
      clinicianSockets: null,
    });
  } catch {
    return null; // for example a clinician-selected target the operator has not chosen yet
  }
}
