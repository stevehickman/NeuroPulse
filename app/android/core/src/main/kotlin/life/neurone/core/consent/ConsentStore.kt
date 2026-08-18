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
 *  - User — UHDR research data flow consent. Managed here, four layers:
 *    L1 contact · L2 category · L3 blanket · L4 results + community.
 *    Layers are not screens — since CLAUDE.md Rev 37 the four layers are presented
 *    across two screens (S1 = L4 + L1, S2 = L2 + L3). The layer identities are what
 *    this store, the withdrawal surfaces and the document set key on.
 *    Revoking research consent at ANY scope immediately stops data flows for that
 *    scope. Revoking blanket (L3) also tears down research analytics, because blanket
 *    withdrawal signals the user does not want any data collection beyond basic device
 *    function. That coupling is enforced in BOTH the named withdrawal method and the UI
 *    commit path ([updateResearchConsent]), because S2 commits L2 and L3 together.
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

    /**
     * Commit a research-consent state produced by the consent UI.
     *
     * This is the single ingestion point for every UI-driven research-consent change, so the
     * blanket→analytics coupling is enforced here and not only in the explicitly-named
     * [withdrawBlanketResearchConsent]. Android already routed its dashboard toggle to that
     * method, but iOS did not, and merging L2 and L3 onto one commit-at-the-end screen
     * (CLAUDE.md §6.2) makes the commit path the ordinary way blanket consent is revoked on
     * both platforms. The guard belongs where the state enters the store.
     *
     * The guard keys on the **transition**, not the value. `blanketConsentGranted == false`
     * holds both for a user who never granted it and for one who just revoked it; only the
     * second is a withdrawal. Testing the value would tear down analytics on every
     * category-only edit — the 2026-06-16 regression inverted.
     */
    fun updateResearchConsent(state: ResearchConsentState) {
        val wasBlanketGranted = researchConsent.blanketConsentGranted
        researchConsent = state
        save()

        if (wasBlanketGranted && !state.blanketConsentGranted) {
            revokeResearchAnalytics()
        }
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
