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
 * A secret and a key are values passed through one call and held by the caller; nothing here stores either.
 * NeurOne never holds either (CLAUDE.md §5.1).
 */
import { nppsMigration } from './nppsCore';

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
export type SecretResult = { ok: true; secret: string } | { ok: false; reason: 'malformed' | 'bad_checksum' | 'too_short'; minChars?: number };

const hex = (bytes: Uint8Array): string => Array.from(bytes, (b) => b.toString(16).padStart(2, '0')).join('');

/** Random bytes from the platform's CSPRNG (the core is pure and has none). */
export type RandomBytes = (count: number) => Uint8Array;
export const systemRandom: RandomBytes = (count) => crypto.getRandomValues(new Uint8Array(count));

/** A fresh recovery code: show `text` once; the bundle can be opened only with it. `secret` is hex. */
export function newRecoveryCode(random: RandomBytes = systemRandom): { text: string; secret: string } {
  const bytes = random(32);
  if (bytes.length !== 32) throw new Error(`The random source gave ${bytes.length} bytes, not 32`);
  const secret = hex(bytes);
  return { text: nppsMigration({ op: 'encodeRecoveryCode', bytes: secret }).code, secret };
}

/** A recovery code typed or pasted by the user. */
export function recoveryCodeSecret(text: string): SecretResult {
  return nppsMigration({ op: 'parseRecoveryCode', code: text });
}

/** A passphrase the user chose (or typed to restore). Normalised and length-checked by the core. */
export function passphraseSecret(text: string): SecretResult {
  return nppsMigration({ op: 'passphraseSecret', passphrase: text });
}

/** The `kdf` block for the header of a bundle about to be sealed: a fresh random salt and the core's fixed cost. */
export function newKdf(method: BundleKeyMethod, random: RandomBytes = systemRandom): KdfBlock {
  const salt = random(32);
  if (salt.length !== 32) throw new Error(`The random source gave ${salt.length} bytes, not 32`);
  return nppsMigration({ op: 'bundleKdf', method, salt: hex(salt) }).kdf;
}

/**
 * The bundle key (hex) for a secret (hex) and a header's `kdf` block: Argon2id computed by the shared core. The core
 * checks the block against the fixed cost first, so a bundle cannot ask this device for a cheaper derivation. Throws for a
 * block the core refuses.
 */
export function deriveBundleKey(secret: string, kdf: KdfBlock | Record<string, unknown>): string {
  const r = nppsMigration({ op: 'deriveBundleKey', secret, kdf });
  if (!r.ok) throw new Error(`unsupported kdf: ${r.reason}`);
  return r.key;
}
