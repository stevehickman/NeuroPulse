package life.neurone.core.consent

import life.neurone.core.models.ClinicianConsentGrant
import life.neurone.core.models.ClinicianUseCaseTier
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
