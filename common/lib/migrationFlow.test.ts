/**
 * The migration flows through the TypeScript driver and WebAssembly, Argon2id included. The Rust tests hold the order of the
 * flow once for every app (neurone_npps_core::migration_flow); this holds the web path: the driver loop, the effects it
 * answers itself (the core's key, the key-value store) and the effect shapes the host sees.
 */
import { describe, expect, it } from 'vitest';
import { newRecoveryCode, newKdf, passphraseSecret, recoveryCodeSecret, deriveBundleKey } from './bundleKey';
import { type FlowEffect, type MigrationHost, type FileStore, type KeyValueStore, type Link, MigrationFlow, type ObjectStore, type StoredFile } from './migrationFlow';
import { initNppsCore, nppsMigration } from './nppsCore';

const toHex = (bytes: number[]) => bytes.map((b) => (b & 255).toString(16).padStart(2, '0')).join('');
const steps = (n: number) => Uint8Array.from({ length: n }, (_, i) => i % 251);

class Route {
  bundle: string | null = null;
  claim: unknown = null;
  confirmation: unknown = null;
}

class Store implements KeyValueStore {
  values = new Map<string, string>();
  getString = (k: string) => this.values.get(k);
  putString = (k: string, v: string) => void this.values.set(k, v);
  remove = (k: string) => void this.values.delete(k);
}

/** A route in memory: dumb storage that performs whatever effect the driver does not answer. */
class Host implements MigrationHost {
  seen: string[] = [];
  ran: string[] = [];
  constructor(private route: Route) {}

  perform(e: FlowEffect): Record<string, unknown> {
    this.seen.push(e.type);
    switch (e.type) {
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
      case 'runClearStep':
        this.ran.push(e.step as string);
        return { ok: true };
      case 'listFiles':
        return { files: [] };
      default:
        throw new Error(`unknown effect ${e.type}`);
    }
  }
}

/** The app's files in memory, with every call recorded in order and one call that can be made to throw. */
class MemoryFiles implements FileStore {
  files = new Map<string, Uint8Array>();
  events: string[] = [];
  failOn: string | null = null;
  private check(call: string, path: string) {
    if (this.failOn === `${call}:${path}`) throw new Error(`${call} failed`);
  }
  async list(): Promise<StoredFile[]> {
    return [...this.files].map(([path, b]) => ({ path, size: b.length }));
  }
  async write(path: string, offset: number, data: string) {
    this.check('write', path);
    const bytes = Uint8Array.from(data.match(/../g) ?? [], (h) => parseInt(h, 16));
    this.files.get(path)!.set(bytes, offset);
    this.events.push(`write:${path}:${offset}:${bytes.length}`);
  }
  async sync(path: string) { this.check('sync', path); this.events.push(`sync:${path}`); }
  async delete(path: string) { this.check('delete', path); this.files.delete(path); this.events.push(`delete:${path}`); }
}

/** A provider with named objects and a create-if-absent the store itself arbitrates. */
class MemoryStore implements ObjectStore {
  objects = new Map<string, string>();
  constructor(
    private opts: { conditional?: boolean; space?: number | null; failOn?: string } = {},
  ) {}
  private check(call: string) {
    if (this.opts.failOn === call) throw new Error(`${call} failed`);
  }
  async get(name: string) { this.check('get'); return this.objects.get(name) ?? null; }
  async put(name: string, data: string) { this.check('put'); this.objects.set(name, data); }
  async createIfAbsent(name: string, data: string) {
    this.check('create');
    if (this.opts.conditional !== false && this.objects.has(name)) return false;
    this.objects.set(name, data);
    return true;
  }
  async delete(name: string) { this.check('delete'); this.objects.delete(name); }
  async list(prefix: string) { this.check('list'); return [...this.objects.keys()].filter((k) => k.startsWith(prefix)); }
  async freeSpace() { this.check('space'); return this.opts.space === undefined ? 1 << 30 : this.opts.space; }
}

/** One end of an in-memory pipe that delivers a few bytes at a time (hex), so a frame always arrives in pieces. */
class PipeEnd implements Link {
  broken = false;
  peer!: PipeEnd;
  private inbox: string[] = [];
  private waiting: ((v: string | null) => void) | null = null;
  private closed = false;
  constructor(private piece = 14) {}
  async send(data: string) {
    if (this.broken) throw new Error('the link is broken');
    for (let i = 0; i < data.length; i += this.piece) this.peer.push(data.slice(i, i + this.piece));
  }
  push(chunk: string) {
    if (this.waiting) { const w = this.waiting; this.waiting = null; w(chunk); } else this.inbox.push(chunk);
  }
  async receive() {
    if (this.broken) throw new Error('the link is broken');
    const next = this.inbox.shift();
    if (next !== undefined) return next;
    if (this.peer.closed) return null;
    return new Promise<string | null>((resolve) => { this.waiting = resolve; });
  }
  close() {
    this.closed = true;
    if (this.peer.waiting) { const w = this.peer.waiting; this.peer.waiting = null; w(null); }
  }
}

