/**
 * The backup and device-replacement flows, driven from TypeScript (SWR-MG-02…10, issue #569 package 6e).
 *
 * The flows are state machines in the shared core (`neurone_npps_core::migration_flow`): the ORDER (open the bundle before
 * taking a claim, verify before confirming, confirm before the old phone clears) is decided there, once, for iOS, Android,
 * Windows, macOS and the web. This is the web's driver, and all of it: ask the core, perform the effect it names through
 * the host, hand back the outcome, until the core says done. The host is where this app touches its own storage, its
 * route, its Argon2 and its randomness; the table of effects is in `migration_flow.rs`.
 *
 * A flow state is opaque, is never stored, and never holds a secret or a key: the host keeps the secret, and a key is
 * returned from `deriveKey`, used by the core for one answer and dropped.
 */
import { nppsMigration } from './nppsCore';

export interface FlowEffect {
  type: string;
  [arg: string]: unknown;
}

export interface MigrationHost {
  /** Perform one effect and return its result object (see the table in `migration_flow.rs`). */
  perform(effect: FlowEffect): Promise<Record<string, unknown>> | Record<string, unknown>;
}

export type FlowName = 'backup' | 'inspect' | 'restore' | 'clear';

// A restore of a large history is a few turns per item; this only stops a flow that never ends.
const MAX_TURNS = 100_000;

// eslint-disable-next-line @typescript-eslint/no-explicit-any
export async function runMigrationFlow(flow: FlowName, params: Record<string, unknown>, host: MigrationHost): Promise<any> {
  let answer = nppsMigration({ op: 'flow', flow, params });
  for (let turn = 0; turn < MAX_TURNS; turn++) {
    if (answer.done) return answer.done;
    const result = await host.perform(answer.effect);
    answer = nppsMigration({ op: 'flow', flow, state: answer.state, result });
  }
  throw new Error(`The ${flow} flow did not end in ${MAX_TURNS} turns`);
}
