package life.neurone.core.analytics

import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.protocol.PersistedKeys
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

    // @verifies REQ-CONSENT-14
    @Test
    fun configureNoOpsWhenConsentKeyUnset() {
        val backend = SpyAnalyticsBackend()
        val gate = ResearchAnalyticsGate(InMemoryKeyValueStore(), backend)
        gate.configure()
        assertTrue(backend.calls.isEmpty())
    }

    // @verifies REQ-SW03-07
    @Test
    fun configureRunsExactlyOnceWhenGateOpen() {
        val store = InMemoryKeyValueStore()
        store.putBoolean(PersistedKeys.RESEARCH_ANALYTICS_KEY, true)
        val backend = SpyAnalyticsBackend()
        val gate = ResearchAnalyticsGate(store, backend)
        gate.configure()
        gate.configure()
        assertEquals(listOf("configure"), backend.calls)
    }

    private fun openGate(): Pair<ResearchAnalyticsGate, SpyAnalyticsBackend> {
        val store = InMemoryKeyValueStore()
        store.putBoolean(PersistedKeys.RESEARCH_ANALYTICS_KEY, true)
        val backend = SpyAnalyticsBackend()
        return ResearchAnalyticsGate(store, backend) to backend
    }

    // @verifies REQ-SW03-07
    @Test
    fun trackSendsAnEventWhoseNameAndPropertiesAreAllOnThePermittedList() {
        val (gate, backend) = openGate()
        val props = mapOf(
            "app_version" to "1.0.3", "os_version" to "Android 15.0", "device_class" to "Android_mid",
            "screen_name" to "protocol_detail", "engagement_tier" to "established", "app_start_type" to "warm_start",
            "onboarding_step" to "consent_flow_step_2", "error_code" to "12", "connectivity_type" to "cellular",
        )
        gate.track("app_opened", props)
        gate.track("app_foregrounded", emptyMap())
        assertEquals(listOf("app_opened" to props, "app_foregrounded" to emptyMap()), backend.trackedEvents)
    }

    // @verifies REQ-SW03-07
    @Test
    fun trackDropsAnEventWithAPropertyTheTelemetryPolicyDoesNotName() {
        val (gate, backend) = openGate()
        // The twelve keys the old denylist knew, and ones it never thought of: an allowlist drops all of them.
        for (key in listOf(
            "eeg", "hrv", "rmssd", "coherence", "session_id", "protocol_id", "session_count", "session_sequence",
            "imp", "impedance", "impedance_flags", "pass_flags",
            "heart_rate", "alpha_power", "electrode_site", "condition", "email", "duration_s", "timestamp",
        )) {
            gate.track("evt", mapOf(key to "1", "engagement_tier" to "new"))
        }
        assertTrue(backend.trackedEvents.isEmpty())
        gate.track("evt", mapOf("engagement_tier" to "new"))
        assertEquals(1, backend.trackedEvents.size)
    }

    // @verifies REQ-SW03-07
    @Test
    fun trackDropsAnEventWithAValueNotOfItsPropertysFormOrAnEventNameThatCarriesAHealthTerm() {
        val (gate, backend) = openGate()
        gate.track("evt", mapOf("engagement_tier" to "42"))            // the raw count the tier replaced
        gate.track("evt", mapOf("screen_name" to "tdcs_depression"))   // a protocol or condition as a screen
        gate.track("evt", mapOf("error_code" to "1746000000000"))      // a timestamp in an error code
        gate.track("evt", mapOf("os_version" to "Android 15.0.1"))     // a patch version
        gate.track("tdcs_started", emptyMap())                         // a modality in the event name
        gate.track("User Email Sent", emptyMap())                      // not snake_case
        assertTrue(backend.trackedEvents.isEmpty())
    }

    // @verifies REQ-CONSENT-14
    @Test
    fun trackNoOpsWhenGateClosed() {
        val backend = SpyAnalyticsBackend()
        val gate = ResearchAnalyticsGate(InMemoryKeyValueStore(), backend)
        gate.track("evt", mapOf("engagement_tier" to "new"))
        assertTrue(backend.trackedEvents.isEmpty())
    }

    // @verifies REQ-SW03-07
    @Test
    fun resetTearsDownBackendBeforeClearingConfiguredState() {
        val store = InMemoryKeyValueStore()
        store.putBoolean(PersistedKeys.RESEARCH_ANALYTICS_KEY, true)
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

    // @verifies REQ-SW02-04
    @Test
    fun warrantyAndResearchGatesUseDistinctKeys() {
        assertTrue(
            WarrantyAnalyticsGate.WARRANTY_CONSENT_KEY !=
                PersistedKeys.RESEARCH_ANALYTICS_KEY,
        )
    }

    // @verifies REQ-CONSENT-01#b
    @Test
    fun warrantyGateIndependentOfResearchKey() {
        val store = InMemoryKeyValueStore()
        val warranty = WarrantyAnalyticsGate(store)
        warranty.grant()
        assertTrue(warranty.isOpen)
        // Clearing the research key has no effect on warranty consent.
        store.remove(PersistedKeys.RESEARCH_ANALYTICS_KEY)
        assertTrue(warranty.isOpen)
        warranty.revoke()
        assertFalse(warranty.isOpen)
    }

    // ── EngagementTier boundaries (ISC-25) ───────────────────────────────

    // @verifies REQ-SW03-07
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

    // @verifies REQ-SW03-07
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

    // @verifies REQ-SW02-04
    @Test
    fun closedUntilAffirmativelyGranted() {
        val (_, _, char) = gates()
        assertFalse(char.isOpen, "warranty consent alone must not enrol a device")
        assertEquals(0, char.consentEpoch)

        // Control: the path works when the owner actually opts in.
        assertEquals(1, char.grant())
        assertTrue(char.isOpen)
    }

    // @verifies REQ-SW02-04
    @Test
    fun grantIsRefusedWithoutWarrantyConsent() {
        val (_, _, char) = gates(warrantyOpen = false)
        assertNull(char.grant(), "enrolling a device with no SHDR consent is incoherent")
        assertFalse(char.isOpen)
    }

    // @verifies REQ-SW02-04
    @Test
    fun revokingWarrantyConsentClosesCharacterisation() {
        val (_, warranty, char) = gates()
        char.grant()
        assertTrue(char.isOpen)
        warranty.revoke()
        assertFalse(char.isOpen, "the extended set must not outlive SHDR consent itself")
    }

    // @verifies REQ-SW02-04
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

    // @verifies REQ-SW02-04
    @Test
    fun enrolmentDoesNotSurviveDeviceTransfer() {
        val (_, _, char) = gates()
        char.grant()
        assertTrue(char.isOpen)
        char.clearForDeviceTransfer()
        assertFalse(char.isOpen, "a previous registrant's consent must not bind a new owner")
        assertEquals(0, char.consentEpoch, "the new owner's first enrolment is genuinely their first")
    }

    // @verifies REQ-SW02-04
    @Test
    fun consentToOneProgrammeDoesNotAuthoriseAnother() {
        val (store, _, char) = gates()
        char.grant()
        assertTrue(char.isOpen)
        store.putInt(PersistedKeys.PROGRAMME_ID_KEY,
                     PersistedKeys.CURRENT_PROGRAMME_ID + 1)
        assertFalse(char.isOpen, "enrolment in programme N must not carry into N+1")
    }

    // @verifies REQ-SW02-04
    @Test
    fun grantingCharacterisationNeverGrantsWarrantyConsent() {
        // The subset relation runs one way only.
        val (store, warranty, char) = gates(warrantyOpen = false)
        char.grant()
        assertFalse(warranty.isOpen)
        assertFalse(store.getBoolean(WarrantyAnalyticsGate.WARRANTY_CONSENT_KEY))
    }

    // @verifies REQ-SW02-04
    @Test
    fun keysAreDistinctFromEveryOtherConsentSurface() {
        // CLAUDE.md §6.0: a shared key would make two consents one consent.
        val keys = listOf(
            PersistedKeys.ENROLMENT_KEY,
            PersistedKeys.CONSENT_EPOCH_KEY,
            PersistedKeys.PROGRAMME_ID_KEY,
        )
        assertEquals(keys.size, keys.toSet().size)
        assertFalse(WarrantyAnalyticsGate.WARRANTY_CONSENT_KEY in keys)
        assertFalse(PersistedKeys.RESEARCH_ANALYTICS_KEY in keys)
    }
}
