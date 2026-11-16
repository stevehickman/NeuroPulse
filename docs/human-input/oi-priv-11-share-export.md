# Question for the principal: OI-PRIV-11, what a protocol or limits file carries when it is shared

**Status:** ANSWERED 2026-11-16 by the principal: **Q1 A, Q2 as recommended, Q3 a new `family_id` and the free text left as written, Q4 the level kept and the ID dropped**. Recorded in `NP-SW-001` §6.3 `SWR-WO-07` and `SWR-WO-03`, `docs/reference/data-architecture-detail.md` §5.1 and `docs/status/completed-decisions.md`. Not a controlled document; see `docs/human-input/README.md`.

> **Answer, 2026-11-16 (the principal, in the session that wrote this file):** Q1: two forms. Q2: "Save my protocols" stays the full form and a new "Share a copy" action writes the share form; the React web app's Copy button and limits file download use the share form. Q3: a new `family_id`; the author is kept. Q4: the level is kept and the ID is dropped. No rationale beyond the recommendation's was given, and no condition that would reopen it.
>
> **Correction to §2 and §3, found while building it:** the React web app's "Save my protocols" (`backupText`) has never written the used flag; its own comment says a protocol opened from a file starts as unused. So row 4 of §3 carried the ids and no flag, and the statement that the web app keeps the flag only in the saved file was wrong. The ruling does not depend on it. That file not carrying the flag is a gap against `SWR-WO-03` that predates this question and is not changed here.
**GitHub issue:** none. The open item was raised by pull request [#885](https://github.com/stevehickman/NeuroPulse/pull/885) and the question was answered before an issue was opened.
**From:** NeurOne Software Engineering (`OI-PRIV-11` part 2 in `docs/status/pending-decisions.md`; it revisits `OI-UI-KMP-10` decision W3 of 2026-10-09 and `NP-SW-001` `SWR-WO-03`)
**To:** the principal
**Response needed:** an option for each of Q1 to Q4, a short rationale, anything that would reopen it, and the date.

## 1. The question

On 2026-10-09 you decided (W3) that an exported protocol carries its used flag, so that a person who replaces their phone does not find a used protocol treated as unused. The reasoning assumed the person exporting is a clinic owner moving their own library.

The same file is the only file there is. A home user who sends a protocol to a friend, a forum or a clinician sends a file that says `used: true` beside the protocol's name, description, author and conditions. Read together, that says the sender ran this protocol for this condition. A limits file carries the helmet's serial number and the individual profile's ID.

Plain form: should there be a second, stripped form of the file for sharing, and which button writes which form?

## 2. What is known (checked in the code, 2026-11-15)

- **The used flag is written.** `common/npps-core/src/serialize.rs` (`header`) writes `used: true` when the model carries `isUsed`. `NPProtocolLibrary.exportScript` and `exportAllScript` (`common/core`) and the web's `ProtocolLibrary.exportScript` set it for every used family. It sits beside the free-text `name`, `description`, `author`, `tags` and `conditions`.
- **The limits file carries two identifiers.** `serialize.rs` (`limits`) writes `helmet_id` and `individual_id` when the set has them. `NPLimitsStore.saveHelmetLimits` stamps `helmetId` with the helmet serial; an individual set carries the profile's random ID. The limits `name` and `description` are free text.
- **The protocol `id` and `family_id` are written** in every file. They are random, and they are the same in every copy the sender gives out, so two recipients' copies can be matched to each other and to the sender's library.
- **The own-device move does not need the flag in the file.** The migration bundle (`SWR-MG-01`) is encrypted and carries the used families under their own key (`np.used-families`), separately from the protocol text. The flag in the plain file matters in two cases only: the React web app, which keeps nothing in the browser (`REQ-SW03-18` (b)) and so has only the saved file; and a person who moves by saving and opening a file instead of using the migration.
- **There is one export action, and it serves both purposes.** See the table in §3. No screen has a "share" action. The system save dialog lets the person put the file anywhere, including a shared drive or another app.
- **The flag is advisory** (W3 (iv)): the file is text, and a person can delete the line.

**Assumed, not established:** that a home user shares protocol files at all. Nothing measures it. The risk is stated on the reading that they will, because the file format is documented and the save dialog offers every destination the device has.

**A limit no option removes:** two copies of the same protocol have the same content and so the same content hash. A share form removes what ties a copy to the sender (the flag, the IDs). It cannot make two copies of one protocol look unrelated to each other.

## 3. Where an exported file can go today

| # | Where | Platform | What is written | Where it can go |
|---|-------|----------|-----------------|-----------------|
| 1 | Migration bundle (`SWR-MG-01`) | all | protocol text, used families, limits, profiles, encrypted | the person's chosen backup place; readable only with the bundle key |
| 2 | The app's own storage (`np.user-protocols.npps`, `np.limits.*`) | Android, iOS, desktop | protocol text; limits text with `helmet_id` and `individual_id` | stays on the device |
| 3 | "Save my protocols" (`ProtocolMenuScreen.kt`, `exportAllScript`) | Android (Storage Access Framework), iOS (export sheet), desktop (save dialog), Compose web (download) | every user protocol, each with its used flag, in clear text | anywhere the person picks: the device, a cloud drive, a USB stick, another app |
| 4 | "Save my protocols" (`ProtocolMenu.tsx`, `backupText`) | React web | as 3 | the browser's downloads |
| 5 | Script editor text (`exportScript`) | Compose, React web, Swift iOS | one protocol with its used flag, shown as editable text | the screen; on the React web app a Copy button puts it on the clipboard (`ScriptEditor.tsx`) |
| 6 | Limits file download (`limitsStore.downloadLimitsFile`) | React web | one limits set with `helmet_id` or `individual_id` | the browser's downloads, named after the limits set |
| 7 | Limits script text (`exportLimitsAsNPPS` in the tier editor) | Compose, Swift iOS | as 6, shown as editable text | the screen; text can be selected and copied |

## 4. Options

### Q1. One form or two?

- **A. Two forms (recommended).** The full form is what exists now. A share form strips `used`, `helmet_id` and `individual_id`, and gives the protocol a new `id`. The rule is written once in the Rust core (`CLAUDE.md` §22) and every platform calls it. *Costs:* one new core function and its bindings; a second action on the protocol screen with its text in eleven locales; W3 and `SWR-WO-03` are reworded to say the flag travels in the full form only.
- **B. One form, as now, with a notice.** The save action says the file records which protocols have been used. *Costs:* one string in eleven locales. The person who wants to share has to edit the file by hand to remove the flag, and most will not.
- **C. One form, always stripped.** The flag never leaves the device in clear text; the own-device move relies on the encrypted migration alone. *Costs:* it reverses W3. The React web app loses the flag at every reload, so a used protocol becomes editable there, against P2 of `OI-UI-KMP-10`. A person who moves by file loses it too.

### Q2. If A: which destination writes which form?

Recommended, by row of §3:

| # | Destination | Form | Why |
|---|-------------|------|-----|
| 1 | Migration bundle | full | the person's own move, encrypted |
| 2 | App storage | full | never leaves the device |
| 3, 4 | "Save my protocols" | full, and the screen says the file is the person's own backup and records which protocols were used | on the React web app it is the only place the flag is kept |
| new | "Share a copy" (new action, per protocol and for all) | share | the file is meant for someone else |
| 5 | Script editor text | full on screen, because saving the edit must find the same protocol; the Copy button copies the share form | the clipboard's destination is unknown |
| 6 | Limits file download | share | a limits file is given to another clinic or person; the person's own limits travel in the migration |
| 7 | Limits script text | full on screen | the editor saves back to the same tier |

The alternative for rows 3 and 4 is to make the existing button write the share form and add a separate "Back up" action. It is safer by default and costs the React web app its only way to keep the flag unless the person finds the second button.

### Q3. If A: does the share form also change `family_id`, and what happens to free text?

- **`family_id`:** F6 of `OI-UI-KMP-10` merges an import into a family by this ID. Keeping it lets a recipient's copy join the family they already hold, and lets any two copies be matched. **Recommended: a new `family_id` in the share form**, for the same reason as the new `id`. *Cost:* a shared copy of a variant arrives as its own family.
- **`author`, `name`, `description`, `tags`, `conditions`:** the sender typed them and can see them. **Recommended: left as written.** The alternative is to blank `author` in the share form, which removes a name the sender may have meant to give.

### Q4. If A: a shared limits set with no ID

A helmet-level or individual-level set with its ID stripped names no helmet and no person. **Recommended:** the share form keeps the `level` and drops the ID, and the recipient's app attaches the set to the helmet or profile they are editing when they open it, as the tier editors already do on import.

## 5. What is not being asked

- Part 1 of `OI-PRIV-11` (the profile's date of birth and notes) is settled in the same change that adds this file: `NP-SW-001` `REQ-SW03-31`.
- Whether the content hash should be salted per copy. It would break the rule that the same content has the same hash (H1).

## 6. What to send back

For each of Q1 to Q4: the option, a short rationale, what would reopen it, and the date. Nothing in the repository acts as if this is answered: every export still writes the full form.
