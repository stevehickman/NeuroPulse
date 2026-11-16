package life.neurone.core.consent

import life.neurone.core.platform.nowIso
import life.neurone.core.platform.todayIso
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
import life.neurone.core.npps.NppsCore
import life.neurone.core.protocol.PersistedKeys
import life.neurone.core.common.UUID
import kotlinx.serialization.KSerializer
import kotlinx.serialization.builtins.ListSerializer
import kotlinx.serialization.builtins.MapSerializer
import kotlinx.serialization.builtins.serializer
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put

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
    /** The clock for withdrawal times (UTC to the second); injectable so a test can order two withdrawals. */
    private val now: () -> String = ::nowIso,
) {
    companion object {
        const val GRANTS_KEY = "np.consent.clinician-grants"
        const val RESEARCH_KEY = "np.consent.research"
        const val EXPANSIONS_KEY = "np.consent.clinician-expansions"
        const val INVITATIONS_KEY = "np.consent.study-invitations"
        const val STUDY_WITHDRAWALS_KEY = "np.consent.study-withdrawals"

        /**
         * Consent records that could not be read, kept as they were found: a JSON object from the key a record
         * was stored under to the list of raw blobs found there. UHDR-class, like the records themselves.
         */
        const val QUARANTINE_KEY = "np.consent.quarantine"

        /** Set while a research consent record could not be read and the person has not decided again. */
        const val REDECISION_KEY = "np.consent.redecision-required"
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
     * Studies the user withdrew from, and when (UTC instant). UHDR-class and kept apart from [studyParticipations], which
     * is SHDR-class. A withdrawal stays here for good, so a restore or a re-presented descriptor cannot put the user back in
     * a study they left (`SWR-MG-04`).
     */
    var studyWithdrawals: Map<String, String> = emptyMap()
        private set

    /**
     * Clinician access-expansion requests (§6.1), decided and undecided alike — a denial is part
     * of the record. The UI shows `expansionRequests.filter { it.isPending }`.
     */
    var expansionRequests: List<ClinicianAccessExpansionRequest> = emptyList()
        private set

    /**
     * A stored research consent record could not be read, and the person has not decided again since
     * (`OI-PRIV-06` point 3). What is in [researchConsent] is then not what they decided: it may be the
     * defaults standing in for a record that said stop. While this is true no study is admitted and no
     * invitation can be accepted. Committing the consent form ([updateResearchConsent]) is the fresh decision
     * that clears it; nothing else does.
     */
    var redecisionRequired: Boolean = false
        private set

    init {
        load()
    }

    // ── Clinician consent ────────────────────────────────────────────────

    /**
     * Store a grant. Refused (false, nothing stored) when the grant has no end day, or one that is not a day
     * after the day it was granted: clinician access is time-limited (CLAUDE.md §5.1), and the place to hold
     * that is where a grant enters the store, not the form that happens to build it.
     */
    fun grantClinicianAccess(grant: ClinicianConsentGrant): Boolean {
        if (!ConsentEngine.hasUsableExpiry(grant)) return false
        clinicianGrants = clinicianGrants.filterNot { it.id == grant.id } + grant
        save()
        return true
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
        // The same refusal covers a grant that has ended since: there is no access left to widen.
        val today = todayIso()
        val grant = clinicianGrants.firstOrNull { it.id == request.grantId } ?: return
        if (ConsentEngine.accessDifferential(grant, request.toTier, today) == null) return

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
                        decidedOnDay = todayIso(),
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
                        sentOnDay = todayIso(),
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
        val previous = researchConsent
        val wasBlanketGranted = previous.blanketConsentGranted
        val isWithdrawal = wasBlanketGranted && !state.blanketConsentGranted

        // blanketConsentWithdrawnOnDay is the store's to set, never the screen's. The UI commits a
        // whole state and cannot see the transition this method is built around; carrying the
        // committed value through would let a stale screen erase the record of the user having
        // said stop, which is the one thing §6.0 needs to survive.
        researchConsent = withTimes(researchConsent, state).copy(
            blanketConsentWithdrawnOnDay = if (isWithdrawal) {
                todayIso()
            } else {
                researchConsent.blanketConsentWithdrawnOnDay
            },
        )
        withdrawUncoveredParticipations(previous, researchConsent)
        // A commit of the whole form is the person deciding again, which is what an unreadable record waits for.
        redecisionRequired = false
        save()

        if (isWithdrawal) {
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
        // Guarded on the transition, like updateResearchConsent: calling this when blanket consent
        // was never granted stops nothing, and marking it would silently bar an L2 participant
        // from studies they are still consenting to.
        val previous = researchConsent
        val wasGranted = previous.blanketConsentGranted
        researchConsent = withTimes(researchConsent, researchConsent.copy(blanketConsentGranted = false)).copy(
            blanketConsentWithdrawnOnDay = if (wasGranted) {
                todayIso()
            } else {
                researchConsent.blanketConsentWithdrawnOnDay
            },
        )
        withdrawUncoveredParticipations(previous, researchConsent)
        save()
        revokeResearchAnalytics()
    }

    /**
     * Revoke research analytics — clears the research analytics gate key and
     * tears down the SDK so it cannot collect passively after withdrawal.
     * Does NOT affect WarrantyAnalyticsGate or SHDR fleet uploads.
     */
    fun revokeResearchAnalytics() {
        store.remove(PersistedKeys.RESEARCH_ANALYTICS_KEY)
        researchAnalyticsGate.reset()
    }

    fun setCategoryConsent(category: ResearchCategory, granted: Boolean) {
        val previous = researchConsent
        researchConsent = withTimes(
            researchConsent,
            researchConsent.copy(categoryConsents = researchConsent.categoryConsents + (category to granted)),
        )
        withdrawUncoveredParticipations(previous, researchConsent)
        save()
    }

    /**
     * Withdraw the study participations that a consent withdrawal leaves without consent (`OI-PRIV-06` point 1).
     *
     * CLAUDE.md §5.3: withdrawal blocks future extracts for every data period. The admission gate already
     * refuses new studies after a withdrawal, but a study joined before it stayed active, so anything that
     * sends extracts for active participations would have kept sending.
     *
     *  - **Blanket consent went true→false:** every active participation is withdrawn. §6.0: withdrawing L3
     *    stops all research flows, and the L2 ticks that withdrawal leaves in place do not outrank it.
     *  - **A category went true→false:** a participation is withdrawn when the consent that is left would no
     *    longer admit its study, by the rule the gate uses: blanket consent admits any study, otherwise one of
     *    the study's categories must still be consented. The categories come from the invitation the
     *    participation was joined through. A participation with no invitation on record has unknown
     *    categories, and is withdrawn only when no category at all is left consented, which is the one case
     *    where it cannot be covered whatever they were.
     *
     * Keyed on the transition, like the withdrawal markers: a commit that withdraws nothing withdraws no
     * study, so editing a contact preference cannot end a participation.
     */
    private fun withdrawUncoveredParticipations(old: ResearchConsentState, next: ResearchConsentState) {
        val blanketWithdrawn = old.blanketConsentGranted && !next.blanketConsentGranted
        val categoryWithdrawn = ResearchCategory.entries.any {
            old.categoryConsents[it] == true && next.categoryConsents[it] != true
        }
        if (!blanketWithdrawn && !categoryWithdrawn) return

        fun stillCovered(studyId: String): Boolean {
            if (next.blanketConsentGranted) return true
            val categories = pendingInvitations.firstOrNull { it.studyId == studyId }?.researchCategories
                ?: return next.categoryConsents.values.any { it }
            return categories.any { next.categoryConsents[it] == true }
        }
        markStudiesWithdrawn(
            studyParticipations
                .filter { it.isActive && (blanketWithdrawn || !stillCovered(it.studyId)) }
                .map { it.studyId }
                .toSet(),
        )
    }

    /**
     * Record a withdrawal from each of [studyIds] that has an active participation: the participation is
     * marked, the study joins [studyWithdrawals] so it is never presented again, and an engagement
     * notification still open for it is closed, because it tells the person they are in the study. The
     * caller saves.
     */
    private fun markStudiesWithdrawn(studyIds: Set<String>) {
        val active = studyParticipations.filter { it.isActive && it.studyId in studyIds }.map { it.studyId }.toSet()
        if (active.isEmpty()) return
        val at = now()
        val today = todayIso()
        studyWithdrawals = studyWithdrawals + active.associateWith { at }
        studyParticipations = studyParticipations.map {
            if (it.studyId in active && it.isActive) it.copy(withdrawnAtDay = today) else it
        }
        pendingInvitations = pendingInvitations.map {
            if (it.studyId in active && it.isOpen && it.posture == StudyInvitation.Posture.ENGAGEMENT_NOTIFICATION) {
                it.copy(decision = StudyInvitation.StudyDecision.Declined(today))
            } else {
                it
            }
        }
    }

    /**
     * [next] with every withdrawal time the store owns taken from [old] and stamped now on a true→false transition, so a
     * screen that commits a whole state can neither set nor erase one (`SWR-MG-04`, the same rule as the withdrawal day).
     */
    private fun withTimes(old: ResearchConsentState, next: ResearchConsentState): ResearchConsentState {
        val t by lazy(now)
        fun at(was: Boolean, will: Boolean, previous: String?) = if (was && !will) t else previous
        return next.copy(
            blanketConsentWithdrawnAt = at(old.blanketConsentGranted, next.blanketConsentGranted, old.blanketConsentWithdrawnAt),
            contactConsentWithdrawnAt = at(old.contactConsentGranted, next.contactConsentGranted, old.contactConsentWithdrawnAt),
            resultsOptInWithdrawnAt = at(old.resultsOptIn, next.resultsOptIn, old.resultsOptInWithdrawnAt),
            suggestionPortalOptInWithdrawnAt = at(old.suggestionPortalOptIn, next.suggestionPortalOptIn, old.suggestionPortalOptInWithdrawnAt),
            categoryWithdrawnAt = old.categoryWithdrawnAt + ResearchCategory.entries.mapNotNull { c ->
                if (old.categoryConsents[c] == true && next.categoryConsents[c] != true) c to t else null
            },
        )
    }

    // ── Withdrawal records in a migration (`SWR-MG-04`) ─────────────────

    /**
     * The withdrawal records, in the shape the shared core merges (`mergeWithdrawal`). Grants are not here and never travel:
     * a restore can only reduce what a phone is consented to.
     */
    fun exportWithdrawals(): JsonObject = buildJsonObject {
        val r = researchConsent
        r.blanketConsentWithdrawnAt?.let { put("blanketConsentWithdrawnAt", it) }
        r.contactConsentWithdrawnAt?.let { put("contactConsentWithdrawnAt", it) }
        r.resultsOptInWithdrawnAt?.let { put("resultsOptInWithdrawnAt", it) }
        r.suggestionPortalOptInWithdrawnAt?.let { put("suggestionPortalOptInWithdrawnAt", it) }
        put("categoryWithdrawals", buildJsonObject { r.categoryWithdrawnAt.forEach { (c, t) -> put(c.wireName, t) } })
        put("studyWithdrawals", buildJsonObject { studyWithdrawals.forEach { (id, t) -> put(id, t) } })
    }

    /**
     * Apply merged withdrawal records. A withdrawal later than the one held here switches the matching grant off and records
     * its time; an equal or older one changes nothing, so a grant made here after the withdrawal stands. Nothing here turns a
     * grant on.
     */
    fun applyWithdrawals(value: JsonElement) {
        val incoming = value.jsonObject
        // Which withdrawals are later than the ones held here is the core's to say (`applyWithdrawal`), so every platform
        // applies a restore the same way. This store only switches the matching grants off and records the times.
        val verdict = NppsCore.migration(
            buildJsonObject {
                put("op", "applyWithdrawal")
                put("held", exportWithdrawals())
                put("incoming", incoming)
            },
        )
        val applied = verdict["applied"]!!.jsonObject
        fun names(key: String): Set<String> = applied[key]!!.jsonArray.map { it.jsonPrimitive.content }.toSet()
        fun time(key: String): String? = incoming[key]?.takeIf { it !is JsonNull }?.jsonPrimitive?.content
        val keys = names("times")
        fun applies(key: String) = key in keys
        val r = researchConsent
        val blanket = time("blanketConsentWithdrawnAt").takeIf { applies("blanketConsentWithdrawnAt") }
        val contact = time("contactConsentWithdrawnAt").takeIf { applies("contactConsentWithdrawnAt") }
        val results = time("resultsOptInWithdrawnAt").takeIf { applies("resultsOptInWithdrawnAt") }
        val portal = time("suggestionPortalOptInWithdrawnAt").takeIf { applies("suggestionPortalOptInWithdrawnAt") }
        val categoryNames = names("categoryWithdrawals")
        val categories = (incoming["categoryWithdrawals"] as? JsonObject).orEmpty().mapNotNull { (name, t) ->
            val c = ResearchCategory.entries.firstOrNull { it.wireName == name } ?: return@mapNotNull null
            if (name in categoryNames) c to t.jsonPrimitive.content else null
        }.toMap()
        researchConsent = r.copy(
            blanketConsentGranted = r.blanketConsentGranted && blanket == null,
            blanketConsentWithdrawnAt = blanket ?: r.blanketConsentWithdrawnAt,
            blanketConsentWithdrawnOnDay = blanket?.substringBefore('T') ?: r.blanketConsentWithdrawnOnDay,
            contactConsentGranted = r.contactConsentGranted && contact == null,
            contactConsentWithdrawnAt = contact ?: r.contactConsentWithdrawnAt,
            resultsOptIn = r.resultsOptIn && results == null,
            resultsOptInWithdrawnAt = results ?: r.resultsOptInWithdrawnAt,
            suggestionPortalOptIn = r.suggestionPortalOptIn && portal == null,
            suggestionPortalOptInWithdrawnAt = portal ?: r.suggestionPortalOptInWithdrawnAt,
            categoryConsents = r.categoryConsents + categories.keys.associateWith { false },
            categoryWithdrawnAt = r.categoryWithdrawnAt + categories,
        )
        val studyNames = names("studyWithdrawals")
        val studies = (incoming["studyWithdrawals"] as? JsonObject).orEmpty()
            .filterKeys { it in studyNames }
            .mapValues { it.value.jsonPrimitive.content }
        studyWithdrawals = studyWithdrawals + studies
        studyParticipations = studyParticipations.map {
            val t = studies[it.studyId]
            if (t != null && it.isActive) it.copy(withdrawnAtDay = t.substringBefore('T')) else it
        }
        // A withdrawal that arrives in a restore ends participations here exactly as one made here does.
        withdrawUncoveredParticipations(r, researchConsent)
        save()
        if (blanket != null) revokeResearchAnalytics()
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
            consentRecordUnreadable = redecisionRequired,
        )
        if (admission !is StudyDescriptorAdmission.Admitted) return admission

        val today = todayIso()
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
            studyParticipations.any { it.studyId == studyId } ||
            studyId in studyWithdrawals

    /**
     * §6.3 step 4, *Yes*. Only a consent request can be accepted: an engagement notification was
     * never a question, and accepting it would record a second participation for a study the user
     * is already in.
     */
    fun acceptInvitation(studyId: String) {
        val invitation = pendingInvitations.firstOrNull { it.studyId == studyId } ?: return
        if (invitation.posture != StudyInvitation.Posture.CONSENT_REQUEST || !invitation.isOpen) return
        // Joining a study is an admission too. While the consent record is unreadable the device does not
        // know whether the person said stop, so the invitation stays open and unanswered until they decide.
        if (redecisionRequired) return

        val today = todayIso()
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
                    decision = StudyInvitation.StudyDecision.Declined(todayIso()),
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
                        todayIso(),
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
            // Joined, and nothing sent: the transmission day and byte count stay empty until an extract is.
            enrolledAtDay = onDay,
        )
        save()
    }

    /** Per-study withdrawal — stops that study only; research analytics unaffected. */
    fun withdrawFromStudy(studyId: String) {
        markStudiesWithdrawn(setOf(studyId))
        save()
    }

    // ── Persistence ──────────────────────────────────────────────────────

    /**
     * Read every stored record. A record that does not decode is **not** left for the next [save] to overwrite
     * (`OI-PRIV-06` point 3): it used to be skipped, the field kept its default, and the first save after it
     * replaced a record that may have said stop with one that says never asked.
     *
     * The raw blob is moved to [QUARANTINE_KEY] as found, and when it is one of the research records the
     * store enters [redecisionRequired]. The clinician records are quarantined too but do not set it: losing
     * them removes access, which is the safe direction, and research consent was not what failed.
     */
    private fun load() {
        redecisionRequired = store.getBoolean(REDECISION_KEY)
        val unreadable = mutableMapOf<String, String>()
        var researchUnreadable = false

        fun <T> read(key: String, serializer: KSerializer<T>, research: Boolean, apply: (T) -> Unit) {
            val blob = store.getString(key) ?: return
            runCatching { json.decodeFromString(serializer, blob) }
                .onSuccess(apply)
                .onFailure {
                    unreadable[key] = blob
                    if (research) researchUnreadable = true
                }
        }

        read(GRANTS_KEY, ListSerializer(ClinicianConsentGrant.serializer()), research = false) { clinicianGrants = it }
        read(RESEARCH_KEY, ResearchConsentState.serializer(), research = true) { researchConsent = it }
        read(STUDY_WITHDRAWALS_KEY, MapSerializer(String.serializer(), String.serializer()), research = true) {
            studyWithdrawals = it
        }
        read(PersistedKeys.PARTICIPATION_KEY, ListSerializer(StudyParticipationRecord.serializer()), research = true) {
            // A record stored before enrolment and transmission were separate fields holds its enrolment day
            // under `transmittedAtDay`, with zero bytes. Nothing has ever been transmitted (no extractor
            // exists), so that day is moved to where it belongs.
            studyParticipations = it.map { record ->
                if (record.enrolledAtDay == null && record.extractBytes == 0L) {
                    record.copy(enrolledAtDay = record.transmittedAtDay, transmittedAtDay = null)
                } else {
                    record
                }
            }
        }
        // Expansion requests persist because §6.1 calls for a *persistent* notification: one the
        // user can dismiss by backgrounding the app is not one.
        read(EXPANSIONS_KEY, ListSerializer(ClinicianAccessExpansionRequest.serializer()), research = false) {
            expansionRequests = it
        }
        // Invitations persist for the same reason expansion requests do, and for one more: §6.3
        // step 4 gives the user two business days for an answer to a question, which an inbox
        // emptied by the next app launch cannot hold. While they were in-memory only, an accepted
        // study's participation record outlived the invitation that recorded the acceptance, so
        // the trail and the inbox disagreed after every restart.
        read(INVITATIONS_KEY, ListSerializer(StudyInvitation.serializer()), research = true) { pendingInvitations = it }

        if (unreadable.isEmpty()) return
        // Written before anything else can save, and the unreadable record is then removed from its own key so
        // a second launch does not quarantine the same blob again.
        val quarantineSerializer = MapSerializer(String.serializer(), ListSerializer(String.serializer()))
        val held = store.getString(QUARANTINE_KEY)
            ?.let { runCatching { json.decodeFromString(quarantineSerializer, it) }.getOrNull() }
            .orEmpty()
        store.putString(
            QUARANTINE_KEY,
            json.encodeToString(
                quarantineSerializer,
                held + unreadable.mapValues { (key, blob) -> held[key].orEmpty() + blob },
            ),
        )
        unreadable.keys.forEach(store::remove)
        if (researchUnreadable) {
            redecisionRequired = true
            store.putBoolean(REDECISION_KEY, true)
        }
    }

    private fun save() {
        store.putBoolean(REDECISION_KEY, redecisionRequired)
        store.putString(
            GRANTS_KEY,
            json.encodeToString(ListSerializer(ClinicianConsentGrant.serializer()), clinicianGrants),
        )
        store.putString(
            RESEARCH_KEY,
            json.encodeToString(ResearchConsentState.serializer(), researchConsent),
        )
        store.putString(
            STUDY_WITHDRAWALS_KEY,
            json.encodeToString(MapSerializer(String.serializer(), String.serializer()), studyWithdrawals),
        )
        store.putString(
            PersistedKeys.PARTICIPATION_KEY,
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
