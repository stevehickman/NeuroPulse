# NeuroPulse Privacy Notice

**Document:** NP-PRIV-NOTICE-001 Rev A  
**Effective date:** 2026-06-03  
**Applies to:** NeuroPulse Home and NeuroPulse Pro devices; NeuroPulse iOS app; NeuroPulse Android app  
**Related documents:** NP-PRIV-001 Rev B · NP-PRIV-REM-001 Rev B · NP-FW-EMMC-001 Rev A · NP-FW-EMMC-002 Rev A

---

## 1. Who we are

NeuroPulse Inc. ("NeuroPulse", "we", "us") is the manufacturer of the NeuroPulse wearable platform. For users in the European Economic Area, NeuroPulse Inc. is the data controller for the limited personal data described in this notice.

Contact: privacy@neuropulse.com

---

## 2. What data your device collects — and who owns it

NeuroPulse operates a strict two-category data architecture. Understanding the difference is important.

### User Health Data Record (UHDR) — yours, always

Your UHDR contains all data that reflects your biology: EEG brainwave recordings, heart rate variability time series, optical PPG signals, neurofeedback performance scores, session timestamps, the protocol parameters used, closed-loop adaptive events, PBM light-energy dose per zone, and any symptom or outcome notes you enter.

**NeuroPulse cannot access your UHDR.** It is stored only on your device, encrypted under a key derived from your biometric or PIN. NeuroPulse has never held and will never hold your decryption key.

Your UHDR is backed up nightly (when the device is on USB-C power) to a location you control — local USB-C drive or an end-to-end encrypted cloud service. NeuroPulse cannot decrypt your backups.

### System Health Data Record (SHDR) — device diagnostics only

Your SHDR contains data about the *device's condition*, not about you: LED output levels, temperature readings, shielding performance ratios, a session count (a plain unsigned integer — no timestamps), consumable usage counts, firmware version history, and connection logs.

SHDR is linked to your device's serial number and warranty token only — never to your name, email address, or any biometric. It is uploaded to NeuroPulse when your device is connected via USB-C, subject to your warranty consent.

**Boundary rule:** When in doubt, data is classified as UHDR. Reclassification to SHDR requires positive proof that no user biology is present.

---

## 3. Data we collect through the app

The NeuroPulse app collects a small amount of data to operate:

- **Account identifier:** your email address, used to register your warranty and manage your subscription (if any).
- **Device pairing data:** BLE device identifiers, to maintain the connection between your phone and your hub.
- **Session protocol uploads:** the protocol you choose is compiled into an Ed25519-signed binary and uploaded to your hub. A copy is stored on your device only.
- **Consumable state:** counts of consumable sessions remaining, stored locally. Not transmitted to NeuroPulse.
- **Analytics events:** we use a privacy-respecting analytics SDK (vendor and DPA to be confirmed — see OI-AUDIT-01). We collect app-launch counts, feature interaction events (e.g., "protocol selected"), and crash reports. We do **not** collect session counts, session durations, protocol names, or any UHDR-derived values in analytics. We collect a coarsened engagement tier ("new", "active", or "established") — not a raw session count. The analytics SDK is not initialised until after you complete the consent flow.
- **Consent records:** your consent choices (onboarding, research opt-in layers, clinician access grants) are stored locally and backed up with your UHDR.

---

## 4. Automated processing and adaptive stimulation

**This section is provided under GDPR Article 13(2)(f).** It describes automated processing that affects how your device operates during a session.

### What adaptive stimulation does

NeuroPulse uses a closed-loop adaptive algorithm during sessions. This algorithm monitors real-time signals from your device — principally your brainwave activity (EEG band patterns) and your heart rhythm (HRV coherence score) — and automatically adjusts stimulation parameters to keep the session working effectively for you.

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

You can view a summary of all adaptive adjustments made during any session in the **Session History** screen within the NeuroPulse app. The summary shows each adjustment as a plain-language sentence. For longer sessions, a "View all" option shows the full list.

Because this data is part of your UHDR, it remains on your device under your sole control. NeuroPulse cannot view your adaptive adjustment history.

### Your rights

You have the right to request a plain-language explanation of any specific adaptive adjustment at any time by contacting privacy@neuropulse.com. You also have the right to disable closed-loop adaptive stimulation entirely through the app's Session Settings screen, in which case sessions run on fixed parameters only.

---

## 5. Research data (if you have opted in)

If you opted into research participation during onboarding, a separately consented process governs how anonymised session data may be shared with researchers. Key facts:

- Data is anonymised **on your device** before any transmission. NeuroPulse does not receive your raw UHDR at any point.
- Anonymisation uses k-anonymity (k≥10) and differential privacy (ε≤1.0) techniques.
- You can view all studies your device has contributed to in the **Research** section of the app.
- Withdrawing research consent immediately and permanently stops your device from contributing to any future study — including studies that would have used data from sessions before your withdrawal. Studies that have already published cannot individually remove your anonymised data (explained at the time of consent), but no new data will ever flow.

---

## 6. Clinician access (T2 / Pro users)

If you grant a clinician access through the Clinical Consent Engine, they receive read-only access to specific elements of your UHDR — only the elements you approved, for the use case you approved, for the time period you approved. Access is revocable immediately from the app at any time. Revocation takes effect at the API level within minutes.

Clinicians cannot access your adaptive adjustment event log unless you grant Full Clinical tier access.

---

## 7. Your rights (EEA, UK, and California users)

| Right | How to exercise |
|-------|----------------|
| Access | Export your full UHDR archive from the app (Settings → Privacy → Export my data) |
| Erasure | Factory reset your device (Settings → Privacy → Factory reset). This irreversibly deletes all UHDR. SHDR is wiped from the device. NeuroPulse erases SHDR from its fleet database within 30 days of receiving a verified written request. |
| Portability | UHDR is exported in EDF+ format (open standard). Adaptation event logs export as JSON. |
| Correction | For UHDR data, corrections are made in the app. For warranty/account data, contact privacy@neuropulse.com. |
| Restriction / objection | Contact privacy@neuropulse.com. Adaptive stimulation can be disabled entirely from app settings without a formal request. |
| Withdraw consent | Research consent: withdraw in the app Research section. Clinician access: revoke in the Clinical Access section. Analytics: opt out in app Settings → Privacy. |

---

## 8. Data retention

| Data | Retention | Deletion mechanism |
|------|-----------|-------------------|
| UHDR (on-device) | Until you delete or factory reset | eMMC SANITIZE (NIST SP 800-88) |
| SHDR (NeuroPulse fleet DB) | While warranty is active + 5 years | Written erasure request; 30-day SLA |
| Warranty / account data | While account is open + 2 years | Account deletion request |
| Anonymised research data | Per study protocol; NeuroPulse cannot individually remove | Irreversibility notice given at consent |
| Analytics events | 90 days rolling (vendor-dependent) | Automatic |

---

## 9. Security

UHDR is encrypted with AES-256-XTS using a key derived from your biometric or PIN (Argon2id, 64 MB memory, 4 iterations). NeuroPulse holds no copy of your key and cannot decrypt your UHDR under any circumstances.

SHDR is encrypted with AES-256-XTS using a key derived from NeuroPulse's manufacturing root key and your device's unique hardware identifier. This allows NeuroPulse to read SHDR for warranty and fleet health analytics.

All over-the-air firmware updates are Ed25519-signed. The device rejects any unsigned or corrupted update.

---

## 10. Changes to this notice

We will notify you in-app of any material change to this notice before it takes effect. The notice version and effective date are shown at the top of this document.

---

*NeuroPulse Inc. · privacy@neuropulse.com*
