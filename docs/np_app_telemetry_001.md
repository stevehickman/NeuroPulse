# App Analytics and Crash Reporting Policy

**Project:** NeurOne
**Document:** NP-APP-TELEMETRY-001
**Revision:** 7
**Date:** 2026-11-19
**Status:** ACTIVE
**Effective Date:** 2026-06-03
**Author:** Quality Lead (interim: Steve Hickman, CEO)
**Approved By:** Steve Hickman, CEO
**References:** NP-PRIV-REM-001 STEP-09; NP-APP-ROADMAP-001 Rev 1; FTC HBNR 16 CFR Part 318; GDPR Art. 7, 25; Apple App Store Review Guidelines §5.1; Apple Privacy Manifest requirements (Spring 2024)
**Related Issues:** —
**Gate:** —
**IEC 62304 Class:** —
**Applicable Standard:** FTC HBNR 16 CFR Part 318; GDPR Art. 7, 25; Apple App Store Review Guidelines §5.1
**Next Review:** Annual; also triggered by any analytics vendor change or new SDK addition
**Jurisdiction Scope:** US federal (FTC Act §5, FTC HBNR), US state (WA MHMD), EU/EEA GDPR, Apple/Google platform policies
**Change Summary:** **Rev 5 (2026-11-19):** new §4.1 records the principal's decision on the network address (`OI-PRIV-05` (5)): analytics events are to go through an Oblivious HTTP relay, as SHDR does, so that the analytics host never receives the address; until that is built the floor is that NeurOne's edge writes the address nowhere and hands it to nothing. The relay is decided and not built. The floor's log filter is new in this revision; the header removal already existed. **Rev 5 (2026-11-16):** §3 closes three ways the permitted list still let a holder of the events rebuild a person's timeline (`OI-PRIV-05`, privacy review of `common/` Findings 5, 18 and 19). (1) Event names are a literal list (new §3.3); a name was permitted if it was short snake_case with no §7 term, which passes any name nobody thought of. (2) `screen_name` loses the three session screens and the two consent screens (§3.1). (3) `error_code` is a literal list of app failures (new §3.4); it was any 1 to 4 digits, which admitted a helmet fault slot. `engagement_tier` is ruled to stay (§3.2). §7's terms are applied to the list, by a test. Whether events go through a relay that hides the network address is asked of the principal and not decided (`docs/human-input/oi-priv-05-analytics-relay.md`). No event is emitted by the app yet. **Rev 4 (2026-10-18):** §5's withdrawal rule now says how the deletion is made to happen on the phone (privacy review Finding 3, `REQ-SW03-19`): the app reads the pseudonymous analytics ID before the SDK resets it and keeps it in a durable queue until the erasure relay has accepted or permanently refused it. The 30 days and the vendor deletion call are unchanged. The relay is not built (`docs/np_analytics_001.md` §7.6, `OI-ANALYTICS-01`). **Rev 3 (2026-10-18):** §3 is now enforced in code as an allowlist (privacy review Finding 5, `REQ-SW03-07`). The apps checked an event against a list of twelve keys to forbid, which passes any key nobody thought of, so the permitted list in §3 was a statement the code did not keep. The shared core (`common/npps-core/src/analytics.rs`, data in `common/npps/analytics-properties.json`) now drops any event whose name is not lowercase snake_case without a §7 term, or that carries a property §3 does not name, or a value not of its property's form. The text of §3 and §7 is unchanged. **Rev 2:** `session_sequence` (unsigned integer) replaced with `engagement_tier` (coarsened 3-bucket enum) per NP-PRIV-001 Rev 2 finding LOW-03. Rationale: raw session count is health-adjacent behavioural data under Washington MHMD and GDPR Art. 9 by inference. §3.2 added with implementation requirements. NP-PRIV-REM-001 Rev 2 STEP-33 cross-reference added.

---

## 1. Purpose and Scope

This document governs all analytics event tracking and crash/error reporting in the NeurOne iOS and Android applications. It applies to:
- All versions of the NeurOne iOS app (life.neurone.app)
- All versions of the NeurOne Android app (life.neurone.app)
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

**This list is enforced in code, and the code is the shared core's, once** (Rev 3): `common/npps/analytics-properties.json` states each permitted event name (§3.3), each property and the form of its value, and `checkAnalyticsEvent` in `common/npps-core/src/analytics.rs` drops the whole event if its name is not one of the listed names, exactly, if a property is not named here, or if a value is not of its form. Both apps ask that one rule. A name, a property or a value is added by changing this section and that file together, after the Privacy Lead's review; the bounds the table leaves open (the digit counts, the platform prefixes of `os_version`) are `docs/status/unjustified-choices.md` UC-094.

