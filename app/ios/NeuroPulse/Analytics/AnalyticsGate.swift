import Foundation
import os

/// Consent gate for all analytics and crash-reporting telemetry (ISC-92, ISC-97).
///
/// No analytics or crash-reporting SDK may initialise or receive any event until
/// the user has explicitly completed (or explicitly skipped) the consent flow.
/// The gate keys on `np.onboarding.consent-accepted`, set inside
/// `ConsentOnboardingView.commitAndDismiss()` — the point at which the user
/// has actively made a decision, not merely been shown the screen. This is the
/// stronger semantic: favours privacy by requiring a deliberate user action
/// rather than passive screen presentation.
///
/// This type is a vendor-agnostic abstraction: call sites use `AnalyticsGate`
/// exclusively and never touch the SDK directly. When a vendor is selected
/// (OI-AUDIT-01), only `AnalyticsSDKStub` is replaced — call sites do not change.
@MainActor
enum AnalyticsGate {

    private static let log = Logger(subsystem: "com.neuropulse.analytics", category: "gate")

    /// Idempotency flag so `configure()` only ever starts the SDK once.
    private static var isConfigured = false

    /// Property keys that must NEVER appear in a tracked event (ISC-97 /
    /// NP-APP-TELEMETRY-001 Rev B). `session_count` / `session_sequence` are the
    /// deprecated raw-count fields replaced by `engagement_tier`.
    private static let prohibitedKeys: Set<String> = [
        "eeg", "hrv", "rmssd", "coherence", "session_id", "protocol_id",
        "session_count", "session_sequence"
    ]

    /// The UserDefaults key set when the user completes or explicitly skips the
    /// consent flow inside `ConsentOnboardingView`. Keyed on the deliberate
    /// user action, not on screen presentation — stronger privacy guarantee.
    static let consentAcceptedKey = "np.onboarding.consent-accepted"

    /// True only when the user has actively completed the consent flow
    /// (accepted or explicitly skipped), as recorded by `consentAcceptedKey`.
    static var isOpen: Bool {
        UserDefaults.standard.bool(forKey: consentAcceptedKey)
    }

    /// Initialise the analytics SDK. Call exactly once, after the consent flow
    /// completes. No-ops if already configured or if the gate is still closed.
    static func configure() {
        guard !isConfigured else { return }
        guard isOpen else {
            log.debug("AnalyticsGate.configure() ignored — consent gate is closed.")
            return
        }
        isConfigured = true
        log.debug("AnalyticsGate.configure() — gate open; initialising analytics SDK.")
        AnalyticsSDKStub.configure()
    }

    /// Record an analytics event. Silently no-ops if the gate is closed.
    /// Drops the event entirely (does NOT transmit) if any prohibited key is
    /// present (ISC-97).
    static func track(event: String, properties: [String: String]) {
        guard isOpen else { return }

        let offending = prohibitedKeys.intersection(properties.keys)
        guard offending.isEmpty else {
            // Log the offending key names (not values) so the leak source is
            // traceable, and drop the event rather than transmitting it.
            let names = offending.sorted().joined(separator: ", ")
            log.error("AnalyticsGate.track dropped event '\(event, privacy: .public)' — prohibited keys present: \(names, privacy: .public)")
            return
        }

        AnalyticsSDKStub.track(event: event, properties: properties)
    }
}
