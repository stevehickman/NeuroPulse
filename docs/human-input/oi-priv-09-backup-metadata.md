# Principal decision for review: OI-PRIV-09, what the backup location can still learn from a backup (sizes, names, the clear header)

**Status:** DRAFT, ready to send; not yet sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#725](https://github.com/stevehickman/NeuroPulse/issues/725) (the privacy-review issue that holds `OI-BACKUP-01`, which this item extends)
**From:** NeurOne Software Engineering (`OI-PRIV-09` items 3 to 5 in `docs/status/pending-decisions.md`; privacy review of `common/`, 2026-11-15, Findings 22 and 23)
**To:** The principal, with the Privacy Lead (`OI-BACKUP-01` item 1 is the same decision for the recordings and is asked here once)
**Response needed:** an option for each of Q1 to Q5, a short rationale, conditions that would reopen it, name, role and date.
**Why now:** nothing is blocked. The two integrity faults of `OI-PRIV-09` are fixed (`SWR-MG-29`). What is left changes the file formats and costs space, so it is not made as a detail. Every option that changes a format is cheapest now, before any backup made by a released app exists.

## 1. What is known

**Who the observer is.** The backup goes to a place the person chose: a folder on a USB-C drive or computer, or their own cloud account (`SWR-MG-10`, `SWR-MG-21` to `-27`). NeurOne is never the observer (`SWR-MG-02`). The observer is the storage provider, anyone else with access to the account or the drive, and anyone who finds the drive.

**What they cannot read.** Any item, any recording, any recording's name, the epoch of a bundle (`SWR-MG-29`), or which file holds which recording (`REQ-SW03-29` (b)). File times are the epoch where the app sets them (`REQ-SW03-29` (f)).

**What they can read today** (checked against the code on 2026-11-18):

| What | Where | What it says |
|---|---|---|
| The bundle's size | `np-migration-bundle.npbk`; the sealed body is the items as JSON plus a 16-byte tag, with no padding (`migration.rs`, `seal_bundle`) | It grows with the session history. The automatic backup rewrites it about once a day (`ScheduledBackup`), so a provider that keeps versions or logs sizes sees on which days it grew, which is roughly on which days there was a session. |
| The number of recording files and each one's size | `wf-<32 hex digits>.npa`; each is the recording plus 16 bytes per 64 KiB chunk and a 15-byte header (`archive.rs`) | How many sessions were recorded and how long each ran. At 8 channels, 500 Hz and 2 bytes a sample (`CLAUDE.md` §3; the 2 bytes is EDF's sample width and is the sender's assumption about the file the hub writes), EEG alone is about 29 MB an hour, so size gives duration to within seconds. A new file appears on the day of a session. `REQ-UHDRBK-01` already says the count and sizes "are not claimed hidden"; `OI-BACKUP-01` item 1 has had this open since 2026-10-18. |
| The object names | `np-migration-bundle.npbk`, `np-migration-claim.json`, `np-migration-confirmation.json`, `wf-index.npa` (`object_route.rs`, `WaveformBackup.kt`) | That the account holds a NeurOne backup. The first four bytes of each file (`NPBK`, `NPAR`) say the same whatever the file is called. A claim and a confirmation appearing says a device replacement happened, and when. |
| The bundle's clear header | `{"kind", "formatVersion", "nonce", "kdf": {"method", "salt", cost}}` | `kind`: a nightly health-data backup or a device move. `method`: whether a typed passphrase or a 256-bit recovery code locks it. The Argon2id cost is fixed by the core, so it says nothing. |

**What a restore needs before it has a key.** The `kdf` salt and the format version: without the salt no key can be made, and the version must be refused before anything is decrypted (`SWR-MG-05`). These two are irreducible. `kind` and `method` are not: the restore uses `method` to know which secret to ask for, and the restore screen shows `kind` before asking (`SWR-MG-03`, package 6p), but both could be learned another way (section 2, Q5).

**What is assumed, not known.** How large a real bundle is and how fast it grows (no measured session-history size); whether a given cloud provider keeps or logs old versions' sizes; the EDF sample width.

## 2. Questions

**Q1. Is the bundle padded, so its size does not track the amount of history?**
- **A. No.** Record that the bundle's size is visible and is not claimed hidden, as `REQ-UHDRBK-01` does for the recordings. Cost: the day-by-day growth stays readable.
- **B. Pad the sealed body to a multiple of a fixed size,** as `ohttp.rs` does for an analytics request (`PAD_TO`, 1,024 bytes). Cost: less than one block per bundle. The size still steps up every few sessions, so it hides one session and not the trend. The block size is a constant with no derivation and takes a row in `docs/status/unjustified-choices.md` (`CLAUDE.md` §19).
- **C. Pad to geometric buckets** (each bucket a fixed fraction larger than the last; "Padmé" from the PURBs paper, Nikitin et al., PETS 2019, is one published rule with at most about 12 % overhead; the sender has not checked the paper). Cost: about a tenth more space; the size changes rarely and a change says less each time. No block size to choose, but the rule itself needs its source checked.
- **D. A fixed size,** refused when the history outgrows it. Cost: either wasteful or a limit on history. Not recommended.

