/**
 * The backup and device-replacement flows, driven from TypeScript (SWR-MG-02…10, issue #569 packages 6e, 6f).
 *
 * The flows are state machines in the shared core (`neurone_npps_core::migration_flow`): the ORDER (open the bundle before
 * taking a claim, verify before confirming, confirm before the old phone clears) is decided there, once, for iOS, Android,
 * Windows, macOS and the web. This is the web's driver, and all of it: ask the core, perform the effect it names, hand back
 * the outcome, until the core says done. It answers these effects itself, the same on every platform: `random` (the
 * platform's CSPRNG), `deriveKey` (the core's own Argon2id over the secret this driver holds), `readKeys` / `writeKeys`
 * (the app's key-value store, whose key list the core holds in common/npps/migration-keys.json), and the object, file and
 * link calls. Everything else is the host's: the route, and any item the app stores another way. The table of effects is in `migration_flow.rs`.
 *
 * A flow state is opaque, is never stored, and never holds a secret or a key.
 */
import { deriveBundleKey, systemRandom, type RandomBytes } from './bundleKey';
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

/** The app's key-value store, as the flows need it: get, put and remove, and nothing else. The same role and the same names on every platform. */
export interface KeyValueStore {
  getString(key: string): string | null | undefined;
  putString(key: string, value: string): void;
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

/** A file the app holds, as `FileStore.list` reports it. */
export interface StoredFile {
  path: string;
  size: number;
}

/**
 * The calls the old phone's clear asks of the app's files (SWR-MG-06), and nothing else: the core decides how a file is
 * overwritten (every byte, in chunks, then a sync, then the delete). `list` reports the UHDR-class files still there; the bytes
 * of a write are lower-case hex. A call that cannot be done rejects, and the clear stops at that step and resumes at the next
 * launch. The same role and names on every platform (Kotlin `FileStore`, Swift `FileStore`, C# `IFileStore`).
 */
export interface FileStore {
  list(): Promise<StoredFile[]>;
  /** Overwrite the bytes of `path` starting at `offset`, in place. */
  write(path: string, offset: number, data: string): Promise<void>;
  /** The writes so far have reached storage. */
  sync(path: string): Promise<void>;
  /** Not an error when there is no such file. */
  delete(path: string): Promise<void>;
}

/**
 * A pipe to the other phone (SWR-MG-10): bytes in, bytes out, as lower-case hex like the object store's. Bluetooth, a local
 * socket or a cable is each one implementation; everything above the bytes (framing, which calls a peer may make, the claim)
 * is in the core (`link.rs`). The same role and names on every platform (Kotlin `Link`, Swift `Link`, C# `ILink`).
 */
export interface Link {
  /** Send the bytes; rejects if the link broke. */
  send(data: string): Promise<void>;
  /** The next bytes, or null once the other end has closed; rejects if the link broke. */
  receive(): Promise<string | null>;
}

export type FlowName = 'backup' | 'inspect' | 'restore' | 'clear' | 'probe' | 'serve';

export interface FlowOptions {
  /** What a backup or restore derives its key from, as hex (from `bundleKey.ts`). */
  secret?: string;
  /** The key-value store the plain-key items live in. When given, the core's plain-key items are declared to it as `keyItems`. */
  keys?: KeyValueStore;
  /**
   * A route that is an object store. The core then drives the route itself (which object is the bundle, how a claim is
   * encoded) and `host.perform` is asked for no route effect.
   */
  store?: ObjectStore;
  /** The app's files, for the old phone's clear: the core overwrites and deletes them itself (SWR-MG-06). */
  files?: FileStore;
  /** Where the `random` effect's bytes come from: the platform's CSPRNG unless a test passes its own. */
  random?: RandomBytes;
  /** The pipe to the phone that holds the bundle: a restore then runs its object calls over it (SWR-MG-10). */
  link?: Link;
}

// A restore of a large history is a few turns per item; this only stops a flow that never ends.
const MAX_TURNS = 100_000;

// eslint-disable-next-line @typescript-eslint/no-explicit-any
async function run(flow: FlowName, params: Record<string, unknown>, host: MigrationHost, options: FlowOptions = {}): Promise<any> {
  const p = { ...params };
  if ((options.store || options.link) && p.objectStore === undefined) p.objectStore = true;
  if (options.link && flow !== 'serve' && p.link === undefined) p.link = true;
  if (options.keys && p.keyItems === undefined) p.keyItems = Object.keys(nppsMigration({ op: 'itemKeys' }).carried);
  let answer = nppsMigration({ op: 'flow', flow, params: p });
  for (let turn = 0; turn < MAX_TURNS; turn++) {
    if (answer.done) return answer.done;
    const result = await perform(answer.effect, host, options);
    answer = nppsMigration({ op: 'flow', flow, state: answer.state, result });
  }
  throw new Error(`The ${flow} flow did not end in ${MAX_TURNS} turns`);
}

async function perform(e: FlowEffect, host: MigrationHost, { secret, keys, store, files, link, random = systemRandom }: FlowOptions): Promise<Record<string, unknown>> {
  switch (e.type) {
    case 'random': {
      const n = e.bytes as number;
      const bytes = random(n);
      if (bytes.length !== n) throw new Error(`The random source gave ${bytes.length} bytes, not ${n}`);
      return { bytes: Array.from(bytes, (b) => b.toString(16).padStart(2, '0')).join('') };
    }
    case 'deriveKey':
      if (secret === undefined) throw new Error('no secret to derive a key from');
      return { key: deriveBundleKey(secret, e.kdf as Record<string, unknown>) };
    case 'readKeys':
      if (!keys) break;
      return {
        values: Object.fromEntries((e.keys as string[]).flatMap((k) => {
          const v = keys.getString(k);
          return v === null || v === undefined ? [] : [[k, v]];
        })),
      };
    case 'writeKeys':
      if (!keys) break;
      for (const k of e.remove as string[]) keys.remove(k);
      for (const [k, v] of Object.entries(e.set as Record<string, string>)) keys.putString(k, v);
      return { ok: true };
    case 'objectGet':
    case 'objectPut':
    case 'objectCreate':
    case 'objectDelete':
    case 'objectList':
    case 'objectSpace':
      if (!store) break;
      return performObject(e, store);
    case 'linkSend':
    case 'linkRecv':
      if (!link) break;
      return performLink(e, link);
    case 'listFiles':
    case 'writeFile':
    case 'syncFile':
    case 'deleteFile':
      if (!files) break;
      return performFile(e, files);
  }
  return host.perform(e);
}

/** One link call. A link that breaks is reported to the core as failed; the core decides what that means (SWR-MG-11). */
async function performLink(e: FlowEffect, link: Link): Promise<Record<string, unknown>> {
  try {
    if (e.type === 'linkSend') {
      await link.send(e.frame as string);
      return { ok: true };
    }
    return { chunk: (await link.receive()) ?? null };
  } catch {
    return { failed: true };
  }
}

/** One file call of the clear. A call that rejects is reported to the core as failed, and the clear stops at that step (SWR-MG-06). */
async function performFile(e: FlowEffect, files: FileStore): Promise<Record<string, unknown>> {
  const path = (e.path as string | undefined) ?? '';
  try {
    switch (e.type) {
      case 'listFiles':
        return { files: await files.list() };
      case 'writeFile':
        await files.write(path, e.offset as number, e.data as string);
        return { ok: true };
      case 'syncFile':
        await files.sync(path);
        return { ok: true };
      default:
        await files.delete(path);
        return { ok: true };
    }
  } catch {
    return { failed: true };
  }
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
function objectNames(): { bundle: string; claim: string; confirmation: string; probePrefix: string } {
  return nppsMigration({ op: 'objectNames' });
}

/**
 * The locale key that words each reason, rejected secret, storage-check result and item (SWR-MG-03), from the core, so no
 * platform spells one its own way. Groups: `reasons`, `secret`, `probe`, `items`; each maps a code to a key.
 */
function messageKeys(): Record<'reasons' | 'secret' | 'probe' | 'items', Record<string, string>> {
  return nppsMigration({ op: 'messageKeys' });
}

/** SWR-MG-12: check an account on throwaway objects that hold no user data. */
async function probe(store: ObjectStore, bundleBytes: number, host: MigrationHost) {
  return run('probe', { bundleBytes }, host, { store });
}

const noHost: MigrationHost = {
  perform() {
    throw new Error('a serving phone is asked for no host effect');
  },
};

/**
 * SWR-MG-10: serve this phone's migration objects from `store` to the phone on the other end of `link`, until it closes.
 * `authenticated` is the app's assertion that the link was paired (the pairing is not designed yet); the core refuses to
 * serve without it. Resolves to `{ok, reason}`.
 */
async function serve(link: Link, store: ObjectStore, authenticated: boolean) {
  return run('serve', { authenticated }, noHost, { store, link });
}

/**
 * The flows, under the one name every platform uses for this role: `MigrationFlow.run`, `MigrationFlow.probe`,
 * `MigrationFlow.serve` and `MigrationFlow.objectNames` (Swift `MigrationFlow`, C# `MigrationFlow`, Kotlin `MigrationFlow`).
 */
export const MigrationFlow = { run, probe, serve, objectNames, messageKeys };
