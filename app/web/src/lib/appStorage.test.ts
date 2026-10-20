import { describe, it, expect, vi, beforeAll } from 'vitest';
import { initNppsCore } from '../../../../common/lib/nppsCore';
import { defaultParams } from '../../../../common/types/protocol';
import type { NPProtocolEntry, NPProtocolDefinition } from '../../../../common/types/protocol';
import { MemoryStorage, LEGACY_LOCAL_STORAGE_KEYS, purgeLegacyBrowserState } from './appStorage';
import { ProtocolLibrary } from './protocolLibrary';
import { limitsStore } from './limitsStore';
import { backupText, openBackup } from './protocolBackup';
import { readFileSync } from 'fs';
import { join, resolve } from 'path';

const SRC = resolve(__dirname);

beforeAll(async () => { await initNppsCore(); });

function entry(id: string, name = `Test ${id}`): NPProtocolEntry {
  return {
    kind: 'single',
    protocol: {
      id, name, description: '', author: 'test', version: '1', tags: [],
      createdAt: '2026-01-01', modifiedAt: '2026-01-01', isPredefined: false,
      timingMode: { type: 'duration', seconds: 600 },
      modalities: [{
        id: 'm1', enabled: true, interval: { intervalOnSeconds: 0, intervalOffSeconds: 0 },
        modalityParams: { type: 'bes_tacs', params: defaultParams('bes_tacs') },
      }],
    } as unknown as NPProtocolDefinition,
  };
}

describe('the React app keeps nothing in the browser', () => {
  // @verifies REQ-SW03-18#b
  it('MemoryStorage keeps what it is given for the life of the object and nowhere else', () => {
    const m = new MemoryStorage();
    m.setItem('k', 'v');
    expect(m.getItem('k')).toBe('v');
    m.removeItem('k');
    expect(m.getItem('k')).toBeNull();
    expect(new MemoryStorage().getItem('k')).toBeNull();
  });

  // @verifies REQ-SW03-18#b
  it('removes every key an earlier build left in localStorage, and counts them', () => {
    const items = new Map<string, string>(LEGACY_LOCAL_STORAGE_KEYS.slice(0, 4).map(k => [k, 'x']));
    items.set('some_other_site_key', 'keep');
    const fake = { getItem: (k: string) => items.get(k) ?? null, removeItem: (k: string) => void items.delete(k) };
    expect(purgeLegacyBrowserState(fake)).toBe(4);
    expect([...items.keys()]).toEqual(['some_other_site_key']);
    expect(purgeLegacyBrowserState(fake)).toBe(0);
  });

  // @verifies REQ-SW03-18#b
  it('a blocked or absent localStorage does not stop the app starting', () => {
    const blocked = { getItem: () => { throw new Error('blocked'); }, removeItem: () => { throw new Error('blocked'); } };
    expect(() => purgeLegacyBrowserState(blocked)).not.toThrow();
    expect(purgeLegacyBrowserState(null)).toBe(0);
  });

  // @verifies REQ-SW03-18#b
  it('saving a person\'s profile, limits and a protocol writes nothing to localStorage', () => {
    const spy = vi.fn();
    const real = Object.getOwnPropertyDescriptor(globalThis, 'localStorage');
    Object.defineProperty(globalThis, 'localStorage', {
      configurable: true,
      value: { getItem: spy, setItem: spy, removeItem: spy, clear: spy, key: spy, length: 0 },
    });
    try {
      limitsStore.saveProfile({ id: 'p1', name: 'Alice', notes: 'n', dateOfBirth: '1980-01-01', createdAt: new Date().toISOString() });
      limitsStore.saveGlobalLimits(limitsStore.resolvedLimits);
      const lib = new ProtocolLibrary();
      expect(lib.saveUserProtocol(entry('a')).kind).toBe('saved');
      expect(limitsStore.profiles.some(p => p.id === 'p1')).toBe(true);
    } finally {
      if (real) Object.defineProperty(globalThis, 'localStorage', real);
      else delete (globalThis as { localStorage?: unknown }).localStorage;
      limitsStore.deleteProfile('p1');
      limitsStore.clearGlobalLimits();
    }
    expect(spy).not.toHaveBeenCalled();
  });

  // @verifies REQ-SW03-18#b
  it('no source file of the app reads or writes localStorage except to remove an earlier build\'s copy', () => {
    for (const f of ['protocolLibrary.ts', 'limitsStore.ts', 'wavelengthRulesStore.ts']) {
      const code = readFileSync(join(SRC, f), 'utf8').split('\n').filter(l => !l.trim().startsWith('//') && !l.trim().startsWith('*')).join('\n');
      expect(code, f).not.toMatch(/localStorage/);
    }
  });
});

describe('the protocol backup file', () => {
  function libraryWith(...entries: NPProtocolEntry[]) {
    const lib = new ProtocolLibrary(new MemoryStorage());
    for (const e of entries) lib.saveUserProtocol(e);
    return lib;
  }

  // @verifies REQ-SW03-18#b
  it('has nothing to save when the person has no protocols', () => {
    expect(backupText(new ProtocolLibrary(new MemoryStorage()))).toBeNull();
  });

  // @verifies REQ-SW03-18#b
  it('saves the person\'s protocols as .npps and opens them into a fresh library', () => {
    const mine = libraryWith(entry('a', 'Alpha'), entry('b', 'Beta'));
    const text = backupText(mine)!;
    expect(text).toContain('Alpha');
    const fresh = new ProtocolLibrary(new MemoryStorage());
    const r = openBackup(fresh, text);
    expect(r).toEqual({ opened: 2, refused: 0, unreadable: false });
    expect(fresh.userProtocols.map(e => (e.kind === 'single' ? e.protocol.name : '')).sort()).toEqual(['Alpha', 'Beta']);
  });

  // @verifies REQ-SW03-18#b
  it('a file that is not .npps changes nothing', () => {
    const fresh = new ProtocolLibrary(new MemoryStorage());
    expect(openBackup(fresh, 'this is not a protocol {{{')).toMatchObject({ opened: 0, unreadable: true });
    expect(fresh.userProtocols.length).toBe(0);
  });
});
