# OI-PRIV-12 stage 2: how a key crosses the boundary of the shared core

**Status:** DRAFT, not sent.
**GitHub issue:** none yet. Rule 7 of `docs/human-input/README.md` asks for one; the session that wrote this did not open an issue unasked. The findings come from pull request [stevehickman/NeuroPulse#885](https://github.com/stevehickman/NeuroPulse/pull/885).
**Sender:** the session that did stage 1 of `OI-PRIV-12` (a Claude agent), drafted 2026-11-18.
**Addressee:** the Principal, with Software Engineering.
**Owning open item:** `OI-PRIV-12` (`docs/status/pending-decisions.md`). §7 also bears on `OI-PRIV-09` (2).
**Not a controlled document and not a design input** (`docs/human-input/README.md`): no serial, not in `NP-DHF-001`, and it writes no requirement (`CLAUDE.md` §18). **Nothing here is built.** Stage 1 is built and is `SWR-MG-28` in `docs/np_sw_001.md` §6.3.7.

**How to read it.** **[E]** is established, with the file that shows it. **[P]** is proposed here. **[choice]** is a figure with no derivation; if adopted it gets a row in `docs/status/unjustified-choices.md` (`CLAUDE.md` §19). Two outside sources are cited from memory and were **not re-read**, because their hosts were not reachable from the session: NIST SP 800-38D (the limit on a repeated GCM nonce) and the MITRE CWE entries named in `SWR-MG-28`.

---

## 1. What I need from you

| # | Question | Recommendation |
|---|----------|----------------|
| Q1 | How should a key cross the boundary: a byte-array entry point the caller clears (A), key handles held in the core (B), or both, each where it fits (C)? | **C**, in the order of §6 |
| Q2 | May the core hold keys between calls? Today it holds nothing, and three module texts say so. B and C need it. | **Yes**, for keys the core itself derives, in a table with the rules of §4.2 |
| Q3 | Once every binding has moved, should the core refuse a key sent as hex inside JSON? | **Yes**: otherwise a later caller can go back to a string and nothing notices |
| Q4 | The archive's nonce (§7): give each file its own key in the next format version, or keep the format and record the bound? | **A per-file key, made in the same format change as `OI-PRIV-09` (2)**, not on its own |
| Q5 | If Q4 is yes: an app older than the change refuses a new archive as `newer_version`. Is that acceptable for a restore onto an old build? | Yes; it is what the format already does for any newer version |

What to send back: the option for each, a short reason (`CLAUDE.md` §18), anything that would reopen it, and your name, role and date.

## 2. The problem, as it is after stage 1

Stage 1 wiped what the Rust side owns: `npps_free` zeroes a buffer, the web binding zeroes its copy of a request, the core's own key buffers are wiped on drop, and a link's state keeps only the channel keys. **What it could not reach is every copy on the other side of the boundary and in the JSON between.** [E]

Every operation of `migration_json` takes one JSON text and answers with one. A key in it is a run of hex digits inside that text. Counting the copies of the data key for one `sealValue` on the JVM (`common/core/.../common/EncryptedKeyValueStore.kt`): [E]

1. `ByteArray` from the keystore, cached in the store (`cached`). The only copy code can clear.
2. `toHex()`: a `String`. Immutable; it lives until the collector frees it, and is not overwritten then.
3. `buildJsonObject`, then `toString()`: the hex inside a `JsonPrimitive`, then inside the request `String`.
4. JNI `get_string` (`common/npps-jni/src/lib.rs`): a Rust `String`, modified UTF-8 decoded. Not wiped.
5. `serde_json::from_str`: the hex as a `Value::String`. Not wiped.
6. `unhex`: the 32 bytes. Wiped on drop since stage 1.
7. The AES-GCM key schedule. Wiped on drop since stage 1.

Copies 2 to 5 cannot be wiped by any change to a call site. The same shape repeats for every secret:

| Secret | Where it crosses | Direction | Notes |
|--------|------------------|-----------|-------|
| Data key (32 bytes) | `EncryptedKeyValueStore.kt`: `sealValue`, `openValue` | in | once per protected value read or written |
| Passphrase | `PassphraseGatedSecretKeyStore.kt` → `BundleKeyDerivation.passphraseSecret` | in, then the normalised secret comes **out** as hex and goes **in** again to `deriveBundleKey` | already a `String` when the screen hands it over |
| Wrapping key | `PassphraseGatedSecretKeyStore.kt`: out of `deriveBundleKey`, in to `openValue` / `sealValue` | out and in | `openValue` then answers with **the data key** as its `value` |
| Backup (bundle) key | `MigrationFlow.kt` (`deriveKey` effect result), `RecordingArchive.kt` | out and in | `key.hex()` is called **once per 64 KiB chunk**: a 50 MB recording makes about 800 strings holding the key |
| Signing seed | `platform/Ed25519.kt`: `ed25519PublicKey`, `ed25519Sign` | in | |
| AWS secret, session token | `cloud/NppsAwsSigV4.kt`: `awsSigV4` | in | once per signed request |
| UHDR key (64 bytes) | `deriveUhdrKey` | seed in, key out | |
| OHTTP response secret | `ohttpSeal` out, `ohttpOpen` in | out and in | lives for one request |
| Link channel keys | the flow state (`link.rs`, `ctx`) | out and in, every call | what stage 1 had to leave (`SWR-MG-28` (d)) |
| Pairing code, SPAKE2 seed | the flow state until the shared key is made | out and in | bounded by stage 1, not removed |

The web is the same with JS strings in place of JVM ones (`common/lib/nppsCore.ts`, and `NppsBackendWasm.kt` for the Compose web host). Swift and C# pass a byte buffer they own (`app/ios/NeurOne/Session/NppsCore.swift`, `app/windows/NeurOne/Protocol/NppsCore.cs`), but the key is hex text in the dictionary or JSON node that buffer is serialised from, and a C# `string` is as immutable as a JVM one. [E]

**What no option here fixes**, so it is not held against any of them:

- A passphrase typed into a text field is already an immutable string in the UI toolkit before any code of ours sees it.
- A recovery code and a pairing code are shown to the person. They are text by purpose.
- Plaintext UHDR items in `sealBundle` and `openBundle` are JSON by nature. They are data, not keys; a streaming form is a separate piece of work.
- A managed runtime's collector may move a byte array and leave the old copy. Clearing the array clears the current copy. This is why "the caller clears it" is weaker on the JVM than it sounds, and it is an argument for B over A.
- Swap, hibernation files and crash dumps.

## 3. Option A: a byte-array entry point the caller clears

**[P]** One new function. The JSON request carries no secret; where one belongs it carries a reference, and the bytes travel beside it in a buffer the caller owns.

```c
int32_t npps_migration_secret_json(
    const uint8_t *req, size_t req_len,          /* JSON; a secret is {"$secret": 0}, {"$secret": 1}, ... */
    const uint8_t *secrets, size_t secrets_len,  /* each: 4-byte big-endian length, then the bytes */
    uint8_t **out, size_t *out_len,              /* JSON answer; a secret in it is {"$secret": n} */
    uint8_t **secret_out, size_t *secret_out_len /* same framing; released with npps_free, which wipes it */
);
```

The core reads `secrets` straight into wiped buffers and never turns them into text. The caller overwrites its array when the call returns, and copies `secret_out` into an array it will clear, then frees it.

- **For.** The core stays stateless, which is what its module texts promise and what makes the flows easy to test. Small: one function, one framing. It is needed by B in any case, to bring in a key the host already holds.
- **Against.** The key still lives in host memory as bytes for as long as the host holds it, and crosses on every call (800 times for the recording above). The link's channel keys would still travel out and in on every step, as a secret part of the state. On a managed runtime "clear the array" is best effort (§2).

## 4. Option B: key handles held in the core

**[P]** A key the core derives stays in the core. The host gets a number.

### 4.1 Shape

- `deriveBundleKey` answers `{"ok": true, "key": {"$handle": 17}}`. `sealBundle`, `openBundle`, `confirmationTag`, `archiveSealChunk`, `archiveOpenChunk`, `sealValue` and `openValue` take that reference where they take hex today.
- A passphrase or recovery code goes in once and the normalised secret never comes back out: `passphraseSecret` and `parseRecoveryCode` fold into the derivation (`deriveBundleKey` with `{"passphrase": {"$secret": 0}}`). They remain as checks that answer `ok` or `too_short` with no secret.
- `unwrapKey` replaces "`openValue` whose value is a key": the data key the passphrase gate unwraps becomes a handle, and never becomes a JVM string.
- A link's two channel keys, and the SPAKE2 session itself, are entries. The state then holds `{"ctx": {"$handle": 21}}`; the seed and, after the first call, the pairing code are not in the state at all. This removes what `SWR-MG-28` (d) had to leave.
- `{"op": "keyRelease", "handle": n}` wipes one entry. `{"op": "keyReleaseAll"}` wipes all (on lock, and as part of the clear's `destroyEncryptionKey`).

Handles are integers inside JSON, so **B adds no function to any binding by itself**. It needs A's function only to import a key the host holds (the data key from the keystore, the signing seed, the AWS secret) and to export one the host must keep (a new data key going to the keystore; the UHDR key, if the host is what uses it).

### 4.2 The table's rules

1. **Never reused.** A handle is a 64-bit count that only goes up. A released or never-issued handle is refused by name (`unknown_key`), so a stale handle cannot reach another key. It is not a capability: code in the same process can read the memory anyway.
2. **Typed.** Each entry has a purpose (bundle key, data key, wrapping key, link send, link receive, signing seed, OHTTP secret, pairing session). An operation names the purpose it takes and refuses another, so a bundle key cannot be used to seal a stored value.
3. **No export** of a purpose the core derives and the host has no reason to hold (bundle key, wrapping key, channel keys, pairing session).
4. **Bounded.** A cap on live entries, refused above it, so a host that forgets to release cannot grow the table without end. The figure is a **[choice]**.
5. **Wiped** on release, on `keyReleaseAll`, and whole if a panic poisons the lock (then every later use is refused until the host derives again).
6. **Lost with the process, and on the web with the instance.** `nppsCore.ts` drops the WebAssembly instance after a trap; every handle goes with it. A caller treats `unknown_key` as "derive again", which for a passphrase means asking the person.

- **For.** A derived key is never in host memory in any form, which is the only complete answer for the JVM and JS. A chunk call carries a number. The link's state stops holding secrets. Key lifetime becomes one explicit act.
- **Against.** The core becomes stateful: `at_rest.rs` ("Nothing is kept"), `migration_flow.rs` and `link.rs` ("the core keeps nothing between calls") say otherwise today and must be rewritten, and that is an architectural change, not a detail (Q2). Lifetime bugs become possible (a leak, a use after release). The table is shared mutable state under a lock in three native bindings; WebAssembly is single-threaded. Two images of the library in one process (the Swift app and a Kotlin/Native framework on iOS, if both link the static library) have two tables, so a handle from one is unknown to the other.

## 5. ABI impact on the four bindings

| Binding | Option A (the byte entry point) | Option B (handles) |
|---------|--------------------------------|--------------------|
| **JNI** (`common/npps-jni`, Android and the desktop JVM) | New `nativeMigrationSecret(request: String, secrets: ByteArray): ByteArray` (the answer and the secret bytes in one framed array the caller splits and clears). Rust reads `secrets` with `GetByteArrayRegion` into a wiped buffer; it does not use `get_string` for it. `NppsJni.kt` gains the `external fun`; the keep rule for native names must cover it. `nativeMigration(request: JString)` stays for requests with no secret | No new native method. `NppsBackend.migration` is unchanged. Callers hold a `KeyHandle` (a `Long` with `close()`) where they hold `BundleKey` bytes today |
| **C ABI, iOS** (`common/npps-ffi`, static library; `NppsCore.swift`, and `NppsBackendIos.kt` through cinterop) | `npps_migration_secret_json` added to `neurone_npps.h`. Additive: no existing symbol changes, so an old caller links unchanged. Swift passes `[UInt8]` with `withUnsafeBytes` and overwrites it after; Kotlin/Native pins a `ByteArray` (`usePinned`). The cinterop `.def` is regenerated | No new symbol. One table per linked image (§4.2) |
| **C ABI, Windows** (`common/npps-ffi`, dynamic library; `NppsCore.cs`) | A new `DllImport` taking `byte*` under `fixed`, cleared with `CryptographicOperations.ZeroMemory`. The C# side must stop building the key into a `string` | No new import |
| **WebAssembly** (`common/npps-ffi` built by `scripts/build-npps-wasm.sh`; `common/lib/nppsCore.ts`, `NppsBackendWasm.kt`) | The same symbol is exported. The binding copies a `Uint8Array` into linear memory and zeroes both copies, as stage 1 does for the request. `simulator/js/vendor/npps-runtime.js` is rebuilt | No new export. Handles die with the instance after a trap |
| **Common Kotlin** (`NppsBackend`, `NppsCore.migration`) | `migrationSecret(request, secrets)` beside `migration(request)`; `expect`/`actual` on four targets | A `KeyHandle` type; `BundleKey.hex()` is removed at the end (Q3) |
| **Core** (`common/npps-core`) | A request reader that resolves `{"$secret": n}` into a wiped buffer; every `unhex(req["key"])` site goes through it | The table, the purposes, `keyRelease`; the module texts of three files |

Both options are **additive at the ABI**: nothing existing is renamed or removed until the last caller has moved, and "protection only goes up" (`CLAUDE.md` §5.1, §23) holds at each step. No primitive moves out of `common/npps-core` (`CLAUDE.md` §22), and no new crate is needed for either.

## 6. Recommendation: C, in three steps

**[P]** Use each where it fits, because neither covers the other's case: a key the host must hold (it came from, or goes to, a keystore) needs A, and a key the core derives is best never handed out, which is B.

1. **The byte entry point (A), on all four bindings, and the imported secrets through it:** the data key (`EncryptedKeyValueStore`), the signing seed (`Ed25519.kt`), the AWS secret and session token (`NppsAwsSigV4.kt`), and the passphrase as UTF-8 bytes. This removes the hex-in-JSON copies for the secrets that are used most often, with no stateful core yet.
2. **Handles (B) for what the core derives:** the bundle key first (it is the one copied per chunk), then the wrapping key with `unwrapKey`, then the link's channel keys and the SPAKE2 session. Needs Q2.
3. **Refuse a hex key in JSON** for every operation that has moved (Q3), remove `BundleKey.hex()`, and correct the module texts.

Each step is its own change, with the requirement row written with the code that meets it (`CLAUDE.md` §18, §21); I have written none here. Verification I would expect: a test on each binding that records every request text passed to the core during the migration and presence tests and finds no key in any of them; table tests for a released, a never-issued and a wrong-purpose handle; and the existing cross-platform golden tests unchanged, since no byte of any sealed output changes.

**What this costs if you choose A alone:** the channel keys stay in host memory for the length of a migration and the bundle key crosses per chunk as bytes. **If B alone:** it cannot be built alone; importing the data key needs A's function.

## 7. The archive nonce, and a per-file key

**What is there.** [E] (`common/npps-core/src/archive.rs`) Every recording is one file sealed with AES-256-GCM in 64 KiB chunks. A chunk's nonce is `prefix (8 random bytes, one per file) ‖ counter (4 bytes)`. The key is the backup key, the same for every file of a backup; the index file is sealed again under it at each backup, with a new prefix (`WaveformBackup.kt`). A recording already backed up is not sealed again.

**The risk.** Two files under one key whose prefixes are equal reuse the nonce of every chunk index they share. For GCM that reveals the XOR of the two plaintexts and lets the authentication key be recovered, so both confidentiality and integrity of those files go. Within one file nothing can collide: the counter is checked against 2³².

**How likely.** For n files sealed under one key, the chance of any two sharing a prefix is about n² / 2⁶⁵:

| Files under one key | What that is | Chance |
|---------------------|--------------|--------|
| 730 | one recording and one index a day for a year | about 1.4 × 10⁻¹⁴ |
| 7,300 | the same for ten years | about 1.4 × 10⁻¹² |
| about 92,000 | | 2⁻³² (2.3 × 10⁻¹⁰), the most NIST SP 800-38D allows for a repeated nonce (not re-read) |

**Assessment.** The format is inside the standard's limit for any use one person will make of it, by a factor of about a hundred at ten years of daily use. It is not a defect to fix urgently. But the margin was not chosen (nothing states why 8 bytes; it is now `UC-194`), the key is long-lived, and the nonce is the only thing between two recordings.

**A per-file key.** **[P]** Format version 2: the header carries a 16-byte random file salt in place of the 8-byte prefix; the file's key is `HKDF-SHA-256(key = backup key, salt = file salt, info = "np.archive.v2" ‖ the header)`, 32 bytes; the nonce is the chunk counter alone.

- A repeated nonce then needs two equal 16-byte salts: about n² / 2¹²⁹, which no number of files reaches.
- Each file is sealed under its own key, so the amount of data under any one key is one recording.
- Putting the header into `info` binds the key to the file. Adding a recording ID to the header at the same time closes `OI-PRIV-09` (2) (one recording opening in place of another), which needs a format change anyway.
- `hkdf` is already a direct dependency; with handles (§4) the file key is derived and dropped inside the core.

**What it costs.** A format version. Version 1 files must stay readable for good, since they are never sealed again. An older app refuses a version 2 file as `newer_version`, which is the existing behaviour for any newer file (Q5). One HKDF expansion per file is not measurable beside the sealing.

**Not recommended:** using the 8-byte prefix as the HKDF salt (a prefix collision then gives the same key and the same nonces, so nothing is gained), and moving to XChaCha20-Poly1305 for its longer nonce (`chacha20poly1305` is in the tree through `hpke`, but that is a second AEAD in the format for no gain over a derived key).

**Recommendation (Q4).** Do not change the format for the nonce alone. Make the per-file key part of the version that binds the recording ID (`OI-PRIV-09` (2)), so the format changes once. Until then `UC-194` records the bound.

## 8. What I need back

For Q1 to Q5: the option, a short reason, what would reopen it, and your name, role and date. If Q2 is "no", step 2 of §6 falls away and A is the whole of stage 2, with the residue of §3 stated in `OI-PRIV-12` as accepted.
