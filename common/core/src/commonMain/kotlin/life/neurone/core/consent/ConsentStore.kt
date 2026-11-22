package life.neurone.core.consent

import life.neurone.core.platform.todayIso
import life.neurone.core.analytics.ResearchAnalyticsGate
import life.neurone.core.research.ResearchSuggestionStore
import life.neurone.core.common.KeyValueStore
import life.neurone.core.common.PresenceLock
import life.neurone.core.common.PresenceState
import life.neurone.core.common.SecretKeyUnavailableException
import life.neurone.core.models.ClinicianAccessExpansionRequest
import life.neurone.core.models.ClinicianAccessScope
import life.neurone.core.models.ClinicianConsentGrant
import life.neurone.core.models.ClinicianUseCaseTier
import life.neurone.core.models.QuarantinedConsentBlob
import life.neurone.core.models.ResearchContact
import life.neurone.core.models.WithdrawalMark
import life.neurone.core.models.RefusingStudyDescriptorVerifier
import life.neurone.core.models.ResearchCategory
import life.neurone.core.models.ResearchConsentState
import life.neurone.core.models.StudyDescriptor
import life.neurone.core.models.StudyDescriptorAdmission
import life.neurone.core.models.StudyDescriptorVerifier
import life.neurone.core.models.StudyInvitation
import life.neurone.core.models.StudyParticipationRecord
import life.neurone.core.models.UHDRElement
import life.neurone.core.npps.NppsCore
import life.neurone.core.protocol.PersistedKeys
import life.neurone.core.common.UUID
import kotlinx.serialization.builtins.ListSerializer
import kotlinx.serialization.builtins.MapSerializer
import kotlinx.serialization.builtins.serializer
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
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
    /** The day a withdrawal is marked with; injectable so a test can put two withdrawals on different days. */
    private val today: () -> String = ::todayIso,
    /**
     * Run when the suggestion-portal opt-in or blanket research consent goes from granted to withdrawn, by a screen, by the named
     * withdrawal or by a restored withdrawal record. The composition root gives it `ResearchSuggestionStore.forgetAuthorToken`,
     * so the token that joins a person's suggestions does not outlive either consent (`OI-PRIV-08` (3)). Keyed on the
     * transition, like the analytics teardown: a state that was never granted withdraws nothing.
     */
    private val onPortalIdentityWithdrawn: () -> Unit = {},
    /**
     * The presence lock when the host gates UHDR (`NP-APP-PRESENCE-001`), or null when it does not. With a lock this store holds no
     * consent while locked and reads it again at each unlock (`REQ-PRES-01` (d), `OI-PRIV-19`). Used from one thread, as every store
     * here is: the lock listener and the operations below do not interleave.
     */
    private val lock: PresenceLock? = null,
) {
    companion object {
        const val GRANTS_KEY = "np.consent.clinician-grants"
        const val RESEARCH_KEY = "np.consent.research"
        const val EXPANSIONS_KEY = "np.consent.clinician-expansions"
        const val INVITATIONS_KEY = "np.consent.study-invitations"
        const val STUDY_WITHDRAWALS_KEY = "np.consent.study-withdrawals"

        /** Stored records that could not be read, as found (`REQ-CONSENT-20`). Never moved in a migration. */
        const val QUARANTINE_KEY = "np.consent.quarantine"

        /**
         * The records research admission depends on. One of them unreadable means the device cannot tell
         * "never asked" from "said stop". The clinician keys are not here: an unreadable grant list reads as
         * no grants, which already gives a clinician nothing.
         */
        private val RESEARCH_RECORD_KEYS = setOf(
            RESEARCH_KEY, STUDY_WITHDRAWALS_KEY, PersistedKeys.PARTICIPATION_KEY, INVITATIONS_KEY, QUARANTINE_KEY,
        )

        /** [day] is a real calendar day written "yyyy-MM-dd", the only form that orders as text. */
        fun isIsoDay(day: String): Boolean = ConsentEngine.isIsoDay(day)

        /**
         * A `contactMethod` or `contactChannel` key and its string value, in plain or escaped JSON text: group 1 is everything up
         * to the value's opening quote, which is kept, and the value is dropped. Used only on text that no longer parses.
         */
        private val CONTACT_IN_TEXT = Regex("""(contact(?:Method|Channel)\\*"\s*:\s*\\*")[^"\\]*""")
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
     * Studies the user withdrew from, and when (a [WithdrawalMark]). UHDR-class and kept apart from [studyParticipations], which
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
     * Stored consent records that could not be read, kept as they were found (`REQ-CONSENT-20`). Kept for good:
     * a later version of the app may be able to read what this one could not. The one exception is the L1 contact method,
     * which is blanked in every one when L1 is off (`REQ-CONSENT-23` c), because nothing ever reads it out of one.
     */
    var quarantinedBlobs: List<QuarantinedConsentBlob> = emptyList()
        private set

    /**
     * A record research admission depends on could not be read, and the person has not decided again since.
     * While this holds no study is admitted, no consent request can be accepted, and the dashboard says so.
     * It clears when the person commits a research-consent decision ([updateResearchConsent],
     * [withdrawBlanketResearchConsent]), which is then the record.
     */
    val consentRecordUnreadable: Boolean
        get() = quarantinedBlobs.any { it.redecidedOnDay == null && it.key in RESEARCH_RECORD_KEYS }

    /**
     * Whether the properties above are what the store holds. False while locked: a locked read answers "absent", which is not "never
     * asked", and a state built on it would replace the sealed consents when it was written (`OI-PRIV-19`). Everything reads as
     * the default then, which admits nothing and grants nothing.
     */
    val isReadable: Boolean get() = loaded

    private var loaded = false

    /**
     * Changes made while locked, oldest first. Each is an **operation** (withdraw, revoke, decline, record a request), not a value:
     * at the unlock it runs on the sealed state that was read, so it can neither replace that state nor be lost (a withdrawal made
     * locked must survive, `CLAUDE.md` §6.0; the principal's ruling on `OI-PRIV-19`, 2026-11-21). A change that would widen what is
     * shared (a grant, an approval, an acceptance, a study descriptor), or that commits a whole state built on what a locked screen
     * could not show, is refused while locked instead of being held.
     */
    private val held = mutableListOf<() -> Unit>()

    init {
        if (lock == null || lock.isUnlocked) {
            load()
            loaded = true
        }
        lock?.onStateChanged { state -> if (state == PresenceState.LOCKED) forget() else reload() }
    }

    /** Hold [op] for the next unlock if the store cannot be read now. True if it was held, and the caller then does nothing more. */
    private fun deferred(op: () -> Unit): Boolean {
        if (loaded) return false
        held += op
        return true
    }

    /** The lock closed: nothing of the person's consent stays decrypted in this store. */
    private fun forget() {
        clinicianGrants = emptyList()
        researchConsent = ResearchConsentState()
        studyParticipations = emptyList()
        pendingInvitations = emptyList()
        studyWithdrawals = emptyMap()
        expansionRequests = emptyList()
        quarantinedBlobs = emptyList()
        loaded = false
    }

    /** The lock opened: read the sealed state, then run what was held on it, in order. */
    private fun reload() {
        load()
        loaded = true
        val operations = held.toList()
        held.clear()
        operations.forEach { it() }
    }

    /** Whether a write can reach the store now. */
    private fun writable(): Boolean = lock == null || lock.isUnlocked

    // ── Clinician consent ────────────────────────────────────────────────

    /**
     * Record a grant. Refused, and false returned, unless:
     *  - it is time-limited (§5.1, `REQ-CONSENT-18`): it carries an expiry that is a real day and has not passed;
     *  - it is built from use cases its tier offers, and its scopes reach only what they need ([ConsentEngine.isWellFormedNewGrant],
     *    `REQ-CONSENT-15` (g));
     *  - no grant with its id is held, revoked ones included. A grant used to replace one with the same id silently, which
     *    could rewrite the record of a revoked grant or widen a live one with no differential shown (`OI-PRIV-64`).
     *
     * No longest expiry is enforced: a maximum would be a number with no derivation (`CLAUDE.md` §18), so how long is the
     * person's decision, as the form already makes it.
     */
    fun grantClinicianAccess(grant: ClinicianConsentGrant): Boolean {
        if (!loaded) return false // widens what is shared: not while the person is not present
        val expiry = grant.expiresAtDay
        if (expiry == null || !isIsoDay(expiry) || !grant.isInForce(todayIso())) return false
        if (!ConsentEngine.isWellFormedNewGrant(grant)) return false
        if (clinicianGrants.any { it.id == grant.id }) return false
        clinicianGrants = clinicianGrants + grant
        save()
        return true
    }

    /**
     * Revoke a grant. It is kept, inactive and marked with the day, as the record of who had access, to what and when
     * (§5.1: clinician access is audited and revocable; `OI-PRIV-64`). [ClinicianConsentGrant.isInForce] is false for it, so
     * it reaches nothing and cannot be widened. Revoking one already revoked changes nothing, so the first day stands.
     */
    fun revokeClinicianAccess(grantId: String) {
        if (deferred { revokeClinicianAccess(grantId) }) return
        val day = todayIso()
        clinicianGrants = clinicianGrants.map {
            if (it.id == grantId && it.revokedOnDay == null) it.copy(isActive = false, revokedOnDay = day) else it
        }
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
        if (deferred { addExpansionRequest(request) }) return
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
        if (!loaded) return // widens what is shared, and a request cannot be shown while locked
        val request = expansionRequests.firstOrNull { it.id == requestId && it.isPending } ?: return

        // Fail closed on a stale request: the grant may have been revoked, or already widened past
        // this tier, since the request was raised. Approving what the differential document no
        // longer describes would apply a change the user was not shown.
        val grant = clinicianGrants.firstOrNull { it.id == request.grantId } ?: return
        // The differential is also null for a grant that is no longer in force (`REQ-CONSENT-18`). What it says is added
        // is what is applied: the use cases' elements within the tier, never the tier's whole set (`REQ-CONSENT-15` f).
        val today = todayIso()
        val differential = ConsentEngine.accessDifferential(grant, request, today) ?: return

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
            elements = differential.newlyVisibleElements,
            useCaseIds = differential.useCaseIds,
            includePriorData = includePriorData,
            decidedOnDay = today,
        )
    }

    /**
     * Record a denial. Nothing about the grant changes — denying is the reversible direction, and
     * the clinician may raise a fresh request.
     */
    fun denyExpansion(requestId: String) {
        if (deferred { denyExpansion(requestId) }) return
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
        if (deferred { askQuestionAboutExpansion(requestId) }) return
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
     *
     * [elements] are the differential's newly visible set, derived from the request's use cases ([ConsentEngine.accessDifferential]);
     * the use cases join the grant's own, so the grant can still be explained in the words it was widened in (§6.1).
     */
    private fun expandClinicianAccess(
        grantId: String,
        newTier: ClinicianUseCaseTier,
        elements: Set<UHDRElement>,
        useCaseIds: Set<String>,
        includePriorData: Boolean,
        decidedOnDay: String,
    ) {
        clinicianGrants = clinicianGrants.map { grant ->
            if (grant.id != grantId) {
                grant
            } else {
                grant.copy(
                    accessScopes = grant.effectiveScopes + ClinicianAccessScope(
                        elements = elements - grant.approvedElements,
                        effectiveFromDay = decidedOnDay,
                        includesPriorData = includePriorData,
                    ),
                    tier = newTier,
                    useCaseIds = (grant.useCaseIds.orEmpty() + useCaseIds).distinct().sorted(),
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
        // A whole state built on what a locked screen could show would replace the sealed consents. A withdrawal made locked goes
        // through the operations below.
        if (!loaded) return
        val wasBlanketGranted = researchConsent.blanketConsentGranted
        // While the stored record is unreadable the device cannot know blanket consent was never granted, so a commit with it
        // off may be the person saying stop. It gets what withdrawBlanketResearchConsent gives the same state (`REQ-CONSENT-20`
        // c, `OI-PRIV-64`): research analytics and the portal token go at once, because a removal costs the person nothing
        // they cannot turn on again. The withdrawal itself is recorded only when the commit consents to no category: a
        // recorded blanket withdrawal outranks the L2 ticks (`REQ-CONSENT-04` e), so marking one would bar the very scope the
        // person chose in the same step. Whether that commit is also "saying stop" is the open ruling on `OI-PRIV-64`.
        val mayHaveBeenGranted = !wasBlanketGranted && consentRecordUnreadable && !state.blanketConsentGranted
        val recordsMayHaveBeenGranted = mayHaveBeenGranted && state.categoryConsents.none { it.value }
        val isWithdrawal = (wasBlanketGranted && !state.blanketConsentGranted) || recordsMayHaveBeenGranted

        // blanketConsentWithdrawnOnDay is the store's to set, never the screen's. The UI commits a
        // whole state and cannot see the transition this method is built around; carrying the
        // committed value through would let a stale screen erase the record of the user having
        // said stop, which is the one thing §6.0 needs to survive.
        val before = researchConsent
        val next = withTimes(researchConsent, withContact(researchConsent, state))
        researchConsent = next.copy(
            blanketConsentWithdrawnOnDay = if (isWithdrawal) {
                todayIso()
            } else {
                researchConsent.blanketConsentWithdrawnOnDay
            },
            blanketConsentWithdrawnAt = if (recordsMayHaveBeenGranted) {
                WithdrawalMark.next(researchConsent.blanketConsentWithdrawnAt, today())
            } else {
                next.blanketConsentWithdrawnAt
            },
        )
        // The address goes from every copy the store holds when L1 is off, quarantined records included (`REQ-CONSENT-23` c).
        if (!researchConsent.contactConsentGranted) scrubQuarantinedContact()
        // A commit from the consent UI is the person deciding, so it is what ends a quarantine.
        markRedecided()
        withdrawUncoveredParticipations()
        save()
        if (mayHaveBeenGranted) forgetPortalIdentity() else portalIdentityAfter(before)

        if (isWithdrawal || mayHaveBeenGranted) {
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
        if (!loaded) {
            // The person is saying stop now. What a removal can do needs no presence, so it is done at once; the withdrawal mark
            // is written on the sealed state at the unlock, where "was it granted" can be read (`REQ-PRES-01` (b)).
            revokeResearchAnalytics()
            forgetPortalIdentity()
            held += { withdrawBlanketResearchConsent() }
            return
        }
        // Guarded on the transition, like updateResearchConsent: calling this when blanket consent
        // was never granted stops nothing, and marking it would silently bar an L2 participant
        // from studies they are still consenting to.
        val wasGranted = researchConsent.blanketConsentGranted
        val before = researchConsent
        // While the stored record is unreadable the device cannot know blanket consent was never granted, and
        // the person is saying stop now. That is recorded as a withdrawal (`REQ-CONSENT-20`).
        val mayHaveBeenGranted = !wasGranted && consentRecordUnreadable
        val next = withTimes(researchConsent, researchConsent.copy(blanketConsentGranted = false))
        researchConsent = next.copy(
            blanketConsentWithdrawnOnDay = if (wasGranted || mayHaveBeenGranted) {
                todayIso()
            } else {
                researchConsent.blanketConsentWithdrawnOnDay
            },
            blanketConsentWithdrawnAt = if (mayHaveBeenGranted) {
                WithdrawalMark.next(researchConsent.blanketConsentWithdrawnAt, today())
            } else {
                next.blanketConsentWithdrawnAt
            },
        )
        markRedecided()
        withdrawUncoveredParticipations()
        save()
        // An unreadable record that may have held the grants is treated as a withdrawal here too (`REQ-CONSENT-20`), so the
        // author token goes with it.
        if (mayHaveBeenGranted) forgetPortalIdentity() else portalIdentityAfter(before)
        revokeResearchAnalytics()
    }

    /** Tell the portal its author token is withdrawn if the change from [before] took the portal opt-in or blanket consent away. */
    private fun portalIdentityAfter(before: ResearchConsentState) {
        val now = researchConsent
        val portalWithdrawn = before.suggestionPortalOptIn && !now.suggestionPortalOptIn
        val blanketWithdrawn = before.blanketConsentGranted && !now.blanketConsentGranted
        if (portalWithdrawn || blanketWithdrawn) forgetPortalIdentity()
    }

    /**
     * The stored author token is removed here, whoever listens: the callback's default does nothing, so a host that builds this
     * store without wiring the portal store would otherwise keep the token past the withdrawal. The callback then lets the portal
     * store take the token off the suggestions it holds.
     */
    private fun forgetPortalIdentity() {
        store.remove(ResearchSuggestionStore.TOKEN_KEY)
        onPortalIdentityWithdrawn()
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
        if (!loaded) {
            if (!granted) held += { setCategoryConsent(category, false) } // a withdrawal is held; a grant is not made locked
            return
        }
        researchConsent = withTimes(
            researchConsent,
            researchConsent.copy(categoryConsents = researchConsent.categoryConsents + (category to granted)),
        )
        withdrawUncoveredParticipations()
        save()
    }

    /**
     * Stop every participation the research consent no longer covers (`REQ-CONSENT-19`).
     *
     * A withdrawal used to change the consent state and leave [studyParticipations] alone, so a study joined
     * under blanket consent stayed active after blanket consent was withdrawn, and anything that sends extracts
     * for active participations would have kept sending (§5.3, `OI-PRIV-06` point 1). Run after every change to
     * [researchConsent], and when the store is read, so a participation is active only while
     * [ConsentEngine.covers] holds for it: blanket withdrawal stops all of them, a category withdrawal stops the
     * studies left with no consented category, and a study whose categories are not on record is covered by
     * blanket consent alone.
     *
     * Only [StudyParticipationRecord.withdrawnAtDay] is written. [studyWithdrawals] is not: that is the list of
     * studies the person named, and it bars a study for good, which a blanket or category withdrawal did not
     * ask for. An engagement notification for a stopped study is closed with it, because it tells the person
     * they are in a study they are no longer in.
     *
     * Skipped while the stored record is unreadable: the state in memory is then defaults, not the person's.
     */
    private fun withdrawUncoveredParticipations() {
        if (consentRecordUnreadable) return
        val stopped = studyParticipations
            .filter { it.isActive && !ConsentEngine.covers(researchConsent, categoriesOf(it.studyId)) }
            .map { it.studyId }
            .toSet()
        if (stopped.isEmpty()) return
        val today = todayIso()
        studyParticipations = studyParticipations.map {
            if (it.studyId in stopped && it.isActive) it.copy(withdrawnAtDay = today) else it
        }
        pendingInvitations = pendingInvitations.map {
            if (it.studyId in stopped && it.isOpen && it.posture == StudyInvitation.Posture.ENGAGEMENT_NOTIFICATION) {
                // A stop the person's withdrawal caused is a withdrawal, not a decline (§6.2, `REQ-CONSENT-11` c).
                it.copy(decision = StudyInvitation.StudyDecision.Withdrawn(today))
            } else {
                it
            }
        }
    }

    /** The categories of a study this device holds an invitation for, or null if it holds none. */
    private fun categoriesOf(studyId: String): List<ResearchCategory>? =
        pendingInvitations.firstOrNull { it.studyId == studyId }?.researchCategories

    /** The person has committed a research-consent decision, which ends a quarantine (`REQ-CONSENT-20`). */
    private fun markRedecided() {
        if (!consentRecordUnreadable) return
        val today = todayIso()
        quarantinedBlobs = quarantinedBlobs.map {
            if (it.redecidedOnDay == null && it.key in RESEARCH_RECORD_KEYS) it.copy(redecidedOnDay = today) else it
        }
    }

    /**
     * Erase the L1 contact method from every quarantined research record (`REQ-CONSENT-23` c, `OI-PRIV-64`).
     *
     * A quarantined record is kept as found so its withdrawal marks are never lost (`REQ-CONSENT-20` a), but a contact method
     * is never read out of one (`REQ-CONSENT-20` e), so the copy it holds serves nothing once L1 is off. Only the method and its
     * channel are blanked; every other byte, withdrawal marks included, stays as found. A record that still parses as JSON
     * loses the two fields; one that does not (truncated, or itself a quarantine list holding records as escaped text) has
     * the value after each `contactMethod` key blanked in place. What cannot be scrubbed: an address in a record so damaged
     * that its `contactMethod` key is no longer legible, and the tail of a free-text method written before `REQ-CONSENT-23`
     * that contains a quotation mark.
     */
    private fun scrubQuarantinedContact() {
        val scrubbed = quarantinedBlobs.map { q ->
            if (q.key != RESEARCH_KEY && q.key != QUARANTINE_KEY) q else q.copy(blob = withoutContact(q.blob))
        }
        if (scrubbed == quarantinedBlobs) return
        quarantinedBlobs = scrubbed
        saveQuarantine()
    }

    private fun withoutContact(blob: String): String {
        val obj = parsed(blob)
        if (obj != null && (obj.containsKey("contactMethod") || obj.containsKey("contactChannel"))) {
            return json.encodeToString(JsonObject.serializer(), JsonObject(obj - "contactMethod" - "contactChannel"))
        }
        return CONTACT_IN_TEXT.replace(blob) { m -> m.groupValues[1] }
    }

    /**
     * [next] with every withdrawal time the store owns taken from [old] and stamped now on a true→false transition, so a
     * screen that commits a whole state can neither set nor erase one (`SWR-MG-04`, the same rule as the withdrawal day).
     */
    private fun withTimes(old: ResearchConsentState, next: ResearchConsentState): ResearchConsentState {
        val day by lazy(today)
        fun at(was: Boolean, will: Boolean, previous: String?) =
            if (was && !will) WithdrawalMark.next(previous, day) else previous
        return next.copy(
            blanketConsentWithdrawnAt = at(old.blanketConsentGranted, next.blanketConsentGranted, old.blanketConsentWithdrawnAt),
            contactConsentWithdrawnAt = at(old.contactConsentGranted, next.contactConsentGranted, old.contactConsentWithdrawnAt),
            resultsOptInWithdrawnAt = at(old.resultsOptIn, next.resultsOptIn, old.resultsOptInWithdrawnAt),
            suggestionPortalOptInWithdrawnAt = at(old.suggestionPortalOptIn, next.suggestionPortalOptIn, old.suggestionPortalOptInWithdrawnAt),
            categoryWithdrawnAt = old.categoryWithdrawnAt + ResearchCategory.entries.mapNotNull { c ->
                if (old.categoryConsents[c] == true && next.categoryConsents[c] != true) {
                    c to WithdrawalMark.next(old.categoryWithdrawnAt[c], day)
                } else {
                    null
                }
            },
        )
    }

    /**
     * [next] with its contact method in the stored form (`REQ-CONSENT-23`).
     *
     * L1 on needs a method the app can use: an email address or a phone number. A commit that turns L1 on
     * without one does not turn it on, and a commit that would replace a usable method with an unusable one
     * keeps the one held. L1 off holds no method at all: the address or number is erased with the consent.
     */
    private fun withContact(old: ResearchConsentState, next: ResearchConsentState): ResearchConsentState {
        if (!next.contactConsentGranted) return next.copy(contactMethod = "", contactChannel = null)
        val contact = ResearchContact.parse(next.contactMethod)
        return when {
            contact != null -> next.copy(contactMethod = contact.value, contactChannel = contact.channel)
            old.contactConsentGranted && old.contactChannel != null ->
                next.copy(contactMethod = old.contactMethod, contactChannel = old.contactChannel)
            else -> next.copy(contactConsentGranted = false, contactMethod = "", contactChannel = null)
        }
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
        if (deferred { applyWithdrawals(value) }) return
        // Only well-formed marks are taken from a bundle. Anything else would be stored as received and would
        // order against real marks by accident of its text (`REQ-CONSENT-24` a).
        val incoming = buildJsonObject {
            value.jsonObject.forEach { (key, held) ->
                if (held is JsonObject) {
                    put(key, buildJsonObject { recoveredMarks(held).forEach { (id, mark) -> put(id, mark) } })
                } else {
                    recoveredMarks(JsonObject(mapOf(key to held)))[key]?.let { put(key, it) }
                }
            }
        }
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
            blanketConsentWithdrawnOnDay = blanket?.let(WithdrawalMark::dayOf) ?: r.blanketConsentWithdrawnOnDay,
            contactConsentGranted = r.contactConsentGranted && contact == null,
            // A restored L1 withdrawal erases the method here too (`REQ-CONSENT-23`).
            contactMethod = if (contact == null) r.contactMethod else "",
            contactChannel = if (contact == null) r.contactChannel else null,
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
        // A restored L1 withdrawal erases the address from quarantined records too (`REQ-CONSENT-23` c).
        if (contact != null) scrubQuarantinedContact()
        studyParticipations = studyParticipations.map {
            val t = studies[it.studyId]
            if (t != null && it.isActive) it.copy(withdrawnAtDay = WithdrawalMark.dayOf(t)) else it
        }
        // A restored blanket or category withdrawal stops what it stops here too (`REQ-CONSENT-19`).
        withdrawUncoveredParticipations()
        save()
        portalIdentityAfter(r)
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
        // The device cannot tell "never asked" from "said stop" without reading the record, so it admits nothing (`REQ-CONSENT-20`).
        if (!loaded) return StudyDescriptorAdmission.Refused(StudyDescriptorAdmission.Reason.CONSENT_RECORD_UNREADABLE)
        val admission = ConsentEngine.admit(
            descriptor = descriptor,
            verification = descriptorVerifier.verify(descriptor),
            consent = researchConsent,
            studyAlreadyDecided = hasDecided(descriptor.studyId),
            consentRecordReadable = !consentRecordUnreadable,
            today = todayIso(),
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
        if (!loaded) return // a consent is never given on a state that could not be read
        val invitation = pendingInvitations.firstOrNull { it.studyId == studyId } ?: return
        if (invitation.posture != StudyInvitation.Posture.CONSENT_REQUEST || !invitation.isOpen) return
        // A request that arrived under a consent since withdrawn, or while the stored record is unreadable,
        // cannot be accepted: accepting would start a flow the withdrawal stopped (`REQ-CONSENT-19` d). The
        // invitation stays open, and is answerable again if the person consents again.
        if (consentRecordUnreadable || !ConsentEngine.covers(researchConsent, invitation.researchCategories)) return
        // Nor can a request whose descriptor has expired: the issuer's undertaking no longer stands.
        if (!ConsentEngine.notExpired(invitation.expiresOnDay, todayIso())) return

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
        if (deferred { declineInvitation(studyId) }) return
        val invitation = pendingInvitations.firstOrNull { it.studyId == studyId } ?: return
        if (!invitation.isOpen) return

        // An L3 user's opt-out is a withdrawal from a study they were in, not a decline (§6.2, `REQ-CONSENT-11` c).
        val day = todayIso()
        val decision = if (invitation.posture == StudyInvitation.Posture.ENGAGEMENT_NOTIFICATION) {
            StudyInvitation.StudyDecision.Withdrawn(day)
        } else {
            StudyInvitation.StudyDecision.Declined(day)
        }
        pendingInvitations = pendingInvitations.map {
            if (it.studyId == studyId) it.copy(decision = decision) else it
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
        if (deferred { askQuestionAboutInvitation(studyId) }) return
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
            // Joined, and nothing sent: the transmission fields stay empty until an extract goes out.
            enrolledOnDay = onDay,
        )
        save()
    }

    /** Per-study withdrawal — stops that study only; research analytics unaffected. */
    fun withdrawFromStudy(studyId: String) {
        if (deferred { withdrawFromStudy(studyId) }) return
        if (studyParticipations.any { it.studyId == studyId && it.isActive }) {
            studyWithdrawals = studyWithdrawals + (studyId to WithdrawalMark.next(studyWithdrawals[studyId], today()))
        }
        studyParticipations = studyParticipations.map {
            if (it.studyId == studyId && it.isActive) {
                it.copy(withdrawnAtDay = todayIso())
            } else {
                it
            }
        }
        save()
    }

    // ── Persistence ──────────────────────────────────────────────────────

    /**
     * Read one stored record. A record that is there and cannot be read is quarantined, never dropped: the
     * caller keeps its default, and [quarantine] keeps the text.
     */
    private fun <T> read(key: String, decode: (String) -> T): T? {
        val blob = store.getString(key) ?: return null
        return runCatching { decode(blob) }.getOrElse {
            quarantine(key, blob)
            null
        }
    }

    /**
     * Keep [blob] as found, and write the quarantine list at once, before any [save] can replace the record
     * under [key] with defaults (`REQ-CONSENT-20`).
     */
    private fun quarantine(key: String, blob: String) {
        if (quarantinedBlobs.any { it.key == key && it.blob == blob && it.redecidedOnDay == null }) return
        quarantinedBlobs = quarantinedBlobs + QuarantinedConsentBlob(key, blob, quarantinedOnDay = todayIso())
        saveQuarantine()
    }

    private fun saveQuarantine() {
        store.putString(
            QUARANTINE_KEY,
            json.encodeToString(ListSerializer(QuarantinedConsentBlob.serializer()), quarantinedBlobs),
        )
    }

    /** [blob] as a JSON object, or null if it is absent or is not one. */
    private fun parsed(blob: String?): JsonObject? =
        blob?.let { runCatching { json.parseToJsonElement(it) as? JsonObject }.getOrNull() }

    /** The entries of [obj] whose value is a well-formed [WithdrawalMark]; anything else is left behind. */
    private fun recoveredMarks(obj: JsonObject?): Map<String, String> =
        obj.orEmpty().mapNotNull { (key, value) ->
            val mark = (value as? JsonPrimitive)?.takeIf { it.isString }?.content
            if (mark != null && WithdrawalMark.isValid(mark)) key to mark else null
        }.toMap()

    /**
     * What can be said of a research record that did not decode: every consent off, and each withdrawal mark
     * that is still legible in the text (`REQ-CONSENT-20` e). Only marks are taken. A grant is never read out of
     * a record the app could not read, so this can show "said stop" and can never show a consent.
     */
    private fun recoveredWithdrawals(blob: String?): ResearchConsentState {
        val obj = parsed(blob) ?: return ResearchConsentState()
        val marks = recoveredMarks(obj)
        val categories = recoveredMarks(obj["categoryWithdrawnAt"] as? JsonObject).mapNotNull { (name, mark) ->
            ResearchCategory.entries.firstOrNull { it.name == name }?.let { it to mark }
        }.toMap()
        val blanket = marks["blanketConsentWithdrawnAt"]
        return ResearchConsentState(
            blanketConsentWithdrawnAt = blanket,
            blanketConsentWithdrawnOnDay = blanket?.let(WithdrawalMark::dayOf),
            contactConsentWithdrawnAt = marks["contactConsentWithdrawnAt"],
            resultsOptInWithdrawnAt = marks["resultsOptInWithdrawnAt"],
            suggestionPortalOptInWithdrawnAt = marks["suggestionPortalOptInWithdrawnAt"],
            categoryWithdrawnAt = categories,
        )
    }

    private fun load() {
        // First, so a record quarantined below is added to what is already held. If the list itself cannot be
        // read, its text becomes the first entry of a new one.
        read(QUARANTINE_KEY) { json.decodeFromString(ListSerializer(QuarantinedConsentBlob.serializer()), it) }
            ?.let { quarantinedBlobs = it }
        read(GRANTS_KEY) { json.decodeFromString(ListSerializer(ClinicianConsentGrant.serializer()), it) }
            ?.let { clinicianGrants = it }
        // An unreadable research record or study-withdrawal list still gives up whatever withdrawal marks can
        // be read out of it, so "said stop" is held from the start and travels in a migration (`REQ-CONSENT-20` e).
        val research = read(RESEARCH_KEY) { json.decodeFromString(ResearchConsentState.serializer(), it) }
        researchConsent = research ?: recoveredWithdrawals(store.getString(RESEARCH_KEY))
        val withdrawals = read(STUDY_WITHDRAWALS_KEY) {
            json.decodeFromString(MapSerializer(String.serializer(), String.serializer()), it)
        }
        studyWithdrawals = withdrawals ?: recoveredMarks(parsed(store.getString(STUDY_WITHDRAWALS_KEY)))
        val storedParticipations = read(PersistedKeys.PARTICIPATION_KEY) {
            json.decodeFromString(ListSerializer(StudyParticipationRecord.serializer()), it)
        }.orEmpty()
        // Expansion requests persist because §6.1 calls for a *persistent* notification: one the
        // user can dismiss by backgrounding the app is not one.
        read(EXPANSIONS_KEY) { json.decodeFromString(ListSerializer(ClinicianAccessExpansionRequest.serializer()), it) }
            ?.let { expansionRequests = it }
        // Invitations persist for the same reason expansion requests do, and for one more: §6.3
        // step 4 gives the user two business days for an answer to a question, which an inbox
        // emptied by the next app launch cannot hold. While they were in-memory only, an accepted
        // study's participation record outlived the invitation that recorded the acceptance, so
        // the trail and the inbox disagreed after every restart.
        read(INVITATIONS_KEY) { json.decodeFromString(ListSerializer(StudyInvitation.serializer()), it) }
            ?.let { pendingInvitations = it }

        // Two repairs to what was stored before `OI-PRIV-06`: a record that called its enrolment day a
        // transmission, and a participation left active by a withdrawal that should have stopped it.
        studyParticipations = storedParticipations.map { it.normalised() }
        withdrawUncoveredParticipations()
        if (studyParticipations != storedParticipations) save()
    }

    private fun save() {
        // The lock closed under a running operation: nothing is written, and the lock's own listener drops what is held in memory.
        if (!writable()) return
        try {
            saveAll()
        } catch (e: SecretKeyUnavailableException) {
            // Under a lock the key went away mid-write: the values already written stand, each being one whole value, and the rest are
            // not written. Without one the error is the host's, as it always was.
            if (lock == null) throw e
        }
    }

    private fun saveAll() {
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
        if (quarantinedBlobs.isNotEmpty()) saveQuarantine()
    }
}
