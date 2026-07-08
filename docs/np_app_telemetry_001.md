# App Analytics and Crash Reporting Policy

**Project:** NeurOne
**Document:** NP-APP-TELEMETRY-001
**Revision:** B
**Date:** 2026-06-03
**Status:** ACTIVE
**Effective Date:** 2026-06-03
**Author:** Quality Lead (interim: Steve Hickman, CEO)
**Approved By:** Steve Hickman, CEO
**References:** NP-PRIV-REM-001 STEP-09; NP-APP-ROADMAP-001 Rev A; FTC HBNR 16 CFR Part 318; GDPR Art. 7, 25; Apple App Store Review Guidelines §5.1; Apple Privacy Manifest requirements (Spring 2024)
**Related Issues:** —
**Gate:** —
**IEC 62304 Class:** —
**Applicable Standard:** FTC HBNR 16 CFR Part 318; GDPR Art. 7, 25; Apple App Store Review Guidelines §5.1
**Next Review:** Annual; also triggered by any analytics vendor change or new SDK addition
**Jurisdiction Scope:** US federal (FTC Act §5, FTC HBNR), US state (WA MHMD), EU/EEA GDPR, Apple/Google platform policies
**Change Summary:** `session_sequence` (unsigned integer) replaced with `engagement_tier` (coarsened 3-bucket enum) per NP-PRIV-001 Rev B finding LOW-03. Rationale: raw session count is health-adjacent behavioural data under Washington MHMD and GDPR Art. 9 by inference. §3.2 added with implementation requirements. NP-PRIV-REM-001 Rev B STEP-33 cross-reference added.

---

## 1. Purpose and Scope

This document governs all analytics event tracking and crash/error reporting in the NeurOne iOS and Android applications. It applies to:
- All versions of the NeurOne iOS app (com.neurone.lifes)
- All versions of the NeurOne Android app (com.neurone.android)
- All third-party SDKs included in either app that have network access or access to device identifiers

This policy exists because:
1. Analytics event names and properties can reconstruct health-management behaviour (which modalities a user is using, which protocols, whether a user is managing depression or PTSD) even without a name or email.
2. Crash reporters by default capture stack-local variables, which in a health app can contain session parameters, biometric readings, or consent state.
3. FTC enforcement actions against GoodRx (2023), BetterHelp (2023), and Premom (2023) established that sharing health-adjacent analytics with advertising or analytics vendors constitutes an unfair practice under FTC Act §5 and triggers FTC HBNR obligations — regardless of whether the data is HIPAA-covered.

---

## 2. Approved Vendors

At the time of this policy's adoption, no analytics or crash reporting vendor has been selected. The following requirements must be met before any vendor is integrated into the app codebase:

### 2.1 Analytics vendor (maximum one)

