# NP-PRIV-AUDIT-001 Rev A — NeuroPulse App Privacy Audit

**Document number:** NP-PRIV-AUDIT-001  
**Revision:** A  
**Status:** ACTIVE  
**Effective date:** 2026-06-03  
**Author:** Quality Lead (interim: Steve Hickman, CEO)  
**Approved by:** Steve Hickman, CEO  
**References:** NP-PRIV-REM-001 Rev A STEP-21; NP-PRIV-001 Rev A; NP-APP-TELEMETRY-001 Rev B; NP-APP-ROADMAP-001 Rev B; NP-FW-EMMC-001 Rev A; NP-FW-EMMC-002 Rev A; NP-FW-ANON-001 Rev A; CLAUDE.md §5 (UHDR/SHDR), §6 (Consent Engine)  
**Gate:** BLOCKING for first external beta (TestFlight / Play Store open beta)  
**Review cadence:** Before each major app release and annually thereafter

---

## Privacy Analysis and Repair

### Summary

The NeuroPulse iOS/Android app handles some of the most sensitive personal data a consumer device can produce — raw EEG waveforms, HRV time series, cardiac rhythm, stimulation response — for users managing conditions including depression, PTSD, TBI, Parkinson's disease, and Alzheimer's disease. The underlying architecture is strong: UHDR is encrypted on-device with a biometric-derived key NeuroPulse never holds, SHDR is strictly separated, and an on-device research anonymisation pipeline avoids transmitting raw UHDR to any server.

**Jurisdiction scope:** Global — US (federal: FTC Act §5, FTC HBNR 16 CFR Part 318, HIPAA/HITECH for T2 clinical; state: BIPA IL, My Health My Data WA, CCPA/CPRA CA, and emerging state omnibus consumer privacy laws), EU/EEA GDPR, UK GDPR + DPA 2018, Canada PIPEDA + Quebec Law 25.

**Total findings: 16** — 0 Critical, 5 High, 7 Medium, 4 Low.

**Single highest-leverage change:** Lock and verify the SDK initialisation gate before any TestFlight or Play beta build ships. No analytics or crash reporting SDK touching a health app may initialise before consent. The FTC has taken enforcement action (GoodRx 2023, BetterHelp 2023, Premom 2023) based on pre-consent SDK initialisation patterns almost identical to what an unverified gate would produce.

---

### Findings

---

#### HIGH — Analytics and crash vendor not yet selected; PrivacyInfo.xcprivacy incomplete

**Where:** NP-APP-TELEMETRY-001 Rev B §2.1, §2.2; iOS app build  
**Category:** Pure privacy failure (governance gap → downstream unauthorised use)  
**Issue:** No analytics vendor and no crash reporter have been selected. Until a vendor is chosen and a DPA/BAA executed, the PrivacyInfo.xcprivacy manifest cannot be correctly completed, no SDK can be initialised in any test build, and the App Store privacy nutrition label cannot be accurately filed. Apple now rejects apps whose PrivacyInfo.xcprivacy declarations don't match the APIs accessed by included SDKs (Spring 2024 enforcement). A vendor selected at the last minute before TestFlight will compress DPA review, BAA negotiation, and PrivacyInfo.xcprivacy authoring into the same week as the TestFlight submission — a known recipe for privacy missteps.  
**Reference:** Apple PrivacyInfo.xcprivacy requirements (Spring 2024); GDPR Art. 28 (processor contracts); FTC Act §5; NP-APP-TELEMETRY-001 Rev B §2 vendor requirements table  
**Remediation:**  
1. Complete vendor evaluation using the criteria table in NP-APP-TELEMETRY-001 Rev B §2.1 at Month 6 (before G1 gate) — not at Month 12 (app launch).  
2. Execute DPA (analytics) and BAA (analytics + crash reporter) before any SDK is added to the Xcode project.  
3. Author PrivacyInfo.xcprivacy with the Privacy Lead reviewing every NSPrivacyAccessedAPICategory declaration against the SDK's own privacy manifest.  
4. Record the vendor name, DPA execution date, and BAA execution date in NP-APP-TELEMETRY-001 Rev B §2.1 (currently `[TBD]`).  
5. Add OI-TEL-02: "Analytics and crash vendor selected, DPA and BAA executed, PrivacyInfo.xcprivacy authored" as a blocking gate for any TestFlight submission.

---

#### HIGH — SDK initialisation gate not yet verified in any build

