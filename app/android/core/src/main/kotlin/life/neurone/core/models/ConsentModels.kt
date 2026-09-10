package life.neurone.core.models

import kotlinx.serialization.Serializable

// Clinical consent engine types — CLAUDE.md §6. Port of iOS ConsentModels.swift.
// Use cases map to minimum necessary UHDR elements; users select use cases,
// not data elements.

enum class ClinicianUseCaseTier(val displayName: String, val monthlyPrice: String) {
    MONITOR("Monitor", "$49/month/patient"),
    ASSESS("Assess", "$149/month/patient"),
    FULL_CLINICAL("Full Clinical", "$299/month/patient"),
    RESEARCH("Research", "$599/month/study");

    // Minimum necessary UHDR elements for this tier — must match iOS mapping.
    val uhdrElements: Set<UHDRElement>
        get() = when (this) {
            MONITOR -> setOf(
                UHDRElement.SESSION_TIMESTAMPS, UHDRElement.SESSION_DURATION,
                UHDRElement.PROTOCOL_PARAMETERS,
            )
            ASSESS -> MONITOR.uhdrElements + setOf(
                UHDRElement.EEG_WAVEFORMS, UHDRElement.NEUROFEEDBACK_SCORES,
                UHDRElement.PBM_DOSE_LOGS,
            )
            FULL_CLINICAL -> ASSESS.uhdrElements + setOf(
                UHDRElement.HRV_TIME_SERIES, UHDRElement.PPG_OPTICAL_SIGNAL,
                UHDRElement.CLOSED_LOOP_EVENTS, UHDRElement.OUTCOME_LOGS,
            )
            RESEARCH -> emptySet()  // IRB-defined; populated per study descriptor
        }
}

// UHDR data elements (per NP-FW-EMMC-001 §12)
enum class UHDRElement(val displayName: String) {
    EEG_WAVEFORMS("EEG Waveforms"),
    HRV_TIME_SERIES("HRV Time Series"),
    PPG_OPTICAL_SIGNAL("PPG Optical Signal"),
    NEUROFEEDBACK_SCORES("Neurofeedback Performance Scores"),
    SESSION_TIMESTAMPS("Session Timestamps"),
    SESSION_DURATION("Session Duration"),
    PROTOCOL_PARAMETERS("Protocol Parameters"),
    CLOSED_LOOP_EVENTS("Closed-Loop Adaptation Events"),
    PBM_DOSE_LOGS("PBM Dose (J/cm²) Per Zone"),
    OUTCOME_LOGS("User-Entered Outcome Logs"),
    EYE_STATE_LOGS("Eye Open/Closed State");

    // Lowest clinician tier that may access this element.
    val minimumTier: ClinicianUseCaseTier
        get() = when (this) {
            SESSION_TIMESTAMPS, SESSION_DURATION, PROTOCOL_PARAMETERS ->
                ClinicianUseCaseTier.MONITOR
            EEG_WAVEFORMS, NEUROFEEDBACK_SCORES, PBM_DOSE_LOGS ->
                ClinicianUseCaseTier.ASSESS
            HRV_TIME_SERIES, PPG_OPTICAL_SIGNAL, CLOSED_LOOP_EVENTS,
            OUTCOME_LOGS, EYE_STATE_LOGS ->
                ClinicianUseCaseTier.FULL_CLINICAL
        }
}

/**
 * One access decision on a clinician grant: a set of UHDR elements, the day from which the
 * clinician may see data carrying them, and — as a **separate** decision — whether that approval
 * also reached backwards over data already recorded.
 *
 * CLAUDE.md §6.1 requires retroactive and prospective access to be presented as separate consent
 * decisions "even if made simultaneously". They therefore cannot collapse into one field on the
 * grant: [ClinicianUseCaseTier.uhdrElements] is timeless, so raising a grant's tier by itself
 * hands the clinician the new elements over every session ever recorded. A user who approves an
 * expansion going forward and refuses it over history has taken a position the record has to be
 * able to hold, and a tier alone cannot hold it.
 *
 * Day granularity, like every other date on this model — consistent with SessionRecord and with
 * TIME-01's discipline of not storing finer time than a decision needs.
 */