**Requirements:**
- Must provide a signed Data Processing Agreement (DPA) meeting GDPR Art. 28 requirements.
- Must provide a Business Associate Agreement (BAA) if any session-derived data (even pseudonymous) is processed.
- Must not use NeurOne event data to build user profiles for any purpose other than providing analytics services to NeurOne.
- Must not sell or share NeurOne event data with any third party.
- Must support data deletion requests (user exercises right to erasure → delete all analytics records for that user's pseudonymous identifier within 30 days).
- Must support server-side data storage in the US (for US users) and EU (for EU users) — no residency in countries without an adequacy decision or DPF/SCCs coverage.
- SDK must not initialise before the NeurOne app consent flow is complete (confirmed by SDK configuration documentation).

**Candidate evaluation criteria (to be completed at vendor selection):**

| Criterion | Weight | Notes |
|---|---|---|
| DPA quality (GDPR Art. 28 compliance) | High | Must cover sub-processors |
| BAA availability | High | Required |
| Event data isolation (no cross-customer profiling) | High | Contractual prohibition required |
| EU data residency option | Medium | Required for EU launch |
| Privacy-first architecture (no persistent device identifiers) | High | Must not use IDFA or Google Ad ID |
| SDK initialisation controls | High | Must support deferred init post-consent |

**Selected vendor (to be completed):** [TBD]  
**DPA executed:** [Date]  
**BAA executed:** [Date]

### 2.2 Crash and error reporter (maximum one)

**Requirements:**
- Must provide a DPA.
- Must support **redacted payload mode**: crash reports must not include request bodies, response bodies, or local variable values. Stack frames and thread state are permitted; variable values are not.
- Must not transmit crash reports to any analytics or advertising partner.
- Must support crash report deletion on user request.
- Must have a documented process for handling crash reports that contain accidental PII (e.g. a user pastes an error message containing their email into a form that then crashes).

**Selected vendor (to be completed):** [TBD]  
**DPA executed:** [Date]  
**Redacted payload mode confirmed:** [Date and method of confirmation]

---

## 3. Permitted Event Properties

Analytics events may include only the following properties. Any event that requires a property not on this list must be reviewed and approved by the Privacy Lead before implementation.

| Property | Description | Format |
|---|---|---|
| `app_version` | NeurOne app version string | Semver string (e.g. "1.0.3") |
| `os_version` | Operating system version | Major.minor (e.g. "iOS 18.2") — not patch version |
| `device_class` | Coarsened device model | Enum: "iPhone_flagship", "iPhone_mid", "iPhone_legacy", "iPad", "Android_flagship", "Android_mid", "Android_budget" — not the specific model |
| `screen_name` | Current app screen | Coarsened enum — see §3.1 |
| `engagement_tier` | Coarsened app-launch engagement bucket — replaces raw session counter (see §3.2) | Enum: `"new"` (1–5 app launches), `"active"` (6–50 app launches), `"established"` (51+ app launches). Computed on-device from local counter. Counts app launches, not stimulation sessions. Resets on uninstall. Never transmitted as a raw integer. |
| `app_start_type` | How the app was launched | Enum: "cold_start", "warm_start", "background_resume" |
| `onboarding_step` | Which onboarding step was reached or completed | Enum: "consent_flow_step_1" through "consent_flow_complete" — no health context in name |
| `error_code` | Non-health error code | Integer code from NeurOne error enum — no free text |
| `connectivity_type` | Network connectivity when performing an action | Enum: "wifi", "cellular", "none" |

### 3.1 Permitted screen name values

Screen names must be coarsened to remove health context. The following mapping is enforced in the app event tracking layer:

### 3.2 engagement_tier implementation note

The `engagement_tier` property replaces the raw `session_sequence` counter removed in NP-PRIV-001 Rev B (2026-06-02). Rationale: a raw session count (e.g. `session_sequence = 450`) tells an analytics vendor that a user has launched a neuromodulation app 450 times — health-adjacent behavioural data under Washington MHMD RCW 70.372.010 and inferrable health data under GDPR Art. 9. The coarsened bucket delivers the same product analytics signal (new user retention, habit formation, established user cohort sizing) without exposing usage intensity.

Implementation requirements:
- The local counter is stored in `UserDefaults` (iOS) / `SharedPreferences` (Android) under the key `NP_APP_LAUNCH_COUNT`
- The counter increments on every cold start **after** the consent flow is complete. Pre-consent launches are not counted.
- The counter is **never** transmitted to the analytics vendor as a raw integer — only the derived tier enum is sent
- The tier is computed at the time the analytics event fires: `new` = count 1–5, `active` = 6–50, `established` = 51+
- On uninstall and reinstall, the counter resets to 0 (acceptable — tier computation is stateless from the analytics vendor's perspective)
- The analytics vendor must not be able to infer the raw count from the tier sequence over time. If the vendor's SDK supports it, disable any automatic session-count tracking in the vendor SDK configuration.

| Actual screen | Permitted `screen_name` value |
|---|---|
| Session setup screen | "session_setup" |
| Session running screen | "session_active" |
| Session results screen | "session_results" |
| Protocol library | "protocol_library" |
| Any protocol detail screen | "protocol_detail" — never include protocol name |
| Research consent flow | "research_consent" |
| Clinical access consent flow | "clinical_consent" |
| Profile / account settings | "account_settings" |
| Device pairing / firmware update | "device_management" |

**Prohibited screen name values** (examples — this list is not exhaustive):
- Any screen name containing a modality name (tDCS, PBM, VNS, EEG, tACS, TMS)
- Any screen name containing a health condition (depression, anxiety, PTSD, Parkinson's, cognitive)
- Any screen name containing a protocol name (alpha_train, tms_depression, ad_prevention)

---

## 4. Prohibited Event Properties

The following properties must **never** appear in any analytics event, regardless of vendor or anonymisation claimed:

| Prohibited property type | Examples |
|---|---|
| User identity | Email address, name, phone number, device serial number, UHDR token |
| Health-inferrable strings | Modality names (tDCS, PBM, VNS, EEG, TMS), protocol names (ad_prevention, tms_depression, anxiety_hrv), health condition names |
| Session parameters | Session duration (reveals usage intensity), frequency settings, current amplitude, zone configuration |
| Biometric or physiological values | HRV score, EEG band values, coherence score, impedance values |
| Precise timestamps | Any timestamp that could be used to infer when a user was in a specific health management activity. Timestamps may only be included as date-only (YYYY-MM-DD) for daily active user metrics. |
| Device advertising identifiers | IDFA, Google Advertising ID, IDFV used as a persistent identifier across app reinstalls |
| IP address | Vendor must not log client IP address associated with any event |

---

## 5. SDK Initialisation Gate

**Binding requirement:** No analytics or crash reporting SDK may initialise, make network calls, or write to local storage before the user has completed the NeurOne consent flow on first launch.

**Implementation:**

```swift
// iOS — enforced in AppDelegate / SceneDelegate
func application(_ application: UIApplication, 
                 didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?) -> Bool {
    // DO NOT initialise analytics or crash reporter here
    // Analytics and crash reporter are initialised only in ConsentFlowViewController.consentCompleted()
    // OR on subsequent launches if NeurOneConsentStore.hasCompletedConsent() returns true
    return true
}
```

```swift
// Correct initialisation point
func consentCompleted() {
    guard NeurOneConsentStore.hasCompletedConsent() else { return }
    AnalyticsVendor.initialise(config: NeurOneAnalyticsConfig())
    CrashReporter.initialise(config: NeurOneCrashConfig())
}
```

**Research consent withdrawal:** If a user withdraws blanket research consent (L3), the research analytics SDK must be de-initialised and all locally cached events must be deleted immediately. The user's pseudonymous analytics identifier must be deleted from the vendor's servers within 30 days (enforce via vendor deletion API call at withdrawal time). Partial withdrawals (specific study or category) do not revoke research analytics. Warranty consent withdrawal (SHDR fleet telemetry) is independent and handled by `WarrantyAnalyticsGate`.

**Apple PrivacyInfo.xcprivacy requirement:** The PrivacyInfo.xcprivacy manifest must accurately declare all APIs used by the analytics and crash reporting SDKs, including any use of `NSPrivacyAccessedAPICategoryUserDefaults`, `NSPrivacyAccessedAPICategoryFileTimestamp`, `NSPrivacyAccessedAPICategorySystemBootTime`, or `NSPrivacyAccessedAPICategoryDiskSpace`. These declarations must be completed at vendor selection.

---

## 6. Crash Reporter Configuration

The crash reporter must be configured with the following settings before the first production build:

| Setting | Required value | Rationale |
|---|---|---|
| Request body capture | DISABLED | Session data, EEG state, or consent state may be in request bodies |
| Response body capture | DISABLED | Clinical API responses may contain FHIR resources with health data |
| Local variable capture | DISABLED | Stack-local variables in the session runner or consent engine may contain health data |
| User identifier attachment | DISABLED | No user identity should appear in crash reports |
| Network request logging | URLs only, no headers, no bodies | Origin URL of a failed request is useful for debugging; header and body content are not |
| Screenshot capture on crash | DISABLED | A screenshot of a crash may show the session screen with health context visible |

**Verification procedure:** Before the first TestFlight or Play Store internal test release, the engineering lead must:
1. Intentionally trigger a crash in the app on a device running a debug session.
2. Confirm the crash report received by the vendor contains: stack trace, OS version, app version, device class.
3. Confirm the crash report does not contain: any text from the session screen, any network request body, any variable value, any screenshot, any user identifier.
4. Document this verification with a screenshot of the vendor's crash report UI in NP-APP-ROADMAP-001 Rev B (OI-TEL-01).

---

## 7. Linting Rule: Prohibited Event Properties

The following custom linting rule must be added to the SwiftLint / Android Lint configuration before any analytics events are implemented:

**Pattern to flag:** Any call to the analytics event tracking function where the event name or any property value contains any of the following strings (case-insensitive):

```
tDCS, tACS, TMS, EEG, VNS, PBM, rTMS, TBS, EMDR, binaural,
depression, anxiety, PTSD, TBI, Parkinson, Alzheimer, dementia,
sleep, seizure, cognitive, focus, calm, theta, alpha, gamma,
1064nm, 808nm, 660nm, cortical, neural, neuro, brain,
hrv, eeg, coherence, impedance, session_type, protocol_name,
email, name, token, uid, device_id, serial
```

Any flagged call must be reviewed by the Privacy Lead before it is merged. This rule does not replace human review but catches the most common accidental violations.

---

## 8. Annual Review

This document must be reviewed annually (first review: June 2027) and updated if:
- A new analytics or crash reporting vendor is added or changed
- A new SDK with network access is added to the app
- Apple or Google introduces new privacy manifest or data safety requirements
- A regulatory change affects the health app analytics obligations (FTC guidance, state law)
- The annual app telemetry audit (NP-PRIV-AUDIT-001) identifies a gap

Review is conducted by: Privacy Lead (interim: CEO) + iOS/Android engineering leads. Output: NP-APP-TELEMETRY-001 Rev B (or higher) with change log.
