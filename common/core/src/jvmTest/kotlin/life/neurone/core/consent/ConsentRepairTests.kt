package life.neurone.core.consent

import kotlinx.serialization.builtins.ListSerializer
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.analytics.AnalyticsBackend
import life.neurone.core.analytics.ResearchAnalyticsGate
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.models.ClinicianAccessExpansionRequest
import life.neurone.core.models.ClinicianConsentGrant
import life.neurone.core.models.ClinicianUseCaseTier
import life.neurone.core.models.ContactChannel
import life.neurone.core.models.ResearchContact
import life.neurone.core.models.WithdrawalMark
import life.neurone.core.models.ResearchCategory
import life.neurone.core.models.ResearchConsentState
import life.neurone.core.models.StudyDescriptor
import life.neurone.core.models.StudyDescriptorAdmission
import life.neurone.core.models.StudyDescriptorVerification
import life.neurone.core.models.StudyDescriptorVerifier
import life.neurone.core.models.UHDRElement
import life.neurone.core.protocol.PersistedKeys
import java.time.LocalDate
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertIs
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * `OI-PRIV-06`: consent state that kept or lost things it should not. Each test pins one repair:
 * a withdrawal that reaches the participations it stops (`REQ-CONSENT-19`), clinician access that
 * ends (`REQ-CONSENT-18`), a stored record that cannot be read (`REQ-CONSENT-20`), and a
 * participation record that says only what happened (`REQ-CONSENT-21`).
 */
class ConsentRepairTests {

    private val json = Json { ignoreUnknownKeys = true }
    private val today = LocalDate.now().toString()

    private object QuietBackend : AnalyticsBackend {
        override fun configure() {}
        override fun reset() {}
        override fun track(event: String, properties: Map<String, String>) {}
    }

    private fun open(kv: InMemoryKeyValueStore) = ConsentStore(
        kv,
        ResearchAnalyticsGate(kv, QuietBackend),
        descriptorVerifier = StudyDescriptorVerifier { StudyDescriptorVerification.Verified("sha256:abc") },
    )

    private fun storeWith(consent: ResearchConsentState): Pair<ConsentStore, InMemoryKeyValueStore> {
        val kv = InMemoryKeyValueStore()
        val store = open(kv)
        store.updateResearchConsent(consent)
        return store to kv
    }

    private fun categories(vararg granted: ResearchCategory) = ResearchConsentState(
        contactConsentGranted = true,
        contactMethod = "person@example.org",
        categoryConsents = ResearchCategory.entries.associateWith { it in granted },
    )

    private val blanket = ResearchConsentState(
        contactConsentGranted = true,
        contactMethod = "person@example.org",
        blanketConsentGranted = true,
    )

    private fun study(id: String, vararg inCategories: ResearchCategory) = StudyDescriptor(
        studyId = id,
        studyTitle = "Study $id",
        researchCategories = inCategories.toList(),
        requestedElements = setOf(UHDRElement.SESSION_DURATION),
        kAnonymity = 10,
        dateRoundingDays = 7,
        issuedOnDay = today,
        expiresOnDay = LocalDate.now().plusDays(30).toString(),
        signature = "sig",
    )

    private fun ConsentStore.join(descriptor: StudyDescriptor) {
        assertIs<StudyDescriptorAdmission.Admitted>(ingestStudyDescriptor(descriptor))
        acceptInvitation(descriptor.studyId)
    }

    private fun ConsentStore.active(): Set<String> =
        studyParticipations.filter { it.isActive }.map { it.studyId }.toSet()

    // ── REQ-CONSENT-19: a withdrawal stops the participations it no longer covers ──

    // @verifies REQ-CONSENT-19#a
    @Test
    fun blanketWithdrawalStopsEveryParticipation() {
        val (store, _) = storeWith(blanket)
        store.ingestStudyDescriptor(study("A", ResearchCategory.DEPRESSION))
        store.ingestStudyDescriptor(study("B", ResearchCategory.SLEEP))
        assertEquals(setOf("A", "B"), store.active(), "an L3 user is in a study from the moment it arrives")

        store.withdrawBlanketResearchConsent()

        assertTrue(store.active().isEmpty())
        assertTrue(store.studyParticipations.all { it.withdrawnAtDay == today })
    }

