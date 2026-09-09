package life.neurone.core.consent

import life.neurone.core.analytics.AnalyticsBackend
import life.neurone.core.analytics.ResearchAnalyticsGate
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.models.ClinicianAccessExpansionRequest
import life.neurone.core.models.ClinicianConsentGrant
import life.neurone.core.models.ClinicianUseCaseTier
import life.neurone.core.models.ResearchCategory
import life.neurone.core.models.ResearchConsentState
import life.neurone.core.models.UHDRElement
import java.time.LocalDate
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

    // ── Clinician access expansion (§6.1, OI-CONSENT-02) ─────────────────
    //
    // The property under test throughout is that the two decisions §6.1 requires to be asked
    // separately are also RECORDED separately. Before this workflow existed, expandClinicianAccess
    // set `tier` and nothing else, and because approvedElements derives from a timeless tier, that
    // handed the clinician the new elements over every session ever recorded — the retroactive
    // decision taken silently, and taken yes.

    private fun grantedYesterday(tier: ClinicianUseCaseTier) = ClinicianConsentGrant(
        id = "grant-1",
        clinicianName = "Dr. Test",
        clinicianOrganization = "Test Hospital",
        tier = tier,
        grantedAtDay = LocalDate.now().minusDays(1).toString(),
    )

    private fun requestTo(tier: ClinicianUseCaseTier) = ClinicianAccessExpansionRequest(
        id = "req-1",
        grantId = "grant-1",
        fromTier = ClinicianUseCaseTier.MONITOR,
        toTier = tier,
        requestedAtDay = LocalDate.now().toString(),
    )

    @Test
    fun approvingForwardOnlyLeavesEarlierSessionsOutOfReach() {
        val (store, _, _) = makeStore()
        store.grantClinicianAccess(grantedYesterday(ClinicianUseCaseTier.MONITOR))
        store.addExpansionRequest(requestTo(ClinicianUseCaseTier.ASSESS))

        store.approveExpansion("req-1", includePriorData = false)

        val grant = store.clinicianGrants.single()
        val yesterday = LocalDate.now().minusDays(1).toString()
        val today = LocalDate.now().toString()

        // The tier moves — it is what the subscription is keyed to — and today's data is in reach.
        assertEquals(ClinicianUseCaseTier.ASSESS, grant.tier)
        assertEquals(ClinicianUseCaseTier.ASSESS.uhdrElements, grant.approvedElements)
        assertEquals(ClinicianUseCaseTier.ASSESS.uhdrElements, grant.elementsVisibleForDataRecordedOn(today))

        // ...but yesterday's sessions still show only what Monitor ever reached. This is the
        // assertion the old implementation could not have passed.
        assertEquals(
            ClinicianUseCaseTier.MONITOR.uhdrElements,
            grant.elementsVisibleForDataRecordedOn(yesterday),
        )
        assertFalse(UHDRElement.EEG_WAVEFORMS in grant.elementsVisibleForDataRecordedOn(yesterday))
        assertEquals(
            ClinicianUseCaseTier.ASSESS.uhdrElements - ClinicianUseCaseTier.MONITOR.uhdrElements,
            grant.forwardOnlyElements,
        )
    }

    @Test
    fun approvingWithPriorDataCoversEarlierSessions() {
        val (store, _, _) = makeStore()
        store.grantClinicianAccess(grantedYesterday(ClinicianUseCaseTier.MONITOR))
        store.addExpansionRequest(requestTo(ClinicianUseCaseTier.ASSESS))

        store.approveExpansion("req-1", includePriorData = true)

        val grant = store.clinicianGrants.single()
        assertEquals(
            ClinicianUseCaseTier.ASSESS.uhdrElements,
            grant.elementsVisibleForDataRecordedOn(LocalDate.now().minusDays(1).toString()),
        )
        assertTrue(grant.forwardOnlyElements.isEmpty())
    }

    @Test
    fun theTwoDecisionsAreRecordedSeparately() {
        val (store, _, _) = makeStore()
        store.grantClinicianAccess(grantedYesterday(ClinicianUseCaseTier.MONITOR))
        store.addExpansionRequest(requestTo(ClinicianUseCaseTier.ASSESS))

        store.approveExpansion("req-1", includePriorData = false)

        // "Approved, and not over history" must be readable back off the record as two answers,
        // not inferred from a single flag.
        val decision = store.expansionRequests.single().decision
        assertTrue(decision is ClinicianAccessExpansionRequest.ExpansionDecision.Approved)
        assertFalse(decision.includesPriorData)
        assertFalse(store.expansionRequests.single().isPending)
    }

    @Test
    fun denyingChangesNothingAboutTheGrant() {
        val (store, _, _) = makeStore()
        store.grantClinicianAccess(grantedYesterday(ClinicianUseCaseTier.MONITOR))
        store.addExpansionRequest(requestTo(ClinicianUseCaseTier.ASSESS))

        store.denyExpansion("req-1")

        assertEquals(ClinicianUseCaseTier.MONITOR, store.clinicianGrants.single().tier)
        assertEquals(
            ClinicianUseCaseTier.MONITOR.uhdrElements,
            store.clinicianGrants.single().approvedElements,
        )
        assertFalse(store.expansionRequests.single().isPending)
    }

    @Test
    fun askingAQuestionIsNotDeciding() {
        val (store, _, _) = makeStore()
        store.grantClinicianAccess(grantedYesterday(ClinicianUseCaseTier.MONITOR))
        store.addExpansionRequest(requestTo(ClinicianUseCaseTier.ASSESS))

        store.askQuestionAboutExpansion("req-1")

        // The notification must survive the question, or the user loses the request by asking
        // about it — and nothing may change about the grant in the meantime.
        assertTrue(store.expansionRequests.single().isPending)
        assertEquals(LocalDate.now().toString(), store.expansionRequests.single().questionSentOnDay)
        assertEquals(ClinicianUseCaseTier.MONITOR, store.clinicianGrants.single().tier)

        // A question does not spend the request: it can still be approved afterwards.
        store.approveExpansion("req-1", includePriorData = false)
        assertEquals(ClinicianUseCaseTier.ASSESS, store.clinicianGrants.single().tier)
    }

    @Test
    fun revokingAGrantWithdrawsItsPendingRequest() {
        val (store, _, _) = makeStore()
        store.grantClinicianAccess(grantedYesterday(ClinicianUseCaseTier.MONITOR))
        store.addExpansionRequest(requestTo(ClinicianUseCaseTier.ASSESS))

        store.revokeClinicianAccess("grant-1")

        assertTrue(store.expansionRequests.isEmpty())
        // And approving it afterwards must not resurrect anything.
        store.approveExpansion("req-1", includePriorData = true)
        assertTrue(store.clinicianGrants.isEmpty())
    }

    @Test
    fun approvingAStaleRequestIsRefused() {
        val (store, _, _) = makeStore()
        store.grantClinicianAccess(grantedYesterday(ClinicianUseCaseTier.MONITOR))
        store.addExpansionRequest(requestTo(ClinicianUseCaseTier.ASSESS))
        // The grant is widened past the request's target by a second, later request.
        store.addExpansionRequest(
            ClinicianAccessExpansionRequest(
                id = "req-2",
                grantId = "grant-1",
                fromTier = ClinicianUseCaseTier.MONITOR,
                toTier = ClinicianUseCaseTier.FULL_CLINICAL,
                requestedAtDay = LocalDate.now().toString(),
            ),
        )
        store.approveExpansion("req-2", includePriorData = false)

        // req-1 was superseded when req-2 arrived, so there is nothing to approve; and even if it
        // were reachable, ASSESS no longer adds anything to a FULL_CLINICAL grant.
        store.approveExpansion("req-1", includePriorData = true)
        assertEquals(ClinicianUseCaseTier.FULL_CLINICAL, store.clinicianGrants.single().tier)
        assertEquals(
            ClinicianUseCaseTier.FULL_CLINICAL.uhdrElements -
                ClinicianUseCaseTier.MONITOR.uhdrElements,
            store.clinicianGrants.single().forwardOnlyElements,
        )
    }

    @Test
    fun aNewRequestSupersedesTheOutstandingOneButNotDecidedHistory() {
        val (store, _, _) = makeStore()
        store.grantClinicianAccess(grantedYesterday(ClinicianUseCaseTier.MONITOR))
        store.addExpansionRequest(requestTo(ClinicianUseCaseTier.ASSESS))
        store.denyExpansion("req-1")

        store.addExpansionRequest(
            ClinicianAccessExpansionRequest(
                id = "req-2",
                grantId = "grant-1",
                fromTier = ClinicianUseCaseTier.MONITOR,
                toTier = ClinicianUseCaseTier.ASSESS,
                requestedAtDay = LocalDate.now().toString(),
            ),
        )

        // The denial stays on the record; only the outstanding request is superseded.
        assertEquals(2, store.expansionRequests.size)
        assertEquals(1, store.expansionRequests.count { it.isPending })
    }

    @Test
    fun expansionRequestsAndScopesSurviveAReload() {
        val kv = InMemoryKeyValueStore()
        val gate = ResearchAnalyticsGate(kv, RecordingBackend())
        val store1 = ConsentStore(kv, gate)
        store1.grantClinicianAccess(grantedYesterday(ClinicianUseCaseTier.MONITOR))
        store1.addExpansionRequest(requestTo(ClinicianUseCaseTier.ASSESS))
        store1.approveExpansion("req-1", includePriorData = false)

        // §6.1 asks for a *persistent* notification, and a retroactive answer that only lives in
        // memory is not an answer. This also exercises the sealed ExpansionDecision serializer.
        val store2 = ConsentStore(kv, gate)
        val decision = store2.expansionRequests.single().decision
        assertTrue(decision is ClinicianAccessExpansionRequest.ExpansionDecision.Approved)
        assertFalse(decision.includesPriorData)
        assertEquals(
            ClinicianUseCaseTier.MONITOR.uhdrElements,
            store2.clinicianGrants.single()
                .elementsVisibleForDataRecordedOn(LocalDate.now().minusDays(1).toString()),
        )
    }

    @Test
    fun aGrantWithNoScopesKeepsItsPreWorkflowMeaning() {
        // A grant written before the workflow existed has no scopes. Introducing the workflow must
        // not silently re-scope it: a bare tier has always meant "these elements, all history".
        val grant = grantedYesterday(ClinicianUseCaseTier.ASSESS)
        assertEquals(null, grant.accessScopes)
        assertEquals(ClinicianUseCaseTier.ASSESS.uhdrElements, grant.approvedElements)
        assertEquals(
            ClinicianUseCaseTier.ASSESS.uhdrElements,
            grant.elementsVisibleForDataRecordedOn("2020-01-01"),
        )
        assertTrue(grant.forwardOnlyElements.isEmpty())
    }

    @Test
    fun aChangeThatIsNotAnExpansionHasNoDifferential() {
        val monitor = grantedYesterday(ClinicianUseCaseTier.MONITOR)
        // Nothing new.
        assertEquals(null, ConsentEngine.accessDifferential(monitor, ClinicianUseCaseTier.MONITOR))
        // Something lost — a re-scope, not a widening.
        val full = grantedYesterday(ClinicianUseCaseTier.FULL_CLINICAL)
        assertEquals(null, ConsentEngine.accessDifferential(full, ClinicianUseCaseTier.ASSESS))
        // Either side Research: its element set is IRB-defined per study descriptor, not derivable
        // from the tier, so the emptiness must never be read as a set.
        assertEquals(null, ConsentEngine.accessDifferential(monitor, ClinicianUseCaseTier.RESEARCH))
        assertEquals(
            null,
            ConsentEngine.accessDifferential(
                grantedYesterday(ClinicianUseCaseTier.RESEARCH),
                ClinicianUseCaseTier.FULL_CLINICAL,
            ),
        )
    }

    @Test
    fun theDifferentialSplitsElementsIntoNewAlreadyVisibleAndWithheld() {
        val differential = ConsentEngine.accessDifferential(
            grantedYesterday(ClinicianUseCaseTier.MONITOR),
            ClinicianUseCaseTier.ASSESS,
        )!!

        assertEquals(
            ClinicianUseCaseTier.ASSESS.uhdrElements - ClinicianUseCaseTier.MONITOR.uhdrElements,
            differential.newlyVisibleElements,
        )
        assertEquals(ClinicianUseCaseTier.MONITOR.uhdrElements, differential.alreadyVisibleElements)
        assertEquals(
            UHDRElement.entries.toSet() - ClinicianUseCaseTier.ASSESS.uhdrElements,
            differential.stillNotAccessibleElements,
        )
        // The three sets partition the element roster: nothing is in two of them, nothing is lost.
        assertEquals(
            UHDRElement.entries.toSet(),
            differential.newlyVisibleElements + differential.alreadyVisibleElements +
                differential.stillNotAccessibleElements,
        )
        assertEquals(
            UHDRElement.entries.size,
            differential.newlyVisibleElements.size + differential.alreadyVisibleElements.size +
                differential.stillNotAccessibleElements.size,
        )
    }

    @Test
    fun corruptPersistedBlobDoesNotCrash() {
        val kv = InMemoryKeyValueStore()
        kv.putString(ConsentStore.RESEARCH_KEY, "{not json")
        val store = ConsentStore(kv, ResearchAnalyticsGate(kv, RecordingBackend()))
        assertFalse(store.researchConsent.hasAnyResearchConsent)
    }
}
