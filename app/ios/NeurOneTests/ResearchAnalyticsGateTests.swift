//
//  ResearchAnalyticsGateTests.swift
//  NeurOneTests
//
//  Subject: ResearchAnalyticsGate (app/ios/NeurOne/Analytics/ResearchAnalyticsGate.swift)
//
//  Verifies:
//  - ISC-92: backend.configure() is never called before the research consent gate opens
//  - ISC-97: an event off the permitted list (NP-APP-TELEMETRY-001 §3) is dropped before reaching the backend
//  - Gate semantics: keys, Skip behavior, blanket research consent coupling
//  - ISC-157 compliance: this file imports only XCTest + NeurOne — no PostHog

import XCTest
@testable import NeurOne

@MainActor
final class ResearchAnalyticsGateTests: XCTestCase {

    private let consentKey = "np.research.consent-granted"
    private var spy: SpyAnalyticsBackend!
    private var erasureDir: URL!

    override func setUp() async throws {
        try await super.setUp()
        spy = SpyAnalyticsBackend()
        ResearchAnalyticsGate._backend = spy
        // A withdrawal queues the old analytics ID for erasure (REQ-SW03-19): give the gate a queue that keeps it in a temporary
        // folder and asks a fake relay, so no test writes to the real queue or makes a network call.
        erasureDir = FileManager.default.temporaryDirectory.appendingPathComponent("gate-erasure-\(UUID().uuidString)")
        ResearchAnalyticsGate._erasureQueue = AnalyticsErasureQueue(
            store: ProtectedDefaults(directory: erasureDir, legacy: UserDefaults(suiteName: "gate-erasure-\(UUID().uuidString)")!),
            relay: FakeErasureRelay(outcome: .retryLater)
        )
        ResearchAnalyticsGate._resetForTesting()
        UserDefaults.standard.removeObject(forKey: consentKey)
    }

    override func tearDown() async throws {
        // Restore real backend and clear internal gate state so tests don't bleed.
        ResearchAnalyticsGate._resetForTesting()
        ResearchAnalyticsGate._backend = PostHogAnalyticsBackend()
        ResearchAnalyticsGate._erasureQueue = .shared
        try? FileManager.default.removeItem(at: erasureDir)
        UserDefaults.standard.removeObject(forKey: consentKey)
        try await super.tearDown()
    }

    // MARK: - Gate state

    // @verifies REQ-CONSENT-14
    func testGateIsClosedWhenConsentKeyAbsent() {
        XCTAssertFalse(ResearchAnalyticsGate.isOpen,
                       "Gate must be closed when consent key has never been set")
    }

    // @verifies REQ-CONSENT-14
    func testGateIsClosedWhenConsentKeyIsFalse() {
        UserDefaults.standard.set(false, forKey: consentKey)
        XCTAssertFalse(ResearchAnalyticsGate.isOpen)
    }

    // @verifies REQ-CONSENT-14
    func testGateIsOpenAfterConsentAccepted() {
        UserDefaults.standard.set(true, forKey: consentKey)
        XCTAssertTrue(ResearchAnalyticsGate.isOpen)
    }

    // @verifies REQ-CONSENT-14
    func testGateDoesNotOpenOnOnboardingShownKey() {
        UserDefaults.standard.set(true, forKey: "np.onboarding.consent-shown")
        XCTAssertFalse(ResearchAnalyticsGate.isOpen,
                       "Gate must not open on consent-shown — only research-consent-granted counts")
    }

    // @verifies REQ-CONSENT-14
    func testGateDoesNotOpenOnLegacyOnboardingAcceptedKey() {
        UserDefaults.standard.set(true, forKey: "np.onboarding.consent-accepted")
        XCTAssertFalse(ResearchAnalyticsGate.isOpen,
                       "Retired onboarding key must not open the research analytics gate")
    }

    // @verifies REQ-CONSENT-14
    func testResearchAnalyticsKeyMatchesConstant() {
        XCTAssertEqual(PersistedKeys.RESEARCH_ANALYTICS_KEY, "np.research.consent-granted")
    }

    // @verifies REQ-CONSENT-14
    func testSkipDoesNotOpenResearchAnalyticsGate() {
        UserDefaults.standard.set(true, forKey: "np.onboarding.consent-shown")
        XCTAssertFalse(ResearchAnalyticsGate.isOpen,
                       "Pressing Skip must not open the research analytics gate")
    }

    // MARK: - ISC-92: backend.configure() must not fire before gate opens

