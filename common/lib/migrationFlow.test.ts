/**
 * The migration flows through the TypeScript driver and WebAssembly. The Rust tests hold the order of the flow once for every
 * app (neurone_npps_core::migration_flow); this holds the web path: the driver loop and the effect shapes the host sees.
 */
import { describe, expect, it } from 'vitest';
import { type FlowEffect, type MigrationHost, runMigrationFlow } from './migrationFlow';
import { initNppsCore } from './nppsCore';

const toHex = (bytes: number[]) => bytes.map((b) => (b & 255).toString(16).padStart(2, '0')).join('');

class Route {
  bundle: string | null = null;
  claim: unknown = null;
  confirmation: unknown = null;
}

/** A phone and a route in memory: dumb storage that performs whatever effect it is asked for. */
class Host implements MigrationHost {
  items = new Map<string, unknown>();
  seen: string[] = [];
  private n: number;
  constructor(
    private route: Route,
    private secret: string,
    seed: number,
  ) {
    this.n = seed;
  }

  perform(e: FlowEffect): Record<string, unknown> {
    this.seen.push(e.type);
    switch (e.type) {
      case 'random':
        return { bytes: toHex(Array.from({ length: e.bytes as number }, () => (this.n = (this.n * 31 + 7) & 255))) };
      case 'deriveKey': {
        // Not Argon2: a stand-in that depends on the secret and the salt, as the real one does.
        const text = this.secret + (e.kdf as { salt: string }).salt;
        return { key: toHex(Array.from({ length: 32 }, (_, i) => text.charCodeAt(i % text.length) * 7 + i)) };
      }
      case 'readItems': {
        const items: Record<string, unknown> = {};
        for (const name of e.names as string[]) if (this.items.has(name)) items[name] = this.items.get(name);
        return { items };
      }
      case 'applyItem':
        this.items.set(e.name as string, e.value);
        return { ok: true };
      case 'getBundle':
        return { bundle: this.route.bundle };
      case 'putBundle':
        this.route.bundle = e.bundle as string;
        return { ok: true };
      case 'readClaim':
        return { claim: this.route.claim };
      case 'createClaim': {
        const created = this.route.claim === null;
        if (created) this.route.claim = e.claim;
        return { created };
      }
      case 'writeConfirmation':
        this.route.confirmation = e.confirmation;
        return { ok: true };
      case 'readConfirmation':
        return { confirmation: this.route.confirmation };
      default:
        throw new Error(`unknown effect ${e.type}`);
    }
  }
}

const ROUTE = { id: 'memory', atomicClaim: true, direct: false, authenticatedPairing: true };
const HOST_ITEMS = ['protocols', 'sessionHistory', 'limitsProfiles', 'characterisationEnrolment'];

describe('runMigrationFlow', () => {
  // @verifies SWR-MG-02
  // @verifies SWR-MG-08
  it('moves a phone to another through the shared flow and ends with a confirmation of the bundle hash', async () => {
    await initNppsCore();
    const route = new Route();
    const old = new Host(route, 'a long enough passphrase', 1);
    for (const n of HOST_ITEMS) old.items.set(n, { [`np.${n}`]: `value-of-${n}` });
    const done = await runMigrationFlow('backup', { kind: 'migrate', method: 'passphrase', route: ROUTE, hostItems: HOST_ITEMS }, old);
    expect(done.ok).toBe(true);
    expect(route.bundle).not.toContain(toHex([...'value-of-sessionHistory'].map((c) => c.charCodeAt(0))));

    const fresh = new Host(route, 'a long enough passphrase', 50);
    const r = await runMigrationFlow('restore', { route: ROUTE, hostItems: HOST_ITEMS }, fresh);
    expect(r).toMatchObject({ ok: true, kind: 'migrate', confirmed: true });
    expect([...fresh.items.entries()].sort()).toEqual([...old.items.entries()].sort());
    expect(route.confirmation).toEqual({ bundleHash: done.bundleHash });
  });

  // @verifies SWR-MG-07
  it('opens the bundle before it takes the claim, so a wrong secret leaves no claim behind', async () => {
    await initNppsCore();
    const route = new Route();
    const old = new Host(route, 'a long enough passphrase', 1);
    old.items.set('sessionHistory', { a: 'b' });
    await runMigrationFlow('backup', { kind: 'migrate', method: 'passphrase', route: ROUTE, hostItems: HOST_ITEMS }, old);
    const wrong = new Host(route, 'a different passphrase', 50);
    const r = await runMigrationFlow('restore', { route: ROUTE, hostItems: HOST_ITEMS }, wrong);
    expect(r).toEqual({ ok: false, reason: 'authentication_failed' });
    expect(route.claim).toBeNull();
    expect(wrong.seen).not.toContain('createClaim');
  });
});
