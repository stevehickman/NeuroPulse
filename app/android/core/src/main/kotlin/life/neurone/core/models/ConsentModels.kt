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

@Serializable
data class ClinicianConsentGrant(
    val id: String,
    val clinicianName: String,
    val clinicianOrganization: String,
    val tier: ClinicianUseCaseTier,
    val grantedAtDay: String,      // "yyyy-MM-dd" — day granularity, consistent with SessionRecord
    val expiresAtDay: String? = null, // null = indefinite until revoked
    val isActive: Boolean = true,
) {
    val approvedElements: Set<UHDRElement> get() = tier.uhdrElements
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
