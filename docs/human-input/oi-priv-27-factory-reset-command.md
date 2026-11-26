# Question for the principal: OI-PRIV-27 (NP-PRIV-ANALYSIS-004), who starts a factory reset, and how

**Status:** ANSWERED by the principal: Q2 and the reset flow on 2026-11-23 (§8); the §8 consequences, Q1, Q3, Q4, Q5 and Q6 on 2026-11-24 (§11). The design points Q7 and Q8 (§12) on 2026-11-26 (§14). The wire format is specified, not built (2026-11-25, `NP-FW-HUB-001` §4.7, §13 below), and asks Q9 and Q10 (§13), which remain open. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#955](https://github.com/stevehickman/NeuroPulse/issues/955)
**From:** NeurOne Firmware Engineering (`OI-PRIV-27` (`NP-PRIV-ANALYSIS-004`) in `docs/status/pending-decisions.md`; firmware privacy review `NP-PRIV-ANALYSIS-004`, Finding H7)
**To:** the principal (a wire and design change); Firmware and Software to implement
**Response needed:** answers to Q9 and Q10 (§13). Everything else is answered (§8, §11, §14).

## 1. The question

A factory reset now purges what it erases (eMMC SANITIZE after the erases, `REQ-SW02-13`, done in the change that wrote this brief). **Nothing can start one.** `np_factory_reset_execute()` has no caller outside its own tests. No GATT characteristic, no USB-C message and no app screen reaches it. `NP-FW-EMMC-002` §B.3 R-1 and R-2 say "the app issues a signed FACTORY_RESET command" and "the hub verifies the signature". Neither half exists, and no wire format was ever specified.

Plain form: **what is the command, who may send it, and how does the hub know it is genuine and fresh?**

> **2026-11-23: the brief as first written (§2 to §7) treated the helmet as one person's.** It is not: the principal ruled on `OI-PRIV-34` (NP-PRIV-ANALYSIS-004) that helmets are shared, each person's UHDR sealed under their own key (`NP-FW-EMMC-002` §C.8). A factory reset wipes the whole helmet, so it destroys every person's UHDR on it, not one person's. §2 to §7 are kept as written; §8 records the principal's answer, and §9 asks what it leaves open.

This is a decision, not a detail. It adds a path that destroys a person's data on the device. It is a new wire message under `CLAUDE.md` §23, and it decides whose consent an erase needs under §5.1 and §6.0.

## 2. What is known

Checked on 2026-11-21 against `main` at `49d6c51`.

| Fact | Where |
|---|---|
| `np_factory_reset_execute()` is called only by `firmware/factory_reset/tests/`. `np_factory_reset_boot_check()` (finishing an interrupted reset) is called at bring-up. | `grep` over the tree; `firmware/hub_control/src/np_hub_control_main.c:599` |
| R-1/R-2 are comments: "caller already verified the Ed25519-signed FACTORY_RESET command", "this module accepts unconditionally". | `firmware/factory_reset/src/np_factory_reset.c` |
| R-4 assumes no session is active and does nothing. It is "enforced by the hub layer", and no hub layer exists for it. | same file |
| The only command path into the hub is the protocol upload (0x0008, BLE with link encryption; USB-C CDC with none). It is reassembled by `np_transport.c` and verified as a session descriptor. | `docs/np_fw_hub_001.md` §2.3, §2.5 |
| §B.2 says: user-initiated, "Reset Device" → "Transfer or Sell Device", current biometric plus a two-step confirmation. No app has this screen. The apps' "Erase this computer's data" erases the app's own store, not the helmet, and says so. | `docs/np_fw_emmc_002.md` §B.2; `locales/en.json` `SETTINGS_ERASE_DEVICE_*` |
| **The BIPA consent text already promises the reset:** EEG data is kept "until I request deletion or perform a factory reset, at which point it is permanently destroyed via hardware-level secure erase (eMMC SANITIZE)", and "use factory reset for full erasure". Until a reset can be started, that promise is not met on a shipped device. | `locales/en.json` `BIPA_STEP4_RELEASE_TEXT` |
| The warranty owner may be a clinic, institution or purchaser, and is not assumed to be the wearer. The UHDR owner is the user. | `CLAUDE.md` §5.1, §6.0 |
| "An erase is not a weakening": it destroys what the presence gate protected. | `CLAUDE.md` §5.1 |
| Erasing the safety MCU's cardiac log on reset is a separate ISO 14971 decision. It is not asked here. | `NP-PRIV-ANALYSIS-004` H5 |