    // @verifies REQ-CONSENT-19#a
    @Test
    fun blanketWithdrawalThroughTheCommitPathStopsEveryParticipation() {
        val (store, _) = storeWith(blanket.copy(categoryConsents = ResearchCategory.entries.associateWith { true }))
        store.ingestStudyDescriptor(study("A", ResearchCategory.DEPRESSION))

        // The nine ticks are still on. They do not keep the study going: withdrawal outranks them (§6.0).
        store.updateResearchConsent(store.researchConsent.copy(blanketConsentGranted = false))

        assertTrue(store.active().isEmpty())
    }

    // @verifies REQ-CONSENT-19#b
    @Test
    fun categoryWithdrawalStopsOnlyTheStudiesLeftWithNoConsentedCategory() {
        val (store, _) = storeWith(categories(ResearchCategory.DEPRESSION, ResearchCategory.SLEEP))
        store.join(study("A", ResearchCategory.DEPRESSION))
        store.join(study("B", ResearchCategory.SLEEP))
        store.join(study("C", ResearchCategory.DEPRESSION, ResearchCategory.SLEEP))

        store.updateResearchConsent(categories(ResearchCategory.SLEEP))
        assertEquals(setOf("B", "C"), store.active(), "C still has a consented category")

        store.setCategoryConsent(ResearchCategory.SLEEP, false)
        assertTrue(store.active().isEmpty())
    }

    // @verifies REQ-CONSENT-19#b
    @Test
    fun aCategoryWithdrawalUnderBlanketConsentStopsNothing() {
        val (store, _) = storeWith(blanket.copy(categoryConsents = ResearchCategory.entries.associateWith { true }))
        store.ingestStudyDescriptor(study("A", ResearchCategory.DEPRESSION))

        store.setCategoryConsent(ResearchCategory.DEPRESSION, false)

        assertEquals(setOf("A"), store.active(), "blanket consent is what covers it")
    }

    // @verifies REQ-CONSENT-19#c
    @Test
    fun aStoppedParticipationIsNotRecordedAsAStudyThePersonNamed() {
        val (store, _) = storeWith(blanket)
        store.ingestStudyDescriptor(study("A", ResearchCategory.DEPRESSION))

        store.withdrawBlanketResearchConsent()

        assertTrue(store.studyWithdrawals.isEmpty(), "the person said stop to everything, not to study A by name")
        assertFalse(store.pendingInvitations.single().isOpen, "the notice that they are in the study is closed with it")
    }

    // @verifies REQ-CONSENT-19#c
    @Test
    fun aRestoredBlanketWithdrawalStopsParticipationsHereToo() {
        val (store, _) = storeWith(blanket)
        store.ingestStudyDescriptor(study("A", ResearchCategory.DEPRESSION))

        store.applyWithdrawals(
            buildJsonObject { put("blanketConsentWithdrawnAt", "2099-01-01T00:00:00Z") },
        )

        assertTrue(store.active().isEmpty())
    }

    // @verifies REQ-CONSENT-19#c
    @Test
    fun aParticipationLeftActiveByAnEarlierWithdrawalIsStoppedWhenTheStoreIsRead() {
        val (store, kv) = storeWith(blanket)
        store.ingestStudyDescriptor(study("A", ResearchCategory.DEPRESSION))
        // What the store wrote before the repair: consent withdrawn, participation untouched.
        kv.putString(
            ConsentStore.RESEARCH_KEY,
            json.encodeToString(
                ResearchConsentState.serializer(),
                ResearchConsentState(contactConsentGranted = true, blanketConsentWithdrawnOnDay = "2026-01-01"),
            ),
        )

        val reopened = open(kv)

        assertTrue(reopened.active().isEmpty())
        assertTrue(open(kv).active().isEmpty(), "and the repair is stored")
    }

