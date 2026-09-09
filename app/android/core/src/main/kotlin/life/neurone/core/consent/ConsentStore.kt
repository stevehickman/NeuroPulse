package life.neurone.core.consent

import life.neurone.core.analytics.ResearchAnalyticsGate
import life.neurone.core.common.KeyValueStore
import life.neurone.core.models.ClinicianAccessExpansionRequest
import life.neurone.core.models.ClinicianAccessScope
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
        const val EXPANSIONS_KEY = "np.consent.clinician-expansions"
    }

    var clinicianGrants: List<ClinicianConsentGrant> = emptyList()
        private set
    var researchConsent: ResearchConsentState = ResearchConsentState()
        private set
    var studyParticipations: List<StudyParticipationRecord> = emptyList()
        private set
    var pendingInvitations: List<StudyInvitation> = emptyList()
        private set

    /**
     * Clinician access-expansion requests (§6.1), decided and undecided alike — a denial is part
     * of the record. The UI shows `expansionRequests.filter { it.isPending }`.
     */
    var expansionRequests: List<ClinicianAccessExpansionRequest> = emptyList()
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
        // An outstanding request to widen a grant that no longer exists is a notification the user
        // can only answer wrongly, so it goes with the grant. Decided requests stay: they are the
        // record of what was asked and answered.
        expansionRequests = expansionRequests.filterNot { it.grantId == grantId && it.isPending }
        save()
    }

    // ── Clinician access expansion (§6.1) ────────────────────────────────
    //
    // §6.1's workflow is: differential consent document → persistent user notification → user
    // approves / denies / asks questions → retroactive access is a SEPARATE decision, "presented
    // as separate consent decisions even if made simultaneously".
    //
    // Before this existed, expandClinicianAccess was a public method that set `tier` and nothing
    // else, with no caller anywhere (OI-CONSENT-02). That is not merely an unimplemented workflow:
    // because approvedElements derives from `tier` and a tier is timeless, raising it hands the
    // clinician the new elements over every session ever recorded — silently taking the
    // retroactive decision §6.1 requires to be asked separately, and taking it in the affirmative.
    // The mutation is now private, and the only way to reach it is a request the user decided.

    /**
     * Ingest a clinician's request to widen a grant. No UI caller: requests arrive from the
     * clinician-portal sync layer, which does not exist yet (`OI-CONSENT-05`) — the same missing
     * layer that leaves [addInvitation] without one.
     *
     * A new request from the same grant supersedes that grant's outstanding one; decided requests
     * are left alone, because they are history rather than an inbox.
     */
    fun addExpansionRequest(request: ClinicianAccessExpansionRequest) {
        expansionRequests =
            expansionRequests.filterNot { it.grantId == request.grantId && it.isPending } + request
        save()
    }

    /**
     * Record the user's approval and apply it.
     *
     * [includePriorData] is the second of the two §6.1 decisions and arrives from its own control
     * on its own step; it is stored beside the approval rather than folded into it, so the record
     * can still say the user approved the expansion *and* refused it over history.
     */
    fun approveExpansion(requestId: String, includePriorData: Boolean) {
        val request = expansionRequests.firstOrNull { it.id == requestId && it.isPending } ?: return

        // Fail closed on a stale request: the grant may have been revoked, or already widened past
        // this tier, since the request was raised. Approving what the differential document no
        // longer describes would apply a change the user was not shown.
        val grant = clinicianGrants.firstOrNull { it.id == request.grantId } ?: return
        if (ConsentEngine.accessDifferential(grant, request.toTier) == null) return

        val today = LocalDate.now().toString()
        expansionRequests = expansionRequests.map {
            if (it.id == requestId) {
                it.copy(
                    decision = ClinicianAccessExpansionRequest.ExpansionDecision.Approved(
                        decidedOnDay = today,
                        includesPriorData = includePriorData,
                    ),
                )
            } else {
                it
            }
        }
        expandClinicianAccess(
            grantId = request.grantId,
            newTier = request.toTier,
            includePriorData = includePriorData,
            decidedOnDay = today,
        )
    }

    /**
     * Record a denial. Nothing about the grant changes — denying is the reversible direction, and
     * the clinician may raise a fresh request.
     */
    fun denyExpansion(requestId: String) {
        expansionRequests = expansionRequests.map {
            if (it.id == requestId && it.isPending) {
                it.copy(
                    decision = ClinicianAccessExpansionRequest.ExpansionDecision.Denied(
                        decidedOnDay = LocalDate.now().toString(),
                    ),
                )
            } else {
                it
            }
        }
        save()
    }

    /**
     * Record that the user asked a question rather than deciding (§6.1's third response).
     *
     * The request stays pending: asking is not answering, and the notification must not clear as
     * though the user had decided. Delivering the question to the clinician needs the same missing
     * outbound channel as [addExpansionRequest] (`OI-CONSENT-05`); until it exists the UI says so
     * plainly rather than implying a message was sent.
     */
    fun askQuestionAboutExpansion(requestId: String) {
        expansionRequests = expansionRequests.map {
            if (it.id == requestId && it.isPending) {
                it.copy(
                    decision = ClinicianAccessExpansionRequest.ExpansionDecision.QuestionSent(
                        sentOnDay = LocalDate.now().toString(),
                    ),
                )
            } else {
                it
            }
        }
        save()
    }

    /**
     * Apply an approved expansion. Private, and deliberately so: §6.1 makes the consent document
     * and the two decisions preconditions of the mutation, and a method that can widen a grant
     * without them is a standing invitation to skip them.
     *
     * The new elements are appended as their own [ClinicianAccessScope] rather than folded into
     * the tier alone, so the retroactive answer survives in the record. The tier still moves — it
     * is what the subscription and the price are keyed to — but it is no longer the only thing
     * deciding what the clinician can see.
     */
    private fun expandClinicianAccess(
        grantId: String,
        newTier: ClinicianUseCaseTier,
        includePriorData: Boolean,
        decidedOnDay: String,
    ) {
        clinicianGrants = clinicianGrants.map { grant ->
            if (grant.id != grantId) {
                grant
            } else {
                grant.copy(
                    accessScopes = grant.effectiveScopes + ClinicianAccessScope(
                        elements = newTier.uhdrElements - grant.approvedElements,
                        effectiveFromDay = decidedOnDay,
                        includesPriorData = includePriorData,
                    ),
                    tier = newTier,
                )
            }
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
        // Expansion requests persist because §6.1 calls for a *persistent* notification: one the
        // user can dismiss by backgrounding the app is not one.
        store.getString(EXPANSIONS_KEY)?.let { blob ->
            runCatching {
                expansionRequests = json.decodeFromString(
                    ListSerializer(ClinicianAccessExpansionRequest.serializer()), blob,
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
        store.putString(
            EXPANSIONS_KEY,
            json.encodeToString(
                ListSerializer(ClinicianAccessExpansionRequest.serializer()), expansionRequests,
            ),
        )
    }
}
