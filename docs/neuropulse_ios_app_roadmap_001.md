# NP-APP-ROADMAP-001 Rev A — iOS App Development Roadmap
**Project:** NeuroPulse  
**Document:** NP-APP-ROADMAP-001 Rev A  
**Date:** 2026-05-11  
**Status:** Baselined  
**Related issues:** GitHub Issue #32  

---

## 1. Scope

This document defines the development roadmap for the NeuroPulse iOS application, including all core session-management functionality and the Apple Watch companion sync app. The Watch app is a Year 1 deliverable, scheduled for development immediately after core iOS app launch.

Both iOS app and Watch app are software-only additions. BOM delta: $0.

---

## 2. Communication Architecture

```
NeuroPulse Hub (BT 5.3 LE, antennas in hub)
        │
        │  BLE GATT custom service
        │  Sub-50ms sync latency target
        ▼
iOS App (WatchConnectivity framework)
        │
        │  WatchConnectivity session
        ▼
watchOS App
        ├── Core Haptics (40Hz pattern)
        ├── Audio playback to AirPods (AVAudioSession)
        └── Watch display (SwiftUI)
```

BT 5.3 LE radio and antennas reside in the control hub only — not the headset. The iOS app communicates with the hub over a custom BLE GATT service. The Watch app communicates with the hub exclusively via the iPhone app using WatchConnectivity; it does not open a direct BLE connection to the hub.

---

## 3. Core iOS App — Required Before Watch App

The Watch app depends on the core iOS app being App Store-live. Core iOS app scope (not detailed in this document):

- Session protocol authoring, signing, and upload to hub (Mode 2 Programming)
- Real-time EEG/HRV/session status display (Mode 1 Connected)
- UHDR/SHDR partition management and nightly backup scheduling
- Clinical consent engine (§6 CLAUDE.md)
- Consumable inventory and reminder engine
- OTA firmware update orchestration
- User account, onboarding, and first-session setup

**Core iOS app target:** Month 12 post-company-formation (aligned with T1 hardware launch).

---

## 4. Apple Watch Sync App — Year 1 Post-Launch

### 4.1 Overview

The Watch app is a free companion delivering three sync channels. All Watch-delivered functions are declared as session monitoring and user interface aids — not therapeutic delivery. Therapeutic claims attach to NeuroPulse hardware only. App Store category: General Wellness.

| Channel | Function | watchOS API |
|---------|----------|-------------|
| Haptic | 40Hz continuous haptic pattern synchronised to hub session clock | Core Haptics (CHHapticEngine) |
| Audio | Binaural beats / isochronic tones / breathing pacer to AirPods | AVAudioSession, routed to paired AirPods |
| Visual | 40Hz flicker, EMDR L/R indicator, HRV breathing ring, session status | SwiftUI, WKInterfaceDevice display |

### 4.2 Development Phases

Development priority order is determined by implementation complexity and user utility, not by channel definition order above.

#### Phase 1 — Session Status + HRV Breathing Ring
**Priority:** Highest utility, lowest complexity  
**Target:** Month 1–2 post-core-app-launch

- Session timer (start, elapsed time, haptic end-of-session alert)
- Protocol name display on Watch face
- HRV biofeedback breathing ring: expanding/contracting SwiftUI animation synchronised to pacer phase from np_hrv_pacer (see NP-FW-HRV-001 Rev A §5)
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

#### Phase 3 — Haptic Sync (40Hz Core Haptics)
**Target:** Month 4–5 post-core-app-launch

- CHHapticEngine continuous event: `HapticContinuous` with sharpness 0.8, intensity 0.6 at 40Hz drive frequency
- Session start synchronisation: hub transmits session epoch; Watch app aligns CHHapticEngine start time using `CHHapticEngine.start(completionHandler:)` with a calculated future timestamp
- Duty cycle: 100% continuous for session duration (target 20 minutes); Apple Watch thermal performance characterisation required before production release — run 20-minute continuous Core Haptics session and record Watch surface temperature and battery drain
- Marketing caveat displayed in-app: "For the full 40Hz vibrotactile experience, use the NeuroPulse mastoid vibrotactile pad (available separately). The Apple Watch haptic is a complementary wrist layer — not a substitute for mastoid bone coupling." (See CLAUDE.md §15 for approved marketing copy.)

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
- Photoparoxysmal safety note in Watch app: "Visual flicker is disabled automatically if your NeuroPulse EEG detects a photoparoxysmal response. Watch app mirrors this safety cutoff within one WatchConnectivity message cycle."

