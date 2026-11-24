# Question for the principal and the cryptography reviewer: OI-PRIV-73, post-quantum key exchange on the paths that carry UHDR

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** none yet.
**From:** NeurOne Firmware Engineering (`OI-PRIV-73` in `docs/status/pending-decisions.md`)
**To:** the principal (Q1, because it changes the design ruled on 2026-11-13 in `docs/human-input/oi-simfw-71-secure-channel-design.md` §14), with the cryptography reviewer named by `OI-EMMC2-02` (Q2 to Q4)
**Response needed:** an option for each question; a short rationale; anything that would reopen it; name, role and date.

## 1. The questions

1. **Q1, the helmet channel.** The §23 channel between the app and the helmet is designed but not built. Its key agreement is classical elliptic-curve only (SPAKE2, static X25519, Noise_IK over X25519). Should it be **hybrid post-quantum** from the start, so that traffic recorded today cannot be decrypted by a future quantum computer?
2. **Q2, the phone-to-phone migration link.** It uses SPAKE2 over an elliptic curve. Should it follow Q1's answer?
3. **Q3, the SHDR fleet upload.** It uses HPKE with X25519 (Oblivious HTTP). Change now, or follow the relay ecosystem?
4. **Q4, firmware signing.** Images are verified with Ed25519 against a key fixed in the bootloader. Should the verification scheme be made upgradable, or hybrid, before the first production key is fixed?

## 2. Why this matters, in one paragraph

