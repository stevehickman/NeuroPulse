# Rust Crates SOUP Inventory — Shared NPPS Core (Companion Apps)

**Project:** NeurOne
**Document:** NP-SOUP-RUST-001
**Revision:** 12
**Date:** 2026-10-28
**Status:** DRAFT
**Effective Date:** —
**Author:** NeurOne Software Engineering
**Approved By:** — (DRAFT, not approved)
**References:** `common/Cargo.lock` (the pin), `common/npps-core/Cargo.toml` (the direct dependencies), `NP-SOUP-BTLE-001` (the other Rust record, and the method precedent), `NP-SOUP-LFS-001` and `NP-SOUP-CMSIS-001` (method precedent), `NP-SW-001` §9.4 (the firmware SOUP register, which this record does not extend), `CLAUDE.md` §20
**Related Issues:** GitHub #569 (packages 6c, 6d), `OI-NPPS-CORE-01`
**Gate:** G2
**IEC 62304 Class:** B, following the app items the core serves. See §3.
**Supersedes:** None
**Change Summary:** Rev 12 (2026-10-28) — `sha2` and `hmac` gain a use, with no new crate and no version change: the AWS Signature Version 4 signer for the S3 cloud adapter (`sigv4.rs`, GitHub #566). Rev 11 (2026-10-27) — new §6.1a: the OSV finding GHSA-qwgh-2vcv-g2f7 on `block-buffer` 0.10.4 is accepted with a recorded reason and an expiry (`common/osv-scanner.toml`, `UC-109`), because `spake2` has no stable release on the next crypto generation. Rev 10 (2026-10-20) — the Oblivious HTTP checks owed by Rev 9 are done (`OI-FLEET-01`, `REQ-SHDR-07` (h)): **RustSec re-run** with `cargo audit` 0.22.2 against the advisory database at commit `999ec89` (2026-10-17, 1,338 advisories): `common/Cargo.lock` (162 packages, the lock file's whole tree including `btle-jni`'s) and `infra/fleet/gateway/Cargo.lock` (92) each report **0 vulnerabilities and 0 warnings**. Three of the eight crates added in Rev 9 have advisories in the database (`rand_core`, `chacha20`, `zeroize_derive`; the other five none), all fixed before the pinned versions (§6.1). **Published vectors** (RFC 9180 A.1.1, RFC 9458 Appendix A) and an **interoperability test against Mozilla's `ohttp` 0.7.2** (a dev-dependency of the gateway crate only, never shipped; MIT OR Apache-2.0; `rust-hpke` backend) now exist (§6.2). Rev 9 (2026-10-19) — Oblivious HTTP for the fleet upload joins the core (`OI-FLEET-01` (1), `REQ-SHDR-07`, `CLAUDE.md` §22): `hpke` 0.13.0 (RFC 9180, default features off; `alloc`, `x25519`) seals the request to the gateway's key, and `rand_core` 0.9 (named `rand_core09`, because `hpke` takes that version's traits) carries the host's entropy to it. Eight crates join the tree: `hpke`, `rand_core` 0.9.5, `x25519-dalek`, `chacha20poly1305`, `chacha20`, `poly1305`, `getrandom` and `zeroize_derive`; **61** crates now (was 53). `chacha20poly1305` is a dependency of `hpke` with its default features, which turns on `getrandom`, and `aes-gcm`'s entry above no longer keeps `getrandom` out of the tree. A `wasm32-unknown-unknown` build has no operating-system entropy, so the core registers a `getrandom` source that always refuses (`ohttp.rs`, target-specific in `Cargo.toml`); the core asks for no entropy of its own. **The RustSec scan was not re-run** (`cargo audit` is not installed where this was written): §6.1 describes Rev 4's scan and says nothing about the eight. The gateway in `infra/fleet/gateway` is a separate crate with its own lock file that depends on the core by path and adds `serde_json` only. Rev 8 (2026-10-17) — the iOS app derives its device-bound UHDR key with the core too (`OI-SOUPKT-01`, `CLAUDE.md` §22): the vendored PHC Argon2 is deleted. A known-answer test (`bundle_key.rs`) pins the core's output to what that vendored reference derived for the same inputs, so an existing key is unchanged. No crate changed. Rev 7 (2026-10-17) — `common/Cargo.lock` is also read by OSV-Scanner in `.github/workflows/dependency-scan.yml` (`OI-SOUPKT-01` item 2), on a dependency change and weekly, so §6.1's `cargo audit` is no longer the only scan. No crate changed. Rev 6 (2026-10-17) — SHA-2 and Ed25519 for every platform (issue #569 package 6w, `OI-SOUPKT-01`, `CLAUDE.md` §22): `ed25519-dalek` 2.2.0 (default features off; `alloc`, `zeroize`) signs and verifies, and the core's `sha256`, `sha512`, `ed25519PublicKey`, `ed25519Sign` and `ed25519Verify` operations replace the Kotlin Ed25519 (a TweetNaCl port that was not constant-time) and SHA-2 that `:core` carried. Verification is `verify_strict`. Six crates join the tree as its dependencies: `ed25519`, `signature`, `pkcs8`, `spki`, `der`, `const-oid`; **53** crates now (was 46). `cargo audit` re-run on `Cargo.lock` (1,334 advisories, 154 crates): no vulnerability. Rev 5 (2026-10-17) — the core derives the UHDR key too (`derive_uhdr_key`, issue #569 package 6v): the same `argon2` crate at the same cost with 64 bytes out, so the Kotlin app needs no Argon2 library of its own. No dependency changed; the argon2 row's role is amended and `REQ-SW03-08` now has a core test. Rev 4 (2026-10-16) — the direct-link pairing joins the core (issue #569 package 6t): `spake2` 0.4.0 (SPAKE2 over Ed25519, the password-authenticated key exchange that lets an 8-digit code pair two phones), `hmac` and `hkdf` (the key confirmation and the directions' keys) and `rand_core` (the randomness trait SPAKE2 asks for, fed from the driver's bytes) become direct dependencies; `curve25519-dalek` 4.1.3, `curve25519-dalek-derive` and `fiat-crypto` come with `spake2`. Inventory regenerated (46 crates, was 40) and the RustSec scan re-run against 1,329 advisories (147 packages in the lock file): 0 vulnerabilities. `spake2` has no known-answer test in this repository and no independent review of its use was done (§6.2). Rev 3 (2026-10-15) — the NFKC corpus owed under `unicode-normalization` (§6.3) is built (issue #569 package 6i): `app/NeurOneShared/TestData/nfkc-corpus.json`, 29 known answers from the Unicode Standard written by hand plus the boundary cases of the 12-character floor, run by the core and by Kotlin, Swift, C# and the web through their binding, so it checks the crate's tables and that each platform hands the core the same code points. No dependency changed. Rev 2 (2026-10-15) — `argon2` 0.5.3 joins the core (issue #569 package 6f): the Argon2id derivation of the bundle key now runs once in the core instead of once per app, so the dependency moves from each app's own library to this record. `zeroize` becomes a direct dependency. Inventory regenerated (40 crates, was 34) and the RustSec scan re-run (0 advisories). The argon2 crate is checked against the RFC 9106 test vector by a test in `bundle_key.rs`, which closes the "no known-answer test" gap for it (§6.2). Rev 1 (2026-10-14) — first issue. Written when `aes-gcm` and `unicode-normalization` joined the core (issue #569 packages 6c and 6d), which made the absence of any record of the core's Rust dependencies a gap: until then the core linked `serde_json` and `sha2` and no document listed them. Inventory regenerated from `common/Cargo.lock` with `cargo metadata --locked`; RustSec scan run (0 advisories). The rest of the anomaly evaluation is not done (§6.2), and the record says so.
**Review Cadence:** On any change to `common/Cargo.lock` that touches the tree below, and at G2

---

## 1. What this record is, and is not

`neurone-npps-core` (`common/npps-core`) is the one implementation of the NPPS parser, the descriptor compiler, the protocol
identity hash, the protocol library rules and the migration and backup rules, shared by the web, Android, iOS, Windows and
simulator builds (`CLAUDE.md` §20). It is built three ways: to WebAssembly (`scripts/build-npps-wasm.sh`), behind JNI
(`common/npps-jni`, Android and the desktop JVM) and behind a C ABI (`common/npps-ffi`, a static library for iOS and a
dynamic one for Windows). Everything those three crates link that NeurOne did not write is SOUP. This record **lists** that
SOUP and records what has and has not been checked about it.

`neurone-btle-jni` is a different crate with a different tree and has its own record (`NP-SOUP-BTLE-001`). The `jni` crate
appears in both trees and is listed in both.

**Done and not done:** §6 records what the anomaly evaluation covered and what it did not. The coverage is partial and the
record says so, so it does not read as "no known hazardous anomaly".

## 2. Direct dependencies

The core (`common/npps-core/Cargo.toml`) names fourteen crates. The FFI crate adds none. The JNI crate adds `jni`.

| Crate | Version | Features | Role in the core | Licence |
|-------|---------|----------|------------------|---------|
| serde_json | 1.0.151 | default | Every operation of the core takes and returns JSON (`api.rs`, `library.rs`, `migration.rs`, `bundle_key.rs`); also a build dependency, reading `common/npps/constants.json` | MIT OR Apache-2.0 |
| sha2 | 0.10.9 | default | SHA-256: the protocol identity hash (`identity.rs`), the bundle hash a migration claim and confirmation carry, the recovery code's check characters, and the SHA-256 of the canonical request in the AWS Signature Version 4 signer (`sigv4.rs`) | MIT OR Apache-2.0 |
| aes-gcm | 0.10.3 | `aes`, `alloc`; default features **off** | AES-256-GCM sealing and opening of a migration or backup bundle (`migration.rs`, `SWR-MG-02`). Default features are off so the `getrandom` crate is not linked: the WebAssembly build stays free of a random source, and the core takes its key and nonce as arguments | Apache-2.0 OR MIT |
| argon2 | 0.5.3 | `alloc`, `zeroize`; default features **off** (no `std`, no `password-hash` string API, no `rand`) | Argon2id (RFC 9106, version 0x13) at the fixed cost of the bundle key: 64 MiB, 4 passes, 1 lane, 32 bytes out (`bundle_key.rs`, `derive_key`, `SWR-MG-02`). One implementation for every app; the device-bound UHDR key is derived here too on Kotlin (`derive_uhdr_key`, 64 bytes out, `REQ-SW03-08`); the Swift app uses it too since Rev 8, and its vendored PHC argon2 is gone; a known-answer test pins the output to that reference (`NP-FW-EMMC-001` §6). Pulls `blake2`, `base64ct` and `password-hash` | MIT OR Apache-2.0 |
| zeroize | 1.9.1 | default | The derived key and the secret are `Zeroizing` buffers, cleared when dropped (`bundle_key.rs`) | Apache-2.0 OR MIT |
| unicode-normalization | 0.1.25 | default | Unicode NFKC of a passphrase before it becomes a key secret (`bundle_key.rs`, `SWR-MG-02`). The crate's tables are Unicode 17.0.0 | MIT OR Apache-2.0 |
| spake2 | 0.4.0 | default features **off** | SPAKE2 over Ed25519 (the `Ed25519Group`), the password-authenticated key exchange of the direct-link pairing: the 8-digit code is the password, and a wrong guess gives an attacker one guess per session and nothing to test offline (`link.rs`, `SWR-MG-10`). The core calls the `_with_rng` entry points only, with the driver's bytes, so the crate's own operating-system randomness is never used | MIT OR Apache-2.0 |
| rand_core | 0.6.4 | default | The `RngCore` and `CryptoRng` traits `spake2` asks for; the core implements them over the bytes the driver gave (`link.rs`, `Given`) | MIT OR Apache-2.0 |
| hpke | 0.13.0 | `alloc`, `x25519`; default features **off** (no `p256`, no `std`) | HPKE (RFC 9180) for Oblivious HTTP (RFC 9458): DHKEM(X25519, HKDF-SHA256) with HKDF-SHA256 and AES-128-GCM seals the fleet upload to the gateway's key and opens it at the gateway, and exports the secret the answer is sealed under (`ohttp.rs`, `REQ-SHDR-07`). The ephemeral key comes from the host's entropy, never from the operating system | MIT OR Apache-2.0 |
| getrandom (`wasm32` only) | 0.2.17 | `custom` | Not used for entropy: `hpke`'s dependency `chacha20poly1305` asks `getrandom` for an operating-system source, which a `wasm32-unknown-unknown` build lacks, so the core registers one that always refuses (`ohttp.rs`). The core takes no entropy from the platform | MIT OR Apache-2.0 |
| rand_core (as `rand_core09`) | 0.9.5 | default features off | The `RngCore` and `CryptoRng` traits of the version `hpke` takes; `ohttp.rs` implements them over the 32 bytes the host supplied and stops rather than repeat them | MIT OR Apache-2.0 |
| hmac | 0.12.1 | default | HMAC-SHA-256 over the two SPAKE2 messages: each phone's proof that it holds the shared key (`link.rs`), and the key chain and the signature of the AWS Signature Version 4 signer (`sigv4.rs`; checked against four of Amazon's published S3 signing examples) | MIT OR Apache-2.0 |
| hkdf | 0.12.4 | default | HKDF-SHA-256: the two directions' keys of the sealed link, from the shared key (`link.rs`) | MIT OR Apache-2.0 |
| ed25519-dalek | 2.2.0 | `alloc`, `zeroize`; default features **off** (no `std`, no `rand_core`, no `pkcs8` or `serde` feature) | Ed25519 (RFC 8032): the public key of a seed, the signature of the hub descriptor's signed region, and the strict verification of a study descriptor's signature (`primitives.rs`). Verification uses `verify_strict`, which also refuses a weak or small-order key. The secret is created inside the call from a seed the driver supplied, and the signing key is zeroized when dropped | BSD-3-Clause |