@Serializable
data class ClinicianAccessScope(
    /** The elements this one decision covers — for an expansion, only the newly added ones. */
    val elements: Set<UHDRElement>,
    /** Prospective decision: data recorded on or after this day is in scope. "yyyy-MM-dd". */
    val effectiveFromDay: String,
    /** Retroactive decision, taken separately: data recorded BEFORE that day is in scope too. */
    val includesPriorData: Boolean,
)

@Serializable
data class ClinicianConsentGrant(
    val id: String,
    val clinicianName: String,
    val clinicianOrganization: String,
    val tier: ClinicianUseCaseTier,
    val grantedAtDay: String,      // "yyyy-MM-dd" — day granularity, consistent with SessionRecord
    val expiresAtDay: String? = null, // null = indefinite until revoked
    val isActive: Boolean = true,
    /**
     * The access decisions this grant is made of, newest last. Null because a grant written
     * before the §6.1 expansion workflow existed has none; null is "never went through the
     * workflow", which is not the same as "went through it and came out with no access".
     * Read it through [effectiveScopes], never directly.
     */
    val accessScopes: List<ClinicianAccessScope>? = null,
) {
    /**
     * The scopes as decided, or the pre-workflow equivalent for a grant that has none: the
     * tier's elements, effective from the grant day, with prior data included.
     *
     * That fallback is deliberately what a bare [tier] has always meant, so introducing the
     * workflow does not silently re-scope an existing grant. Whether an *initial* grant should
     * default to prior data at all is a real question, and a separate one from expansion —
     * `OI-CONSENT-06`.
     */
    val effectiveScopes: List<ClinicianAccessScope>
        get() = accessScopes ?: listOf(
            ClinicianAccessScope(
                elements = tier.uhdrElements,
                effectiveFromDay = grantedAtDay,
                includesPriorData = true,
            ),
        )

    /**
     * Everything this grant reaches, ignoring when the data was recorded. This is the dashboard's
     * "what can they see" line; for the time-aware answer use [elementsVisibleForDataRecordedOn].
     */
    val approvedElements: Set<UHDRElement>
        get() = effectiveScopes.flatMap { it.elements }.toSet()

    /**
     * Elements the clinician may see in data recorded on [day] ("yyyy-MM-dd") — the predicate
     * that makes the retroactive decision mean something. A scope contributes when the data is on
     * or after its effective day, or when the user separately approved prior data for it.
     *
     * ISO-8601 day strings compare correctly as strings, which is why they are stored that way.
     */
    fun elementsVisibleForDataRecordedOn(day: String): Set<UHDRElement> =
        effectiveScopes
            .filter { day >= it.effectiveFromDay || it.includesPriorData }
            .flatMap { it.elements }
            .toSet()

    /**
     * Elements this grant reaches only from some day onwards — what the user kept out of the
     * clinician's view of their earlier sessions. Surfaced on the dashboard so a "going forward
     * only" answer stays visible after the decision, rather than vanishing into the record.
     */
    val forwardOnlyElements: Set<UHDRElement>
        get() = effectiveScopes.filterNot { it.includesPriorData }.flatMap { it.elements }.toSet()
}

/**
 * A clinician's request to widen an existing grant, held until the user decides.
 *
 * §6.1's workflow is *differential consent document → persistent user notification → user
 * approves / denies / asks questions → retroactive access is a separate decision*. This type is
 * the persistent half: it survives process death because a notification the user can lose by
 * backgrounding the app is not a notification, and it stays pending after a question is sent,
 * because asking is not deciding.
 */
