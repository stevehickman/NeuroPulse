# NeuroPulse — App Store Privacy Nutrition Label

**Document:** App Privacy declaration for App Store Connect manual entry
**Satisfies:** ISA ISC-6, ISC-132
**Cross-reference:** `NP-APP-TELEMETRY-001 Rev B`, `PrivacyInfo.xcprivacy`, CLAUDE.md §5 (UHDR/SHDR)
**Tracking:** This app does **not** track users. `NSPrivacyTracking` is `false`; App Tracking Transparency prompt is never shown.

This document is the source of truth for the "App Privacy" section in App Store Connect.
Enter each row below exactly as stated. No placeholder entries may remain (ISC-6).

---

## Summary table

| Data Type | Collected? | Linked to Identity? | Used for Tracking? | Purpose |
|-----------|-----------|---------------------|--------------------|---------|
| Device ID | Yes | Yes (warranty owner linkage — SHDR opaque token only) | No | App functionality |
| Crash Data | Yes (post-consent only) | Yes | No | App functionality — crash diagnostics |
| Health & Fitness — HRV | Yes | No | No | App functionality — real-time session display only, session-scoped |
| Email Address | Optional (research contact only, user-provided) | No | No | App functionality — optional research study invitations |
| Product Interaction | Yes (post-consent only) | No | No | Analytics — anonymous in-app events (PostHog) |
| Other Usage Data | Yes (post-consent only) | No | No | Analytics — anonymous device/OS/app version (PostHog) |
| Name | No | — | — | Not collected |

---

## Per-type detail (for App Store Connect data-type screens)

### Device ID — **Collected**
- **App Store Connect category:** Identifiers → Device ID
- **Linked to the user's identity:** Yes. The device is associated with a warranty
  owner record. In the device's own telemetry (SHDR) the linkage is an opaque
  256-bit TRNG warranty token, never the user's name or biology (CLAUDE.md §5.1).
- **Used for tracking:** No.
- **Purposes:** App Functionality (warranty registration, support, predictive
  maintenance prompts).

### Crash Data — **Collected (only after consent)**
- **App Store Connect category:** Diagnostics → Crash Data
- **Collection gate:** The crash-reporting SDK is initialised only after the user
  completes the consent flow and opts in (NP-APP-TELEMETRY-001 Rev B). No crash
  data leaves the device before consent.
- **Linked to the user's identity:** Yes.
- **Used for tracking:** No.
- **Purposes:** App Functionality (diagnostics, stability).

### Health & Fitness (HRV) — **Collected, not linked, on-device only**
- **App Store Connect category:** Health & Fitness → Health
- **What:** Heart-rate-variability values read during HRV biofeedback sessions to
  drive the live coherence display and breathing pacer.
- **Linked to the user's identity:** No.
- **Used for tracking:** No.
- **Shared with third parties:** No.
- **Off-device transmission:** None. HRV is processed in memory for the live
  session display and is never stored or transmitted to NeuroPulse servers
  (Info.plist `NSHealthShareUsageDescription`). User health records (UHDR) remain
  encrypted on device under a key NeuroPulse cannot hold.
- **Purposes:** App Functionality (real-time session display only).

### Email Address — **Optional, user-provided**
- **App Store Connect category:** Contact Info → Email Address
- **What:** Captured only if the user voluntarily provides it to receive research
  study invitations (Telos research-contact consent, L1).
- **Linked to the user's identity:** No (stored against research-contact
  preference, not used to identify the user across apps/sites).
- **Used for tracking:** No.
- **Purposes:** App Functionality (optional research study invitations).
- **Note:** If App Store Connect requires a strict yes/no for "collected," declare
  Email Address as **collected** with the Optional note above, since the app can
  receive it.

### Product Interaction — **Collected (only after consent), not linked, anonymous**
- **App Store Connect category:** Usage Data → Product Interaction
- **What:** Anonymous in-app events (app opens, session starts, screen names) sent
  via PostHog after the user opts in to analytics.
- **Collection gate:** PostHog is initialised only after the consent flow completes
  and the user opts in (NP-APP-TELEMETRY-001 Rev B). No events leave the device
  before consent.
- **Linked to the user's identity:** No. `personProfiles = .never` — PostHog builds
  no Person record.
- **Used for tracking:** No.
- **Purposes:** Analytics.

### Other Usage Data — **Collected (only after consent), not linked, anonymous**
- **App Store Connect category:** Usage Data → Other Usage Data
- **What:** Device model, OS version, and app version auto-attached to PostHog
  events as `$device_model` / `$os_version` / `$app_version`.
- **Collection gate:** Same analytics opt-in gate as Product Interaction.
- **Linked to the user's identity:** No.
- **Used for tracking:** No.
- **Purposes:** Analytics.

### Name — **Not collected**
- The app does not collect the user's name. Do not add a Name entry in App Store
  Connect.

---

## Required-Reason API note (informational)

The `PrivacyInfo.xcprivacy` manifest declares the Required-Reason APIs used:
- `NSPrivacyAccessedAPICategoryUserDefaults` — reason `CA92.1`
- `NSPrivacyAccessedAPICategoryFileTimestamp` — reason `C617.1`
- `NSPrivacyAccessedAPICategoryDiskSpace` — reason `E174.1`

CoreBluetooth (`CBCentralManager`) is disclosed via Info.plist
`NSBluetoothAlwaysUsageDescription` / `NSBluetoothPeripheralUsageDescription`.
It does not collect personal data and is not declared as a collected data type.