The JNI crate (`common/npps-jni`) also links `jni` 0.21.1 (MIT / Apache-2.0), the JNI surface. The first-party core contains
no `unsafe` block; the FFI and JNI crates do, at the foreign-function boundary, as that is what they are.

## 3. Class

The core serves the app items classified Class B in `NP-SW-001` §6.3 (the `SWR-*` rows carry "B (SW-03)"), and this record
follows them. The hub's safety MCU owns every stimulation enable line (`CLAUDE.md` §4.2), so a fault in the core cannot
enable stimulation. What a fault *can* do is: compute a wrong protocol identity (a protocol marked used, or not used, when
it was not or was); refuse a valid bundle; or, in the cryptographic crates, expose or accept altered UHDR-class data in a
bundle. The last of those is why the cryptographic crates get the closer reading in §7. Whether Class B is the right
classification for a component that holds a user's health data in transit is a software-safety-classification decision and
is not made here.

## 4. Where the lock file is used

The WebAssembly (`scripts/build-npps-wasm.sh`), iOS (`scripts/build-npps-xcframework.sh`), Android host and device
(`app/android/core/build.gradle.kts`, `app/android/app/build.gradle.kts`) builds and the Rust test job (`web-ci.yml`) all
pass `--locked`, so the versions below are the versions shipped and a dependency moves only in a commit that changes
`common/Cargo.lock`. The Windows project (`app/windows/NeurOne/NeurOne.csproj`) states the same in a comment but runs no
cargo itself: it copies a library built beforehand, and whether that build passes `--locked` was not checked.