## 3. Q1: the form of the command

| Option | What it is | Cost | Risk |
|---|---|---|---|
| **A. A signed reset blob on the existing upload path** | A new blob type beside the session descriptor, on 0x0008 and the same framing. It carries the device serial, a hub-issued nonce and the request, Ed25519-signed with the same infrastructure. Verified by `np_proto_verify`'s code, then dispatched to the reset instead of the runner. | Moderate: a blob type, a nonce read characteristic, a dispatcher branch, app and simulator support, tests. | One parser serves two kinds of blob, so a type confusion must be impossible by construction (distinct magic, verified before dispatch). |
| B. A dedicated GATT characteristic | A write-only reset characteristic with its own signed payload. | Similar; also a new UUID in the frozen table (`REQ-SW03-01`). | Two verification paths to keep identical. |
| C. USB-C only, with physical presence | The reset is accepted only over a cable. | Lower on BLE. | USB-C CDC has no encryption (§23 gap, `OI-SIMFW-70`), so this path is not conformant until its payload is encrypted. |

**Every option needs:**
- a nonce from the hub, so a recorded reset cannot be replayed on this device or any other;
- binding to the device serial;
- refusal while a session is active (R-4 made real);
- an encrypted path (§23).

## 4. Q2: who may authorize it (ANSWERED 2026-11-23: the warranty owner only; see §8)

| Option | Meaning | Consequence |
|---|---|---|
| **A. The person, present (§B.2 as written)** | The app signs only after the person's biometric or passphrase (`docs/np_app_presence_001.md`) and the two-step confirmation. | Matches §5.1: the user owns UHDR, and only the user destroys it. A clinic that owns the warranty cannot reset a wearer's helmet without that wearer. |
| B. The person or the warranty owner | Either may reset. | A clinic can recover a device from a patient who has left. It also lets an institution destroy a person's health data without the person. That needs your ruling under §6.0. |
| C. A as default, plus an offline reset | A, plus a physical-presence reset on the helmet (for example a held button at boot). It needs no app, for a device whose owner lost their phone. | It destroys UHDR without the owner's credential. §5.1 says an erase is not a weakening, but the new owner of a stolen device could use it. It is a hardware and IFU change. |

## 5. Q3: the BIPA text

Until the reset ships, should the consent text keep saying "perform a factory reset"? Yes means it stays, and the gap is tracked here. No means Software changes it to what is true today (regulatory and legal review, `docs/reference/regulatory-strategy.md`).

## 6. Recommendation

- **Q1: A.** It uses the one framing and verifier the hub already has, needs no new frozen UUID, and is encrypted on BLE today. Accept it on BLE only until USB-C carries payload encryption. The nonce and serial binding are part of the format, not added later.
- **Q2: A.** *(Superseded 2026-11-23, §8: on a shared helmet one person's approval would destroy everyone else's record, and the principal ruled that only the warranty owner resets.)* The erase destroys the user's record, so it needs the user. The lost-phone case is real. I recommend raising option C as its own decision with the hardware, not folding it in here.
- **Q3: No.** Change the text to what is true until the reset ships. A consent text that promises an erase the product cannot perform is the kind of statement a regulator reads literally.

## 7. What does not wait for this

Done in the same change:
- the corrected command in `NP-FW-EMMC-002` §B.3;
- `np_emmc_sanitize()` in the bootloader, and the sequence in `np_emmc_sanitize.h` with host tests;
- R-7a, which purges UHDR, SHDR and Config after their erases (`REQ-SW02-13`).

Still open under `OI-PRIV-27` (`NP-PRIV-ANALYSIS-004`) whatever is decided here:
- JEDEC JESD84-B51 and the selected part's datasheet read against the command;
- the 240 s bound (`UC-202`);
- the application's eMMC transport behind the trapping HALs (`OI-RESET-01`).

## 8. Answer received (the principal, 2026-11-23)

The principal was told that a factory reset is per helmet (it erases the whole UHDR, SHDR and Config partitions, and SANITIZE purges the whole eMMC), and that helmets are shared (`OI-PRIV-34` (NP-PRIV-ANALYSIS-004)). The ruling:

