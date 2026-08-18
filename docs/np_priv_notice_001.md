# NeurOne Privacy Notice

**Project:** NeurOne  
**Document:** NP-PRIV-NOTICE-001  
**Revision:** 4
**Date:** 2026-08-16  
**Status:** ACTIVE  
**Effective Date:** 2026-08-16  
**Author:** Steve Hickman (CEO, interim Quality authority)  
**Approved By:** Steve Hickman, CEO  
**References:** NP-PRIV-001 Rev 2; NP-PRIV-REM-001 Rev 2; NP-FW-EMMC-001 Rev 1; NP-FW-EMMC-002 Rev 1; CLAUDE.md Rev 37 §6.2  
**Related Issues:** —  
**Gate:** —  
**IEC 62304 Class:** —  
**Applicable Standard:** GDPR; CCPA/CPRA; HIPAA; FTC Act §5  
**Next Review:** Annual; also triggered by any material change to data processing  
**Jurisdiction Scope:** Global — applies to NeurOne Home and NeurOne Pro devices; NeurOne iOS app; NeurOne Android app

---

## 1. Who we are

NeurOne Inc. ("NeurOne", "we", "us") is the manufacturer of the NeurOne wearable platform. For users in the European Economic Area, NeurOne Inc. is the data controller for the limited personal data described in this notice.

Contact: privacy@neurone.life

---

## 2. What data your device collects — and who owns it

NeurOne operates a strict two-category data architecture. Understanding the difference is important.

### User Health Data Record (UHDR) — yours, always

Your UHDR contains all data that reflects your biology: EEG brainwave recordings, heart rate variability time series, optical PPG signals, neurofeedback performance scores, session timestamps, the protocol parameters used, closed-loop adaptive events, PBM light-energy dose per zone, and any symptom or outcome notes you enter.

**NeurOne cannot access your UHDR.** It is stored only on your device, encrypted under a key derived from your biometric or PIN. NeurOne has never held and will never hold your decryption key.

Your UHDR is backed up nightly (when the device is on USB-C power) to a location you control — local USB-C drive or an end-to-end encrypted cloud service. NeurOne cannot decrypt your backups.

### System Health Data Record (SHDR) — device diagnostics only

Your SHDR contains data about the *device's condition*, not about you: LED output levels, temperature readings, shielding performance ratios, a session count (a plain unsigned integer — no timestamps), consumable usage counts, firmware version history, and connection logs.

SHDR is linked to your device's serial number and warranty token only — never to your name, email address, or any biometric. It is uploaded to NeurOne when your device is connected via USB-C.

**Important: SHDR consent is held by the warranty owner, not the device user.** The warranty owner is the person or organisation that registered the device — this may be a clinic, employer, or purchasing institution, not the individual wearing the device. A user does not need to take any action to consent to SHDR uploads; that consent was provided at warranty registration by whoever purchased the device. If you are a device user who did not register the warranty yourself, the SHDR consent decision was made by the registrant. SHDR contains no user biology and cannot identify you — it describes your device's condition only.

**Boundary rule:** When in doubt, data is classified as UHDR. Reclassification to SHDR requires positive proof that no user biology is present.

### Two consent subjects

NeurOne operates with two entirely separate consent subjects. They are independent and do not cross-affect each other.

| Consent subject | Scope | Who holds it | What it covers |
|----------------|-------|-------------|---------------|
| **Warranty owner** | SHDR fleet telemetry | The person or organisation that registered the device (may be a clinic, employer, or institution — not necessarily the device user) | Device diagnostics uploaded to NeurOne for warranty and fleet health analytics. No user biology. |
| **Device user** | UHDR research data flows | Each individual who uses the device | Whether and how anonymised session data may be contributed to research studies. Managed entirely within the app by the user. Unrelated to warranty registration. |

A clinic registering 20 devices gives SHDR warranty consent for all 20 devices. Each patient using those devices makes their own independent research consent decisions — the clinic's warranty consent has no effect on patient research consent, and a patient's research withdrawal has no effect on SHDR uploads.

---

## 3. Data we collect through the app

The NeurOne app collects a small amount of data to operate:

