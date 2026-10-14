/**
 * The migration rules, through the TypeScript wrapper over the shared core (neurone_npps_core::migration, SWR-MG). The Rust
 * tests hold the rules and the claim race; this holds the WebAssembly path every web build calls.
 */
import { describe, expect, it } from 'vitest';
import { initNppsCore, nppsMigration } from './nppsCore';

describe('nppsMigration', () => {
  // @verifies SWR-MG-05
  it('refuses a bundle of a newer format version whole', async () => {
    await initNppsCore();
    const r = nppsMigration({ op: 'readHeader', header: { kind: 'migrate', formatVersion: 99 } });
    expect(r.ok).toBe(false);
    expect(r.reason).toBe('newer_version');
  });

  // @verifies SWR-MG-03
  it('names what the nightly kind leaves out', async () => {
    await initNppsCore();
    const r = nppsMigration({ op: 'inventory', kind: 'uhdr' });
    expect(r.omitted).toContain('protocols');
    expect(r.omitted).toContain('limitsProfiles');
    expect(r.carried).toContain('withdrawalState');
  });

  // @verifies SWR-MG-04
  it('keeps a withdrawal across a restore', async () => {
    await initNppsCore();
    const r = nppsMigration({
      op: 'mergeWithdrawal',
      local: {},
      incoming: { blanketConsentWithdrawnAt: '2026-03-01T00:00:00Z' },
    });
    expect(r.blanketConsentWithdrawnAt).toBe('2026-03-01T00:00:00Z');
  });

  // @verifies SWR-MG-02
  it('seals and opens a bundle through WebAssembly, and refuses a wrong key', async () => {
    await initNppsCore();
    const key = '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f';
    const nonce = 'a0a1a2a3a4a5a6a7a8a9aaab';
    const items = { sessionHistory: [{ id: 1 }], withdrawalState: { blanketConsentWithdrawnAt: null } };
    const kdf = nppsMigration({ op: 'bundleKdf', method: 'passphrase', salt: '5a'.repeat(32) }).kdf;
    const sealed = nppsMigration({ op: 'sealBundle', kind: 'uhdr', key, nonce, items, kdf });
    expect(nppsMigration({ op: 'openBundle', bundle: sealed.bundle, key }).items).toEqual(items);
    const wrong = nppsMigration({ op: 'openBundle', bundle: sealed.bundle, key: 'ff'.repeat(32) });
    expect(wrong).toEqual({ ok: false, reason: 'authentication_failed' });
  });

  // @verifies SWR-MG-05
  it('reads the kind and version of a bundle with no key', async () => {
    await initNppsCore();
    const key = '00'.repeat(32);
    const kdf = nppsMigration({ op: 'bundleKdf', method: 'recoveryCode', salt: '5a'.repeat(32) }).kdf;
    const sealed = nppsMigration({ op: 'sealBundle', kind: 'migrate', key, nonce: '11'.repeat(12), items: { protocols: [] }, kdf });
    const h = nppsMigration({ op: 'readBundleHeader', bundle: sealed.bundle });
    expect(h).toMatchObject({ ok: true, kind: 'migrate', formatVersion: 1, bundleHash: sealed.bundleHash, method: 'recoveryCode' });
  });

  // @verifies SWR-MG-02
  it('writes a recovery code a person can read back, and finds a typing error', async () => {
    await initNppsCore();
    const bytes = Array.from({ length: 32 }, (_, i) => (i * 37 + 11) & 255)
      .map((b) => b.toString(16).padStart(2, '0'))
      .join('');
    const { code } = nppsMigration({ op: 'encodeRecoveryCode', bytes });
    expect(code).toMatch(/^([0-9A-Z]{4}-){13}[0-9A-Z]{4}$/);
    expect(nppsMigration({ op: 'parseRecoveryCode', code: code.toLowerCase() }).secret).toBe(bytes);
    const typo = (code[0] === '2' ? '3' : '2') + code.slice(1);
    expect(nppsMigration({ op: 'parseRecoveryCode', code: typo })).toEqual({ ok: false, reason: 'bad_checksum' });
  });

  // @verifies SWR-MG-02
  it('normalises a passphrase and refuses a short one', async () => {
    await initNppsCore();
    const a = nppsMigration({ op: 'passphraseSecret', passphrase: 'caf\u00e9 au lait noir' });
    const b = nppsMigration({ op: 'passphraseSecret', passphrase: 'cafe\u0301 au lait noir' });
    expect(a.secret).toBe(b.secret);
    expect(nppsMigration({ op: 'passphraseSecret', passphrase: 'too short' })).toMatchObject({ ok: false, reason: 'too_short' });
  });

  // @verifies SWR-MG-05
  it('refuses a header that asks for a cheaper derivation', async () => {
    await initNppsCore();
    const kdf = nppsMigration({ op: 'bundleKdf', method: 'passphrase', salt: '5a'.repeat(32) }).kdf;
    expect(kdf).toMatchObject({ algorithm: 'argon2id', memoryKiB: 65536, iterations: 4, parallelism: 1, outputLength: 32 });
    expect(() =>
      nppsMigration({ op: 'sealBundle', kind: 'uhdr', key: '00'.repeat(32), nonce: '11'.repeat(12), items: {}, kdf: { ...kdf, memoryKiB: 8 } }),
    ).toThrow();
  });
});