**Where:** NP-APP-TELEMETRY-001 Rev B §5; iOS/Android app entrypoint  
**Category:** Security failure + Pure privacy failure  
**Issue:** NP-APP-TELEMETRY-001 Rev B specifies the SDK initialisation gate (no SDK initialises before `consentCompleted()` is called), but no implementation exists yet and no verification procedure has been run. The policy document describes the pattern correctly — the risk is that a future engineer adds an SDK in `AppDelegate.application(_:didFinishLaunchingWithOptions:)` because that is the SDK vendor's integration instruction, and the gate is silently broken. Apple's App Tracking Transparency framework and GDPR Art. 7 both require that consent precedes any tracking. The FTC's 2023 enforcement actions against GoodRx, BetterHelp, and Premom were based on exactly this failure mode.  
**Reference:** "Third-party Free-for-all" anti-pattern; FTC Act §5; FTC HBNR 16 CFR Part 318; GDPR Art. 6, 7; NP-APP-TELEMETRY-001 Rev B §5  
**Remediation:**  
1. Implement `NeuroPulseConsentStore.hasCompletedConsent()` guard as the first line of any SDK initialisation call, using the code pattern specified in NP-APP-TELEMETRY-001 Rev B §5.  
2. Add a CI test (XCTest / instrumented Android test) that: (a) launches the app in a reset state; (b) confirms no network calls are made before `consentCompleted()` fires; (c) confirms the analytics SDK and crash reporter are not initialised at cold start.  
3. Add the SwiftLint / Android Lint rule from NP-APP-TELEMETRY-001 Rev B §7 to the linting configuration immediately — it should be in the repo before any analytics events are authored.  
4. Run the crash reporter verification procedure (NP-APP-TELEMETRY-001 Rev B §6) and document results as OI-TEL-01 before TestFlight.

---

#### HIGH — App Store privacy nutrition label and Google Play Data Safety not yet filed