- **Account identifier:** your email address, used to manage your subscription (if any) and, if you are the warranty registrant, to manage the warranty. If a clinic, employer, or institution purchased your device, the warranty was registered by them — their contact details are linked to the device, not yours.
- **Device pairing data:** BLE device identifiers, to maintain the connection between your phone and your hub.
- **Session protocol uploads:** the protocol you choose is compiled into an Ed25519-signed binary and uploaded to your hub. A copy is stored on your device only.
- **Consumable state:** counts of consumable sessions remaining, stored locally. Not transmitted to NeurOne.
- **Analytics events:** we use a privacy-respecting analytics SDK (vendor and DPA to be confirmed — see OI-AUDIT-01). We collect app-launch counts, feature interaction events (e.g., "protocol selected"), and crash reports. We do **not** collect session counts, session durations, protocol names, or any UHDR-derived values in analytics. We collect a coarsened engagement tier ("new", "active", or "established") — not a raw session count. The analytics SDK is not initialised until after you complete the consent flow.
- **Consent records:** your consent choices (onboarding, research opt-in layers, clinician access grants) are stored locally and backed up with your UHDR.

---

## 4. Automated processing and adaptive stimulation

**This section is provided under GDPR Article 13(2)(f).** It describes automated processing that affects how your device operates during a session.

### What adaptive stimulation does

NeurOne uses a closed-loop adaptive algorithm during sessions. This algorithm monitors real-time signals from your device — principally your brainwave activity (EEG band patterns) and your heart rhythm (HRV coherence score) — and automatically adjusts stimulation parameters to keep the session working effectively for you.

Parameters the algorithm can adjust include:

- **Audio beat frequency** — the binaural beat or isochronic tone frequency delivered through the headset speakers
- **PBM pulse frequency** — the pulse rate of the near-infrared light panels
- **tACS/BES stimulus frequency or amplitude** — the brainwave entrainment signal
- **VNS stimulation phase** — the timing of the vagus nerve stimulation pulse relative to your breathing cycle

The algorithm does **not** make decisions with legal effects, and it does **not** produce clinical diagnoses. All adaptive changes remain within the safety limits set at the hardware level (the Safety MCU, which cannot be overridden by software).

### Why we do this

Static stimulation protocols are less effective than closed-loop adaptive ones. If your focus-related brainwave activity drops mid-session, the algorithm increases the entrainment frequency to support focus. If your heart rhythm coherence rises above target, it eases the breathing pacer rate to avoid over-correction. The goal is to make every session as effective as possible without any manual adjustment from you.

### What is recorded

Every adaptation event is recorded in your UHDR with a plain-language trigger classification (e.g., "calm-focus brainwave activity dipped"), the modality that was changed, the parameter that changed, and the session timestamp. Raw EEG values and raw HRV values are never stored in the adaptation event record — only the classified trigger and the parameter change.

### Your right to view adaptive adjustments

You can view a summary of all adaptive adjustments made during any session in the **Session History** screen within the NeurOne app. The summary shows each adjustment as a plain-language sentence. For longer sessions, a "View all" option shows the full list.

Because this data is part of your UHDR, it remains on your device under your sole control. NeurOne cannot view your adaptive adjustment history.

### Your rights

You have the right to request a plain-language explanation of any specific adaptive adjustment at any time by contacting privacy@neurone.life. You also have the right to disable closed-loop adaptive stimulation entirely through the app's Session Settings screen, in which case sessions run on fixed parameters only.

---

## 5. Research data (if you have opted in)

If you opted into research participation during onboarding, a separately consented process governs how anonymised session data may be shared with researchers. Key facts:

- Data is anonymised **on your device** before any transmission. NeurOne does not receive your raw UHDR at any point.
- Anonymisation uses k-anonymity (k≥10) and differential privacy (ε≤1.0) techniques.
- You can view all studies your device has contributed to in the **Research** section of the app.
- Withdrawing research consent immediately and permanently stops your device from contributing to any future study — including studies that would have used data from sessions before your withdrawal. Studies that have already published cannot individually remove your anonymised data (explained at the time of consent), but no new data will ever flow.

### 5.1 What the research consent flow asks you

