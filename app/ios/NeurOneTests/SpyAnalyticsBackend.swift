import Foundation
@testable import NeurOne

/// Test spy for `AnalyticsBackend`. Injected via `ResearchAnalyticsGate._backend` in
/// `AnalyticsGateTests.setUp()`.
///
/// Records every call made to it so tests can assert on call counts and event
/// payloads without touching PostHog or making any network requests (ISC-157).
/// This file lives in the test target only — no production code imports it.
@MainActor
final class SpyAnalyticsBackend: AnalyticsBackend {
    var configureCallCount = 0
    var trackCallCount = 0
    var resetCallCount = 0

    /// The pseudonymous ID this spy pretends the vendor holds; nil models an SDK that was never set up.
    var distinctID: String? = "spy-distinct-id"

    struct TrackedEvent: Equatable {
        let event: String
        let properties: [String: String]
    }
    var trackedEvents: [TrackedEvent] = []

    func configure() {
        configureCallCount += 1
    }

    func track(event: String, properties: [String: String]) {
        trackCallCount += 1
        trackedEvents.append(TrackedEvent(event: event, properties: properties))
    }

    func reset() {
        resetCallCount += 1
        // The real SDK makes a NEW anonymous ID on reset, so the old one is only readable before it (REQ-SW03-19).
        distinctID = "spy-distinct-id-after-reset"
    }
}