    // @verifies REQ-CONSENT-14
    func testISC92_backendNotCalledWhenGateClosed() {
        ResearchAnalyticsGate.configure()
        XCTAssertEqual(spy.configureCallCount, 0,
                       "ISC-92: backend.configure() must not be called when research analytics gate is closed")
    }

    // @verifies REQ-SW03-07
    func testISC92_backendCalledOnceWhenGateOpen() {
        UserDefaults.standard.set(true, forKey: consentKey)
        ResearchAnalyticsGate.configure()
        XCTAssertEqual(spy.configureCallCount, 1,
                       "ISC-92: backend.configure() must be called exactly once after gate opens")
    }

    // @verifies REQ-SW03-07
    func testISC92_backendCalledOnlyOnceOnRepeatConfigure() {
        UserDefaults.standard.set(true, forKey: consentKey)
        ResearchAnalyticsGate.configure()
        ResearchAnalyticsGate.configure()
        XCTAssertEqual(spy.configureCallCount, 1,
                       "ISC-92: configure() is idempotent — backend must not be called a second time")
    }

    // @verifies REQ-SW03-07
    func testISC92_gateStateUnchangedAfterConfigureWithClosedGate() {
        ResearchAnalyticsGate.configure()
        XCTAssertFalse(ResearchAnalyticsGate.isOpen,
                       "configure() with closed gate must not side-effect the gate state")
        XCTAssertEqual(spy.configureCallCount, 0)
    }

    // MARK: - track() no-ops when gate closed

    // @verifies REQ-CONSENT-14
    func testTrackNoOpsWhenGateClosed() {
        XCTAssertFalse(ResearchAnalyticsGate.isOpen)
        ResearchAnalyticsGate.track(event: "test_event", properties: ["key": "value"])
        XCTAssertEqual(spy.trackCallCount, 0,
                       "backend.track() must not be called when research analytics gate is closed")
    }

    // MARK: - reset()

    // @verifies REQ-CONSENT-03
    func testResetNoOpsWhenNotConfigured() {
        // reset() guards on isConfigured; since setUp calls _resetForTesting,
        // isConfigured is false here. The backend must not receive reset().
        ResearchAnalyticsGate.reset()
        XCTAssertEqual(spy.resetCallCount, 0,
                       "backend.reset() must not be called when gate was never configured")
    }

    // @verifies REQ-CONSENT-03
    func testResetCallsBackendWhenConfigured() {
        UserDefaults.standard.set(true, forKey: consentKey)
        ResearchAnalyticsGate.configure()
        ResearchAnalyticsGate.reset()
        XCTAssertEqual(spy.resetCallCount, 1)
    }

    // @verifies REQ-CONSENT-03
    func testResetAllowsReconfigureAfterwards() {
        UserDefaults.standard.set(true, forKey: consentKey)
        ResearchAnalyticsGate.configure()
        ResearchAnalyticsGate.reset()
        // Simulate re-consent
        ResearchAnalyticsGate.configure()
        XCTAssertEqual(spy.configureCallCount, 2,
                       "After reset(), configure() must be able to call the backend again")
    }

    // MARK: - Blanket research consent coupling

    // @verifies REQ-CONSENT-02#b
    func testWithdrawBlanketResearchConsentRevokesResearchAnalytics() {
        // Open the gate and configure the SDK.
        UserDefaults.standard.set(true, forKey: consentKey)
        ResearchAnalyticsGate.configure()
        XCTAssertEqual(spy.configureCallCount, 1)
        XCTAssertTrue(ResearchAnalyticsGate.isOpen)

        // Blanket research consent withdrawal must close the research analytics gate
        // AND tear down the SDK — the user is signaling no data collection beyond
        // basic device function (CLAUDE.md §6.0, ConsentStore authoritative design).
        let store = ConsentStore()
        store.withdrawBlanketResearchConsent()

        XCTAssertFalse(ResearchAnalyticsGate.isOpen,
                       "Research analytics gate must close after blanket research consent withdrawal")
        XCTAssertEqual(spy.resetCallCount, 1,
                       "backend.reset() must be called on blanket research consent withdrawal")
    }

    // @verifies REQ-CONSENT-02#a
    func testWithdrawCategoryConsentDoesNotRevokeResearchAnalytics() {
        // Partial withdrawal (specific category) must NOT revoke research analytics
        // — the user remains a research participant in other categories.
        UserDefaults.standard.set(true, forKey: consentKey)
        ResearchAnalyticsGate.configure()
        XCTAssertEqual(spy.configureCallCount, 1)

        let store = ConsentStore()
        store.setCategoryConsent(.depression, granted: false)

        XCTAssertTrue(ResearchAnalyticsGate.isOpen,
                      "Research analytics gate must stay open after a single category withdrawal")
        XCTAssertEqual(spy.resetCallCount, 0,
                       "backend.reset() must not be called on partial category withdrawal")
    }

