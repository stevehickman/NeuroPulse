/**
 * What the TypeScript wrapper leaves in the core's memory (SWR-MG-28 (a), OI-PRIV-12 stage 1). The WebAssembly linear
 * memory never shrinks, so a request that held a passphrase stays readable for as long as the page unless it is cleared.
 * These tests stand between the wrapper and the module and look at every buffer at the moment the wrapper releases it.
 */
import { describe, expect, it } from 'vitest';
import { initNppsCore, nppsMigration, NppsRefusal, wrapNppsCoreForTest, type NppsCoreExports } from './nppsCore';

interface Release { len: number; zero: boolean }

/** Run `work` with every `npps_free` observed: the length released, and whether every byte was zero when it was. */
function releases(work: () => void): Release[] {
  const seen: Release[] = [];
  const restore = wrapNppsCoreForTest((real: NppsCoreExports) => ({
    memory: real.memory,
    npps_alloc: real.npps_alloc,
    npps_parse_json: real.npps_parse_json, npps_compile_json: real.npps_compile_json, npps_namespace_json: real.npps_namespace_json,
    npps_serialize_json: real.npps_serialize_json, npps_validate_json: real.npps_validate_json,
    npps_resolve_limits_json: real.npps_resolve_limits_json, npps_content_hash_json: real.npps_content_hash_json,
    npps_library_json: real.npps_library_json, npps_migration_json: real.npps_migration_json,
    npps_free(ptr: number, len: number): void {
      seen.push({ len, zero: new Uint8Array(real.memory.buffer, ptr, len).every((b) => b === 0) });
      real.npps_free(ptr, len);
    },
  }));
  try { work(); } finally { restore(); }
  return seen;
}

describe('the wrapper clears what it wrote before it releases it', () => {
  // @verifies SWR-MG-28#a
  it('a request that holds a passphrase, and its answer, are zero when released', async () => {
    await initNppsCore();
    const request = { op: 'passphraseSecret', passphrase: 'correct horse battery staple' };
    const requestBytes = new TextEncoder().encode(JSON.stringify(request)).length;
    let answer: { ok: boolean; secret: string } | undefined;
    const seen = releases(() => { answer = nppsMigration(request); });
    expect(answer?.ok).toBe(true);
    // The answer, the request (one spare byte, so its pointer is never null) and the two out cells.
    expect(seen.map((r) => r.len).sort((a, b) => a - b)).toEqual([8, requestBytes + 1, JSON.stringify(answer).length].sort((a, b) => a - b));
    expect(seen.filter((r) => !r.zero)).toEqual([]);
  });

  // @verifies SWR-MG-28#a
  it('a request the core refuses is cleared all the same', async () => {
    await initNppsCore();
    let refused = false;
    const seen = releases(() => {
      try {
        nppsMigration({ op: 'sealValue', key: '00', nonce: '00', name: 'n', value: 'the history of the person' });
      } catch (e) {
        refused = e instanceof NppsRefusal;
      }
    });
    expect(refused).toBe(true);
    expect(seen.length).toBe(3);
    expect(seen.filter((r) => !r.zero)).toEqual([]);
  });
});