    // @verifies REQ-CONSENT-19#d
    @Test
    fun aRequestCannotBeAcceptedAfterItsConsentWasWithdrawn() {
        val (store, _) = storeWith(categories(ResearchCategory.DEPRESSION))
        store.ingestStudyDescriptor(study("A", ResearchCategory.DEPRESSION))

        store.updateResearchConsent(categories())
        store.acceptInvitation("A")
        assertTrue(store.studyParticipations.isEmpty(), "accepting would start a flow the withdrawal stopped")
        assertTrue(store.pendingInvitations.single().isOpen, "and the request is not answered for the person")

        // Consenting again is a fresh decision, and the request can then be answered.
        store.updateResearchConsent(categories(ResearchCategory.DEPRESSION))
        store.acceptInvitation("A")
        assertEquals(setOf("A"), store.active())
    }

    // @verifies REQ-CONSENT-19#d
    @Test
    fun aRequestCannotBeAcceptedWhileBlanketConsentIsWithdrawn() {
        val (store, _) = storeWith(categories(ResearchCategory.DEPRESSION))
        store.ingestStudyDescriptor(study("A", ResearchCategory.DEPRESSION))
        store.updateResearchConsent(store.researchConsent.copy(blanketConsentGranted = true))
        store.withdrawBlanketResearchConsent()

        store.acceptInvitation("A")

        assertTrue(store.studyParticipations.isEmpty(), "the category is still ticked, and withdrawal outranks it")
    }

    // ── REQ-CONSENT-18: clinician access is time-limited ─────────────────

    private fun grant(expiresAtDay: String?, tier: ClinicianUseCaseTier = ClinicianUseCaseTier.MONITOR) =
        ClinicianConsentGrant(
            id = "grant-1",
            clinicianName = "Dr. Test",
            clinicianOrganization = "Test Hospital",
            tier = tier,
            grantedAtDay = "2026-01-01",
            expiresAtDay = expiresAtDay,
        )

    private fun storeHolding(stored: ClinicianConsentGrant): ConsentStore {
        val kv = InMemoryKeyValueStore()
        kv.putString(
            ConsentStore.GRANTS_KEY,
            json.encodeToString(ListSerializer(ClinicianConsentGrant.serializer()), listOf(stored)),
        )
        return open(kv)
    }

    // @verifies REQ-CONSENT-18#a
    @Test
    fun aGrantWithNoUsableExpiryIsRefused() {
        val store = open(InMemoryKeyValueStore())
        val yesterday = LocalDate.now().minusDays(1).toString()

        assertFalse(store.grantClinicianAccess(grant(expiresAtDay = null)), "no expiry is not time-limited")
        assertFalse(store.grantClinicianAccess(grant(expiresAtDay = "2099-13-40")), "not a day")
        assertFalse(store.grantClinicianAccess(grant(expiresAtDay = "2099-1-1")), "not the form that orders as text")
        assertFalse(store.grantClinicianAccess(grant(expiresAtDay = yesterday)), "already over")
        assertTrue(store.clinicianGrants.isEmpty())

        assertTrue(store.grantClinicianAccess(grant(expiresAtDay = today)), "the expiry day is the last day in force")
        assertEquals(1, store.clinicianGrants.size)
    }

    // @verifies REQ-CONSENT-18#b
    @Test
    fun aGrantIsInForceThroughItsExpiryDayAndNotAfter() {
        val g = grant(expiresAtDay = "2026-12-01")

        assertTrue(g.isInForce("2026-11-30"))
        assertTrue(g.isInForce("2026-12-01"))
        assertFalse(g.isInForce("2026-12-02"))
        assertFalse(g.copy(isActive = false).isInForce("2026-11-30"))
    }

    // @verifies REQ-CONSENT-18#c
    @Test
    fun aLapsedGrantShowsTheClinicianNothing() {
        val g = grant(expiresAtDay = "2026-12-01")

        assertEquals(g.approvedElements, g.elementsVisibleForDataRecordedOn("2026-06-01", today = "2026-12-01"))
        assertTrue(g.elementsVisibleForDataRecordedOn("2026-06-01", today = "2026-12-02").isEmpty())
        assertTrue(g.approvedElements.isNotEmpty(), "the record of what was granted stays readable")
    }