**Where:** App Store Connect; Google Play Console; NP-PRIV-REM-001 STEP-16, STEP-17  
**Category:** Pure privacy failure (unawareness — users have no prior notice)  
**Issue:** Apple requires a privacy nutrition label (App Privacy section in App Store Connect) before any app is distributed, including TestFlight external testers. Google requires a Data Safety section before any open-beta submission. Neither has been filed. These disclosures are not optional — Apple's App Store Review Guidelines §5.1 requires them, and misrepresentation (including incomplete or inaccurate disclosures) can result in removal. For a health app collecting biometric data, the label will receive scrutiny from security researchers and regulators. An inaccurate label filed under time pressure is worse than a filing delay.  
**Reference:** Apple App Store Review Guidelines §5.1; Google Play Developer Policy (Data Safety); GDPR Art. 13 (transparency at collection point); NP-PRIV-REM-001 STEP-16/17  
**Remediation:**  
1. Complete the vendor selection (High finding #1 above) first — the label cannot be accurate without knowing which SDKs are present.  
2. Conduct a data flow mapping exercise: list every API the app calls, every SDK it bundles, and every piece of data that crosses the device boundary.  
3. Author the App Privacy label using the data flow map. For a health/EEG app, the following disclosures are expected: Health & Fitness data (App Functionality — not linked to identity, not used for tracking), Identifiers (if any analytics pseudonym is used), Diagnostics (crash data).  
4. The label must be reviewed by the Privacy Lead and legal counsel before submission.  
5. Add OI-TEL-03: "App Store privacy nutrition label and Google Play Data Safety filed and verified" as blocking for any external beta submission. Cross-reference with NP-APP-ROADMAP-001 Rev B OI-WA-06.

---

#### HIGH — BIPA written release screen implementation not verified

**Where:** NP-APP-ROADMAP-001 Rev B §9.3; NP-PRIV-001 Rev B HIGH-01; OI-PA-03  
**Category:** Pure privacy failure + legal exposure (Illinois BIPA statutory damages)  
**Issue:** The BIPA written release screen is specified in NP-APP-ROADMAP-001 Rev B §9.3 and is a binding engineering constraint, but it has not been implemented and OI-PA-03 (legal counsel review of the copy) remains open. BIPA provides a private right of action with statutory damages of $1,000–$5,000 per violation — the Facebook ($650M) and TikTok ($92M) settlements quantify the exposure scale. EEG waveforms are biometric information under BIPA 740 ILCS 14/10. Any Illinois resident who activates the device and runs a neurofeedback session without a BIPA-compliant written release creates individual liability. The app cannot ship to the US App Store without this screen being complete and legally reviewed, because geographic blocking at device activation is not an adequate substitute.  
**Reference:** BIPA 740 ILCS 14/15(b)(1)–(3); NP-APP-ROADMAP-001 Rev B §9.3; NP-PRIV-001 Rev B HIGH-01  
**Remediation:**  
1. Implement the BIPA written release screen using the approved copy in NP-APP-ROADMAP-001 Rev B §9.3 before any US App Store submission.  
2. Resolve OI-PA-03: engage Illinois-qualified privacy counsel to review the BIPA release copy. Budget 2–4 weeks for legal review.  
3. Implement Illinois detection via device locale (`NSLocale.current.regionCode == "US"` is insufficient — also check IP geolocation via the onboarding server call, and allow users to self-declare state of residence). Err toward showing the screen for all US users if geolocation is unavailable — false positives (showing the screen to non-Illinois users) carry no legal cost; false negatives (missing an Illinois user) carry statutory damages.  
4. Implement the graceful degradation path from NP-APP-ROADMAP-001 Rev B §9.3: if the user declines BIPA consent, EEG and closed-loop adaptive stimulation are disabled; the device still functions for PBM, VNS, audio, and visual. This must be tested before any US beta.  
5. Add OI-PA-03 closure as a blocking gate for US App Store submission.

---

#### HIGH — Washington My Health My Data compliance not verified for SHDR behavioural data

**Where:** SHDR data schema; NP-FW-EMMC-002 Rev A §G; NP-PRIV-001 Rev B HIGH-02  
**Category:** Pure privacy failure (potential unauthorised use of consumer health data)  
**Issue:** Washington's My Health My Data Act (MHMD, RCW 70.372) covers "consumer health data" — which explicitly includes data that is used to infer health-related characteristics. SHDR fields such as consumable session counts, device session count (even as an unsigned integer), and the `maintenance_alert` flag are counts of stimulation device usage that could reveal health management behaviour. CLAUDE.md §13.5 confirms `engagement_tier` replaces `session_sequence` in analytics, but the SHDR device session count (`device_session_count`, unsigned integer in NP-FW-EMMC-001 Rev A) is still a raw integer in SHDR. MHMD requires standalone authorisation (separate from HIPAA consent), prohibits sale, and provides a private right of action with AG enforcement. NP-PRIV-001 Rev B HIGH-02 flags this but the regulatory analysis has not yet been obtained (CLAUDE.md §13.4 open item: "Washington MHMD regulatory analysis — obtain before any Washington state device activation").  
**Reference:** Washington MHMD RCW 70.372; GDPR Art. 9 (inferrable health data); NP-PRIV-001 Rev B HIGH-02; CLAUDE.md §13.4  
**Remediation:**  
1. Engage Washington-qualified privacy counsel for MHMD regulatory analysis before any US device activation (not just Washington state). Estimated cost: $5,000–8,000; timeline: 3–4 weeks.  
2. Pending the legal analysis, apply the same coarsening principle that resolved the `session_sequence` issue in analytics: replace raw `device_session_count` (integer) in SHDR with a coarsened tier enum if the analysis determines the raw count is consumer health data.  
3. Confirm with counsel whether the SHDR fleet upload on USB-C connect constitutes a "collection" or "sharing" of consumer health data under MHMD and, if so, whether the warranty consent flow (described in NP-FW-EMMC-002 Rev A §A) provides the required standalone authorisation.  
4. Add the MHMD analysis completion as a blocking gate for any US device activation (not just Washington state — advice will clarify scope).

---

#### MEDIUM — Research consent UX four-layer onboarding not yet designed or tested

**Where:** CLAUDE.md §6.2 (four-layer a priori consent); NP-APP-ROADMAP-001 Rev B §3  
**Category:** Pure privacy failure (unawareness — consent not freely given if UX is coercive or confusing)  
**Issue:** The four-layer research consent system (L1 contact, L2 category, L3 blanket, L4 results) is fully specified in CLAUDE.md §6.2 and is one of NeuroPulse's architectural differentiators. However, no UX design exists, no formative usability testing has been conducted, and there is no review of whether the layered consent UX meets GDPR Art. 7 requirements (freely given, specific, informed, unambiguous) in implementation — not just in specification. Common failure modes in layered consent UX: the "all on" option is visually prominent while "all off" requires additional steps ("Dark Patterns" anti-pattern); the L3 blanket consent irreversibility notice is displayed in small print or after the user has already tapped through; per-category selection (L2) feels like a dark pattern when categories are pre-selected. The POA (power of attorney) upload path (NP-PROC-POA-001 Rev A) has no in-app UX at all.  
**Reference:** "Dark Patterns" anti-pattern; "Bundled Consent" anti-pattern; GDPR Art. 7 (consent); GDPR Rec. 32 (clear and plain language); NP-PRIV-REM-001 STEP-20; NP-PROC-POA-001 Rev A  
**Remediation:**  
1. Engage a UX designer with consent UX experience to produce wireframes for the four-layer consent onboarding before any implementation begins.  
2. Conduct formative usability testing with 5–8 participants (IEC 62366-1 §5.7 threshold for formative cycles). Test tasks: (a) "opt out of all research contact"; (b) "opt in to research for depression only"; (c) "find where to withdraw research consent after setup"; (d) read and understand the L3 irreversibility notice. Document pass/fail rates.  
3. Verify that: no category is pre-selected at L2; "decline all" is as prominent as "accept all" at L3; the irreversibility notice text meets a Flesch–Kincaid grade level ≤ 8; the POA upload path is accessible from the L1 screen via a clearly labelled link.  
4. Add OI-RES-01: "Research consent UX formative study — 5+ participants, L1–L4 task completion documented" as a gate for research consent system implementation.

---

#### MEDIUM — BLE GATT characteristics may transmit health data without audit trail

**Where:** NP-APP-ROADMAP-001 Rev B §5 (GATT service definition); NP-APP-ROADMAP-001 Rev B §2 (WatchConnectivity relay)  
**Category:** Pure privacy failure (data in transit scope exceeds documented minimum)  
**Issue:** The GATT service definition in NP-APP-ROADMAP-001 Rev B §5 transmits `HRV_COHERENCE` (coherence score × 100 + RMSSD ms) and `PACER_PHASE` (inhale/exhale state) at 100ms and 5-second intervals. HRV RMSSD is a direct physiological measurement — it falls within UHDR under the 27-element classification table in NP-FW-EMMC-001 Rev A §12. The current spec says this NOTIFY characteristic is broadcast over BLE to any connected app client, including the Apple Watch app via WatchConnectivity. There is no documented scope limitation on who can connect to the GATT service, no encryption requirement beyond BLE pairing, and no audit trail that this characteristic was read.  
**Reference:** GDPR Art. 5(1)(f) (integrity and confidentiality); GDPR Art. 32 (security of processing); NP-FW-EMMC-001 Rev A §12 (UHDR classification); "Least Privilege" pattern  
**Remediation:**  
1. Document the intended consumers of each GATT characteristic. `HRV_COHERENCE` should only be readable by the paired user's iPhone app. The service specification should explicitly state that BLE bonding is required and that only the device bonded during setup may access NOTIFY characteristics.  
2. Evaluate whether `HRV_COHERENCE` can transmit the coherence score only (0–10 scaled integer) rather than raw RMSSD ms. The Watch app uses the coherence score for the breathing ring display; it does not need RMSSD. This would remove a UHDR-class value from the BLE characteristic.  
3. Log GATT client connection and characteristic subscription events in SHDR (client device class, connection timestamp, characteristic subscribed) — not the values transmitted. This creates an audit trail without creating a new UHDR data stream.  
4. Add GATT service access control to OI-SEC-01 as a security verification item before production BLE firmware is finalised.

---

#### MEDIUM — Apple Watch sync app data flow has unresolved privacy implications

**Where:** NP-APP-ROADMAP-001 Rev B §4 (Watch phases 1–4)  
**Category:** Pure privacy failure (data minimisation; third-party data scope)  
**Issue:** The WatchConnectivity relay sends a subset of GATT data to the Apple Watch. Apple Watch platforms (watchOS, HealthKit, Workout context) have their own data retention and sharing behaviours that are outside NeuroPulse's control: (a) HealthKit may automatically record HRV and heart rate data from sessions if any app component calls `HKWorkoutSession` or `HKWorkoutBuilder` — even inadvertently; (b) watchOS may surface session data in Siri Suggestions or Handoff state; (c) the Apple Watch coherence score display (Phase 1) is visible on the wrist in any context — a side-channel disclosure in clinical or workplace settings. None of these downstream behaviours are currently addressed in any privacy document.  
**Reference:** "Purpose Creep" anti-pattern; GDPR Art. 28 (processors — Apple is a data processor for HealthKit); Apple HealthKit API agreement; NP-APP-ROADMAP-001 Rev B §9.1  
**Remediation:**  
1. Audit the Watch app implementation plan for any inadvertent use of HealthKit APIs (even indirect, via watchOS workout context). Confirm that `HKWorkoutSession` and `HKWorkoutBuilder` are never instantiated by the Watch app.  
2. Explicitly configure AVAudioSession in Phase 2 audio sync without any HealthKit or Workout context categories that could cause automatic HRV/HR recording.  
3. Address wrist-display privacy in the Watch app UX: the HRV breathing ring and coherence score should be suppressible with a single tap (privacy mode — replaces the health display with a neutral session timer). Document this as a UX requirement.  
4. Review whether Apple Watch clipboard, Handoff, and Universal Clipboard APIs are disabled for any WatchConnectivity message that carries session state.  
5. Resolve OI-WA-06 ("HealthKit permission review + privacy nutrition label sign-off before App Store submission") before Phase 1 App Store submission.

---

#### MEDIUM — Clinical consent engine three-tier data display not designed

**Where:** CLAUDE.md §6.1 (use-case subscription tiers: Monitor/Assess/Full Clinical)  
**Category:** Pure privacy failure (unawareness — clinician access scope not visible to user in real time)  
**Issue:** The clinical consent engine specifies three tiers (Monitor, Assess, Full Clinical) with different UHDR element access, and uses plain-language decision support documents to explain what each clinician can and cannot see. However, no UX design exists for: (a) the in-app display that shows the user which clinicians currently have active access, at what tier, and for how long; (b) the "expand access" differential consent flow described in CLAUDE.md §6.1; (c) retroactive vs. prospective access consent, which CLAUDE.md §6.1 says must be presented as two separate decisions. There is also no UX design for access revocation — the user must be able to revoke individual clinician access per element, per time window, without revoking the entire relationship.  
**Reference:** "Privacy Dashboard" pattern; "Personal Data Table" pattern; GDPR Art. 7(3) (withdrawal of consent as easy as giving it); GDPR Art. 17 (right to erasure, which includes revocation of processing consent)  
**Remediation:**  
1. Design a "Connected Clinicians" screen in the app showing: clinician name and institution; active tier; elements accessible (plain-language list); access expiry date; [Revoke access] button.  
2. Design the differential consent flow for tier expansion: present the incremental UHDR elements being newly requested, not the full set; present retroactive access request as a separate, clearly labelled decision beneath the prospective consent.  
3. Design the revocation flow: single-tap revoke from the Connected Clinicians screen; immediate local revocation confirmation; 30-day deletion cascade per NP-LEGAL-BAA-001 Rev A §5.1.  
4. Formative test the clinician access UX with 3–5 participants. Task: "your neurologist now wants to see your EEG data — show me where you go to approve or decline this."

---

#### MEDIUM — Crash reporter verification procedure has no CI enforcement

**Where:** NP-APP-TELEMETRY-001 Rev B §6  
**Category:** Pure privacy failure + security failure  
**Issue:** NP-APP-TELEMETRY-001 Rev B §6 defines a manual verification procedure (intentionally trigger a crash, inspect the vendor UI) but there is no automated enforcement. Crash reporter configurations can be changed by a vendor SDK update, a library version bump, or a copy-paste from a Stack Overflow answer that re-enables payload capture. The manual procedure is documented as OI-TEL-01 but has no CI gate. A session runner crash — which is the most common crash path in a hardware-interfacing app — can have stack-local variables containing EEG band values, session parameters, or BLE payload buffers. If the crash reporter is misconfigured, these end up in the vendor's infrastructure.  
**Reference:** "Full Payload Logging" anti-pattern; GDPR Art. 32; NP-APP-TELEMETRY-001 Rev B §6  
**Remediation:**  
1. Implement a CI test that builds the app in release mode with the crash reporter SDK included and verifies, via a mock network layer, that no request body, no local variable map, and no screenshot is transmitted in a simulated crash event.  
2. Pin the crash reporter SDK version in the package manifest (not a version range) and require the Privacy Lead to review every version bump for changes to capture behaviour.  
3. Add crash reporter configuration verification to the pre-release checklist for every app version — not only the first TestFlight build.

---

#### MEDIUM — `engagement_tier` counter stored in UserDefaults without clear deletion-on-erasure path

**Where:** NP-APP-TELEMETRY-001 Rev B §3.2; iOS `UserDefaults`; Android `SharedPreferences`  
**Category:** Pure privacy failure (incomplete data lifecycle)  
**Issue:** NP-APP-TELEMETRY-001 Rev B §3.2 specifies that the `NP_APP_LAUNCH_COUNT` counter is stored in `UserDefaults` (iOS) / `SharedPreferences` (Android) and resets on uninstall. However, iOS `UserDefaults` backed by iCloud backup does not reset on uninstall if iCloud backup is enabled — reinstallation on the same device restores the counter. This could allow an analytics vendor to infer continuity of the same user across "new" installs. Additionally, the GDPR right to erasure requires that on account deletion, all locally stored data is purged — there is no documented erasure path for `NP_APP_LAUNCH_COUNT`.  
**Reference:** GDPR Art. 17 (right to erasure); "Permanent Storage" anti-pattern; NP-APP-TELEMETRY-001 Rev B §3.2  
**Remediation:**  
1. Store `NP_APP_LAUNCH_COUNT` in `UserDefaults(suiteName:)` with `.local` scope (excludes iCloud sync) on iOS, and in a non-backed-up storage location on Android (`Context.getNoBackupFilesDir()`).  
2. Add `NP_APP_LAUNCH_COUNT` to the account deletion erasure cascade: when the user deletes their account or exercises the right to erasure, clear this key alongside all other app-local storage.  
3. Document this erasure path in the app's data erasure procedure (to be authored as part of the account deletion flow).

---

#### LOW — Adaptive stimulation trigger enum not yet authored or reviewed

**Where:** NP-APP-ROADMAP-001 Rev B §9.4; NP-PRIV-REM-001 STEP-33; OI-PA-04  
**Category:** Pure privacy failure (unawareness — transparency obligation not fulfilled)  
**Issue:** The Adaptive Adjustments card in Session History is a GDPR Art. 13(2)(f) compliance requirement (information about automated decision-making that significantly affects the user). The card is specified in NP-APP-ROADMAP-001 Rev B §9.4 and requires a plain-language trigger enum mapping. OI-PA-04 (Privacy Lead sign-off on the trigger copy) is open. No trigger enum has been authored yet. If the session runner firmware is implemented before the enum is defined and reviewed, the adaptive transparency card will be implemented as a retrofit, which historically produces incomplete enums (the firmware has more trigger types than the app surfaces).  
**Reference:** GDPR Art. 13(2)(f) (automated individual decision-making information); NP-PRIV-REM-001 STEP-33; NP-APP-ROADMAP-001 Rev B §9.4  
**Remediation:**  
1. Author the trigger enum *before* the session runner firmware is implemented, not after. Work with the firmware team to enumerate every closed-loop adaptation event type the session runner can produce, then write the plain-language label for each. Add to NP-QMS-DC-001 that extending the enum is required whenever a new adaptive trigger is added to firmware.  
2. Resolve OI-PA-04 (Privacy Lead sign-off on trigger copy) before the Adaptive Adjustments card is included in any build.  
3. Verify that no raw EEG value (band power ratio, frequency, amplitude) is included in the displayed event — only the plain-language label from the approved enum.

---

#### LOW — App privacy notice update process not documented

**Where:** App distribution; NP-PRIV-001 Rev A; GDPR Art. 13–14  
**Category:** Pure privacy failure (unawareness — stale notice creates legal exposure)  
**Issue:** No process exists for updating the in-app privacy notice when the data processing scope changes (new SDK, new GATT characteristic, new UHDR element, new research study type). GDPR Art. 13 requires that the notice accurately reflects current processing at all times. If the app adds the Watch app data relay (Phase 1), audio sync to AirPods (Phase 2), or any new analytics event, the privacy notice must be updated before the new version ships and existing users must be notified.  
**Reference:** GDPR Art. 13 (transparency at collection), Art. 14 (transparency from other sources), Art. 34 (communication to data subjects); ICO guidance on privacy notices  
**Remediation:**  
1. Add a privacy notice review step to the app release checklist: any PR that adds a new SDK, new GATT characteristic, new analytics event, or new UHDR element must be reviewed for privacy notice impact before merging.  
2. Implement an in-app "What changed" summary for material privacy notice updates — not just a ToS update checkbox — that surfaces to existing users on the first launch after the update.  
3. Version the privacy notice (PN-001, PN-002) and store the version number in the user's consent record so that users who accepted an older notice are identified and notified when a material update ships.

---

#### LOW — Minimum age gate threshold pending legal confirmation; no guardian pathway for T2

**Where:** NP-APP-ROADMAP-001 Rev B §9.2; OI-PA-01; OI-PA-02  
**Category:** Pure privacy failure (COPPA / GDPR Art. 8 compliance gap)  
**Issue:** The minimum age gate is specified (16 years, with a checkbox declaration) but OI-PA-01 (legal counsel confirmation that 16 is the correct threshold) remains open. The threshold matters: COPPA applies to under-13; GDPR Art. 8 allows member states to set the threshold as low as 13 (Germany) or as high as 16 (many EU states); BIPA implicitly requires adult consent (18) for biometric data. A threshold of 16 may be wrong for some US states and correct for most EU states. Additionally, OI-PA-02 (guardian consent pathway for T2 minor patients) is open — the T2 clinical use case for paediatric neurological patients (Parkinson's, TBI rehabilitation) requires a different consent architecture.  
**Reference:** COPPA 15 U.S.C. §6502; GDPR Art. 8; BIPA 740 ILCS 14/15(b); NP-APP-ROADMAP-001 Rev B §9.2  
**Remediation:**  
1. Resolve OI-PA-01: engage privacy counsel to confirm the age threshold matrix by jurisdiction (US, EU per-member-state, UK, Canada) and produce a jurisdiction-aware consent flow decision tree.  
2. Implement the age gate as a conditional declaration with jurisdiction-derived threshold (13, 16, or 18) — not a fixed global value.  
3. Resolve OI-PA-02: design the guardian consent pathway for T2 minor patients before T2 clinical launch. This pathway requires separate legal review and likely a wet-signature process outside the app.

---

#### LOW — Privacy-specific regression test suite not planned

**Where:** CI/CD pipeline; app test suite  
**Category:** Pure privacy failure (monitoring gap)  
**Issue:** The app's CI pipeline has no privacy-specific regression tests. Without tests that fail when a prohibited event property (from NP-APP-TELEMETRY-001 Rev B §4) appears in an analytics call, or when the SDK initialisation gate is bypassed, or when a UHDR value appears in a log, privacy regressions will only be caught in manual review — if at all. Privacy regressions are among the most common and most costly class of bugs in health apps, and they typically arise from well-intentioned debugging code that is never removed.  
**Reference:** "Privacy & Security Regression Tests" security pattern; GDPR Art. 32(1)(d) (regular testing of security measures); NP-APP-TELEMETRY-001 Rev B §5, §7  
**Remediation:**  
1. Implement the SwiftLint / Android Lint rule from NP-APP-TELEMETRY-001 Rev B §7 immediately — this is a zero-cost prevention measure.  
2. Add the following to the CI test suite before any TestFlight submission: (a) SDK gate test (no network calls before consent); (b) event property lint test (no prohibited string in analytics event name or value); (c) GATT output test (no UHDR-class raw value transmitted in any mock session event); (d) crash reporter payload test (no variable values in crash report body).  
3. Run the privacy regression suite on every pull request, not just before release builds.

---

### What looks good

**UHDR encryption architecture.** The biometric-derived AES-256-XTS key scheme (NP-FW-EMMC-001 Rev A, updated by NP-FW-EMMC-002 Rev A) is the right architecture for a health data device. NeuroPulse never holds the decryption key. If the company's servers are breached, user health data is not exposed. This is a genuine privacy-by-design choice that goes well beyond regulatory minimum and directly addresses the category of harm (operator access to health data) that the FTC has been enforcing against health apps.

**SHDR/UHDR strict separation.** The two-partition eMMC architecture with separate encryption keys ensures that NeuroPulse's fleet telemetry (SHDR) is structurally isolated from user health data (UHDR). The 27-element classification table (NP-FW-EMMC-001 Rev A §12) is thorough and the boundary-case resolution rule ("when in doubt → UHDR") is the right default.

**On-device research anonymisation.** The architecture where anonymisation runs on-device, on demand, per study, using a signed study descriptor, is the correct approach. It means NeuroPulse cannot be compelled to produce raw UHDR even in response to a data request, because it genuinely does not have it.

**Analytics telemetry policy is appropriately cautious.** NP-APP-TELEMETRY-001 Rev B correctly identifies the FTC precedents, specifies a one-vendor maximum, requires BAA, prohibits IDFA/GAID, and produces an explicit whitelist of permitted event properties. The `engagement_tier` coarsening is a good response to the `session_sequence` privacy finding.

**Consent withdrawal is forward-effective for research.** CLAUDE.md §6.2 and the research anonymisation architecture ensure that consent withdrawal immediately stops all future data flows from any time period — including historical sessions. The per-study, on-demand generation model means there is no database of pre-generated extracts to roll back; withdrawal is structurally effective, not just procedurally promised.

**BIPA, MHMD, and age gate are identified as pending.** Many health app developers discover BIPA exposure post-launch. NeuroPulse has identified it pre-tooling and has specific open items for resolution. The same is true for Washington MHMD. This is the right governance posture.

**Crash reporter policy prohibits variable capture and screenshot capture.** These two settings are responsible for the majority of accidental health data leakage in crash reports. Prohibiting them in policy before a vendor is selected means the requirement enters vendor evaluation rather than being retrofitted.

---

### What couldn't be reviewed

**No app code exists yet.** This audit is necessarily specification-level, not code-level. When the first implementation sprint produces a working prototype — even a stub — a code-level privacy review should be conducted. OWASP MASVS (Mobile Application Security Verification Standard) provides the right framework for the implementation-level pass.

**No vendor SDKs are present.** The privacy implications of the chosen analytics and crash reporter SDKs (their sub-processors, their own privacy manifests, their data retention terms) cannot be assessed until vendors are selected. OI-TEL-02 above addresses this.

**Clinical consent engine UX.** No wireframes or implementation exist. The specification in CLAUDE.md §6.1 is thorough; the implementation risk is in the translation of that specification into screens that users actually understand and that courts and regulators would accept as meeting GDPR Art. 7 standards.

**T2 clinical backend.** The FHIR R4 profile (NP-INT-FHIR-001 Rev A), BAA template (NP-LEGAL-BAA-001 Rev A), and T2 scripting API (NP-API-001, not yet authored) are separate from this audit's scope. NP-PRIV-AUDIT-001 covers the iOS/Android app client only.

---

### Recommended next steps

**1. Select analytics and crash reporting vendors (Month 6 — before G1 gate).** Execute DPA and BAA before any SDK is added to the Xcode project. Author PrivacyInfo.xcprivacy. This unblocks the App Store privacy nutrition label, the SDK init gate verification, and the crash reporter verification procedure. All five High findings converge on vendor selection as a prerequisite. Estimated effort: 2–4 weeks for evaluation and legal review.

**2. Implement and CI-verify the SDK initialisation gate before any test build ships.** Write the `NeuroPulseConsentStore.hasCompletedConsent()` guard, the CI network-call test, and the SwiftLint lint rule. These three items are a single engineering sprint and eliminate the most likely FTC enforcement vector for health app analytics. Estimated effort: 3–5 days.

**3. Engage privacy counsel for BIPA (Illinois) and MHMD (Washington) regulatory analyses before any US App Store submission.** These are legal opinions, not engineering tasks, but they gate US device activation and require 3–5 weeks' lead time. Brief counsel simultaneously on both: BIPA EEG biometric consent requirements and MHMD consumer health data classification of SHDR session counts. Estimated cost: $10,000–15,000 combined. Estimated timeline: 4 weeks from engagement.

**4. Commission research consent UX formative study and clinical access UX design before research or clinical systems are implemented.** UX design for layered consent is harder than UX design for functional screens, and poorly designed consent UX is a regulatory enforcement target. Commission both simultaneously. Estimated effort: 3–4 weeks for wireframes; 1–2 weeks for formative testing. Output closes OI-RES-01 and addresses the clinical consent Medium finding.

**5. Author the adaptive stimulation trigger enum before session runner firmware implementation begins.** This is a zero-cost, low-effort item (half-day with the firmware team) that prevents a retrofit problem later. Close OI-PA-04 in the same session. Estimated effort: 4 hours.

---

There are 4 additional items in this audit (the four Low findings) beyond the top 5 above. To continue, either apply the five changes above and re-run NP-PRIV-AUDIT-001 Rev B against the resulting implementation (which will retire several findings and reveal any new ones), or request the full action list for all 16 findings as a flat implementation backlog.

---

## Compliance Checklist

The following items must all be verified before first external beta (TestFlight / Play Store open beta). Each maps to a finding above.

| ID | Item | Owner | Status | Blocking |
|----|------|-------|--------|---------|
| AUDIT-01 | Analytics vendor selected; DPA + BAA executed | Privacy Lead | OPEN | External beta |
| AUDIT-02 | Crash reporter selected; DPA executed; redacted payload mode confirmed | Privacy Lead | OPEN | External beta |
| AUDIT-03 | PrivacyInfo.xcprivacy authored and reviewed | iOS Engineering + Privacy Lead | OPEN | App Store submission |
| AUDIT-04 | App Store privacy nutrition label filed and verified | Privacy Lead + Legal | OPEN | App Store submission |
| AUDIT-05 | Google Play Data Safety filed and verified | Privacy Lead + Legal | OPEN | Play Store submission |
| AUDIT-06 | SDK initialisation gate implemented and CI-verified | iOS/Android Engineering | OPEN | External beta |
| AUDIT-07 | SwiftLint / Android Lint prohibited event property rule in repo | iOS/Android Engineering | OPEN | External beta |
| AUDIT-08 | Crash reporter verification procedure run; results in OI-TEL-01 | iOS Engineering | OPEN | External beta |
| AUDIT-09 | BIPA written release screen implemented and OI-PA-03 (legal review) closed | Legal + iOS Engineering | OPEN | US App Store submission |
| AUDIT-10 | Washington MHMD regulatory analysis received | Legal Counsel | OPEN | US device activation |
| AUDIT-11 | Research consent UX formative study complete | UX + Privacy Lead | OPEN | Research system implementation |
| AUDIT-12 | Clinical access UX designed (Connected Clinicians screen + revocation) | UX + SW Engineering | OPEN | Clinical system implementation |
| AUDIT-13 | `NP_APP_LAUNCH_COUNT` stored in non-iCloud-synced location | iOS Engineering | OPEN | External beta |
| AUDIT-14 | Adaptive stimulation trigger enum authored and OI-PA-04 closed | Firmware + Privacy Lead | OPEN | Session runner implementation |
| AUDIT-15 | Privacy notice version management and update process documented | Privacy Lead | OPEN | First release |
| AUDIT-16 | Minimum age gate threshold confirmed by legal counsel (OI-PA-01) | Legal Counsel | OPEN | US + EU launch |

---

## Open Items

| ID | Item | Raised by | Priority |
|----|------|-----------|---------|
| OI-AUDIT-01 | Code-level privacy audit (OWASP MASVS) — conduct when first prototype build exists | NP-PRIV-AUDIT-001 Rev A | High |
| OI-AUDIT-02 | Vendor SDK privacy manifest review — conduct at vendor selection | NP-PRIV-AUDIT-001 Rev A | High |
| OI-AUDIT-03 | T2 clinical backend privacy audit (FHIR, scripting API, BAA cascade) — separate scope from this document | NP-PRIV-AUDIT-001 Rev A | Medium |
| OI-AUDIT-04 | NP-PRIV-AUDIT-001 Rev B — re-audit against first implementation build; target Month 9 | NP-PRIV-AUDIT-001 Rev A | High |

---

*NP-PRIV-AUDIT-001 Rev A — CONFIDENTIAL — NeuroPulse Design Programme*  
*This document is a QMS record under NP-QMS-DC-001 and is entered into the Design History File (NP-DHF-001) as a design verification activity.*
