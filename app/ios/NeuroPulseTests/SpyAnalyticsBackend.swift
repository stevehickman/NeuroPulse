import Foundation
@testable import NeuroPulse

/// Test spy for `AnalyticsBackend`. Injected via `AnalyticsGate._backend` in
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
    }
}
