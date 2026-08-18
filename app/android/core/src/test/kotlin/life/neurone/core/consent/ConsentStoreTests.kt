package life.neurone.core.consent

import life.neurone.core.analytics.AnalyticsBackend
import life.neurone.core.analytics.ResearchAnalyticsGate
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.models.ResearchCategory
import life.neurone.core.models.ResearchConsentState
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
            life.neurone.core.models.StudyInvitation(
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

    // ── Blanket coupling at the UI commit path (CLAUDE.md §6.2.5) ────────
    //
    // The two tests above pin the coupling in the explicitly-named withdrawal method. These
    // pin it in updateResearchConsent — the path the consent UI actually uses. Before Rev 37
    // the named method was correct and tested on both platforms while nothing on iOS called
    // it from the UI; merging L2 and L3 onto one commit-at-the-end screen makes the commit
    // path the ordinary way blanket consent is revoked, so it needs its own probe. Both
    // directions are pinned: teardown must fire on a true→false transition and must NOT fire
    // on a category-only edit.

    @Test
    fun commitPathBlanketWithdrawalRevokesResearchAnalytics() {
        val (store, kv, backend) = makeStore()
        store.updateResearchConsent(ResearchConsentState(blanketConsentGranted = true))
        assertEquals(0, backend.resetCount, "granting blanket consent must tear nothing down")

        // The withdrawal the merged S2 screen actually produces.
        store.updateResearchConsent(store.researchConsent.copy(blanketConsentGranted = false))

        assertFalse(store.researchConsent.blanketConsentGranted)
        assertNull(kv.getString(ResearchAnalyticsGate.RESEARCH_ANALYTICS_KEY))
        assertEquals(1, backend.resetCount, "commit path must revoke research analytics")
    }

    @Test
    fun commitPathCategoryOnlyEditDoesNotRevokeResearchAnalytics() {
        val (store, kv, backend) = makeStore()
        // No blanket consent at any point — only L2 scope changes. Guarding on the value
        // rather than the transition would tear down here: the inverse regression.
        store.updateResearchConsent(ResearchConsentState().withAllCategories(true))
        store.updateResearchConsent(store.researchConsent.withAllCategories(false))
        store.updateResearchConsent(
            store.researchConsent.copy(
                categoryConsents = store.researchConsent.categoryConsents +
                    (ResearchCategory.DEPRESSION to true),
            ),
        )

        assertTrue(kv.getBoolean(ResearchAnalyticsGate.RESEARCH_ANALYTICS_KEY))
        assertEquals(0, backend.resetCount, "category-only edits must never revoke analytics")
    }

    @Test
    fun commitPathBlanketStaysOnDoesNotRevokeResearchAnalytics() {
        val (store, kv, backend) = makeStore()
        store.updateResearchConsent(ResearchConsentState(blanketConsentGranted = true))

        store.updateResearchConsent(
            store.researchConsent.copy(
                categoryConsents = store.researchConsent.categoryConsents +
                    (ResearchCategory.SLEEP to true),
            ),
        )

        assertTrue(kv.getBoolean(ResearchAnalyticsGate.RESEARCH_ANALYTICS_KEY))
        assertEquals(0, backend.resetCount, "a true→true blanket state is not a withdrawal")
    }

    // ── L2 and L3 are orthogonal axes (CLAUDE.md §6.2.2 / §6.2.3) ────────

    @Test
    fun selectAllCategoriesDoesNotEnableBlanketConsent() {
        val (store, _, _) = makeStore()
        store.updateResearchConsent(ResearchConsentState().withAllCategories(true))

        assertTrue(store.researchConsent.allCategoriesSelected)
        assertFalse(
            store.researchConsent.blanketConsentGranted,
            "Selecting all nine categories expresses scope, not posture — it must never " +
                "imply blanket consent (§6.2.3).",
        )
    }

    @Test
    fun everythingButAskMeIsRepresentable() {
        // The position the merge must not destroy: all nine categories, still asked each time.
        val (store, _, _) = makeStore()
        store.updateResearchConsent(ResearchConsentState().withAllCategories(true))

        val state = store.researchConsent
        assertTrue(state.allCategoriesSelected)
        assertFalse(state.blanketConsentGranted)
        assertTrue(state.hasAnyResearchConsent)
    }

    @Test
    fun blanketConsentWithoutAnyCategoryIsRepresentable() {
        // The mirror position: pre-approved, no category preference expressed.
        val (store, _, _) = makeStore()
        store.updateResearchConsent(ResearchConsentState(blanketConsentGranted = true))

        val state = store.researchConsent
        assertFalse(state.allCategoriesSelected)
        assertTrue(state.blanketConsentGranted)
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