All of B to D change the bundle format. Whether that needs a version bump depends on whether a build that writes the present form has been released by then: the epoch and the object ID were added with no bump, because no copy of the earlier form should exist (the principal, 2026-11-19).

**Q2. Are the recordings padded, and is their number hidden?** (`OI-BACKUP-01` item 1.)
- **A. No.** Keep "not claimed hidden". Cost: the count and durations of sessions stay readable from the folder.
- **B. Pad each recording to a bucket** (the rule chosen in Q1). Hides the exact duration, not the count or the day a file appears. Cost: the bucket overhead on files of tens of megabytes; on a USB-C drive that is the larger cost of the two questions.
- **C. B, plus a fixed number of files** (dummy files, or one container of fixed slots), so the count says nothing. Cost: space for sessions that never happened, and a rewrite of how a backup adds a recording (today each is sealed once and never again, `REQ-SW03-29` (a)). This is design work, not a format tweak.

**Q3. Are the object names made unrecognisable?**
- **A. No.** Cost: the account visibly holds a NeurOne backup. A new phone finds the backup by its fixed name with nothing but access to the folder.
- **B. A per-user prefix or random names.** Cost: a new phone has to find the backup before it has a key. The ways to do that each have a price: the person types or scans a locator (one more thing to keep with the recovery code); or the name is derived from the secret with a fixed salt, which gives the location a way to test guessed passphrases against a name and so must not be done for a passphrase; or the app lists the folder and reads each file's header, which finds the backup by its `NPBK` bytes, which the observer can do too.

The sender's reading: B buys nothing unless the magic bytes and the clear header go too, and the place is one the person chose for this purpose. It leans to A.

**Q4. Should a device move stop leaving a claim and a confirmation that say when it happened?** They are needed while a move is under way (`SWR-MG-07`, `SWR-MG-08`). Nothing removes them afterwards.
- **A. Leave them.**
- **B. The old phone removes both once it has cleared.** Cost: a small change to the clear flow; a second restore of the same bundle would no longer be refused as claimed, which touches `SWR-MG-07` and `OI-PRIV-02`.

**Q5. Do `kind` and `method` leave the clear header?**
- **A. No. Record that the header shows them.** Cost: the location knows which backups are locked by a passphrase, which are the only ones worth a guessing attack (a recovery code is 256 random bits), and when a move is under way.
- **B. Move `kind` inside the seal and drop `method`.** The restore asks for "your recovery code or passphrase" and tells them apart by form (a recovery code has a fixed shape and a checksum, `bundle_key.rs`); the kind is shown after the bundle opens and before anything is applied, which still meets the words of `SWR-MG-05` ("readable before the bundle is restored") but changes the restore screen, which today shows the kind before asking for the secret. Cost: a format change, a reworded screen, and one ambiguity to settle (a passphrase typed in the shape of a recovery code).
- **C. Drop `method` only.** Removes the more useful of the two facts for the smaller change.

The salt and the format version stay in the clear under every option.

## 3. One more thing the sender wants ruled, from the integrity fix

`SWR-MG-29` (d) says plainly that a phone which has seen no bundle cannot tell an old bundle from the newest. Two follow-ons were not built because they are choices:

- **Q6. Should a restore on a new phone show which backup it was given** (its number, and a date sealed inside it) **and ask the person to confirm it is their latest?** It is the only check a new phone can make. Cost: the person has to know roughly when they last backed up; a date inside the seal is new content in the bundle.
- **Q7. May a person choose to restore an older bundle on purpose?** Today a phone that has seen a later one refuses, and the only way round is an erase. Cost of allowing it: the refusal becomes a warning the person can dismiss, which is the path an attacker wants them to take.

## 4. What the sender needs back

1. An option for each of Q1 to Q7. "Leave as is and record it" is a complete answer to any of them.
2. For any padding option, who checks the rule against its source, and whether a constant may be chosen provisionally.
3. A rationale that names what fails if the option is wrong (`CLAUDE.md` §18), and conditions that would reopen it (for example, a measured bundle size).
4. Name, role and date.

The sender assumes none of these answers. Nothing in the repository pads a bundle, renames an object or changes the clear header until the answer is recorded in `docs/status/completed-decisions.md`.
