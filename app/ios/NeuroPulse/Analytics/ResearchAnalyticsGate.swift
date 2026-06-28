import Foundation
import os

/// Consent gate for research analytics and crash-reporting telemetry (ISC-92, ISC-97).
///
/// No analytics or crash-reporting SDK may initialize or receive any event until
/// the user has **actively completed** the consent onboarding flow by tapping Done.
/// The gate keys on `np.research.consent-granted` (`researchAnalyticsKey`), set
/// inside `ConsentOnboardingView.commitAndDismiss(grantResearchAnalytics: true)`.
///
/// Pressing Skip does NOT set this key — analytics stays off until an explicit
/// Done action. Withdrawing blanket research consent (L3) clears this key because
/// blanket withdrawal signals the user does not want any data collection beyond
/// basic device function. Partial withdrawals (specific study or category) do not
/// revoke research analytics — the user remains a research participant in other scopes.
///
/// This gate is entirely independent of `WarrantyAnalyticsGate`, which controls
/// SHDR fleet telemetry under warranty consent. The two consent subjects are never
/// mixed: research consent belongs to the device user; warranty consent belongs to
/// the warranty owner (which may be a clinic or institution, not the end user).
///
/// Call sites use `ResearchAnalyticsGate` exclusively and never call the SDK directly.
/// The concrete backend is `PostHogAnalyticsBackend`; tests inject a
/// `SpyAnalyticsBackend` via `_backend` (ISC-157).
@MainActor
enum ResearchAnalyticsGate {

    private static let log = Logger(subsystem: "com.neuropulse.analytics", category: "research-gate")

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
    /// completed" with "research analytics consented."
    ///
    /// NOT set when the user presses Skip — skipping defers all data decisions
    /// including research analytics. Cleared by blanket research consent withdrawal
    /// because blanket withdrawal implies full data-collection opt-out.
    ///
    /// NOT the same as `WarrantyAnalyticsGate.warrantyConsentKey` — research and
    /// warranty consent are independent and must never share a UserDefaults key.
    static let researchAnalyticsKey = "np.research.consent-granted"

    /// True only when the user has actively completed the research consent flow
    /// (tapped Done, not Skip), as recorded by `researchAnalyticsKey`.
    static var isOpen: Bool {
        UserDefaults.standard.bool(forKey: researchAnalyticsKey)
    }

    /// The active analytics backend. Defaults to `PostHogAnalyticsBackend` in
    /// production. Tests inject a `SpyAnalyticsBackend` via `@testable import`
    /// to assert call counts without touching the real PostHog SDK (ISC-157).
    static var _backend: any AnalyticsBackend = PostHogAnalyticsBackend()

    // MARK: - Public interface

    /// Initialize the analytics SDK. Call exactly once, after the research consent
    /// flow completes. No-ops if already configured or if the gate is still closed.
    static func configure() {
        guard !isConfigured else { return }
        guard isOpen else {
            log.debug("ResearchAnalyticsGate.configure() ignored — research consent gate is closed.")
            return
        }
        isConfigured = true
        log.debug("ResearchAnalyticsGate.configure() — gate open; initialising analytics SDK.")
        _backend.configure()
    }

    /// Close the research analytics gate and tear down the SDK.
    ///
    /// Call when the user revokes research analytics — either via the explicit
    /// analytics opt-out toggle in Settings, or indirectly via blanket research
    /// consent withdrawal (L3). The `isOpen` guard in `track()` already stops new
    /// events once `researchAnalyticsKey` is cleared, but a running SDK can still
    /// collect passively — this method tears it down fully and resets `isConfigured`
    /// so `configure()` becomes a no-op until re-consent.
    ///
    /// Does NOT affect `WarrantyAnalyticsGate` or SHDR fleet uploads.
    static func reset() {
        guard isConfigured else { return }
        // Tear down the SDK BEFORE clearing the flag. If teardown is async or can
        // throw in a real vendor implementation, clearing the flag first would let
        // a concurrent configure() call re-initialize the SDK before the previous
        // instance is actually shut down.
        log.debug("ResearchAnalyticsGate.reset() — research analytics revoked; tearing down SDK.")
        _backend.reset()
        isConfigured = false
    }

    /// Record a research analytics event. Silently no-ops if the research gate is closed.
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
                ResearchAnalyticsGate.track dropped event '\(event, privacy: .public)' \
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
