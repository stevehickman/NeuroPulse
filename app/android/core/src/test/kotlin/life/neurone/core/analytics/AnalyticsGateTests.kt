package life.neurone.core.analytics

import life.neurone.core.common.InMemoryKeyValueStore
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
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

// ── CharacterisationEnrolmentGate (NP-FW-EMMC-002 §H) ────────────────────
//
// Every negative below is paired with the positive that proves the path exists.
// "No enrolment was recorded" is satisfied just as well by a gate that records
// nothing at all, so each refusal is asserted against a control that succeeds.

class CharacterisationEnrolmentGateTests {

    private fun gates(warrantyOpen: Boolean = true):
        Triple<InMemoryKeyValueStore, WarrantyAnalyticsGate, CharacterisationEnrolmentGate> {
        val store = InMemoryKeyValueStore()
        val warranty = WarrantyAnalyticsGate(store)
        if (warrantyOpen) warranty.grant()
        return Triple(store, warranty, CharacterisationEnrolmentGate(store, warranty))
    }

    @Test
    fun closedUntilAffirmativelyGranted() {
        val (_, _, char) = gates()
        assertFalse(char.isOpen, "warranty consent alone must not enrol a device")
        assertEquals(0, char.consentEpoch)

        // Control: the path works when the owner actually opts in.
        assertEquals(1, char.grant())
        assertTrue(char.isOpen)
    }

    @Test
    fun grantIsRefusedWithoutWarrantyConsent() {
        val (_, _, char) = gates(warrantyOpen = false)
        assertNull(char.grant(), "enrolling a device with no SHDR consent is incoherent")
        assertFalse(char.isOpen)
    }

    @Test
    fun revokingWarrantyConsentClosesCharacterisation() {
        val (_, warranty, char) = gates()
        char.grant()
        assertTrue(char.isOpen)
        warranty.revoke()
        assertFalse(char.isOpen, "the extended set must not outlive SHDR consent itself")
    }

    @Test
    fun withdrawalStopsCollectionButKeepsTheEpochMonotone() {
        val (_, _, char) = gates()
        assertEquals(1, char.grant())
        char.withdraw()
        assertFalse(char.isOpen)
        assertEquals(1, char.consentEpoch, "withdrawal must not reset the epoch")

        // Re-enrolment is a NEW grant and must be distinguishable from the first.
        assertEquals(2, char.grant())
        assertTrue(char.isOpen)
    }

    @Test
    fun enrolmentDoesNotSurviveDeviceTransfer() {
        val (_, _, char) = gates()
        char.grant()
        assertTrue(char.isOpen)
        char.clearForDeviceTransfer()
        assertFalse(char.isOpen, "a previous registrant's consent must not bind a new owner")
        assertEquals(0, char.consentEpoch, "the new owner's first enrolment is genuinely their first")
    }

    @Test
    fun consentToOneProgrammeDoesNotAuthoriseAnother() {
        val (store, _, char) = gates()
        char.grant()
        assertTrue(char.isOpen)
        store.putInt(CharacterisationEnrolmentGate.PROGRAMME_ID_KEY,
                     CharacterisationEnrolmentGate.CURRENT_PROGRAMME_ID + 1)
        assertFalse(char.isOpen, "enrolment in programme N must not carry into N+1")
    }

    @Test
    fun grantingCharacterisationNeverGrantsWarrantyConsent() {
        // The subset relation runs one way only.
        val (store, warranty, char) = gates(warrantyOpen = false)
        char.grant()
        assertFalse(warranty.isOpen)
        assertFalse(store.getBoolean(WarrantyAnalyticsGate.WARRANTY_CONSENT_KEY))
    }

    @Test
    fun keysAreDistinctFromEveryOtherConsentSurface() {
        // CLAUDE.md §6.0: a shared key would make two consents one consent.
        val keys = listOf(
            CharacterisationEnrolmentGate.ENROLMENT_KEY,
            CharacterisationEnrolmentGate.CONSENT_EPOCH_KEY,
            CharacterisationEnrolmentGate.PROGRAMME_ID_KEY,
        )
        assertEquals(keys.size, keys.toSet().size)
        assertFalse(WarrantyAnalyticsGate.WARRANTY_CONSENT_KEY in keys)
        assertFalse(ResearchAnalyticsGate.RESEARCH_ANALYTICS_KEY in keys)
    }
}