    // @verifies REQ-CONSENT-18#c
    @Test
    fun aLapsedGrantCannotBeWidened() {
        val lapsed = grant(expiresAtDay = "2020-01-01")
        assertNull(ConsentEngine.accessDifferential(lapsed, ClinicianUseCaseTier.ASSESS, today))

        val store = storeHolding(lapsed)
        val request = ClinicianAccessExpansionRequest(
            id = "req-1",
            grantId = lapsed.id,
            fromTier = ClinicianUseCaseTier.MONITOR,
            toTier = ClinicianUseCaseTier.ASSESS,
            requestedAtDay = today,
        )
        assertEquals(
            ClinicianRequestAdmission.RefusedNotAnExpansion,
            ClinicianPortalSync(store) { ClinicianRequestVerification.Verified }.ingest(request),
        )

        // A request that was already waiting when the grant lapsed cannot be approved either.
        store.addExpansionRequest(request)
        store.approveExpansion(request.id, includePriorData = true)
        assertEquals(ClinicianUseCaseTier.MONITOR, store.clinicianGrants.single().tier)
        assertTrue(store.expansionRequests.single().isPending)
    }

    // @verifies REQ-CONSENT-18#d
    @Test
    fun aStoredGrantWithNoExpiryIsNotInForce() {
        val store = storeHolding(grant(expiresAtDay = null))
        val stored = store.clinicianGrants.single()

        assertFalse(stored.isInForce(today))
        assertTrue(stored.elementsVisibleForDataRecordedOn(today, today).isEmpty())
    }

    // ── REQ-CONSENT-20: a stored record that cannot be read ──────────────

    private val unreadable = """{"blanketConsentGranted":"perhaps"}"""

    /** A store whose research record was good, held a withdrawal, and then stopped decoding. */
    private fun storeWithUnreadableResearchRecord(): InMemoryKeyValueStore {
        val (store, kv) = storeWith(blanket)
        store.withdrawBlanketResearchConsent()
        kv.putString(ConsentStore.RESEARCH_KEY, unreadable)
        return kv
    }

    // @verifies REQ-CONSENT-20#a
    @Test
    fun anUnreadableRecordIsKeptAsFoundAndSurvivesLaterSaves() {
        val kv = storeWithUnreadableResearchRecord()

        val store = open(kv)
        assertEquals(unreadable, store.quarantinedBlobs.single().blob)
        assertEquals(ConsentStore.RESEARCH_KEY, store.quarantinedBlobs.single().key)

        // Any save now writes defaults over the record itself. The text that was there is still held.
        assertTrue(store.grantClinicianAccess(grant(expiresAtDay = today)))
        assertFalse(kv.getString(ConsentStore.RESEARCH_KEY) == unreadable)
        assertEquals(unreadable, open(kv).quarantinedBlobs.single().blob)
    }

    // @verifies REQ-CONSENT-20#b
    @Test
    fun nothingIsAdmittedWhileTheRecordIsUnreadable() {
        val kv = storeWithUnreadableResearchRecord()
        val store = open(kv)

        assertTrue(store.consentRecordUnreadable)
        assertEquals(
            StudyDescriptorAdmission.Refused(StudyDescriptorAdmission.Reason.CONSENT_RECORD_UNREADABLE),
            store.ingestStudyDescriptor(study("A", ResearchCategory.DEPRESSION)),
        )
        // It still holds after a save has replaced the unreadable text with defaults, and after a restart.
        store.grantClinicianAccess(grant(expiresAtDay = today))
        assertTrue(open(kv).consentRecordUnreadable)
    }

    // @verifies REQ-CONSENT-20#b
    @Test
    fun aRequestCannotBeAcceptedAndNoParticipationIsStoppedWhileTheRecordIsUnreadable() {
        val (first, kv) = storeWith(categories(ResearchCategory.DEPRESSION, ResearchCategory.SLEEP))
        first.join(study("A", ResearchCategory.DEPRESSION))
        first.ingestStudyDescriptor(study("B", ResearchCategory.SLEEP))
        kv.putString(ConsentStore.RESEARCH_KEY, unreadable)

        val store = open(kv)
        store.acceptInvitation("B")

        assertEquals(setOf("A"), store.active(), "defaults are not the person's decision, so nothing is stopped on them")
        assertEquals(1, store.studyParticipations.size, "and nothing is started")
    }

