package life.neurone.core.consent

import life.neurone.core.analytics.AnalyticsBackend
import life.neurone.core.analytics.ResearchAnalyticsGate
import life.neurone.core.common.PresenceInstall
import life.neurone.core.common.PresenceState
import life.neurone.core.models.ClinicianAccessScope
import life.neurone.core.models.ClinicianConsentGrant
import life.neurone.core.models.ClinicianUseCaseTier
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
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * The consent store under the presence gate (`REQ-PRES-01` (d), `OI-PRIV-19`). With the gate on every launch starts locked, so a store
 * built then reads "absent" for every record. It must never take that for "never asked", and what the person does while locked is an
 * operation replayed on the sealed state at the unlock, never a whole value written in its place.
 */
class ConsentStorePresenceTests {

    private class Counting : AnalyticsBackend {
        var resets = 0
        override fun configure() {}
        override fun reset() { resets++ }
        override fun track(event: String, properties: Map<String, String>) {}
    }

    private val sealed = setOf(
        ConsentStore.GRANTS_KEY, ConsentStore.RESEARCH_KEY, ConsentStore.EXPANSIONS_KEY, ConsentStore.INVITATIONS_KEY,
        ConsentStore.STUDY_WITHDRAWALS_KEY, ConsentStore.QUARANTINE_KEY, PersistedKeys.PARTICIPATION_KEY,
    )

    private val verifier = StudyDescriptorVerifier { StudyDescriptorVerification.Verified("sha256:abc") }

    private fun PresenceInstall.Run.consent(backend: Counting = Counting()) =
        ConsentStore(kv, ResearchAnalyticsGate(kv, backend), descriptorVerifier = verifier, lock = lock)

    private fun descriptor() = StudyDescriptor(
        studyId = "study-1",
        studyTitle = "Test Study",
        researchCategories = listOf(ResearchCategory.SLEEP),
        requestedElements = setOf(UHDRElement.SESSION_TIMESTAMPS, UHDRElement.NEUROFEEDBACK_SCORES),
        kAnonymity = 10,
        dateRoundingDays = 7,
        issuedOnDay = LocalDate.now().toString(),
        expiresOnDay = LocalDate.now().plusDays(30).toString(),
        signature = "sig",
    )

    /** An install whose sealed consent holds a sleep-category consent, an accepted study and a grant, left as an earlier run would. */
    private fun installWithConsent(): PresenceInstall = PresenceInstall { sealed }.also { install ->
        val run = install.Run()
        run.unlock()
        val store = run.consent()
        store.updateResearchConsent(
            ResearchConsentState(
                contactConsentGranted = true, contactMethod = "person@example.org",
                categoryConsents = ResearchCategory.entries.associateWith { it == ResearchCategory.SLEEP },
            ),
        )
        store.ingestStudyDescriptor(descriptor())
        store.acceptInvitation("study-1")
        check(store.studyParticipations.single().isActive)
        run.lock.lock()
    }

    // The defect: the store loaded once at construction, so one built locked held defaults, and the next save replaced every sealed record.
    // @verifies REQ-PRES-01#d
    @Test fun a_store_built_locked_holds_nothing_and_reads_the_sealed_consent_at_unlock() {
        val install = installWithConsent()
        val run = install.Run() // a cold start: locked
        val store = run.consent()

        assertFalse(store.isReadable)
        assertTrue(store.studyParticipations.isEmpty())
        assertFalse(store.researchConsent.categoryConsents.getValue(ResearchCategory.SLEEP))

        run.unlock()

        assertTrue(store.isReadable)
        assertTrue(store.researchConsent.categoryConsents.getValue(ResearchCategory.SLEEP))
        assertTrue(store.studyParticipations.single().isActive)
    }

    // A withdrawal made locked is held as an operation, runs on the sealed state, and leaves everything else the person had.
    // @verifies REQ-PRES-01#d
    @Test fun a_withdrawal_made_while_locked_is_applied_to_the_sealed_state_and_nothing_else_is_lost() {
        val install = installWithConsent()
        val run = install.Run()
        val store = run.consent()

        store.withdrawFromStudy("study-1")
        store.setCategoryConsent(ResearchCategory.SLEEP, false)
        assertTrue(store.studyWithdrawals.isEmpty(), "nothing is shown or written while locked")

        run.unlock()

        assertNotNull(store.studyWithdrawals["study-1"], "the withdrawal survived")
        assertFalse(store.studyParticipations.single().isActive)
        assertFalse(store.researchConsent.categoryConsents.getValue(ResearchCategory.SLEEP))
        assertTrue(store.researchConsent.contactConsentGranted, "what the person did not touch is as it was sealed")

        // And it is what was written: the next run reads it.
        run.lock.lock()
        val next = install.Run()
        val again = next.consent()
        next.unlock()
        assertNotNull(again.studyWithdrawals["study-1"])
        assertTrue(again.researchConsent.contactConsentGranted)
    }

