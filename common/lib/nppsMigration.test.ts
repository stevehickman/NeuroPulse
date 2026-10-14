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
    const sealed = nppsMigration({ op: 'sealBundle', kind: 'uhdr', key, nonce, items });
    expect(nppsMigration({ op: 'openBundle', bundle: sealed.bundle, key }).items).toEqual(items);
    const wrong = nppsMigration({ op: 'openBundle', bundle: sealed.bundle, key: 'ff'.repeat(32) });
    expect(wrong).toEqual({ ok: false, reason: 'authentication_failed' });
  });

  // @verifies SWR-MG-05
  it('reads the kind and version of a bundle with no key', async () => {
    await initNppsCore();
    const key = '00'.repeat(32);
    const sealed = nppsMigration({ op: 'sealBundle', kind: 'migrate', key, nonce: '11'.repeat(12), items: { protocols: [] } });
    const h = nppsMigration({ op: 'readBundleHeader', bundle: sealed.bundle });
    expect(h).toMatchObject({ ok: true, kind: 'migrate', formatVersion: 1, bundleHash: sealed.bundleHash });
  });
});
