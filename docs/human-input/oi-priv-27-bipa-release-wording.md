# Question for regulatory and legal review: OI-PRIV-27 (NP-PRIV-ANALYSIS-004), the biometric release's erasure wording

**Status:** DRAFT, not sent. Addressee: regulatory and legal review (BIPA-specialised counsel, `NP-PRIV-REM-001`; `docs/reference/regulatory-strategy.md`); not yet named. Not a controlled document; see `docs/human-input/README.md`.
**GitHub issue:** [#955](https://github.com/stevehickman/NeuroPulse/issues/955)
**From:** NeurOne Software Engineering (`OI-PRIV-27` (NP-PRIV-ANALYSIS-004) in `docs/status/pending-decisions.md`)
**To:** regulatory and legal review; the principal for Q2
**Response needed:** a ruling on Q1 to Q3 (§7), and a view on Q4.

**Nothing here has shipped.** No locale file is changed by the change that wrote this brief. The approved wording goes into all eleven locales (`CLAUDE.md` §17) only after review, and the published policy and the two documents that hold its text change in the same later change.

## 1. Why this brief exists

The principal ruled on 2026-11-24 that **the biometric (BIPA) consent text must change** (`docs/human-input/oi-priv-27-factory-reset-command.md` §11, Q3). The text told the person to "use factory reset for full erasure". Under the rulings that is wrong in three ways:

- **A factory reset is per helmet, and only the warranty owner may start one** (`NP-FW-EMMC-002` §B.2, Rev 16). No user can, present or not.
- **A person's own erasure is deleting their profile** (`NP-FW-EMMC-002` §C.8.4).
- **Neither is built.** No command reaches `np_factory_reset_execute()`, and nothing deletes a profile from the app (§5).

## 2. What changed since the ruling was briefed: the text it names no longer ships

The ruling and the factory-reset brief name `locales/en.json` key `BIPA_STEP4_RELEASE_TEXT`. **That key was deleted on 2026-11-22 with the SwiftUI app** (#946, `OI-UI-KMP-05` PR B), because no remaining code referenced it (`docs/reference/localization.md` §17.1). The release the shared app shows now is a different, shorter text (§3.2). The factory-reset promise did not disappear with it. It moved:

| Where | What it says now | Shown to the person? |
|---|---|---|
| The shipping release, `BipaConsentScreen` | "Retention: your recordings stay on your device until you delete them." It then points to the published policy | Yes, before the first EEG session |
| The published policy (`NP-PRIV-NOTICE-001` §10; `NP-PRIV-REM-001` STEP-34, Step 2; neurone.life/biometric-policy) | Retained until "(1) you delete your data in the app, (2) you perform a factory reset, or (3) you request account deletion", then erased by eMMC SANITIZE | By reference, from the release |
| The retired release `BIPA_STEP4_RELEASE_TEXT` | "until I request deletion or perform a factory reset … eMMC SANITIZE"; "use factory reset for full erasure" | No, deleted 2026-11-22 |

So the ruling now bites on two texts: the shipping release's retention sentence, and the policy it incorporates. This brief drafts both, and also a corrected long form, in case review wants the long release back (Q2).

## 3. The current texts

### 3.1 The retired long release (`BIPA_STEP4_RELEASE_TEXT`, deleted 2026-11-22, last in `220748e^`)

> I authorize NeurOne Inc. to collect, store, and use my electroencephalogram (EEG) brainwave data — which constitutes biometric identifiers and biometric information under Illinois BIPA (740 ILCS 14), special-category personal data under GDPR Article 9, and consumer health data under Washington MHMD — for the purposes of session delivery, neurofeedback display, and closed-loop stimulation adaptation.
>
> I understand that:
> • This data is encrypted with AES-256 and stored only on my NeurOne device
> • NeurOne cannot access, read, or decrypt this data at any time
> • The data is retained on my device until I request deletion or perform a factory reset, at which point it is permanently destroyed via hardware-level secure erase (eMMC SANITIZE), consistent with NeurOne's publicly available biometric data retention and destruction policy
> • NeurOne will not sell, lease, trade, or otherwise profit from this data
> • NeurOne will not share this data with any third party without my separate written consent, except as required by law
>
> I can revoke this authorization at any time from Settings → Privacy → Manage EEG Data Consent. Revoking authorization prevents future EEG data collection but does not retroactively erase data already stored on my device (use factory reset for full erasure).

### 3.2 The shipping release (`app/NeurOneUI/shared/src/commonMain/kotlin/life/neurone/shared/ui/OnboardingScreens.kt`, `BipaConsentScreen`)

The body is eight keys joined in order (`AGE_NEURONE_RECORDS_EEG_BRAINWAVE_SIGNALS_DURING` through `AGE_ON_YOUR_DEVICE_FOR_SESSION_DELIVERY_AND_NEUR`), then the limits heading and two limits (`BIPA_STEP3_*`, `REQ-ONBOARD-03`):

> NeurOne records EEG (brainwave) signals during sessions. EEG patterns are biometric information. Your recordings are encrypted on your device with a key only you hold — NeurOne cannot read them, ever.
>
> **Retention: your recordings stay on your device until you delete them.** NeurOne's biometric data retention and destruction policy is available at neurone.life/biometric-policy.
>
> By continuing you provide written release for NeurOne to process EEG data on your device for session delivery and neurofeedback.
>
> Absolute limits — NeurOne will never:
> • Sell, lease, trade, or otherwise profit from your biometric data
> • Share it with any third party without your separate written consent, except as required by law

The bold sentence is one key, `AGE_RETENTION_YOUR_RECORDINGS_STAY_ON_YOUR_DEVIC`. It is the only sentence of this text the ruling reaches.

### 3.3 The published policy (`NP-PRIV-NOTICE-001` §10, the "Retention and destruction policy" bullet)

> NeurOne collects brainwave (EEG) biometric data during sessions. It is stored only on your device, encrypted under a key NeurOne does not hold. It is retained until (1) you delete your data in the app, (2) you perform a factory reset, or (3) you request account deletion — after which it is permanently erased from the device using hardware-level secure erasure (eMMC SANITIZE, NIST SP 800-88). NeurOne never receives or retains a copy of your EEG biometric data. This policy is also published at neurone.life/biometric-policy.

`NP-PRIV-REM-001` STEP-34, Step 2, holds the same content as the text required on the website.

## 4. The proposed texts

### 4.1 Draft 1: the shipping release's retention sentence (replaces `AGE_RETENTION_YOUR_RECORDINGS_STAY_ON_YOUR_DEVIC`)

> Retention: your recordings stay on your helmet until they are erased. You will erase your own by deleting your profile, which is not available yet; until it is, you cannot erase them from the helmet yourself. The person or organization that registered the helmet's warranty, which may be you or a clinic, will be able to reset the helmet; that is not available yet either. A reset erases every user's recordings on the helmet, except those the warranty owner chooses to export first. An export stays encrypted, and the warranty owner cannot read it. Erasing the NeurOne app's data on a phone or computer does not erase the helmet's copy.

**Draft 1b, the minimal alternative** (Q1): it says only what exists today and describes nothing planned.

> Retention: your recordings stay on your helmet. There is not yet a way for you to erase them from the helmet. Erasing the NeurOne app's data on a phone or computer does not erase the helmet's copy.

Every other sentence of §3.2 is unchanged, and so is the key order.

### 4.2 Draft 2: the long release, corrected (only if review restores it, Q2)

The first paragraph, the two "NeurOne will not" bullets and the first two bullets are as in §3.1, subject to §6. The retention bullet and the revocation paragraph become:

> • This data is retained on my NeurOne helmet until it is erased. I will be able to erase my own data by deleting my profile; that is not available yet, and until it is I cannot erase it from the helmet myself. The person or organization that registered the helmet's warranty (its warranty owner, which may be me) will be able to reset the helmet; that is not available yet either. A reset permanently erases the data of every user on the helmet, except data the warranty owner chooses to export first, which stays encrypted and which the warranty owner cannot read. This is consistent with NeurOne's publicly available biometric data retention and destruction policy.
>
> I can revoke this authorization at any time from the app's settings. Revoking authorization prevents future EEG data collection but does not erase data already stored on my helmet. Deleting my profile will erase it, once that is available.

### 4.3 Draft 3: the published policy (replaces the §3.3 bullet in `NP-PRIV-NOTICE-001` §10, `NP-PRIV-REM-001` STEP-34 Step 2, and neurone.life/biometric-policy)

> NeurOne collects brainwave (EEG) biometric data during sessions. It is stored on the user's helmet, and in the NeurOne app on the phones and computers they use with it, encrypted under a key NeurOne does not hold. NeurOne never receives or retains a copy of it. It is retained until it is erased. Two ways to erase it from the helmet are designed, and neither is available yet: (1) a user deleting their own profile, which makes that user's data on the helmet unreadable to anyone and deletes it; and (2) the helmet's warranty owner (the person or organization that registered its warranty) resetting the helmet, which permanently erases every user's data on it, except data the warranty owner first chooses to export. An export stays encrypted, and the warranty owner cannot read it. Erasing the app's data on a phone or computer erases the app's copy, not the helmet's. This policy is also published at neurone.life/biometric-policy.

## 5. Each change and why

| # | Change | Why | Source |
|---|---|---|---|
| 1 | "perform a factory reset" / "use factory reset for full erasure" removed as something the person does | A reset is per helmet and only the warranty owner may start one | The principal, 2026-11-23 (`NP-FW-EMMC-002` §B.2 Rev 16; brief §8) |
| 2 | The person's own erasure is named as deleting their profile, marked not available | That is the person's erasure under the shared-helmet design; it is not built: §C.8.4 is design, increments 1 and 2 (§C.8.6, §C.8.7) have no caller, and no app screen deletes a profile | `NP-FW-EMMC-002` §C.8.4, §C.8.6, §C.8.7; the principal, 2026-11-24 (brief §11, Q3) |
| 3 | "until you delete them" (shipping text) removed | No path deletes a recording from the helmet today. The apps' erase ("Erase this computer's data", "Erase this browser's data") erases the app's own store and says it leaves the helmet's data alone | `locales/en.json` `SETTINGS_ERASE_DEVICE_*`, `SETTINGS_ERASE_LOCAL_BODY`; brief §2 |
| 4 | The warranty owner's reset is stated, marked not available, with its consequence for every user and the export exception | It is what the ruling decided, and a person on a clinic's helmet needs to know someone else can erase their data | `NP-FW-EMMC-002` §B.2 Rev 16; brief §8, §11 |
| 5 | "An export stays encrypted, and the warranty owner cannot read it" | The export carries sealed files and the recovery wrap only; the owner holds nothing that opens them | `NP-FW-EMMC-002` §C.8.4, `REQ-EMMC2-04`; brief §8 |
| 6 | "eMMC SANITIZE" (and NIST SP 800-88) removed from the person's erasure | Profile deletion destroys the profile's key record and deletes its files (a cryptographic erase); it is not a SANITIZE. SANITIZE belongs to the reset (R-7a, `REQ-SW02-13`), is host-tested only, and its target HALs trap (`OI-RESET-01`). The JEDEC check and the NIST citation are open (`OI-PRIV-27` (c), (e)) | `NP-FW-EMMC-002` §B.3, §C.8.4; `pending-decisions.md` `OI-PRIV-27` |
| 7 | "account deletion" removed from the policy | NeurOne has no account that holds EEG data, and no account-deletion path exists in the app | `grep` over `locales/en.json` and `app/NeurOneUI/` |
| 8 | "device" becomes "helmet" in the retention sentences; the app's copy is named | Recordings are on the helmet and, after a download (Mode 4), in the app; "device" did not say which, and the app's erase reaches only its own copy | `CLAUDE.md` §4.6; `SETTINGS_ERASE_DEVICE_BODY` |
| 9 | The revocation path "Settings → Privacy → Manage EEG Data Consent" (Draft 2) becomes "the app's settings" | That path named the retired SwiftUI app's screens; the shared app's names differ (`AND_UI_BRAINWAVE_EEG_DATA_CONSENT`) | #946 |

**Kept unchanged in meaning:** what is recorded and why, that it is biometric information, that it is encrypted under a key NeurOne does not hold, that NeurOne cannot read it, the written release and its purposes, the link to the published policy, and the two absolute limits.

## 6. What is still not true or not built

Not changed by the drafts, because the ruling does not reach them; review should see them before approving any text.

- **Neither erasure exists.** Until profile deletion is built, nothing the person does erases their data from the helmet. Until the reset command is built (with or after §C.8, brief §11 Q6), no one can.
- **"Encrypted … with a key only you hold" (§3.2) is not true on a shared helmet today.** The helmet has one UHDR key, and whoever unlocks it reads every user's UHDR until per-person sealing is wired (`OI-PRIV-34` (NP-PRIV-ANALYSIS-004); `NP-FW-EMMC-002` §C.8.6 and §C.8.7: built, no caller).
- **"AES-256" (§3.1)** rests on a key the cryptography review has found is not an XTS-AES-256 key (`OI-PRIV-35` (NP-PRIV-ANALYSIS-004)).
- **"Stored only on my NeurOne device" (§3.1, §3.3)** is not true after a download: the app holds a copy, and a person's own backups may hold another. Drafts 1 to 3 name the app's copy.
- **The shipping release no longer names the statutes** (BIPA, GDPR Article 9, Washington MHMD), the closed-loop adaptation purpose, or the right to revoke. They went with `BIPA_STEP4_RELEASE_TEXT`. Only the limits were restored (`REQ-ONBOARD-03`). That is Q2.
- **BIPA 740 ILCS 14/15(a) asks for a retention schedule** (destruction when the initial purpose is satisfied, or within three years of the last interaction, whichever comes first). Neither the policy nor any draft states one. That is from the statute as recalled, and was not checked against its text in this change. `NP-PRIV-REM-001` already asks counsel whether NeurOne "possesses" the data at all; Q4.
- **The release keys are not marked for legal review.** `locales/_metadata.json` marks `BIPA_STEP3_*` and `BIPA_STEP4_DECLINE_BUTTON` `legal_review_required`, but none of the eight `AGE_*` keys of the release body. The change that ships the approved wording should mark them, so translators are told.
- **The release is built from sentence fragments joined in code**, so a translator sees partial sentences. Moving the retention text into one whole-sentence key is proposed for the shipping change; review rules on the English only.

## 7. The questions

- **Q1. For the shipping release, Draft 1 or Draft 1b?** Draft 1 describes the two designed erasures, each marked "not available yet". It tells a person on a clinic's helmet that the warranty owner will be able to erase their data. But it is a forward-looking statement about features not yet built. Draft 1b says only what exists today, and leaves the designed erasures to the published policy.
- **Q2. Which form is the release: the short shipping text (§3.2, as amended), or the long text restored (Draft 2)?** The long form named the statutes, the closed-loop purpose and the right to revoke. The short form does not. The principal is asked too, because restoring it is a product change.
- **Q3. Approve Draft 3 as the published policy**, replacing the factory-reset and account-deletion triggers and the SANITIZE claim in `NP-PRIV-NOTICE-001` §10, `NP-PRIV-REM-001` STEP-34 Step 2 and neurone.life/biometric-policy?
- **Q4. (A view, not a ruling here.)** Does the policy need a BIPA 15(a) retention schedule, given the possession question already put to counsel?

**What to send back:** for each of Q1 to Q3, approve, approve with edits (give the text), or reject with the reason; any condition that would reopen it; and the reviewer's name, role and date.

## 8. What happens with the answers

- The approved English goes into `locales/en.json` and then into all eleven locales through the translation workflow (`docs/reference/localization.md` §17.7), with the release keys marked `legal_review_required`. The policy paragraph is changed in `NP-PRIV-NOTICE-001` and `NP-PRIV-REM-001` through their revision fragments (`docs/revisions/README.md`), and on the website.
- The `OI-PRIV-27` (NP-PRIV-ANALYSIS-004) row's "the BIPA rewording" item closes with that change. The text is revisited when profile deletion or the reset command ships, so "not available yet" is removed only when it stops being true.
