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
import life.neurone.core.protocol.PersistedKeys
import java.time.LocalDate
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertIs
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
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
        kv.putBoolean(PersistedKeys.RESEARCH_ANALYTICS_KEY, true)
        val backend = RecordingBackend()
        val gate = ResearchAnalyticsGate(kv, backend)
        gate.configure()
        return Triple(ConsentStore(kv, gate), kv, backend)
    }

    // ISC-22: blanket withdrawal clears the analytics key AND resets the gate.
    // @verifies REQ-CONSENT-02#b
    @Test
    fun blanketWithdrawalRevokesResearchAnalytics() {
        val (store, kv, backend) = makeStore()
        store.updateResearchConsent(ResearchConsentState(blanketConsentGranted = true))

        store.withdrawBlanketResearchConsent()

        assertFalse(store.researchConsent.blanketConsentGranted)
        assertNull(kv.getString(PersistedKeys.RESEARCH_ANALYTICS_KEY))
        assertFalse(kv.getBoolean(PersistedKeys.RESEARCH_ANALYTICS_KEY))
        assertEquals(1, backend.resetCount)
    }

    // ISC-23: partial withdrawals do NOT revoke research analytics.
    // @verifies REQ-CONSENT-02#a
    @Test
    fun categoryWithdrawalDoesNotRevokeResearchAnalytics() {
        val (store, kv, backend) = makeStore()
        store.setCategoryConsent(ResearchCategory.SLEEP, true)

        store.setCategoryConsent(ResearchCategory.SLEEP, false)

        assertTrue(kv.getBoolean(PersistedKeys.RESEARCH_ANALYTICS_KEY))
        assertEquals(0, backend.resetCount)
    }

    // @verifies REQ-CONSENT-02#a
    @Test
    fun studyWithdrawalDoesNotRevokeResearchAnalytics() {
        val kv = InMemoryKeyValueStore()
        kv.putBoolean(PersistedKeys.RESEARCH_ANALYTICS_KEY, true)
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
        assertTrue(kv.getBoolean(PersistedKeys.RESEARCH_ANALYTICS_KEY))
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
    // @verifies REQ-CONSENT-09#a REQ-CONSENT-10#c
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

    // @verifies REQ-CONSENT-09#a
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
    // @verifies REQ-CONSENT-09#b
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
    // @verifies REQ-CONSENT-09#a
    @Test
    fun signatureIsCheckedBeforeTheDescriptorsOwnClaims() {
        val (store, _) = ingestingStore(StudyDescriptorVerification.Rejected, blanketConsent())
        assertEquals(
            StudyDescriptorAdmission.Reason.SIGNATURE_INVALID,
            refusalOf(store.ingestStudyDescriptor(descriptor(k = 1))),
        )
    }

    // @verifies REQ-CONSENT-09#g
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
    // @verifies REQ-CONSENT-09#e
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

    // @verifies REQ-CONSENT-09#d
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
    // @verifies REQ-CONSENT-11#a
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
    // @verifies REQ-CONSENT-11#a
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
    // @verifies REQ-CONSENT-11#c
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
    // @verifies REQ-CONSENT-11#b
    @Test
    fun engagementNotificationCannotBeAccepted() {
        val (store, _) = ingestingStore(consent = blanketConsent())
        store.ingestStudyDescriptor(descriptor())

        store.acceptInvitation(STUDY_ID)

        assertEquals(1, store.studyParticipations.size)
        assertNull(store.pendingInvitations.single().decision)
    }

    // ── §6.3 step 4's third response ─────────────────────────────────────

    // @verifies REQ-CONSENT-11#d
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
    // @verifies REQ-CONSENT-11
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
    // @verifies REQ-CONSENT-09#f
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

    // @verifies REQ-CONSENT-09#f
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
    // @verifies REQ-CONSENT-12
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

    // ── "Never asked" vs "said stop" (§6.0) ──────────────────────────────
    //
    // `blanketConsentGranted == false` is true of a user who never turned L3 on and of one who
    // turned it off. §6.0 treats them differently — withdrawal "stops ALL research data flows" —
    // so the flag alone cannot decide admission.

    /**
     * The defect the marker exists to fix. Withdrawal does not un-tick the nine L2 categories, so
     * before this a withdrawn user's stale checkboxes kept admitting studies as consent requests —
     * research data flows continuing after §6.0 says they all stop.
     */
    // @verifies REQ-CONSENT-04#e REQ-CONSENT-09#c
    @Test
    fun withdrawnBlanketConsentStopsStudiesTheStaleCategoriesWouldStillAdmit() {
        val (store, _) = ingestingStore(
            consent = blanketConsent().copy(
                categoryConsents = ResearchCategory.entries.associateWith { true },
            ),
        )
        store.withdrawBlanketResearchConsent()
        assertTrue(
            store.researchConsent.categoryConsents.values.all { it },
            "withdrawal deliberately leaves L2 alone; the marker is what must outrank it",
        )

        assertEquals(
            StudyDescriptorAdmission.Reason.RESEARCH_CONSENT_WITHDRAWN,
            refusalOf(store.ingestStudyDescriptor(descriptor())),
        )
    }

    /** The other direction: a user who was never asked is not a user who said stop. */
    // @verifies REQ-CONSENT-04#d
    @Test
    fun neverGrantingBlanketConsentLeavesCategoryConsentWorking() {
        val (store, _) = ingestingStore()
        assertNull(store.researchConsent.blanketConsentWithdrawnOnDay)

        val admission = assertIs<StudyDescriptorAdmission.Admitted>(
            store.ingestStudyDescriptor(descriptor()),
        )
        assertEquals(StudyInvitation.Posture.CONSENT_REQUEST, admission.posture)
    }

    /** Re-granting is a fresh decision, so it clears the condition rather than being outranked. */
    // @verifies REQ-CONSENT-04#f
    @Test
    fun reGrantingBlanketConsentResumesAdmission() {
        val (store, _) = ingestingStore(consent = blanketConsent())
        store.withdrawBlanketResearchConsent()
        store.updateResearchConsent(store.researchConsent.copy(blanketConsentGranted = true))

        val admission = assertIs<StudyDescriptorAdmission.Admitted>(
            store.ingestStudyDescriptor(descriptor()),
        )
        assertEquals(StudyInvitation.Posture.ENGAGEMENT_NOTIFICATION, admission.posture)
        assertNotNull(
            store.researchConsent.blanketConsentWithdrawnOnDay,
            "the withdrawal stays in the record; it is simply no longer in force",
        )
    }

    /** The commit path the merged S2 screen actually uses must set the marker too. */
    // @verifies REQ-CONSENT-04#a
    @Test
    fun commitPathWithdrawalSetsTheMarker() {
        val (store, _) = ingestingStore(consent = blanketConsent())
        store.updateResearchConsent(store.researchConsent.copy(blanketConsentGranted = false))

        assertNotNull(store.researchConsent.blanketConsentWithdrawnOnDay)
        assertTrue(store.researchConsent.blanketConsentWithdrawn)
    }

    /** A category-only edit is not a withdrawal — the §6.2.5 guard shape, applied to the marker. */
    // @verifies REQ-CONSENT-04#b
    @Test
    fun categoryOnlyEditDoesNotSetTheMarker() {
        val (store, _) = ingestingStore()
        store.updateResearchConsent(
            store.researchConsent.copy(
                categoryConsents = ResearchCategory.entries.associateWith { true },
            ),
        )

        assertNull(store.researchConsent.blanketConsentWithdrawnOnDay)
        assertFalse(store.researchConsent.blanketConsentWithdrawn)
    }

    /**
     * The marker is the store's, not the screen's. A UI that commits a stale state — one read
     * before the withdrawal — must not erase the record of the user having said stop.
     */
    // @verifies REQ-CONSENT-04#c
    @Test
    fun aStaleCommitCannotClearTheMarker() {
        val (store, _) = ingestingStore(consent = blanketConsent())
        val staleSnapshot = store.researchConsent
        store.withdrawBlanketResearchConsent()

        store.updateResearchConsent(staleSnapshot.copy(blanketConsentGranted = false))

        assertNotNull(store.researchConsent.blanketConsentWithdrawnOnDay)
        assertEquals(
            StudyDescriptorAdmission.Reason.RESEARCH_CONSENT_WITHDRAWN,
            refusalOf(store.ingestStudyDescriptor(descriptor())),
        )
    }

    /** Withdrawing something never granted stops nothing, so it must not bar an L2 participant. */
    // @verifies REQ-CONSENT-04#d
    @Test
    fun withdrawingBlanketConsentThatWasNeverGrantedDoesNotBarStudies() {
        val (store, _) = ingestingStore()
        store.withdrawBlanketResearchConsent()

        assertNull(store.researchConsent.blanketConsentWithdrawnOnDay)
        assertIs<StudyDescriptorAdmission.Admitted>(store.ingestStudyDescriptor(descriptor()))
    }

    // @verifies REQ-CONSENT-04
    @Test
    fun theWithdrawalMarkerSurvivesAReload() {
        val (store, kv) = ingestingStore(consent = blanketConsent())
        store.withdrawBlanketResearchConsent()

        val reloaded = ConsentStore(kv, ResearchAnalyticsGate(kv, RecordingBackend()))
        assertNotNull(reloaded.researchConsent.blanketConsentWithdrawnOnDay)
        assertTrue(reloaded.researchConsent.blanketConsentWithdrawn)
    }

    // ── Withdrawal is permanent for every data period (REQ-CONSENT-13) ───
    //
    // The device has the admission gate and nothing past it: no extract is generated or sent yet
    // (OI-CONSENT-07). These pin what exists: once a user has withdrawn, nothing a study says about
    // itself (its issue date, its categories, the elements it asks for) gets it past the gate.

    /**
     * A descriptor carries no "data period" the gate could be talked into: the only date on it is
     * when it was issued. Withdrawal must hold for a study issued long before, just before or long
     * after the withdrawal, which are the sessions that predate it and the ones that follow.
     */
    // @verifies REQ-CONSENT-13#a
    @Test
    fun blanketWithdrawalRefusesEveryStudyWhateverItsDateOrScope() {
        val (store, _) = ingestingStore(consent = blanketConsent())
        assertIs<StudyDescriptorAdmission.Admitted>(store.ingestStudyDescriptor(descriptor()))
        store.withdrawBlanketResearchConsent()

        val today = LocalDate.now()
        val issued = listOf("2020-01-01", today.minusDays(1).toString(), today.plusDays(30).toString())
        issued.forEachIndexed { i, day ->
            val d = descriptor(
                categories = ResearchCategory.entries,
                elements = UHDRElement.entries.toSet(),
            ).copy(studyId = "AFTER-WITHDRAWAL-$i", issuedOnDay = day)
            assertEquals(
                StudyDescriptorAdmission.Reason.RESEARCH_CONSENT_WITHDRAWN,
                refusalOf(store.ingestStudyDescriptor(d)),
                "a study issued $day must not be processed after withdrawal",
            )
        }
        assertEquals(1, store.pendingInvitations.size, "no invitation was created after withdrawal")
        assertEquals(1, store.studyParticipations.size, "and no participation was recorded")
    }

    /**
     * The study the user had already joined is no exception: withdrawal outranks "already joined",
     * so a re-issued descriptor for it is refused as withdrawn, not quietly treated as known.
     */
    // @verifies REQ-CONSENT-13#a
    @Test
    fun withdrawalAlsoRefusesAReissueOfAStudyAlreadyJoined() {
        val (store, _) = ingestingStore(consent = blanketConsent())
        store.ingestStudyDescriptor(descriptor())
        store.withdrawBlanketResearchConsent()

        val reissued = descriptor(elements = UHDRElement.entries.toSet())
            .copy(issuedOnDay = LocalDate.now().plusDays(1).toString())
        assertEquals(
            StudyDescriptorAdmission.Reason.RESEARCH_CONSENT_WITHDRAWN,
            refusalOf(store.ingestStudyDescriptor(reissued)),
        )
    }

    /** Permanent means across a relaunch too: the refusal comes from persisted state. */
    // @verifies REQ-CONSENT-13#a
    @Test
    fun withdrawalStillRefusesEveryStudyAfterARelaunch() {
        val (store, kv) = ingestingStore(consent = blanketConsent())
        store.withdrawBlanketResearchConsent()

        // A fresh store over the same persisted state, with a verifier that accepts everything, so
        // the only thing between the descriptor and the user is the withdrawal.
        val reloaded = ConsentStore(
            kv, ResearchAnalyticsGate(kv, RecordingBackend()),
            descriptorVerifier = { StudyDescriptorVerification.Verified(HASH) },
        )
        assertEquals(
            StudyDescriptorAdmission.Reason.RESEARCH_CONSENT_WITHDRAWN,
            refusalOf(reloaded.ingestStudyDescriptor(descriptor().copy(issuedOnDay = "2020-01-01"))),
        )
        assertEquals(
            StudyDescriptorAdmission.Reason.RESEARCH_CONSENT_WITHDRAWN,
            refusalOf(reloaded.ingestStudyDescriptor(descriptor())),
        )
    }

    /**
     * Withdrawing from one study is permanent for that study: a re-issue that is newer and asks
     * for more is still the study the user left.
     */
    // @verifies REQ-CONSENT-13#b
    @Test
    fun withdrawnStudyStaysWithdrawnWhateverTheReissueSays() {
        val (store, kv) = ingestingStore()
        store.ingestStudyDescriptor(descriptor())
        store.acceptInvitation(STUDY_ID)
        store.withdrawFromStudy(STUDY_ID)
        assertFalse(store.studyParticipations[0].isActive)

        val reissued = descriptor(elements = UHDRElement.entries.toSet())
            .copy(issuedOnDay = LocalDate.now().plusDays(30).toString())
        assertEquals(
            StudyDescriptorAdmission.Reason.STUDY_ALREADY_DECIDED,
            refusalOf(store.ingestStudyDescriptor(reissued)),
        )

        val reloaded = ConsentStore(
            kv, ResearchAnalyticsGate(kv, RecordingBackend()),
            descriptorVerifier = { StudyDescriptorVerification.Verified(HASH) },
        )
        assertEquals(
            StudyDescriptorAdmission.Reason.STUDY_ALREADY_DECIDED,
            refusalOf(reloaded.ingestStudyDescriptor(reissued)),
        )
        assertFalse(reloaded.studyParticipations[0].isActive)
    }

    /**
     * The §5.3 audit trail names a descriptor. It used to record the literal string "pending",
     * because there was no descriptor to hash.
     */
    // @verifies REQ-CONSENT-09
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

    // @verifies REQ-CONSENT-03#a
    @Test
    fun commitPathBlanketWithdrawalRevokesResearchAnalytics() {
        val (store, kv, backend) = makeStore()
        store.updateResearchConsent(ResearchConsentState(blanketConsentGranted = true))
        assertEquals(0, backend.resetCount, "granting blanket consent must tear nothing down")

        // The withdrawal the merged S2 screen actually produces.
        store.updateResearchConsent(store.researchConsent.copy(blanketConsentGranted = false))

        assertFalse(store.researchConsent.blanketConsentGranted)
        assertNull(kv.getString(PersistedKeys.RESEARCH_ANALYTICS_KEY))
        assertEquals(1, backend.resetCount, "commit path must revoke research analytics")
    }

    // @verifies REQ-CONSENT-03#b
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

        assertTrue(kv.getBoolean(PersistedKeys.RESEARCH_ANALYTICS_KEY))
        assertEquals(0, backend.resetCount, "category-only edits must never revoke analytics")
    }

    // @verifies REQ-CONSENT-03#c
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

        assertTrue(kv.getBoolean(PersistedKeys.RESEARCH_ANALYTICS_KEY))
        assertEquals(0, backend.resetCount, "a true→true blanket state is not a withdrawal")
    }

    // ── L2 and L3 are orthogonal axes (CLAUDE.md §6.2.2 / §6.2.3) ────────

    // @verifies REQ-CONSENT-05#a
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

    // @verifies REQ-CONSENT-05#b
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

    // @verifies REQ-CONSENT-05#c
    @Test
    fun blanketConsentWithoutAnyCategoryIsRepresentable() {
        // The mirror position: pre-approved, no category preference expressed.
        val (store, _, _) = makeStore()
        store.updateResearchConsent(ResearchConsentState(blanketConsentGranted = true))

        val state = store.researchConsent
        assertFalse(state.allCategoriesSelected)
        assertTrue(state.blanketConsentGranted)
    }

    // @verifies REQ-CONSENT-05#d
    @Test
    fun allFourConsentLayersRemainIndependentlySettable() {
        // Four layers, two screens. Each layer still has its own field and can be set without touching the others.
        val (store, kv, _) = makeStore()
        store.updateResearchConsent(
            ResearchConsentState(
                contactConsentGranted = true, // L1
                categoryConsents = ResearchCategory.entries.associateWith { it == ResearchCategory.entries.first() }, // L2
                blanketConsentGranted = true, // L3
                resultsOptIn = true, // L4
                suggestionPortalOptIn = false, // L4
            ),
        )

        val reloaded = ConsentStore(kv, ResearchAnalyticsGate(kv, RecordingBackend())).researchConsent
        assertTrue(reloaded.contactConsentGranted)
        assertEquals(true, reloaded.categoryConsents[ResearchCategory.entries.first()])
        assertTrue(reloaded.blanketConsentGranted)
        assertTrue(reloaded.resultsOptIn)
        assertFalse(reloaded.suggestionPortalOptIn)
    }

    // @verifies REQ-CONSENT-02
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

    // @verifies REQ-CONSENT-16#b
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

    // @verifies REQ-CONSENT-16#a
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

    // @verifies REQ-CONSENT-16#a
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

    // @verifies REQ-CONSENT-16
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

    // @verifies REQ-CONSENT-16
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

    // @verifies REQ-CONSENT-16
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

    // @verifies REQ-CONSENT-16
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

    // @verifies REQ-CONSENT-16
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

    // @verifies REQ-CONSENT-16
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

    // @verifies REQ-CONSENT-16#c
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

    // @verifies REQ-CONSENT-16
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

    // @verifies REQ-CONSENT-02
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

    // @verifies REQ-CONSENT-02
    @Test
    fun corruptPersistedBlobDoesNotCrash() {
        val kv = InMemoryKeyValueStore()
        kv.putString(ConsentStore.RESEARCH_KEY, "{not json")
        val store = ConsentStore(kv, ResearchAnalyticsGate(kv, RecordingBackend()))
        assertFalse(store.researchConsent.hasAnyResearchConsent)
    }

    // ── OI-CONSENT-04: the use-case selection decides the grant ──────────────────

    /// The defect the item was raised about: the form collected a use-case selection and built a
    /// grant from the tier, so two different answers produced the same access.
    // @verifies REQ-CONSENT-15#a
    @Test
    fun useCaseSelectionNarrowsAccessBelowTheTierCeiling() {
        val narrow = ConsentEngine.initialAccessScope(
            selectedUseCaseIds = setOf("adherence_monitoring"),
            tier = ClinicianUseCaseTier.FULL_CLINICAL,
            grantedAtDay = LocalDate.now().toString(),
            includesPriorData = false,
        )
        assertEquals(ClinicianUseCaseTier.MONITOR.uhdrElements, narrow.elements)
        assertTrue(ClinicianUseCaseTier.FULL_CLINICAL.uhdrElements.containsAll(narrow.elements))
        assertFalse(narrow.elements == ClinicianUseCaseTier.FULL_CLINICAL.uhdrElements)
    }

    // @verifies REQ-CONSENT-15#b
    @Test
    fun useCasesOfferedAreCappedByTheTier() {
        assertEquals(
            listOf("adherence_monitoring"),
            ConsentEngine.useCasesAvailableFor(ClinicianUseCaseTier.MONITOR).map { it.id },
        )
        assertEquals(
            listOf("adherence_monitoring", "eeg_review"),
            ConsentEngine.useCasesAvailableFor(ClinicianUseCaseTier.ASSESS).map { it.id },
        )
        assertEquals(3, ConsentEngine.useCasesAvailableFor(ClinicianUseCaseTier.FULL_CLINICAL).size)
        // Research is empty rather than everything: its elements are IRB-defined per study
        // descriptor, and the emptiness must never be read as a set.
        assertTrue(ConsentEngine.useCasesAvailableFor(ClinicianUseCaseTier.RESEARCH).isEmpty())
    }

    /// A caller that skips [ConsentEngine.useCasesAvailableFor] still cannot exceed the ceiling
    /// the subscription was sold at.
    // @verifies REQ-CONSENT-15#b
    @Test
    fun selectionAboveTheTierIsClampedNotHonoured() {
        val scope = ConsentEngine.initialAccessScope(
            selectedUseCaseIds = setOf("adherence_monitoring", "hrv_outcomes"),
            tier = ClinicianUseCaseTier.MONITOR,
            grantedAtDay = LocalDate.now().toString(),
            includesPriorData = false,
        )
        assertEquals(ClinicianUseCaseTier.MONITOR.uhdrElements, scope.elements)
        assertFalse(UHDRElement.HRV_TIME_SERIES in scope.elements)
    }

    // @verifies REQ-CONSENT-15#c
    @Test
    fun researchTierYieldsNoElementsWhateverIsSelected() {
        val scope = ConsentEngine.initialAccessScope(
            selectedUseCaseIds = setOf("adherence_monitoring", "eeg_review", "hrv_outcomes"),
            tier = ClinicianUseCaseTier.RESEARCH,
            grantedAtDay = LocalDate.now().toString(),
            includesPriorData = true,
        )
        assertTrue(scope.elements.isEmpty())
    }

    /// Every library entry must sit at or above the highest tier its own elements require, or the
    /// use case promises access its tier cannot carry and the clamp silently removes it.
    // @verifies REQ-CONSENT-15
    @Test
    fun everyUseCaseIsReachableAtItsDeclaredTier() {
        for (useCase in USE_CASE_LIBRARY) {
            assertTrue(
                useCase.tier.uhdrElements.containsAll(useCase.requiredElements),
                "${useCase.id} requires elements its declared tier does not carry",
            )
        }
    }

    /// The library is a cross-platform table: a grant records the IDs it was made from, and the
    /// two platforms read each other's grants. Pins the IDs so a rename on one side is caught.
    // @verifies REQ-CONSENT-15
    @Test
    fun theUseCaseLibraryMatchesTheIosTable() {
        assertEquals(
            listOf("adherence_monitoring", "eeg_review", "hrv_outcomes"),
            USE_CASE_LIBRARY.map { it.id },
        )
        for (useCase in USE_CASE_LIBRARY) {
            val upper = useCase.id.uppercase()
            assertEquals("CLINICIAN_USECASE_${upper}_NAME", useCase.titleKey)
            assertEquals("CLINICIAN_USECASE_${upper}_DESC", useCase.descriptionKey)
        }
    }

    // ── OI-CONSENT-06: the two decisions stay apart on an initial grant ──────────

    // @verifies REQ-CONSENT-16#a,b
    @Test
    fun initialScopeCarriesTheRetroactiveAnswerBothWays() {
        val today = LocalDate.now().toString()
        val forwardOnly = ConsentEngine.initialAccessScope(
            setOf("eeg_review"), ClinicianUseCaseTier.ASSESS, today, includesPriorData = false,
        )
        val includingHistory = ConsentEngine.initialAccessScope(
            setOf("eeg_review"), ClinicianUseCaseTier.ASSESS, today, includesPriorData = true,
        )
        // The history answer changes from when, never what.
        assertEquals(forwardOnly.elements, includingHistory.elements)
        assertFalse(forwardOnly.includesPriorData)
        assertTrue(includingHistory.includesPriorData)
    }

    /// The point of the item: "widen it going forward, leave my earlier sessions alone" has to be
    /// representable on a FIRST grant, not only on an expansion.
    // @verifies REQ-CONSENT-16#b
    @Test
    fun forwardOnlyInitialGrantHidesEarlierSessions() {
        val today = LocalDate.now().toString()
        val yesterday = LocalDate.now().minusDays(1).toString()
        val grant = grantedYesterday(ClinicianUseCaseTier.ASSESS).copy(
            grantedAtDay = today,
            accessScopes = listOf(
                ConsentEngine.initialAccessScope(
                    setOf("eeg_review"), ClinicianUseCaseTier.ASSESS, today,
                    includesPriorData = false,
                ),
            ),
            useCaseIds = listOf("eeg_review"),
        )
        assertTrue(grant.elementsVisibleForDataRecordedOn(yesterday).isEmpty())
        assertEquals(grant.approvedElements, grant.elementsVisibleForDataRecordedOn(today))
        assertEquals(grant.approvedElements, grant.forwardOnlyElements)
    }

    /// A grant written before `OI-CONSENT-04`/`-06` keeps meaning what it meant: the tier's
    /// elements, from the grant day, prior data included.
    // @verifies REQ-CONSENT-16#c
    @Test
    fun preWorkflowGrantStillFallsBackToTheTier() {
        val legacy = grantedYesterday(ClinicianUseCaseTier.ASSESS)
        assertNull(legacy.useCaseIds)
        assertEquals(ClinicianUseCaseTier.ASSESS.uhdrElements, legacy.approvedElements)
        assertEquals(
            ClinicianUseCaseTier.ASSESS.uhdrElements,
            legacy.elementsVisibleForDataRecordedOn(LocalDate.now().minusDays(30).toString()),
        )
    }

    /// The elements are frozen at grant time, so editing the library cannot reach backwards into
    /// a grant already made.
    // @verifies REQ-CONSENT-15
    @Test
    fun storedScopeDoesNotTrackTheLibrary() {
        val today = LocalDate.now().toString()
        val grant = grantedYesterday(ClinicianUseCaseTier.FULL_CLINICAL).copy(
            accessScopes = listOf(
                ConsentEngine.initialAccessScope(
                    setOf("adherence_monitoring"), ClinicianUseCaseTier.FULL_CLINICAL, today,
                    includesPriorData = false,
                ),
            ),
            useCaseIds = listOf("adherence_monitoring"),
        )
        assertEquals(ClinicianUseCaseTier.MONITOR.uhdrElements, grant.approvedElements)
    }

    // @verifies REQ-CONSENT-02
    @Test
    fun tierRankOrdersTheCeilings() {
        assertTrue(ClinicianUseCaseTier.MONITOR.rank < ClinicianUseCaseTier.ASSESS.rank)
        assertTrue(ClinicianUseCaseTier.ASSESS.rank < ClinicianUseCaseTier.FULL_CLINICAL.rank)
        // Research outranks the clinical ladder despite having no elements — ranking by element
        // count would put it at the bottom.
        assertTrue(ClinicianUseCaseTier.FULL_CLINICAL.rank < ClinicianUseCaseTier.RESEARCH.rank)
    }

    // ── OI-CONSENT-05: the clinician-portal channel seam ─────────────────────────

    /// The shipped channel refuses everything, and says the anchor is missing rather than that the
    /// request is forged. A default that admitted would make the missing key silent.
    // @verifies REQ-CONSENT-09
    @Test
    fun theDefaultPortalChannelRefusesAndNamesTheMissingAnchor() {
        val (store, _, _) = makeStore()
        store.grantClinicianAccess(grantedYesterday(ClinicianUseCaseTier.MONITOR))

        val outcome = ClinicianPortalSync(store).ingest(requestTo(ClinicianUseCaseTier.ASSESS))

        assertTrue(outcome is ClinicianRequestAdmission.RefusedUnverifiable)
        assertTrue("OI-CONSENT-05" in outcome.reason)
        assertTrue(store.expansionRequests.isEmpty())
    }

    // @verifies REQ-CONSENT-09
    @Test
    fun aVerifiedRequestIsIngested() {
        val (store, _, _) = makeStore()
        store.grantClinicianAccess(grantedYesterday(ClinicianUseCaseTier.MONITOR))
        val sync = ClinicianPortalSync(store) { ClinicianRequestVerification.Verified }

        assertEquals(
            ClinicianRequestAdmission.Admitted,
            sync.ingest(requestTo(ClinicianUseCaseTier.ASSESS)),
        )
        assertEquals(1, store.expansionRequests.count { it.isPending })
    }

    /// Identity is checked before anything the request claims. A forged request naming a grant
    /// this device never issued is refused as forged — so the refusal cannot be read backwards to
    /// learn which grants the device holds (CLAUDE.md §5.1 rule 2).
    // @verifies REQ-CONSENT-09
    @Test
    fun aForgedRequestIsRefusedAsForgedNotAsUnknownGrant() {
        val (store, _, _) = makeStore()
        val sync = ClinicianPortalSync(store) { ClinicianRequestVerification.Rejected }

        assertTrue(store.clinicianGrants.isEmpty())
        assertEquals(
            ClinicianRequestAdmission.RefusedForgedRequest,
            sync.ingest(requestTo(ClinicianUseCaseTier.ASSESS)),
        )
    }

    // @verifies REQ-CONSENT-09
    @Test
    fun aVerifiedRequestForAnUnknownGrantIsRefused() {
        val (store, _, _) = makeStore()
        val sync = ClinicianPortalSync(store) { ClinicianRequestVerification.Verified }

        assertEquals(
            ClinicianRequestAdmission.RefusedUnknownGrant,
            sync.ingest(requestTo(ClinicianUseCaseTier.ASSESS)),
        )
        assertTrue(store.expansionRequests.isEmpty())
    }

    /// A change with no differential has no consent document, so there is nothing the user could
    /// be asked — the same condition `approveExpansion` already fails closed on.
    // @verifies REQ-CONSENT-09
    @Test
    fun aVerifiedRequestThatIsNotAnExpansionIsRefused() {
        val (store, _, _) = makeStore()
        store.grantClinicianAccess(grantedYesterday(ClinicianUseCaseTier.FULL_CLINICAL))
        val sync = ClinicianPortalSync(store) { ClinicianRequestVerification.Verified }

        assertEquals(
            ClinicianRequestAdmission.RefusedNotAnExpansion,
            sync.ingest(requestTo(ClinicianUseCaseTier.ASSESS)),
        )
        assertTrue(store.expansionRequests.isEmpty())
    }

    private companion object {
        const val STUDY_ID = "TEST-001"
        const val HASH = "sha256:abc"
    }

    // ── Withdrawal records that travel in a migration (SWR-MG-04) ───────────────

    private fun clocked(vararg times: String): ConsentStore {
        val kv = InMemoryKeyValueStore()
        val it = times.iterator()
        return ConsentStore(kv, ResearchAnalyticsGate(kv, RecordingBackend()), now = { it.next() })
    }

    // @verifies SWR-MG-04
    @Test
    fun aWithdrawalIsStampedAtTheTransitionAndNeverErasedByARegrant() {
        val s = clocked("2026-03-01T00:00:00Z", "2026-03-02T00:00:00Z", "2026-03-03T00:00:00Z")
        s.updateResearchConsent(ResearchConsentState(blanketConsentGranted = true, contactConsentGranted = true))
        assertNull(s.researchConsent.blanketConsentWithdrawnAt, "never asked is not said stop")
        s.updateResearchConsent(ResearchConsentState(contactConsentGranted = true))
        assertEquals("2026-03-01T00:00:00Z", s.researchConsent.blanketConsentWithdrawnAt)
        assertNull(s.researchConsent.contactConsentWithdrawnAt)
        // A screen that commits a stale state cannot erase it, and a re-grant leaves the record.
        s.updateResearchConsent(ResearchConsentState(blanketConsentGranted = true, contactConsentGranted = true))
        assertEquals("2026-03-01T00:00:00Z", s.researchConsent.blanketConsentWithdrawnAt)
        s.setCategoryConsent(ResearchCategory.entries.first(), true)
        s.setCategoryConsent(ResearchCategory.entries.first(), false)
        assertEquals("2026-03-02T00:00:00Z", s.researchConsent.categoryWithdrawnAt[ResearchCategory.entries.first()])
    }

    // @verifies SWR-MG-04
    @Test
    fun aRestoredWithdrawalSwitchesAGrantOffAndNothingTurnsOneOn() {
        val s = clocked()
        s.updateResearchConsent(
            ResearchConsentState(
                blanketConsentGranted = true,
                contactConsentGranted = true,
                categoryConsents = ResearchCategory.entries.associateWith { true },
            ),
        )
        val c = ResearchCategory.entries.first()
        s.applyWithdrawals(
            buildJsonObject {
                put("blanketConsentWithdrawnAt", "2026-03-01T00:00:00Z")
                put("categoryWithdrawals", buildJsonObject { put(c.wireName, "2026-03-01T00:00:00Z") })
                put("studyWithdrawals", buildJsonObject { put("study-a", "2026-02-01T00:00:00Z") })
            },
        )
        assertFalse(s.researchConsent.blanketConsentGranted)
        assertTrue(s.researchConsent.contactConsentGranted, "a withdrawal that was not made leaves the grant alone")
        assertFalse(s.researchConsent.categoryConsents.getValue(c))
        assertEquals("2026-03-01", s.researchConsent.blanketConsentWithdrawnOnDay)
        assertTrue("study-a" in s.studyWithdrawals)

        // The same record again, or an older one, changes nothing: a grant made afterwards stands.
        s.updateResearchConsent(s.researchConsent.copy(blanketConsentGranted = true))
        s.applyWithdrawals(buildJsonObject { put("blanketConsentWithdrawnAt", "2026-03-01T00:00:00Z") })
        assertTrue(s.researchConsent.blanketConsentGranted)
        // An empty record (a bundle from a phone that never withdrew) turns nothing on or off.
        s.applyWithdrawals(buildJsonObject { })
        assertTrue(s.researchConsent.blanketConsentGranted)
        // And what survives a restart.
        val kv = InMemoryKeyValueStore()
        val first = ConsentStore(kv, ResearchAnalyticsGate(kv, RecordingBackend()))
        first.applyWithdrawals(buildJsonObject { put("studyWithdrawals", buildJsonObject { put("s", "2026-01-01T00:00:00Z") }) })
        assertEquals(mapOf("s" to "2026-01-01T00:00:00Z"), ConsentStore(kv, ResearchAnalyticsGate(kv, RecordingBackend())).studyWithdrawals)
    }

    // @verifies SWR-MG-04
    @Test
    fun aStudyTheUserLeftIsRememberedAndNeverRepresented() {
        val s = clocked("2026-04-01T00:00:00Z")
        val exported = s.exportWithdrawals()
        assertNull(exported["blanketConsentWithdrawnAt"], "nothing withdrawn exports no time, so never-asked stays null")
        s.applyWithdrawals(buildJsonObject { put("studyWithdrawals", buildJsonObject { put("study-x", "2026-04-01T00:00:00Z") }) })
        assertEquals("2026-04-01T00:00:00Z", s.exportWithdrawals()["studyWithdrawals"]!!.jsonObject["study-x"]!!.jsonPrimitive.content)
    }
}
