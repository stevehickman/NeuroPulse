# OI-SIMFW-71: a combined secure-channel design for `CLAUDE.md` §23, and what other systems do

**Status:** ANSWERED 2026-11-13 by the Principal; the rulings are in §14 and are recorded in `docs/np_sim_fw_001.md` (`OI-SIMFW-70`, `OI-SIMFW-71`) and `docs/status/completed-decisions.md`. The rest of the file is the trail.
**Sender:** the design session (a Claude agent), drafted 2026-11-11.
**Addressee:** the Principal (also the owner of the safety MCU; nothing here touches the safety MCU).
**Owning open items:** `OI-SIMFW-70` (which layer carries the rule), `OI-SIMFW-71` (end-to-end payload encryption, which USB-C needs), `OI-SIMFW-72` (whether pairing is authenticated), `OI-SIMFW-73` (the unencrypted stop and pause/panic writes).
**Not a controlled document and not a design input** (`docs/human-input/README.md`): it carries no serial, is not in `NP-DHF-001`, and writes no requirement (`CLAUDE.md` §18). Nothing in the repo may act as if any part of it were decided, except the rulings recorded in §14.

**How to read it.** Every statement is marked: **[E]** established, with its source; **[P]** proposed here, for you to accept, change or reject; **[?]** not stated in anything read. Numbers marked **[choice]** have no derivation: if adopted they go into `docs/status/unjustified-choices.md` (`CLAUDE.md` §19), and I have not added the rows because nothing is adopted.

---

## 1. What I need from you

Nine questions (§12) decide what gets built. The four that block everything:

1. **Stage 1 secret, and the factory.** A code printed on or with the unit is acceptable (you ruled 2026-11-11). May the factory know it, and keep no copy? Or must it be born in the hub? (§6)
2. **Layer.** Do you want a payload layer under both BLE and USB-C, with BLE link encryption kept as defence in depth (what I propose), or link encryption for BLE and a payload layer for USB-C only? (`OI-SIMFW-70`)
3. **Pairing-mode action.** What physical action opens pairing on the hub? The control hub's "single rear toggle" has no stated function. (§4.1)
4. **Audit.** Who commissions an independent review of the crypto, and is an unreviewed implementation acceptable for a first release? (§8)

You have already ruled: the hub's main-processor firmware may include `no_std` Rust (`CLAUDE.md` §22, Rev 81); a printed code is an acceptable pairing secret; the hub generating its own key pair looks most secure.

---

## 2. What other systems do, checked against primary sources

**What I could and could not check.** Reachable: IETF RFCs (`rfc-editor.org`), the public Apple HomeKit ADK and Matter reference-stack source on `raw.githubusercontent.com`, the W3C WebAuthn specification source on the same host, the Noise specification source on the same host, and Google's Fast Pair pages. **Blocked by this session's network policy, so not verified:** `support.apple.com` and `help.apple.com` (Apple Platform Security Guide), `bluetooth.com` and `nvlpubs.nist.gov` (Bluetooth pairing methods), `developers.yubico.com`, `w3.org`, `noiseprotocol.org`. I did not route around the blocks. The page-reading helper could not be used because a worker agent may not spawn another; I read the files myself and searched them.

### 2.1 The claims I made to you, checked

