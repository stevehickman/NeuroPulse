/**
 * The web binding wipes its copy of a request before it releases it (SWR-MG-28 (b)). WebAssembly linear memory is never
 * given back to the system, so a request left in a released buffer is readable for the life of the page.
 */
import { afterEach, describe, expect, it } from 'vitest';
import { initNppsCore, nppsMigration, wrapNppsCoreForTest, type CoreExports } from './nppsCore';

/** What each released buffer held at the moment it was released. */
interface Release { ptr: number; len: number; bytes: Uint8Array }

let restore: (() => void) | null = null;
afterEach(() => { restore?.(); restore = null; });

/** Record every `npps_free`, with the buffer as it was when the binding let go of it (before the core's own wipe). */
function watchReleases(): Release[] {
  const seen: Release[] = [];
  restore = wrapNppsCoreForTest((real: CoreExports) => ({
    memory: real.memory,
    npps_alloc: real.npps_alloc,
    npps_parse_json: real.npps_parse_json,
    npps_compile_json: real.npps_compile_json,
    npps_namespace_json: real.npps_namespace_json,
    npps_serialize_json: real.npps_serialize_json,
    npps_validate_json: real.npps_validate_json,
    npps_resolve_limits_json: real.npps_resolve_limits_json,
    npps_content_hash_json: real.npps_content_hash_json,
    npps_library_json: real.npps_library_json,
    npps_migration_json: real.npps_migration_json,
    npps_free(ptr: number, len: number) {
      seen.push({ ptr, len, bytes: new Uint8Array(real.memory.buffer, ptr, len).slice() });
      real.npps_free(ptr, len);
    },
  }));
  return seen;
}

describe('the web binding and the memory a request passed through', () => {
  // @verifies SWR-MG-28#b
  it('zeroes the request it copied into the module before releasing it', async () => {
    await initNppsCore();
    const seen = watchReleases();
    const passphrase = 'a passphrase nobody else should read';
    const r = nppsMigration({ op: 'passphraseSecret', passphrase });
    expect(r.ok).toBe(true);
    // The request is the buffer whose length is the request's UTF-8 length plus one.
    const requestLength = new TextEncoder().encode(JSON.stringify({ op: 'passphraseSecret', passphrase })).length + 1;
    const request = seen.filter((f) => f.len === requestLength);
    expect(request).toHaveLength(1);
    expect(request[0].bytes.every((b) => b === 0)).toBe(true);
    // The answer was not the binding's to wipe before reading it; it held the secret until the core's npps_free wiped it.
    const answer = seen.find((f) => new TextDecoder().decode(f.bytes).includes('"secret"'));
    expect(answer).toBeDefined();
  });

  // @verifies SWR-MG-28#a
  it('leaves an answer that carried a key zeroed in the module once it is released', async () => {
    await initNppsCore();
    const seen = watchReleases();
    const r = nppsMigration({ op: 'passphraseSecret', passphrase: 'a passphrase nobody else should read' });
    const answer = seen.find((f) => new TextDecoder().decode(f.bytes).includes(r.secret));
    expect(answer).toBeDefined();
    // The allocator may reuse the first bytes of a released block for its own bookkeeping; the secret itself is gone.
    const memory = watchedMemory();
    const after = new TextDecoder().decode(memory.slice(answer!.ptr, answer!.ptr + answer!.len));
    expect(after).not.toContain(r.secret.slice(0, 16));
    expect(after).not.toContain('"secret"');
  });

  // @verifies SWR-MG-28#b
  it('zeroes the request of a call the core refuses', async () => {
    await initNppsCore();
    const seen = watchReleases();
    expect(() => nppsMigration({ op: 'no such operation', key: '00'.repeat(32) })).toThrow();
    const requestLength = new TextEncoder().encode(JSON.stringify({ op: 'no such operation', key: '00'.repeat(32) })).length + 1;
    const request = seen.filter((f) => f.len === requestLength);
    expect(request).toHaveLength(1);
    expect(request[0].bytes.every((b) => b === 0)).toBe(true);
  });
});

/** The module's whole memory, as it is now. */
function watchedMemory(): Uint8Array {
  let memory: WebAssembly.Memory | null = null;
  const undo = wrapNppsCoreForTest((real) => { memory = real.memory; return real; });
  undo();
  return new Uint8Array(memory!.buffer);
}

/** How many times `needle` occurs anywhere in the module's linear memory, as it is now. */
function copiesInMemory(memory: WebAssembly.Memory, needle: string): number {
  const all = new Uint8Array(memory.buffer);
  const want = new TextEncoder().encode(needle);
  let found = 0;
  outer: for (let i = 0; i + want.length <= all.length; i++) {
    for (let j = 0; j < want.length; j++) if (all[i + j] !== want[j]) continue outer;
    found++;
  }
  return found;
}

describe('the whole of the module\'s memory after a call that carried a secret', () => {
  // Reading released memory is undefined inside the module and well defined from outside it: linear memory is one array.
  // Before the wiping allocator (SWR-MG-28 (g)) this found one copy of the passphrase, of the hex key and of the value,
  // left by the core's own parse of the request.
  // @verifies SWR-MG-28#g
  it('holds no copy of the passphrase, the key or the plaintext value', async () => {
    await initNppsCore();
    let memory: WebAssembly.Memory | null = null;
    restore = wrapNppsCoreForTest((real: CoreExports) => { memory = real.memory; return real; });
    const passphrase = 'MARKER-passphrase-7f3a91c2';
    const key = 'd34c917e02b865fa1da730c95be4862f790abd43e1589c27f63a84cd126fb055';
    const value = 'MARKER-plaintext-55aa';

    const secret = nppsMigration({ op: 'passphraseSecret', passphrase }).secret as string;
    expect(copiesInMemory(memory!, passphrase)).toBe(0);
    expect(copiesInMemory(memory!, secret)).toBe(0);

    const sealed = nppsMigration({ op: 'sealValue', key, nonce: 'a0a1a2a3a4a5a6a7a8a9aaab', name: 'np.n', value }).sealed as string;
    expect(copiesInMemory(memory!, key)).toBe(0);
    expect(copiesInMemory(memory!, key.slice(0, 16))).toBe(0);   // nor the front of it, as a buffer that grew would leave
    expect(copiesInMemory(memory!, value)).toBe(0);

    expect(nppsMigration({ op: 'openValue', key, name: 'np.n', sealed }).value).toBe(value);
    expect(copiesInMemory(memory!, key)).toBe(0);
    expect(copiesInMemory(memory!, value)).toBe(0);
    // The search itself works: text the module keeps for good (a refusal it can give) is found.
    expect(copiesInMemory(memory!, 'key is required')).toBeGreaterThan(0);
  });
});