function pipe(): [PipeEnd, PipeEnd] {
  const a = new PipeEnd();
  const b = new PipeEnd();
  a.peer = b;
  b.peer = a;
  return [a, b];
}

const ROUTE = { id: 'memory', atomicClaim: true, direct: false, authenticatedPairing: true };

function filled(): Store {
  const store = new Store();
  const carried = nppsMigration({ op: 'itemKeys' }).carried as Record<string, string[]>;
  for (const k of Object.values(carried).flat()) store.putString(k, `value-of-${k}`);
  return store;
}

const secretOf = (text: string): string => {
  const r = passphraseSecret(text);
  if (!r.ok) throw new Error(r.reason);
  return r.secret;
};

describe('MigrationFlow.run', () => {
  // @verifies SWR-MG-02
  // @verifies SWR-MG-08
  it('moves a phone to another through the shared flow and ends with a confirmation of the bundle hash', async () => {
    await initNppsCore();
    const route = new Route();
    const oldStore = filled();
    const done = await MigrationFlow.run('backup', { kind: 'migrate', method: 'passphrase', route: ROUTE }, new Host(route), {
      secret: secretOf('a long enough passphrase'),
      keys: oldStore,
    });
    expect(done.ok).toBe(true);
    expect(route.bundle).not.toContain(toHex([...'value-of-np.session.history'].map((c) => c.charCodeAt(0))));

    const fresh = new Store();
    const r = await MigrationFlow.run('restore', { route: ROUTE }, new Host(route), { secret: secretOf('a long enough passphrase'), keys: fresh });
    expect(r).toMatchObject({ ok: true, kind: 'migrate', confirmed: true });
    expect([...fresh.values.entries()].sort()).toEqual([...oldStore.values.entries()].sort());
    expect(route.confirmation).toEqual({ bundleHash: done.bundleHash });
  }, 60_000);

  // @verifies SWR-MG-01
  it('never moves a key that belongs to the install', async () => {
    await initNppsCore();
    const route = new Route();
    const oldStore = filled();
    const installKeys = ['np.research.device-token', 'np.warranty.consent-granted', 'ble.last-hub-id', 'np.consumable.snooze-counts'];
    for (const k of installKeys) oldStore.putString(k, 'belongs-to-this-phone');
    await MigrationFlow.run('backup', { kind: 'migrate', method: 'passphrase', route: ROUTE }, new Host(route), {
      secret: secretOf('a long enough passphrase'),
      keys: oldStore,
    });
    const fresh = new Store();
    await MigrationFlow.run('restore', { route: ROUTE }, new Host(route), { secret: secretOf('a long enough passphrase'), keys: fresh });
    for (const k of installKeys) expect(fresh.getString(k)).toBeUndefined();
    expect(fresh.getString('np.session.history')).toBe('value-of-np.session.history');
  }, 60_000);

  // @verifies SWR-MG-08
  // @verifies SWR-MG-09
  it('clears the old phone through the shared flow: the host does only its own steps and the core forgets the keys', async () => {
    await initNppsCore();
    const route = new Route();
    const oldStore = filled();
    oldStore.putString('ble.last-hub-id', 'hub-1');
    const done = await MigrationFlow.run('backup', { kind: 'migrate', method: 'passphrase', route: ROUTE }, new Host(route), {
      secret: secretOf('a long enough passphrase'),
      keys: oldStore,
    });
    const before = await MigrationFlow.run('clear', { bundleHash: done.bundleHash, canRemoveOsBond: false }, new Host(route), { keys: oldStore });
    expect(before).toEqual({ ok: false, reason: 'no_confirmation' });

    await MigrationFlow.run('restore', { route: ROUTE }, new Host(route), { secret: secretOf('a long enough passphrase'), keys: new Store() });
    const host = new Host(route);
    const after = await MigrationFlow.run('clear', { bundleHash: done.bundleHash, canRemoveOsBond: false }, host, { keys: oldStore });
    expect(after).toMatchObject({ ok: true, removeOsBond: true });
    expect(host.ran).toEqual(['destroyEncryptionKey', 'forgetPairingRecord']);
    expect([...oldStore.values.keys()]).toEqual(['np.migration.cleared']);
  }, 60_000);

  // @verifies SWR-MG-06
  // @verifies SWR-MG-08
  it('overwrites every byte of the files, syncs and deletes them after the key, and resumes after a failed call', async () => {
    await initNppsCore();
    const route = new Route();
    const oldStore = filled();
    const done = await MigrationFlow.run('backup', { kind: 'migrate', method: 'passphrase', route: ROUTE }, new Host(route), {
      secret: secretOf('a long enough passphrase'),
      keys: oldStore,
    });
    await MigrationFlow.run('restore', { route: ROUTE }, new Host(route), { secret: secretOf('a long enough passphrase'), keys: new Store() });

    const files = new MemoryFiles();
    files.files.set('a-big.db', new Uint8Array(40_000).fill(0x11));
    files.files.set('b-empty.db', new Uint8Array(0));
    files.files.set('c-small.db', new Uint8Array(100).fill(0x22));
    files.failOn = 'delete:c-small.db';
    const host = new Host(route);
    const r1 = await MigrationFlow.run('clear', { bundleHash: done.bundleHash, canRemoveOsBond: true }, host, { keys: oldStore, files });
    expect(r1).toEqual({ ok: false, reason: 'step_failed', step: 'overwriteAndDeleteFiles' });
    expect([...files.files.keys()]).toEqual(['c-small.db']);
    expect(host.ran).toEqual(['destroyEncryptionKey']);

    const writes = files.events.filter((e) => e.startsWith('write:a-big.db:')).map((e) => e.split(':').map(Number));
    expect(writes.reduce((n, w) => n + w[3], 0)).toBe(40_000);
    expect(writes.every((w) => w[3] <= 16 * 1024)).toBe(true);
    expect(files.events.indexOf('sync:a-big.db')).toBeLessThan(files.events.indexOf('delete:a-big.db'));
    expect(files.events.filter((e) => e.includes('b-empty.db'))).toEqual(['delete:b-empty.db']);

    files.failOn = null;
    const second = new Host(new Route());
    const r2 = await MigrationFlow.run('clear', { bundleHash: done.bundleHash, canRemoveOsBond: true }, second, { keys: oldStore, files });
    expect(r2).toMatchObject({ ok: true });
    expect(files.files.size).toBe(0);
    expect(second.ran).toEqual(['forgetPairingRecord']);
  }, 60_000);

  // @verifies SWR-MG-10
  // @verifies SWR-MG-11
  // @verifies SWR-MG-07
  it('runs a migration over an object store the app only has to provide, and refuses a second restore', async () => {
    await initNppsCore();
    const store = new MemoryStore();
    const oldStore = filled();
    const done = await MigrationFlow.run('backup', { kind: 'migrate', method: 'passphrase', route: ROUTE }, new Host(new Route()), {
      secret: secretOf('a long enough passphrase'),
      keys: oldStore,
      store,
    });
    expect(done.ok).toBe(true);
    expect([...store.objects.keys()]).toEqual([MigrationFlow.objectNames().bundle]);

    const fresh = new Store();
    const host = new Host(new Route());
    const r = await MigrationFlow.run('restore', { route: ROUTE }, host, { secret: secretOf('a long enough passphrase'), keys: fresh, store });
    expect(r).toMatchObject({ ok: true, confirmed: true });
    expect([...fresh.values.entries()].sort()).toEqual([...oldStore.values.entries()].sort());
    expect(host.seen.filter((t) => ['getBundle', 'putBundle', 'readClaim', 'createClaim'].includes(t))).toEqual([]);

    const second = await MigrationFlow.run('restore', { route: ROUTE }, new Host(new Route()), {
      secret: secretOf('a long enough passphrase'),
      keys: new Store(),
      store,
    });
    expect(second).toEqual({ ok: false, reason: 'claimed' });
  }, 60_000);

  // @verifies SWR-MG-10
  // @verifies SWR-MG-11
  // @verifies SWR-MG-07
  it('runs a migration over a direct link in small pieces, and refuses a second restore', async () => {
    await initNppsCore();
    const store = new MemoryStore();
    const oldStore = filled();
    const done = await MigrationFlow.run('backup', { kind: 'migrate', method: 'passphrase', route: ROUTE }, new Host(new Route()), {
      secret: secretOf('a long enough passphrase'),
      keys: oldStore,
      store,
    });
    expect(done.ok).toBe(true);

    const restoreOver = async (keys: Store) => {
      const [client, server] = pipe();
      const serving = MigrationFlow.serve(server, store, true);
      const r = await MigrationFlow.run('restore', { route: ROUTE }, new Host(new Route()), {
        secret: secretOf('a long enough passphrase'),
        keys,
        link: client,
      });
      client.close();
      expect(await serving).toEqual({ ok: true, reason: 'closed' });
      return r;
    };
    const fresh = new Store();
    expect(await restoreOver(fresh)).toMatchObject({ ok: true, confirmed: true });
    expect([...fresh.values.entries()].sort()).toEqual([...oldStore.values.entries()].sort());
    const names = MigrationFlow.objectNames();
    expect(store.objects.has(names.claim) && store.objects.has(names.confirmation)).toBe(true);
    expect(await restoreOver(new Store())).toEqual({ ok: false, reason: 'claimed' });
  }, 60_000);

  // @verifies SWR-MG-10
  it('serves nothing over a link that is not authenticated', async () => {
    await initNppsCore();
    const [, server] = pipe();
    expect(await MigrationFlow.serve(server, new MemoryStore(), false)).toEqual({ ok: false, reason: 'not_authenticated' });
  });

  // @verifies SWR-MG-11
  it('ends a restore with the route unavailable when the link breaks', async () => {
    await initNppsCore();
    const [client] = pipe();
    client.broken = true;
    const r = await MigrationFlow.run('restore', { route: ROUTE }, new Host(new Route()), {
      secret: secretOf('a long enough passphrase'),
      keys: new Store(),
      link: client,
    });
    expect(r).toEqual({ ok: false, reason: 'route_unavailable' });
  }, 60_000);

  // @verifies SWR-MG-11
  it('ends the flow with the route unavailable when an object call rejects', async () => {
    await initNppsCore();
    const r = await MigrationFlow.run('backup', { kind: 'migrate', method: 'passphrase', route: ROUTE }, new Host(new Route()), {
      secret: secretOf('a long enough passphrase'),
      keys: filled(),
      store: new MemoryStore({ failOn: 'put' }),
    });
    expect(r).toEqual({ ok: false, reason: 'route_unavailable' });
  }, 60_000);

  // @verifies SWR-MG-12
  it('probes an account on throwaway objects, says what it can do and leaves nothing behind', async () => {
    await initNppsCore();
    const host = () => new Host(new Route());
    const good = new MemoryStore();
    good.objects.set('users-file', '01');
    expect(await MigrationFlow.probe(good, 1000, host())).toMatchObject({ result: 'sufficient' });
    expect([...good.objects.keys()]).toEqual(['users-file']);

    const noConditional = new MemoryStore({ conditional: false });
    expect(await MigrationFlow.probe(noConditional, 1000, host())).toMatchObject({ result: 'backup_only', failedOperation: 'createIfAbsentSecondRefused' });
    expect(noConditional.objects.size).toBe(0);

    expect(await MigrationFlow.probe(new MemoryStore({ space: 10 }), 1000, host())).toMatchObject({ result: 'insufficient', failedOperation: 'space' });
    expect(await MigrationFlow.probe(new MemoryStore({ space: null }), 1000, host())).toMatchObject({ failedOperation: 'space' });
  }, 60_000);

  // @verifies SWR-MG-07
  it('opens the bundle before it takes the claim, so a wrong secret leaves no claim behind', async () => {
    await initNppsCore();
    const route = new Route();
    await MigrationFlow.run('backup', { kind: 'migrate', method: 'passphrase', route: ROUTE }, new Host(route), {
      secret: secretOf('a long enough passphrase'),
      keys: filled(),
    });
    const wrong = new Host(route);
    const r = await MigrationFlow.run('restore', { route: ROUTE }, wrong, { secret: secretOf('a different passphrase'), keys: new Store() });
    expect(r).toEqual({ ok: false, reason: 'authentication_failed' });
    expect(route.claim).toBeNull();
    expect(wrong.seen).not.toContain('createClaim');
  }, 60_000);
});

