import Foundation
import os

// Replace this file with the real analytics SDK wrapper once a vendor is selected (OI-AUDIT-01).
//
// This stub exposes the exact interface that `AnalyticsGate` calls through to.
// It performs NO network I/O and emits NO real telemetry — every operation is a
// local debug log only. When a vendor is selected, replace the bodies of
// `configure()` and `track(event:properties:)` with the real SDK calls while
// keeping the signatures identical, so `AnalyticsGate` call sites never change.
enum AnalyticsSDKStub {

    private static let log = Logger(subsystem: "com.neuropulse.analytics", category: "sdk-stub")

    /// Initialise the analytics SDK. Stub: logs only, no SDK is started.
    static func configure() {
        log.debug("AnalyticsSDKStub.configure() — no vendor selected (OI-AUDIT-01); no telemetry started.")
    }

    /// Record an analytics event. Stub: logs only, nothing is transmitted.
    /// - Parameters:
    ///   - event: event name.
    ///   - properties: pre-validated event properties (gate has already
    ///     stripped/rejected prohibited keys before reaching this call).
    static func track(event: String, properties: [String: String]) {
        log.debug("AnalyticsSDKStub.track(event:\(event, privacy: .public)) — \(properties.count, privacy: .public) properties (not transmitted).")
    }

    /// Tear down the analytics SDK on consent withdrawal. Stub: logs only.
    static func reset() {
        log.debug("AnalyticsSDKStub.reset() — SDK teardown on consent withdrawal (no vendor selected; OI-AUDIT-01).")
    }
}
