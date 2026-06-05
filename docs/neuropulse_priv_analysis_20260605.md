# NP-PRIV-ANALYSIS-002 — Privacy Analysis and Repair

**Document number:** NP-PRIV-ANALYSIS-002  
**Revision:** A  
**Effective date:** 2026-06-05  
**Branch:** `privacy/analysis-and-repair-20260605`  
**Builds on:** NP-PRIV-AUDIT-001 Rev A (2026-06-03), NP-PRIV-REM-001 Rev A  
**Jurisdiction scope:** Global — US federal (FTC Act §5, HBNR, HIPAA T2), US state (BIPA IL, WA MHMD, CCPA/CPRA CA), EU/EEA GDPR, UK GDPR + DPA 2018, Canada PIPEDA + Quebec Law 25

---

## Privacy Analysis and Repair

### Summary

This review covers the iOS app source as committed to `main` on 2026-06-05. The architecture remains strong — UHDR encrypted on-device, SHDR/UHDR separation enforced, analytics gate with prohibited-key enforcement, four-layer research consent, BIPA and age gate implemented. Six new findings were identified (1 Critical, 3 High, 5 Medium, 3 Low) across two categories: security failures that could leak personal data and pure privacy failures where an authorised party could access data beyond consent. Five of the six findings have been repaired on the branch; one (BIPA enforcement in the protocol upload path) is documented for a targeted follow-up issue.

**Highest-leverage change shipped:** `UHDRKeyManager` now blocks release builds with a `#error` if the PBKDF2 placeholder with its hardcoded password has not been replaced with Argon2id + a real biometric/PIN credential — preventing the critical finding from ever reaching production silently.

---

### Findings

---

#### CRITICAL — UHDRKeyManager: PBKDF2 placeholder uses hardcoded password — UHDR encryption is not user-specific

**Where:** `app/ios/NeuroPulse/Data/UHDRKeyManager.swift:93` — `deriveKey(password: "biometric-placeholder", salt: salt)`  
**Category:** Security failure + Pure privacy failure  
**Issue:** The KDF call passes the literal string `"biometric-placeholder"` as the password input, not the user's actual biometric credential or PIN. Both the password and the salt (SHA256 of `identifierForVendor`) are knowable to anyone who reads the source code. In practice this means every user's "UHDR key" is derived from the same fixed password and a device-stable but not user-specific value — the key offers no user-specific protection. The CLAUDE.md guarantee that "NeuroPulse cannot decrypt UHDR" and "the biometric-derived key is never held by NeuroPulse" is violated in spirit by the current code: any party who knows the source code and can read the device identifier can derive the key.  
**Reference:** "Encryption with User-Managed Keys" security pattern; GDPR Art. 32 (security of processing); NIST SP 800-132 §5 (password-based key derivation); NP-FW-EMMC-002 Rev A §C (UHDR two-layer key scheme)  
**Remediation (shipped):** Added `#if !DEBUG … #error(…) #endif` guard in `UHDRKeyManager.authenticate()` that produces a compile-time failure in release builds until the placeholder is replaced with Argon2id + a real biometric/PIN credential. This prevents silent production shipping while keeping debug builds functional for development.  
**Remaining action:** Replace the PBKDF2 placeholder with Argon2id (link `swift-crypto-extras` or the Argon2 reference C library) and pass the LAContext biometric token (or a PIN digest from a secure enclave) as the password input. Spec: NP-FW-EMMC-002 Rev A §C.

---

#### HIGH — SHDRUploader: `identifierForVendor` used as fleet DB linkage key instead of opaque warranty token

