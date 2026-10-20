import {
  NPProtocolEntry,
  NPModalityTypeId,
  entryId,
  entryName,
  entryIsReadOnly,
  requiredModalities,
} from '../../../../common/types/protocol';
import { loadPredefinedProtocols, getCachedPredefinedProtocols } from './predefinedProtocols';
import {
  rules, summaries, supportedVersions, contentHash, familyKey,
  type SaveResult, type DeleteResult, type UploadGate, type UploadOutcome,
} from './libraryRules';
import { parseNPPS } from '../../../../common/lib/nppsParser';
import { serializeNPPS } from '../../../../common/lib/nppsSerializer';
import { appStorage } from './appStorage';

export interface ProtocolAvailability {
  available: boolean;
  missingModalities?: NPModalityTypeId[];
  reason?: string;
}

export type DeviceTier = 'none' | 't1' | 't2';

const STORAGE_KEY = 'np_user_protocols';
/** The families that have been used (SWR-WO-02): a list of `{familyId}` records, the family id and nothing else. */
export const USED_FAMILIES_KEY = 'np_used_families';
/** The families the warranty owner has archived (SWR-WO-01): a list of family ids. A list state, never a deletion. */
export const ARCHIVED_FAMILIES_KEY = 'np_archived_families';
/** Clone entry id → the entry it was cloned from, for the unmodified-clone check at upload (SWR-PL-06). */
export const CLONE_SOURCES_KEY = 'np_clone_sources';

/** The storage the library keeps its state in: the page's memory in a browser (`appStorage`), anything with these two calls in a test. */
export interface KeyValueStorage {
  getItem(key: string): string | null;
  setItem(key: string, value: string): void;
}

function defaultStorage(): KeyValueStorage | null {
  // Memory only (REQ-SW03-18 (b)): nothing the library keeps is written to the browser.
  return appStorage;
}

// Modalities available at each tier
const T1_MODALITIES: Set<NPModalityTypeId> = new Set([
  'pbm_transcranial', 'pbm_intranasal', 'eeg_neurofeedback',
  'bes_tacs', 'tdcs', 'vns_hrv', 'audio_entrainment', 'visual_stimulation',
]);

const T2_MODALITIES: Set<NPModalityTypeId> = new Set([
  ...T1_MODALITIES,
  'qeeg_21ch', 'tms', 'pbm_deep_1170nm', 'clinical_tacs', 'hd_tdcs', 'cervical_vns',
]);

const ACCESSORY_MODALITIES: Set<NPModalityTypeId> = new Set<NPModalityTypeId>([
  'vibrotactile_40hz',
]);

export class ProtocolLibrary {
  private _userProtocols: NPProtocolEntry[] = [];
  private _used = new Set<string>();
  private _archived = new Set<string>();
  private _cloneSources = new Map<string, string>();
  private _predefinedProtocols: NPProtocolEntry[] = [];
  private _predefinedLoaded = false;
  private _deviceTier: DeviceTier = 'none';
  private _has1064SmartModules: boolean = false;
  private _listeners: Array<() => void> = [];

  constructor(private readonly storage: KeyValueStorage | null = defaultStorage()) {
    this.load();
    // Start async load of predefined protocols from .npps files
    loadPredefinedProtocols().then(protocols => {
      this._predefinedProtocols = protocols;
      this._predefinedLoaded = true;
      this.notify();
    });
  }

  get isLoaded(): boolean {
    return this._predefinedLoaded;
  }

  get allProtocols(): NPProtocolEntry[] {
    // While loading, use any already-cached protocols (e.g. from a prior call)
    const predefined = this._predefinedLoaded
      ? this._predefinedProtocols
      : getCachedPredefinedProtocols();
    return [...predefined, ...this._userProtocols];
  }

  get userProtocols(): NPProtocolEntry[] {
    return [...this._userProtocols];
  }

  get deviceTier(): DeviceTier {
    return this._deviceTier;
  }

  get has1064SmartModules(): boolean {
    return this._has1064SmartModules;
  }

  canEdit(entry: NPProtocolEntry): boolean {
    return !entryIsReadOnly(entry);
  }

  canDelete(entry: NPProtocolEntry): boolean {
    return !entryIsReadOnly(entry);
  }