**Every part of an event is a listed literal, apart from the numbers of a version and of an onboarding step.** A rule that says what a name or a value must not contain passes whatever nobody thought of, so there is none at run time.

| Property | Description | Format |
|---|---|---|
| `app_version` | NeurOne app version string | Semver string (e.g. "1.0.3") |
| `os_version` | Operating system version | Major.minor (e.g. "iOS 18.2") — not patch version |
| `device_class` | Coarsened device model | Enum: "iPhone_flagship", "iPhone_mid", "iPhone_legacy", "iPad", "Android_flagship", "Android_mid", "Android_budget" — not the specific model |
| `screen_name` | Current app screen | Coarsened enum — see §3.1 |
| `engagement_tier` | Coarsened app-launch engagement bucket — replaces raw session counter (see §3.2) | Enum: `"new"` (1–5 app launches), `"active"` (6–50 app launches), `"established"` (51+ app launches). Computed on-device from local counter. Counts app launches, not stimulation sessions. Resets on uninstall. Never transmitted as a raw integer. |
| `app_start_type` | How the app was launched | Enum: "cold_start", "warm_start", "background_resume" |
| `onboarding_step` | Which onboarding step was reached or completed | Enum: "consent_flow_step_1" through "consent_flow_complete" — no health context in name |
| `error_code` | Non-health error code | One of the codes of §3.4 — no free text, and never a helmet fault code |
| `connectivity_type` | Network connectivity when performing an action | Enum: "wifi", "cellular", "none" |

### 3.1 Permitted screen name values

Screen names must be coarsened to remove health context. The following mapping is the whole list (`screen_name` in `common/npps/analytics-properties.json`):

| Actual screen | Permitted `screen_name` value |
|---|---|
| Protocol library | "protocol_library" |
| Any protocol detail screen | "protocol_detail" — never include protocol name |
| Profile / account settings | "account_settings" |
| Device pairing / firmware update | "device_management" |
| Session setup, session running, session results | **None.** No screen event is sent from these screens |
| Research consent flow, clinical access consent flow | **None.** No screen event is sent from these screens |

**Why the session and consent screens have no value** (`OI-PRIV-05` (2)). Events are held under the installation's stable pseudonymous ID. Under that ID, "session_setup", "session_active" and "session_results" were a timestamped record of each stimulation session and its length, which §4 prohibits as a session parameter and a precise timestamp; "clinical_consent" recorded that the person has a clinician, and "research_consent" that they were deciding on a study. One neutral value for the three session screens would still mark when a session happened, so they are dropped, not collapsed. A screen added later that only a session, a consent or a clinician relationship reaches gets no value either.