## 5. Transitive inventory

Every crate in `common/Cargo.lock` reachable from `neurone-npps-core` through normal and build dependencies, over all
targets: **61** crates. Generated with `cargo metadata --locked` on 2026-10-16 (Rev 6 adds the seven in the Ed25519 row's tree; Rev 9 adds the eight in the `hpke` row's tree: `hpke`, `rand_core` 0.9.5, `x25519-dalek`, `chacha20poly1305`, `chacha20`, `poly1305`, `getrandom` and `zeroize_derive`). The `jni` crate and its dependencies add 25 more
(the same crates sit in the tree `NP-SOUP-BTLE-001` §4 inventories, which also links `jni`). The proc-macro and build-script crates (`proc-macro2`, `quote`, `syn`, `serde_derive`,
`version_check`) run at build time and are not in the shipped library.

| Crate | Version | Licence (as declared) |
|-------|---------|-----------------------|
| aead | 0.5.2 | MIT OR Apache-2.0 |
| aes | 0.8.4 | MIT OR Apache-2.0 |
| aes-gcm | 0.10.3 | Apache-2.0 OR MIT |
| argon2 | 0.5.3 | MIT OR Apache-2.0 |
| base64ct | 1.8.3 | Apache-2.0 OR MIT |
| blake2 | 0.10.6 | MIT OR Apache-2.0 |
| block-buffer | 0.10.4 | MIT OR Apache-2.0 |
| cfg-if | 1.0.5 | MIT OR Apache-2.0 |
| chacha20 | 0.9.1 | Apache-2.0 OR MIT |
| chacha20poly1305 | 0.10.1 | Apache-2.0 OR MIT |
| cipher | 0.4.4 | MIT OR Apache-2.0 |
| const-oid | 0.9.6 | Apache-2.0 OR MIT |
| cpufeatures | 0.2.17 | MIT OR Apache-2.0 |
| crypto-common | 0.1.7 | MIT OR Apache-2.0 |
| ctr | 0.9.2 | MIT OR Apache-2.0 |
| curve25519-dalek | 4.1.3 | BSD-3-Clause |
| curve25519-dalek-derive | 0.1.1 | MIT/Apache-2.0 |
| der | 0.7.10 | Apache-2.0 OR MIT |
| digest | 0.10.7 | MIT OR Apache-2.0 |
| ed25519 | 2.2.3 | Apache-2.0 OR MIT |
| ed25519-dalek | 2.2.0 | BSD-3-Clause |
| fiat-crypto | 0.2.9 | MIT OR Apache-2.0 OR BSD-1-Clause |
| generic-array | 0.14.7 | MIT |
| getrandom | 0.2.17 | MIT OR Apache-2.0 |
| ghash | 0.5.1 | Apache-2.0 OR MIT |
| hkdf | 0.12.4 | MIT OR Apache-2.0 |
| hmac | 0.12.1 | MIT OR Apache-2.0 |
| hpke | 0.13.0 | MIT/Apache-2.0 |
| inout | 0.1.4 | MIT OR Apache-2.0 |
| itoa | 1.0.18 | MIT OR Apache-2.0 |
| libc | 0.2.190 | MIT OR Apache-2.0 |
| memchr | 2.8.3 | Unlicense OR MIT |
| opaque-debug | 0.3.1 | MIT OR Apache-2.0 |
| password-hash | 0.5.0 | MIT OR Apache-2.0 |
| pkcs8 | 0.10.2 | Apache-2.0 OR MIT |
| poly1305 | 0.8.0 | Apache-2.0 OR MIT |
| polyval | 0.6.2 | Apache-2.0 OR MIT |
| proc-macro2 | 1.0.107 | MIT OR Apache-2.0 |
| quote | 1.0.47 | MIT OR Apache-2.0 |
| rand_core | 0.6.4 | MIT OR Apache-2.0 |
| rand_core | 0.9.5 | MIT OR Apache-2.0 |
| serde | 1.0.229 | MIT OR Apache-2.0 |
| serde_core | 1.0.229 | MIT OR Apache-2.0 |
| serde_derive | 1.0.229 | MIT OR Apache-2.0 |
| serde_json | 1.0.151 | MIT OR Apache-2.0 |
| sha2 | 0.10.9 | MIT OR Apache-2.0 |
| signature | 2.2.0 | Apache-2.0 OR MIT |
| spake2 | 0.4.0 | MIT OR Apache-2.0 |
| spki | 0.7.3 | Apache-2.0 OR MIT |
| subtle | 2.6.1 | BSD-3-Clause |
| syn | 3.0.6 | MIT OR Apache-2.0 |
| tinyvec | 1.13.3 | Zlib OR Apache-2.0 OR MIT |
| typenum | 1.20.1 | MIT OR Apache-2.0 |
| unicode-ident | 1.0.26 | (MIT OR Apache-2.0) AND Unicode-3.0 |
| unicode-normalization | 0.1.25 | MIT OR Apache-2.0 |
| universal-hash | 0.5.1 | MIT OR Apache-2.0 |
| version_check | 0.9.5 | MIT/Apache-2.0 |
| x25519-dalek | 2.0.1 | BSD-3-Clause |
| zeroize | 1.9.1 | Apache-2.0 OR MIT |
| zeroize_derive | 1.5.0 | Apache-2.0 OR MIT |
| zmij | 1.0.23 | MIT |