The flow is two screens, and every part of it is optional. Skipping it entirely leaves every device function working identically.

**Screen 1 — what you get back.** Whether you want to hear the results of studies your data contributes to (including studies that found nothing — null results), whether you want access to the research suggestion portal, and if so how we should reach you and how often.

We show this first deliberately, and it is worth being plain about why. A study that uses your data and never tells you what it found has taken something and returned nothing. Being told the results is the other half of that exchange, not a reward for agreeing to it. **Turning these options on gives no one any access to your data, and turning them off costs you nothing** — you have not been asked to share anything at this point in the flow.

**Screen 2 — what you share.** Two separate questions, because they are genuinely different questions:

| Question | What it controls |
|---|---|
| **Which research areas interest you?** | *Scope.* Nine areas — dementia, depression, PTSD, brain injury, sleep, attention, Parkinson's, healthy ageing, visual health. You are contacted only about studies in the areas you pick. |
| **Do you want to be asked about each study?** | *Posture.* By default we ask you separately about every study and you decide each time. You may instead pre-approve reviewed studies, in which case we tell you about each one rather than asking. |

**Selecting all nine areas is not the same as pre-approving studies, and we do not treat it as such.** Ticking every area says you are interested in everything; it does not say you want us to stop asking. If you select all nine and leave the second setting alone, you will still be asked about each individual study — which is a position many people want and which the app lets you hold. The two settings are never linked automatically in either direction.

If you turn on pre-approval, the irreversibility notice is shown alongside it, and remains visible whenever that setting is on.

### 5.2 Withdrawing

You can withdraw at three levels, at any time, from the **Research** section:

| Level | Effect |
|---|---|
| **A single study** | Your device stops contributing to that study. Other studies and app analytics are unaffected. |
| **A research area** | Your device stops contributing to studies in that area. Other areas and app analytics are unaffected. |
| **Pre-approval (all reviewed research)** | Your device stops contributing to every study, and **app research analytics are switched off as well** — turning this off is a signal that you do not want data collection beyond basic device function. Your research-area choices are kept, so you go back to being asked about each study individually. |

Withdrawal is unchanged by the shorter consent flow: making it quicker to say yes has not made it slower, coarser, or harder to say no.

---

## 6. Clinician access (T2 / Pro users)

If you grant a clinician access through the Clinical Consent Engine, they receive read-only access to specific elements of your UHDR — only the elements you approved, for the use case you approved, for the time period you approved. Access is revocable immediately from the app at any time. Revocation takes effect at the API level within minutes.

Clinicians cannot access your adaptive adjustment event log unless you grant Full Clinical tier access.

---

## 7. Your rights (EEA, UK, and California users)

| Right | How to exercise |
|-------|----------------|
| Access | Export your full UHDR archive from the app (Settings → Privacy → Export my data) |
| Erasure | Factory reset your device (Settings → Privacy → Factory reset). This irreversibly deletes all UHDR. SHDR is wiped from the device. NeurOne erases SHDR from its fleet database within 30 days of receiving a verified written request. |
| Portability | UHDR is exported in EDF+ format (open standard). Adaptation event logs export as JSON. |
| Correction | For UHDR data, corrections are made in the app. For warranty/account data, contact privacy@neurone.life. |
| Restriction / objection | Contact privacy@neurone.life. Adaptive stimulation can be disabled entirely from app settings without a formal request. |
| Withdraw consent | **Research consent (user):** withdraw any scope in the app Research section — by study, by category, or blanket. Blanket research withdrawal also revokes app analytics. **Clinician access:** revoke in the Clinical Access section. **Analytics only:** opt out in Settings → Privacy. **SHDR warranty consent:** held by the warranty registrant; contact the registrant or privacy@neurone.life for queries about SHDR consent. |

---

## 8. Data retention

| Data | Retention | Deletion mechanism |
|------|-----------|-------------------|
| UHDR (on-device) | Until you delete or factory reset | eMMC SANITIZE (NIST SP 800-88) |
| SHDR (NeurOne fleet DB) | While warranty is active + 5 years | Written erasure request by warranty owner; 30-day SLA |
| Warranty / account data (warranty owner) | While account is open + 2 years | Account deletion request by warranty owner |
| Anonymised research data | Per study protocol; NeurOne cannot individually remove | Irreversibility notice given at consent |
| Analytics events | 90 days rolling (vendor-dependent) | Automatic |

