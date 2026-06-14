//
//  AnalyticsGateTests.swift
//  NeuroPulseTests
//
//  Subject: AnalyticsGate (app/ios/NeuroPulse/Analytics/AnalyticsGate.swift)
//
//  Verifies:
//  - ISC-92: backend.configure() is never called before the gate opens
//  - ISC-97: prohibited-key events are dropped before reaching the backend
//  - Gate semantics: keys, Skip behaviour, research-consent independence
//  - ISC-157 compliance: this file imports only XCTest + NeuroPulse — no PostHog

import XCTest
@testable import NeuroPulse

@MainActor
final class AnalyticsGateTests: XCTestCase {

    private let consentKey = "np.analytics.consent-granted"
    private var spy: SpyAnalyticsBackend!

    override func setUp() async throws {
        try await super.setUp()
        spy = SpyAnalyticsBackend()
        AnalyticsGate._backend = spy
        AnalyticsGate._resetForTesting()
        UserDefaults.standard.removeObject(forKey: consentKey)
    }

    override func tearDown() async throws {
        // Restore real backend and clear internal gate state so tests don't bleed.
        AnalyticsGate._resetForTesting()
        AnalyticsGate._backend = PostHogAnalyticsBackend()
        UserDefaults.standard.removeObject(forKey: consentKey)
        try await super.tearDown()
    }

