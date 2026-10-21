# iOS App Development Roadmap

**Project:** NeurOne  
**Document:** NP-APP-ROADMAP-001  
**Revision:** 9
**Date:** 2026-10-21  
**Status:** BASELINED  
**Effective Date:** 2026-07-13  
**Author:** Steve Hickman (CEO, interim Quality authority)  
**Approved By:** Steve Hickman, CEO  
**References:** —  
**Related Issues:** GitHub Issue #32  
**Gate:** —  
**IEC 62304 Class:** —  
**Supersedes:** NP-APP-ROADMAP-001 Rev 2  
**Change Summary:** Rev 9 (2026-10-21) — **The Apple Watch app is retired (the principal, 2026-10-21).** §4 (the Watch sync app, Phases 1 to 4), the WatchConnectivity mirror in §5 and the Watch statements in §6 are retired in place, not deleted (`CLAUDE.md` §18); `OI-WA-02`, `OI-WA-03` (the Watch latency item; the same ID names the hub warranty-token item elsewhere, which this does not touch) and `OI-WA-05` are retired with them, and `OI-WA-01`, `OI-WA-04` stay superseded. The code is deleted: `app/watchos/`, the iOS `WatchBridge`, `watchos-ci.yml`. The phone-side Watch preset gate (the old `PhoneSessionManager.acceptedPreset`) goes with it; the protocol menu's consent gate, which is the gate that matters, is unchanged (`REQ-ONBOARD-05`). No requirement on the helmet, the hub or the shared apps is changed. Rev 8 (2026-10-21) — `REQ-NOTIFY-01` (a) says what a host with no private body shows: the desktop balloon carries the title and subtitle only, never the consumable or its count (the stricter reading of `NP-PRIV-ANALYSIS-003` LOW-1; no requirement is relaxed). Rev 7 (2026-10-20) — §9.7 gives the HealthKit residency rule (§9.1) and the lock-screen and permission-timing rules for consumable reminders (`NP-PRIV-ANALYSIS-003` LOW-1, LOW-2) an ID each (`REQ-HEALTH-01`, `REQ-NOTIFY-01`), so the shared iOS host's tests can name them. No requirement is added, changed or relaxed. Rev 6 (2026-10-11) — §9.6 gives the onboarding rules §9.2 and §9.3 already make binding an ID each (`REQ-ONBOARD-01` to `-06`), so a test can name the rule it verifies (`scripts/req-trace.ts`). No requirement is added, changed or relaxed. Rev 5 (2026-09-23) — §5 gains three optional T2 characteristics for `NP-SW-FAULTMSG-001`: `CVNS_FAULT_STATUS` (offline cervical faults, the hub re-enable state and the per-user cardiac flags), `CVNS_REENABLE_CONFIRM` (the wearer's `REQ-CVNS-09` confirmation) and `ACTIVE_USER` (the opaque profile tag). None is required, and no hub publishes them yet (`OI-WA-03`). Rev 4 (2026-09-22) — §5 gains `CVNS_PAD_STATUS` (T2, optional, NOTIFY 4 B): the cervical gel pad contact result with the side of the neck of each pad, so the phone app can tell the wearer which pad location failed (`OI-ACC-07`). Rev 3: Corrected a platform-fact error in the Watch Phase 3 haptic spec. Core Haptics (`CHHapticEngine`) does not exist on watchOS; the only watchOS haptic API is `WKInterfaceDevice.play(_:)`, which cannot render a continuous 40Hz waveform. Phase 3 on the Watch is rescoped from "40Hz continuous Core Haptics" to a low-rate **rhythmic session cue** (WKInterfaceDevice named haptic, explicitly not 40Hz, honestly labelled as a supplement per §3b/§15). True 40Hz Core Haptics is achievable only on iPhone (where the phone is the contact surface); therapeutic 40Hz vibrotactile remains the mastoid LRA pad. OI-WA-01 and OI-WA-04 superseded (no continuous Core Haptics path exists on watchOS to characterise); OI-WA-05 rescoped. §4.1 diagram + channel table, §4.2 Phase 3, §7/§10 open items updated. Rev 2 change (retained): §9 Privacy Constraints; HealthKit binding; age gate; BIPA release; adaptive transparency card; OI-WA-06, OI-PA-01..04.

---

## 1. Scope

This document defines the development roadmap for the NeurOne iOS application, including all core session-management functionality. **The Apple Watch companion sync app this document once also planned is retired (Rev 9, 2026-10-21): §4 is kept for the record and builds nothing.**

The iOS app is a software-only addition. BOM delta: $0.

---

## 2. Communication Architecture

```
NeurOne Hub (BT 5.3 LE, antennas in hub)
        │
        │  BLE GATT custom service
        │  Sub-50ms sync latency target
        ▼
iOS App
```

*(Rev 9: the watchOS app that used to hang off the iOS app over WatchConnectivity is retired.)*

BT 5.3 LE radio and antennas reside in the control hub only — not the headset. The iOS app communicates with the hub over a custom BLE GATT service. 

---

## 3. Core iOS App

Core iOS app scope (not detailed in this document):

- Session protocol authoring, signing, and upload to hub (Mode 2 Programming)
- Real-time EEG/HRV/session status display (Mode 1 Connected)
- UHDR/SHDR partition management and nightly backup scheduling
- Clinical consent engine (§6 CLAUDE.md)
- Consumable inventory and reminder engine
- OTA firmware update orchestration
- User account, onboarding, and first-session setup

**Core iOS app target:** Month 12 post-company-formation (aligned with T1 hardware launch).

---

## 4. Apple Watch Sync App — RETIRED (Rev 9, 2026-10-21)

> **RETIRED.** The principal retired the Watch app on 2026-10-21. Nothing below is planned, built or maintained; the text is kept in place so the history of why it was specified is not lost (`CLAUDE.md` §18). The therapeutic 40 Hz vibrotactile channel is the mastoid LRA pad, which never depended on the Watch.

### 4.1 Overview

The Watch app is a free companion delivering three sync channels. All Watch-delivered functions are declared as session monitoring and user interface aids — not therapeutic delivery. Therapeutic claims attach to NeurOne hardware only. App Store category: General Wellness.

| Channel | Function | watchOS API |
|---------|----------|-------------|
| Haptic cue | Low-rate rhythmic "session-active" wrist cue (NOT 40Hz, NOT therapeutic) | `WKInterfaceDevice.play(_:)` named haptic (Core Haptics is unavailable on watchOS) |
| Audio | Binaural beats / isochronic tones / breathing pacer to AirPods | AVAudioSession, routed to paired AirPods |
| Visual | 40Hz flicker, EMDR L/R indicator, HRV breathing ring, session status | SwiftUI, WKInterfaceDevice display |

### 4.2 Development Phases

Development priority order is determined by implementation complexity and user utility, not by channel definition order above.

#### Phase 1 — Session Status + HRV Breathing Ring
**Priority:** Highest utility, lowest complexity  
**Target:** Month 1–2 post-core-app-launch

- Session timer (start, elapsed time, haptic end-of-session alert)
- Protocol name display on Watch face
- HRV biofeedback breathing ring: expanding/contracting SwiftUI animation synchronised to pacer phase from np_hrv_pacer (see NP-FW-HRV-001 §7)
- Real-time coherence score display (0–10, colour-coded) — wrist-glance complement to iPhone display
- RMSSD per-session value
- Quick impedance check result notification (pass/fail push from iPhone app)
- Consumable low reminders (push from iPhone app)
- Protocol selector for 3 basic session presets (without full iPhone UI)

**App Store submission target:** Month 3 post-core-app-launch

#### Phase 2 — Audio Sync
**Target:** Month 3–4 post-core-app-launch

- Binaural beats, isochronic tones, and pink/brown noise playback through AirPods paired to Watch
- Session clock synchronisation: audio start timestamp transmitted from hub via GATT → iPhone → WatchConnectivity; Watch audio engine aligns to hub session offset on resume
- Breathing pacer audio cue (bone conduction channel reserved for this on hub; Watch AirPods handle binaural content in parallel)
- AVAudioSession category: `.playback`, mode `.default`, shared with iPhone app via WatchConnectivity state transfer
- Latency budget: WatchConnectivity message round-trip ≤50ms; audio pre-buffer 200ms at session start to absorb jitter

#### Phase 3 — Haptic Session Cue (WKInterfaceDevice, NOT 40Hz)
**Target:** Month 4–5 post-core-app-launch

**Platform correction (2026-07-13):** The Rev 1/B spec assumed a continuous 40Hz `CHHapticEngine` waveform on the Watch. **Core Haptics does not exist on watchOS** — `CHHapticEngine`, `CHHapticPattern`, and `CHHapticEvent` are iOS/iPadOS only. The sole watchOS haptic API is `WKInterfaceDevice.play(_:)`, which fires a fixed set of *named* haptics on demand and **cannot render an arbitrary continuous 40Hz waveform**. Phase 3 on the Watch is therefore delivered as a low-rate rhythmic session cue, not a 40Hz vibrotactile channel.

- Implementation: `WKInterfaceDevice.current().play(.click)` fired by a repeating timer at a gentle, reliable cadence (default 2Hz / one tap per 500ms). watchOS coalesces/rate-limits rapid `play()` calls, so a continuous buzz — and any true 40Hz drive — is not attainable; the cadence is a deliberately low, *felt* "session-active" pulse.
- Purpose: a supplementary wrist presence indicating the session is running. Declared as a user-interface aid, not therapeutic delivery (consistent with §6 regulatory notes).
- Sync: session epoch is UHDR-class and excluded from the WatchConnectivity bridge (NP-PRIV-ANALYSIS-002 MEDIUM-08); the cue starts immediately with no meaningful sync offset. Auto-stops on session idle/completed and on a 20-minute safety cap.
- Thermal guard: a repeating haptic timer carries far less thermal/battery cost than the (nonexistent) continuous Core Haptics path, but the manager still monitors `ProcessInfo.thermalState` and pauses the cue at `.serious`/`.critical`.
- Where true 40Hz lives: the **mastoid LRA pad** (hardware, DRV2605L @ 40Hz ±0.5Hz) is the therapeutic vibrotactile channel. The **iPhone** app can render 40Hz via Core Haptics where the phone is the contact surface. The Watch delivers the coarse cue only.
- Marketing caveat displayed in-app: "For the full 40Hz vibrotactile experience, use the NeurOne mastoid vibrotactile pad (available separately). The Apple Watch haptic is a complementary wrist cue — not a substitute for mastoid bone coupling." (See docs/reference/marketing-notes.md for approved marketing copy.)
- Implemented in `app/watchos/NeurOneWatch/Phase3/HapticSyncManager.swift`.

#### Phase 4 — 40Hz Visual Flicker
**Target:** Month 6+ post-core-app-launch, after screen characterisation

**Blocking prerequisite:** Watch screen brightness must be characterised at 40Hz before this phase ships. Requirement: ≥100 nits sustained output at 40Hz on-off cycle. Characterisation test:
1. Drive Watch display with full-white / full-black 40Hz SwiftUI animation at maximum brightness
2. Measure luminance with calibrated photometer at 10cm (standard photopic test distance)
3. Confirm ≥100 nits at 40Hz; if not achieved at 40Hz, test at highest achievable frequency ≥20Hz
4. Log characterisation results in SHDR as device metadata for fleet tracking

Until characterisation passes:
- Visual flicker channel must not ship
- Watch display used for EMDR L/R indicator arrow and HRV breathing ring only (no therapeutic flicker claim)

When characterisation passes:
- 40Hz visual flicker: full-white/black SwiftUI animation, synchronised to hub session 40Hz gamma clock
- EMDR L/R alternation arrow: session-clock-driven L→R→L cue at EMDR protocol cadence
- Photoparoxysmal safety note in Watch app: "Visual flicker is disabled automatically if your NeurOne EEG detects a photoparoxysmal response. Watch app mirrors this safety cutoff within one WatchConnectivity message cycle."

---

## 5. BLE GATT Service Definition

Custom BLE GATT service: UUID `4E455550-0001-1000-8000-00805F9B34FB`; each characteristic is `4E455550-XXXX-1000-8000-00805F9B34FB` with the id in the second group (assigned in the apps' `NPUUID` / `GattUuids`, and built from the same base by the hub, `firmware/hub_control/include/np_gatt_server.h`).

**What the hub publishes (2026-09-25, GitHub #381):** only characteristics it has a producer for — `CONSUMABLE_STATUS` (0x0007), `WARRANTY_TOKEN` (0x0010), `CVNS_FAULT_STATUS` (0x0014), `CVNS_REENABLE_CONFIRM` (0x0015), `ACTIVE_USER` (0x0016) and `DEVICE_SERIAL` (0x0017). The others below are parsed by the apps and not yet published; each is added with its producer. `CVNS_PAD_STATUS` waits on `OI-ACC-07`. The BLE host stack under the server is a platform trap until the radio is selected.

Characteristic layout:

| Characteristic | Direction | Length | Content |
|---------------|-----------|--------|---------|
| SESSION_STATE | NOTIFY | 4 bytes | Session epoch (uint32, ms since Unix epoch) |
| SESSION_STATUS | NOTIFY | 2 bytes | Protocol ID (uint8) + status flags (uint8) |
| HRV_COHERENCE | NOTIFY | 4 bytes | Coherence score × 100 (uint16) + RMSSD ms (uint16) |
| PACER_PHASE | NOTIFY | 2 bytes | Phase (uint8: 0=inhale, 1=exhale) + elapsed % (uint8) |
| IMPEDANCE_RESULT | NOTIFY | 2 bytes | Pass/fail flags per electrode (uint16 bitmask) |
| CONSUMABLE_STATUS | READ/NOTIFY + WRITE | 8 bytes (write: 1 byte) | Per-consumable session counts since replacement (4× uint16 LE: intranasal sleeves, hydrogel tips, VNS clip pads, audio cup foam). The hub owns them: each advances by one at the end of a session that drove its modality (intranasal, EEG, auricular VNS, audio). **WRITE** one byte, the kind index 0–3, when the wearer marks that part replaced: the hub zeroes it and notifies the zero. SHDR (`OI-ACC-08`, `firmware/hub_control/include/np_consumables.h`) |
| CVNS_PAD_STATUS | NOTIFY | 4 bytes | T2 only, optional. Failed-electrode bitmask (uint8) + check (uint8: 0 = pre-enable, 1 = mid-session) + side of the neck of electrode 1's and electrode 2's pad (2× uint8: 1 = left, 2 = right). Hub supplies the side; UHDR, display only (`OI-ACC-07`) |
| CVNS_FAULT_STATUS | READ/NOTIFY | 4 + 8n bytes | T2 only, optional (`0x0014`). Version + hub re-enable state + n (≤ 4) + flags (bit 0 = the active user's cervical VNS is withheld; bit 1 = someone on this device has an outstanding cardiac cutoff), then per record: session counter (uint32) + fault kind (uint8) + failing-pad side mask (uint8) + 2 reserved. The active user's own records; UHDR (`NP-SW-FAULTMSG-001` §9.3) |
| CVNS_REENABLE_CONFIRM | WRITE | 1 byte | T2 only, optional (`0x0015`). `0x01` = the wearer confirms resuming after a cardiac cutoff (`REQ-CVNS-09`); accepted only while the hub awaits it |
| WARRANTY_TOKEN | READ | 32 bytes | `0x0010`. The 256-bit TRNG warranty token (`NP-FW-EMMC-002` §A): generated at the first read, persisted in the Config partition, never user-linked. SHDR-class device identity (`OI-WA-03`) |
| ACTIVE_USER | WRITE | 4 bytes | Optional (`0x0016`). Opaque little-endian tag from the active individual profile; scopes a cardiac cutoff to that person. Forwarded to the safety MCU only while no session runs |
| DEVICE_SERIAL | READ | 32 bytes | `0x0017`, **encrypted link only** (`NP_GATT_PROP_ENC`: the stack refuses a read on an unencrypted link). The hub's replay-guard serial, the same bytes `np_protocol_verify_and_parse()` compares a descriptor's header against (`NP-FW-HUB-001` §4.2). The app stamps it into the descriptor it compiles so the hub does not refuse it `WRONG_DEVICE`. Held in memory for the link, never persisted, uploaded or displayed. SHDR-class device identity, not user data (`OI-AND-WIRE-02`) |

Notification interval: 100ms for SESSION_STATE and PACER_PHASE; 5s for HRV_COHERENCE; event-driven for IMPEDANCE_RESULT, CONSUMABLE_STATUS and CVNS_PAD_STATUS (on a change of result within a session attempt, and at each new attempt).

~~WatchConnectivity message from iPhone app to Watch app mirrors the GATT data in real time, reformatted as a WCSession `sendMessage` dictionary.~~ *Retired, Rev 9.*

---

## 6. Regulatory Notes

*(Rev 9: the Watch statements in this section are retired with the Watch app; they bind nothing.)*

All Watch-delivered functions must be described in App Store metadata and in-app UI as:

> "Session monitoring and user interface aids for use with a NeurOne wearable. The Apple Watch app does not deliver therapeutic stimulation. All therapeutic functions are provided exclusively by NeurOne hardware."

- App Store primary category: Health & Fitness (not Medical)
- No claim that Watch haptic, audio, or visual output produces therapeutic neurological effects
- Watch app version numbers tracked in SHDR (app_version field) for fleet analytics
- 40Hz visual flicker channel (Phase 4) carries the following in-app disclosure: "This feature uses flashing light. Do not enable if you have photosensitive epilepsy or have been advised to avoid flashing lights."

---

## 7. Open Items

| ID | Item | Owner | Blocking |
|----|------|-------|---------|
| OI-WA-01 | SUPERSEDED (2026-07-13) — no continuous Core Haptics path exists on watchOS to characterise. Phase 3 is now a low-rate WKInterfaceDevice cue; the lightweight repeating-timer thermal cost is handled at runtime via `ProcessInfo.thermalState` auto-pause. | SW Engineering | Not blocking |
| OI-WA-02 | RETIRED (2026-10-21, the Watch app is retired) — Watch screen brightness characterisation at 40Hz ≥100 nits | SW Engineering | Phase 4 (visual flicker channel) |
| OI-WA-03 | RETIRED (2026-10-21, the Watch app is retired) — WatchConnectivity latency measurement on current watchOS release | SW Engineering | Phase 2 audio sync quality gate |
| OI-WA-04 | SUPERSEDED (2026-07-13) — `CHHapticEngine` does not exist on watchOS; there is no 40Hz Core Haptics frequency to verify on any Apple Watch model. (40Hz verification applies only to the iPhone Core Haptics path and the mastoid LRA pad, tracked elsewhere.) | SW Engineering | Not blocking |
| OI-WA-05 | RETIRED (2026-10-21, the Watch app is retired) — App Store review pre-submission check for the Phase 3 haptic cue — confirm the low-rate `WKInterfaceDevice.play` cue and its in-app copy carry no therapeutic/40Hz claim (rescoped from Core Haptics 40Hz continuous use) | SW/Regulatory | Phase 3 |

---

## 8. Status Summary

| Phase | Channel | Complexity | Target | Status |
|-------|---------|------------|--------|--------|
| 1 | Session status + HRV breathing ring | Low | Month 1–2 post-core-app | Retired (Rev 9) |
| 2 | Audio sync to AirPods | Medium | Month 3–4 post-core-app | Retired (Rev 9) |
| 3 | Haptic session cue (WKInterfaceDevice, not 40Hz) | Low | Month 4–5 post-core-app | Retired (Rev 9) |
| 4 | 40Hz visual flicker | High | Month 6+ post-core-app | Retired (Rev 9) |

---

---

## 9. Privacy Constraints (Binding — NP-PRIV-REM-001 Rev 2)

The following are binding engineering constraints, not optional guidelines. Deviation requires a formal design change order with Privacy Lead sign-off under NP-QMS-DC-001.

### 9.1 HealthKit data residency (NP-PRIV-REM-001 STEP-16)

> **HealthKit data accessed by the NeurOne iOS app is used for real-time session display only. It is not persisted, not cached beyond the active session, not transmitted to NeurOne servers, not transmitted to any analytics or crash reporting vendor, and not used for any purpose outside the active session in which it was read.**

Permitted HealthKit quantity types (Phase 1): `HKQuantityTypeIdentifierHeartRateVariabilitySDNN`, `HKQuantityTypeIdentifierHeartRate`. No other HealthKit types may be requested without a formal design change order and App Store privacy label update.

Any future proposal to transmit HealthKit data to NeurOne servers or any third party requires: (a) Privacy Lead written approval; (b) updated App Privacy Nutrition Label in App Store Connect; (c) updated GDPR Art. 13 privacy notice; (d) new BAA/DPA if the recipient is a vendor.

### 9.2 Minimum age gate (NP-PRIV-001 Rev 2 MEDIUM-03)

The app consent flow must include a minimum age declaration **before** any personal data is collected or any consent is presented:

> ☐ **I confirm I am 16 years of age or older.** *(Required)*

Implementation requirements:
- The checkbox is not pre-ticked (GDPR dark patterns prohibition)
- The flow cannot proceed past this screen if the checkbox is unchecked
- No age verification beyond a declaration is required — the declaration creates a terms-of-service record
- Add OI-PA-01: legal counsel confirms 16 is the correct threshold (covers COPPA 13, most EU GDPR member states 16, BIPA adults-only requirement)
- For T2 clinical minor patients: a separate "Authorised Guardian" pathway is required (OI-PA-02)

### 9.3 Biometric (EEG) written release — ALL users (NP-PRIV-001 HIGH-01)

**Universal (2026-07-10):** the consent flow includes a **separate biometric written release screen** (not bundled with general consent), shown to **every user regardless of location** — no IP, locale, or state detection. BIPA 740 ILCS 14/15(b)(1)–(3) is the driving standard, but the control is applied globally (also satisfies GDPR Art. 9 and WA MHMD). `RegionHelper.isLikelyIllinois` was deleted. Required elements:

```
Screen title: "Brain Activity Data Consent"

Body text (required):
"NeurOne collects your brainwave (EEG) data during sessions to 
provide neurofeedback and to adapt stimulation settings in real time.
Brainwave data is sensitive personal information — biometric 
information under applicable law.

• Purpose: Session operation, neurofeedback display, closed-loop adaptation
• Retention: Until you delete your data or transfer/sell your device
• Destruction method: Secure hardware-level erasure (eMMC SANITIZE)
• NeurOne will not sell, lease, or profit from your brainwave data
• NeurOne will not share your brainwave data with third parties 
  without your separate consent, except as required by law

Do you consent to NeurOne collecting and using your brainwave data 
as described above?"

[Yes, I consent]    [No, decline]
```

If the user declines, EEG neurofeedback and closed-loop adaptive stimulation are disabled. The device still functions for PBM, VNS, audio entrainment, and visual stimulation. A separate toggle to re-enable EEG is available in Settings after accepting the consent.

OI-PA-03 (locale gating of the BIPA screen) is RESOLVED: the screen is shown to all users; no location detection remains. BIPA counsel review of the copy is advisory and does not gate presentation (showing the disclosure to everyone is the conservative default).

### 9.4 Adaptive stimulation transparency card (NP-PRIV-REM-001 STEP-33)

The Session History screen must include an "Adaptive Adjustments" card for any session containing closed-loop adaptive events. Requirements:

- Rendered from a fixed plain-language enum (maintained in the app codebase — see NP-PRIV-001 Rev 2 MEDIUM-05 for the full trigger enum mapping)
- Maximum 5 events displayed; "and N more" with "View all" link for longer lists
- No raw EEG values visible (band power ratios stay in UHDR, not displayed to user)
- Each event is a single plain-language sentence from the approved trigger enum
- The trigger enum must be extended whenever new adaptive triggers are added to firmware (add as a change control checklist item in NP-QMS-DC-001)
- Add OI-PA-04: Privacy Lead sign-off on plain-language trigger copy before any build with the Adaptive Adjustments card ships

### 9.5 SDK initialisation gate (NP-APP-TELEMETRY-001 Rev 2 §5)

No analytics or crash reporting SDK may initialise before the consent flow is complete. See NP-APP-TELEMETRY-001 Rev 2 for full requirements. The `engagement_tier` property (coarsened 3-bucket enum) replaces any raw session counter in all analytics events.

### 9.6 Requirements

*(Rev 6.)* Each row states a rule §9.2 or §9.3, or `NP-HFE-001` CT-06, already makes binding; the ID only makes it citable. A test names the row it verifies with `@verifies REQ-ONBOARD-NN` (`scripts/req-trace.ts`); where a row lists (a), (b), (c), a test claims a part with `@verifies REQ-ONBOARD-NN#b`. Every row binds the iOS and Android apps.

| ID | Requirement | What fails if it is not met | Traced to | Verification |
|---|---|---|---|---|
| **`REQ-ONBOARD-01`** | **The age declaration comes first and cannot be skipped.** (a) The consent flow presents "I confirm I am 16 years of age or older" before any personal data is collected or any consent is presented. (b) The checkbox is not pre-ticked. (c) The flow cannot proceed while it is unchecked. (d) No verification beyond the declaration is required | A pre-ticked box is a GDPR dark pattern; consent collected from a user who has not declared the age threshold is consent the app cannot rely on | §9.2; `NP-PRIV-001` Rev 2 MEDIUM-03 | Test |
| **`REQ-ONBOARD-02`** | **The biometric written release is its own screen and is shown to every user.** (a) It is a separate screen, not bundled with general consent. (b) It is shown to every user regardless of location, with no IP, locale or state detection | BIPA 740 ILCS 14/15(b)(1)–(3) requires a written release for biometric information. Detecting a user's state to decide whether to show it fails the user the detection misjudges, and the control also serves GDPR Art. 9 and WA MHMD | §9.3; `NP-PRIV-001` HIGH-01; OI-PA-03 (resolved) | Test; inspection that no location detection remains |
| **`REQ-ONBOARD-03`** | **The release states what it collects and what NeurOne will not do.** The copy gives the purpose, the retention, the destruction method, that NeurOne will not sell, lease or profit from brainwave data, and that it will not share it with third parties without separate consent except as required by law. Its text lives in the locale files | BIPA 15(b) requires the purpose and length of retention to be disclosed in writing; a release without them is not informed | §9.3; CLAUDE.md §17 | Test that every localization key exists; inspection of the copy |
| **`REQ-ONBOARD-04`** | **The onboarding order is age gate, biometric release, then research consent, as four distinct steps.** Research consent (L1 to L4) is not presented until the biometric release has been answered | The release is a precondition for collecting EEG; research consent presented before it asks the user to share data the app has not yet been authorised to collect | §9.2, §9.3; `NP-HFE-001` CT-06 | Test |
| **`REQ-ONBOARD-05`** | **Declining the biometric release disables what depends on EEG and nothing else.** (a) EEG neurofeedback and closed-loop adaptive stimulation are unavailable, including EEG-adaptive audio. (b) PBM, VNS, audio entrainment and visual stimulation that do not depend on EEG remain available. (c) A protocol that needs EEG is available once the release is accepted | Collecting EEG without the release is the harm BIPA addresses; disabling more than the EEG-dependent features punishes a user for declining | §9.3; CLAUDE.md §3 (modality roster) | Test |
| **`REQ-ONBOARD-06`** | **The release answer is recorded and is what the EEG gate reads.** (a) Accepting records the screen as shown and accepted; declining records it as shown and not accepted; the two callbacks are independent. (b) EEG consent is granted exactly when the release was accepted. (c) The protocol library's gate reads the persisted answer, so a relaunch does not reset it | A gate that reads anything other than the recorded answer enables EEG on a user who declined, or disables it on one who accepted | §9.3 | Test |

### 9.7 Requirements: HealthKit and reminder notifications

*(Rev 7.)* Each row states a rule §9.1, or `NP-PRIV-ANALYSIS-003` LOW-1 and LOW-2, already makes binding; the ID only makes it citable. A test names the row it verifies with `@verifies REQ-HEALTH-01` or `@verifies REQ-NOTIFY-01`; where a row lists (a), (b), a test claims a part with `#a`. Every row binds every app that offers the feature (the Swift app and the shared iOS host).

| ID | Requirement | What fails if it is not met | Traced to | Verification |
|---|---|---|---|---|
| **`REQ-HEALTH-01`** | **HealthKit is read only, only during an HRV biofeedback session, and leaves nothing behind.** (a) Permission is requested only when an HRV biofeedback session is running, never at launch or at consent. (b) The share (write) set is empty and nothing writes to HealthKit. (c) The values are held in memory, shown on the session screen, and cleared when the session ends or the screen goes; none is stored or sent. | A request at launch asks for health data with no reason on screen; a write or a stored copy turns a display into a health record the person did not agree to keep, which §9.1 forbids | §9.1; NP-PRIV-REM-001 STEP-16; iOS ISC-94 to ISC-96 | Test: the host starts the source only while a session with HRV data is running and clears it when that stops |
| **`REQ-NOTIFY-01`** | **A consumable reminder shows nothing on the lock screen that names the consumable or the device, and is asked for in context.** (a) The title is the app's name and the subtitle says only that action is needed; the consumable and its count are in the body, shown only once the phone is unlocked. A host with no such private body (the desktop balloon) shows the title and subtitle alone and never the consumable or its count. (b) Permission to notify is requested when the Consumables screen first appears, never at launch or before the consent flow completes. (c) A reminder for the same consumable replaces the one before it. | A lock screen that names the consumable tells anyone who sees it that the person owns the device and what for; a permission asked before the person can see why is declined or resented | `NP-PRIV-ANALYSIS-003` LOW-1 and LOW-2 | Test: the words handed to the host for an exceeded and a low consumable, and the permission request on the Consumables screen |

---

## 10. Updated Open Items

| ID | Item | Owner | Blocking |
|----|------|-------|---------|
| OI-WA-01 | SUPERSEDED (2026-07-13) — no continuous Core Haptics path exists on watchOS to characterise. Phase 3 is now a low-rate WKInterfaceDevice cue; the lightweight repeating-timer thermal cost is handled at runtime via `ProcessInfo.thermalState` auto-pause. | SW Engineering | Not blocking |
| OI-WA-02 | RETIRED (2026-10-21, the Watch app is retired) — Watch screen brightness characterisation at 40Hz ≥100 nits | SW Engineering | Phase 4 (visual flicker channel) |
| OI-WA-03 | RETIRED (2026-10-21, the Watch app is retired) — WatchConnectivity latency measurement on current watchOS release | SW Engineering | Phase 2 audio sync quality gate |
| OI-WA-04 | SUPERSEDED (2026-07-13) — `CHHapticEngine` does not exist on watchOS; there is no 40Hz Core Haptics frequency to verify on any Apple Watch model. (40Hz verification applies only to the iPhone Core Haptics path and the mastoid LRA pad, tracked elsewhere.) | SW Engineering | Not blocking |
| OI-WA-05 | RETIRED (2026-10-21, the Watch app is retired) — App Store review pre-submission check for the Phase 3 haptic cue — confirm the low-rate `WKInterfaceDevice.play` cue and its in-app copy carry no therapeutic/40Hz claim (rescoped from Core Haptics 40Hz continuous use) | SW/Regulatory | Phase 3 |
| OI-WA-06 | HealthKit permission review + privacy nutrition label sign-off before App Store submission | Privacy Lead | Phase 1 App Store submission |
| OI-PA-01 | Legal counsel confirms 16 as correct minimum age threshold for age gate | Legal Counsel | Age gate implementation |
| OI-PA-02 | Design and implement Authorised Guardian consent pathway for T2 minor patients | SW Engineering + Legal | T2 clinical launch |
| OI-PA-03 | RESOLVED — biometric release screen shown to ALL users (locale gate removed); BIPA counsel copy review is advisory, non-gating | Legal Counsel | Advisory (non-gating) |
| OI-PA-04 | Privacy Lead sign-off on plain-language adaptive trigger enum copy | Privacy Lead | Adaptive Adjustments card ship |

---

*All Watch functions are user interface and session monitoring aids. Therapeutic claims attach to NeurOne hardware only. See CLAUDE.md §3b for full Apple Watch sync app specification and §15 for marketing copy.*