Two licence points for the record. `unicode-ident` is `(MIT OR Apache-2.0) AND Unicode-3.0`: the Unicode-3.0 half is
attached to the data tables it carries and is a notice obligation on distribution, which the licence inventory for the app
bundles must carry. `subtle` is BSD-3-Clause, and `tinyvec` is `Zlib OR Apache-2.0 OR MIT`. No crate is copyleft and none
is undeclared, other than the first-party crates themselves.

## 6. Anomaly evaluation

IEC 62304 asks for the published anomaly list of each SOUP version in use to be evaluated for anomalies that could lead to a
hazardous situation. One source was read on 2026-10-14. The others were not.

### 6.1 RustSec advisory database

`cargo audit` 0.22.2 against the RustSec database (1,329 advisories; re-run on 2026-10-16 for Rev 4, the commit not recorded) scanned `common/Cargo.lock`
(147 packages, a superset of the 46 in §5, as the lock file also holds the `btle-jni` tree). **Result: 0
vulnerabilities and 0 warnings** (none unmaintained, unsound or yanked). This covers every crate in §5, but only for what
RustSec has published: it is an absence of reports.

**Rev 10:** the scan above is Rev 4's. It was re-run on 2026-10-20 (see the Change Summary): 0 vulnerabilities and 0 warnings on both lock files, and the three crates below with database entries that Rev 9 added (`rand_core` 0.9.5, `chacha20` 0.9.1, `zeroize_derive` 1.5.0) are past their patched versions (`RUSTSEC-2019-0035` and `RUSTSEC-2021-0023`, patched at 0.4.2 and 0.6.2; `RUSTSEC-2019-0029`, 0.2.3; `RUSTSEC-2021-0115`, 1.1.1). It is an absence of reports only.