| # | Claim | Source read | What the source states | Result |
|---|-------|-------------|------------------------|--------|
| 1 | Apple: a per-device UID key is fused at manufacture and Apple keeps no record of it | Apple Platform Security Guide, `support.apple.com` | page unreachable (403 from the proxy) | **not verified** |
| 2 | Apple: keys generated in the Secure Enclave are never exported | same | not reachable | **not verified** |
| 3 | HomeKit: 8-digit setup code, printed on the accessory or box | Apple HomeKit ADK, `HAP/HAPAccessorySetup.c` | "Format: XXX-XX-XXX with X being digit from 0-9"; a random-code generator; a validity check that rejects codes with all digits equal and the ascending and descending runs. "Printed on the box" is not stated in the files read; they say the setup payload is "presented on applicable displays and programmable NFC tags" (`HAP.h`) | **verified** (format, random generation); printed label **not stated** |
| 4 | HomeKit pairs with SRP | `HAP/HAPPairingPairSetup.c` | message comments "SRP Start Request" and "SRP Start Response"; the accessory loads a salt and a verifier ("Pair Setup M2: salt", "verifier") and computes the SRP public key from the verifier | **verified**; the SRP group size (I said 3072) was **not confirmed**: the constant is defined in a file I could not fetch |
| 5 | HomeKit then exchanges long-term Ed25519 public keys and stores them | same, M5 | the accessory verifies the controller's Ed25519 signature over its identifier and public key, then "persistently save[s] the iOSDevicePairingID and iOSDeviceLTPK as a pairing"; the accessory has its own LTSK and LTPK | **verified** |
| 6 | (new) the accessory's long-term secret key is made on the device | `HAP/HAPAccessoryServer.c` | if no LTSK is stored, the server purges the pairings domain, fills a new LTSK from the platform random-number generator and stores it | **verified**: the key is born in the accessory |
| 7 | HomeKit Pair Verify: ephemeral X25519, ChaCha20-Poly1305 | `HAP/HAPPairingPairVerify.c`, `HAP/HAPSession.c` | an X25519 key pair is made per verify; HKDF with the strings "Pair-Verify-Encrypt-Salt" and "-Info"; ChaCha20-Poly1305 with fixed per-message nonces in the handshake; the session channel uses a little-endian 64-bit counter nonce that increments per message, one counter per direction | **verified** |
| 8 | (new) HomeKit limits guesses | `HAP/HAPPairingPairSetup.c` | a persisted counter; at 100 unsuccessful authentication attempts the accessory refuses with a max-tries error | **verified** |
| 9 | (new) HomeKit pairing mode and pairing count | `HAP/HAP.h` | pairing mode "deactivates automatically after 5 minutes or when a pairing attempt is registered", has effect only while the accessory is not paired, and a pairing is cleared by a factory reset or a remove-pairing; pairing storage has a minimum of 16 elements; a full table returns a max-peers error; a pairing carries an admin permission to add and remove pairings | **verified** |
| 10 | Matter: passcode from the label, PASE with SPAKE2+ | connectedhomeip `PASESession.h`, `SetupPayload.h`, `CHIPCryptoPAL.h` | the device waits for pairing "using PASE verifier" with a salt and a PBKDF2 iteration count; the commissioner pairs with "peer's setup PIN code"; a verifier-generation function can make a random PIN; the passcode runs 00000001 to 99999998; PBKDF2 iterations 1,000 to 100,000 and a salt of 16 to 32 bytes; the code calls the protocol "Spake2p" | **verified** at the interface level; whether the manufacturer knows the passcode is **not stated** |
| 11 | Matter: CASE with certificates | `CASESession.h` | session establishment from "configured fabrics" using a certificate "from the device's" operational credentials, Sigma messages | **verified** at the interface level; per-session fresh keys **not stated** in the lines read |
| 12 | Matter: manufacturer-signed per-device attestation certificates | `DeviceAttestationCredsProvider.h` | the provider returns the DAC, the PAI and the PAA-linked chain, and signs with "the device attestation private key" | **partly verified**: the chain and the signing interface exist; whether the key is made on the device or injected, and whether the DAC is per device, is **not stated** in the file |
| 13 | (new) Matter fabrics and window | `CHIPConfig.h`, `CommissioningWindowManager.h` | the default maximum is 16 fabrics; the commissioning window's minimum timeout is 3 minutes; no numeric lockout for failed PASE attempts was found in the files read | **verified**; lockout **not stated** |
| 14 | SPAKE2+ is the augmented form: the verifier side holds a record, not the password | RFC 9383 | "only one party makes direct use of the password"; the other "only needs a record"; "the record cannot be used directly ... as a Prover"; "this protocol can be advantageously combined with a salted Password Hashing Function"; recommended scrypt parameters (32768, 8, 1); key confirmation by both sides | **verified** |
| 15 | SPAKE2 is symmetric and stores a password equivalent | RFC 9382 | "SPAKE2 does not support augmentation. As a result, the server has to store a password equivalent. This is considered a significant drawback in some use cases" | **verified** |
| 16 | HPKE encrypts one way and has no forward secrecy against the recipient's key | RFC 9180 | "Encryption is unidirectional from sender to recipient"; "HPKE does not provide forward secrecy with respect to recipient compromise"; four modes, the Auth modes add sender authentication | **verified** |
| 17 | Noise patterns IK and XX, ciphers, rekey | Noise specification (the repository copy) | IK: the initiator already knows the responder's static key and sends its own in the first message; XX: neither knows the other and each sends its static key; ciphers named ChaChaPoly and AESGCM; a `Rekey()` function | **verified** |
| 18 | Bluetooth LE Secure Connections methods, and Just Works has no man-in-the-middle protection | Bluetooth Core specification pages, NIST SP 800-121 | the hosts are blocked | **not verified** |
| 19 | Fast Pair: an anti-spoofing key pair, and a key per model | Fast Pair "characteristics" page | the Seeker "verifies that the Provider is in possession of the anti-spoofing private key"; the Seeker's own key pair "will change for each pairing attempt"; an Account Key exists | **partly verified**; that the anti-spoofing key is per model, not per device, is **not stated** on the pages read |
| 20 | YubiKey attestation keys are shared across batches for privacy | W3C WebAuthn source; Yubico documentation | the WebAuthn text says manufacturers "may choose to ship authenticators in batches" sharing one attestation certificate, which "will anonymize the user at the risk of not being able to revoke a particular attestation certificate", and cites a UAF requirement of at least 100,000 devices per certificate; Yubico's own pages are blocked | **general mechanism verified**; **YubiKey-specific not verified** |
| 21 | MFi: manufacturer-signed per-device certificates | no public primary source reachable | | **not verified** |