@Serializable
data class ClinicianAccessExpansionRequest(
    val id: String,
    val grantId: String,
    val fromTier: ClinicianUseCaseTier,
    val toTier: ClinicianUseCaseTier,
    val requestedAtDay: String,
    val decision: ExpansionDecision? = null,
) {
    /**
     * The two §6.1 decisions are kept apart in the record, not merged into one Boolean:
     * [ExpansionDecision.Approved] carries the prospective answer, and its [includesPriorData]
     * carries the retroactive one that was asked separately after it.
     */
    @Serializable
    sealed interface ExpansionDecision {
        @Serializable
        data class Approved(val decidedOnDay: String, val includesPriorData: Boolean) :
            ExpansionDecision

        @Serializable
        data class Denied(val decidedOnDay: String) : ExpansionDecision

        @Serializable
        data class QuestionSent(val sentOnDay: String) : ExpansionDecision
    }

    /**
     * Still awaiting a decision. A sent question leaves the request pending — the user has not
     * approved or denied anything, and the notification must not disappear as though they had.
     */
    val isPending: Boolean
        get() = decision == null || decision is ExpansionDecision.QuestionSent

    val questionSentOnDay: String?
        get() = (decision as? ExpansionDecision.QuestionSent)?.sentOnDay
}

enum class ContactFrequency { WEEKLY, MONTHLY, QUARTERLY }

/**
 * The four consent layers (CLAUDE.md §6.2).
 *
 * Layers are not screens: since Rev 37 they are presented across two screens
 * (S1 = L4 + L1, S2 = L2 + L3), but the layer identities are what this type, the
 * withdrawal surfaces and the document set all key on.
 *
 * L2 and L3 are orthogonal and must never be collapsed into one field. L2 is **scope**
 * (which research areas); L3 is **posture** (ask-me-each-time versus pre-approved).
 * "All nine categories, but ask me about each study" is a real and distinct position,
 * expressible only while both survive (§6.2.2).
 */
@Serializable
data class ResearchConsentState(
    // L1: Contact consent
    val contactConsentGranted: Boolean = false,
    val contactMethod: String = "",
    val contactFrequency: ContactFrequency = ContactFrequency.MONTHLY,
    val isPoaHolder: Boolean = false,

    // L2: Category consent (9 research categories)
    val categoryConsents: Map<ResearchCategory, Boolean> =
        ResearchCategory.entries.associateWith { false },

    // L3: Blanket consent
    val blanketConsentGranted: Boolean = false,

    // L4: Results + community
    val resultsOptIn: Boolean = false,
    val suggestionPortalOptIn: Boolean = false,
) {
    val hasAnyResearchConsent: Boolean
        get() = contactConsentGranted || blanketConsentGranted ||
            categoryConsents.values.any { it }

    /**
     * True when every research category is selected. Drives the Select-all affordance's
     * checked state (§6.2.3). Says nothing about [blanketConsentGranted] — selecting all
     * nine categories is L2 scope, not L3 posture.
     */
    val allCategoriesSelected: Boolean
        get() = ResearchCategory.entries.all { categoryConsents[it] == true }

    /**
     * Set or clear all nine categories at once.
     *
     * Deliberately does NOT touch [blanketConsentGranted] in either direction. Ticking nine
     * boxes expresses breadth of interest; the blanket toggle surrenders the right to be
     * consulted, and inferring the second from the first attributes to the user a decision
     * they did not make (§6.2.3). Coupling them here would also mean a later un-tick could
     * trigger the research-analytics teardown that only blanket withdrawal should cause.
     */
    fun withAllCategories(granted: Boolean): ResearchConsentState =
        copy(categoryConsents = ResearchCategory.entries.associateWith { granted })
}

enum class ResearchCategory(val displayName: String) {
    ALZHEIMERS_AND_DEMENTIA("Alzheimer's / Dementia"),
    DEPRESSION("Depression"),
    PTSD("PTSD"),
    TBI("Traumatic Brain Injury"),
    SLEEP("Sleep Disorders"),
    ATTENTION("Attention / ADHD"),
    PARKINSONS("Parkinson's Disease"),
    HEALTHY_AGEING("Healthy Ageing"),
    VISUAL_HEALTH("Visual Health"),
}

// Study participation record (audit trail — SHDR-class, not UHDR)
@Serializable
data class StudyParticipationRecord(
    val id: String,
    val studyId: String,
    val descriptorHash: String,     // SHA-256 of signed study descriptor
    val transmittedAtDay: String,   // day granularity
    val extractBytes: Long,
    val withdrawnAtDay: String? = null,
) {
    val isActive: Boolean get() = withdrawnAtDay == null
}

// ── Signed study descriptor (§5.3 step 1) ────────────────────────────────────