Three advisories exist for crates the core uses, and none applies to the pinned versions:

| Advisory | Crate | Affected | Pinned | Reading |
|----------|-------|----------|--------|---------|
| RUSTSEC-2023-0096 | aes-gcm | `decrypt_in_place_detached`, >= 0.10.0, < 0.10.3 (plaintext released even when the tag check fails) | 0.10.3 | The pin is the first patched version. The core does not call the in-place detached function: it uses the allocating `decrypt`, which returns an error and no plaintext on a failed tag. The test `the_bundle_is_unreadable_without_the_key_and_its_plaintext_is_not_in_it` asserts that a failed open returns no `items` |
| RUSTSEC-2021-0100 | sha2 | Only 0.9.7 (a buggy AVX2 backend, yanked) | 0.10.9 | Not the 0.10 line |
| RUSTSEC-2019-0019 | blake2 | Before 0.8.1, and only when used inside HMAC (a wrong block size) | 0.10.6 | Argon2 uses BLAKE2b directly, not through HMAC, and the pin is far past the patch |

### 6.1a An OSV finding RustSec does not list: `block-buffer` 0.10.4 (Rev 11)

The OSV-Scanner job (`dependency-scan.yml`) reads GitHub's advisory database as well as RustSec's. On 2026-10-26 it began reporting **GHSA-qwgh-2vcv-g2f7** (CVSS 6.3, CWE-119, "panic corrupts inline buffer position") against `block-buffer` 0.10.4; `cargo audit` does not list it. The advisory's affected range is every version below **0.12.1**.

