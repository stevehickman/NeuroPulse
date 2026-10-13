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
});
