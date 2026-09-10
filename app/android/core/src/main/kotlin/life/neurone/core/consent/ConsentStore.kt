package life.neurone.core.consent

import life.neurone.core.analytics.ResearchAnalyticsGate
import life.neurone.core.common.KeyValueStore
import life.neurone.core.models.ClinicianAccessExpansionRequest
import life.neurone.core.models.ClinicianAccessScope
import life.neurone.core.models.ClinicianConsentGrant
import life.neurone.core.models.ClinicianUseCaseTier
import life.neurone.core.models.RefusingStudyDescriptorVerifier
import life.neurone.core.models.ResearchCategory
import life.neurone.core.models.ResearchConsentState
import life.neurone.core.models.StudyDescriptor
import life.neurone.core.models.StudyDescriptorAdmission
import life.neurone.core.models.StudyDescriptorVerifier
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
    /**
     * Checks the signature on every study descriptor before anything derived from it reaches the
     * user (§5.3 step 1). The default refuses everything: see [RefusingStudyDescriptorVerifier].
     */
    private val descriptorVerifier: StudyDescriptorVerifier = RefusingStudyDescriptorVerifier,
) {
    companion object {
        const val GRANTS_KEY = "np.consent.clinician-grants"
        const val RESEARCH_KEY = "np.consent.research"
        const val PARTICIPATION_KEY = "np.consent.study-participations"
        const val EXPANSIONS_KEY = "np.consent.clinician-expansions"
        const val INVITATIONS_KEY = "np.consent.study-invitations"
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
     * clinician-portal sync layer, which does not exist yet (`OI-CONSENT-05`) — the sibling of
     * the study-service transport that leaves [ingestStudyDescriptor] without one.
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

    // ── Study invitations (§6.3 per-project workflow) ────────────────────
    //
    // Ingestion used to be `addInvitation(invitation: StudyInvitation)`: a finished invitation,
    // plain-language "what they cannot see" list and irreversibility notice included, taken on
    // trust from whatever called it, held in memory only, and called from nothing but a unit test
    // (`OI-CONSENT-03`). Like `expandClinicianAccess` before it, that was not merely an
    // unimplemented workflow — it took decisions silently and took them all in the affirmative:
    // that the descriptor's signature need not be checked, that §5.3's anonymisation floors need
    // not be met, that the user's own L1/L2/L3 state need not be consulted, and that the party
    // asking for access may write the sentence describing what it cannot see.
    //
    // What enters the device now is the signed descriptor. The invitation is derived from it.

    /**
     * Ingest a signed study descriptor (§5.3 step 1) and, if it is admissible, put the invitation
     * derived from it in front of the user.
     *
     * **No UI caller, and none is expected**: descriptors arrive from the NeurOne study service,
     * which does not exist yet (`OI-CONSENT-07`). With no verifier injected this refuses every
     * descriptor with [StudyDescriptorAdmission.Reason.VERIFIER_UNAVAILABLE], which is the honest
     * description of a device that has no signing key — not a silent no-op.
     *
     * Returns the admission so the caller can tell a refusal from a delivery. Nothing is persisted
     * on a refusal: a descriptor the device would not show the user is not a record of anything
     * the user did.
     */
    fun ingestStudyDescriptor(descriptor: StudyDescriptor): StudyDescriptorAdmission {
        val admission = ConsentEngine.admit(
            descriptor = descriptor,
            verification = descriptorVerifier.verify(descriptor),
            consent = researchConsent,
            studyAlreadyDecided = hasDecided(descriptor.studyId),
        )
        if (admission !is StudyDescriptorAdmission.Admitted) return admission

        val today = LocalDate.now().toString()
        pendingInvitations = pendingInvitations.filterNot { it.studyId == descriptor.studyId } +
            ConsentEngine.invitation(
                descriptor = descriptor,
                posture = admission.posture,
                descriptorHash = admission.descriptorHash,
                receivedOnDay = today,
            )

        // An engagement notification is not a question (§6.2 L3): the user pre-approved this
        // study, so they are in it from the moment it arrives, and the audit trail has to say so.
        // Recording participation only on an explicit acceptance would leave an L3 user
        // participating in studies their own dashboard did not list.
        if (admission.posture == StudyInvitation.Posture.ENGAGEMENT_NOTIFICATION) {
            recordParticipation(descriptor.studyId, admission.descriptorHash, today)
        }
        save()
        return admission
    }

    /**
     * Whether this study already has an answer on this device — decided, joined, or withdrawn
     * from. §5.3 and §6.3 step 6 make withdrawal block future descriptor processing, and a device
     * that re-presented a withdrawn study would be forgetting an answer the user gave.
     */
    private fun hasDecided(studyId: String): Boolean =
        pendingInvitations.any { it.studyId == studyId && !it.isOpen } ||
            studyParticipations.any { it.studyId == studyId }

    /**
     * §6.3 step 4, *Yes*. Only a consent request can be accepted: an engagement notification was
     * never a question, and accepting it would record a second participation for a study the user
     * is already in.
     */
    fun acceptInvitation(studyId: String) {
        val invitation = pendingInvitations.firstOrNull { it.studyId == studyId } ?: return
        if (invitation.posture != StudyInvitation.Posture.CONSENT_REQUEST || !invitation.isOpen) return

        val today = LocalDate.now().toString()
        pendingInvitations = pendingInvitations.map {
            if (it.studyId == studyId) {
                it.copy(decision = StudyInvitation.StudyDecision.Accepted(today))
            } else {
                it
            }
        }
        recordParticipation(studyId, invitation.descriptorHash, today)
        save()
    }

    /**
     * §6.3 step 4, *No* — and the L3 per-study opt-out, which is the same intent reached from the
     * other posture. For an engagement notification the user is already in the study, so declining
     * has to withdraw the participation ingestion recorded; for a consent request there is nothing
     * to withdraw.
     */
    fun declineInvitation(studyId: String) {
        val invitation = pendingInvitations.firstOrNull { it.studyId == studyId } ?: return
        if (!invitation.isOpen) return

        pendingInvitations = pendingInvitations.map {
            if (it.studyId == studyId) {
                it.copy(
                    decision = StudyInvitation.StudyDecision.Declined(LocalDate.now().toString()),
                )
            } else {
                it
            }
        }
        if (invitation.posture == StudyInvitation.Posture.ENGAGEMENT_NOTIFICATION) {
            withdrawFromStudy(studyId)
        }
        save()
    }

    /**
     * §6.3 step 4's third response — *Ask a question* (secure message to a NeurOne liaison,
     * 2 business day response).
     *
     * The invitation stays open, exactly as [askQuestionAboutExpansion] leaves an expansion request
     * pending: asking is not deciding, and the invitation must not clear as though the user had
     * answered. Delivering the question needs the same absent channel as the descriptor fetch
     * (`OI-CONSENT-07`); until it exists the UI says so plainly rather than implying a message was
     * sent.
     */
    fun askQuestionAboutInvitation(studyId: String) {
        val invitation = pendingInvitations.firstOrNull { it.studyId == studyId } ?: return
        if (invitation.posture != StudyInvitation.Posture.CONSENT_REQUEST || !invitation.isOpen) return

        pendingInvitations = pendingInvitations.map {
            if (it.studyId == studyId) {
                it.copy(
                    decision = StudyInvitation.StudyDecision.QuestionSent(
                        LocalDate.now().toString(),
                    ),
                )
            } else {
                it
            }
        }
        save()
    }

    // ── Participation audit (SHDR-class) ─────────────────────────────────

    /**
     * Append the §5.3 audit-trail record for a study the device has joined.
     *
     * Idempotent per study: a second record for a study already in the trail would make the
     * dashboard list one study twice and give [withdrawFromStudy] two rows to find. Before
     * invitations were persisted this was reachable by ordinary use — an app restart emptied
     * `pendingInvitations` while the trail survived, so the same descriptor could be ingested and
     * accepted again.
     */
    private fun recordParticipation(studyId: String, descriptorHash: String, onDay: String) {
        if (studyParticipations.any { it.studyId == studyId }) return
        studyParticipations = studyParticipations + StudyParticipationRecord(
            id = UUID.randomUUID().toString(),
            studyId = studyId,
            descriptorHash = descriptorHash,
            transmittedAtDay = onDay,
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
        // Invitations persist for the same reason expansion requests do, and for one more: §6.3
        // step 4 gives the user two business days for an answer to a question, which an inbox
        // emptied by the next app launch cannot hold. While they were in-memory only, an accepted
        // study's participation record outlived the invitation that recorded the acceptance, so
        // the trail and the inbox disagreed after every restart.
        store.getString(INVITATIONS_KEY)?.let { blob ->
            runCatching {
                pendingInvitations = json.decodeFromString(
                    ListSerializer(StudyInvitation.serializer()), blob,
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
        store.putString(
            INVITATIONS_KEY,
            json.encodeToString(ListSerializer(StudyInvitation.serializer()), pendingInvitations),
        )
    }
}
