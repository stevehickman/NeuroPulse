import Foundation
import os

/// Consent gate for research analytics and crash-reporting telemetry (ISC-92, ISC-97).
///
/// No analytics or crash-reporting SDK may initialize or receive any event until
/// the user has **actively completed** the consent onboarding flow by tapping Done.
/// The gate keys on `np.research.consent-granted` (`RESEARCH_ANALYTICS_KEY`), set
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

    private static let log = Logger(subsystem: "life.neurone.analytics", category: "research-gate")

    /// Idempotency flag so `configure()` only ever starts the SDK once.
    private static var isConfigured = false

    /// True only when the user has actively completed the research consent flow
    /// (tapped Done, not Skip), as recorded by `RESEARCH_ANALYTICS_KEY`.
    static var isOpen: Bool {
        ProtectedDefaults.standard.bool(forKey: PersistedKeys.RESEARCH_ANALYTICS_KEY)
    }

    /// The active analytics backend. Defaults to `PostHogAnalyticsBackend` in
    /// production. Tests inject a `SpyAnalyticsBackend` via `@testable import`
    /// to assert call counts without touching the real PostHog SDK (ISC-157).
    static var _backend: any AnalyticsBackend = PostHogAnalyticsBackend()

    /// Where the withdrawn person's old pseudonymous ID waits to be erased from the vendor (REQ-SW03-19). Tests inject a queue
    /// with a fake relay so no test makes a network call.
    static var _erasureQueue: AnalyticsErasureQueue = .shared

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
    /// events once `RESEARCH_ANALYTICS_KEY` is cleared, but a running SDK can still
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
        // Read the pseudonymous ID BEFORE the backend resets it: `reset()` makes a new one, and the old one is what the vendor
        // holds this person's events under. It is queued, durably, and erased from the vendor within 30 days
        // (NP-APP-TELEMETRY-001 §5, REQ-SW03-19).
        let withdrawnID = _backend.distinctID
        _backend.reset()
        isConfigured = false
        if let withdrawnID {
            _erasureQueue.enqueue(withdrawnID)
            Task { await _erasureQueue.drain() }
        }
    }

    /// Record a research analytics event. Silently no-ops if the research gate is closed.
    /// Drops the event entirely (does NOT transmit) unless `AnalyticsEventPolicy` permits it: a property the
    /// telemetry policy does not name, or a value not of its form, drops the whole event (an allowlist,
    /// NP-APP-TELEMETRY-001 §3), and so does an event that cannot be checked (ISC-97).
    static func track(event: String, properties: [String: String]) {
        guard isOpen else { return }

        guard AnalyticsEventPolicy.permits(event: event, properties: properties) else {
            // Log the event name only (never a property value), so the source is traceable and nothing the
            // vendor was not to have is written down.
            log.error("""
                ResearchAnalyticsGate.track dropped event '\(event, privacy: .private)' \
                — not on the permitted list (NP-APP-TELEMETRY-001 §3)
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