**What the defect is.** `EagerBuffer` and `ReadBuffer` keep their cursor inside the block. If the caller-supplied compression or generator callback panics, and safe code *catches the panic and goes on using the same buffer*, the cursor can be out of range and a later read or write can go out of bounds.

**Reading for this core (not an exhaustive trace).** Two conditions are needed, a panic inside the callback and a buffer used again afterwards. The `catch_unwind` sites in `common/npps-ffi` and `common/npps-jni` wrap whole calls (parse, compile, hash; JSON in and out) and the hashers in them are locals of that call, so none is used after a caught panic. The flows keep their state as JSON between calls, not as live hashers. The compression functions of `sha2` and `blake2` do not panic on valid input. This reading has not been tested by forcing a panic.

**Why it is not upgraded.** `block-buffer` 0.10.4 is pulled in through `digest` 0.10 by `argon2` 0.5.3 (via `blake2`), `ed25519-dalek` 2.2.0, `hmac`, `hkdf`, `sha2`, `hpke` 0.13 and `spake2` 0.4.0 (via `curve25519-dalek` 4.1.3). Every one has a stable release on the next generation (`argon2` 0.6.0, `ed25519-dalek` 3.0.0, `hmac` and `hkdf` 0.13.0, `sha2` 0.11.1, `hpke` 0.15.0, read on crates.io 2026-10-27) **except `spake2`**: its only stable release is 0.4.0, and 0.5.0-pre.0 is a pre-release. Moving the rest and leaving `spake2` would not remove `block-buffer` 0.10.4 from the lock file, and taking the pre-release is against `NP-SOUP-KOTLIN-001` §2a rule 2. The principal chose to accept the finding (2026-10-27).

