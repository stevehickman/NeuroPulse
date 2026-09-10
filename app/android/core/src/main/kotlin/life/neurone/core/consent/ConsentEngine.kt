package life.neurone.core.consent

import life.neurone.core.models.ClinicianConsentGrant
import life.neurone.core.models.ClinicianUseCaseTier
import life.neurone.core.models.ResearchConsentState
import life.neurone.core.models.StudyDescriptor
import life.neurone.core.models.StudyDescriptorAdmission
import life.neurone.core.models.StudyDescriptorVerification
import life.neurone.core.models.StudyInvitation
import life.neurone.core.models.UHDRElement

/**
 * Clinical consent engine — CLAUDE.md §6.1. Peer of iOS `Consent/ConsentEngine.swift`.
 *
 * Only the differential document is ported today. iOS's use-case library is not: the Android
 * grant form derives its element lists from the tier directly, and iOS discards its use-case
 * picker's selection when it builds the grant, so porting the library would port a control that
 * decides nothing (recorded when ConsentDashboardScreen landed).
 */
object ConsentEngine {

    /**
     * The §6.1 **differential consent document** for widening an existing grant: what the change
     * would newly show, what it already shows, and what stays out of reach either way.
     *
     * Returns null when the change is not an expansion, and the caller must treat that as a
     * refusal rather than a formatting problem. Three cases are not expansions:
     *
     *  - **Nothing new.** The target tier adds no element the grant does not already reach, so
     *    there is nothing to consent to.
     *  - **Something lost.** The target tier does not contain everything the grant already
     *    reaches. That is a re-scope, not a widening, and describing it with an expansion
     *    document would tell the user only about what they are gaining.
     *  - **Either side is [ClinicianUseCaseTier.RESEARCH].** Its `uhdrElements` is empty because a
     *    research tier's elements are IRB-defined per study descriptor, not derivable from the
     *    tier. Treating that emptiness as a set would describe a research grant as seeing nothing
     *    and a move away from one as adding everything; both are wrong. Research access changes
     *    go through the per-study consent path (§6.3), not this one.
     *
     * The document carries no prose of its own: the elements are the substance, and the wording
     * around them belongs to whichever platform is rendering it (CLAUDE.md §17).
     */
    fun accessDifferential(
        grant: ClinicianConsentGrant,
        newTier: ClinicianUseCaseTier,
    ): ClinicianAccessDifferential? {
        if (grant.tier == ClinicianUseCaseTier.RESEARCH ||
            newTier == ClinicianUseCaseTier.RESEARCH
        ) {
            return null
        }

        val current = grant.approvedElements
        val target = newTier.uhdrElements
        if (!target.containsAll(current)) return null

        val newlyVisible = target - current
        if (newlyVisible.isEmpty()) return null

        return ClinicianAccessDifferential(
            grantId = grant.id,
            clinicianName = grant.clinicianName,
            organization = grant.clinicianOrganization,
            fromTier = grant.tier,
            toTier = newTier,
            newlyVisibleElements = newlyVisible,
            alreadyVisibleElements = current,
            stillNotAccessibleElements = UHDRElement.entries.toSet() - target,
        )
    }

    // ── Study descriptor ingestion (§6.3 per-project workflow) ───────────

    /**
     * §5.3's anonymisation floors. They are locked, and the device is the only place they can be
     * enforced, because §5.3 puts the anonymisation on the device: a descriptor that asks for
     * weaker anonymisation than this is not one a user may be asked to consent to.
     */
    const val MINIMUM_K_ANONYMITY = 10
    const val MINIMUM_DATE_ROUNDING_DAYS = 7

