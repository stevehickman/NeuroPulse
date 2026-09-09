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

// Per-project consent invitation (held in memory; decisions recorded to store)
data class StudyInvitation(
    val studyId: String,
    val studyTitle: String,
    val researchCategories: List<ResearchCategory>,
    val approvedElements: Set<UHDRElement>,
    val cannotLearn: List<String>,          // plain-language "what we CANNOT see"
    val irreversibilityNotice: String,
    val decision: StudyDecision? = null,
) {
    sealed interface StudyDecision {
        data object Accepted : StudyDecision
        data object Declined : StudyDecision
        data object QuestionSent : StudyDecision
    }

    val hasNoDecision: Boolean get() = decision == null
}