---

## 5. BLE GATT Service Definition

Custom BLE GATT service UUID and characteristic layout (to be assigned at firmware implementation stage):

| Characteristic | Direction | Length | Content |
|---------------|-----------|--------|---------|
| SESSION_STATE | NOTIFY | 4 bytes | Session epoch (uint32, ms since Unix epoch) |
| SESSION_STATUS | NOTIFY | 2 bytes | Protocol ID (uint8) + status flags (uint8) |
| HRV_COHERENCE | NOTIFY | 4 bytes | Coherence score × 100 (uint16) + RMSSD ms (uint16) |
| PACER_PHASE | NOTIFY | 2 bytes | Phase (uint8: 0=inhale, 1=exhale) + elapsed % (uint8) |
| IMPEDANCE_RESULT | NOTIFY | 2 bytes | Pass/fail flags per electrode (uint16 bitmask) |
| CONSUMABLE_STATUS | READ/NOTIFY | 8 bytes | Per-consumable session counts (4× uint16) |

Notification interval: 100ms for SESSION_STATE and PACER_PHASE; 5s for HRV_COHERENCE; event-driven for IMPEDANCE_RESULT and CONSUMABLE_STATUS.

WatchConnectivity message from iPhone app to Watch app mirrors the GATT data in real time, reformatted as a WCSession `sendMessage` dictionary.

---

## 6. Regulatory Notes

All Watch-delivered functions must be described in App Store metadata and in-app UI as:

> "Session monitoring and user interface aids for use with a NeuroPulse wearable. The Apple Watch app does not deliver therapeutic stimulation. All therapeutic functions are provided exclusively by NeuroPulse hardware."

- App Store primary category: Health & Fitness (not Medical)
- No claim that Watch haptic, audio, or visual output produces therapeutic neurological effects
- Watch app version numbers tracked in SHDR (app_version field) for fleet analytics
- 40Hz visual flicker channel (Phase 4) carries the following in-app disclosure: "This feature uses flashing light. Do not enable if you have photosensitive epilepsy or have been advised to avoid flashing lights."

---

## 7. Open Items

| ID | Item | Owner | Blocking |
|----|------|-------|---------|
| OI-WA-01 | Apple Watch Series thermal characterisation for 20-min continuous Core Haptics | SW Engineering | Phase 3 production release |
| OI-WA-02 | Watch screen brightness characterisation at 40Hz ≥100 nits | SW Engineering | Phase 4 (visual flicker channel) |
| OI-WA-03 | WatchConnectivity latency measurement on current watchOS release | SW Engineering | Phase 2 audio sync quality gate |
| OI-WA-04 | CHHapticEngine 40Hz frequency verification on Apple Watch Ultra 2 vs Series 10 | SW Engineering | Phase 3 |
| OI-WA-05 | App Store review pre-submission check for Core Haptics 40Hz continuous use pattern | SW/Regulatory | Phase 3 |

---

## 8. Status Summary

| Phase | Channel | Complexity | Target | Status |
|-------|---------|------------|--------|--------|
| 1 | Session status + HRV breathing ring | Low | Month 1–2 post-core-app | Planned |
| 2 | Audio sync to AirPods | Medium | Month 3–4 post-core-app | Planned |
| 3 | Haptic sync (40Hz Core Haptics) | Medium | Month 4–5 post-core-app | Planned |
| 4 | 40Hz visual flicker | High | Month 6+ post-core-app | Blocked — OI-WA-02 |

---

*All Watch functions are user interface and session monitoring aids. Therapeutic claims attach to NeuroPulse hardware only. See CLAUDE.md §3b for full Apple Watch sync app specification and §15 for marketing copy.*
