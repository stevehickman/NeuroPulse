/**
 * A key crosses into the WebAssembly core as bytes beside the request, or not at all (SWR-MG-30), through the wrapper
 * every web build calls. The Rust tests hold the rules (common/npps-core/tests/secret_boundary.rs); this holds the
 * marshalling: the framing, the bytes that come back, and that nothing of the key is left in the module's memory.
 */
import { afterEach, describe, expect, it } from 'vitest';
import { initNppsCore, nppsMigration, nppsMigrationSecret, NppsRefusal, wrapNppsCoreForTest, type CoreExports } from './nppsCore';

const KEY_HEX = 'd34c917e02b865fa1da730c95be4862f790abd43e1589c27f63a84cd126fb055';
const NONCE = 'a0a1a2a3a4a5a6a7a8a9aaab';
const key = (): Uint8Array => Uint8Array.from(KEY_HEX.match(/../g)!.map((b) => parseInt(b, 16)));
const hex = (b: Uint8Array): string => Array.from(b, (x) => x.toString(16).padStart(2, '0')).join('');

let restore: (() => void) | null = null;
afterEach(() => { restore?.(); restore = null; nppsMigration({ op: 'keyReleaseAll' }); });

/** How many times `needle` (raw bytes) occurs anywhere in the module's linear memory. */
function copies(memory: WebAssembly.Memory, needle: Uint8Array): number {
  const all = new Uint8Array(memory.buffer);
  let found = 0;
  outer: for (let i = 0; i + needle.length <= all.length; i++) {
    for (let j = 0; j < needle.length; j++) if (all[i + j] !== needle[j]) continue outer;
    found++;
  }
  return found;
}

describe('a key beside the request', () => {
  // @verifies SWR-MG-30#a
  it('seals what the same key in hex sealed, and comes back from a derivation as bytes', async () => {
    await initNppsCore();
    const byHex = nppsMigration({ op: 'sealValue', key: KEY_HEX, nonce: NONCE, name: 'np.n', value: 'history' });
    const request = { op: 'sealValue', key: { $secret: 0 }, nonce: NONCE, name: 'np.n', value: 'history' };
    expect(JSON.stringify(request)).not.toContain(KEY_HEX.slice(0, 16));
    const sealed = nppsMigrationSecret(request, [key()]);
    expect(sealed.answer).toEqual(byHex);
    expect(sealed.secrets).toEqual([]);

    const seed = new Uint8Array(32).fill(7);
    const derived = nppsMigrationSecret({ op: 'deriveUhdrKey', seed: { $secret: 0 }, salt: '09'.repeat(32) }, [seed]);
    expect(derived.answer).toEqual({ ok: true, key: { $secret: 0 } });
    expect(derived.secrets).toHaveLength(1);
    expect(hex(derived.secrets[0])).toBe(nppsMigration({ op: 'deriveUhdrKey', seed: '07'.repeat(32), salt: '09'.repeat(32) }).key);

    // Two secrets in one call, in the order the request names them.
    const aws = (secret: unknown, token: unknown) => ({ op: 'awsSigV4', method: 'GET', host: 'examplebucket.s3.amazonaws.com', path: '/test.txt', query: [],
      headers: {}, payloadHash: 'UNSIGNED-PAYLOAD', region: 'us-east-1', service: 's3', accessKeyId: 'AKIAIOSFODNN7EXAMPLE',
      secretAccessKey: secret, sessionToken: token, amzDate: '20130524T000000Z' });
    const enc = new TextEncoder();
    const signed = nppsMigrationSecret(aws({ $secret: 1 }, { $secret: 0 }), [enc.encode('a-session-token'), enc.encode('wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY')]);
    expect(signed.answer).toEqual(nppsMigration(aws('wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY', 'a-session-token')));
  });

  // @verifies SWR-MG-30#a
  it('refuses a secret the call was not given, and leaves the next call working', async () => {
    await initNppsCore();
    const request = { op: 'sealValue', key: { $secret: 0 }, nonce: NONCE, name: 'np.n', value: 'v' };
    expect(() => nppsMigrationSecret(request, [])).toThrow(NppsRefusal);
    expect(nppsMigrationSecret(request, [key()]).answer.sealed).toBeTypeOf('string');
  });

  // @verifies SWR-MG-30#a
  it('leaves no copy of the key in the module, as bytes or as hex, once the call is over', async () => {
    await initNppsCore();
    let memory: WebAssembly.Memory | null = null;
    restore = wrapNppsCoreForTest((real: CoreExports) => { memory = real.memory; return real; });
    const k = key();
    nppsMigrationSecret({ op: 'sealValue', key: { $secret: 0 }, nonce: NONCE, name: 'np.n', value: 'history' }, [k]);
    expect(copies(memory!, k)).toBe(0);
    expect(copies(memory!, k.slice(0, 8))).toBe(0);
    expect(copies(memory!, new TextEncoder().encode(KEY_HEX.slice(0, 16)))).toBe(0);
    // The search works: the caller's own array still holds the key, and a kept key is found (next test).
    expect(hex(k)).toBe(KEY_HEX);
  });
});

describe('a key the core keeps', () => {
  // @verifies SWR-MG-30#b
  it('is brought in once, used by its handle, held in the module until released, and unknown after', async () => {
    await initNppsCore();
    let memory: WebAssembly.Memory | null = null;
    restore = wrapNppsCoreForTest((real: CoreExports) => { memory = real.memory; return real; });
    const k = key();
    const handle = nppsMigrationSecret({ op: 'keyImport', purpose: 'data', key: { $secret: 0 } }, [k]).answer.key;
    expect(handle.$handle).toBeTypeOf('number');
    // The handle goes through the plain entry point: there is no secret left to carry.
    const sealed = nppsMigration({ op: 'sealValue', key: handle, nonce: NONCE, name: 'np.n', value: 'history' });
    expect(sealed).toEqual(nppsMigration({ op: 'sealValue', key: KEY_HEX, nonce: NONCE, name: 'np.n', value: 'history' }));
    expect(nppsMigration({ op: 'openValue', key: handle, name: 'np.n', sealed: sealed.sealed }).value).toBe('history');
    // Kept means held: exactly one copy of the key is in the module while the handle lives.
    nppsMigration({ op: 'keyCount' });   // a call with no key in it, so nothing of the hex call above is in flight
    expect(copies(memory!, k)).toBe(1);
    const header = nppsMigration({ op: 'archiveStart', salt: '01'.repeat(16) }).header;
    expect(() => nppsMigration({ op: 'archiveSealChunk', key: handle, header, counter: 0, last: true, data: '00' })).toThrow(/wrong_key_purpose/);
    expect(nppsMigration({ op: 'keyRelease', key: handle })).toEqual({ released: true });
    expect(copies(memory!, k)).toBe(0);
    expect(() => nppsMigration({ op: 'sealValue', key: handle, nonce: NONCE, name: 'np.n', value: 'v' })).toThrow('unknown_key');
  });
});