| Ruling | What follows |
|---|---|
| **A per-helmet factory reset is done by the warranty owner only.** | Q2 is answered, and none of §4's options is taken: not the person (A), not the person or the owner (B). No user, present or not, can start a whole-helmet reset. §4's option C (an offline reset with no app) is not taken either; it would be a reset by whoever holds the helmet. |
| **Any attempt to reset first shows a list of every user who has any data on that helmet.** | The list comes before any choice and before anything is erased. |
| **The warranty owner chooses which users' data to back up (export). Every user is selected by default.** | Exporting is the default; losing a person's data takes a deliberate act. |
| **Deselecting every user means wipe everything.** | There is no separate "wipe without backup" path. It is the same screen with nobody selected. |
| **The backup, if any, is followed by the wipe that actually resets the helmet.** | The wipe is the R-1 to R-12 sequence of `NP-FW-EMMC-002` §B.3, with SANITIZE (R-7a, `REQ-SW02-13`). |

**How this fits what is already decided.**

- **The export is the clinic backup of `NP-FW-EMMC-002` §C.8.4, restricted to the selected profiles.** Each profile leaves as its sealed files and its recovery wrap. The warranty owner can read none of it. Each person restores their own by their recovery code, on any helmet. So the default (everyone selected) loses nothing that any person can still open.
- **`CLAUDE.md` §5.1:** the user owns UHDR unconditionally, and an erase is not a weakening. A deselected person's UHDR is destroyed without that person. That is an erase, and it is the principal's ruling.
- **`CLAUDE.md` §6.0:** the warranty owner is the SHDR consent subject and is not assumed to be any user. This ruling gives the warranty owner one more power, to wipe the helmet, and no new access: nothing in the flow lets the owner read any person's UHDR.

**Design consequences this brief draws from the ruling.** Please confirm them; each follows from "backup, if any, is followed by the wipe".

1. **The wipe starts only after every selected export has finished and the warranty owner's app has confirmed it holds it.** If any selected export fails, nothing is erased, and the owner is told which one failed. A wipe after a failed export would destroy data the owner chose to keep.
2. **A user who is deselected is named on the confirmation step:** "these people's data will be destroyed and cannot be recovered". That is the two-step confirmation of §B.2, moved to the owner.
3. **The list is shown on every attempt, even when it is empty.** An empty list says so: no user data is on this helmet.

## 9. What the ruling leaves open

**Q1 (the form of the command) is still open, with one change.** The sender is now the warranty owner's app, so the command must prove the warranty owner's role, not a person's presence. That role does not exist on the helmet yet: `NP-FW-EMMC-002` §C.8.5 already leaves "who may request an export (the warranty owner's pairing role)" to `OI-SIMFW-71` Q8, and the reset depends on the same answer. The recommendation stays option A, a signed reset blob on the upload path with a hub nonce, signed by a key bound to the warranty owner's pairing.

**Q3 (the BIPA text) is still open, and the ruling makes the current text wrong even after the reset ships.** The text tells the person "use factory reset for full erasure". Under the ruling, a person cannot start a factory reset at all. A person's own erasure is deleting their profile (`NP-FW-EMMC-002` §C.8.4), which is not built either. **The recommendation is unchanged, and firmer: change the text.** It should name profile deletion, not a factory reset, and say that it is not yet available. That needs regulatory and legal review.

**Q4. What does each row of the list show?** A profile record names its person by an opaque identifier, never a name (`NP-FW-EMMC-002` §C.8.2). Possible answers:

