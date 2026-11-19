/**
 * The user-held key of a migration or backup bundle, for the web (SWR-MG-02, issue #569 packages 6d, 6f).
 *
 * The UHDR key is bound to the device, so a bundle moving to another device is sealed under a key made from something the
 * user holds: a recovery code the app shows once, or a passphrase the user chooses. Every rule (the cost, the code's text
 * form and checksum, the passphrase's normalisation and minimum length, what a header's kdf block may say) and the Argon2id
 * derivation itself are the shared core's (neurone_npps_core::bundle_key), so every app derives the same key and none
 * carries a second implementation. This file only marshals them. The Kotlin, Swift and C# counterparts are
 * BundleKey.kt, BundleKey.swift and BundleKey.cs.
 *
 * **A secret is bytes and a key is a handle** (SWR-MG-32). A secret (what a recovery code or a passphrase stands for) is
 * a `Uint8Array` the caller holds and overwrites (`fill(0)`) when done; it reaches the core beside the request, never as
 * text in it. A bundle key is never here at all: the core derives it and keeps it, `deriveBundleKey` returns the handle,
 * and `releaseBundleKey` has the core wipe it. Neither is ever a JavaScript string, which could not be cleared. Nothing
 * here stores either, and NeurOne never holds either (CLAUDE.md §5.1).
 */
import { nppsMigration, nppsMigrationSecret } from './nppsCore';

export type BundleKeyMethod = 'recoveryCode' | 'passphrase';

export interface KdfBlock {
  method: BundleKeyMethod;
  algorithm: string;
  salt: string;
  memoryKiB: number;
  iterations: number;
  parallelism: number;
  outputLength: number;
}

/** What a secret from a person became. `reason` is a code for the screen to word from the locale files. */
export type SecretResult = { ok: true; secret: Uint8Array } | { ok: false; reason: 'malformed' | 'bad_checksum' | 'too_short'; minChars?: number };

/** A bundle key the core keeps: what a request carries where the key belongs. Not a secret; release it with `releaseBundleKey`. */
export interface BundleKey { $handle: number }

const hex = (bytes: Uint8Array): string => Array.from(bytes, (b) => b.toString(16).padStart(2, '0')).join('');

/** Random bytes from the platform's CSPRNG (the core is pure and has none). */
export type RandomBytes = (count: number) => Uint8Array;
export const systemRandom: RandomBytes = (count) => crypto.getRandomValues(new Uint8Array(count));

/** A fresh recovery code: show `text` once; the bundle can be opened only with it. `secret` is the caller's to overwrite. */
export function newRecoveryCode(random: RandomBytes = systemRandom): { text: string; secret: Uint8Array } {
  const secret = random(32);
  if (secret.length !== 32) throw new Error(`The random source gave ${secret.length} bytes, not 32`);
  return { text: nppsMigrationSecret({ op: 'encodeRecoveryCode', bytes: { $secret: 0 } }, [secret]).answer.code, secret };
}

/**
 * `text` is already a string when a screen hands it over, and that copy is the browser's. From here on it is bytes: it
 * goes to the core beside the request, and the secret it stands for comes back beside the answer.
 */
function secretFrom(op: 'parseRecoveryCode' | 'passphraseSecret', field: 'code' | 'passphrase', text: string): SecretResult {
  const typed = new TextEncoder().encode(text);
  try {
    const r = nppsMigrationSecret({ op, [field]: { $secret: 0 } }, [typed]);
    if (!r.answer.ok) return r.answer.minChars === undefined ? { ok: false, reason: r.answer.reason } : { ok: false, reason: r.answer.reason, minChars: r.answer.minChars };
    return { ok: true, secret: r.secrets[r.answer.secret.$secret] };
  } finally {
    typed.fill(0);
  }
}

/** A recovery code typed or pasted by the user. */
export function recoveryCodeSecret(text: string): SecretResult {
  return secretFrom('parseRecoveryCode', 'code', text);
}

/** A passphrase the user chose (or typed to restore). Normalised and length-checked by the core. */
export function passphraseSecret(text: string): SecretResult {
  return secretFrom('passphraseSecret', 'passphrase', text);
}

/** The `kdf` block for the header of a bundle about to be sealed: a fresh random salt and the core's fixed cost. */
export function newKdf(method: BundleKeyMethod, random: RandomBytes = systemRandom): KdfBlock {
  const salt = random(32);
  if (salt.length !== 32) throw new Error(`The random source gave ${salt.length} bytes, not 32`);
  return nppsMigration({ op: 'bundleKdf', method, salt: hex(salt) }).kdf;
}

/**
 * The bundle key for a secret and a header's `kdf` block: Argon2id computed by the shared core, **and kept there**. What
 * comes back is the key's handle; the key itself never reaches JavaScript. The core checks the block against the fixed
 * cost first, so a bundle cannot ask this device for a cheaper derivation. Throws for a block the core refuses. Release
 * the key with `releaseBundleKey` when the work is done: the core holds only so many, and a page that reloads (or a core
 * that trapped) has lost them all, so `unknown_key` means "derive it again".
 */
export function deriveBundleKey(secret: Uint8Array, kdf: KdfBlock | Record<string, unknown>): BundleKey {
  const r = nppsMigrationSecret({ op: 'deriveBundleKey', secret: { $secret: 0 }, kdf, keep: 'bundle' }, [secret]).answer;
  if (!r.ok) throw new Error(`unsupported kdf: ${r.reason}`);
  return r.key;
}

/** Have the core wipe a key `deriveBundleKey` made. Safe to call twice. */
export function releaseBundleKey(key: BundleKey): void {
  nppsMigration({ op: 'keyRelease', key });
}