**Corrections to what I told you.** (a) The HomeKit ADK supports a **dynamic** setup code, shown on a display or an NFC tag, in a pairing mode that closes after 5 minutes; so a factory-known label is one option there, not the only one. (b) "Matter is SPAKE2+" is right at the interface level, but its docs call it SPAKE2P and I found no numeric lockout. (c) I said HomeKit rate limits; it is a lifetime-style counter of 100 failures, which is loose. (d) I could not confirm the SRP group size, Apple's UID statements, or the Bluetooth, Yubico and MFi claims.

### 2.2 How those systems answer your questions

| Question | HomeKit ADK | Matter reference stack | Others |
|----------|-------------|------------------------|--------|
| Where is the setup secret made, and does the factory know it? | a random generator exists; a static code can be provisioned (salt and verifier loaded at pairing); a dynamic code can be shown on a display or NFC tag, which a factory need not know | the PIN is random (generator excludes invalid values) and a verifier is derived from it; who provisions it is **not stated** in the files read | Fast Pair: not applicable to a code |
| How many pairings does a device hold? | at least 16 slots, a max-peers error when full | 16 fabrics by default | **[?]** |
| How is a pairing revoked or the device reset? | a remove-pairing (admin only), or a factory reset; first start with no stored LTSK makes a new one and purges the pairings | a fabric is removed; the session code reacts to a removed fabric | **[?]** |
| How does the app obtain the device's public key authentically? | learned inside the code-authenticated pairing (M5/M6), then stored | commissioning attestation chain to a trust store (not read) | Fast Pair: the Seeker verifies possession of the anti-spoofing private key |
| What rate-limits the code? | persisted counter, 100 failures, then refusal; a 5-minute pairing mode | a window of at least 3 minutes; no numeric lockout found | SPAKE2 and SPAKE2+ RFCs state no retry limit; they leave it to the application |

---

## 3. Threat model [P]

| Threat | What it means here | Defended by |
|--------|-------------------|-------------|
| Passive eavesdropper (BLE or a USB sniffer) | records uploads and downloads | stage 3 records (all transports) |
| Active man in the middle during pairing | sits between app and hub at first pairing | stage 1: the code; a wrong guess gives one online guess |
| Active man in the middle after pairing | tries to alter or replay | stage 3: authenticated records, counter nonce, a closed session on any failure |
| Malicious host or phone | a computer or phone that is not the user's | needs the code to pair, then the pairing record; a paired phone is trusted (§7 for revocation) |
| Malware injection into an upload | altered or replayed protocol | the signature (unchanged) plus stage 3 authenticity; encryption never replaces the signature (§11) |
| Someone who photographed the label | holds the code | **not defended**: the code is the pairing secret; mitigated by a physical action to open pairing and a lockout |
| Stolen, lost or resold unit | a new owner or thief | a physical action is needed to pair; factory reset makes a new hub key pair and clears pairings (§7); a stolen unit with its label can be paired by the thief, which is true of any label-coded device |
| Factory insider | knows the code, or keeps a copy | open question 1; a retained copy breaks §23 |
| Clinic hub used by many patients | `CLAUDE.md` §6.0: each patient has their own `ConsentStore`; the clinic registered the warranty | a pairing table with several entries and admin and user roles (§7); the clinic's phone is one paired app |