describe('bundleKey', () => {
  // @verifies SWR-MG-02
  it('derives the same key from a recovery code shown and the same code typed back', async () => {
    await initNppsCore();
    const made = newRecoveryCode(steps);
    const kdf = newKdf('recoveryCode', steps);
    const typed = recoveryCodeSecret(made.text.toLowerCase().replace(/-/g, ' '));
    expect(typed.ok).toBe(true);
    if (typed.ok) expect(deriveBundleKey(typed.secret, kdf)).toBe(deriveBundleKey(made.secret, kdf));
    const flipped = (made.text[0] === '2' ? '3' : '2') + made.text.slice(1);
    expect(recoveryCodeSecret(flipped)).toEqual({ ok: false, reason: 'bad_checksum' });
  }, 60_000);

  // @verifies SWR-MG-05
  it('refuses a header that asks for a cheaper derivation', async () => {
    await initNppsCore();
    const kdf = newKdf('passphrase', steps);
    expect(kdf).toMatchObject({ algorithm: 'argon2id', memoryKiB: 65536, iterations: 4, parallelism: 1, outputLength: 32 });
    expect(() => deriveBundleKey(secretOf('a long enough passphrase'), { ...kdf, memoryKiB: 8 })).toThrow();
  });

  // @verifies SWR-MG-02
  it('normalises a passphrase and refuses a short one', async () => {
    await initNppsCore();
    expect(passphraseSecret('café au lait noir')).toEqual(passphraseSecret('café au lait noir'));
    expect(passphraseSecret('too short')).toMatchObject({ ok: false, reason: 'too_short' });
  });
});
