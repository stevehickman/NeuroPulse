/**
 * The backup and device-replacement flows, driven from TypeScript (SWR-MG-02…10, issue #569 packages 6e, 6f).
 *
 * The flows are state machines in the shared core (`neurone_npps_core::migration_flow`): the ORDER (open the bundle before
 * taking a claim, verify before confirming, confirm before the old phone clears) is decided there, once, for iOS, Android,
 * Windows, macOS and the web. This is the web's driver, and all of it: ask the core, perform the effect it names, hand back
 * the outcome, until the core says done. Three effects it answers itself, the same on every platform: `deriveKey` (the
 * core's own Argon2id over the secret this driver holds), and `readKeys` / `writeKeys` (the app's key-value store, whose key
 * list the core holds in common/npps/migration-keys.json). Everything else is the host's: the route, and any item the app
 * stores another way. The table of effects is in `migration_flow.rs`.
 *
 * A flow state is opaque, is never stored, and never holds a secret or a key.
 */
import { deriveBundleKey } from './bundleKey';
import { nppsMigration } from './nppsCore';

export interface FlowEffect {
  type: string;
  [arg: string]: unknown;
}

/** Where this app performs the effects the driver does not answer itself. */
export interface MigrationHost {
  /** Perform one effect and return its result object (see the table in `migration_flow.rs`). */
  perform(effect: FlowEffect): Promise<Record<string, unknown>> | Record<string, unknown>;
}

/** The app's key-value store, as the flows need it: get, put and remove, and nothing else. */
export interface MigrationKeyStore {
  get(key: string): string | null | undefined;
  put(key: string, value: string): void;
  remove(key: string): void;
}

/**
 * The calls a storage provider is asked for (SWR-MG-11), on named objects, with the bytes as lower-case hex. This is the whole of
 * what the web writes for a cloud account, the local computer or a shared folder; the core holds the rest (`object_route.rs`).
 * A call that cannot be done (no network, no permission, a refused write) rejects, and the flow ends with the route unavailable.
 */
export interface ObjectStore {
  /** The object's bytes, or null/undefined when there is none. */
  get(name: string): Promise<string | null | undefined>;
  /** Write the object, replacing any earlier one. */
  put(name: string, data: string): Promise<void>;
  /** Write the object if and only if none exists, arbitrated by the store itself (SWR-MG-07). True when this call wrote it. */
  createIfAbsent(name: string, data: string): Promise<boolean>;
  /** Remove the object. Not an error when there is none. */
  delete(name: string): Promise<void>;
  list(prefix: string): Promise<string[]>;
  /** Free space in bytes, or null when the provider cannot say. */
  freeSpace(): Promise<number | null>;
}

export type FlowName = 'backup' | 'inspect' | 'restore' | 'clear' | 'probe';

export interface FlowOptions {
  /** What a backup or restore derives its key from, as hex (from `bundleKey.ts`). */
  secret?: string;
  /** The key-value store the plain-key items live in. When given, the core's plain-key items are declared to it as `keyItems`. */
  keys?: MigrationKeyStore;
  /**
   * A route that is an object store. The core then drives the route itself (which object is the bundle, how a claim is
   * encoded) and `host.perform` is asked for no route effect.
   */
  store?: ObjectStore;
}

// A restore of a large history is a few turns per item; this only stops a flow that never ends.
const MAX_TURNS = 100_000;

// eslint-disable-next-line @typescript-eslint/no-explicit-any
export async function runMigrationFlow(flow: FlowName, params: Record<string, unknown>, host: MigrationHost, options: FlowOptions = {}): Promise<any> {
  const p = { ...params };
  if (options.store && p.objectStore === undefined) p.objectStore = true;
  if (options.keys && p.keyItems === undefined) p.keyItems = Object.keys(nppsMigration({ op: 'itemKeys' }).carried);
  let answer = nppsMigration({ op: 'flow', flow, params: p });
  for (let turn = 0; turn < MAX_TURNS; turn++) {
    if (answer.done) return answer.done;
    const result = await perform(answer.effect, host, options);
    answer = nppsMigration({ op: 'flow', flow, state: answer.state, result });
  }
  throw new Error(`The ${flow} flow did not end in ${MAX_TURNS} turns`);
}

async function perform(e: FlowEffect, host: MigrationHost, { secret, keys, store }: FlowOptions): Promise<Record<string, unknown>> {
  switch (e.type) {
    case 'deriveKey':
      if (secret === undefined) throw new Error('no secret to derive a key from');
      return { key: deriveBundleKey(secret, e.kdf as Record<string, unknown>) };
    case 'readKeys':
      if (!keys) break;
      return {
        values: Object.fromEntries((e.keys as string[]).flatMap((k) => {
          const v = keys.get(k);
          return v === null || v === undefined ? [] : [[k, v]];
        })),
      };
    case 'writeKeys':
      if (!keys) break;
      for (const k of e.remove as string[]) keys.remove(k);
      for (const [k, v] of Object.entries(e.set as Record<string, string>)) keys.put(k, v);
      return { ok: true };
    case 'objectGet':
    case 'objectPut':
    case 'objectCreate':
    case 'objectDelete':
    case 'objectList':
    case 'objectSpace':
      if (!store) break;
      return performObject(e, store);
  }
  return host.perform(e);
}

/** One object call. A call that rejects is reported to the core as failed; the core decides what that means (SWR-MG-11). */
async function performObject(e: FlowEffect, store: ObjectStore): Promise<Record<string, unknown>> {
  const name = (e.name as string | undefined) ?? '';
  try {
    switch (e.type) {
      case 'objectGet':
        return { data: (await store.get(name)) ?? null };
      case 'objectPut':
        await store.put(name, e.data as string);
        return { ok: true };
      case 'objectCreate':
        return { created: await store.createIfAbsent(name, e.data as string) };
      case 'objectDelete':
        await store.delete(name);
        return { ok: true };
      case 'objectList':
        return { names: await store.list(e.prefix as string) };
      default:
        return { bytes: await store.freeSpace() };
    }
  } catch {
    return { failed: true };
  }
}

/** The object names the core uses, for a host that releases a claim on the person's explicit act (SWR-MG-07). */
export function objectNames(): { bundle: string; claim: string; confirmation: string; probePrefix: string } {
  return nppsMigration({ op: 'objectNames' });
}

/** SWR-MG-12: check an account on throwaway objects that hold no user data. `random` is the host's, as for any flow. */
export async function probe(store: ObjectStore, bundleBytes: number, host: MigrationHost) {
  return runMigrationFlow('probe', { bundleBytes }, host, { store });
}