| Option | Row shows | Cost |
|---|---|---|
| **A. A label the warranty owner's app holds** | The owner's own name for the profile (a clinic's patient label), matched to the opaque identifier in the app. Nothing new on the helmet | The owner must label each profile when it is created. An unlabelled profile shows as "unnamed profile" and its identifier |
| B. A label stored on the helmet | A display name kept in the profile record | A person's name on the helmet, readable by the hub with no one present. That is new person-identifying data on the device, against §C.8.2's "never a name" |
| C. Identifier only | The opaque identifier and the size bucket | The owner cannot tell who is who |

Recommendation: **A.** It adds nothing to the helmet, and the clinic already knows its patients.

**Q5. Does "any data on that helmet" include the safety MCU's per-user cardiac-cutoff log?** That log is kept per user on the safety MCU, and what a reset does to it is a separate decision for Safety and the principal (`OI-PRIV-25` (NP-PRIV-ANALYSIS-004), `docs/human-input/oi-priv-25-safety-mcu-cutoff-log-erasure.md`). Recommendation: **the list shows a user who has only a cutoff record too**, marked as such, so that the list really is everyone with data. What happens to that record is left to `OI-PRIV-25`.

**Q6. Until per-person profiles are built, can the helmet list its users?** **No.** Today the helmet has one UHDR key and one store, and its session files are named by the device-wide counter (`NP-FW-EMMC-002` §C.2, Rev 10 note). It cannot say whose data it holds. The list the ruling requires exists only once §C.8 is built. No device is deployed (`OI-PRIV-34` Q4), so nothing is lost by building the reset command with §C.8, after it. Recommendation: **build the reset command with or after §C.8, and do not ship a reset that cannot show the list.**

## 10. What happens with the answers

- The ruling in §8 is recorded in `NP-FW-EMMC-002` §B.2, in the `OI-PRIV-27` (NP-PRIV-ANALYSIS-004) row and in the decision log, in the change that wrote this section. No code is changed: the reset command is not built.
- Q1's answer, with `OI-SIMFW-71` Q8, fixes the wire format. Q4 to Q6 fix the list. Then the command, the list and the export screen are built together, with their requirement rows and tests (`CLAUDE.md` §21).

## 11. Answers received (the principal, 2026-11-24)

| Q | Answer | What follows |
|---|--------|--------------|
| §8 consequences 1 to 3 | **Confirmed, all three.** | (1) The wipe starts only after every selected export has finished and the warranty owner's app holds it; a failed export erases nothing. (2) The confirmation step names every deselected person: their data will be destroyed and cannot be recovered. (3) The list is shown on every attempt, even when it is empty. |
| Q1 | **Recommendation accepted.** | A signed reset blob on the existing upload path (0x0008), with a hub-issued nonce and the device serial, signed by a key bound to the warranty owner's pairing. It is accepted on BLE only until USB-C carries payload encryption (`CLAUDE.md` §23). It waits on the warranty owner's role (`OI-SIMFW-71` Q8). |
| Q3 | **Change the text.** | The BIPA consent text stops promising a factory reset to the person. It names profile deletion as the person's own erasure, and says that it is not yet available. The new wording goes to regulatory and legal review before it ships, then into all eleven locales (`CLAUDE.md` §17). |
| Q4 | **The warranty owner must see something it can associate with a specific user, and never the user's data.** It must know enough to decide whose data to back up, and no more. | Option A meets this: each row shows a label the warranty owner's app holds for the profile, matched to the opaque profile identifier, and the export size bucket §C.8.1 already allows. No UHDR content, and no name stored on the helmet. How the owner's app learns which profile is which, without seeing any data, is Q8 (§12). |
| Q4, added | **By default, only the data of users who are *current* on that helmet is offered for backup.** A user who is not current on the helmet is assumed not to want it backed up. | **This changes §8's default.** Every user with data is still listed. Current users start selected; users who are not current start deselected, and the owner may select them. A deselected user is named on the confirmation step as before (§8 consequence 2). How the helmet knows who is current is Q7 (§12). |
| Q5 | **Agreed.** | A user whose only data on the helmet is a safety-MCU cardiac-cutoff record is listed, marked as such. What the reset does to that record is `OI-PRIV-25`'s. |
| Q6 | **Understood.** | The reset command is built with per-person profiles (§C.8), or after them. No reset ships that cannot show the list. |

**The flow, as ruled.** The warranty owner starts a reset. The helmet lists every user with any data on it, each row labelled so the owner can tell who it is, with no data shown. Users current on the helmet start selected for export; the others start deselected. The owner adjusts the selection. Deselecting everyone means wipe everything. The confirmation names everyone whose data will be destroyed. The selected exports run, and the owner's app confirms it holds each one. Then the wipe runs, and resets the helmet. If any export fails, nothing is erased.

## 12. What remains (design, not new rulings)

**Q7. What "current on that helmet" means, and how the helmet knows it.** "Current" already exists for research: a person is current on the last device they registered, and moving publishes a signed superseded notice under the person's current-device key (`docs/reference/data-architecture-detail.md` §5.3 step 1a, `OI-PRIV-68`). That rule is written for the device that holds the person's UHDR and runs the extract. It does not say whether a helmet a person has moved away from learns it, or how. Two readings:

| Option | Meaning | Cost |
|---|---|---|
| **A. The same key and notice, extended to helmets** | A profile on a helmet records the person's current-device key id. When the person moves to another helmet, the superseded notice reaches this helmet through the owner's or the person's app, and the profile is marked not current | One mechanism for both. The notice names no person and the helmet verifies it, so nothing new is trusted. A helmet that never sees the notice keeps the profile current, which is the safe direction: its data is offered for backup |
| B. A helmet-local rule | A profile is current until that person exports it to another helmet (§C.8.4, "a person moving to their own helmet") | Nothing new, but it misses a person who simply stopped using the helmet without moving |

Recommendation: **A**, because it reuses a mechanism already decided and fails toward keeping data. **What it discloses:** the warranty owner learns that a person has moved to another device. That is a fact about the person's behaviour, not their health, and it follows from the ruling's need for a default. It is noted here, not decided.

**Q8. How the owner's app learns the label for each profile without seeing any data.** Option A of Q4 needs the owner's app to hold a label per profile identifier. The natural moment is when the profile is created on the helmet, in the clinic, with both the person and the owner present: the owner's app records its own label for the new identifier. The helmet never stores it. A profile created without the owner present, or imported (§C.8.4), shows as an unlabelled profile with its identifier and size bucket until the owner labels it. Recommendation: **label at profile creation, in the owner's app only.**

Both points are recorded under `OI-PRIV-27` (NP-PRIV-ANALYSIS-004) and are settled with the build of §C.8, which the reset command waits on anyway (Q6). **Both were answered on 2026-11-26 (§14).**

## 13. The wire format, specified (2026-11-25)

Q1's answer is now a specification: `NP-FW-HUB-001` §4.7. Nothing is built. In short:

- **A second blob kind on 0x0008**, with its own magic (`"NPHR"`) and version, the device serial, a 16-byte challenge,
  the signer's Ed25519 public key and one entry per profile on the helmet (the opaque identifier, exported or destroy,
  and for an exported profile the SHA-256 of the export the app holds), Ed25519-signed.