  getAvailableModalities(): Set<NPModalityTypeId> {
    const available = new Set<NPModalityTypeId>();
    if (this._deviceTier === 'none') return available;

    const tierSet = this._deviceTier === 't2' ? T2_MODALITIES : T1_MODALITIES;
    for (const m of tierSet) available.add(m);

    // 1064nm upgrade
    if (this._has1064SmartModules) {
      available.add('pbm_transcranial'); // already there, but 1064nm wavelength enabled
    }

    // Accessories always considered present for availability check
    for (const m of ACCESSORY_MODALITIES) available.add(m);

    return available;
  }

  checkAvailability(entry: NPProtocolEntry): ProtocolAvailability {
    if (this._deviceTier === 'none') {
      return { available: false, reason: 'No device connected' };
    }

    if (entry.kind === 'composite') {
      // Composite: check if referenced protocols exist
      const missing: string[] = [];
      for (const layer of entry.composite.layers) {
        const found = this.allProtocols.some(
          e => e.kind === 'single' && e.protocol.name === layer.protocolName
        );
        if (!found) missing.push(layer.protocolName);
      }
      if (missing.length > 0) {
        return { available: false, reason: `Missing referenced protocols: ${missing.join(', ')}` };
      }
      return { available: true };
    }

    const required = requiredModalities(entry);
    const available = this.getAvailableModalities();
    const missing: NPModalityTypeId[] = [];
    for (const m of required) {
      if (!available.has(m)) missing.push(m);
    }

    if (missing.length > 0) {
      return { available: false, missingModalities: missing };
    }
    return { available: true };
  }

  /**
   * Save `entry`. The rules are the core's (SWR-PL-02/03, SWR-FM-03/06): a predefined protocol is never saved over; a
   * protocol of a used family is refused, nothing is written, and a clone of the whole family is offered; an unused one
   * saves in place. A new protocol joining a used family is a new variant, allowed only for a hardware version the
   * family has no variant for.
   */
  saveUserProtocol(entry: NPProtocolEntry): SaveResult {
    if (entryIsReadOnly(entry)) return { kind: 'read_only' };
    const id = entryId(entry);
    const idx = this._userProtocols.findIndex(e => entryId(e) === id);
    if (idx >= 0) {
      const check = rules.saveCheck(this.familySummaries(false), id);
      if (!check.allowed) {
        return { kind: 'family_used', familyId: check.familyId ?? familyKey(entry), memberIds: check.members ?? [] };
      }
      this._userProtocols[idx] = entry;
    } else {
      const isMember = entry.kind === 'single' ? !!entry.protocol.familyId : !!entry.composite.familyId;
      if (isMember && this._used.has(familyKey(entry))) {
        const version = supportedVersions(entry)[0] ?? 1;
        const check = rules.addVariantCheck(this.familySummaries(true), familyKey(entry), version);
        if (!check.allowed) return { kind: 'variant_exists' };
      }
      this._userProtocols.push(entry);
    }
    this.persist();
    this.notify();
    return { kind: 'saved' };
  }

  /** Delete a user protocol. A used family is archived, never deleted (SWR-FM-05). */
  deleteUserProtocol(id: string): DeleteResult {
    const isPredefined = this._predefinedProtocols.some(e => entryId(e) === id);
    if (isPredefined) return { kind: 'read_only' };
    if (!this._userProtocols.some(e => entryId(e) === id)) return { kind: 'not_found' };
    const check = rules.deleteCheck(this.familySummaries(false), id);
    if (!check.allowed) return { kind: 'family_used' };
    this._userProtocols = this._userProtocols.filter(e => entryId(e) !== id);
    this._cloneSources.delete(id);
    this.persist();
    this.notify();
    return { kind: 'deleted' };
  }

  // ── Families (SWR-FM-03…07, SWR-PL-02) ─────────────────────────────────────

  /** Whether `entry`'s family has been used. The use of any variant marks the family (SWR-FM-03). */
  isUsed(entry: NPProtocolEntry): boolean {
    return this._used.has(familyKey(entry));
  }

