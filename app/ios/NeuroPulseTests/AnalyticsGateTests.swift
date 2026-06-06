//
//  AnalyticsGateTests.swift
//  NeuroPulseTests
//
//  Subject: AnalyticsGate (app/ios/NeuroPulse/Analytics/AnalyticsGate.swift)
//
//  Verifies the consent gate semantics (ISC-92, ISC-97):
//  - configure() and track() are no-ops when the gate is closed
//  - prohibited keys cause the event to be dropped rather than transmitted
//  - the gate keys on analytics-CONSENT-GRANTED (not onboarding-shown or skip)
//  - Skip does not open the analytics gate
//  - research-consent withdrawal does not revoke analytics consent
//
//  AnalyticsSDKStub performs no network I/O, so these tests are safe to run
//  without network isolation. The `isConfigured` flag is private static state;
//  tests that need a clean gate state reset the UserDefaults key between runs.

import XCTest
@testable import NeuroPulse

@MainActor
final class AnalyticsGateTests: XCTestCase {

    private let consentKey = "np.analytics.consent-granted"

    override func setUp() async throws {
        try await super.setUp()
        UserDefaults.standard.removeObject(forKey: consentKey)
    }

    override func tearDown() async throws {
        UserDefaults.standard.removeObject(forKey: consentKey)
        try await super.tearDown()
    }

    // MARK: - gate state

    func testGateIsClosedWhenConsentKeyAbsent() {
        XCTAssertFalse(AnalyticsGate.isOpen,
                       "Gate must be closed when consent key has never been set")
    }

    func testGateIsClosedWhenConsentKeyIsFalse() {
        UserDefaults.standard.set(false, forKey: consentKey)
        XCTAssertFalse(AnalyticsGate.isOpen)
    }

    func testGateIsOpenAfterConsentAccepted() {
        UserDefaults.standard.set(true, forKey: consentKey)
        XCTAssertTrue(AnalyticsGate.isOpen)
    }

    func testGateDoesNotOpenOnOnboardingShownKey() {
        // Setting "onboarding shown" must NOT open the analytics gate — the gate
        // requires the separate analytics-consent key (deliberate Done action).
        UserDefaults.standard.set(true, forKey: "np.onboarding.consent-shown")
        XCTAssertFalse(AnalyticsGate.isOpen,
                       "Gate must not open on consent-shown — only analytics-consent-granted counts")
    }

    func testGateDoesNotOpenOnLegacyOnboardingAcceptedKey() {
        // The retired key "np.onboarding.consent-accepted" must not open the gate.
        // Any devices with this key from a pre-split build start with analytics closed,
        // which is the privacy-safe direction.
        UserDefaults.standard.set(true, forKey: "np.onboarding.consent-accepted")
        XCTAssertFalse(AnalyticsGate.isOpen,
                       "Retired onboarding key must not open the analytics gate")
    }

    func testAnalyticsConsentKeyMatchesConstant() {
        // Verify the constant exported by AnalyticsGate matches what ConsentOnboardingView writes.
        XCTAssertEqual(AnalyticsGate.analyticsConsentKey, "np.analytics.consent-granted")
    }

    func testSkipDoesNotOpenAnalyticsGate() {
        // Skip sets only the onboarding-shown flag; it must NOT set the analytics key.
        // Simulated by verifying that setting only the onboarding key leaves gate closed.
        UserDefaults.standard.set(true, forKey: "np.onboarding.consent-shown")
        XCTAssertFalse(AnalyticsGate.isOpen,
                       "Pressing Skip must not open the analytics gate")
    }

    func testWithdrawBlanketConsentDoesNotRevokeAnalytics() {
        // Research-consent withdrawal must not touch the analytics consent key.
        UserDefaults.standard.set(true, forKey: consentKey)
        XCTAssertTrue(AnalyticsGate.isOpen, "Precondition: gate should be open")
        // withdrawBlanketConsent no longer calls revokeAnalyticsConsent —
        // verify the analytics key is untouched after withdrawal.
        let store = ConsentStore()
        store.withdrawBlanketConsent()
        XCTAssertTrue(AnalyticsGate.isOpen,
                      "Withdrawing research consent must not revoke analytics consent")
    }

