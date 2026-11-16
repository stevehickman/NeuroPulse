# Question for the principal: OI-PRIV-12 stage 2, how keys cross the shared core's boundary

**Status:** DRAFT, not sent
**GitHub issue:** none yet. The item was raised by pull request [stevehickman/NeuroPulse#885](https://github.com/stevehickman/NeuroPulse/pull/885); an issue is to be opened when this is sent.
**From:** NeurOne Software Engineering (`OI-PRIV-12` in `docs/status/pending-decisions.md`; `NP-SW-001` §6.3.7 `SWR-MG-28`)
**To:** the principal
**Response needed:** an option for each of Q1 to Q4, a short rationale, anything that would reopen it, and the date.

Nothing in this file is built. Stage 1 of `OI-PRIV-12` is built and is `SWR-MG-28`; this is the part that changes an
interface on four bindings, so it waits for the scope to be agreed.

## 1. The question

Keys and the passphrase reach the shared core (`common/npps-core`) as hex text inside a JSON request, and on the host
they are JVM, JavaScript, Swift or C# strings. A string cannot be cleared: it stays in memory until the runtime collects
it, and may be copied before then. **Should key-bearing operations get an entry point that takes and returns bytes the
caller clears (A), should the core hold keys itself and hand out handles (B), or both, and should the core's own heap be
cleared on release in the meantime (C)?**

## 2. What is known

**Where a key is a string today** (read in the source, 2026-11-16):

| Secret | Where it becomes a string | How often it crosses |
|---|---|---|
| Data key (32 bytes) | `common/core/.../common/EncryptedKeyValueStore.kt`: hex in a `sealValue` or `openValue` request | Every read and write of a protected value |
| Wrapping key and passphrase | `common/core/.../common/PassphraseGatedSecretKeyStore.kt` | Each unlock |
| Backup (bundle) key | `common/core/.../migration/MigrationFlow.kt`: the `deriveKey` effect's `result` | Each backup and restore, and each recording chunk |
| Signing seed | `common/core/.../platform/Ed25519.kt` | Each signature |
| AWS secret | `common/core/.../cloud/NppsAwsSigV4.kt` | Each signed request |
| All of the above on Android and the desktop JVM | `nativeMigration(request: JString)` in `common/npps-jni/src/lib.rs` | Every call |
| Link channel keys | `ctx.sk`, `ctx.rk` in a link flow's state (`link.rs`), which the host holds as a string between turns | Every turn of a paired link |

The same requests are a Swift `Data` made from a dictionary (`app/ios/NeurOne/Session/NppsCore.swift`), a C# `string`
(`app/windows/NeurOne/Protocol/NppsCore.cs`), a Kotlin string on wasm (`NppsBackendWasm.kt`) and a JS string
(`common/lib/nppsCore.ts`).

**What stage 1 did and did not do.** `npps_free` and the web wrapper now clear every buffer they release, and the core
clears its decoded keys (`SWR-MG-28` (a), (b)). **Measured in the WebAssembly build on 2026-11-16**, after those fixes:
one call of `passphraseSecret` leaves **one copy of the passphrase** in linear memory, and one call of `sealValue`
leaves **one copy of the hex key and one of the plaintext value**. They are the core's own parse of the request: the
JSON parser copies each text field into a string of its own and releases it as it is. A later call may overwrite them
by chance, not by design. Stage 1 does not reach these, and neither A nor B alone does for fields that stay text.

**What is assumed, not known.** That no host logs a request or a flow state (none was found). How long a JVM or a
browser keeps a dead string was not measured and is not bounded by anything we control.

## 3. The options

### C. Clear the core's heap on release (a complement to A and B, not an alternative)

`common/npps-ffi` and `common/npps-jni` each install an allocator that fills a block with zeros when it is released
(about twenty lines each, over the system allocator; `zeroize` is already a dependency).

- **Closes:** every copy the core itself makes on the heap, whatever the operation, including the parsed request
  measured above, strings that grew, and intermediate results. No list of "key vectors" to keep complete.
- **Does not close:** the host's strings; the stack.
- **ABI impact:** none on any binding. The header, the JNI signatures and the WebAssembly exports are unchanged.
- **Cost:** one pass over each block at release. A compile of a protocol releases a few thousand small blocks; this
  was not measured and should be before it ships. The test allocators of `SWR-MG-28` (a) and (b) would move into the
  crate's own allocator (one global allocator per binary).

### A. A byte entry point the caller clears

One new call beside `npps_migration_json`, for the operations that take or return a secret:

```c
int32_t npps_migration_keyed(const uint8_t *req, size_t req_len,          /* JSON, with no secret in it */
                             const uint8_t *secret_in, size_t secret_in_len, /* the key, seed or passphrase, raw */
                             uint8_t *secret_out, size_t secret_out_cap, size_t *secret_out_len, /* a derived key, raw */
                             uint8_t **out, size_t *out_len);              /* JSON, with no secret in it */
```

The request names which field the bytes stand for (`"key": {"arg": true}`), so the rules stay in the core.

| Binding | Change | Who clears |
|---|---|---|
| JNI (`npps-jni`, Android and desktop JVM) | `nativeMigrationKeyed(request: String, secretIn: ByteArray?, secretOut: ByteArray?): String`; the Rust side reads the array region into a `Zeroizing` buffer | Kotlin: `fill(0)` in a `finally` |
| C ABI, iOS (Swift and Kotlin/Native cinterop) | The call above; `neurone_npps.h` gains one function | Swift: `resetBytes` on the `Data`, or a pointer buffer it owns |
| C ABI, Windows (C#) | One more `DllImport`; `byte[]` pinned for the call | `CryptographicOperations.ZeroMemory` |
| WebAssembly (`nppsCore.ts`, `NppsBackendWasm.kt`) | Two more regions from `npps_alloc`; no new mechanism | `Uint8Array.fill(0)`, then `npps_free` (which clears again) |

- **Closes:** the hex-in-JSON crossing for the data key, the wrapping key, the signing seed, the AWS secret and the
  backup key, **if** the host holds them as bytes. `SecretKeyStore` already returns a `ByteArray`. The AWS secret and
  the passphrase arrive from a text field as a string on every toolkit; A reduces their copies to that one, and cannot
  remove it.
- **Does not close:** the backup key inside a flow (it arrives as a `result` of the `deriveKey` effect, so the flow
  call needs the same byte channel), and the link's channel keys in the flow state.
- **Cost:** every host's call sites for the seven secrets change type from text to bytes; the hex forms stay accepted
  until every host has moved (see Q4).

### B. Key handles held inside the core

The core keeps a table of keys and the host holds a small number: `keyImport` (bytes in, through A's `secret_in`)
or `keyDerive` (from a handle or from a passphrase) returns a handle; operations take `"key": {"handle": 7}`;
`keyRelease` clears and forgets it; `keyExport` (bytes out, through A's `secret_out`) exists only for the keys that must
leave (a key the platform keystore is to wrap).

- **Closes:** what A does, and also the two things A cannot: the backup key never leaves the core between
  `deriveKey` and its use (the flow's `result` carries a handle), and the link's channel keys leave the flow state
  (`ctx` carries two handles). The data key crosses once per run, not once per read and write.
- **ABI impact:** the same one new call as A (for import and export); the rest is new `op` names inside the existing
  JSON calls, so no further signature changes on any binding.
- **Cost, which is the real one:** the core stops being stateless. Today every document says it keeps nothing
  (`SWR-MG-02`'s status, `at_rest.rs`, `migration_flow.rs`), every call is independent, and a WebAssembly trap is
  survived by dropping the instance. With handles: a handle that is not released is a key that stays (lifetime rules
  and a test for leaks); a dropped WebAssembly instance loses every handle, so the host must import again; the table
  needs a lock on the JVM, iOS and Windows; and a flow state is no longer the whole of a flow, so it cannot be replayed
  against a fresh core. The handle is a counter, not random (the core has no entropy), which is acceptable because it
  never leaves the process.

## 4. Recommendation

1. **C first.** It is the smallest change, it has no interface impact, and it closes the residue that was measured
   rather than argued.
2. **A next, for one-shot secrets** (the signing seed, the AWS secret, the passphrase, the wrapping key) and as the
   import and export path B needs.
3. **B for the keys that live for a run or a flow** (the data key, the backup key inside a flow, the link's channel
   keys), and nowhere else, so the core's new state is three kinds of key with stated lifetimes.

A alone leaves the link state and the flow's `result` as they are. B alone still needs A's byte channel to get a key in.

## 5. The archive nonce (assessed, not changed)

A recording is sealed in chunks under the backup key with the nonce `prefix (8 random bytes) ‖ counter (4 bytes)`
(`archive.rs`). The prefix is drawn once per file by the host; the key is the same for every file of a backup, for as
long as the person keeps the same recovery code or passphrase.

- **The risk is two files drawing the same prefix**, which repeats a nonce under one key; for AES-GCM that exposes the
  XOR of the two plaintexts and allows forgery. Within one file the counter cannot repeat (the core refuses a counter
  above 2³² − 1).
- **How likely.** With *n* files under one key the chance is about *n*² / 2⁶⁵. For 10,000 recordings that is about
  3 × 10⁻¹²; it reaches 2⁻³² at about 92,000 recordings (2¹⁶·⁵). NIST SP 800-38D §8.3 asks that the chance of a
  repeated nonce under one key stay at or below 2⁻³² (**cited from memory; not read against the standard in this
  work**). At three recordings a day, 92,000 is over eighty years.
- **The bundle shares the key** and uses a 12-byte random nonce each night. A bundle nonce meeting an archive nonce is
  of the order of (bundles × chunks) / 2⁹⁶ and is not the binding term.
- **What it rests on.** Nothing in the core counts files per key or refuses the 92,001st, and the bound assumes the
  host's random source is sound. That is an unstated assumption, now `UC-194`.
- **A per-file subkey** (`HKDF-SHA-256(backup key, salt = 32 random bytes in the header, info = "np.archive.v2")`, with
  the nonce a plain counter) removes the question: two files would have to draw the same 32 bytes. It costs one HKDF
  per file, 24 more header bytes, and a format version (`VERSION` 2, with version 1 still opened). It needs no new
  crate.
- **View:** warranted as inexpensive hardening, not urgent. It is best done with the next change to the archive
  format, or with B (the subkey would be a handle). Until then the bound should be written down, which `UC-194` does.

## 6. Questions

- **Q1.** Build C (clear the core's heap on release) now? **Yes / No.**
- **Q2.** The boundary: **A only**, **A and B as in §4**, or **neither** (accept strings, and say so in `SWR-MG-28`)?
- **Q3.** The archive: a per-file subkey **now** (format version 2), **with the next format change**, or **not at
  all** (keep `UC-194` as the stated bound)?
- **Q4.** Once every host uses the byte entry point, should the core **refuse** a key given as hex text (so no host
  can go back, `CLAUDE.md` §5.1 "protection only goes up"), or keep accepting it?
