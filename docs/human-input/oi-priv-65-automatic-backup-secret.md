# Principal decision for review: OI-PRIV-65 (2), what the automatic backup keeps on the phone in place of the person's passphrase

**Status:** DRAFT, ready to send; not yet sent. Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Software Engineering (`OI-PRIV-65` (2) in `docs/status/pending-decisions.md`; `NP-PRIV-ANALYSIS-007` Finding L21)
**To:** The principal
**Response needed:** an option for Q1 (and Q2 if Q1 is B or C), a short rationale, conditions that would reopen it, name, role and date.
**Why now:** nothing is blocked, and the automatic backup works as it is. Every option except A changes either a rule the principal set for the shared core or the bundle format, so none is made as a detail. Each is cheapest before any backup made by a released app exists.

## 1. What is known

**What is stored.** When the person turns on "keep backing up" while making a health-data backup, `ScheduledBackup.turnOn` keeps the secret the backup was made under, so later runs need no one to type it: `np.backup.auto-secret` holds `<method>:<hex>` (`BundleSecret.toStoredText`, `common/core/.../data/BundleKey.kt`). For a recovery code the hex is the code's 256 random bits. **For a passphrase it is the passphrase itself** (its NFKC bytes). The value sits in the sealed per-install store under the device-only data key (`SWR-MG-06`), is never put in a bundle or moved, and is destroyed by turning the backup off and by the clear (`NP-SW-001` Rev 92, `SWR-MG-03`).

**Why the passphrase is the case that matters.** A recovery code is used for nothing else. A passphrase is chosen by a person and is often reused elsewhere, so anyone who opens the sealed store (a memory-disclosure defect, a crash dump, malware running as the person, or an unsealed run, `OI-PRIV-61`) learns a secret that may open the person's other accounts, not only this backup. A key derived from it would open this backup and nothing else.

**Why it cannot simply be swapped for a derived key today.**
1. **Each bundle gets a fresh salt.** The core's backup flow draws a new salt for every bundle (`migration_flow.rs`, the `random` step, then `deriveKey` over the secret), and so does each new recordings set (`WaveformBackup.newSet`). A stored derived key fits only one salt, so the automatic bundles would all have to reuse the salt it was derived under.
2. **No operation gives a kept key back** (`SWR-MG-32` (b), rule 3 of `secret_io.rs`; the principal, 2026-11-19). The derived key exists only as a handle in the core and is lost with the process. To keep it across runs the core would need a new operation that seals a kept key under the data key and opens it back into a handle (a sealed export), and the flow would need to accept a fixed `kdf` and a key handle in place of drawing its own salt.
3. **A random backup key wrapped under the passphrase** (the other remedy the review names) means the bundle no longer opens from the passphrase directly: the header must carry the wrapped key, which is a bundle format change in the core, the restore flow and the web host.

**What is assumed, not known.** How often people reuse the passphrase they choose here; whether the Swift app's own backup (`UHDRBackupFormat`, slated for retirement under `OI-UI-KMP-05`) keeps the same secret (it is not built that way and is out of scope).

## 2. Questions

**Q1. What does the automatic backup keep?**
- **A. Leave it: the passphrase, sealed, as built, and record the exposure.** Cost: a disclosure of the sealed store discloses a secret that may be reused. `OI-PRIV-61` still has to keep it out of an unsealed run either way.
- **B. A derived key, sealed, with one salt for all automatic bundles.** At turn-on the core derives the key from the passphrase under a fresh salt; a new core operation seals that kept key under the data key and opens it back into a handle; the automatic backup and its recordings set reuse that salt. Cost: amends `SWR-MG-32` (b) rule 3 to allow a *sealed* export (never bytes or hex) for this one purpose; every automatic bundle then shares one key and one salt, so the backup place can tell they come from the same secret (it already can, from the folder), and a nonce is still fresh each time (AES-GCM with random 96-bit nonces is not near its limit at one bundle a day). No bundle format change, and restore is unchanged.
- **C. A random backup key wrapped under the passphrase-derived key in the bundle header.** Only the random key is kept, sealed. Cost: a format change (the header carries the wrapped key), the restore flow unwraps first, and every host that opens bundles changes.
- **D. Stop keeping a passphrase: the automatic backup runs only under a recovery code.** A person who chose a passphrase is asked to make a code for the automatic backup, or types the passphrase when a backup is due. Cost: a screen change, and the automatic backup no longer runs unattended for passphrase users.

The sender's reading: B keeps the person's experience and the format, and limits the change to the core and the two Kotlin callers (`ScheduledBackup`, `WaveformBackup`). D is the smallest change if a passphrase-only automatic backup is rare.

**Q2 (if B or C). Is an automatic backup made before the change readable?** Nothing has been released, so by the rulings of 2026-11-19 no earlier form has to stay readable; the sender would refuse an old `np.backup.auto-secret` value and ask the person to turn the backup on again.

## 3. What the sender needs back

1. An option for Q1, and for Q2 if it applies.
2. If B: confirmation that `SWR-MG-32` (b) may allow a sealed export of a kept bundle key under the data key, for the automatic backup only.
3. Conditions that would reopen it, name, role and date.