/**
 * A study as NeurOne's research service issues it: what it wants, how it undertakes to anonymise
 * it, and a signature over all of that.
 *
 * **This, not [StudyInvitation], is what enters the device.** The invitation the user reads is
 * *derived* from a descriptor on-device, after the signature verifies — so every sentence on the
 * consent surface is one the app wrote, and the party asking for access supplies facts about the
 * study rather than the prose describing it. The descriptor used to be skipped entirely:
 * `addInvitation` took a finished invitation, plain-language "what they cannot see" list and all,
 * on trust from whatever called it (`OI-CONSENT-03`).
 */
@Serializable
data class StudyDescriptor(
    val studyId: String,
    /**
     * The study's own name. The one field that is server-supplied text and stays that way: it
     * names a specific study the way a part number names a part (CLAUDE.md §17).
     */
    val studyTitle: String,
    val researchCategories: List<ResearchCategory>,
    /**
     * The UHDR elements the study asks for. What it *cannot* see is derived from this, not
     * asserted alongside it — see [StudyInvitation.cannotLearn].
     */
    val requestedElements: Set<UHDRElement>,
    /**
     * §5.3 anonymisation parameters, checked against their floors at ingestion. The device is the
     * only place they can be checked, because §5.3 puts the anonymisation on the device.
     */
    val kAnonymity: Int,
    val dateRoundingDays: Int,
    val issuedOnDay: String,
    /**
     * Detached signature over the canonical descriptor bytes. Never parsed here: it is the
     * verifier's input, and nothing derived from an unverified descriptor may reach the user.
     */
    val signature: String,
)

// ── Study descriptor verification (§5.3 step 1) ──────────────────────────────

/**
 * The result of checking a descriptor's signature.
 *
 * [Unavailable] is a third answer rather than a flavour of [Rejected] because "we cannot check"
 * and "we checked and it is forged" are different facts about the world, and the device says
 * which. Both refuse ingestion.
 */
sealed interface StudyDescriptorVerification {
    /**
     * Signature checks out. Carries the SHA-256 of the exact bytes that were signed — the §5.3
     * audit-trail hash, produced by the only component that has those bytes.
     */
    data class Verified(val descriptorHash: String) : StudyDescriptorVerification
    data object Rejected : StudyDescriptorVerification

    /** No signing key is configured, so no descriptor can be checked at all. */
    data object Unavailable : StudyDescriptorVerification
}

/**
 * Port for descriptor signature verification. §5.3 locks that study descriptors are
 * cryptographically signed; the key distribution and the fetch that carries them belong to the
 * NeurOne study service, which does not exist yet (`OI-CONSENT-07`).
 */
fun interface StudyDescriptorVerifier {
    fun verify(descriptor: StudyDescriptor): StudyDescriptorVerification
}

/**
 * The default verifier, and the reason `ConsentStore` takes one at all: it verifies nothing and
 * admits nothing.
 *
 * A store that ingested descriptors without a verifier would let whoever builds the transport
 * decide whether §5.3's signature is checked, and the store could not tell a verified descriptor
 * from a fabricated one. With this as the default the decision is not theirs to skip: until a real
 * verifier is injected, ingestion refuses everything and says why.
 */
object RefusingStudyDescriptorVerifier : StudyDescriptorVerifier {
    override fun verify(descriptor: StudyDescriptor): StudyDescriptorVerification =
        StudyDescriptorVerification.Unavailable
}

// ── Study descriptor ingestion outcome (§6.3) ────────────────────────────────

/**
 * What the device did with a descriptor. Every refusal names its reason, because "nothing
 * appeared" is the one outcome a user cannot distinguish from "nothing was sent".
 */
sealed interface StudyDescriptorAdmission {
    /** Admitted, as an invitation of this posture. */
    data class Admitted(
        val posture: StudyInvitation.Posture,
        val descriptorHash: String,
    ) : StudyDescriptorAdmission

    data class Refused(val reason: Reason) : StudyDescriptorAdmission

    enum class Reason {
        /** No signing key configured — the state of every device today (`OI-CONSENT-07`). */
        VERIFIER_UNAVAILABLE,
        SIGNATURE_INVALID,

