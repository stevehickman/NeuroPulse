import Foundation
import os

/// Consent gate for all analytics and crash-reporting telemetry (ISC-92, ISC-97).
///
/// No analytics or crash-reporting SDK may initialise or receive any event until
/// the user has **actively completed** the consent onboarding flow by tapping Done.
/// The gate keys on `np.analytics.consent-granted` (`analyticsConsentKey`), set
/// inside `ConsentOnboardingView.commitAndDismiss(grantAnalyticsConsent: true)`.
///
/// Pressing Skip does NOT set this key — analytics stays off until an explicit
/// Done action. Research-consent withdrawal also does NOT clear this key —
/// analytics and research participation are independent consent decisions.
///
/// Call sites use `AnalyticsGate` exclusively and never call the SDK directly.
/// The concrete backend is `PostHogAnalyticsBackend`; tests inject a
/// `SpyAnalyticsBackend` via `_backend` (ISC-157).
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
        "session_count", "session_sequence",
        // Impedance data is UHDR-class (raw per-electrode bitmask at named scalp positions).
        "imp", "impedance", "impedance_flags", "pass_flags"
    ]

    /// The UserDefaults key set when the user **actively completes** the consent
    /// onboarding flow (taps Done on the final layer). Distinct from
    /// `np.onboarding.consent-shown` which is set when the view appears, and from
    /// the retired `np.onboarding.consent-accepted` which conflated "onboarding
    /// completed" with "analytics consented."
    ///
    /// NOT set when the user presses Skip — skipping defers all data decisions
    /// including analytics. NOT cleared by research-consent withdrawal —
    /// analytics and research participation are independent consent decisions.
    static let analyticsConsentKey = "np.analytics.consent-granted"

    /// True only when the user has actively completed the consent flow
    /// (tapped Done, not Skip), as recorded by `analyticsConsentKey`.
    static var isOpen: Bool {
        UserDefaults.standard.bool(forKey: analyticsConsentKey)
    }

    /// The active analytics backend. Defaults to `PostHogAnalyticsBackend` in
    /// production. Tests inject a `SpyAnalyticsBackend` via `@testable import`
    /// to assert call counts without touching the real PostHog SDK (ISC-157).
    static var _backend: any AnalyticsBackend = PostHogAnalyticsBackend()

    // MARK: - Public interface

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
        _backend.configure()
    }

    /// Close the analytics gate and tear down the SDK.
    ///
    /// Call when the user explicitly opts out of analytics. The `isOpen` guard in
    /// `track()` already stops new events once `analyticsConsentKey` is cleared, but
    /// a running SDK can still collect passively — this method tears it down fully and
    /// resets `isConfigured` so `configure()` becomes a no-op until re-consent.
    ///
    /// Note: do NOT call this from research-consent withdrawal paths.
    /// Analytics consent and research participation are independent decisions.
    static func reset() {
        guard isConfigured else { return }
        // Tear down the SDK BEFORE clearing the flag. If teardown is async or can
        // throw in a real vendor implementation, clearing the flag first would let
        // a concurrent configure() call re-initialise the SDK before the previous
        // instance is actually shut down.
        log.debug("AnalyticsGate.reset() — analytics consent withdrawn; tearing down SDK.")
        _backend.reset()
        isConfigured = false
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
            log.error("""
                AnalyticsGate.track dropped event '\(event, privacy: .public)' \
                — prohibited keys present: \(names, privacy: .public)
                """)
            return
        }

        _backend.track(event: event, properties: properties)
    }

    // MARK: - Testing support

#if DEBUG
    /// Reset internal gate state between unit tests. Must not be called in
    /// production code paths. Clears `isConfigured` without touching the backend,
    /// so the next `configure()` call enters the full setup path again.
    static func _resetForTesting() {
        isConfigured = false
    }
#endif
}