---

## 9. Security

UHDR is encrypted with AES-256-XTS using a key derived from your biometric or PIN (Argon2id, 64 MB memory, 4 iterations). NeurOne holds no copy of your key and cannot decrypt your UHDR under any circumstances.

SHDR is encrypted with AES-256-XTS using a key derived from NeurOne's manufacturing root key and your device's unique hardware identifier. This allows NeurOne to read SHDR for warranty and fleet health analytics.

All over-the-air firmware updates are Ed25519-signed. The device rejects any unsigned or corrupted update.

---

## 10. Biometric data — your written release and our retention policy

Your EEG brainwave recordings are **biometric information**. Some laws impose specific requirements on biometric data — most notably the Illinois Biometric Information Privacy Act (BIPA, 740 ILCS 14) and EU GDPR Article 9. **We apply these protections to every NeurOne user, everywhere, regardless of where you live.** We do not switch this protection on or off based on your location.

- **Written release.** Before your first EEG session, we ask for your explicit written release to collect and store your EEG biometric data. You may decline. If you decline, EEG recording and closed-loop adaptive (EEG-driven) stimulation are disabled — the device still functions for light (PBM), vagus-nerve, audio, and visual modalities.
- **Retention and destruction policy.** NeurOne collects brainwave (EEG) biometric data during sessions. It is stored only on your device, encrypted under a key NeurOne does not hold. It is retained until (1) you delete your data in the app, (2) you perform a factory reset, or (3) you request account deletion — after which it is permanently erased from the device using hardware-level secure erasure (eMMC SANITIZE, NIST SP 800-88). NeurOne never receives or retains a copy of your EEG biometric data. This policy is also published at neurone.life/biometric-policy.
- **No profiting from biometric data.** We do not sell, lease, trade, or otherwise profit from your biometric data (see §11).

---

## 11. We do not sell your data

**We do not sell, lease, or trade your data — and this applies to every user, everywhere.** This covers:

- your UHDR (EEG, HRV, PPG, dose, adaptive events, and everything else on your device);
- your SHDR device diagnostics, **including behavioral indicators such as consumable usage counts and the device session count**; and
- your app and account data.

Washington's My Health My Data Act (RCW 70.372) prohibits the sale of consumer health data and requires a standalone authorization — separate from any HIPAA consent — before such data is collected. Rather than apply that protection only to Washington residents, **we apply it as a baseline commitment to all NeurOne users.** Accordingly: (1) we never sell your data, even with your permission; (2) SHDR uploads are governed by a standalone warranty-owner authorization that is separate and distinct from any HIPAA consent, obtained from every warranty owner (not only Washington registrants); and (3) we do not condition any device function on permission to sell or share your data.

---

## 12. Changes to this notice

We will notify you in-app of any material change to this notice before it takes effect. The notice version and effective date are shown at the top of this document.

*Rev 4 (2026-08-16): §5 expanded with §5.1 (what the research consent flow asks) and §5.2 (withdrawal levels), reflecting the move from four consent screens to two (CLAUDE.md Rev 37 §6.2). **No data processing changed and no consent option was removed.** The four consent layers are unchanged; they are now presented on two screens, with the reciprocity question (results and portal) shown first. §5.1 states in the notice's own voice the two things a user could otherwise get wrong: that opting into results grants no data access, and that selecting all nine research areas is not the same as pre-approving studies — the app never links those two settings automatically. §5.2 sets out the three withdrawal levels and states plainly that the shorter flow to consent has not made withdrawal any coarser. §7's withdraw-consent row is unchanged and remains accurate.*

*Rev C (2026-07-10): Added §10 (biometric written release + retention/destruction policy) and §11 (no-sale), both stated unconditionally for all users. These universalize protections whose driving regulations are BIPA (IL) and MHMD (WA) respectively, per NeurOne's most-privacy-protecting-globally principle. No change to data processing — these sections make existing universal commitments explicit.*

---

*NeurOne Inc. · privacy@neurone.life*
