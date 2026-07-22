package life.neurone.core.analytics

import life.neurone.core.common.InMemoryKeyValueStore
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

private class SpyAnalyticsBackend : AnalyticsBackend {
    val calls = mutableListOf<String>()
    val trackedEvents = mutableListOf<Pair<String, Map<String, String>>>()

    override fun configure() { calls += "configure" }
    override fun reset() { calls += "reset" }
    override fun track(event: String, properties: Map<String, String>) {
        calls += "track"
        trackedEvents += event to properties
    }
}

class AnalyticsGateTests {

    // ── ResearchAnalyticsGate (ISC-19, 20, 21) ───────────────────────────

    @Test
    fun configureNoOpsWhenConsentKeyUnset() {
        val backend = SpyAnalyticsBackend()
        val gate = ResearchAnalyticsGate(InMemoryKeyValueStore(), backend)
        gate.configure()
        assertTrue(backend.calls.isEmpty())
    }

    @Test
    fun configureRunsExactlyOnceWhenGateOpen() {
        val store = InMemoryKeyValueStore()
        store.putBoolean(ResearchAnalyticsGate.RESEARCH_ANALYTICS_KEY, true)
        val backend = SpyAnalyticsBackend()
        val gate = ResearchAnalyticsGate(store, backend)
        gate.configure()
        gate.configure()
        assertEquals(listOf("configure"), backend.calls)
    }

    @Test
    fun trackDropsEventsWithProhibitedKeys() {
        val store = InMemoryKeyValueStore()
        store.putBoolean(ResearchAnalyticsGate.RESEARCH_ANALYTICS_KEY, true)
        val backend = SpyAnalyticsBackend()
        val gate = ResearchAnalyticsGate(store, backend)

        // Every prohibited key must cause a full drop — identical set to iOS.
        for (key in listOf(
            "eeg", "hrv", "rmssd", "coherence", "session_id", "protocol_id",
            "session_count", "session_sequence",
            "imp", "impedance", "impedance_flags", "pass_flags",
        )) {
            gate.track("evt", mapOf(key to "1", "ok" to "2"))
        }
        assertTrue(backend.trackedEvents.isEmpty())

        gate.track("evt", mapOf("engagement_tier" to "new"))
        assertEquals(1, backend.trackedEvents.size)
    }

    @Test
    fun trackNoOpsWhenGateClosed() {
        val backend = SpyAnalyticsBackend()
        val gate = ResearchAnalyticsGate(InMemoryKeyValueStore(), backend)
        gate.track("evt", mapOf("engagement_tier" to "new"))
        assertTrue(backend.trackedEvents.isEmpty())
    }

    @Test
    fun resetTearsDownBackendBeforeClearingConfiguredState() {
        val store = InMemoryKeyValueStore()
        store.putBoolean(ResearchAnalyticsGate.RESEARCH_ANALYTICS_KEY, true)
        val backend = SpyAnalyticsBackend()
        val gate = ResearchAnalyticsGate(store, backend)
        gate.configure()
        gate.reset()
        assertEquals(listOf("configure", "reset"), backend.calls)
        // reset() when not configured is a no-op
        gate.reset()
        assertEquals(listOf("configure", "reset"), backend.calls)
    }

    // ── Two consent subjects never share keys (ISC-24) ───────────────────

    @Test
    fun warrantyAndResearchGatesUseDistinctKeys() {
        assertTrue(
            WarrantyAnalyticsGate.WARRANTY_CONSENT_KEY !=
                ResearchAnalyticsGate.RESEARCH_ANALYTICS_KEY,
        )
    }

    @Test
    fun warrantyGateIndependentOfResearchKey() {
        val store = InMemoryKeyValueStore()
        val warranty = WarrantyAnalyticsGate(store)
        warranty.grant()
        assertTrue(warranty.isOpen)
        // Clearing the research key has no effect on warranty consent.
        store.remove(ResearchAnalyticsGate.RESEARCH_ANALYTICS_KEY)
        assertTrue(warranty.isOpen)
        warranty.revoke()
        assertFalse(warranty.isOpen)
    }

    // ── EngagementTier boundaries (ISC-25) ───────────────────────────────

    @Test
    fun engagementTierBucketsMatchSpec() {
        val store = InMemoryKeyValueStore()
        fun tierAt(count: Int): EngagementTier {
            store.putInt(EngagementTier.LAUNCH_COUNT_KEY, count)
            return EngagementTier.current(store)
        }
        assertEquals(EngagementTier.NEW, tierAt(0))
        assertEquals(EngagementTier.NEW, tierAt(5))
        assertEquals(EngagementTier.ACTIVE, tierAt(6))
        assertEquals(EngagementTier.ACTIVE, tierAt(50))
        assertEquals(EngagementTier.ESTABLISHED, tierAt(51))
    }

    @Test
    fun incrementLaunchCountIncrements() {
        val store = InMemoryKeyValueStore()
        EngagementTier.incrementLaunchCount(store)
        EngagementTier.incrementLaunchCount(store)
        assertEquals(2, store.getInt(EngagementTier.LAUNCH_COUNT_KEY))
    }
}