---

## 4. The proposed protocol, in three stages

### 4.1 Stage 1: first pairing with the printed code [P]

- **Which PAKE.** Recommend **SPAKE2 (RFC 9382) with the `spake2` crate already in the repo** (0.4.0, used by the phone link in `common/npps-core/src/link.rs`, already in `docs/np_soup_rust_001.md`), with the hub's code held on the hub. Reasons: least new third-party code; the same crate and shape as the phone link; the pairing code is on the label anyway, so the augmentation SPAKE2+ adds (a record that cannot be used as the prover) protects little here. **Against:** RFC 9382 calls the password-equivalent store "a significant drawback", Matter uses the augmented SPAKE2+ and HomeKit uses SRP (also augmented), and nothing in the repo implements SPAKE2+. **Alternatives, not recommended for a first release:** AuCPace (`aucpace` 0.1.1, 2023, verifier-based, unreviewed, new SOUP); SPAKE2+ once a reviewed implementation exists (a decision recorded as a question).
- **The code.** 8 decimal digits, random, with the trivial codes HomeKit rejects also rejected **[choice, range 8 to 10 digits]**. HomeKit's is 8 digits (3-2-3), Matter's is 8 digits, `link.rs` uses 8 digits; 8 digits is 26.6 bits, so the defence is the guess limit, not the entropy.
- **Guess limit.** A failed-attempt counter kept on the hub, persisted so a power cycle does not reset it. Proposed: 10 failures, then pairing refuses until a physical action reopens it **[choice, range 3 to 100; `link.rs` allows 3 sessions per code, HomeKit 100]**. At 100 attempts against a uniformly random 8-digit code an online attacker succeeds with probability about 1 in 1,000,000 per code; at 10 attempts, about 1 in 10,000,000.
- **Pairing mode.** Pairing is accepted only in a window opened by a physical action at the hub, open for 3 to 5 minutes **[choice; HomeKit 5 minutes, Matter minimum 3, `link.rs` 2]**. **What the action is: [?].** The control hub has a "single rear toggle" and LEDs (`docs/reference/hardware-detail.md` §4.1); the toggle's function is not stated, and no display, keypad, NFC or pairing button is stated anywhere I read. Options: hold the rear toggle; the first minutes after power-on; plugging in a USB-C cable (wired-first, `CLAUDE.md` §1: possession is the action); or no window until the first pairing, as HomeKit (only an unpaired accessory is pairable). I recommend a window opened by the toggle once someone confirms what the toggle does.
- **Key confirmation.** SPAKE2 as in RFC 9382 with the mutual key confirmation `link.rs` already uses; the app checks the hub's first, so a wrong code is found on the phone the person holds.
- **Transport.** The exchange runs over the same byte stream as the upload (BLE write characteristic or USB-C CDC), so first pairing works over USB-C, where link-layer pairing does not exist.

### 4.2 Stage 2: long-term keys exchanged inside the authenticated channel [P]