    // @verifies REQ-CONSENT-20#c
    @Test
    fun aDecisionByThePersonEndsTheQuarantineAndKeepsTheText() {
        val kv = storeWithUnreadableResearchRecord()
        val store = open(kv)

        store.updateResearchConsent(categories(ResearchCategory.DEPRESSION))

        assertFalse(store.consentRecordUnreadable)
        assertIs<StudyDescriptorAdmission.Admitted>(store.ingestStudyDescriptor(study("A", ResearchCategory.DEPRESSION)))
        val reopened = open(kv)
        assertFalse(reopened.consentRecordUnreadable)
        assertEquals(unreadable, reopened.quarantinedBlobs.single().blob)
        assertEquals(today, reopened.quarantinedBlobs.single().redecidedOnDay)
    }

    // @verifies REQ-CONSENT-20#c
    @Test
    fun sayingStopWhileTheRecordIsUnreadableIsRecordedAsAWithdrawal() {
        val kv = storeWithUnreadableResearchRecord()
        val store = open(kv)
        assertFalse(store.researchConsent.blanketConsentWithdrawn, "the defaults read as never asked")

        store.withdrawBlanketResearchConsent()

        assertTrue(store.researchConsent.blanketConsentWithdrawn)
        assertEquals(today, store.researchConsent.blanketConsentWithdrawnOnDay)
        assertTrue(store.researchConsent.blanketConsentWithdrawnAt != null)
        assertFalse(store.consentRecordUnreadable)
    }

    // @verifies REQ-CONSENT-20#d
    @Test
    fun anUnreadableQuarantineListIsItselfKeptAndBlocksAdmission() {
        val kv = InMemoryKeyValueStore()
        kv.putString(ConsentStore.QUARANTINE_KEY, "not a list")

        val store = open(kv)

        assertTrue(store.consentRecordUnreadable, "what it held may have been an unanswered quarantine")
        assertEquals("not a list", store.quarantinedBlobs.single().blob)
        assertEquals("not a list", open(kv).quarantinedBlobs.single().blob)
    }

    // @verifies REQ-CONSENT-20#d
    @Test
    fun anUnreadableClinicianRecordIsKeptAndDoesNotBlockResearch() {
        val kv = InMemoryKeyValueStore()
        kv.putString(ConsentStore.GRANTS_KEY, "[{")

        val store = open(kv)

        assertTrue(store.clinicianGrants.isEmpty(), "which gives a clinician nothing")
        assertEquals("[{", store.quarantinedBlobs.single().blob)
        assertFalse(store.consentRecordUnreadable)
    }

    // ── REQ-CONSENT-21: the participation record says only what happened ──

    // @verifies REQ-CONSENT-21#a
    @Test
    fun enrolmentRecordsTheDayJoinedAndNoTransmission() {
        val (store, _) = storeWith(categories(ResearchCategory.DEPRESSION))
        store.join(study("A", ResearchCategory.DEPRESSION))

        val record = store.studyParticipations.single()
        assertEquals(today, record.enrolledOnDay)
        assertNull(record.transmittedAtDay, "nothing has been sent")
        assertEquals(0, record.extractBytes)
    }

    // @verifies REQ-CONSENT-21#b
    @Test
    fun aRecordStoredBeforeTheRepairIsReadAsEnrolledWithNothingSent() {
        val kv = InMemoryKeyValueStore()
        kv.putString(ConsentStore.RESEARCH_KEY, json.encodeToString(ResearchConsentState.serializer(), blanket))
        kv.putString(
            PersistedKeys.PARTICIPATION_KEY,
            """[{"id":"1","studyId":"A","descriptorHash":"sha256:abc","transmittedAtDay":"2026-01-05","extractBytes":0}]""",
        )

        val record = open(kv).studyParticipations.single()

        assertEquals("2026-01-05", record.enrolledOnDay)
        assertNull(record.transmittedAtDay)
        assertTrue(record.isActive)
        assertTrue(kv.getString(PersistedKeys.PARTICIPATION_KEY)!!.contains("enrolledOnDay"), "and it is stored that way")
    }

