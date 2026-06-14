import Foundation
import os
import PostHog

/// Production `AnalyticsBackend` that wraps the PostHog iOS SDK.
///
/// Consumed only by `AnalyticsGate`. Nothing else in the app imports PostHog
/// directly — this is the single integration point (ISC-157).
///
/// INFRASTRUCTURE — eu.i.posthog.com (PostHog EU cluster)
///
/// Domain   : PostHog-owned. No domain registration required by NeuroPulse.
/// Purpose  : Analytics event ingestion — engagement funnels only. No health data.
///
/// Setup:
///   (1) Create a PostHog EU account at https://eu.posthog.com (not us.posthog.com —
///       EU residency is required to satisfy GDPR Art. 44 for EU/EEA users).
///   (2) Create a new project and copy the project API token (format: "phc_<chars>").
///   (3) Set POSTHOG_PROJECT_TOKEN=phc_<your_token> in one of:
///         • Xcode local .xcconfig (developer builds) — add to .gitignore, never commit.
///         • GitHub Actions secret POSTHOG_PROJECT_TOKEN passed to xcodebuild via
///           -xcconfig or an environment variable override.
///       Info.plist already reads: PostHogProjectToken = $(POSTHOG_PROJECT_TOKEN).
///       No source changes are needed — the token is injected at build time only.
///   (4) In PostHog project settings confirm: EU region, no session replay, no heatmaps.
///       All autocapture is disabled in code below; double-check the project toggle matches.
///   (5) OI-ANALYTICS-01 (open): implement server-side distinct_id deletion relay endpoint
///       for the consent-withdrawal deletion flow in reset() below.
///
/// ISC-9: the phc_... token must NEVER appear in source. It is read at runtime from
/// Info.plist. The no_hardcoded_secret SwiftLint rule enforces this in CI.
///
/// Configuration decisions:
/// - EU endpoint (https://eu.i.posthog.com) — data stored in EU.
/// - All auto-capture features disabled — we control every event via AnalyticsGate.track().
/// - personProfiles = .never — no Person record created server-side; the anonymous
///   distinct_id is sent with events for funnel analysis but PostHog does not build
///   a profile. Matches "not linked to identity" in App Store nutrition label.
@MainActor
final class PostHogAnalyticsBackend: AnalyticsBackend {

    private static let log = Logger(subsystem: "com.neuropulse.analytics", category: "posthog")

    /// True once `PostHogSDK.shared.setup()` has been called for this process lifetime.
    /// On re-consent after withdrawal, `configure()` calls `optIn()` instead of `setup()`.
    private var isSetUp = false

    // MARK: - AnalyticsBackend

    func configure() {
        if isSetUp {
            // User previously consented, then withdrew and is now re-consenting.
            // Re-enable the already-initialised SDK rather than calling setup() again.
            PostHogSDK.shared.optIn()
            Self.log.debug("PostHogAnalyticsBackend: re-enabled after prior opt-out.")
            return
        }

        guard let token = Bundle.main.object(forInfoDictionaryKey: "PostHogProjectToken") as? String,
              !token.isEmpty,
              // Catch the literal "$(POSTHOG_PROJECT_TOKEN)" when the build
              // variable is not set — this happens in developer builds that have
              // not set the variable in their local .xcconfig or environment.
              !token.hasPrefix("$(")
        else {
            Self.log.error("""
                PostHogAnalyticsBackend: PostHogProjectToken missing or unexpanded \
                in Info.plist — analytics disabled. \
                Set POSTHOG_PROJECT_TOKEN in your .xcconfig or CI secrets.
                """)
            return
        }

        let config = PostHogConfig(projectToken: token, host: "https://eu.i.posthog.com")

        // Disable all auto-capture. Every event is intentionally sent via
        // AnalyticsGate.track(), which enforces the prohibited-key list (ISC-97).
        config.captureApplicationLifecycleEvents = false
        config.captureScreenViews = false
        config.captureElementInteractions = false
        config.enableSwizzling = false
        config.sendFeatureFlagEvent = false
        config.preloadFeatureFlags = false
        config.sessionReplay = false
        config.surveys = false

        // Never create person profiles — the anonymous UUID distinct_id is sent
        // with events for cohort analysis but no "Person" record is built in
        // PostHog's database. This is the most privacy-preserving profile mode.
        config.personProfiles = .never

        PostHogSDK.shared.setup(config)
        isSetUp = true
        Self.log.debug("PostHogAnalyticsBackend: PostHog initialised (EU endpoint).")
    }

    func track(event: String, properties: [String: String]) {
        // [String: String] is narrower than PostHog's [String: Any]; the cast is safe.
        PostHogSDK.shared.capture(event, properties: properties as [String: Any])
    }

    func reset() {
        // Stop all future data collection and clear the locally queued event queue.
        PostHogSDK.shared.optOut()

        // Generate a fresh anonymous distinct_id. This de-links any future sessions
        // from pre-withdrawal events even if the user re-consents later.
        PostHogSDK.shared.reset()

        // OI-ANALYTICS-01: call the PostHog server-side deletion API for the
        // distinct_id that was active at the time of withdrawal to honour the
        // NP-APP-TELEMETRY-001 §5 "deletion within 30 days" requirement.
        // Implementation requires capturing `PostHogSDK.shared.getDistinctId()`
        // BEFORE calling reset() above, then making an authenticated
        // DELETE /api/person/?distinct_id=<id> request against the EU endpoint
        // using a server-side PostHog Personal API key (must not be embedded in
        // the app binary). Implement via a NeuroPulse backend relay endpoint.
        Self.log.debug("PostHogAnalyticsBackend: opted out and distinct_id reset on consent withdrawal.")
    }
}