- The **hub makes its own key pair on first start** (an X25519 static key for the session handshake), from its hardware random-number generator, and never exports the private key. This is what the HomeKit ADK does with its LTSK (row 6), and it answers the factory question for the key pair: the factory never sees it.
- The **app makes its own static key pair** at pairing. Each side sends its public key inside the SPAKE2-authenticated channel, bound to the SPAKE2 transcript, and stores the other's. That is how the app obtains the hub's public key authentically (no trust on first contact over the air).
- **Where they live.** Hub: the private key in the hub's secure storage or OTP **[?]**: no document read states the custody of any hub key (the protocol public key's provisioning is not stated either; the manufacturing HKDF key protects SHDR at boot per `NP-FW-HUB-001`). **App:** the platform keystore. The presence gate (`docs/np_app_presence_001.md`) binds the UHDR key to the device credential and lets only the "device tier" be read locked; if the hub pairing key were UHDR-class, a locked app could not talk to the hub, and the rulings say the fleet upload works locked. So the app's hub-pairing key needs a **device-tier ruling** (`REQ-PRES-05`: only a ruling moves a key to the device tier); I think it qualifies (it tells nothing about the person), but that is a ruling, not mine (question 7).
- **Pairing table.** The hub keeps a small table of paired apps' public keys, each with a role. HomeKit's minimum is 16 with an admin permission; Matter's default is 16. Proposed **8 entries, roles admin and user [choice, range 4 to 16]**. A clinic hub used by many patients holds the clinic's device as an admin; whether each patient's phone pairs separately or the clinic's device mediates is question 8.

### 4.3 Stage 3: a fresh session on every connection [P]

