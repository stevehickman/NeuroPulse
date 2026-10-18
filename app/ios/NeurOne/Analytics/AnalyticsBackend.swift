import Foundation

/// Abstraction layer between `ResearchAnalyticsGate` and any concrete analytics SDK.
///
/// All PostHog API calls live exclusively in `PostHogAnalyticsBackend`.
/// Tests inject a `SpyAnalyticsBackend` (test target only) via
/// `ResearchAnalyticsGate._backend` — no test file ever imports PostHog directly (ISC-157).
@MainActor
protocol AnalyticsBackend: AnyObject {
    /// Initialize (or re-enable) the analytics SDK after the user has consented.
    func configure()

    /// Forward a pre-validated event to the SDK. Called only when the gate is open
    /// and the permitted-event check (AnalyticsEventPolicy) has already passed.
    func track(event: String, properties: [String: String])

    /// The pseudonymous ID the vendor holds this person's events under, or nil when there is none (the SDK was never set
    /// up). Read BEFORE `reset()`, which makes a new one: the old ID is what has to be erased from the vendor (REQ-SW03-19).
    var distinctID: String? { get }

    /// Opt the SDK out and reset the anonymous distinct ID on consent withdrawal.
    func reset()
}