    /**
     * The **ingestion gate**: whether a signed study descriptor may become something the user
     * sees, and if so which of §6.2's two postures it arrives in.
     *
     * Pure, and deliberately separate from the store: the store holds the state, this holds the
     * policy, in the same way [accessDifferential] holds §6.1's.
     *
     * **Order is load-bearing.** The signature is checked first, so nothing a descriptor *claims*
     * — its categories, its element list, its title — can influence any later step of an
     * unverified descriptor. Then §5.3's floors, which are properties of the study itself. Only
     * then the user's consent state, which is the part that varies per device.
     *
     * @param verification the result of checking the descriptor's signature.
     * @param consent the user's current research consent (L1–L4).
     * @param studyAlreadyDecided whether this study ID has already been answered, joined or
     *   withdrawn from on this device.
     */
    fun admit(
        descriptor: StudyDescriptor,
        verification: StudyDescriptorVerification,
        consent: ResearchConsentState,
        studyAlreadyDecided: Boolean,
    ): StudyDescriptorAdmission {
        val descriptorHash = when (verification) {
            StudyDescriptorVerification.Unavailable ->
                return StudyDescriptorAdmission.Refused(
                    StudyDescriptorAdmission.Reason.VERIFIER_UNAVAILABLE,
                )
            StudyDescriptorVerification.Rejected ->
                return StudyDescriptorAdmission.Refused(
                    StudyDescriptorAdmission.Reason.SIGNATURE_INVALID,
                )
            is StudyDescriptorVerification.Verified -> verification.descriptorHash
        }

        if (descriptor.kAnonymity < MINIMUM_K_ANONYMITY ||
            descriptor.dateRoundingDays < MINIMUM_DATE_ROUNDING_DAYS
        ) {
            return StudyDescriptorAdmission.Refused(
                StudyDescriptorAdmission.Reason.ANONYMISATION_BELOW_FLOOR,
            )
        }

        if (!consent.hasAnyResearchConsent) {
            return StudyDescriptorAdmission.Refused(
                StudyDescriptorAdmission.Reason.NO_RESEARCH_CONSENT,
            )
        }

        // L1 is the shared precondition for all three delivery paths — per-study invitations,
        // per-study engagement notifications and results notifications (§6.2.1) — so it gates both
        // postures, not just the one that asks a question.
        if (!consent.contactConsentGranted) {
            return StudyDescriptorAdmission.Refused(
                StudyDescriptorAdmission.Reason.NO_CONTACT_CONSENT,
            )
        }

        if (studyAlreadyDecided) {
            return StudyDescriptorAdmission.Refused(
                StudyDescriptorAdmission.Reason.STUDY_ALREADY_DECIDED,
            )
        }

        // L3 is posture, L2 is scope (§6.2.2). Blanket consent means this study is pre-approved
        // and the user is told rather than asked; without it, at least one of the study's
        // categories must be one the user opted into.
        if (consent.blanketConsentGranted) {
            return StudyDescriptorAdmission.Admitted(
                posture = StudyInvitation.Posture.ENGAGEMENT_NOTIFICATION,
                descriptorHash = descriptorHash,
            )
        }
        val consentedCategory =
            descriptor.researchCategories.any { consent.categoryConsents[it] == true }
        if (!consentedCategory) {
            return StudyDescriptorAdmission.Refused(
                StudyDescriptorAdmission.Reason.CATEGORY_NOT_CONSENTED,
            )
        }
        return StudyDescriptorAdmission.Admitted(
            posture = StudyInvitation.Posture.CONSENT_REQUEST,
            descriptorHash = descriptorHash,
        )
    }

    /**
     * Build the invitation the user reads from a verified descriptor.
     *
     * Everything the consent surface needs beyond the study's own facts is derived here or at
     * render time: `cannotLearn` is the complement of the approved elements, and the
     * irreversibility notice §6.3 step 4 requires is a locale key the UI resolves rather than
     * prose travelling with the descriptor.
     */
    fun invitation(
        descriptor: StudyDescriptor,
        posture: StudyInvitation.Posture,
        descriptorHash: String,
        receivedOnDay: String,
    ): StudyInvitation = StudyInvitation(
        studyId = descriptor.studyId,
        studyTitle = descriptor.studyTitle,
        researchCategories = descriptor.researchCategories,
        approvedElements = descriptor.requestedElements,
        descriptorHash = descriptorHash,
        kAnonymity = descriptor.kAnonymity,
        dateRoundingDays = descriptor.dateRoundingDays,
        posture = posture,
        receivedOnDay = receivedOnDay,
        decision = null,
    )
}

/**
 * What a proposed expansion would change, element by element. The retroactive decision applies to
 * [newlyVisibleElements] only: the elements already in the grant keep whatever history posture
 * they were granted under, and re-asking about them would invite the user to widen access they
 * were not asked about.
 */
data class ClinicianAccessDifferential(
    val grantId: String,
    val clinicianName: String,
    val organization: String,
    val fromTier: ClinicianUseCaseTier,
    val toTier: ClinicianUseCaseTier,
    val newlyVisibleElements: Set<UHDRElement>,
    val alreadyVisibleElements: Set<UHDRElement>,
    val stillNotAccessibleElements: Set<UHDRElement>,
)