- A **Noise_IK** handshake (Noise specification: the initiator, the app, already knows the responder's static key and sends its own static key encrypted in the first message), X25519, with a counter-nonce AEAD, and the Noise `Rekey()` for long sessions. Reasons for Noise IK: it is a published pattern; both sides already hold each other's static keys from stage 2; and the transport keys are fresh per session (forward secrecy against later key theft; the first message's payload has only weak forward secrecy, so I would send no payload in it).
- **Cipher:** AES-256-GCM with HKDF and a counter nonce, direction-separated keys, as `link.rs` does (the Noise `AESGCM` suite is AES-256-GCM).
- **Implementation:** either the Noise framework by a crate (none is in the repo; I did not check any crate's `no_std` support) or hand-written to the specification from the repo's primitives. The study (`docs/np_sim_fw_001.md` §29) found AES-GCM, HKDF, HMAC, SHA-2, X25519 and Ed25519 compile without `std` for the hub's Cortex-M7.
- **A failed record ends the session.** Any record that fails authentication, arrives out of order or repeats a counter closes the session and needs a new handshake (§5).

---

## 5. The record format on the settled stream [P]

The framing is settled (`NP-FW-HUB-001` §2.3, `REQ-FWHUB-80`): a 4-byte little-endian length, then the signed blob of `[1, NP_HUB_PROTO_BLOB_MAX]` bytes, as one byte stream cut into writes of 1 to 512 bytes, the hub reassembling by the length. `NP_HUB_PROTO_BLOB_MAX` is 6,148 bytes per the `OI-SIMFW-42` row (I did not recompute it).

**Proposal: put a record layer under `np_transport`, and leave the transport and its mailbox unchanged.** The stream the app writes is a sequence of records; the hub decrypts them and feeds the plaintext to `np_transport_feed()` exactly as today. So the 4-byte length and the blob are both inside the encryption; the transport still reassembles by the length.

| Part | Proposal | Reason |
|------|----------|--------|
| Record | 2-byte length in clear, then ciphertext and a 16-byte tag; the length is authenticated as associated data | a decoder needs the length to know how much to read; altering it breaks the tag |
| Plaintext size | at most 1,024 bytes per record **[choice, range 256 to 4,096]** | bounds the hub's decode buffer; a 6,148-byte blob is 7 records; a record may span several 512-byte writes |
| Nonce | a counter per direction, never reused; the counter is the record number | as `link.rs` and HomeKit's session channel (row 7) |
| Truncated record | waits; if the link drops, `np_gatt_on_disconnect()` and the CDC line-state drop reset the decoder and the transport (`np_transport_reset`) | a half record never reaches the transport |
| Replayed, reordered or altered record | fails the tag: the session closes, the transport is reset, nothing is delivered | the single-slot mailbox only ever holds a completed blob made of decrypted records of a live session |
| Pending blob | unchanged: a second blob while one waits is refused (`NP_ATT_APP_BUSY`) | the mailbox behaviour of `REQ-FWHUB-80` is untouched |
| Overhead | 18 bytes per record (2 + 16) | 7 records for the largest blob: 126 bytes |

**Not decided here:** whether the inner 4-byte length should stay (the transport needs it). I propose it stays so that nothing below the record layer changes.

---

## 6. The factory [P, with an open decision]

What the factory knows under your printed-code ruling: **the code.** With an LED-only hub there is no way to show a code to the person other than a label, so the station that prints the label sees the code, unless the code is shown by some other means.

| Option | What happens | Effect on `CLAUDE.md` §23 ("keys held by the two ends and never by NeurOne") |
|--------|--------------|-----------------------------------------------------------------|
| A: the factory knows the code transiently and keeps no copy | the code is made (by the hub or the station), written to the hub, printed, and the station's record is destroyed; no database | the station holds the code for the seconds of provisioning. §23 says "never held by NeurOne"; read as "never retained" this passes, read literally it does not. **Your ruling.** |
| B: the factory keeps a serial-to-code list | support can reissue a lost label | a retained list lets whoever holds it pair any unit and, with a stolen unit, the data it carries. Breaks §23 on any reading. Not recommended. |
| C: the code is never known to the factory | the hub shows it, or a person sets it | needs a display or a keypad the hub does not have (`OI-SIMFW-72`). HomeKit's dynamic setup code (row 3, row 9) is this option. |

The session keys and the hub's and the app's long-term keys are never known to the factory under any option: the hub makes its own. What the factory knows is only the pairing code, and the code is not a key.

---

## 7. Lifecycle [P]

| Event | Proposed behaviour |
|-------|--------------------|
| Re-pairing the same app | opens a pairing window by the physical action, runs stages 1 and 2 again; the old entry is replaced |
| Revoking one app | an admin app removes an entry; **who may remove whom on a clinic hub** is question 8 |
| Lost phone | revoke it from another admin app, or reset by the physical action |
| Factory reset | `firmware/factory_reset` runs R-1 to R-12 (`NP-FW-EMMC-002` §B); I did not establish whether those steps already cover a key pair. Proposed: a reset makes a new hub key pair and clears the pairing table, as the HomeKit ADK purges pairings when it finds no LTSK. This changes `NP-FW-EMMC-002` and needs a requirement. **An erase is not a weakening** (`CLAUDE.md` §5.1) |
| Resale or ownership transfer | the seller resets; the buyer pairs with the label's code. If the label is lost, no code: a reissue path is a question |
| Key rotation | session keys rotate every session; the hub's static key rotates only on a factory reset; a rotation without a reset is not proposed |
| Guess lockout | after the limit, pairing refuses until the physical action reopens it; a successful pairing resets the counter |

---

## 8. Where the code lives, and what it costs [P]

- **Hub.** `no_std` Rust (your ruling, main processor only; **the safety MCU stays C and is not involved**). The study found: `spake2` 0.4.0 compiles but needs a global allocator (its group API returns `Vec`); `hpke` fails until an entropy hook is registered (hub hardware random-number generator); the primitives compile as they are. Class B placement.
- **`npps-core`.** It is not `no_std`; `link.rs` and `primitives.rs` take and return `serde_json::Value`, and `migration.rs` uses `std`. The hub-side channel needs the pairing state machine, the HKDF key schedule and the AES-GCM seal and open with a counter nonce, with a driver-supplied random source, in a `no_std` crate with `alloc`, leaving the JSON effect layer in `npps-core`. That is a design change, not a build flag.
- **Crates.** Each joins `docs/np_soup_rust_001.md` with licence, hazard reading and anomaly evaluation (`CLAUDE.md` §22). Pre-releases are excluded where a stable version exists (`spake2` 0.5.0-pre.0, `aucpace` 0.2.0-pre.0). New crates only if the Noise framework is taken as a crate.
- **Audit status (from `docs/np_sim_fw_001.md` §29).** `aes-gcm` has one 2020 audit by NCC Group; no audit statement was found for the others; the RustCrypto PAKE crates state they have never been independently audited and made no effort on blinding or secret erasure. **What an independent review of a protocol of this shape would cost or take: not stated anywhere I read, and I have not invented a figure.** It is a commissioning decision (question 4).
- **Apps.** The same Rust core reached by JNI, the C ABI and WebAssembly (`CLAUDE.md` §22); nothing is rewritten in Kotlin, Swift, C# or TypeScript.

---

## 9. The twin [P]

The same Rust code runs on both ends in the simulator, as the hub's own sources already do (§25 of `docs/np_sim_fw_001.md`). The twin pairs with a **TEST code and TEST keys**, labelled as such wherever shown (`REQ-SIMFW-08`, `REQ-SIMFW-17` (d)); a TEST code is never a production code. The twin has no radio or USB bus (`OI-SIMFW-46`), so the pairing runs over the bridge's byte relay.

---

## 10. Coverage of every path against §23 [P]

| Path | Today | Under this proposal | Not covered |
|------|-------|---------------------|-------------|
| BLE protocol upload (0x0008) | link encryption required (`REQ-FWHUB-81`) | link encryption plus the record layer | nothing, once built |
| BLE reads and notifications (data characteristics) | link encryption required | link encryption; a value that is user or session data is carried in a record, the rest (device serial, sensitivities) stays link-encrypted only until you decide | the low-value reads are covered by the link only |
| USB-C CDC | **none** | the record layer is the only protection | everything until stage 3 is built |
| Mode 4 download (EDF+ and logs) | does not exist | through the record layer | not built |
| OTA firmware and the bootloader | does not exist | a transfer through the record layer; the image's secrecy at rest and the bootloader's own signature check are a separate design | not designed here |
| SHDR upload | hub to app leg is a download over the channel; the app to cloud leg is the existing path | the hub to app leg through the record layer | the app to cloud leg is not in this design |
| Stop, pause and panic writes (`OI-SIMFW-73`) | not encrypted on purpose (`REQ-FWHUB-81` (c)) | unchanged: one command byte, no data, can only reduce stimulation | an exception to §23; your ruling |

---

## 11. Relation to the signature [P]

Sign, then encrypt: the signing service signs the blob as today (`REQ-FWHUB-76`, `np_proto_verify.c`); the record layer encrypts the signed blob; the hub decrypts, then verifies the signature and the device-serial replay guard (`np_runner_load`). Encryption authenticates the paired phone, not the author of the protocol, so **it never replaces the signature**. A captured encrypted upload cannot be replayed in a later session (fresh keys); a signed protocol replayed inside a live session is caught by the guards that exist today.

---

## 12. What the design does not do, and the decisions it leaves

It does not: encrypt the stop and pause/panic writes; add device attestation (a manufacturer-signed hub identity as Matter and Fast Pair have, so a counterfeit hub that knows the code could pair); settle the OTA image's secrecy at rest; make the factory unable to know the code; measure any size, timing or memory (nothing was run on the target); choose a Noise crate or hand-written Noise.

**Questions for you**

1. **Factory.** Option A (transient, no copy), B or C of §6? (Blocks stage 1.)
2. **Layer.** A payload layer under both transports with link encryption kept as defence in depth (proposed), or link encryption for BLE and a payload layer for USB-C only, or another? (`OI-SIMFW-70`)
3. **Pairing action.** What does the rear toggle do, and may it open pairing? If not, power-on window, USB-C plug-in, or none until first pairing?
4. **Audit.** Who commissions the review, and are unreviewed crates acceptable for a first release?
5. **Which PAKE.** SPAKE2 now with the crate already in the repo (proposed), SPAKE2+ when a reviewed implementation exists, or AuCPace?
6. **Noise.** The framework from a crate, or written to the specification from the repo's primitives?
7. **Presence gate.** May the app's hub-pairing key be device tier (`REQ-PRES-05`)?
8. **Clinic hubs.** One admin phone that mediates patients, or each patient's phone paired separately; who may remove whom?
9. **Attestation.** Do you want a manufacturer-signed hub identity (it needs a CA held by NeurOne, which holds no unit's keys)?

**What I need back** (`docs/human-input/README.md` rule 5): your ruling on each question you answer, a short rationale, conditions that would reopen it, and your name and date. An answer takes effect only when recorded in the owning file and `docs/status/completed-decisions.md`.

---

## 13. How this differs from the industry pattern

| Point | HomeKit, Matter | This proposal | Why it differs |
|-------|-----------------|---------------|----------------|
| First-contact secret | printed or displayed code, augmented PAKE (SRP; SPAKE2+) | printed code, symmetric SPAKE2 | reuse of the crate already in the repo; the code is on the label anyway; SPAKE2+ is the upgrade path |
| Long-term identity | HomeKit: Ed25519 pairing keys; Matter: operational certificates from a fabric | static X25519 keys, no certificates | no NeurOne certificate authority of units (§23); a certificate authority could be added for attestation (question 9) |
| Device made its own key | yes (the ADK makes its LTSK on first start) | yes | the same, and it is what keeps the factory out of the key |
| Session keys | HomeKit: ephemeral X25519, ChaCha20-Poly1305; Matter: CASE | Noise IK, AES-256-GCM | matches the repo's existing AES-256-GCM and `link.rs` |
| Pairing count | at least 16 / 16 fabrics | 8 proposed [choice] | the hub serves a person or a clinic, not a home |
| Pairing window | 5 minutes / at least 3 minutes | 3 to 5 minutes [choice] | the same shape |
| Guess limit | 100 failures, persisted | 10 failures, persisted [choice] | stricter, because the hub is unattended and a wrong guess costs nothing to the owner |
| Attestation | Matter and Fast Pair have manufacturer-signed identity | none proposed | would need a CA; question 9 |
| Sensitive data on the wire | a separate concern in those products | the record layer carries data about a person (`CLAUDE.md` §5) | the reason §23 exists |

---

## 14. Rulings (2026-11-13)

The Principal answered all nine questions of §12 on 2026-11-13 by accepting the design session's recommendations. **Final** means decided. **Conditional** means decided, subject to a fact the repo does not yet hold; the condition is in the Reopens cell. Reviewer: the Principal (name not given to the design session), principal.

| Q | Ruling | Rationale | Reopens if | Status |
|---|--------|-----------|-----------|--------|
| 1 Factory | Option A: the factory knows the code transiently, keeps no copy, and destroys the station's record after the label is printed. A lost label means a service re-provisioning with a new label. | A code is not a session key: the hub and the app each make their own keys. | The factory is found to retain codes. | Final for the design. The `CLAUDE.md` §23 wording ("never held" to "never retained") is **not** changed here; it needs the Principal's explicit confirmation and a separate revision. |
| 2 Layer | A payload layer under both transports, with BLE link encryption kept as defence in depth. | USB-C has no link encryption; one mechanism for both is simpler to build and review. | The payload layer cannot be shown to cover a path. | Final |
| 3 Pairing action | Holding the rear toggle for about 3 s opens a window of 3 to 5 min. If the toggle is the power switch, plugging in a USB-C cable is the physical action instead. "Open until first pairing" is rejected. | A clinic hub is unattended. | What the rear toggle does is established and differs from the assumption. | Conditional |
| 4 Audit | An independent review is commissioned before the first shipped release. Unreviewed crates are acceptable for a prototype only. | The release's main claim is privacy, and only `aes-gcm` has a stated audit. | The Principal names who commissions it. | Conditional (who commissions it and its cost are not decided) |
| 5 PAKE | SPAKE2 now, with the crate already in the repo; SPAKE2+ when a reviewed implementation exists. | The code is printed on the label, so SPAKE2+'s extra protection buys little today. | A reviewed SPAKE2+ implementation exists. | Final |
| 6 Noise | A crate, if a stable, maintained `no_std` one passes the SOUP rules; not a hand-written handshake. | A hand-written handshake is the larger review burden. | No crate passes: then the question returns to the Principal. | Conditional (no crate has been checked yet) |
| 7 Presence gate | The app's hub-pairing key may be device tier (`REQ-PRES-05`). | The key says nothing about the person. | The key is ever used to decrypt person data at rest. | Final |
| 8 Clinic hubs | One clinic admin app is paired with the hub; patients are handled in per-patient stores on it. An admin can remove any paired app, a user only itself, and a physical reset clears all. Admin pairing is not consent to read any patient's data. | Fits `CLAUDE.md` §6.0: each patient on a clinic device has their own `ConsentStore`. | The clinical review finds a patient must hold their own pairing. | Final |
| 9 Attestation | Deferred. | It needs a CA held by NeurOne; a counterfeit hub that knows the code could pair, which is a stimulation-safety question for the ISO 14971 analysis. | The risk analysis asks for it. | Final (deferred) |

Choices adopted without a derivation (the 3 to 5 min window, the failure counter of 10, the 8-entry table) go into `docs/status/unjustified-choices.md` when they are built, not before.