![The app's screens after `OI-PRIV-05`: four send a screen name to the analytics host, and the session and consent screens send nothing](diagrams/np_app_telemetry_screen_names.svg)

*Figure: the app's screens and the analytics host. The app flow is unchanged by this rule; only which screens send a name is. Which screen each name attaches to follows the table above, and is not wired in code while no app emits an event.*

**Prohibited screen name values** (examples — this list is not exhaustive):
- Any screen name containing a modality name (tDCS, PBM, VNS, EEG, tACS, TMS)
- Any screen name containing a health condition (depression, anxiety, PTSD, Parkinson's, cognitive)
- Any screen name containing a protocol name (alpha_train, tms_depression, ad_prevention)

### 3.2 engagement_tier implementation note

The `engagement_tier` property replaces the raw `session_sequence` counter removed in NP-PRIV-001 Rev 2 (2026-06-02). Rationale: a raw session count (e.g. `session_sequence = 450`) tells an analytics vendor that a user has launched a neuromodulation app 450 times — health-adjacent behavioural data under Washington MHMD RCW 70.372.010 and inferrable health data under GDPR Art. 9. The coarsened bucket delivers the same product analytics signal (new user retention, habit formation, established user cohort sizing) without exposing usage intensity.

Implementation requirements:
- The local counter is stored in `UserDefaults` (iOS) / `SharedPreferences` (Android) under the key `NP_APP_LAUNCH_COUNT`
- The counter increments on every cold start **after** the consent flow is complete. Pre-consent launches are not counted.
- The counter is **never** transmitted to the analytics vendor as a raw integer — only the derived tier enum is sent
- The tier is computed at the time the analytics event fires: `new` = count 1–5, `active` = 6–50, `established` = 51+
- On uninstall and reinstall, the counter resets to 0 (acceptable — tier computation is stateless from the analytics vendor's perspective)
- **Ruled to stay (`OI-PRIV-05` (4), 2026-11-16).** The tier is derived from a count of app launches, not of sessions, and it tells the holder of the events nothing the events do not: every event already arrives under the installation's ID, so the holder can count launches itself, more finely than three buckets. Removing the tier would remove nothing the holder has. With the session screens gone (§3.1) no event separates a launch that led to a session from one that did not. What a launch history under a stable ID shows is a question about the ID and the network address, not about this property (`OI-PRIV-05` (5)). The ruling is reopened if the ID stops being stable across launches, because the tier would then be the only thing joining them.
- The analytics vendor must not be able to infer the raw count from the tier sequence over time. If the vendor's SDK supports it, disable any automatic session-count tracking in the vendor SDK configuration.

### 3.3 Permitted event names

An event is sent only if its name is one of these, exactly (`events` in `common/npps/analytics-properties.json`; `OI-PRIV-05` (1)):

| Event name | When |
|---|---|
| `app_opened` | The app comes to the foreground |
| `onboarding_step_completed` | An onboarding step is completed (`onboarding_step`) |
| `screen_viewed` | One of the four screens of §3.1 is shown (`screen_name`) |
| `feature_used` | A non-health feature is used. No property names the feature; one needs a new permitted property first |
| `error_reported` | One of the app failures of §3.4 happened (`error_code`) |
| `app_crashed` | The next launch after a crash. No stack trace |

No event is named for a session, a stimulation, a consent or a clinician: there is no `consent_granted` and no `consent_revoked` (an event sent only by an installation that has completed the consent flow already says so, and the gate is closed at a withdrawal, so neither could add anything but a timestamp). A name carries no digit, so it cannot carry a value. The run-time check is the list and nothing else; the terms of §7 are checked against the list by a test.

### 3.4 Permitted error codes

`error_code` is one of these, exactly (`error_code` and `errorCodeMeanings` in `common/npps/analytics-properties.json`; `OI-PRIV-05` (3)):

| Code | Meaning |
|---|---|
| 1001 | A network request failed or timed out |
| 1002 | Cloud sign-in failed |
| 1003 | A cloud storage request failed |
| 1101 | Firmware update: the helmet was not connected |
| 1102 | Firmware update: the image could not be loaded |
| 1103 | Firmware update: the image's signature was invalid |
| 1104 | Firmware update: the image was of the wrong kind |
| 1105 | Firmware update: the transfer failed |
| 1106 | Firmware update: the hub rejected the image (the hub's own code is not carried) |
| 1107 | Firmware update: timed out |
| 1108 | Firmware update: verification failed |
| 1201 | The shared core could not be loaded or asked |

**A code is listed only for a failure of the app itself.** Nothing raised during or about a session, a consent, an electrode contact or a helmet fault has a code, and none may be added. A helmet fault can be a physiological halt: the cervical VNS cardiac interlock and the photoparoxysmal halt are safety MCU fault slots, and an event carrying such a slot would tell the holder of the events that this person's heart rate stepped or that their EEG showed a photoparoxysmal response (`CLAUDE.md` §5.1 rule 2). The earlier form, any 1 to 4 digits, admitted every such slot, because a slot is one byte. **Every listed code is above 255**, so no one-byte fault slot or module fault code is a permitted value whatever a caller passes. Leaving a code out for one kind of fault only would itself show which kind happened, so no helmet fault has a code at all. There was no "NeurOne error enum" in the repository before this list; these codes are the app's own failure kinds (`OtaError`, the cloud sign-in and storage errors, the core binding) and are not derived from the firmware's.

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

### 4.1 The network address: a relay, and a floor until it is built

**Decided by the principal, 2026-11-19 (`OI-PRIV-05` (5), GitHub #903; the options are in `docs/human-input/oi-priv-05-analytics-relay.md`).**

An event is held under the installation's stable pseudonymous ID, and a connection shows the server the sender's network address. An address beside that ID places the installation on a household, a clinic or an employer network. Two things follow, and they are not the same strength:

1. **The protection: analytics events go through an Oblivious HTTP relay, as the SHDR upload does** (RFC 9458; `common/npps-core/src/ohttp.rs`, `NP-INFRA-001` §3.3b). The app seals each event to a gateway's key and sends it to a relay run by another party: the relay sees the address and a ciphertext, the analytics host sees the event and never the address. **This is decided and not built.** It protects only while the relay and the gateway are run by different parties and the relay keeps no log, which no code can check. **Analytics may share the fleet path's relay operator and gateway** (the principal, 2026-11-20). Sharing the infrastructure does not join the two consent subjects (`CLAUDE.md` §6.0): the two gates stay independent in the app, and the design of the shared path has to show that the gateway cannot pair a device token with an analytics ID, by timing or otherwise. That showing is design work still to do, not something the decision settles. **No analytics event ships before the relay is built** (the principal, 2026-11-20): the floor below is not a basis for sending events. Nothing calls the event-tracking function today, and the first call waits for the relay.
2. **The floor: NeurOne's edge writes the address nowhere and hands it to nothing.** It is a backstop for anything that reaches the host before the relay exists (a test build, the erasure request, a mistake), not permission to send events. The proxy in front of the analytics host removes `X-Forwarded-For` and `X-Real-IP` before the request reaches PostHog, and neither of its logs (the access log and the default logger, which carries error entries) keeps the client's address, its port or those headers (`infra/posthog/caddy/Caddyfile`; `ci/test_posthog_infra.py` P6, P8, P9). The project setting that masks addresses (`docs/np_analytics_001.md` §3.4) is a second layer and is unverified on a real instance (`OI-ANALYTICS-02`).

**The floor is a promise about NeurOne's own server, not a property the person can rely on:** the address still reaches the edge, and a misconfiguration, a breach of the edge or a legal demand served on NeurOne reaches it. It is not to be described to a user as the address being hidden. The floor is not removed when the relay is built (`CLAUDE.md` §23: protection only goes up).

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

**Research consent withdrawal:** If a user withdraws blanket research consent (L3), the research analytics SDK must be de-initialised and all locally cached events must be deleted immediately. The user's pseudonymous analytics identifier must be deleted from the vendor's servers within 30 days (enforce via vendor deletion API call at withdrawal time). **The identifier is read before the SDK resets it** (the reset makes a new one, so the old one would otherwise be lost) **and kept in a durable on-device queue, which asks the erasure relay at withdrawal and at every launch until the relay has accepted it; a refusal never drops it, and a device erase asks the relay before it wipes the queue** (`REQ-SW03-19`). The relay holds the vendor's private key, which is never in the app, and is specified in `docs/np_analytics_001.md` §7.6. Partial withdrawals (specific study or category) do not revoke research analytics. Warranty consent withdrawal (SHDR fleet telemetry) is independent and handled by `WarrantyAnalyticsGate`.

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
4. Document this verification with a screenshot of the vendor's crash report UI in NP-APP-ROADMAP-001 Rev 2 (OI-TEL-01).

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

**How the terms are applied in code** (Rev 5). Every event name and every property value, apart from version and step numbers, is a literal in `common/npps/analytics-properties.json` (§3), so the terms are checked against that file: a test in `common/npps-core/src/analytics.rs` fails if a listed name or literal contains one. They are no longer the run-time test of an event name, because a name is now permitted only by being listed. The lint rule above, on call sites, is still not written.

---

## 8. Annual Review

This document must be reviewed annually (first review: June 2027) and updated if:
- A new analytics or crash reporting vendor is added or changed
- A new SDK with network access is added to the app
- Apple or Google introduces new privacy manifest or data safety requirements
- A regulatory change affects the health app analytics obligations (FTC guidance, state law)
- The annual app telemetry audit (NP-PRIV-AUDIT-001) identifies a gap

Review is conducted by: Privacy Lead (interim: CEO) + iOS/Android engineering leads. Output: NP-APP-TELEMETRY-001 Rev 2 (or higher) with change log.

## Revision history

Revisions before the first row below were not recorded in a table in this document.

| Rev | Date | Author | Description |
|---|---|---|---|
| 5 | 2026-11-21 | NeurOne Software Engineering | **§5: the erasure queue keeps the identifier until the relay has accepted it (a refusal never drops it), and a device erase asks the relay before it wipes the queue (`REQ-SW03-19` (b), (d)).** |
| 6 | 2026-11-16 | NeurOne Software Engineering | **§3: event names are a literal list (new §3.3); `screen_name` loses the session and consent screens (§3.1); `error_code` is a literal list of app failures, each above 255, never a helmet fault code (new §3.4); `engagement_tier` is ruled to stay (§3.2); §7's terms are checked against the list by a test (`OI-PRIV-05`).** |
| 7 | 2026-11-19 | NeurOne Software Engineering | **New §4.1: analytics events are to go through an Oblivious HTTP relay (decided, not built); until then the edge hands the address to nothing and writes it in no log (`OI-PRIV-05` (5), the principal, 2026-11-19); analytics may share the fleet path's relay and gateway, and no event ships before the relay is built (the principal, 2026-11-20).** |