**The acceptance.** `common/osv-scanner.toml` ignores the one identifier until **2027-01-27** (`UC-109`), with this reason. **Closes when** `spake2` publishes a stable release on the digest 0.11 generation (then the stack moves together), or `spake2` is replaced. Tracked by GitHub #689. Until then the job reports every other finding as before.

### 6.2 What was not done

- **No reading of any crate's own changelog or issue tracker** for the pinned versions, as `NP-SOUP-BTLE-001` §5.2 did for
  btleplug. RustSec is the only source read.
- **No independent review of the cryptographic crates.** The record relies on the RustCrypto implementations as published
  and on the known-answer and round-trip tests in `common/npps-core` (which test the core's use of them). **Argon2id has a
  known-answer test**: `the_argon2_crate_reproduces_the_rfc_9106_argon2id_test_vector` runs the RFC 9106 §5.3 vector and
  passes. There is still no AES-GCM known-answer test against a published vector in this repository.
- **`spake2` has no known-answer test here and the use of it was not independently reviewed.** The pairing is tested for what
  it must do (the right code pairs, a wrong one does not, a session costs a try, a frame is sealed, replayed or altered
  frames are refused), but not against a published SPAKE2 vector, and the crate itself says it is not audited. The pin is
  0.4.0, the last release before 0.5.0-pre. A review of the construction (`link.rs`, "The pairing") by someone who does this
  for a living is owed before the direct link ships to people (`UC-093`).
- **`hpke` and the Oblivious HTTP construction (Rev 10).** Checked against RFC 9180 Appendix A.1.1 (the sender, from the vector's input keying material fed through the core's entropy path, reproduces the published `enc`, every ciphertext and every export; the receiver opens it), against RFC 9458 Appendix A (the published request opens to the published message with the published encapsulated key and exported secret, the published response keys and ciphertext are reproduced, and the published response opens), and against Mozilla's `ohttp` crate in both directions through the gateway. **Not covered:** the `ohttp` crate's `rust-hpke` backend is the same `hpke` crate, so the interoperability test cannot show a fault in that crate (only the RFC 9180 vector can, for this suite alone); the RFC 9458 vector does not exercise the core's sealing side (its ephemeral key is a private key, and the core derives its own from entropy); no third-party gateway or relay product was tried; and `ohttp` with its NSS backend, which would be independent, was not built.
- **No check of the licence texts** against the declared SPDX identifiers.
- **No run on iOS, Android, Windows or in a browser of the cryptographic path.** The Rust tests run on Linux; the
  WebAssembly path is exercised by the web test; the JNI and C ABI paths are exercised by CI.

## 7. Hazard reading

| Crate | Fault | Effect | Held by |
|-------|-------|--------|---------|
| aes-gcm | Accepts a bundle whose tag does not verify, or returns plaintext on a failed tag | UHDR-class data from an altered or substituted bundle is restored, or leaks | The header is associated data, so a re-labelled kind fails; `openBundle` returns no `items` on failure (tests in `migration.rs`); RUSTSEC-2023-0096 is patched at the pin (§6.1). **No published known-answer vector is run** (§6.2) |
| aes-gcm | A nonce repeated under one key | GCM loses confidentiality and authenticity for the messages that share it | Not the crate's fault. The key is made from a fresh random salt per bundle (`bundle_key.rs`), so a key encrypts one bundle once; a platform that reused a salt *and* a secret *and* a nonce would break this, and the core does not (and cannot, being handed all three) detect it |
| unicode-normalization | A later release of the crate normalises a character differently, because its tables moved to a newer Unicode version (this pin is 17.0.0) | The same passphrase gives a different key, so a bundle sealed under an older table fails `authentication_failed`, and the person is told their passphrase is wrong when it is not. The bundle is intact and a recovery-code bundle is unaffected | `the_nfkc_corpus_of_known_answers_holds` runs `app/NeurOneShared/TestData/nfkc-corpus.json` (29 answers from the Unicode Standard's decomposition mappings, written by hand and not produced by this crate; the floor counted in characters after normalising) on every build, and the same corpus runs through each platform's binding, so a change to the crate or its Unicode version that changes an answer fails before it ships. It samples the standard and does not cover all of Unicode, so a character outside it can still move without being seen |
| argon2 | A wrong derivation (another variant, version or parameter set) | Every app derives a key that differs from another app's, so a bundle sealed on one cannot be opened on another (the person is told the secret is wrong); or, if the derivation were cheaper than stated, a bundle would be easier to attack offline | The RFC 9106 test vector (§6.2); a test that the wrapper equals the crate; for the UHDR key, a vector produced by the vendored PHC reference that iOS used before called directly at the fixed cost; the header's `kdf` block is compared with the fixed one before any derivation (`unsupported_kdf`), so a bundle cannot choose a cheaper cost |
| sha2 | A wrong digest | A protocol family's identity (and so its used flag) or a bundle hash (and so a claim or confirmation) does not match | The identity tests (`tests/`), the claim tests (`migration.rs`) and the SHA-256 vector the Kotlin test checks. A wrong digest fails closed for migration (no clear without a matching confirmation) |
| serde_json | Rejects or mis-parses a request | An operation refuses a valid request, or reads a field as another type | Parsing errors return an error and no answer; the tests of each operation. Recursion depth is the crate's default limit |
| spake2 | A password-authenticated exchange that leaks the password, or accepts a wrong one | A bystander on the same network pairs with the old phone and is sent the (encrypted) bundle and the claim, or learns the code offline | Both phones check the other's MAC over the two messages before any bundle byte moves; the session cost a try whatever happens; the bundle is still sealed under the person's own secret, so a paired bystander holds ciphertext only; the construction is not independently reviewed (§6.2) |
| hmac / hkdf | A wrong MAC or a key derived the same for both directions | A proof that does not prove, or a sealed link whose two directions share a keystream | The MAC is verified in constant time (`verify_slice`); the two directions use distinct HKDF labels and distinct keys, tested by the link tests |
| hpke | A wrong key schedule, or a request that opens for a party that does not hold the gateway's key | A helmet's SHDR record, with its device token, is readable by the relay or a bystander, or a gateway refuses a good record; the sender's address is no longer hidden from the party that reads the record | The sealing is tested end to end against the same core's gateway side (`ohttp.rs`, `FleetOhttpTests`, `infra/fleet/gateway`): a wrong key, a tampered request and a tampered answer all fail to open. Also checked against RFC 9180 and RFC 9458 vectors and an independent implementation, with the limits in §6.2 (Rev 10) |
| ed25519-dalek | Accepts a forged or malleable signature, or signs wrongly | A study descriptor from a forged source is trusted and admitted (`ConsentEngine.admit()` re-checks consent but not the source); a hub rejects a good protocol | `verify_strict` refuses a malleable signature and a small-order key; the RFC 8032 test 1 vector (`primitives.rs`); the Kotlin parity tests against the JDK's Ed25519 on random inputs |
| the rest (§5) | A fault in a support crate | As the crate that depends on it | RustSec (§6.1) |

## 8. Keeping this record true

- A change to `common/Cargo.lock` that touches the tree in §5 regenerates §5 and re-runs §6.1, in the same change.
- Adding a direct dependency to `common/npps-core/Cargo.toml` adds a row to §2 and a row to §7 in the same change.
- The Argon2id memory (64 MiB) is allocated inside the core: a host with less memory than that cannot derive a bundle key. No host is known to lack it; none has been measured.
- Regenerate §5 with `cargo metadata --locked --format-version 1` from `common/`, taking the crates reachable from
  `neurone-npps-core` by normal and build dependencies; re-run §6.1 with `cargo audit` from `common/`.
- This record does not extend `NP-SW-001` §9.4, which is the firmware register for SW-01 and SW-02. If the app SOUP is
  to be registered in the software development plan, that is a change to the plan.