    // MARK: - Blanket coupling at the UI commit path (CLAUDE.md §6.2.5)
    //
    // The two tests above pin the coupling in the explicitly-named withdrawal method. These
    // two pin it in `updateResearchConsent` — the path the consent UI actually uses. Before
    // Rev 37 the named method was correct and tested on both platforms while nothing on iOS
    // called it from the UI, so turning blanket consent off in Research Preferences committed
    // through `updateResearchConsent` and silently skipped the teardown. Merging L2 and L3
    // onto one commit-at-the-end screen makes that the ordinary path, so it needs its own
    // probe. Both directions are pinned: the teardown must fire on a true→false transition,
    // and must NOT fire on a category-only edit.

    /// Clears the store's own persistence so a stale `np.consent.research` blob from another
    /// test cannot decide the transition under test.
    private func makeCleanStore() -> ConsentStore {
        UserDefaults.standard.removeObject(forKey: "np.consent.research")
        return ConsentStore()
    }

    // @verifies REQ-CONSENT-03#a
    func testCommitPathBlanketWithdrawalRevokesResearchAnalytics() {
        UserDefaults.standard.set(true, forKey: consentKey)
        ResearchAnalyticsGate.configure()

        let store = makeCleanStore()
        var state = ResearchConsentState()
        state.blanketConsentGranted = true
        store.updateResearchConsent(state)

        XCTAssertEqual(spy.resetCallCount, 0,
                       "Granting blanket consent must not tear anything down")

        // The withdrawal the merged S2 screen actually produces: same struct, blanket off.
        state.blanketConsentGranted = false
        store.updateResearchConsent(state)

        XCTAssertFalse(ResearchAnalyticsGate.isOpen,
                       "Committing a blanket true→false transition must close the research analytics gate")
        XCTAssertEqual(spy.resetCallCount, 1,
                       "backend.reset() must be called when the commit path revokes blanket consent")
    }

    // @verifies REQ-CONSENT-03#b
    func testCommitPathCategoryOnlyEditDoesNotRevokeResearchAnalytics() {
        UserDefaults.standard.set(true, forKey: consentKey)
        ResearchAnalyticsGate.configure()

        let store = makeCleanStore()
        // No blanket consent at any point — only L2 scope changes. This is the inverse
        // regression: guarding on the value rather than the transition would tear down here.
        var state = ResearchConsentState()
        state.setAllCategories(true)
        store.updateResearchConsent(state)

        state.setAllCategories(false)
        store.updateResearchConsent(state)

        state.categoryConsents[.depression] = true
        store.updateResearchConsent(state)

        XCTAssertTrue(ResearchAnalyticsGate.isOpen,
                      "Category-only edits must leave the research analytics gate open")
        XCTAssertEqual(spy.resetCallCount, 0,
                       "backend.reset() must never be called for a category-only edit")
    }

    // @verifies REQ-CONSENT-03#c
    func testCommitPathBlanketStaysOnDoesNotRevokeResearchAnalytics() {
        UserDefaults.standard.set(true, forKey: consentKey)
        ResearchAnalyticsGate.configure()

        let store = makeCleanStore()
        var state = ResearchConsentState()
        state.blanketConsentGranted = true
        store.updateResearchConsent(state)

        // Editing categories while blanket consent stays on is not a withdrawal.
        state.categoryConsents[.sleep] = true
        store.updateResearchConsent(state)

        XCTAssertTrue(ResearchAnalyticsGate.isOpen)
        XCTAssertEqual(spy.resetCallCount, 0,
                       "A true→true blanket state is not a withdrawal")
    }

    // MARK: - Permitted-event allowlist (ISC-97, NP-APP-TELEMETRY-001 §3)

    // The twelve keys the old denylist knew and ones it never thought of: an allowlist drops every one.
    // @verifies REQ-SW03-07
    func testTrackDropsEventWithAPropertyTheTelemetryPolicyDoesNotName() {
        UserDefaults.standard.set(true, forKey: consentKey)
        let keys = [
            "eeg", "hrv", "rmssd", "coherence", "session_id", "protocol_id", "session_count", "session_sequence",
            "imp", "impedance", "impedance_flags", "pass_flags",
            "heart_rate", "alpha_power", "electrode_site", "condition", "email", "duration_s", "timestamp",
        ]
        for key in keys {
            ResearchAnalyticsGate.track(event: "evt", properties: [key: "1", "engagement_tier": "new"])
        }
        XCTAssertEqual(spy.trackCallCount, 0, "A property not on the permitted list must prevent the backend call")
        XCTAssertTrue(ResearchAnalyticsGate.isOpen, "Dropping an event must not close the gate")
    }