  /**
   * Clone the whole family `id` belongs to: a new family from the moment it is made, a copy of every variant, none of
   * them used, fully editable, the source family recorded for provenance only (SWR-FM-03). `copyName` words each
   * copy's name from the source's (a locale key, resolved by the caller); a clash is made unique with a number, because
   * a protocol is referred to by name.
   */
  cloneFamily(id: string, copyName: (base: string) => string): NPProtocolEntry[] {
    const entry = this.allProtocols.find(e => entryId(e) === id);
    if (!entry) return [];
    const key = familyKey(entry);
    const members = this.allProtocols.filter(e => familyKey(e) === key);
    const newFamily = crypto.randomUUID();
    const newIds = members.map(() => crypto.randomUUID());
    // The core checks the count, that the family is new and that every id is new, before anything is made.
    rules.cloneFamily(this.familySummaries(false), id, newFamily, newIds);
    const taken = new Set(this.allProtocols.map(e => entryName(e)));
    const copies = members.map((m, i) => {
      const name = this.uniqueName(copyName(entryName(m)), taken);
      taken.add(name);
      return this.copyAs(m, newIds[i], name, newFamily);
    });
    members.forEach((m, i) => {
      this._userProtocols.push(copies[i]);
      this._cloneSources.set(newIds[i], entryId(m));
    });
    this.persist();
    this.notify();
    return copies;
  }

  /**
   * The edit that a used family refused, kept: clone the whole family, then save `edited` over its own copy. The copy's
   * id and family are the clone's; the content is the person's edit, and the name is theirs when they changed it.
   * An edit that is not in the library yet is kept as a family of its own. Returns the saved copy.
   */
  saveAsClone(edited: NPProtocolEntry, copyName: (base: string) => string): NPProtocolEntry | null {
    const original = this.allProtocols.find(e => entryId(e) === entryId(edited));
    if (!original) return this.saveAsNewFamily(edited, copyName);
    const copies = this.cloneFamily(entryId(edited), copyName);
    const copy = copies.find(c => this._cloneSources.get(entryId(c)) === entryId(edited));
    if (!copy) return null;
    const renamed = entryName(edited) !== entryName(original);
    const merged = this.copyAs(edited, entryId(copy), renamed ? entryName(edited) : entryName(copy), entryFamilyIdOf(copy));
    const idx = this._userProtocols.findIndex(e => entryId(e) === entryId(copy));
    this._userProtocols[idx] = merged; // the copy is a new, unused family, so this is a save in place
    this.persist();
    this.notify();
    return merged;
  }

  private saveAsNewFamily(edited: NPProtocolEntry, copyName: (base: string) => string): NPProtocolEntry | null {
    const taken = new Set(this.allProtocols.map(e => entryName(e)));
    const name = taken.has(entryName(edited)) ? this.uniqueName(copyName(entryName(edited)), taken) : entryName(edited);
    const merged = this.copyAs(edited, crypto.randomUUID(), name, crypto.randomUUID());
    this._userProtocols.push(merged);
    this.persist();
    this.notify();
    return merged;
  }

  private copyAs(entry: NPProtocolEntry, id: string, name: string, familyId: string | undefined): NPProtocolEntry {
    const now = new Date().toISOString();
    if (entry.kind === 'single') {
      return {
        kind: 'single',
        protocol: {
          ...entry.protocol, id, name, familyId, isUsed: undefined,
          isPredefined: false, isReadOnly: false, createdAt: now, modifiedAt: now,
          modalities: entry.protocol.modalities.map(m => ({ ...m, id: crypto.randomUUID() })),
        },
      };
    }
    return {
      kind: 'composite',
      composite: {
        ...entry.composite, id, name, familyId, isUsed: undefined,
        isPredefined: false, isReadOnly: false, createdAt: now, modifiedAt: now,
        layers: entry.composite.layers.map(l => ({ ...l, id: crypto.randomUUID() })),
      },
    };
  }

  private uniqueName(base: string, taken: ReadonlySet<string>): string {
    if (!taken.has(base)) return base;
    let n = 2;
    while (taken.has(`${base} ${n}`)) n += 1;
    return `${base} ${n}`;
  }

  /** The variants of `entry`'s family that fit a helmet of `helmetVersion`, latest first; empty means the person decides whether to create one (SWR-HV-04). */
  variantsFor(entry: NPProtocolEntry, helmetVersion: number): NPProtocolEntry[] {
    const ids = rules.variantFor(this.familySummaries(true), familyKey(entry), helmetVersion);
    return ids.map(id => this.allProtocols.find(e => entryId(e) === id)).filter((e): e is NPProtocolEntry => !!e);
  }

  // ── Archive and the offered list (SWR-WO-01, SWR-FM-05) ────────────────────

