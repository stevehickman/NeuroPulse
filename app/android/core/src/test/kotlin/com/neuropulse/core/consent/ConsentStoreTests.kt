package com.neuropulse.core.consent

import com.neuropulse.core.analytics.AnalyticsBackend
import com.neuropulse.core.analytics.ResearchAnalyticsGate
import com.neuropulse.core.common.InMemoryKeyValueStore
import com.neuropulse.core.models.ResearchCategory
import com.neuropulse.core.models.ResearchConsentState
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

private class RecordingBackend : AnalyticsBackend {
    var resetCount = 0
    override fun configure() {}
    override fun reset() { resetCount++ }
    override fun track(event: String, properties: Map<String, String>) {}
}

class ConsentStoreTests {

    private fun makeStore(): Triple<ConsentStore, InMemoryKeyValueStore, RecordingBackend> {
        val kv = InMemoryKeyValueStore()
        kv.putBoolean(ResearchAnalyticsGate.RESEARCH_ANALYTICS_KEY, true)
        val backend = RecordingBackend()
        val gate = ResearchAnalyticsGate(kv, backend)
        gate.configure()
        return Triple(ConsentStore(kv, gate), kv, backend)
    }

    // ISC-22: blanket withdrawal clears the analytics key AND resets the gate.
    @Test
    fun blanketWithdrawalRevokesResearchAnalytics() {
        val (store, kv, backend) = makeStore()
        store.updateResearchConsent(ResearchConsentState(blanketConsentGranted = true))

        store.withdrawBlanketResearchConsent()

        assertFalse(store.researchConsent.blanketConsentGranted)
        assertNull(kv.getString(ResearchAnalyticsGate.RESEARCH_ANALYTICS_KEY))
        assertFalse(kv.getBoolean(ResearchAnalyticsGate.RESEARCH_ANALYTICS_KEY))
        assertEquals(1, backend.resetCount)
    }

    // ISC-23: partial withdrawals do NOT revoke research analytics.
    @Test
    fun categoryWithdrawalDoesNotRevokeResearchAnalytics() {
        val (store, kv, backend) = makeStore()
        store.setCategoryConsent(ResearchCategory.SLEEP, true)

        store.setCategoryConsent(ResearchCategory.SLEEP, false)

        assertTrue(kv.getBoolean(ResearchAnalyticsGate.RESEARCH_ANALYTICS_KEY))
        assertEquals(0, backend.resetCount)
    }

    @Test
    fun studyWithdrawalDoesNotRevokeResearchAnalytics() {
        val (store, kv, backend) = makeStore()
        store.addInvitation(
            com.neuropulse.core.models.StudyInvitation(
                studyId = "S1", studyTitle = "Sleep Study",
                researchCategories = listOf(ResearchCategory.SLEEP),
                approvedElements = emptySet(), cannotLearn = emptyList(),
                irreversibilityNotice = "notice",
            ),
        )
        store.acceptInvitation(studyId = "S1")
        assertTrue(store.studyParticipations.single().isActive)

        store.withdrawFromStudy(studyId = "S1")

        assertFalse(store.studyParticipations.single().isActive)
        assertTrue(kv.getBoolean(ResearchAnalyticsGate.RESEARCH_ANALYTICS_KEY))
        assertEquals(0, backend.resetCount)
    }

    @Test
    fun consentStatePersistsAcrossInstances() {
        val kv = InMemoryKeyValueStore()
        val gate = ResearchAnalyticsGate(kv, RecordingBackend())
        val store1 = ConsentStore(kv, gate)
        store1.setCategoryConsent(ResearchCategory.DEPRESSION, true)

        val store2 = ConsentStore(kv, gate)
        assertEquals(true, store2.researchConsent.categoryConsents[ResearchCategory.DEPRESSION])
        assertTrue(store2.researchConsent.hasAnyResearchConsent)
    }

    @Test
    fun corruptPersistedBlobDoesNotCrash() {
        val kv = InMemoryKeyValueStore()
        kv.putString(ConsentStore.RESEARCH_KEY, "{not json")
        val store = ConsentStore(kv, ResearchAnalyticsGate(kv, RecordingBackend()))
        assertFalse(store.researchConsent.hasAnyResearchConsent)
    }
}
