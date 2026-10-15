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

export type FlowName = 'backup' | 'inspect' | 'restore' | 'clear';

export interface FlowOptions {
  /** What a backup or restore derives its key from, as hex (from `bundleKey.ts`). */
  secret?: string;
  /** The key-value store the plain-key items live in. When given, the core's plain-key items are declared to it as `keyItems`. */
  keys?: MigrationKeyStore;
}

// A restore of a large history is a few turns per item; this only stops a flow that never ends.
const MAX_TURNS = 100_000;

// eslint-disable-next-line @typescript-eslint/no-explicit-any
export async function runMigrationFlow(flow: FlowName, params: Record<string, unknown>, host: MigrationHost, options: FlowOptions = {}): Promise<any> {
  const p = { ...params };
  if (options.keys && p.keyItems === undefined) p.keyItems = Object.keys(nppsMigration({ op: 'itemKeys' }).carried);
  let answer = nppsMigration({ op: 'flow', flow, params: p });
  for (let turn = 0; turn < MAX_TURNS; turn++) {
    if (answer.done) return answer.done;
    const result = await perform(answer.effect, host, options);
    answer = nppsMigration({ op: 'flow', flow, state: answer.state, result });
  }
  throw new Error(`The ${flow} flow did not end in ${MAX_TURNS} turns`);
}

async function perform(e: FlowEffect, host: MigrationHost, { secret, keys }: FlowOptions): Promise<Record<string, unknown>> {
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
  }
  return host.perform(e);
}