    // A person saying stop while locked is not made to wait for the unlock for what a removal can do.
    // @verifies REQ-PRES-01#d
    @Test fun a_blanket_withdrawal_made_while_locked_stops_analytics_at_once_and_marks_the_withdrawal_at_unlock() {
        val install = PresenceInstall { sealed }
        install.Run().also { first ->
            first.unlock()
            first.consent().updateResearchConsent(
                ResearchConsentState(
                    contactConsentGranted = true, contactMethod = "person@example.org", blanketConsentGranted = true,
                ),
            )
            first.lock.lock()
        }
        val run = install.Run()
        val backend = Counting()
        val store = run.consent(backend)

        store.withdrawBlanketResearchConsent()

        assertEquals(1, backend.resets, "the analytics teardown does not wait for the unlock")

        run.unlock()

        assertFalse(store.researchConsent.blanketConsentGranted)
        assertNotNull(store.researchConsent.blanketConsentWithdrawnAt, "\"said stop\" is on record, not \"never asked\"")
    }

    // A change that would widen what is shared, or commit a whole state a locked screen could not show, is refused, not held.
    // @verifies REQ-PRES-01#d
    @Test fun a_grant_an_acceptance_a_descriptor_and_a_whole_state_are_refused_while_locked_and_the_sealed_state_is_untouched() {
        val install = installWithConsent()
        val run = install.Run()
        val store = run.consent()
        val today = LocalDate.now()
        val tier = ClinicianUseCaseTier.MONITOR
        val grant = ClinicianConsentGrant(
            id = "grant-1", clinicianName = "Dr. Test", clinicianOrganization = "Test Hospital", tier = tier,
            grantedAtDay = today.toString(), expiresAtDay = today.plusDays(30).toString(),
            accessScopes = listOf(ClinicianAccessScope(tier.uhdrElements, today.toString(), false)),
            useCaseIds = listOf("adherence_monitoring"),
        )

        assertFalse(store.grantClinicianAccess(grant))
        store.updateResearchConsent(ResearchConsentState(blanketConsentGranted = true))
        store.setCategoryConsent(ResearchCategory.DEPRESSION, true)
        store.acceptInvitation("study-1")
        val admission = store.ingestStudyDescriptor(descriptor())
        assertIs<StudyDescriptorAdmission.Refused>(admission)

        run.unlock()

        assertTrue(store.clinicianGrants.isEmpty(), "no grant was made")
        assertFalse(store.researchConsent.blanketConsentGranted, "the whole state a locked screen built was not applied")
        assertFalse(store.researchConsent.categoryConsents.getValue(ResearchCategory.DEPRESSION))
        assertTrue(store.researchConsent.categoryConsents.getValue(ResearchCategory.SLEEP), "the sealed consent stands")
        assertTrue(store.studyParticipations.single().isActive)
    }

    // The lock closing takes the consent out of memory, and the next unlock reads it again.
    // @verifies REQ-PRES-01#d
    @Test fun the_consent_leaves_memory_when_the_lock_closes_and_returns_at_the_next_unlock() {
        val install = installWithConsent()
        val run = install.Run()
        val store = run.consent()
        run.unlock()
        assertTrue(store.researchConsent.categoryConsents.getValue(ResearchCategory.SLEEP))

        run.lock.lock()

        assertEquals(PresenceState.LOCKED, run.lock.state.value)
        assertFalse(store.isReadable)
        assertFalse(store.researchConsent.categoryConsents.getValue(ResearchCategory.SLEEP))
        assertTrue(store.studyParticipations.isEmpty())

        run.unlock()

        assertTrue(store.researchConsent.categoryConsents.getValue(ResearchCategory.SLEEP))
        assertNull(store.quarantinedBlobs.firstOrNull())
    }
}
