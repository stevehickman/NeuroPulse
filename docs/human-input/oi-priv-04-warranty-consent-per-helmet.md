# Question for the principal: OI-PRIV-04, whose consent an SHDR upload rests on when one phone meets several helmets

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#888](https://github.com/stevehickman/NeuroPulse/issues/888)
**From:** NeurOne Software Engineering (`OI-PRIV-04` in `docs/status/pending-decisions.md`; privacy review of `common/`, 2026-11-15, Findings 3 and 4; raised in GitHub #885)
**To:** the principal
**Response needed:** an option for each of Q1 to Q4, a short rationale, anything that would reopen it, and the date.

## 1. The question

`CLAUDE.md` §6.0 says the consent subject for System Health Data Record (SHDR) uploads is the warranty owner of the device. The app records that consent once per phone, and uploads for whichever helmet is connected. So the consent of one helmet's owner sends another helmet's record.

Plain form: should warranty consent be recorded per helmet? If so, what becomes of the consent already recorded, and of the characterisation enrolment that hangs from it?

Nothing is changed. No consent behaviour changes until you answer.

## 2. What is known

All of this was read in the code on 2026-11-16.

**Point 1, confirmed.** `WarrantyAnalyticsGate.isOpen` reads one boolean, `np.warranty.consent-granted`. `FleetUploader.upload` checks that boolean, then loads or makes a device token for the connected helmet and sends. The token is already per helmet (`REQ-SHDR-05`); the consent is not. The consent screen (`WarrantyOwnerScreen`, "fleet reports") sets the flag with no helmet named and works with none connected.

**Point 2, confirmed.** `CharacterisationEnrolmentGate.isOpen` is `warrantyGate.isOpen` and enrolled and the current programme id and an epoch of at least 1. `WarrantyAnalyticsGate.revoke()` removes only the warranty key. Revoke and then grant, by the same owner or a new one, reopens the extended collection with no new opt-in.

The rule this breaks is not CHAR-4. CHAR-4 is non-coercion (`NP-FW-EMMC-002` §H.6). The rules are: the opt-in is separate, affirmative, granular and revocable (`NP-FW-EMMC-002` §H), and enrolment does not survive a change of custody (`NP-MOD-ID-001` §A.4). `clearForDeviceTransfer()` exists for §A.4 and nothing calls it.

**Point 3, confirmed.** In `common/npps/migration-keys.json` the three `np.characterisation.*` keys are under `carried`, and `np.warranty.consent-granted` is under `perInstall`. The Rust core agrees (`common/npps-core/src/migration.rs`), and it also lists `characterisationEnrolment` among the items of the nightly UHDR (User Health Data Record) backup. `SWR-MG-01` (`NP-SW-001`) states this on purpose: enrolment moves, the warranty flag does not. So a new phone receives an enrolment with no warranty consent under it, and the first warranty grant on that phone opens the extended collection.

**The iOS Swift twins have the same shape.** `app/ios/NeurOne/Analytics/WarrantyAnalyticsGate.swift` is one boolean. `CharacterisationEnrolmentGate.swift` is the same conjunction, and its `revoke` twin removes only the warranty key. `SHDRUploader.uploadIfConsented` checks the one boolean and then takes a per-helmet token from the Keychain.

**How much of this is live.**
- Nothing uploads today. `FleetConfig.GATEWAY_KEY_CONFIG_HEX` is empty and the uploader sends nothing without it. No unit has shipped (`CLAUDE.md`: pre-tooling).
- The enrolment gate has no screen and no caller on any platform. Nothing reads `isOpen` outside its own tests. Point 2 and point 3 are defects waiting for the first caller.
- So every option below can be taken before any owner's consent has been used for an upload.

**What a helmet is called.** `HubTokenTable` keys each helmet's token by SHA-256 of a per-install salt and the helmet's identifier, so the Bluetooth address is not stored. The salt never leaves the install, so a value keyed this way cannot be carried to another phone. On iOS the identifier is already a per-phone value.

**Assumed, not established.**
- That consent stays in the app. The consent is kept on the phone, so a helmet whose owner consented on one phone does not upload through another. That is true today and stays true under every option here. Keeping the consent on the helmet would fix it and is listed as Q1 option C.
- That the person at the screen is the warranty owner. Nothing checks it today, and nothing here adds a check.

## 3. Who is affected

| Case | Today | With consent per helmet (Q1 A) |
|------|-------|-------------------------------|
| A clinic, several helmets, one phone | One tap covers every helmet the phone ever meets, including one bought later or one borrowed from another clinic. | One choice per helmet, made while that helmet is connected. A clinic with ten helmets makes ten choices on that phone. Revoking for one leaves the others. |
| A clinic, several helmets, several phones | Each phone has its own flag. A helmet uploads or not depending on which phone it meets. | The same, per helmet and per phone. More choices to make; no new gap. |
| A patient's own phone meets the clinic's helmet | If the patient consented for their own helmet, the clinic's helmet record is sent. The clinic was never asked. | Nothing is sent for the clinic's helmet unless consent is recorded for it on that phone. |
| A household, two helmets, one phone | One owner's consent sends both helmets' records. One owner's revoke stops both. | Each helmet has its own state. The phone still cannot tell two people apart; whoever holds the phone can change either. |
| A household, one helmet, one phone, two wearers | The owner's consent covers the helmet. | No change. The subject is the owner, not the wearer (§6.0). |
| A helmet sold or handed on | The old flag on the old phone keeps covering whatever that phone meets. | The new owner's phone has no record for the helmet and asks. The old phone keeps a record for a helmet it may meet again, until §A.4's reset clears it (not wired, see Q3). |

## 4. Questions

**Q1. Is warranty consent recorded per helmet?**

- **A. Per helmet, in the app (recommended).** Consent is kept in one sealed per-install value, keyed by the same salted helmet hash as the token table. `isOpen` becomes a question about a helmet, and the uploader asks it for the connected helmet. Grant and revoke act on the connected helmet; with none connected the screen says so and offers neither. Cost: a clinic makes one choice per helmet per phone; the screen needs new text in eleven locales; the Swift twins change with it while the Swift app remains.
- **B. Per install, and say so.** Keep the one flag and reword the screen to "every helmet used with this phone". Cost: the person at the phone cannot consent for a helmet someone else owns, so the patient-phone and borrowed-helmet cases stay as they are. This does not meet §6.0 as written.
- **C. On the helmet.** The hub keeps the flag and the app reads it. Consent then follows the helmet to any phone. Cost: firmware and a new characteristic, and a rule for who may write it; it also needs §23's encrypted link. Not buildable now. It can follow A later without discarding anything A records.

**Q2. On upgrade, what does the existing single flag mean?**

The flag cannot be attributed to a helmet. The phone knows which helmets it has tokens for and which it paired last, and neither is a record of what the owner meant.

- **A. Not granted for any helmet; ask again (recommended).** The old key is removed. Each helmet is asked for when it next connects. Cost: consent already given is discarded and the owner taps once more per helmet. No upload has ever been sent and no unit has shipped, so the consent discarded has never been acted on.
- **B. Granted for the last paired helmet only** (`ble.last-hub-id`). Cost: the last paired helmet may not be the one the owner had in mind; that is the over-reach this item is about, kept for one helmet.
- **C. Granted for every helmet the phone has a token for.** Cost: keeps today's over-reach for every helmet already met, and stops it only for new ones.

A and "ask again" may show a one-time notice that the setting is now per helmet. That is copy, not consent, and is yours to want or not.

**Q3. Does a warranty revoke end characterisation enrolment, and is enrolment per helmet?**

- **A. Yes to both (recommended).** Enrolment is keyed per helmet like the consent. Revoking warranty consent for a helmet withdraws that helmet's enrolment. A later grant leaves it withdrawn until someone enrols again on the enrolment screen. The epoch is kept, as `withdraw()` keeps it, because the gate's own rule is that it only increases. `clearForDeviceTransfer()` stays the reset path and still has to be wired to a reset (§A.4); that wiring is outside this item.
- **B. Leave the conjunction.** Revoke suspends, grant resumes. Cost: a new owner's warranty grant enrols them in a programme they were never asked about. Against §H and §A.4.
- **C. Suspend, and confirm on re-grant.** A re-grant asks "resume the maintenance study?". Cost: a second screen whose default the owner may tap through; A reaches the same result with the existing opt-in.

**Q4. Does enrolment stop being carried to a new phone and in the UHDR backup?**

- **A. Yes: per install, never carried, not in the backup (recommended).** It matches the issue #569 ruling in `SWR-MG-01` that a grant is stated afresh on each device. It is also forced by Q1 A and Q3 A: a value keyed by a per-install salt cannot be read on another phone. And it removes the warranty owner's decision from the wearer's health-data backup, which holds two consent subjects in one file today. Cost: `SWR-MG-01` changes (a controlled document, so a revision fragment), the Rust core's item lists change, and the migration tests in four languages; a bundle made before the change still holds the item and a restore must ignore it. An owner who moves phones enrols again.
- **B. Keep it carried.** Cost: point 3 stays, and it cannot be combined with Q3 A.
- **C. Carry only a withdrawal.** As the consent adapter does for research (`SWR-MG-04`), move "withdrawn" and never "enrolled". Cost: a new adapter for a state that A already leaves safe, since a phone with no record is not enrolled.

## 5. Recommendation

Q1 A, Q2 A, Q3 A, Q4 A.

The reason for A on Q2 is the one you flagged as its cost. Discarding consent is a real loss when consent has been relied on. Here it has not: the uploader has never been able to send. Asking again now costs a tap; keeping a consent that cannot be tied to a helmet builds the defect into the upgrade.

## 6. What follows a ruling (not started)

1. A requirement row first (`CLAUDE.md` §18, §21). None states that SHDR consent is per helmet. What fails without it: one owner's consent sends another owner's device record. Trace: `CLAUDE.md` §6.0.
2. `WarrantyAnalyticsGate` and `CharacterisationEnrolmentGate` take a helmet; `FleetUploader` asks for the connected one and still has no reference to `ConsentStore`.
3. `migration-keys.json`, `migration.rs`, `SWR-MG-01`, the screen and its locale keys, the Swift twins.
4. Tests with `@verifies` tags; `check-migration-keys.ts`, `check-consent-reachability.ts`, `:core:jvmTest`.
5. An entry in `docs/status/completed-decisions.md`.

## 7. What I need back

- An option for each of Q1 to Q4.
- A sentence of rationale for each, for the requirement row.
- Whether you want the one-time notice under Q2.
- Anything that would reopen it (for example, consent moving onto the helmet, Q1 C).
- Your name, role and the date.
