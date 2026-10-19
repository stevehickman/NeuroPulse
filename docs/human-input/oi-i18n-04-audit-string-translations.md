# OI-I18N-04 — verify the draft translations of the privacy-review strings

**Status:** DRAFT, not sent. **From:** NeurOne Software Engineering. **To:** not yet named — one native-speaking
reviewer per locale (ar, bn, ca, es-419, fr, hi, id, ru, zh-Hans; en-GB needs only a read-through).

## What is being asked

Fifteen user-facing strings were added or reworded by the privacy review and the desktop audit that followed
it. No locale has any verified translation yet (`bun scripts/translations.ts status` shows 0 of about 1,750 in
each), and `CLAUDE.md` §17 / `docs/reference/localization.md` §17.7 allow a translation to land only after a
native speaker has verified it. These are **machine drafts**, not translations: each file in
`translation-drafts/` is a normal `translations.ts export` for one locale, cut down to these fifteen keys, with
the `translation` field filled in. **Nothing has been written to `locales/*.json` and no reviewer has been
recorded.**

For each locale, please:

1. Read every draft against its English `source` and **correct it where it is wrong or unnatural**. Edit the
   `translation` field in place.
2. Answer the questions below.
3. Return the file with your name, role and the date.

Whoever lands them runs, per locale (this is the step that records you as the verifier):

```
bun scripts/translations.ts import docs/human-input/translation-drafts/<locale>.json --verified-by "<name>"
```

`import` refuses a key whose English has changed since the draft, a changed placeholder set, and a printf
conversion. These strings have no placeholders. None of them is consent, age-gate or BIPA copy, so no
`--legal-reviewed` is needed.

## The strings

| Key | Where it appears | Note for the reviewer |
|-----|------------------|-----------------------|
| `STORAGE_LOCKED_NOTICE` | Banner above every screen when saved data cannot be opened because the system key store is unavailable | Must say plainly that nothing was deleted, that work in this run is lost when the app closes, and what to do |
| `STORAGE_UNSEALED_NOTICE` | Banner when the key store is unavailable and data is therefore kept unencrypted (desktop) | |
| `STORAGE_MEMORY_ONLY_NOTICE` | Banner in the browser: nothing is saved between visits | "protocols" means the stimulation protocols the person authors |
| `SETTINGS_ERASE_LOCAL_*` (4) | Browser Settings: erase what an earlier version left in the browser | |
| `SETTINGS_ERASE_DEVICE_*` (8) | Desktop Settings: erase everything NeurOne keeps on the computer | `KEY_KEPT` names a literal credential entry, `life.neurone.data-key`, which must stay in Latin script |

## Questions for each reviewer

1. **Terms.** "Key store" (the operating system's secure place for keys), "credential store" (the operating
   system's saved-passwords place), "helmet" (the NeurOne headset) and "session" (one stimulation session):
   are these the words people in your locale use for these things? Name any that are wrong.
2. **Address.** The drafts use the formal or polite "you" where the language has one (fr *vous*, es-419
   *usted*, ca *vós*, ru *вы*, zh *您*). Is that the register NeurOne should use? The same choice will apply
   to the other 1,700 strings, so please say so if it should change.
3. **Hindi:** the confirmation sentence uses "समझता/समझती हूँ" to avoid choosing a gender. Is there a neutral
   form you prefer?
4. **Do the buttons fit?** The longest are "Erase this computer's data" and its translations; say if one is too
   long for a button label in your language.

## What is already known and what is assumed

- The English is final for these keys. If it changes before the drafts are imported, `import` will refuse the
  stale key and the draft must be redone.
- The drafts were produced without a native-speaker check and may be wrong in register, in terminology and in
  small grammatical points. They are provided to save a reviewer typing, not to be trusted.
- Nothing in the repository may treat these drafts as translations. The locale gate
  (`bun scripts/check-locale-strings.ts`) still requires every unverified value to equal the English.

## What to send back

Per locale: the corrected file, the reviewer's name, role and date, and the answers to questions 1 to 4.