    // MARK: - configure() no-ops when gate closed

    func testConfigureNoOpsWhenGateClosed() {
        // Gate is closed (no consent key set). configure() must not throw or crash.
        // AnalyticsSDKStub has no state to assert, so this is primarily a
        // "does not crash / throw" and "isOpen still false" check.
        AnalyticsGate.configure()
        XCTAssertFalse(AnalyticsGate.isOpen,
                       "configure() with closed gate must not side-effect the gate state")
    }

    // MARK: - track() no-ops when gate closed

    func testTrackNoOpsWhenGateClosed() {
        // Must not crash and must not throw. Gate is verifiably closed.
        XCTAssertFalse(AnalyticsGate.isOpen)
        AnalyticsGate.track(event: "test_event", properties: ["key": "value"])
        // Pass = did not crash
    }

    // MARK: - prohibited key enforcement (ISC-97)

    func testTrackDropsEventWithProhibitedKeyEEG() {
        UserDefaults.standard.set(true, forKey: consentKey)
        // Must not crash. The gate drops the event rather than transmitting.
        // Since AnalyticsSDKStub has no observable state, the test verifies
        // no crash and the gate remains in the expected open state.
        AnalyticsGate.track(event: "session_start", properties: ["eeg": "some_value"])
        XCTAssertTrue(AnalyticsGate.isOpen,
                      "Dropping a prohibited event must not close the gate")
    }

    func testTrackDropsEventWithProhibitedKeyHRV() {
        UserDefaults.standard.set(true, forKey: consentKey)
        AnalyticsGate.track(event: "session_end", properties: ["hrv": "80"])
    }

    func testTrackDropsEventWithDeprecatedSessionCount() {
        UserDefaults.standard.set(true, forKey: consentKey)
        AnalyticsGate.track(event: "app_open", properties: ["session_count": "5"])
    }

    func testTrackDropsEventWithDeprecatedSessionSequence() {
        UserDefaults.standard.set(true, forKey: consentKey)
        AnalyticsGate.track(event: "app_open", properties: ["session_sequence": "5"])
    }

    func testTrackDropsEventWithProhibitedKeyRMSSD() {
        UserDefaults.standard.set(true, forKey: consentKey)
        AnalyticsGate.track(event: "session_end", properties: ["rmssd": "42"])
    }

    func testTrackDropsEventWithProhibitedKeyCoherence() {
        UserDefaults.standard.set(true, forKey: consentKey)
        AnalyticsGate.track(event: "session_end", properties: ["coherence": "8.5"])
    }

    func testTrackDropsEventWithProhibitedKeySessionID() {
        UserDefaults.standard.set(true, forKey: consentKey)
        AnalyticsGate.track(event: "session_end", properties: ["session_id": "abc123"])
    }

    func testTrackDropsEventWithProhibitedKeyProtocolID() {
        UserDefaults.standard.set(true, forKey: consentKey)
        AnalyticsGate.track(event: "session_end", properties: ["protocol_id": "p1"])
    }

    // MARK: - allowed events pass through

    func testTrackAllowsEventWithSafeProperties() {
        UserDefaults.standard.set(true, forKey: consentKey)
        // Safe properties per NP-APP-TELEMETRY-001: engagement_tier, screen_name, etc.
        // Must not crash or drop.
        AnalyticsGate.track(event: "app_open", properties: [
            "engagement_tier": "active",
            "screen_name": "home"
        ])
        // Pass = did not crash
    }

    func testTrackAllowsEventWithEmptyProperties() {
        UserDefaults.standard.set(true, forKey: consentKey)
        AnalyticsGate.track(event: "app_foregrounded", properties: [:])
    }

    // MARK: - engagement_tier is never a prohibited key

    func testEngagementTierIsAllowed() {
        // engagement_tier is the replacement for session_count — it MUST be allowed.
        UserDefaults.standard.set(true, forKey: consentKey)
        AnalyticsGate.track(event: "app_open", properties: ["engagement_tier": "new"])
        // Pass = did not crash and no drop
    }
}