- **It reuses the one-time upload challenge decided under `OI-PRIV-31` (NP-PRIV-ANALYSIS-004)**, merged in PR #993
  (option A, `docs/human-input/oi-priv-31-protocol-freshness.md` §5; `NP-FW-HUB-001` §4.2; `REQ-FWHUB-90` covers the
  serial half). §3 of this brief asked for "a hub-issued nonce"; that nonce is the same challenge a descriptor
  carries, read from the same place and consumed the same way, not a second mechanism. Taking any blob from the
  mailbox consumes it, so a refused reset cannot be retried with the same challenge. The reset therefore also waits
  for the challenge to be built, which is sequenced with §23's payload encryption (`OI-SIMFW-62`).
- **Type confusion is impossible by construction:** distinct magics, each refused by the other verifier first; and
  distinct keys (a descriptor verifies only under the protocol key, a reset only under the warranty owner's key, and
  a reset signed by the protocol key is refused).
- **Refused, in this order, before anything is dispatched:** a malformed blob; not BLE (until USB-C carries payload
  encryption); an unknown signer; a bad signature; a wrong or unknown serial; a stale or replayed challenge; a session
  active (the dispatcher claims the session lease and holds it to the reboot); a profile list that is no longer the
  helmet's; an exported profile whose digest does not match an export the hub sent on the same link.
- **How §11's consequence 1 is enforced.** The last check puts "a failed export erases nothing" in the hub, not only
  in the owner's app: the hub keeps, in RAM for the link, the SHA-256 of each export it sent, and refuses the blob
  unless every exported entry matches one. Listing every profile, destroyed ones included, in the signed blob is the
  wire form of the confirmation step that names every deselected person (consequence 2), and it stops the wipe of a
  profile added after the owner's list was read.
- **No requirement row and no test** are written: nothing that meets the specification exists (`CLAUDE.md` §18, §21).
  They are written with the code, with or after per-person profiles (Q6).

