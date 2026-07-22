package life.neurone.core.consent

import life.neurone.core.analytics.ResearchAnalyticsGate
import life.neurone.core.common.KeyValueStore
import life.neurone.core.models.ClinicianConsentGrant
import life.neurone.core.models.ClinicianUseCaseTier
import life.neurone.core.models.ResearchCategory
import life.neurone.core.models.ResearchConsentState
import life.neurone.core.models.StudyInvitation
import life.neurone.core.models.StudyParticipationRecord
import java.time.LocalDate
import java.util.UUID
import kotlinx.serialization.builtins.ListSerializer
import kotlinx.serialization.json.Json

/**
 * Persisted consent state — clinician grants, research consent, study
 * participation audit trail. Port of iOS ConsentStore.swift.
 *
 * TWO CONSENT SUBJECTS (CLAUDE.md §6.0 — never conflated):
 *  - Warranty owner — SHDR fleet telemetry consent, held by WarrantyAnalyticsGate
 *    / the SHDR uploader. May be a clinic, not the end user. Not managed here.
 *  - User — UHDR research data flow consent. Managed here, scoped
 *    L1 contact → L2 category → L3 blanket. Revoking research consent at ANY
 *    scope immediately stops data flows for that scope. Revoking blanket (L3)
 *    also tears down research analytics, because blanket withdrawal signals
 *    the user does not want any data collection beyond basic device function.
 */
class ConsentStore(
    private val store: KeyValueStore,
    private val researchAnalyticsGate: ResearchAnalyticsGate,
    private val json: Json = Json { ignoreUnknownKeys = true },
) {
    companion object {
        const val GRANTS_KEY = "np.consent.clinician-grants"
        const val RESEARCH_KEY = "np.consent.research"
        const val PARTICIPATION_KEY = "np.consent.study-participations"
    }

    var clinicianGrants: List<ClinicianConsentGrant> = emptyList()
        private set
    var researchConsent: ResearchConsentState = ResearchConsentState()
        private set
    var studyParticipations: List<StudyParticipationRecord> = emptyList()
        private set
    var pendingInvitations: List<StudyInvitation> = emptyList()
        private set

    init {
        load()
    }

    // ── Clinician consent ────────────────────────────────────────────────

    fun grantClinicianAccess(grant: ClinicianConsentGrant) {
        clinicianGrants = clinicianGrants.filterNot { it.id == grant.id } + grant
        save()
    }

    fun revokeClinicianAccess(grantId: String) {
        clinicianGrants = clinicianGrants.filterNot { it.id == grantId }
        save()
    }

    fun expandClinicianAccess(grantId: String, newTier: ClinicianUseCaseTier) {
        clinicianGrants = clinicianGrants.map {
            if (it.id == grantId) it.copy(tier = newTier) else it
        }
        save()
    }

    // ── Research consent ─────────────────────────────────────────────────

    fun updateResearchConsent(state: ResearchConsentState) {
        researchConsent = state
        save()
    }

    /**
     * Withdraw blanket research consent (L3). Immediately prevents future study
     * descriptor processing; already-published extracts are unchanged
     * (irreversibility notice given at L3 consent time).
     *
     * Blanket withdrawal also revokes research analytics — the user is
     * signaling they do not want any data collection beyond basic device
     * function. Partial withdrawals (specific study or category) do NOT
     * revoke research analytics.
     */
    fun withdrawBlanketResearchConsent() {
        researchConsent = researchConsent.copy(blanketConsentGranted = false)
        save()
        revokeResearchAnalytics()
    }

    /**
     * Revoke research analytics — clears the research analytics gate key and
     * tears down the SDK so it cannot collect passively after withdrawal.
     * Does NOT affect WarrantyAnalyticsGate or SHDR fleet uploads.
     */
    fun revokeResearchAnalytics() {
        store.remove(ResearchAnalyticsGate.RESEARCH_ANALYTICS_KEY)
        researchAnalyticsGate.reset()
    }

    fun setCategoryConsent(category: ResearchCategory, granted: Boolean) {
        researchConsent = researchConsent.copy(
            categoryConsents = researchConsent.categoryConsents + (category to granted),
        )
        save()
    }

    // ── Study invitations ────────────────────────────────────────────────

    fun addInvitation(invitation: StudyInvitation) {
        pendingInvitations =
            pendingInvitations.filterNot { it.studyId == invitation.studyId } + invitation
    }

    fun acceptInvitation(studyId: String) {
        pendingInvitations = pendingInvitations.map {
            if (it.studyId == studyId) it.copy(decision = StudyInvitation.StudyDecision.Accepted) else it
        }
        recordParticipation(studyId = studyId, descriptorHash = "pending")
    }

    fun declineInvitation(studyId: String) {
        pendingInvitations = pendingInvitations.map {
            if (it.studyId == studyId) it.copy(decision = StudyInvitation.StudyDecision.Declined) else it
        }
    }

    // ── Participation audit (SHDR-class) ─────────────────────────────────

    private fun recordParticipation(studyId: String, descriptorHash: String) {
        studyParticipations = studyParticipations + StudyParticipationRecord(
            id = UUID.randomUUID().toString(),
            studyId = studyId,
            descriptorHash = descriptorHash,
            transmittedAtDay = LocalDate.now().toString(),
            extractBytes = 0,
        )
        save()
    }

    /** Per-study withdrawal — stops that study only; research analytics unaffected. */
    fun withdrawFromStudy(studyId: String) {
        studyParticipations = studyParticipations.map {
            if (it.studyId == studyId && it.isActive) {
                it.copy(withdrawnAtDay = LocalDate.now().toString())
            } else {
                it
            }
        }
        save()
    }

    // ── Persistence ──────────────────────────────────────────────────────

    private fun load() {
        store.getString(GRANTS_KEY)?.let { blob ->
            runCatching {
                clinicianGrants =
                    json.decodeFromString(ListSerializer(ClinicianConsentGrant.serializer()), blob)
            }
        }
        store.getString(RESEARCH_KEY)?.let { blob ->
            runCatching {
                researchConsent = json.decodeFromString(ResearchConsentState.serializer(), blob)
            }
        }
        store.getString(PARTICIPATION_KEY)?.let { blob ->
            runCatching {
                studyParticipations = json.decodeFromString(
                    ListSerializer(StudyParticipationRecord.serializer()), blob,
                )
            }
        }
    }

    private fun save() {
        store.putString(
            GRANTS_KEY,
            json.encodeToString(ListSerializer(ClinicianConsentGrant.serializer()), clinicianGrants),
        )
        store.putString(
            RESEARCH_KEY,
            json.encodeToString(ResearchConsentState.serializer(), researchConsent),
        )
        store.putString(
            PARTICIPATION_KEY,
            json.encodeToString(
                ListSerializer(StudyParticipationRecord.serializer()), studyParticipations,
            ),
        )
    }
}
