# OI-PRIV-78: should the React web app's backup file be sealed?

**Status:** DRAFT, not sent
**GitHub issue:** [stevehickman/NeuroPulse#1068](https://github.com/stevehickman/NeuroPulse/issues/1068)
**From:** NeurOne Software Engineering (raised while repairing `NP-PRIV-ANALYSIS-006` F-14)
**To:** the principal

## The question

The React web app (`app/web`) writes two files the person downloads, both as plain text:

1. **The protocol backup** (*Save my protocols*, `app/web/src/lib/protocolBackup.ts`): the person's own protocols and
   composites, as a `.npps` file.
2. **The individual-limits download** (`app/web/src/lib/limitsStore.ts` `exportLimitsAsNPPS`): a person's dose limits.

Should either be sealed under a recovery code or passphrase that only the person holds?

## What is known

- You chose, for the React app, to keep nothing in the browser and to give the person a backup file instead
  (NP-SW-001 revision 89, `REQ-SW03-18` (b)). A reload starts clean; the file is how work survives it. That choice is
  not in question here, only whether the file is sealed.
- `CLAUDE.md` §5.1: UHDR is protected by a key NeurOne does not hold, and protection only goes up. A person's own
  protocols and their individual limits say something about the person (when in doubt, UHDR). The other apps' backups
  are sealed under a code or passphrase the person holds (`SWR-MG-02`). Whether these two web files are an unsealed copy
  of UHDR was not weighed when the design was chosen.
- The web already has the sealing path: `common/lib/bundleKey.ts` calls the shared Rust core's Argon2id key derivation,
  the same one every other app uses.
- Plain text is what makes the files useful in other ways: a person can read the backup, open it in any NeurOne app
  without a code, and hand a limits file to a clinician.

## Options

| Option | What changes | Cost |
|---|---|---|
| **A.** Seal both | Saving asks for a code or a passphrase (or shows a new code); opening asks for it again | The person must keep the code; a lost code loses the backup. Two more screens. A clinician cannot read a sealed limits file without the code |
| **B.** Seal the backup only | As A for the backup; the limits download stays plain, as a document meant to be read or handed on | The limits file still says something about the person, in the clear |
| **C.** Keep both plain | The save buttons say the file is not protected and should be kept like any private document | Nothing protects a copy in a downloads folder, a synced folder or an email |

No option is assumed. Nothing in the repository acts on an answer until it is recorded here and in the owning files.

## What to send back

The option (or another), a sentence of rationale, anything that would reopen it, and your name and the date.