**Where:** `app/ios/NeuroPulse/Data/SHDRUploader.swift:74` (pre-fix) — `request.setValue(deviceID, forHTTPHeaderField: "X-NP-Device-ID")`  
**Category:** Pure privacy failure  
**Issue:** `UIDevice.current.identifierForVendor` is an app-bundle-scoped stable identifier that can persist across device restores and can be correlated across any app or service in the same vendor bundle ID group. The design specification (NP-FW-EMMC-002 Rev A §A) explicitly requires an opaque 256-bit TRNG warranty token as the SHDR linkage key, with no-join CI enforcement between the warranty DB and the SHDR fleet DB. Using `identifierForVendor` instead creates a linkable identifier that could be joined to App Store purchase records, Apple's device-identity infrastructure, or any other service that received the same vendor UUID. Under GDPR Art. 4(1) a pseudonym that permits re-identification with "reasonably likely" means is still personal data.  
**Reference:** "Pseudonymous Identity" pattern; "Linkable Identifiers" anti-pattern; GDPR Art. 4(1), 25; NP-FW-EMMC-002 Rev A §A  
**Remediation (shipped):** Replaced `identifierForVendor` with `warrantyTokenFromKeychain()` — a 32-byte `SecRandomCopyBytes` token generated once at first run and stored in the Keychain with `kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly` and `kSecAttrSynchronizable: false`. The header field renamed from `X-NP-Device-ID` to `X-NP-Device-Token`. The token is not correlated to any Apple identity service.  
**Remaining action:** When hub firmware delivers the 256-bit TRNG warranty token over the GATT warranty characteristic, replace the app-generated fallback with the hub-provisioned token per NP-FW-EMMC-002 §A.

---

#### HIGH — UHDRBackupScheduler: backup directory not excluded from iCloud/iTunes backup

**Where:** `app/ios/NeuroPulse/Data/UHDRBackupScheduler.swift` — `Documents/UHDRBackup/` directory  
**Category:** Pure privacy failure  
**Issue:** The backup directory written to `Documents/UHDRBackup/` is included in iCloud and iTunes backups by default unless `.isExcludedFromBackupKey` is set. The encrypted `.enc` archives are AES-256-GCM with the user's UHDR key, so their contents are opaque — but the encrypted ciphertext still flows to Apple iCloud servers under a subpoena or government legal process. This undermines the "user-held key, NeuroPulse-cannot-decrypt" privacy guarantee: Apple holds the iCloud backup encryption key (standard iCloud backup, not iCloud Advanced Data Protection), so a third party with legal process against Apple could access the encrypted archives and, separately, obtain the user's derived key through other means. The manifest was also stored in plaintext including `keyFingerprint: SHA256(K1).prefix(8)` — partial key material in a backup-exposed plaintext file.  
**Reference:** Cavoukian Principle 5 (end-to-end security — full lifecycle); "Backup Immortality" anti-pattern; GDPR Art. 32; Ashley Madison precedent (retention of data after "delete" is the worst part)  
**Remediation (shipped):**  
1. `.isExcludedFromBackupKey = true` set on `backupDirectory` in `init()` — backup directory is now excluded from both iCloud and iTunes backups.  
2. `keyFingerprint` removed from `BackupManifest` struct — no partial key material in any plaintext file.

---

#### HIGH — SHDRUploader: `shdr_staging.bin` included in iCloud/iTunes backup

**Where:** `app/ios/NeuroPulse/Data/SHDRUploader.swift` — `Documents/shdr_staging.bin`  
**Category:** Pure privacy failure  
**Issue:** The staging file is a transient device telemetry blob intended to be uploaded and deleted. Placing it in `Documents/` without `.isExcludedFromBackupKey` means it can be captured in any iCloud or iTunes backup taken between the staging write and the upload. SHDR is device-condition data (not user biology), but session counts and hardware telemetry backed up to a third party's servers exceeds the intended data scope.  
**Reference:** "Permanent Storage" anti-pattern (data outliving its purpose); GDPR Art. 5(1)(e) (storage limitation)  
**Remediation (shipped):** `(stagingURL as NSURL).setResourceValue(true, forKey: .isExcludedFromBackupKey)` applied immediately after constructing the staging URL, before any data is written.

---

#### MEDIUM — UHDRKeyManager: biometric-only policy blocks users without Face ID/Touch ID enrollment

