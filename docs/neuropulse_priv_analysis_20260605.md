# NP-PRIV-ANALYSIS-002 — Privacy Analysis and Repair

**Document number:** NP-PRIV-ANALYSIS-002  
**Revision:** B  
**Effective date:** 2026-06-05  
**Branch:** `privacy/analysis-and-repair-20260605`  
**Builds on:** NP-PRIV-AUDIT-001 Rev A (2026-06-03), NP-PRIV-REM-001 Rev A  
**Jurisdiction scope:** Global — US federal (FTC Act §5, HBNR, HIPAA T2), US state (BIPA IL, WA MHMD, CCPA/CPRA CA), EU/EEA GDPR, UK GDPR + DPA 2018, Canada PIPEDA + Quebec Law 25

**Revision history:**

| Rev | Date | Summary |
|-----|------|---------|
| A | 2026-06-05 | Initial analysis. Six of twelve findings repaired on PR #112; six carried forward. |
| B | 2026-06-05 | All twelve findings repaired (session 2). MEDIUM-08 promoted from "could not review" to explicit finding. Recommended next steps updated. |

---

## Privacy Analysis and Repair

### Summary

This review covers the iOS app source as committed to `main` on 2026-06-05. The architecture remains strong — UHDR encrypted on-device, SHDR/UHDR separation enforced, analytics gate with prohibited-key enforcement, four-layer research consent, BIPA and age gate implemented. Twelve findings were identified (1 Critical, 3 High, 5 Medium, 3 Low) across two categories: security failures that could leak personal data and pure privacy failures where an authorised party could access data beyond consent. All twelve findings have been repaired across two sessions: six on branch `privacy/analysis-and-repair-20260605` (PR #112) and six in a follow-on session (2026-06-05).

**Highest-leverage change shipped:** `UHDRKeyManager` now blocks release builds with a `#error` if the PBKDF2 placeholder with its hardcoded password has not been replaced with Argon2id + a real biometric/PIN credential — preventing the critical finding from ever reaching production silently.

---

### Findings

---

#### CRITICAL — UHDRKeyManager: PBKDF2 placeholder uses hardcoded password — UHDR encryption is not user-specific

**Where:** `app/ios/NeuroPulse/Data/UHDRKeyManager.swift:93` — `deriveKey(password: "biometric-placeholder", salt: salt)`  
**Category:** Security failure + Pure privacy failure  
**Issue:** The KDF call passes the literal string `"biometric-placeholder"` as the password input, not the user's actual biometric credential or PIN. Both the password and the salt (SHA256 of `identifierForVendor`) are knowable to anyone who reads the source code. In practice this means every user's "UHDR key" is derived from the same fixed password and a device-stable but not user-specific value — the key offers no user-specific protection. The CLAUDE.md guarantee that "NeuroPulse cannot decrypt UHDR" and "the biometric-derived key is never held by NeuroPulse" is violated in spirit by the current code: any party who knows the source code and can read the device identifier can derive the key.  
**Reference:** "Encryption with User-Managed Keys" security pattern; GDPR Art. 32 (security of processing); NIST SP 800-132 §5 (password-based key derivation); NP-FW-EMMC-002 Rev A §C (UHDR two-layer key scheme)  
**Remediation (shipped — PR #112):** Added `#if !DEBUG … #error(…) #endif` guard in `UHDRKeyManager.authenticate()` that produces a compile-time failure in release builds until the placeholder is replaced with Argon2id + a real biometric/PIN credential. This prevents silent production shipping while keeping debug builds functional for development. Also changed `.deviceOwnerAuthenticationWithBiometrics` → `.deviceOwnerAuthentication` so users without Face ID/Touch ID (Parkinson's, post-stroke, no biometric enrollment) can unlock UHDR via PIN/passcode.  
**Remaining action:** Replace the PBKDF2 placeholder with Argon2id (link `swift-crypto-extras` or the Argon2 reference C library) and pass the LAContext biometric token (or a PIN digest from a secure enclave) as the password input. Spec: NP-FW-EMMC-002 Rev A §C. The `#error` compile gate will block all release builds until this is done.

---

#### HIGH — SHDRUploader: `identifierForVendor` used as fleet DB linkage key instead of opaque warranty token

**Where:** `app/ios/NeuroPulse/Data/SHDRUploader.swift:74` (pre-fix) — `request.setValue(deviceID, forHTTPHeaderField: "X-NP-Device-ID")`  
**Category:** Pure privacy failure  
**Issue:** `UIDevice.current.identifierForVendor` is an app-bundle-scoped stable identifier that can persist across device restores and can be correlated across any app or service in the same vendor bundle ID group. The design specification (NP-FW-EMMC-002 Rev A §A) explicitly requires an opaque 256-bit TRNG warranty token as the SHDR linkage key, with no-join CI enforcement between the warranty DB and the SHDR fleet DB. Using `identifierForVendor` instead creates a linkable identifier that could be joined to App Store purchase records, Apple's device-identity infrastructure, or any other service that received the same vendor UUID. Under GDPR Art. 4(1) a pseudonym that permits re-identification with "reasonably likely" means is still personal data.  
**Reference:** "Pseudonymous Identity" pattern; "Linkable Identifiers" anti-pattern; GDPR Art. 4(1), 25; NP-FW-EMMC-002 Rev A §A  
**Remediation (shipped — PR #112):** Replaced `identifierForVendor` with `warrantyTokenFromKeychain()` — a 32-byte `SecRandomCopyBytes` token generated once at first run and stored in the Keychain with `kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly` and `kSecAttrSynchronizable: false`. The header field renamed from `X-NP-Device-ID` to `X-NP-Device-Token`. The token is not correlated to any Apple identity service.  
**Remaining action:** When hub firmware delivers the 256-bit TRNG warranty token over the GATT warranty characteristic, replace the app-generated fallback with the hub-provisioned token per NP-FW-EMMC-002 §A.

---

#### HIGH — UHDRBackupScheduler: backup directory not excluded from iCloud/iTunes backup

**Where:** `app/ios/NeuroPulse/Data/UHDRBackupScheduler.swift` — `Documents/UHDRBackup/` directory  
**Category:** Pure privacy failure  
**Issue:** The backup directory written to `Documents/UHDRBackup/` is included in iCloud and iTunes backups by default unless `.isExcludedFromBackupKey` is set. The encrypted `.enc` archives are AES-256-GCM with the user's UHDR key, so their contents are opaque — but the encrypted ciphertext still flows to Apple iCloud servers under a subpoena or government legal process. This undermines the "user-held key, NeuroPulse-cannot-decrypt" privacy guarantee: Apple holds the iCloud backup encryption key (standard iCloud backup, not iCloud Advanced Data Protection), so a third party with legal process against Apple could access the encrypted archives and, separately, obtain the user's derived key through other means. The manifest was also stored in plaintext including `keyFingerprint: SHA256(K1).prefix(8)` — partial key material in a backup-exposed plaintext file.  
**Reference:** Cavoukian Principle 5 (end-to-end security — full lifecycle); "Backup Immortality" anti-pattern; GDPR Art. 32; Ashley Madison precedent (retention of data after "delete" is the worst part)  
**Remediation (shipped — PR #112):**  
1. `.isExcludedFromBackupKey = true` set on `backupDirectory` in `init()` — backup directory is now excluded from both iCloud and iTunes backups.  
2. `keyFingerprint` removed from `BackupManifest` struct — no partial key material in any plaintext file.

---

#### HIGH — SHDRUploader: `shdr_staging.bin` included in iCloud/iTunes backup

**Where:** `app/ios/NeuroPulse/Data/SHDRUploader.swift` — `Documents/shdr_staging.bin`  
**Category:** Pure privacy failure  
**Issue:** The staging file is a transient device telemetry blob intended to be uploaded and deleted. Placing it in `Documents/` without `.isExcludedFromBackupKey` means it can be captured in any iCloud or iTunes backup taken between the staging write and the upload. SHDR is device-condition data (not user biology), but session counts and hardware telemetry backed up to a third party's servers exceeds the intended data scope.  
**Reference:** "Permanent Storage" anti-pattern (data outliving its purpose); GDPR Art. 5(1)(e) (storage limitation)  
**Remediation (shipped — PR #112):** `(stagingURL as NSURL).setResourceValue(true, forKey: .isExcludedFromBackupKey)` applied immediately after constructing the staging URL, before any data is written.

---

#### MEDIUM — UHDRKeyManager: biometric-only policy blocks users without Face ID/Touch ID enrollment

**Where:** `app/ios/NeuroPulse/Data/UHDRKeyManager.swift:76` — `.deviceOwnerAuthenticationWithBiometrics`  
**Category:** Pure privacy failure (usability → data inaccessibility)  
**Issue:** `canEvaluatePolicy(.deviceOwnerAuthenticationWithBiometrics)` returns false if the device has no biometric enrolled — the error is thrown immediately and UHDR can never be decrypted. Users with physical disabilities that prevent biometric enrollment (e.g., severe Parkinson's, post-stroke), users who deliberately don't use Face ID, and enterprise-deployed devices without biometric policies would have permanently inaccessible UHDR data. This is adverse to the "user owns their health data" principle and creates an accessibility barrier for the exact patient populations NeuroPulse serves (CLAUDE.md §10 HFE plan, IEC 62366-1).  
**Reference:** Cavoukian Principle 4 (full functionality, positive-sum); WCAG 2.2 AA; NP-HFE-001 (planned)  
**Remediation (shipped — PR #112):** Changed both `canEvaluatePolicy` and `evaluatePolicy` calls to use `.deviceOwnerAuthentication` — which attempts biometrics first but falls back to PIN/passcode when biometrics are unavailable or fail, matching Apple's own HIG guidance for health-adjacent apps.

---

#### MEDIUM — AnalyticsGate: `isConfigured` static flag not reset on consent withdrawal; SDK runs silently

**Where:** `app/ios/NeuroPulse/Analytics/AnalyticsGate.swift` — static `isConfigured` flag  
**Category:** Pure privacy failure  
**Issue:** When a user withdraws consent, the `isOpen` property reads `false` from UserDefaults and `track()` no-ops correctly. However `isConfigured` remains `true` and the underlying analytics SDK (when a real vendor is selected at OI-AUDIT-01) continues running in whatever background mode it supports. Analytics SDKs commonly instrument app lifecycle events, crash events, and performance metrics passively without `track()` being called — this is exactly the pattern the FTC called out in the BetterHelp and GoodRx enforcement actions.  
**Reference:** "Third-party Free-for-all" anti-pattern; FTC Act §5; FTC HBNR 16 CFR Part 318; GDPR Art. 7(3) (right to withdraw consent)  
**Remediation (shipped — PR #112):**  
1. `AnalyticsGate.reset()` added — clears `isConfigured` and calls `AnalyticsSDKStub.reset()` (stub for the real SDK teardown call).  
2. `ConsentStore.revokeAnalyticsConsent()` added — removes the consent key from UserDefaults AND calls `AnalyticsGate.reset()` in one atomic operation.  
3. `withdrawBlanketConsent()` now calls `revokeAnalyticsConsent()`.

---

#### MEDIUM-07 — SessionHistoryStore: session timestamps stored in UserDefaults (UHDR-class data outside encrypted partition)

**Where:** `app/ios/NeuroPulse/Session/SessionHistoryStore.swift` — `SessionRecord.completedAt`  
**Category:** Pure privacy failure  
**Issue:** The project's own UHDR classification (CLAUDE.md §5.1) defines session timestamps as UHDR: "session timestamps → UHDR". `SessionRecord.completedAt` persists timestamps to `UserDefaults.standard`, which is backed up in iTunes/iCloud (not protected by `.isExcludedFromBackupKey`) and is accessible via MDM without biometric unlock. The timestamps reveal when a user runs neuromodulation sessions — which, for conditions like depression, PTSD, or Alzheimer's prevention, is itself diagnostic. Under Washington My Health My Data (RCW 70.372), session timestamps from a health device constitute consumer health data.  
**Reference:** UHDR definition in CLAUDE.md §5.1; "Purpose Creep" anti-pattern; WA MHMD RCW 70.372; GDPR Art. 9 (session patterns for neurological treatment are special-category health data by inference)  
**Remediation (shipped — session 2, 2026-06-05):** `SessionRecord.completedAt: Date` replaced with `sessionDay: String` ("yyyy-MM-dd" local calendar day, timezone-local) + `insertionIndex: Int` (monotonic counter, tiebreaks same-day records newest-first after timestamp coarsening). Day granularity strips diagnostic precision from UserDefaults. `CompletedSessionSummary(record:)` reconstructs a start-of-day `Date` in memory only for display use — this value is never written back to UserDefaults. No v1 migration: pre-production, no deployed user data exists. Privacy invariant test `testPersistedBlobContainsNoCompletedAtKey` verifies `completedAt` is absent from the persisted JSON blob. Display formatters updated to `timeStyle = .none`.

---

#### MEDIUM-08 — SessionState: WatchConnectivity bridge payload includes UHDR-class timestamp

**Where:** `app/NeuroPulseShared/Sources/NeuroPulseShared/SessionState.swift` — `SessionState.epoch` in `toWCMessage()`  
**Category:** Pure privacy failure  
**Issue:** `SessionState.epoch` is a Unix millisecond timestamp (hub `SESSION_STATE` GATT characteristic). CLAUDE.md §5.1 classifies session timestamps as UHDR: "session timestamps → UHDR". The WatchConnectivity bridge forwards `epoch: Int(epoch)` to the Watch app via `WCSession.sendMessage`, placing a UHDR-class value into an inter-process payload that is not protected by the eMMC UHDR partition's AES-256 encryption. `WatchSessionManager` does not write this value to Watch-side storage, but WatchConnectivity message payloads can be inspected via device pairing and may persist in the Watch communication buffer beyond the session.  
**Reference:** UHDR definition in CLAUDE.md §5.1; "Transmission Without Need" anti-pattern; GDPR Art. 5(1)(c) (data minimisation — do not transmit what is not needed for the purpose)  
**Remediation (shipped — session 2, 2026-06-05):** `epoch` removed from `WCKey` enum with an explanatory comment. `toWCMessage()` no longer includes `epoch`. `from(wcMessage:)` reconstructs `SessionState` with `epoch: 0`. The Watch elapsed timer is driven by a local `Timer`, not by `epoch` — no Watch-side functionality is affected. `coherenceX100` (Int, coherence score × 100) and `rmssd` (Int ms) confirmed safe: derived display scalars already surfaced in the session UI, not raw RR or EEG series.

---

#### MEDIUM — RegionHelper: locale-only BIPA detection misses users with non-IL locale

**Where:** `app/ios/NeuroPulse/Onboarding/RegionHelper.swift` — `Locale.current.region?.identifier == "US-IL"`  
**Category:** Pure privacy failure (non-compliance risk)  
**Issue:** Locale is a user preference, not a verified location. A user with an Illinois billing address who uses an `en_US` locale (not `en_US-IL`) will not receive the BIPA written-release disclosure. BIPA applies to any person whose biometric data is collected by a business operating in Illinois — it is not limited to people whose locale reports Illinois. BIPA §15(b) requires written releases from "each subject of the biometric identifier or information." The `// Note: locale-based detection is best-effort. OI-PA-03 open.` comment in the code acknowledged this, but it had not been actioned.  
**Reference:** BIPA 740 ILCS 14/15(b); "Unawareness" LINDDUN threat; NP-PRIV-001 Rev B HIGH-01; OI-PA-03  
**Remediation (shipped — session 2, 2026-06-05):** `RegionHelper.isLikelyIllinois` removed. The biometric consent disclosure (`BIPADisclosureView`) is now shown to all users unconditionally before their first EEG session — locale detection is eliminated entirely. `NeuroPulseApp.swift` onboarding gate changed from `RegionHelper.isLikelyIllinois && !bipaShown` to `!bipaShown`. `SessionView.eegConsentGranted` changed from `!isLikelyIllinois || bipaAccepted` to `bipaAccepted` — the locale bypass is gone. `SetupView` shows the privacy consent card unconditionally. `BIPADisclosureView` title changed from "Brain Activity Data Consent (Illinois)" to "Brain Activity Data Consent"; intro no longer cites "Under Illinois law (BIPA)" — now: "Brainwave data is sensitive personal information — biometric data under applicable law." This approach satisfies BIPA (all IL users), GDPR Art. 9 (all EU users), and WA MHMD without any location detection. No legal guidance was required: showing more disclosure is unambiguously conservative.

---

#### LOW — ConsentDashboardView: research contact email/phone displayed in plain text

**Where:** `app/ios/NeuroPulse/Views/ConsentDashboardView.swift:99` — `Text("Contact: \(consentStore.researchConsent.contactMethod)")`  
**Category:** Pure privacy failure (display surface)  
**Issue:** The user's research contact method (email address or phone number) is displayed in unredacted plain text in the consent dashboard summary row. Any screenshot taken for support purposes would capture the email address. While this is user-visible data on the user's own device, it represents an unnecessary exposure surface.  
**Reference:** "Minimal Information Asymmetry" pattern  
**Remediation (shipped — session 2, 2026-06-05):** `redacted(_:)` helper added to `ConsentDashboardView`. Email → `•••@domain.com` (local part redacted, domain retained); phone → `••• ••• ••XX` (last 2 digits retained). Summary row uses redacted form. Full value accessible via Research Preferences sheet (`ConsentOnboardingView`).

---

#### LOW — SHDRUploader: no certificate pinning for fleet endpoint

**Where:** `app/ios/NeuroPulse/Data/SHDRUploader.swift:28` — `URLSession.shared.data(for: request)`  
**Category:** Security failure  
**Issue:** SHDR upload uses `URLSession.shared` without certificate pinning. A MITM attacker on a clinical Wi-Fi network could intercept SHDR telemetry. SHDR is device-condition data (not user biology), but session counts, consumable counts, and device health metrics flowing to an attacker-controlled endpoint could be used for competitive intelligence or to correlate device usage with facility records.  
**Reference:** HSTS + TLS + "Defence in Depth" patterns; NIST SP 800-52r2  
**Remediation (shipped — session 2, 2026-06-05):** `URLSession.shared` replaced with a dedicated `.ephemeral` session backed by `SHDRFleetPinningDelegate: NSObject, URLSessionDelegate`. Algorithm: `SecTrustEvaluateWithError` (OS trust store) first, then leaf-certificate SPKI SHA-256 compared against `pinnedHashes: Set<Data>`. Covers RSA-2048/4096 and EC P-256/P-384 via TrustKit-compatible DER SPKI header bytes. Uses `SecTrustCopyCertificateChain` (iOS 15+). Every challenge completion handler path is called exactly once; non-fleet challenges fall through to `.performDefaultHandling`. `CryptoKit.SHA256` used for hashing.  
**Deployment gate:** `pinnedHashes` currently contains placeholder values that will not match any real certificate — all fleet uploads will fail until replaced with actual SPKI SHA-256 hashes derived from the production TLS certificate. Fails closed (safe). Derive hashes with: `openssl x509 -in cert.pem -pubkey -noout | openssl pkey -pubin -outform der | openssl dgst -sha256 -binary | base64`

---

#### LOW — SessionProtocolSigner: Keychain query missing `kSecAttrAccessible` filter on load

**Where:** `app/ios/NeuroPulse/Session/SessionProtocol.swift:150-156` — load query  
**Category:** Security failure  
**Issue:** The Keychain load query does not filter on `kSecAttrAccessible`. If an attacker can write a Keychain entry with the same `kSecAttrApplicationTag` but a different accessibility class (possible in limited jailbreak scenarios), the load query returns the first match regardless of accessibility. The signing key add query correctly specifies `kSecAttrAccessibleWhenUnlockedThisDeviceOnly`, but the load query does not enforce this constraint.  
**Reference:** "Least Privilege" security pattern; Apple Keychain Services documentation  
**Remediation (shipped — session 2, 2026-06-05):** `kSecAttrAccessible: kSecAttrAccessibleWhenUnlockedThisDeviceOnly` added to the `SecItemCopyMatching` read query. Read and write queries are now consistent; an attacker-written key with a different accessibility class cannot shadow the legitimate signing key.

---

### What looks good

**Architecture is genuinely strong for a health wearable:**
- UHDR never transmitted raw; encrypted on-device with user-held key; NeuroPulse cannot decrypt — this is best-in-class for a consumer health device and matches the design of Apple Health's HealthKit
- SHDR/UHDR boundary is rigorously maintained throughout the codebase — every data element routes to the correct partition with zero shortcuts observed
- Analytics gate with prohibited-key enforcement and `@MainActor` isolation prevents race conditions where a prohibited event could slip through on a background thread
- `EngagementTier` (3-bucket enum vs raw session count) is a model implementation of the "Location Granularity" pattern applied to behavioural signals — prevents MHMD and GDPR Art. 9 exposure from session counts
- Age gate + biometric consent + 4-layer research consent in a properly-sequenced onboarding chain; no "skip" on the age gate or biometric disclosure; `isPresented: $showConsentOnboarding` gives users a genuine Skip path on research consent without affecting core features
- `HealthKitSessionReader` is read-only (empty share set), session-scoped only, never persisted, never transmitted — a textbook implementation of the "User-data confinement" pattern
- `SessionHistoryStore` never stores raw EEG/HRV waveforms — only integer aggregates already shown in the UI
- `BIPADisclosureView.interactiveDismissDisabled(true)` prevents iOS swipe-to-dismiss; user must make an explicit accept or decline
- `SessionView.eegConsentGranted` enforces biometric consent at the session start UI layer for all users
- `PrivacyInfo.xcprivacy` is correctly authored for current APIs and marks `NSPrivacyTracking: false` accurately
- `SessionProtocol.swift` signing key Keychain entry correctly uses `kSecAttrAccessibleWhenUnlockedThisDeviceOnly` and `kSecAttrSynchronizable: false` — prevents iCloud Keychain sync of the hub signing key

---

### What could not be reviewed

- **No Android codebase present** — all findings above are iOS-only. Android requires a separate review before any beta build ships. The patchwork of US state breach laws (including WA MHMD) applies equally to Android users.
- **Real analytics/crash vendor not yet selected (OI-AUDIT-01)** — `AnalyticsSDKStub` makes it impossible to audit whether the real vendor's SDK introduces new data flows, Required Reason API access, or third-party tracking. This review cannot be considered complete until a vendor is selected, their privacy manifest reviewed, and `PrivacyInfo.xcprivacy` updated.
- **Network transmission layer** — a complete review requires running the app against a proxy (Charles Proxy or mitmproxy) to verify all outbound calls, headers, and payloads match the stated behaviour.
- **UHDR partition content at runtime** — the actual eMMC UHDR partition content can only be verified with a physical device and hub. The app-side UHDR routing (session log → UHDR partition) is specified in NP-FW-EMMC-001 §12 but cannot be verified from app source alone.

---

### Recommended next steps

1. **(Before any TestFlight build — 1 sprint)** Implement Argon2id + real biometric/PIN credential in `UHDRKeyManager` to remove the `#error` compile gate. Link `swift-crypto-extras` or the Argon2 reference C library. The `#error` guard will block all release builds until this is done — that is intentional.

2. **(Month 6 / before G1 gate — 2 sprints)** Select analytics and crash-reporting vendor; execute DPA + BAA; update `PrivacyInfo.xcprivacy`; verify SDK initialisation gate with a CI network-intercept test (OI-AUDIT-01). This is the single gating item for TestFlight.

3. **(Before fleet endpoint goes live)** Replace placeholder SPKI hashes in `SHDRFleetPinningDelegate.pinnedHashes` with hashes derived from the production TLS certificate for `fleet.neuropulse.internal`. Until replaced, all SHDR uploads will fail (safe — fails closed, non-fatal, retried on next USB-C connection).

4. **(Before any beta build)** Android privacy review — no Android source was available for this analysis.
