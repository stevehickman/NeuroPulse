package life.neurone.core.analytics

// Analytics backend abstraction — port of iOS AnalyticsBackend protocol.
// The concrete production backend (PostHog Android SDK) lives in :app;
// call sites use ResearchAnalyticsGate exclusively and never the SDK directly.

interface AnalyticsBackend {
    fun configure()
    fun reset()
    fun track(event: String, properties: Map<String, String>)

    /**
     * The pseudonymous ID the vendor holds this installation's events under, or null if there is none to erase (nothing was ever
     * sent). The gate reads it before [reset] makes a new one, so the old one can be queued for erasure (`REQ-SW03-19`).
     */
    val distinctId: String? get() = null
}
