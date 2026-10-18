package life.neurone.core.analytics

import life.neurone.core.common.KeyValueStore
import life.neurone.core.protocol.PersistedKeys

/**
 * Consent gate for research analytics and crash-reporting telemetry.
 * Port of iOS ResearchAnalyticsGate (NP-APP-TELEMETRY-001 Rev B).
 *
 * No analytics or crash-reporting SDK may initialize or receive any event until
 * the user has ACTIVELY completed the consent onboarding flow (tapped Done —
 * Skip does NOT open the gate). Withdrawing blanket research consent (L3)
 * clears the gate key and tears down the SDK.
 *
 * Entirely independent of [WarrantyAnalyticsGate] (SHDR fleet telemetry under
 * warranty-owner consent). The two consent subjects are never mixed and never
 * share a persistence key (CLAUDE.md §6.0).
 */
class ResearchAnalyticsGate(
    private val store: KeyValueStore,
    private val backend: AnalyticsBackend,
) {
    /** Idempotency flag so configure() only ever starts the SDK once. */
    private var isConfigured = false

    /** True only when the user actively completed the research consent flow. */
    val isOpen: Boolean
        get() = store.getBoolean(PersistedKeys.RESEARCH_ANALYTICS_KEY)

    /**
     * Initialize the analytics SDK. Call exactly once, after the research
     * consent flow completes. No-ops if already configured or gate closed.
     */
    fun configure() {
        if (isConfigured) return
        if (!isOpen) return
        isConfigured = true
        backend.configure()
    }

    /**
     * Close the research analytics gate and tear down the SDK.
     * Tears down the backend BEFORE clearing the configured flag so a
     * concurrent configure() cannot re-initialize before shutdown completes
     * (order preserved from the iOS implementation).
     * Does NOT affect [WarrantyAnalyticsGate] or SHDR fleet uploads.
     */
    fun reset() {
        if (!isConfigured) return
        backend.reset()
        isConfigured = false
    }

    /**
     * Record a research analytics event. Silently no-ops when the gate is
     * closed. Drops the event entirely (does NOT transmit) unless [AnalyticsEventPolicy]
     * permits it: a property the telemetry policy does not name, or a value not of
     * its form, drops the whole event (an allowlist, NP-APP-TELEMETRY-001 §3), and
     * so does an event that cannot be checked.
     */
    fun track(event: String, properties: Map<String, String>) {
        if (!isOpen) return
        if (!AnalyticsEventPolicy.permits(event, properties)) return
        backend.track(event, properties)
    }
}