        /**
         * k < 10 or date rounding < 1 week (§5.3). A study whose anonymisation is below the locked
         * floor is not one the user may be asked to consent to.
         */
        ANONYMISATION_BELOW_FLOOR,

        /**
         * L1 is the shared precondition for every delivery path, invitations and engagement
         * notifications alike (§6.2.1).
         */
        NO_CONTACT_CONSENT,

        /** No L2 category on this descriptor is consented, and L3 is off. */
        CATEGORY_NOT_CONSENTED,

        /** The user holds no research consent at any layer. */
        NO_RESEARCH_CONSENT,

        /**
         * This study was already decided or already withdrawn from. §5.3 and §6.3 step 6 make
         * withdrawal block future descriptor processing; re-asking would be the device forgetting
         * an answer the user already gave.
         */
        STUDY_ALREADY_DECIDED,
    }
}

// ── Per-project consent notification (§6.3) ──────────────────────────────────

/**
 * A study as the user sees it — derived from a verified [StudyDescriptor], never received
 * ready-made.
 */
@Serializable
data class StudyInvitation(
    val studyId: String,
    val studyTitle: String,
    val researchCategories: List<ResearchCategory>,
    val approvedElements: Set<UHDRElement>,
    /**
     * SHA-256 of the signed descriptor this was derived from (§5.3 audit trail). Carried onto the
     * participation record on acceptance, which used to record the literal string `"pending"`
     * because there was no descriptor to hash.
     */
    val descriptorHash: String,
    val kAnonymity: Int,
    val dateRoundingDays: Int,
    val posture: Posture,
    val receivedOnDay: String,
    val decision: StudyDecision? = null,
) {
    /**
     * **Which question the device is putting to the user, and what silence means.**
     *
     * §6.2 locks that an L3 user "still receives per-study *engagement* notifications, **not
     * consent requests**" — they pre-approved NeurOne-reviewed research and may opt out per study.
     * So the same study reaches an L2 user and an L3 user as two different things, and the default
     * on no answer is opposite: an unanswered consent request means *not participating*, an unread
     * engagement notification means *participating*.
     *
     * One shape for both would have to pick one of those defaults for everyone. That is the reason
     * this type could not simply be persisted as it was.
     */
    enum class Posture {
        /** L2 path: the user is being asked, and is not in the study until they say yes. */
        CONSENT_REQUEST,

        /**
         * L3 path: the user pre-approved this study, is in it already, and is being told — with a
         * per-study opt-out that goes through `withdrawFromStudy`.
         */
        ENGAGEMENT_NOTIFICATION,
    }

    /** §6.3 step 4's three responses: Yes / No / Ask a question. */
    @Serializable
    sealed interface StudyDecision {
        @Serializable
        data class Accepted(val decidedOnDay: String) : StudyDecision

        @Serializable
        data class Declined(val decidedOnDay: String) : StudyDecision

        @Serializable
        data class QuestionSent(val sentOnDay: String) : StudyDecision
    }

    /**
     * The elements this study cannot see — **derived here, never asserted by the descriptor.**
     *
     * §6.3 step 3 requires the invitation to be "explicit about what researchers CAN and CANNOT
     * see". Taking the second half as prose from the party asking for access lets that party write
     * it, and it drifts from the first half by construction the moment either changes. It is the
     * complement of [approvedElements], so the app computes it.
     */
    val cannotLearn: Set<UHDRElement>
        get() = UHDRElement.entries.toSet() - approvedElements

    /**
     * Still in the user's inbox. A sent question leaves it open — asking is not deciding, the same
     * rule [ClinicianAccessExpansionRequest.isPending] holds for §6.1.
     */
    val isOpen: Boolean
        get() = decision == null || decision is StudyDecision.QuestionSent

    /**
     * Whether an unanswered invitation means the user is in the study. True only for an engagement
     * notification, where L3 already answered.
     */
    val participatesWithoutAnswer: Boolean
        get() = posture == Posture.ENGAGEMENT_NOTIFICATION

    val questionSentOnDay: String?
        get() = (decision as? StudyDecision.QuestionSent)?.sentOnDay
}
