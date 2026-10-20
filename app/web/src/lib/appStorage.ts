// The React app keeps nothing in the browser (privacy review Finding 2 leftovers, REQ-SW03-18 (b)).
//
// The protocol library, the limits store and the wavelength-rules store used to keep their state in `localStorage`, which any
// script on the page and any browser extension reads in the clear and for which a browser has no keystore. The Compose browser
// app holds all of its state in memory for the same reason, and this app follows it: the stores write here, to an object that
// lives as long as the page, and a reload starts clean. The person keeps protocols through the backup file the library can save
// and open (`protocolBackup.ts`) and limits through their existing .npps export.
//
// What an earlier build left in `localStorage` is removed the first time this module loads, so it does not sit there unread.

import type { KeyValueStorage } from './protocolLibrary';

/** Every key an earlier build of this app wrote to `localStorage`. */
export const LEGACY_LOCAL_STORAGE_KEYS: readonly string[] = [
  'np_user_protocols',
  'np_used_families',
  'np_archived_families',
  'np_clone_sources',
  'np_global_limits',
  'np_helmet_limits',
  'np_individual_limits',
  'np_profiles',
  'np_active_profile_id',
  'np_active_helmet_id',
  'np_wavelength_rules_user',
];

/** A key-value store that lives in memory for the life of the page and writes nowhere. */
export class MemoryStorage implements KeyValueStorage {
  private readonly items = new Map<string, string>();

  getItem(key: string): string | null {
    return this.items.get(key) ?? null;
  }

  setItem(key: string, value: string): void {
    this.items.set(key, String(value));
  }

  removeItem(key: string): void {
    this.items.delete(key);
  }

  get size(): number {
    return this.items.size;
  }
}

/**
 * Remove what an earlier build left in `localStorage`. Returns how many of the known keys were present. Never throws: storage
 * can be blocked, absent or full, and the app must start regardless.
 */
export function purgeLegacyBrowserState(storage?: Pick<Storage, 'getItem' | 'removeItem'> | null): number {
  let target: Pick<Storage, 'getItem' | 'removeItem'> | null | undefined = storage;
  if (target === undefined) {
    try {
      target = typeof localStorage !== 'undefined' ? localStorage : null;
    } catch {
      target = null;
    }
  }
  if (!target) return 0;
  let removed = 0;
  for (const key of LEGACY_LOCAL_STORAGE_KEYS) {
    try {
      if (target.getItem(key) !== null) removed++;
      target.removeItem(key);
    } catch {
      // Blocked or unavailable: nothing more can be done for this key.
    }
  }
  return removed;
}

/** The one store the app's state lives in. */
export const appStorage = new MemoryStorage();

purgeLegacyBrowserState();
