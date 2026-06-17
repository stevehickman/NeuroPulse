import Foundation

/// Abstraction layer between `ResearchAnalyticsGate` and any concrete analytics SDK.
///
/// All PostHog API calls live exclusively in `PostHogAnalyticsBackend`.
/// Tests inject a `SpyAnalyticsBackend` (test target only) via
/// `ResearchAnalyticsGate._backend` — no test file ever imports PostHog directly (ISC-157).
@MainActor
protocol AnalyticsBackend: AnyObject {
    /// Initialise (or re-enable) the analytics SDK after the user has consented.
    func configure()

    /// Forward a pre-validated event to the SDK. Called only when the gate is open
    /// and all prohibited-key checks have already passed.
    func track(event: String, properties: [String: String])

    /// Opt the SDK out and reset the anonymous distinct ID on consent withdrawal.
    func reset()
}
