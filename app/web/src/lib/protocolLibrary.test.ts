// The web protocol library asks the shared core for every library rule (SWR-PL-02/03/06, SWR-FM-03/05, SWR-UP-02,
// SWR-HV-04). These tests run the real core (the WebAssembly build) over an in-memory store, so a rule that drifts from
// the Kotlin, Swift and C# libraries fails here.

import { describe, it, expect, beforeAll } from 'vitest';
import { initNppsCore } from '../../../../common/lib/nppsCore';
import { entryId, entryName, defaultParams } from '../../../../common/types/protocol';
import type { NPProtocolEntry, NPProtocolDefinition } from '../../../../common/types/protocol';
import { ProtocolLibrary, type KeyValueStorage } from './protocolLibrary';

class MemoryStorage implements KeyValueStorage {
  private m = new Map<string, string>();
  getItem(k: string) { return this.m.get(k) ?? null; }
  setItem(k: string, v: string) { this.m.set(k, v); }
}

function definition(id: string, seconds = 600, familyId?: string): NPProtocolDefinition {
  return {
    id, name: `Test ${id}`, description: '', author: 'test', version: '1', tags: [],
    createdAt: '2026-01-01', modifiedAt: '2026-01-01', isPredefined: false,
    ...(familyId ? { familyId } : {}),
    timingMode: { type: 'duration', seconds },
    modalities: [{
      id: 'm1', enabled: true,
      interval: { intervalOnSeconds: 0, intervalOffSeconds: 0 },
      modalityParams: {
        type: 'bes_tacs',
        params: defaultParams('bes_tacs'),
      },
    }],
  } as unknown as NPProtocolDefinition;
}

const entry = (id: string, seconds = 600, familyId?: string): NPProtocolEntry =>
  ({ kind: 'single', protocol: definition(id, seconds, familyId) });

const copyName = (base: string) => `${base} (copy)`;

function libraryWith(...entries: NPProtocolEntry[]): ProtocolLibrary {
  const lib = new ProtocolLibrary(new MemoryStorage());
  for (const e of entries) lib.saveUserProtocol(e);
  return lib;
}

function use(lib: ProtocolLibrary, e: NPProtocolEntry) {
  const hash = lib.contentHash(e)!;
  return lib.recordUploadResult(e, { sentHash: hash, helmetStatus: 'accepted', helmetHash: hash });
}

beforeAll(async () => { await initNppsCore(); });

describe('editing and deleting (SWR-PL-02, SWR-PL-03, SWR-FM-05)', () => {
  // @verifies SWR-PL-03
  it('saves an unused protocol in place', () => {
    const a = entry('a');
    const lib = libraryWith(a);
    expect(lib.saveUserProtocol(entry('a', 700)).kind).toBe('saved');
    expect(lib.isUsed(a)).toBe(false);
  });

  // @verifies SWR-PL-02
  it('refuses a save into a used protocol and writes nothing', () => {
    const a = entry('a');
    const lib = libraryWith(a);
    use(lib, a);
    expect(lib.saveUserProtocol(entry('a', 700)).kind).toBe('family_used');
    expect(lib.contentHash(lib.userProtocols[0])).toBe(lib.contentHash(a));
  });

  // @verifies SWR-FM-05
  it('archives a used protocol instead of deleting it, and restores it', () => {
    const a = entry('a');
    const lib = libraryWith(a);
    use(lib, a);
    expect(lib.deleteUserProtocol('a').kind).toBe('family_used');
    lib.archiveFamily(a);
    expect(lib.isArchived(a)).toBe(true);
    expect(lib.offeredProtocols.some(e => entryId(e) === 'a')).toBe(false);
    lib.unarchiveFamily(a);
    expect(lib.isArchived(a)).toBe(false);
  });

  // @verifies SWR-FM-05
  it('deletes an unused protocol', () => {
    const lib = libraryWith(entry('a'));
    expect(lib.deleteUserProtocol('a').kind).toBe('deleted');
    expect(lib.userProtocols).toHaveLength(0);
  });
});

describe('cloning (SWR-PL-02, SWR-FM-03)', () => {
  // @verifies SWR-FM-03
  it('makes a clone a new, unused, editable family', () => {
    const a = entry('a');
    const lib = libraryWith(a);
    use(lib, a);
    const copies = lib.cloneFamily('a', copyName);
    expect(copies).toHaveLength(1);
    expect(entryId(copies[0])).not.toBe('a');
    expect(entryName(copies[0]).endsWith('(copy)')).toBe(true);
    expect(lib.isUsed(copies[0])).toBe(false);
    expect(lib.saveUserProtocol(entry(entryId(copies[0]), 900)).kind).toBe('saved');
  });
});

describe('upload gate and outcomes (SWR-PL-06, SWR-HV-04, SWR-UP-02)', () => {
  // @verifies SWR-PL-06
  it('stops an unmodified clone and allows an edited one', () => {
    const a = entry('a');
    const lib = libraryWith(a);
    const [copy] = lib.cloneFamily('a', copyName);
    const gate = lib.uploadGate(copy, 1);
    expect(gate.kind).toBe('unmodified_clone');
    const id = entryId(copy);
    lib.saveUserProtocol(entry(id, 901));
    expect(lib.uploadGate(lib.userProtocols.find(e => entryId(e) === id)!, 1).kind).toBe('allow');
  });

  // @verifies SWR-HV-04
  it('refuses a helmet layout the protocol has no version for', () => {
    const a = entry('a');
    const lib = libraryWith(a);
    expect(lib.uploadGate(a, 99).kind).toBe('hardware_mismatch');
  });

  // @verifies SWR-UP-02
  it('marks a family used for a confirmed or unknown upload, not for a refusal', () => {
    const a = entry('a'); const b = entry('b'); const c = entry('c');
    const lib = libraryWith(a, b, c);
    const h = (e: NPProtocolEntry) => lib.contentHash(e)!;
    expect(lib.recordUploadResult(a, { sentHash: h(a), helmetStatus: 'accepted', helmetHash: h(a) })).toBe('confirmed');
    expect(lib.recordUploadResult(b, { sentHash: h(b) })).toBe('unknown');
    expect(lib.recordUploadResult(c, { sentHash: h(c), refusedBeforeSend: true })).toBe('not_sent');
    expect(lib.isUsed(a)).toBe(true);
    expect(lib.isUsed(b)).toBe(true);
    expect(lib.isUsed(c)).toBe(false);
  });
});

describe('persistence and export (SWR-WO-02)', () => {
  // @verifies SWR-WO-02
  it('keeps the used flag across a reload and carries it in a script', () => {
    const store = new MemoryStorage();
    const lib = new ProtocolLibrary(store);
    const a = entry('a');
    lib.saveUserProtocol(a);
    use(lib, a);
    const reloaded = new ProtocolLibrary(store);
    expect(reloaded.isUsed(reloaded.userProtocols[0])).toBe(true);
    const text = reloaded.exportScript(reloaded.userProtocols[0]);
    const other = new ProtocolLibrary(new MemoryStorage());
    const [imported] = other.importScript(text);
    expect(other.isUsed(imported)).toBe(true);
  });
});
