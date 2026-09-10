package life.neurone.core.consent

import life.neurone.core.analytics.AnalyticsBackend
import life.neurone.core.analytics.ResearchAnalyticsGate
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.models.ClinicianAccessExpansionRequest
import life.neurone.core.models.ClinicianConsentGrant
import life.neurone.core.models.ClinicianUseCaseTier
import life.neurone.core.models.ResearchCategory
import life.neurone.core.models.ResearchConsentState
import life.neurone.core.models.StudyDescriptor
import life.neurone.core.models.StudyDescriptorAdmission
import life.neurone.core.models.StudyDescriptorVerification
import life.neurone.core.models.StudyDescriptorVerifier
import life.neurone.core.models.StudyInvitation
import life.neurone.core.models.UHDRElement
import java.time.LocalDate
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertIs
import kotlin.test.assertNotNull
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
        val kv = InMemoryKeyValueStore()
        kv.putBoolean(ResearchAnalyticsGate.RESEARCH_ANALYTICS_KEY, true)
        val backend = RecordingBackend()
        val gate = ResearchAnalyticsGate(kv, backend)
        gate.configure()
        val store = ConsentStore(kv, gate, descriptorVerifier = verifying(HASH))
        store.updateResearchConsent(categoryConsent(ResearchCategory.SLEEP))

        store.ingestStudyDescriptor(descriptor(categories = listOf(ResearchCategory.SLEEP)))
        store.acceptInvitation(STUDY_ID)
        assertTrue(store.studyParticipations.single().isActive)

        store.withdrawFromStudy(STUDY_ID)

        assertFalse(store.studyParticipations.single().isActive)
        assertTrue(kv.getBoolean(ResearchAnalyticsGate.RESEARCH_ANALYTICS_KEY))
        assertEquals(0, backend.resetCount)
    }

    // ── Study descriptor ingestion (§6.3 / OI-CONSENT-03) ────────────────
    //
    // The gate the old `addInvitation` did not have. Each refusal below is a decision that
    // method took silently and took in the affirmative. Peer of iOS ConsentStoreTests.

    private fun verifying(hash: String) =
        StudyDescriptorVerifier { StudyDescriptorVerification.Verified(hash) }

    private fun ingestingStore(
        verification: StudyDescriptorVerification = StudyDescriptorVerification.Verified(HASH),
        consent: ResearchConsentState = categoryConsent(ResearchCategory.DEPRESSION),
    ): Pair<ConsentStore, InMemoryKeyValueStore> {
        val kv = InMemoryKeyValueStore()
        val gate = ResearchAnalyticsGate(kv, RecordingBackend())
        val store = ConsentStore(kv, gate, descriptorVerifier = { verification })
        store.updateResearchConsent(consent)
        return store to kv
    }

    private fun categoryConsent(category: ResearchCategory) = ResearchConsentState(
        contactConsentGranted = true,
        categoryConsents = ResearchCategory.entries.associateWith { it == category },
    )

    private fun blanketConsent() = ResearchConsentState(
        contactConsentGranted = true,
        blanketConsentGranted = true,
    )

    private fun descriptor(
        categories: List<ResearchCategory> = listOf(ResearchCategory.DEPRESSION),
        elements: Set<UHDRElement> = setOf(
            UHDRElement.SESSION_TIMESTAMPS, UHDRElement.EEG_WAVEFORMS,
        ),
        k: Int = 10,
        rounding: Int = 7,
    ) = StudyDescriptor(
        studyId = STUDY_ID,
        studyTitle = "Test Study",
        researchCategories = categories,
        requestedElements = elements,
        kAnonymity = k,
        dateRoundingDays = rounding,
        issuedOnDay = LocalDate.now().toString(),
        signature = "sig",
    )

    private fun refusalOf(admission: StudyDescriptorAdmission): StudyDescriptorAdmission.Reason =
        assertIs<StudyDescriptorAdmission.Refused>(admission).reason

    /**
     * The state of every device today, and the reason the verifier is injected rather than
     * assumed: with no signing key nothing can be checked, so nothing is shown.
     */
    @Test
    fun withNoVerifierConfiguredEveryDescriptorIsRefused() {
        val kv = InMemoryKeyValueStore()
        val store = ConsentStore(kv, ResearchAnalyticsGate(kv, RecordingBackend()))
        store.updateResearchConsent(blanketConsent())

        assertEquals(
            StudyDescriptorAdmission.Reason.VERIFIER_UNAVAILABLE,
            refusalOf(store.ingestStudyDescriptor(descriptor())),
        )
        assertTrue(store.pendingInvitations.isEmpty())
        assertTrue(store.studyParticipations.isEmpty())
    }

    @Test
    fun forgedDescriptorNeverReachesTheUser() {
        val (store, _) = ingestingStore(StudyDescriptorVerification.Rejected, blanketConsent())
        assertEquals(
            StudyDescriptorAdmission.Reason.SIGNATURE_INVALID,
            refusalOf(store.ingestStudyDescriptor(descriptor())),
        )
        assertTrue(store.pendingInvitations.isEmpty())
    }

    /**
     * §5.3 locks k≥10 and ≥1-week date rounding, and the device is the only place they can be
     * checked. A study below the floor is not one the user may be *asked* about.
     */
    @Test
    fun anonymisationBelowTheLockedFloorIsRefused() {
        val (store, _) = ingestingStore(consent = blanketConsent())
        assertEquals(
            StudyDescriptorAdmission.Reason.ANONYMISATION_BELOW_FLOOR,
            refusalOf(store.ingestStudyDescriptor(descriptor(k = 9))),
        )
        assertEquals(
            StudyDescriptorAdmission.Reason.ANONYMISATION_BELOW_FLOOR,
            refusalOf(store.ingestStudyDescriptor(descriptor(rounding = 6))),
        )
        assertTrue(store.pendingInvitations.isEmpty())
    }

    /**
     * The signature is checked before anything the descriptor *claims*, so a forged descriptor
     * asking for impossible anonymisation is refused as forged, not as out-of-range.
     */
    @Test
    fun signatureIsCheckedBeforeTheDescriptorsOwnClaims() {
        val (store, _) = ingestingStore(StudyDescriptorVerification.Rejected, blanketConsent())
        assertEquals(
            StudyDescriptorAdmission.Reason.SIGNATURE_INVALID,
            refusalOf(store.ingestStudyDescriptor(descriptor(k = 1))),
        )
    }

    @Test
    fun categoryTheUserDidNotOptIntoIsRefused() {
        val (store, _) = ingestingStore()
        assertEquals(
            StudyDescriptorAdmission.Reason.CATEGORY_NOT_CONSENTED,
            refusalOf(store.ingestStudyDescriptor(descriptor(categories = listOf(ResearchCategory.SLEEP)))),
        )
    }

    /**
     * §6.2.1: a contact method is the shared precondition for all three delivery paths, so L1
     * gates the engagement notification too — not only the invitation that asks a question.
     */
    @Test
    fun contactConsentGatesBothPostures() {
        val (store, _) = ingestingStore(
            consent = blanketConsent().copy(contactConsentGranted = false),
        )
        assertEquals(
            StudyDescriptorAdmission.Reason.NO_CONTACT_CONSENT,
            refusalOf(store.ingestStudyDescriptor(descriptor())),
        )
    }

    @Test
    fun deviceWithNoResearchConsentAtAllIsRefused() {
        val (store, _) = ingestingStore(consent = ResearchConsentState())
        assertEquals(
            StudyDescriptorAdmission.Reason.NO_RESEARCH_CONSENT,
            refusalOf(store.ingestStudyDescriptor(descriptor())),
        )
    }

    // ── The two postures (§6.2 L2 vs L3) ─────────────────────────────────

    /** L2 consent means the user is *asked*: they are not in the study until they answer. */
    @Test
    fun categoryConsentUserIsAskedAndIsNotInTheStudyUntilTheyAnswer() {
        val (store, _) = ingestingStore()
        val admission = assertIs<StudyDescriptorAdmission.Admitted>(
            store.ingestStudyDescriptor(descriptor()),
        )

        assertEquals(StudyInvitation.Posture.CONSENT_REQUEST, admission.posture)
        assertEquals(HASH, admission.descriptorHash)
        assertFalse(store.pendingInvitations.single().participatesWithoutAnswer)
        assertTrue(
            store.studyParticipations.isEmpty(),
            "An unanswered consent request must not enrol the user.",
        )

        store.acceptInvitation(STUDY_ID)
        assertEquals(1, store.studyParticipations.size)
    }

    /**
     * L3 blanket consent means the user is *told*: §6.2 locks that they "still receive per-study
     * engagement notifications, not consent requests", so they are in the study on arrival.
     */
    @Test
    fun blanketConsentUserIsToldAndIsInTheStudyOnArrival() {
        val (store, _) = ingestingStore(consent = blanketConsent())
        val admission = assertIs<StudyDescriptorAdmission.Admitted>(
            store.ingestStudyDescriptor(descriptor(categories = listOf(ResearchCategory.SLEEP))),
        )

        assertEquals(StudyInvitation.Posture.ENGAGEMENT_NOTIFICATION, admission.posture)
        assertTrue(store.pendingInvitations.single().participatesWithoutAnswer)
        assertEquals(
            1, store.studyParticipations.size,
            "L3 pre-approved the study, so the audit trail must say the user is in it.",
        )
        assertTrue(store.studyParticipations.single().isActive)
    }

    /**
     * The L3 opt-out. "Decline" from an engagement notification is a withdrawal, because the user
     * is already enrolled — the one place the two postures must not share behaviour.
     */
    @Test
    fun leavingAnEngagementNotificationWithdrawsTheParticipation() {
        val (store, _) = ingestingStore(consent = blanketConsent())
        store.ingestStudyDescriptor(descriptor())

        store.declineInvitation(STUDY_ID)

        assertFalse(store.studyParticipations.single().isActive)
        assertFalse(store.pendingInvitations.single().isOpen)
    }

    /**
     * An engagement notification was never a question, so it cannot be answered "yes" — that
     * would file a second participation for a study the user is already in.
     */
    @Test
    fun engagementNotificationCannotBeAccepted() {
        val (store, _) = ingestingStore(consent = blanketConsent())
        store.ingestStudyDescriptor(descriptor())

        store.acceptInvitation(STUDY_ID)

        assertEquals(1, store.studyParticipations.size)
        assertNull(store.pendingInvitations.single().decision)
    }

    // ── §6.3 step 4's third response ─────────────────────────────────────

    @Test
    fun askingAQuestionAboutAStudyIsNotDeciding() {
        val (store, _) = ingestingStore()
        store.ingestStudyDescriptor(descriptor())

        store.askQuestionAboutInvitation(STUDY_ID)

        assertNotNull(store.pendingInvitations.single().questionSentOnDay)
        assertTrue(
            store.pendingInvitations.single().isOpen,
            "Asking is not answering: the invitation must stay in the inbox.",
        )
        assertTrue(store.studyParticipations.isEmpty())

        store.acceptInvitation(STUDY_ID)
        assertEquals(1, store.studyParticipations.size)
    }

    // ── Persistence and re-ingestion ─────────────────────────────────────

    /**
     * Invitations were in-memory only. An accepted study's participation record outlived the
     * invitation that recorded the acceptance, so the trail and the inbox disagreed after every
     * restart — and the same descriptor could be ingested and accepted a second time.
     */
    @Test
    fun invitationsAndTheirDecisionsSurviveAReload() {
        val (store, kv) = ingestingStore()
        store.ingestStudyDescriptor(descriptor())
        store.askQuestionAboutInvitation(STUDY_ID)

        val reloaded = ConsentStore(kv, ResearchAnalyticsGate(kv, RecordingBackend()))
        assertEquals(1, reloaded.pendingInvitations.size)
        assertNotNull(reloaded.pendingInvitations.single().questionSentOnDay)
        assertEquals(
            StudyInvitation.Posture.CONSENT_REQUEST,
            reloaded.pendingInvitations.single().posture,
        )
    }

    /**
     * §5.3 and §6.3 step 6: withdrawal blocks future descriptor processing. Re-presenting a study
     * the user already answered would be the device forgetting the answer.
     */
    @Test
    fun studyAlreadyAnsweredIsNotPresentedAgain() {
        val (store, _) = ingestingStore()
        store.ingestStudyDescriptor(descriptor())
        store.declineInvitation(STUDY_ID)

        assertEquals(
            StudyDescriptorAdmission.Reason.STUDY_ALREADY_DECIDED,
            refusalOf(store.ingestStudyDescriptor(descriptor())),
        )
        assertEquals(1, store.pendingInvitations.size)
    }

    @Test
    fun withdrawnStudyIsNotPresentedAgain() {
        val (store, _) = ingestingStore()
        store.ingestStudyDescriptor(descriptor())
        store.acceptInvitation(STUDY_ID)
        store.withdrawFromStudy(STUDY_ID)

        assertEquals(
            StudyDescriptorAdmission.Reason.STUDY_ALREADY_DECIDED,
            refusalOf(store.ingestStudyDescriptor(descriptor())),
        )
        assertEquals(1, store.studyParticipations.size)
    }

    // ── What the consent surface says ────────────────────────────────────

    /**
     * §6.3 step 3 requires the invitation to be explicit about what researchers CANNOT see. It is
     * the complement of the approved set, computed here — not prose supplied by the party asking
     * for access.
     */
    @Test
    fun cannotSeeListIsDerivedFromTheApprovedSet() {
        val (store, _) = ingestingStore()
        store.ingestStudyDescriptor(
            descriptor(elements = setOf(UHDRElement.SESSION_TIMESTAMPS)),
        )

        val invitation = store.pendingInvitations.single()
        assertEquals(setOf(UHDRElement.SESSION_TIMESTAMPS), invitation.approvedElements)
        assertEquals(
            UHDRElement.entries.toSet() - UHDRElement.SESSION_TIMESTAMPS,
            invitation.cannotLearn,
        )
    }

    /**
     * The §5.3 audit trail names a descriptor. It used to record the literal string "pending",
     * because there was no descriptor to hash.
     */
    @Test
    fun participationRecordCarriesTheDescriptorHash() {
        val (store, _) = ingestingStore()
        store.ingestStudyDescriptor(descriptor())
        store.acceptInvitation(STUDY_ID)

        assertEquals(HASH, store.studyParticipations.single().descriptorHash)
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

    private companion object {
        const val STUDY_ID = "TEST-001"
        const val HASH = "sha256:abc"
    }
}