    // ── REQ-CONSENT-09 h: what a study may ask for ───────────────────────

    // @verifies REQ-CONSENT-09#h
    @Test
    fun aStudyAskingForARecordedSignalIsRefusedWhateverKItDeclares() {
        val (store, _) = storeWith(blanket)
        // EEG, PPG and HRV alike: the three recorded signals are handled the same way.
        val signals = listOf(UHDRElement.EEG_WAVEFORMS, UHDRElement.PPG_OPTICAL_SIGNAL, UHDRElement.HRV_TIME_SERIES)
        for (signal in signals) {
            val asksForSignal = study("A-${signal.name}", ResearchCategory.DEPRESSION).copy(
                requestedElements = setOf(UHDRElement.SESSION_DURATION, signal),
                kAnonymity = 1000,
            )
            assertEquals(
                StudyDescriptorAdmission.Refused(StudyDescriptorAdmission.Reason.REQUESTS_ELEMENT_NO_EXTRACT_MAY_CARRY),
                store.ingestStudyDescriptor(asksForSignal),
                signal.name,
            )
        }
        assertTrue(store.pendingInvitations.isEmpty())
        assertIs<StudyDescriptorAdmission.Admitted>(store.ingestStudyDescriptor(study("B")), "a study that does not ask for it is admitted")
    }

    // ── REQ-CONSENT-09 i: a descriptor stands only until the day its issuer signed ──

    // @verifies REQ-CONSENT-09#i
    @Test
    fun aDescriptorIsRefusedAfterItsExpiryDayOrWithAnExpiryThatCannotBeHeldTo() {
        val (store, _) = storeWith(blanket)
        val yesterday = LocalDate.now().minusDays(1).toString()
        val expired = StudyDescriptorAdmission.Refused(StudyDescriptorAdmission.Reason.DESCRIPTOR_EXPIRED)

        assertEquals(expired, store.ingestStudyDescriptor(study("A").copy(issuedOnDay = "2026-01-01", expiresOnDay = yesterday)))
        assertEquals(expired, store.ingestStudyDescriptor(study("B").copy(expiresOnDay = "soon")))
        assertEquals(expired, store.ingestStudyDescriptor(study("C").copy(issuedOnDay = today, expiresOnDay = yesterday)))
        assertIs<StudyDescriptorAdmission.Admitted>(
            store.ingestStudyDescriptor(study("D").copy(expiresOnDay = today)),
            "the expiry day is the last day",
        )
    }

    // @verifies REQ-CONSENT-09#i
    @Test
    fun aRequestCannotBeAcceptedAfterItsDescriptorExpired() {
        val (store, kv) = storeWith(categories(ResearchCategory.DEPRESSION))
        store.ingestStudyDescriptor(study("A", ResearchCategory.DEPRESSION))
        // The invitation as it will be once its expiry day has passed.
        val key = ConsentStore.INVITATIONS_KEY
        kv.putString(key, kv.getString(key)!!.replace(Regex("\"expiresOnDay\":\"[0-9-]+\""), "\"expiresOnDay\":\"2020-01-01\""))

        val reopened = open(kv)
        reopened.acceptInvitation("A")

        assertTrue(reopened.studyParticipations.isEmpty())
        assertTrue(reopened.pendingInvitations.single().isOpen)
    }

    // ── REQ-CONSENT-23: the contact method is typed, needed for L1, and erased with it ──

    // @verifies REQ-CONSENT-23#a
    @Test
    fun aContactIsAnEmailAddressOrAPhoneNumberAndNothingElse() {
        assertEquals(ResearchContact(ContactChannel.EMAIL, "a.b@example.org"), ResearchContact.parse("  a.b@example.org "))
        assertEquals(ResearchContact(ContactChannel.PHONE, "+15551234567"), ResearchContact.parse("+1 (555) 123-4567"))
        for (notOne in listOf("", "call me after my EEG results", "a@b", "a@@example.org", "a b@example.org", "@example.org",
            "+1234567890123456", "555-CALL")) {
            assertNull(ResearchContact.parse(notOne), notOne)
        }
    }