  /** Archive the family of `entry`: it drops out of the offered list and every variant is kept. */
  archiveFamily(entry: NPProtocolEntry): void {
    if (!this._archived.has(familyKey(entry))) {
      this._archived.add(familyKey(entry));
      this.persist();
      this.notify();
    }
  }

  unarchiveFamily(entry: NPProtocolEntry): void {
    if (this._archived.delete(familyKey(entry))) {
      this.persist();
      this.notify();
    }
  }

  isArchived(entry: NPProtocolEntry): boolean {
    return this._archived.has(familyKey(entry));
  }

  /** The warranty owner's set: every protocol whose family is not archived. */
  get offeredProtocols(): NPProtocolEntry[] {
    const offered = new Set(rules.offered(this.familySummaries(false), [...this._archived]));
    return this.allProtocols.filter(e => offered.has(entryId(e)));
  }

  // ── Upload (SWR-PL-06, SWR-HV-04, SWR-UP-02) ───────────────────────────────

  /** `entry`'s identity as the core defines it, or null for a kind that has none. */
  contentHash(entry: NPProtocolEntry): string | null {
    return contentHash(entry);
  }

  /**
   * What an upload of `entry` to a helmet of `helmetHardwareVersion` meets before any byte is sent. The hardware
   * refusal comes first, then the unmodified clone; an allowed upload is then sent and its result passed to
   * `recordUploadResult`.
   */
  uploadGate(entry: NPProtocolEntry, helmetHardwareVersion: number): UploadGate {
    const hash = contentHash(entry);
    if (hash === null) return { kind: 'allow' };
    const sourceId = this._cloneSources.get(entryId(entry));
    const source = sourceId ? this.allProtocols.find(e => entryId(e) === sourceId) : undefined;
    const answer = rules.uploadGate(helmetHardwareVersion, supportedVersions(entry), hash, source ? contentHash(source) : null);
    if (answer.reason === 'hardware_mismatch') {
      const diff = answer.layoutDiffs?.[0]?.diff ?? null;
      return {
        kind: 'hardware_mismatch',
        helmetVersion: helmetHardwareVersion,
        protocolVersions: answer.protocolVersions,
        sharedSockets: diff ? diff.both.length : null,
        onlyProtocolSockets: diff ? diff.onlyA.length : null,
        onlyHelmetSockets: diff ? diff.onlyB.length : null,
      };
    }
    if (answer.reason === 'unmodified_clone' && source) return { kind: 'unmodified_clone', sourceId: entryId(source) };
    return { kind: 'allow' };
  }

  /**
   * Record how an upload that was sent ended. Confirmed and unknown mark `entry`'s family used; a confirmed failure, a
   * refusal before sending and a helmet refusal for hardware version, unmodified clone or tier do not (SWR-UP-02).
   * `helmetStatus` is the hub's UPLOAD_CHECK verdict, `accepted` or `refused`, or undefined when it never answered.
   */
  recordUploadResult(
    entry: NPProtocolEntry,
    result: { sentHash: string; helmetStatus?: string; helmetHash?: string; helmetRefusal?: string; refusedBeforeSend?: boolean },
  ): UploadOutcome {
    const answer = rules.uploadResult(result);
    if (answer.markUsed) this.markUsed(entry);
    return answer.outcome;
  }

  /** The used flag is one bit per family and is never cleared (SWR-WO-02). */
  private markUsed(entry: NPProtocolEntry): void {
    if (!this._used.has(familyKey(entry))) {
      this._used.add(familyKey(entry));
      this.persist();
      this.notify();
    }
  }

  private familySummaries(withVersions: boolean): object[] {
    return summaries(this.allProtocols, this._used, withVersions);
  }

  duplicateProtocol(entry: NPProtocolEntry, newName: string): NPProtocolEntry {
    const now = new Date().toISOString();
    if (entry.kind === 'single') {
      const dup: NPProtocolEntry = {
        kind: 'single',
        protocol: {
          ...entry.protocol,
          id: crypto.randomUUID(),
          name: newName,
          isPredefined: false,
          isReadOnly: false,  // duplicates are always editable
          createdAt: now,
          modifiedAt: now,
          modalities: entry.protocol.modalities.map(m => ({ ...m, id: crypto.randomUUID() })),
        },
      };
      return dup;
    } else {
      const dup: NPProtocolEntry = {
        kind: 'composite',
        composite: {
          ...entry.composite,
          id: crypto.randomUUID(),
          name: newName,
          isPredefined: false,
          isReadOnly: false,  // duplicates are always editable
          createdAt: now,
          modifiedAt: now,
          layers: entry.composite.layers.map(l => ({ ...l, id: crypto.randomUUID() })),
        },
      };
      return dup;
    }
  }