**Q9. How does the warranty owner's signing key reach the hub?** The secure channel (`OI-SIMFW-71`, ruled 2026-11-13)
pairs apps by X25519 keys, used for the session handshake; it has no signing key, and the reset blob must carry a
signature the hub can check on its own (the blob is verified before dispatch, whatever channel carried it).

| Option | What it is | Cost |
|---|---|---|
| **A. A signing key in the pairing (recommended)** | At pairing, the owner's app also makes an Ed25519 key pair and sends the public key inside the SPAKE2-authenticated channel, with its X25519 key. The hub stores it in that pairing's table entry. The reset's `signer_key` must match it | One more key per pairing entry (32 bytes). The blob stays verifiable on its own, and the same check works over USB-C once its payload is encrypted |
| B. No separate signature: the channel is the proof | The reset is accepted only on the owner's live channel session, and the blob's signature field is dropped | Ties the reset to the channel's build; the "signed" command of `NP-FW-EMMC-002` §B.3 R-1 becomes "authenticated by the session", which is a change to R-1 and R-2 as written |
| C. The protocol signing key | Sign the reset as a descriptor is signed | Any app that can sign a protocol can then reset the helmet: no owner role at all. Contradicts §8. Not recommended |

**Q10. Is the warranty owner the admin pairing of `OI-SIMFW-71` Q8?** Q8 gave a clinic hub one admin pairing and did
not say who holds it; `OI-PRIV-33` (NP-PRIV-ANALYSIS-004) asks the same question for the warranty token. The
recommendation is **yes**: the warranty owner registers the helmet (`CLAUDE.md` §6.0) and is the one paired app that
may remove the others, so the owner's role on the helmet and the admin role are the same thing. If the answer is no,
the hub needs a separate owner mark on one pairing entry, and a rule for who sets it. Either answer fixes the
`UNKNOWN_SIGNER` check (`NP-FW-HUB-001` §4.7.4 step 4); until one is given, no hub accepts a reset.

## 14. Answers received (the principal, 2026-11-26)

The principal was asked Q7 and Q8 of §12 in plain words, with the options, the recommendation and what each discloses. For Q7, the disclosure put to the principal included one point §12 did not state. Under option A the helmet keeps the person's current-device key id. That id is the same on every device the person uses, so two helmets that hold the person's profile can be matched to each other, though not to a name. Q9 and Q10 (§13) were not asked and remain open.

| Q | Answer | What follows |
|---|--------|--------------|
| Q7 | **Option A: the current-device key and the superseded notice of `docs/reference/data-architecture-detail.md` §5.3 step 1a (`OI-PRIV-68`), extended to helmets.** | A profile on a helmet records its person's current-device key id. When the person moves to another device, the signed superseded notice reaches this helmet through the warranty owner's app or the person's app. The helmet verifies it against the key it holds for that profile and marks the profile not current. A notice the helmet cannot verify changes nothing, and neither does one whose move count is not higher than the last it accepted. A helmet that never sees the notice keeps the profile current, so its data starts selected for backup, which is the safe direction. **Disclosures accepted with the answer:** the warranty owner learns that a person has moved to another device (behaviour, not health), and the key id held on the helmet links that person's profiles across helmets, though not to a name. The key id is kept in the profile record only. It is never written to SHDR and never sent to NeurOne. |
| Q8 | **The warranty owner's app records the label when the profile is created. The helmet never stores a name.** | At profile creation, with the person and the owner present, the owner's app records its own label against the new opaque profile identifier (for a clinic, its patient name or number). A profile created without the owner present, or imported (`NP-FW-EMMC-002` §C.8.4), is listed in the reset list as an unlabelled profile, with its identifier and its export size bucket, until the owner labels it. Nothing new is stored on the helmet, and `NP-FW-EMMC-002` §C.8.2's "never a name" stands. The labels are the warranty owner's own data. They are kept with the owner's other data and never with any user's (`CLAUDE.md` §6.0, `REQ-SHDR-10`). |

**What this leaves.** For the principal: Q9 and Q10 (§13). The reset command, the list and the export screen are built with per-person profiles (§C.8) or after them (Q6), with their requirement rows and tests (`CLAUDE.md` §21). They also wait on the warranty owner's pairing role (`OI-SIMFW-71` Q8, and Q10 here), and on the regulatory and legal review of the BIPA rewording (§11, Q3). These answers change no code.