    // @verifies REQ-CONSENT-23#b
    @Test
    fun contactConsentIsNotTakenWithoutAUsableMethod() {
        val store = open(InMemoryKeyValueStore())

        store.updateResearchConsent(ResearchConsentState(contactConsentGranted = true, contactMethod = "ask my sister"))
        assertFalse(store.researchConsent.contactConsentGranted)
        assertEquals("", store.researchConsent.contactMethod, "and what was typed is not kept")
        assertNull(store.researchConsent.contactConsentWithdrawnAt, "nothing was granted, so nothing was withdrawn")

        store.updateResearchConsent(ResearchConsentState(contactConsentGranted = true, contactMethod = "+44 20 7946 0000"))
        assertTrue(store.researchConsent.contactConsentGranted)
        assertEquals("+442079460000", store.researchConsent.contactMethod)
        assertEquals(ContactChannel.PHONE, store.researchConsent.contactChannel)

        // An unusable replacement does not displace the method held, and does not read as a withdrawal.
        store.updateResearchConsent(store.researchConsent.copy(contactMethod = "nope"))
        assertTrue(store.researchConsent.contactConsentGranted)
        assertEquals("+442079460000", store.researchConsent.contactMethod)
    }

    // @verifies REQ-CONSENT-23#c
    @Test
    fun withdrawingContactConsentErasesTheMethod() {
        val (store, kv) = storeWith(blanket)
        assertEquals("person@example.org", store.researchConsent.contactMethod)

        store.updateResearchConsent(store.researchConsent.copy(contactConsentGranted = false))

        assertEquals("", store.researchConsent.contactMethod)
        assertNull(store.researchConsent.contactChannel)
        assertFalse(kv.getString(ConsentStore.RESEARCH_KEY)!!.contains("person@example.org"))
    }

    // @verifies REQ-CONSENT-23#c
    @Test
    fun aRestoredContactWithdrawalErasesTheMethodHereToo() {
        val (store, _) = storeWith(blanket)

        store.applyWithdrawals(buildJsonObject { put("contactConsentWithdrawnAt", "2099-01-01#000001") })

        assertFalse(store.researchConsent.contactConsentGranted)
        assertEquals("", store.researchConsent.contactMethod)
    }

    // ── REQ-CONSENT-24: a withdrawal is marked with a day and a count ─────

    private fun onDays(vararg days: String): ConsentStore {
        val kv = InMemoryKeyValueStore()
        val it = days.iterator()
        return ConsentStore(kv, ResearchAnalyticsGate(kv, QuietBackend), today = { it.next() })
    }

    // @verifies REQ-CONSENT-24#a
    @Test
    fun aMarkHoldsADayAndACountAndNoTimeOfDay() {
        val store = onDays("2026-03-01")
        store.updateResearchConsent(blanket)

        store.updateResearchConsent(blanket.copy(blanketConsentGranted = false))

        assertEquals("2026-03-01#000001", store.researchConsent.blanketConsentWithdrawnAt)
        assertTrue(WithdrawalMark.isValid("2026-03-01#000001"))
        assertFalse(WithdrawalMark.isValid("2026-03-01T10:15:30Z"), "an instant is not a mark")
    }

    // @verifies REQ-CONSENT-24#b
    @Test
    fun twoWithdrawalsOfOneDecisionOnOneDayAreOrderedByTheirCount() {
        val store = onDays("2026-03-01", "2026-03-01", "2026-03-05")
        store.updateResearchConsent(blanket)
        store.updateResearchConsent(blanket.copy(blanketConsentGranted = false))
        val first = store.researchConsent.blanketConsentWithdrawnAt!!
        store.updateResearchConsent(blanket)
        store.updateResearchConsent(blanket.copy(blanketConsentGranted = false))
        val second = store.researchConsent.blanketConsentWithdrawnAt!!
        store.updateResearchConsent(blanket)
        store.updateResearchConsent(blanket.copy(blanketConsentGranted = false))
        val third = store.researchConsent.blanketConsentWithdrawnAt!!

        assertEquals("2026-03-01#000002", second)
        assertEquals("2026-03-05#000003", third)
        assertTrue(first < second && second < third, "the text orders them")
    }