    // MARK: - Gate state

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
        UserDefaults.standard.set(true, forKey: "np.onboarding.consent-shown")
        XCTAssertFalse(AnalyticsGate.isOpen,
                       "Gate must not open on consent-shown — only analytics-consent-granted counts")
    }

    func testGateDoesNotOpenOnLegacyOnboardingAcceptedKey() {
        UserDefaults.standard.set(true, forKey: "np.onboarding.consent-accepted")
        XCTAssertFalse(AnalyticsGate.isOpen,
                       "Retired onboarding key must not open the analytics gate")
    }

    func testAnalyticsConsentKeyMatchesConstant() {
        XCTAssertEqual(AnalyticsGate.analyticsConsentKey, "np.analytics.consent-granted")
    }

    func testSkipDoesNotOpenAnalyticsGate() {
        UserDefaults.standard.set(true, forKey: "np.onboarding.consent-shown")
        XCTAssertFalse(AnalyticsGate.isOpen,
                       "Pressing Skip must not open the analytics gate")
    }

    // MARK: - ISC-92: backend.configure() must not fire before gate opens

    func testISC92_backendNotCalledWhenGateClosed() {
        AnalyticsGate.configure()
        XCTAssertEqual(spy.configureCallCount, 0,
                       "ISC-92: backend.configure() must not be called when analytics gate is closed")
    }

    func testISC92_backendCalledOnceWhenGateOpen() {
        UserDefaults.standard.set(true, forKey: consentKey)
        AnalyticsGate.configure()
        XCTAssertEqual(spy.configureCallCount, 1,
                       "ISC-92: backend.configure() must be called exactly once after gate opens")
    }

    func testISC92_backendCalledOnlyOnceOnRepeatConfigure() {
        UserDefaults.standard.set(true, forKey: consentKey)
        AnalyticsGate.configure()
        AnalyticsGate.configure()
        XCTAssertEqual(spy.configureCallCount, 1,
                       "ISC-92: configure() is idempotent — backend must not be called a second time")
    }

    func testISC92_gateStateUnchangedAfterConfigureWithClosedGate() {
        AnalyticsGate.configure()
        XCTAssertFalse(AnalyticsGate.isOpen,
                       "configure() with closed gate must not side-effect the gate state")
        XCTAssertEqual(spy.configureCallCount, 0)
    }

    // MARK: - track() no-ops when gate closed

    func testTrackNoOpsWhenGateClosed() {
        XCTAssertFalse(AnalyticsGate.isOpen)
        AnalyticsGate.track(event: "test_event", properties: ["key": "value"])
        XCTAssertEqual(spy.trackCallCount, 0,
                       "backend.track() must not be called when gate is closed")
    }

    // MARK: - reset()

    func testResetNoOpsWhenNotConfigured() {
        // reset() guards on isConfigured; since setUp calls _resetForTesting,
        // isConfigured is false here. The backend must not receive reset().
        AnalyticsGate.reset()
        XCTAssertEqual(spy.resetCallCount, 0,
                       "backend.reset() must not be called when gate was never configured")
    }

    func testResetCallsBackendWhenConfigured() {
        UserDefaults.standard.set(true, forKey: consentKey)
        AnalyticsGate.configure()
        AnalyticsGate.reset()
        XCTAssertEqual(spy.resetCallCount, 1)
    }

    func testResetAllowsReconfigureAfterwards() {
        UserDefaults.standard.set(true, forKey: consentKey)
        AnalyticsGate.configure()
        AnalyticsGate.reset()
        // Simulate re-consent
        AnalyticsGate.configure()
        XCTAssertEqual(spy.configureCallCount, 2,
                       "After reset(), configure() must be able to call the backend again")
    }

    // MARK: - Research-consent independence

    func testWithdrawBlanketConsentDoesNotRevokeAnalytics() {
        UserDefaults.standard.set(true, forKey: consentKey)
        AnalyticsGate.configure()
        XCTAssertEqual(spy.configureCallCount, 1)

        let store = ConsentStore()
        store.withdrawBlanketConsent()

        XCTAssertTrue(AnalyticsGate.isOpen,
                      "Analytics gate must stay open after research consent withdrawal")
        XCTAssertEqual(spy.resetCallCount, 0,
                       "backend.reset() must not be called on research consent withdrawal")
    }

    // MARK: - Prohibited key enforcement (ISC-97)

    func testTrackDropsEventWithProhibitedKeyEEG() {
        UserDefaults.standard.set(true, forKey: consentKey)
        AnalyticsGate.track(event: "session_start", properties: ["eeg": "some_value"])
        XCTAssertEqual(spy.trackCallCount, 0, "Prohibited key 'eeg' must prevent backend call")
        XCTAssertTrue(AnalyticsGate.isOpen, "Dropping a prohibited event must not close the gate")
    }

    func testTrackDropsEventWithProhibitedKeyHRV() {
        UserDefaults.standard.set(true, forKey: consentKey)
        AnalyticsGate.track(event: "session_end", properties: ["hrv": "80"])
        XCTAssertEqual(spy.trackCallCount, 0)
    }

    func testTrackDropsEventWithDeprecatedSessionCount() {
        UserDefaults.standard.set(true, forKey: consentKey)
        AnalyticsGate.track(event: "app_open", properties: ["session_count": "5"])
        XCTAssertEqual(spy.trackCallCount, 0)
    }

    func testTrackDropsEventWithDeprecatedSessionSequence() {
        UserDefaults.standard.set(true, forKey: consentKey)
        AnalyticsGate.track(event: "app_open", properties: ["session_sequence": "5"])
        XCTAssertEqual(spy.trackCallCount, 0)
    }

    func testTrackDropsEventWithProhibitedKeyRMSSD() {
        UserDefaults.standard.set(true, forKey: consentKey)
        AnalyticsGate.track(event: "session_end", properties: ["rmssd": "42"])
        XCTAssertEqual(spy.trackCallCount, 0)
    }

    func testTrackDropsEventWithProhibitedKeyCoherence() {
        UserDefaults.standard.set(true, forKey: consentKey)
        AnalyticsGate.track(event: "session_end", properties: ["coherence": "8.5"])
        XCTAssertEqual(spy.trackCallCount, 0)
    }

    func testTrackDropsEventWithProhibitedKeySessionID() {
        UserDefaults.standard.set(true, forKey: consentKey)
        AnalyticsGate.track(event: "session_end", properties: ["session_id": "abc123"])
        XCTAssertEqual(spy.trackCallCount, 0)
    }

    func testTrackDropsEventWithProhibitedKeyProtocolID() {
        UserDefaults.standard.set(true, forKey: consentKey)
        AnalyticsGate.track(event: "session_end", properties: ["protocol_id": "p1"])
        XCTAssertEqual(spy.trackCallCount, 0)
    }

    func testTrackDropsEventWithProhibitedImpedanceKeys() {
        UserDefaults.standard.set(true, forKey: consentKey)
        AnalyticsGate.track(event: "setup", properties: ["imp": "0xFF"])
        XCTAssertEqual(spy.trackCallCount, 0, "Prohibited key 'imp' must prevent backend call")
    }

    // MARK: - Allowed events reach the backend

    func testTrackAllowsEventWithSafeProperties() {
        UserDefaults.standard.set(true, forKey: consentKey)
        AnalyticsGate.track(event: "app_open", properties: [
            "engagement_tier": "active",
            "screen_name": "home"
        ])
        XCTAssertEqual(spy.trackCallCount, 1)
        XCTAssertEqual(spy.trackedEvents.first?.event, "app_open")
        XCTAssertEqual(spy.trackedEvents.first?.properties["engagement_tier"], "active")
    }

    func testTrackAllowsEventWithEmptyProperties() {
        UserDefaults.standard.set(true, forKey: consentKey)
        AnalyticsGate.track(event: "app_foregrounded", properties: [:])
        XCTAssertEqual(spy.trackCallCount, 1)
    }

    func testEngagementTierIsAllowed() {
        UserDefaults.standard.set(true, forKey: consentKey)
        AnalyticsGate.track(event: "app_open", properties: ["engagement_tier": "new"])
        XCTAssertEqual(spy.trackCallCount, 1,
                       "engagement_tier is the approved replacement for session_count — must be allowed")
    }

    func testMultipleSafeEventsAccumulate() {
        UserDefaults.standard.set(true, forKey: consentKey)
        AnalyticsGate.track(event: "e1", properties: [:])
        AnalyticsGate.track(event: "e2", properties: [:])
        XCTAssertEqual(spy.trackCallCount, 2)
        XCTAssertEqual(spy.trackedEvents.map(\.event), ["e1", "e2"])
    }

    // MARK: - ISC-157 static assertion (compile-time)
    //
    // This test file imports only `XCTest` and `@testable import NeuroPulse`.
    // It does NOT import `PostHog`. The SpyAnalyticsBackend in SpyAnalyticsBackend.swift
    // also does not import PostHog. No test file touches the PostHog SDK directly.
    // Verified by: grep -rn "^import PostHog" NeuroPulseTests/ — must return zero lines.
    func testISC157_noDirectPostHogImportInTestTarget() throws {
        let testDir = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent()  // NeuroPulseTests/
        let fm = FileManager.default
        guard let enumerator = fm.enumerator(at: testDir,
                                             includingPropertiesForKeys: nil) else {
            XCTFail("Could not enumerate test directory")
            return
        }
        var violations: [String] = []
        for case let url as URL in enumerator where url.pathExtension == "swift" {
            let source = try String(contentsOf: url, encoding: .utf8)
            // Match `import PostHog` as a standalone import statement on its own line,
            // ignoring comments and `@testable import`.
            let lines = source.components(separatedBy: .newlines)
            for line in lines {
                let trimmed = line.trimmingCharacters(in: .whitespaces)
                if trimmed == "import PostHog" {
                    violations.append(url.lastPathComponent)
                }
            }
        }
        XCTAssertTrue(violations.isEmpty,
                      "ISC-157: test files must not import PostHog directly. Violations: \(violations)")
    }
}
