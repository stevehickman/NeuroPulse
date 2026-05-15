import {
  NPProtocolEntry,
  NPModalityTypeId,
  entryId,
  requiredModalities,
} from '../types/protocol';
import { PREDEFINED_PROTOCOLS } from './predefinedProtocols';
import { parseNPPS } from './nppsParser';
import { serializeNPPS } from './nppsSerializer';

export interface ProtocolAvailability {
  available: boolean;
  missingModalities?: NPModalityTypeId[];
  reason?: string;
}

export type DeviceTier = 'none' | 't1' | 't2';

const STORAGE_KEY = 'np_user_protocols';

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

class ProtocolLibrary {
  private _userProtocols: NPProtocolEntry[] = [];
  private _deviceTier: DeviceTier = 'none';
  private _has1064SmartModules: boolean = false;
  private _listeners: Array<() => void> = [];

  constructor() {
    this.load();
  }

  get allProtocols(): NPProtocolEntry[] {
    return [...PREDEFINED_PROTOCOLS, ...this._userProtocols];
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

  saveUserProtocol(entry: NPProtocolEntry): void {
    const id = entryId(entry);
    const idx = this._userProtocols.findIndex(e => entryId(e) === id);
    if (idx >= 0) {
      this._userProtocols[idx] = entry;
    } else {
      this._userProtocols.push(entry);
    }
    this.persist();
    this.notify();
  }

  deleteUserProtocol(id: string): void {
    this._userProtocols = this._userProtocols.filter(e => entryId(e) !== id);
    this.persist();
    this.notify();
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

  exportScript(entry: NPProtocolEntry): string {
    return serializeNPPS([entry]);
  }

  importScript(text: string): NPProtocolEntry[] {
    return parseNPPS(text);
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
    try {
      const raw = localStorage.getItem(STORAGE_KEY);
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
  }

  private persist(): void {
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(this._userProtocols));
    } catch {
      // Storage full or unavailable — silent fail
    }
  }
}

export const protocolLibrary = new ProtocolLibrary();