    // @verifies REQ-SW03-07
    func testTrackDropsEventWithAValueNotOfItsPropertysForm() {
        UserDefaults.standard.set(true, forKey: consentKey)
        ResearchAnalyticsGate.track(event: "evt", properties: ["engagement_tier": "42"])           // the raw count the tier replaced
        ResearchAnalyticsGate.track(event: "evt", properties: ["screen_name": "tdcs_depression"])  // a protocol or condition as a screen
        ResearchAnalyticsGate.track(event: "evt", properties: ["error_code": "1746000000000"])     // a timestamp in an error code
        ResearchAnalyticsGate.track(event: "evt", properties: ["os_version": "iOS 18.2.1"])        // a patch version
        XCTAssertEqual(spy.trackCallCount, 0)
    }

    // @verifies REQ-SW03-07
    func testTrackDropsEventWhoseNameIsNotSnakeCaseOrCarriesAHealthTerm() {
        UserDefaults.standard.set(true, forKey: consentKey)
        ResearchAnalyticsGate.track(event: "tdcs_started", properties: [:])
        ResearchAnalyticsGate.track(event: "User Email Sent", properties: [:])
        XCTAssertEqual(spy.trackCallCount, 0)
    }

    // @verifies REQ-SW03-07
    func testTrackDropsTheWholeEventWhenOneOfSeveralPropertiesIsNotPermitted() {
        UserDefaults.standard.set(true, forKey: consentKey)
        ResearchAnalyticsGate.track(event: "evt", properties: ["engagement_tier": "new", "app_version": "1.0.3", "hrv": "50"])
        XCTAssertEqual(spy.trackCallCount, 0, "An event is dropped whole, never trimmed to fit")
    }

    // MARK: - Allowed events reach the backend

    // @verifies REQ-SW03-07
    func testTrackAllowsEveryPropertyTheTelemetryPolicyPermits() {
        UserDefaults.standard.set(true, forKey: consentKey)
        let properties = [
            "app_version": "1.0.3", "os_version": "iOS 18.2", "device_class": "iPhone_flagship",
            "screen_name": "protocol_detail", "engagement_tier": "active", "app_start_type": "cold_start",
            "onboarding_step": "consent_flow_step_1", "error_code": "42", "connectivity_type": "wifi",
        ]
        ResearchAnalyticsGate.track(event: "app_open", properties: properties)
        XCTAssertEqual(spy.trackCallCount, 1)
        XCTAssertEqual(spy.trackedEvents.first?.event, "app_open")
        XCTAssertEqual(spy.trackedEvents.first?.properties, properties)
    }

    // @verifies REQ-SW03-07
    func testTrackAllowsEventWithEmptyProperties() {
        UserDefaults.standard.set(true, forKey: consentKey)
        ResearchAnalyticsGate.track(event: "app_foregrounded", properties: [:])
        XCTAssertEqual(spy.trackCallCount, 1)
    }

    // @verifies REQ-SW03-07
    func testEngagementTierIsAllowed() {
        UserDefaults.standard.set(true, forKey: consentKey)
        ResearchAnalyticsGate.track(event: "app_open", properties: ["engagement_tier": "new"])
        XCTAssertEqual(spy.trackCallCount, 1,
                       "engagement_tier is the approved replacement for session_count — must be allowed")
    }

    // @verifies REQ-SW03-07
    func testMultipleSafeEventsAccumulate() {
        UserDefaults.standard.set(true, forKey: consentKey)
        ResearchAnalyticsGate.track(event: "e1", properties: [:])
        ResearchAnalyticsGate.track(event: "e2", properties: [:])
        XCTAssertEqual(spy.trackCallCount, 2)
        XCTAssertEqual(spy.trackedEvents.map(\.event), ["e1", "e2"])
    }

    // MARK: - ISC-157 static assertion (compile-time)
    //
    // This test file imports only `XCTest` and `@testable import NeurOne`.
    // It does NOT import `PostHog`. The SpyAnalyticsBackend in SpyAnalyticsBackend.swift
    // also does not import PostHog. No test file touches the PostHog SDK directly.
    // Verified by: grep -rn "^import PostHog" NeurOneTests/ — must return zero lines.
    // @verifies REQ-SW03-07
    func testISC157_noDirectPostHogImportInTestTarget() throws {
        let testDir = URL(fileURLWithPath: #filePath)
            .deletingLastPathComponent()  // NeurOneTests/
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