    // @verifies REQ-CONSENT-24#c
    @Test
    fun aSecondWithdrawalOnTheOtherPhoneTheSameDayIsLaterAndIsApplied() {
        // This phone holds the first withdrawal and consent was granted again here. The other phone
        // withdrew again the same day: its count is higher, so the restore switches the grant off.
        val store = onDays("2026-03-01")
        store.updateResearchConsent(blanket)
        store.updateResearchConsent(blanket.copy(blanketConsentGranted = false))
        store.updateResearchConsent(blanket)

        store.applyWithdrawals(buildJsonObject { put("blanketConsentWithdrawnAt", "2026-03-01#000001") })
        assertTrue(store.researchConsent.blanketConsentGranted, "the same withdrawal again changes nothing")

        store.applyWithdrawals(buildJsonObject { put("blanketConsentWithdrawnAt", "2026-03-01#000002") })
        assertFalse(store.researchConsent.blanketConsentGranted)
        assertEquals("2026-03-01#000002", store.researchConsent.blanketConsentWithdrawnAt)
    }

    // ── REQ-CONSENT-20 e: marks are read out of a record that did not decode ──

    // @verifies REQ-CONSENT-20#e
    @Test
    fun withdrawalMarksAreRecoveredFromAnUnreadableRecordAndGrantsNeverAre() {
        val kv = InMemoryKeyValueStore()
        kv.putString(
            ConsentStore.RESEARCH_KEY,
            """{"blanketConsentGranted":"perhaps","contactConsentGranted":true,"contactMethod":"person@example.org",
               "blanketConsentWithdrawnAt":"2026-02-01#000002","resultsOptInWithdrawnAt":"2026-02-01T10:00:00Z",
               "categoryWithdrawnAt":{"SLEEP":"2026-01-15#000001","NOT_A_CATEGORY":"2026-01-15#000001"}}""",
        )
        kv.putString(ConsentStore.STUDY_WITHDRAWALS_KEY, """{"S-1":"2026-01-20#000001","S-2":17""" + "}")

        val store = open(kv)

        assertTrue(store.consentRecordUnreadable)
        assertTrue(store.researchConsent.blanketConsentWithdrawn, "said stop is held from the start")
        assertEquals("2026-02-01#000002", store.researchConsent.blanketConsentWithdrawnAt)
        assertEquals(mapOf(ResearchCategory.SLEEP to "2026-01-15#000001"), store.researchConsent.categoryWithdrawnAt)
        assertNull(store.researchConsent.resultsOptInWithdrawnAt, "a value that is not a mark is left behind")
        assertFalse(store.researchConsent.contactConsentGranted, "a grant is never read out of an unreadable record")
        assertEquals("", store.researchConsent.contactMethod)
        assertEquals("2026-02-01#000002", store.exportWithdrawals()["blanketConsentWithdrawnAt"]!!.jsonPrimitive.content)

        // After the person decides again the recovered marks are still the record.
        store.updateResearchConsent(categories(ResearchCategory.DEPRESSION))
        assertFalse(store.consentRecordUnreadable)
        assertEquals("2026-02-01#000002", open(kv).researchConsent.blanketConsentWithdrawnAt)
    }

    // @verifies REQ-CONSENT-20#e
    @Test
    fun studyWithdrawalsAreRecoveredFromAnUnreadableList() {
        val kv = InMemoryKeyValueStore()
        kv.putString(ConsentStore.STUDY_WITHDRAWALS_KEY, """{"S-1":"2026-01-20#000001","S-2":17}""")

        val store = open(kv)

        assertEquals(mapOf("S-1" to "2026-01-20#000001"), store.studyWithdrawals)
        assertTrue(store.consentRecordUnreadable)
    }
}