  updateDeviceTier(tier: DeviceTier, has1064SmartModules: boolean): void {
    this._deviceTier = tier;
    this._has1064SmartModules = has1064SmartModules;
    this.notify();
  }

  /** The script for `entry`, carrying the used flag when its family is used (SWR-WO-03). The flag is not part of the content hash. */
  exportScript(entry: NPProtocolEntry): string {
    if (!this.isUsed(entry)) return serializeNPPS([entry]);
    const flagged: NPProtocolEntry = entry.kind === 'single'
      ? { kind: 'single', protocol: { ...entry.protocol, isUsed: true } }
      : { kind: 'composite', composite: { ...entry.composite, isUsed: true } };
    return serializeNPPS([flagged]);
  }

  /**
   * Read the entries of `text`. An entry that arrives flagged used makes its family used, and one that arrives unflagged
   * but whose content is already used here stays used; an import never clears the flag (SWR-WO-03).
   */
  importScript(text: string): NPProtocolEntry[] {
    const entries = parseNPPS(text);
    const usedHashes = this.allProtocols.filter(e => this.isUsed(e)).map(e => contentHash(e)).filter((h): h is string => h !== null);
    const incoming = entries.map(e => ({
      id: entryId(e),
      hash: contentHash(e) ?? undefined,
      used: (e.kind === 'single' ? e.protocol.isUsed : e.composite.isUsed) === true,
    }));
    const merged = rules.mergeImport(usedHashes, incoming);
    for (const m of merged) {
      const e = entries.find(x => entryId(x) === m.id);
      if (m.used && e) this.markUsed(e);
    }
    return entries;
  }

  subscribe(listener: () => void): () => void {
    this._listeners.push(listener);
    return () => {
      this._listeners = this._listeners.filter(l => l !== listener);
    };
  }

  private notify(): void {
    for (const l of this._listeners) l();
  }

  private load(): void {
    const st = this.storage;
    if (!st) return;
    try {
      const raw = st.getItem(STORAGE_KEY);
      if (raw) {
        const parsed = JSON.parse(raw) as NPProtocolEntry[];
        // Basic validation — ensure array
        if (Array.isArray(parsed)) {
          this._userProtocols = parsed;
        }
      }
    } catch {
      // Corrupt storage — start fresh
      this._userProtocols = [];
    }
    try {
      const used = JSON.parse(st.getItem(USED_FAMILIES_KEY) ?? '[]');
      if (Array.isArray(used)) {
        // A stored record with any field but the family id is not one this library wrote: it is dropped, not trusted.
        for (const rec of used) if (rules.isValidUsedRecord(rec)) this._used.add(rec.familyId);
      }
      const archived = JSON.parse(st.getItem(ARCHIVED_FAMILIES_KEY) ?? '[]');
      if (Array.isArray(archived)) for (const f of archived) if (typeof f === 'string') this._archived.add(f);
      const sources = JSON.parse(st.getItem(CLONE_SOURCES_KEY) ?? '{}');
      if (sources && typeof sources === 'object') {
        for (const [k, v] of Object.entries(sources)) if (typeof v === 'string') this._cloneSources.set(k, v);
      }
    } catch {
      // Unreadable family state — start without it rather than trust it.
    }
  }

  private persist(): void {
    const st = this.storage;
    if (!st) return;
    try {
      st.setItem(STORAGE_KEY, JSON.stringify(this._userProtocols));
      st.setItem(USED_FAMILIES_KEY, JSON.stringify([...this._used].map(f => rules.usedRecord(f))));
      st.setItem(ARCHIVED_FAMILIES_KEY, JSON.stringify([...this._archived]));
      st.setItem(CLONE_SOURCES_KEY, JSON.stringify(Object.fromEntries(this._cloneSources)));
    } catch {
      // Storage full or unavailable — silent fail
    }
  }
}

function entryFamilyIdOf(e: NPProtocolEntry): string | undefined {
  return e.kind === 'single' ? e.protocol.familyId : e.composite.familyId;
}

export const protocolLibrary = new ProtocolLibrary();