Symmetric cryptography is not the problem: AES-256 keeps about 128-bit strength against a quantum attacker (Grover's algorithm), and the backups are already safe for that reason. Elliptic-curve key exchange and signatures are the problem: a large quantum computer running Shor's algorithm breaks X25519, the Ed25519 group SPAKE2 runs on, and Ed25519 signatures. For key exchange the danger is already present, because traffic can be **recorded now and decrypted later** ("harvest now, decrypt later"). UHDR is a person's health record and stays sensitive for their lifetime, so a channel that carries it should resist decryption decades from now. No one can say when such a computer will exist. NIST published its first post-quantum standards in August 2024 (ML-KEM, FIPS 203, for key exchange; ML-DSA, FIPS 204, and SLH-DSA, FIPS 205, for signatures), and the usual migration is **hybrid**: run a classical exchange and ML-KEM together and combine both secrets, so the session is safe if either holds. Nothing in this repository mentions post-quantum cryptography today.

## 3. What each path uses (checked against the code, 2026-11-23)

| Path | Key agreement | Built? | Carries | Quantum exposure |
|---|---|---|---|---|
| Backups: nightly UHDR backup, migration bundle, archived recordings (`common/npps-core/src/bundle_key.rs`, `archive.rs`, `migration.rs`) | **None.** AES-256-GCM under a key Argon2id derives from a 256-bit recovery code or a passphrase of at least 12 characters (`UC-087`) | Yes | UHDR | **None from quantum.** Symmetric only. The passphrase is the weak point (`OI-PRIV-65`) |
| Phone-to-phone migration link (`common/npps-core/src/link.rs`) | **SPAKE2** (RFC 9382) over `Ed25519Group` (`spake2` crate), with the 8-digit code as the password; AES-256-GCM after | Yes | The migration bundle, which the flow seals under the bundle key (`sealBundle`) before it is sent | **Low.** A broken link exposes the sealed bundle, which is still protected by the bundle key |
| Helmet ↔ app, as built (`NP-FW-HUB-001` §2.5) | Bluetooth link encryption only, from Bluetooth pairing's elliptic-curve exchange; pairing is Just Works, so unauthenticated (`OI-SIMFW-72`). USB-C CDC is not encrypted | Partly | UHDR: session downloads, EDF+ | **High**, but this path is to be replaced by the designed channel |
| Helmet ↔ app, as designed (`oi-simfw-71-secure-channel-design.md` §4, ruled §14) | SPAKE2 with the printed label code, then static **X25519** keys exchanged inside it, then **Noise_IK** over X25519 on every connection; AES-256-GCM | No | UHDR | **High.** Every key it agrees comes from elliptic curves |
| SHDR fleet upload (`common/npps-core/src/ohttp.rs`) | HPKE: **DHKEM(X25519, HKDF-SHA256)**, AES-128-GCM (Oblivious HTTP, RFC 9458) | Yes | SHDR (device health, not the person) | **Moderate.** Not UHDR. The relay's whole purpose is to hide which device sent what, and a later decryption would undo that for recorded traffic |
| Signatures: session protocols (app → helmet), firmware images (`firmware/bootloader/src/np_app_image.c`, `np_dfu.c`) | **Ed25519** | Yes | — | **Future forgery, not harvest.** Recorded traffic cannot be forged after the fact, but a helmet in the field could be sent a forged image once a quantum computer exists, unless its verification key can be changed |

## 4. Options

### Q1, the helmet channel

| Option | What changes | Cost | Protects recorded traffic? |
|---|---|---|---|
| **A. Keep the ruled design** (classical only) | Nothing | None now; a later retrofit changes the wire format on shipped helmets and apps | No |
| **B. Hybrid on every connection** | Each Noise_IK handshake also carries an ephemeral ML-KEM-768 encapsulation, and its shared secret is mixed into the session key with the X25519 one. Pairing does the same inside SPAKE2, so the long-term keys exchanged there are covered too | About 2.3 KB more per handshake (an ML-KEM-768 encapsulation key is 1,184 bytes and a ciphertext 1,088 bytes, FIPS 203), negligible at USB-C speeds and a few BLE round trips; a few milliseconds of computation on a Cortex-M class core; a Rust ML-KEM crate that runs without the standard library on the hub and passes `docs/np_soup_rust_001.md`; no standard Noise pattern for it yet, so the combination needs the reviewer's sign-off | Yes, with forward secrecy |
| **C. Hybrid at pairing only** | ML-KEM inside the SPAKE2 pairing produces a pre-shared key both ends keep; every later session runs Noise_IKpsk2, mixing that key in | Smaller: one ML-KEM exchange per pairing, standard Noise pattern for the session | Yes, as long as the stored key is never stolen. If it is stolen later, recorded sessions fall to a quantum attacker (no post-quantum forward secrecy) |

**Recommendation: B**, decided now, because nothing is built yet and a retrofit would change the wire format on helmets already sold. If no acceptable ML-KEM crate exists for the hub, C is the fallback and still closes the harvest-now risk for the common case. A is not recommended for a channel that carries UHDR.

The principal's ruling on Noise (§14, Q6: "a crate, if a stable, maintained `no_std` one passes the SOUP rules; not a hand-written handshake") applies here too. If no Noise crate supports a hybrid handshake, the choice between writing the combination and falling back to C returns to the principal.

### Q2, the migration link

**Recommendation: follow Q1, at low priority.** The bundle is already sealed under the bundle key before it travels (§3), so the link's own exposure is small. Hybridising it means reusing whatever Q1 builds, since `link.rs` and the designed helmet pairing share the same SPAKE2 code.

### Q3, the SHDR fleet upload

**Recommendation: follow the ecosystem.** Oblivious HTTP works only if the relay and gateway agree on the suite, and hybrid HPKE suites (X25519 combined with ML-KEM-768) are still in IETF standardisation as far as I know. Record the exposure, and switch when a hybrid suite is standardised and the gateway supports it. This carries SHDR, not UHDR.

### Q4, firmware signing

**Recommendation: decide before the first production verification key is fixed.** Either make the bootloader's verification key replaceable through a signed rotation, or verify a hybrid signature (Ed25519 together with ML-DSA, FIPS 204, or SLH-DSA, FIPS 205). This is an image-format and bootloader decision for the reviewer, with `OI-PRIV-32` (anti-rollback), which already touches the same code.

## 5. What this brief does not settle

- Which ML-KEM, ML-DSA or Noise crate. None is in the repository, and I have not checked any crate's `no_std` support, licence or audit status. The §14 Q4 ruling applies: an independent review before the first shipped release.
- Measured costs on the i.MX RT1062. The figures in §4 are sizes from FIPS 203 and orders of magnitude, not measurements on this hub.
- Device attestation, which §14 Q9 deferred. It is a separate question from key exchange.

## 6. What happens with each answer

- Q1 B or C → `oi-simfw-71-secure-channel-design.md` §4 and its §14 rulings are amended by the principal, the crate joins `docs/np_soup_rust_001.md`, and `OI-SIMFW-71` carries the build. A → the exposure is recorded beside the ruling with the date to revisit.
- Q2 → folded into the same work, or recorded as accepted.
- Q3 → a watch note on `OI-PRIV-73` until a hybrid HPKE suite is standardised.
- Q4 → a new item for the bootloader's verification-key policy, with `OI-PRIV-32`.

`OI-PRIV-73` stays open until Q1 and Q4 are answered.