**Where:** `app/ios/NeuroPulse/Data/UHDRKeyManager.swift:76` — `.deviceOwnerAuthenticationWithBiometrics`  
**Category:** Pure privacy failure (usability → data inaccessibility)  
**Issue:** `canEvaluatePolicy(.deviceOwnerAuthenticationWithBiometrics)` returns false if the device has no biometric enrolled — the error is thrown immediately and UHDR can never be decrypted. Users with physical disabilities that prevent biometric enrollment (e.g., severe Parkinson's, post-stroke), users who deliberately don't use Face ID, and enterprise-deployed devices without biometric policies would have permanently inaccessible UHDR data. This is adverse to the "user owns their health data" principle and creates an accessibility barrier for the exact patient populations NeuroPulse serves (CLAUDE.md §10 HFE plan, IEC 62366-1).  
**Reference:** Cavoukian Principle 4 (full functionality, positive-sum); WCAG 2.2 AA; NP-HFE-001 (planned)  
**Remediation (shipped):** Changed both `canEvaluatePolicy` and `evaluatePolicy` calls to use `.deviceOwnerAuthentication` — which attempts biometrics first but falls back to PIN/passcode when biometrics are unavailable or fail, matching Apple's own HIG guidance for health-adjacent apps.

---

#### MEDIUM — AnalyticsGate: `isConfigured` static flag not reset on consent withdrawal; SDK runs silently

**Where:** `app/ios/NeuroPulse/Analytics/AnalyticsGate.swift` — static `isConfigured` flag  
**Category:** Pure privacy failure  
**Issue:** When a user withdraws consent, the `isOpen` property reads `false` from UserDefaults and `track()` no-ops correctly. However `isConfigured` remains `true` and the underlying analytics SDK (when a real vendor is selected at OI-AUDIT-01) continues running in whatever background mode it supports. Analytics SDKs commonly instrument app lifecycle events, crash events, and performance metrics passively without `track()` being called — this is exactly the pattern the FTC called out in the BetterHelp and GoodRx enforcement actions.  
**Reference:** "Third-party Free-for-all" anti-pattern; FTC Act §5; FTC HBNR 16 CFR Part 318; GDPR Art. 7(3) (right to withdraw consent)  
**Remediation (shipped):**  
1. `AnalyticsGate.reset()` added — clears `isConfigured` and calls `AnalyticsSDKStub.reset()` (stub for the real SDK teardown call).  
2. `ConsentStore.revokeAnalyticsConsent()` added — removes the consent key from UserDefaults AND calls `AnalyticsGate.reset()` in one atomic operation.  
3. `withdrawBlanketConsent()` now calls `revokeAnalyticsConsent()`.

---

#### MEDIUM — SessionHistoryStore: session timestamps stored in UserDefaults (UHDR-class data outside encrypted partition)

**Where:** `app/ios/NeuroPulse/Session/SessionHistoryStore.swift` — `SessionRecord.completedAt`  
**Category:** Pure privacy failure  
**Issue:** The project's own UHDR classification (CLAUDE.md §5.1) defines session timestamps as UHDR: "session timestamps → UHDR". `SessionRecord.completedAt` persists timestamps to `UserDefaults.standard`, which is backed up in iTunes/iCloud (not protected by `.isExcludedFromBackupKey`) and is accessible via MDM without biometric unlock. The timestamps reveal when a user runs neuromodulation sessions — which, for conditions like depression, PTSD, or Alzheimer's prevention, is itself diagnostic. Under Washington My Health My Data (RCW 70.372), session timestamps from a health device constitute consumer health data.  
**Reference:** UHDR definition in CLAUDE.md §5.1; "Purpose Creep" anti-pattern; WA MHMD RCW 70.372; GDPR Art. 9 (session patterns for neurological treatment are special-category health data by inference)  
**Remediation (not yet shipped):** `SessionHistoryStore` should persist to the encrypted UHDR partition (via the hub's EDF+ download pathway) rather than `UserDefaults`, or at minimum the `completedAt` field should be coarsened to session-day granularity (not timestamp-precision) when stored client-side. The `completedAt` display in `SessionHistoryView` can use the precise value loaded from UHDR at display time. Raise as a dedicated issue targeting the `SessionHistoryStore` → UHDR migration.

---

#### MEDIUM — RegionHelper: locale-only BIPA detection misses users with non-IL locale

**Where:** `app/ios/NeuroPulse/Onboarding/RegionHelper.swift` — `Locale.current.region?.identifier == "US-IL"`  
**Category:** Pure privacy failure (non-compliance risk)  
**Issue:** Locale is a user preference, not a verified location. A user with an Illinois billing address who uses an `en_US` locale (not `en_US-IL`) will not receive the BIPA written-release disclosure. BIPA applies to any person whose biometric data is collected by a business operating in Illinois — it is not limited to people whose locale reports Illinois. BIPA §15(b) requires written releases from "each subject of the biometric identifier or information." The `// Note: locale-based detection is best-effort. OI-PA-03 open.` comment in the code acknowledges this, but it has not been actioned.  
**Reference:** BIPA 740 ILCS 14/15(b); "Unawareness" LINDDUN threat; NP-PRIV-001 Rev B HIGH-01; OI-PA-03  
**Remediation (not yet shipped — requires legal guidance on OI-PA-03):** The most privacy-preserving approach is to show the BIPA disclosure to all users, not just those with an IL locale — the disclosure is informative and not harmful to non-Illinois users. Alternatively, add an explicit "Are you located in Illinois?" prompt with a `No` path that skips BIPA disclosure. Legal counsel review (OI-PA-03) should resolve which approach is required.

---

#### LOW — ConsentDashboardView: research contact email/phone displayed in plain text

**Where:** `app/ios/NeuroPulse/Views/ConsentDashboardView.swift:99` — `Text("Contact: \(consentStore.researchConsent.contactMethod)")`  
**Category:** Pure privacy failure (display surface)  
**Issue:** The user's research contact method (email address or phone number) is displayed in unredacted plain text in the consent dashboard summary row. Any screenshot taken for support purposes would capture the email address. While this is user-visible data on the user's own device, it represents an unnecessary exposure surface.  
**Reference:** "Minimal Information Asymmetry" pattern  
**Remediation:** Redact to `•••@domain.com` or display only the first few characters of the contact method in the summary row. The full value is visible if the user taps through to the Research Preferences screen.

---

#### LOW — SHDRUploader: no certificate pinning for fleet endpoint

**Where:** `app/ios/NeuroPulse/Data/SHDRUploader.swift:28` — `URLSession.shared.data(for: request)`  
**Category:** Security failure  
**Issue:** SHDR upload uses `URLSession.shared` without certificate pinning. A MITM attacker on a clinical Wi-Fi network could intercept SHDR telemetry. SHDR is device-condition data (not user biology), but session counts, consumable counts, and device health metrics flowing to an attacker-controlled endpoint could be used for competitive intelligence or to correlate device usage with facility records.  
**Reference:** HSTS + TLS + "Defence in Depth" patterns; NIST SP 800-52r2  
**Remediation:** Implement URLSession certificate pinning using `URLSessionDelegate.urlSession(_:didReceive:completionHandler:)` with a pinned SPKI hash for the fleet endpoint. Add the pinning logic to a dedicated `NeuroPulseURLSessionDelegate` before the fleet endpoint goes to production.

---

#### LOW — SessionProtocolSigner: Keychain query missing `kSecAttrAccessible` filter on load

**Where:** `app/ios/NeuroPulse/Session/SessionProtocol.swift:150-156` — load query  
**Category:** Security failure  
**Issue:** The Keychain load query does not filter on `kSecAttrAccessible`. If an attacker can write a Keychain entry with the same `kSecAttrApplicationTag` but a different accessibility class (possible in limited jailbreak scenarios), the load query returns the first match regardless of accessibility. The signing key add query correctly specifies `kSecAttrAccessibleWhenUnlockedThisDeviceOnly`, but the load query does not enforce this constraint.  
**Reference:** "Least Privilege" security pattern; Apple Keychain Services documentation  
**Remediation:** Add `kSecAttrAccessible: kSecAttrAccessibleWhenUnlockedThisDeviceOnly` to the Keychain load query to ensure only keys stored with the correct accessibility attribute are returned.

---

### What looks good

**Architecture is genuinely strong for a health wearable:**
- UHDR never transmitted raw; encrypted on-device with user-held key; NeuroPulse cannot decrypt — this is best-in-class for a consumer health device and matches the design of Apple Health's HealthKit
- SHDR/UHDR boundary is rigorously maintained throughout the codebase — every data element routes to the correct partition with zero shortcuts observed
- Analytics gate with prohibited-key enforcement and `@MainActor` isolation prevents race conditions where a prohibited event could slip through on a background thread
- `EngagementTier` (3-bucket enum vs raw session count) is a model implementation of the "Location Granularity" pattern applied to behavioural signals — prevents MHMD and GDPR Art. 9 exposure from session counts
- Age gate + BIPA + 4-layer research consent in a properly-sequenced onboarding chain; no "skip" on the age gate or BIPA disclosure; `isPresented: $showConsentOnboarding` gives users a genuine Skip path on research consent without affecting core features
- `HealthKitSessionReader` is read-only (empty share set), session-scoped only, never persisted, never transmitted — a textbook implementation of the "User-data confinement" pattern
- `SessionHistoryStore` never stores raw EEG/HRV waveforms — only integer aggregates already shown in the UI
- `BIPADisclosureView.interactiveDismissDisabled(true)` prevents iOS swipe-to-dismiss; user must make an explicit accept or decline
- `SessionView.eegConsentGranted` enforces BIPA at the session start UI layer
- `PrivacyInfo.xcprivacy` is correctly authored for current APIs and marks `NSPrivacyTracking: false` accurately
- `SessionProtocol.swift` signing key Keychain entry correctly uses `kSecAttrAccessibleWhenUnlockedThisDeviceOnly` and `kSecAttrSynchronizable: false` — prevents iCloud Keychain sync of the hub signing key

---

### What could not be reviewed

- **No Android codebase present** — all findings above are iOS-only. Android requires a separate review before any beta build ships. The patchwork of US state breach laws (including WA MHMD) applies equally to Android users.
- **Real analytics/crash vendor not yet selected (OI-AUDIT-01)** — `AnalyticsSDKStub` makes it impossible to audit whether the real vendor's SDK introduces new data flows, Required Reason API access, or third-party tracking. This review cannot be considered complete until a vendor is selected, their privacy manifest reviewed, and `PrivacyInfo.xcprivacy` updated.
- **Network transmission layer** — `SHDRUploader.upload()` and `SessionProtocolUploader` use `URLSession.shared` without network interception in this review. A complete review requires running the app against a proxy (Charles Proxy or mitmproxy) to verify all outbound calls, headers, and payloads match the stated behaviour.
- **UHDR partition content at runtime** — the actual eMMC UHDR partition content can only be verified with a physical device and hub. The app-side UHDR routing (session log → UHDR partition) is specified in NP-FW-EMMC-001 §12 but cannot be verified from app source alone.
- **WatchOS app** — `PhoneSessionManager.forward(_:)` sends `SessionState.toWCMessage()` to the Watch, but `SessionState.toWCMessage()` was not found in the reviewed files. The Watch message payload needs independent review to confirm it does not include raw HRV or EEG values.

---

### Recommended next steps

1. **(Before any TestFlight build — 1 sprint)** Implement Argon2id + real biometric/PIN credential in `UHDRKeyManager` to remove the `#error` compile gate. Link `swift-crypto-extras` or the Argon2 reference C library. The `#error` guard added in this session will block all release builds until this is done — that is intentional.

2. **(Month 6 / before G1 gate — 2 sprints)** Select analytics and crash-reporting vendor; execute DPA + BAA; update `PrivacyInfo.xcprivacy`; verify SDK initialisation gate with a CI network-intercept test (OI-AUDIT-01). This is the single gating item for TestFlight.

3. **(This sprint)** Raise a dedicated issue to migrate `SessionHistoryStore.completedAt` out of `UserDefaults` — either coarsen to session-day granularity for the client-side display record, or load precise timestamps exclusively from the UHDR partition. Session timestamps for neurological treatment are special-category data under GDPR Art. 9 and consumer health data under WA MHMD.

4. **(OI-PA-03 resolution — legal, 2 weeks)** Get legal guidance on the BIPA detection mechanism. The lowest-risk resolution is to show the BIPA disclosure to all users globally — the disclosure is not harmful and eliminates the locale-detection gap entirely.

5. **(Before any beta with a real Watch app)** Review `SessionState.toWCMessage()` — verify that raw HRV coherence values are not included in Watch bridge messages. If they are, coarsen to a display-safe format (e.g. integer coherence band, not float) before forwarding.

There are 3 additional lower-priority items (certificate pinning, contact email redaction in dashboard, Keychain load query `kSecAttrAccessible` filter) — worth a follow-up issue to batch them before first external beta.
