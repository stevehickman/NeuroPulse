package life.neurone.core.consent

import life.neurone.core.models.ClinicianAccessScope
import life.neurone.core.models.ClinicianConsentGrant
import life.neurone.core.models.ClinicianAccessExpansionRequest
import life.neurone.core.models.ClinicianUseCaseTier
import life.neurone.core.models.ResearchCategory
import life.neurone.core.models.ResearchConsentState
import life.neurone.core.models.StudyDescriptor
import life.neurone.core.models.StudyDescriptorAdmission
import life.neurone.core.models.StudyDescriptorVerification
import life.neurone.core.models.StudyInvitation
import life.neurone.core.models.UHDRElement
import life.neurone.core.protocol.ConsentLimits

/**
 * One plain-language thing a clinician may do with a grant, and the UHDR elements doing it needs.
 * §6.1's key principle is that this is the unit the user consents to — the elements are derived
 * from it, never picked directly.
 *
 * The library carries **keys, not text** (CLAUDE.md §17). `:core` is a pure-JVM module by design
 * (ISC-2..4) and cannot reference `R.string` at all, so English here would be English no platform
 * could translate; the `:app` UI resolves each key at the point of render, and iOS resolves the
 * same keys through `String(localized:)`.
 */
data class ClinicalUseCase(
    val id: String,
    /** `CLINICIAN_USECASE_<ID>_NAME` — resolve at render, never display this. */
    val titleKey: String,
    /** `CLINICIAN_USECASE_<ID>_DESC` — resolve at render. */
    val descriptionKey: String,
    val requiredElements: Set<UHDRElement>,
    /**
     * The lowest tier that may select this use case. A use case is offered only at or above it;
     * the tier remains the ceiling, and the use cases choose within it.
     */
    val tier: ClinicianUseCaseTier,
)

/**
 * Clinical consent engine — CLAUDE.md §6.1. Peer of iOS `Consent/ConsentEngine.swift`.
 *
 * The use-case library is ported as of `OI-CONSENT-04`. It deliberately was not before: iOS
 * discarded its picker's selection when it built the grant, so porting the library would have
 * ported a control that decides nothing, and the Android form showed the tier's derived element
 * lists instead. Now that the selection decides the grant's elements on both platforms, the
 * library has to be the same table on both — a grant records the use-case IDs it was made from,
 * and the two platforms read each other's grants.
 */
object ConsentEngine {

    /** Minimum necessary UHDR elements for a set of selected use cases (§6.1). */
    fun minimumNecessaryElements(selectedUseCaseIds: Set<String>): Set<UHDRElement> =
        USE_CASE_LIBRARY
            .filter { it.id in selectedUseCaseIds }
            .flatMap { it.requiredElements }
            .toSet()

    /**
     * The use cases a grant at [tier] may be built from: those whose own tier it reaches.
     *
     * The tier is the ceiling and the subscription — it is what the clinician pays for — and the
     * use cases choose within it. Offering one the tier cannot carry would let the user approve
     * access the grant then silently clamps away, which is the honesty defect `OI-CONSENT-04` was
     * raised about, inverted.
     *
     * [ClinicianUseCaseTier.RESEARCH] is deliberately empty rather than everything: its elements
     * are IRB-defined per study descriptor (§6.3), not derivable from a library, and the same
     * emptiness [accessDifferential] refuses to read as a set must not be read as one here either.
     */
    fun useCasesAvailableFor(tier: ClinicianUseCaseTier): List<ClinicalUseCase> =
        if (tier == ClinicianUseCaseTier.RESEARCH) {
            emptyList()
        } else {
            USE_CASE_LIBRARY.filter { it.tier.rank <= tier.rank }
        }

    /**
     * The initial grant's access decision (`OI-CONSENT-04` + `OI-CONSENT-06`).
     *
     * Two things are load-bearing about deriving it here rather than at the point of storage.
     *
     * **The elements are frozen into the grant, not recomputed from the IDs.** A grant records its
     * use-case IDs because §6.1 makes the use case the thing the user consented to, but what the
     * clinician may see is the element set as it stood that day. Re-deriving it on read would mean
     * editing [USE_CASE_LIBRARY] — adding an element to an existing use case, say — widens every
     * grant already made, retroactively, with nobody asked.
     *
     * **The result is clamped to the tier.** [useCasesAvailableFor] already filters, so the
     * intersection is a no-op on any selection the UI can produce; it is here so a caller that
     * passes an unfiltered set cannot exceed the ceiling the subscription was sold at.
     */
    fun initialAccessScope(
        selectedUseCaseIds: Set<String>,
        tier: ClinicianUseCaseTier,
        grantedAtDay: String,
        includesPriorData: Boolean,
    ): ClinicianAccessScope = ClinicianAccessScope(
        elements = if (tier == ClinicianUseCaseTier.RESEARCH) {
            emptySet()
        } else {
            minimumNecessaryElements(selectedUseCaseIds) intersect tier.uhdrElements
        },
        effectiveFromDay = grantedAtDay,
        includesPriorData = includesPriorData,
    )

    /**
     * The §6.1 **differential consent document** for widening an existing grant: what the change
     * would newly show, what it already shows, and what stays out of reach either way.
     *
     * **An expansion is derived from use cases, exactly as a grant is** (`REQ-CONSENT-15` (f),
     * `OI-PRIV-64`). What it would add is what the request's use cases need, clamped to the
     * request's tier, less what the grant already reaches. The tier is the ceiling and the
     * subscription; it never decides the elements. This used to grant `toTier.uhdrElements` less
     * what was approved, so widening a grant built from one use case handed over the whole new
     * tier, and a request for the grant's own tier read as a widening to the rest of it.
     *
     * Returns null when the change is not an expansion, and the caller must treat that as a
     * refusal rather than a formatting problem. These cases are not expansions:
     *
     *  - **The grant is not in force on [today]** (`REQ-CONSENT-18`). An expired, revoked or
     *    undated grant reaches nothing, so there is nothing to widen; the clinician needs a new
     *    grant, which is asked for whole.
     *  - **No use case, or one the tier cannot carry.** A request that names no use case asks for
     *    no element (§6.1: clinicians select use cases, never data elements). A use case above
     *    [ClinicianAccessExpansionRequest.toTier] is refused, not clamped, because the person would
     *    be shown a use case whose elements the tier then withholds.
     *  - **Nothing new.** The use cases add no element the grant does not already reach.
     *  - **Something lost.** The target tier ranks below the grant's, or does not contain
     *    everything the grant already reaches. That is a re-scope, not a widening, and describing
     *    it with an expansion document would tell the user only about what they are gaining.
     *  - **Either side is [ClinicianUseCaseTier.RESEARCH].** Its `uhdrElements` is empty because a
     *    research tier's elements are IRB-defined per study descriptor, not derivable from the
     *    tier. Research access changes go through the per-study consent path (§6.3), not this one.
     *
     * The document carries no prose of its own: the elements are the substance, and the wording
     * around them belongs to whichever platform is rendering it (CLAUDE.md §17).
     */
    fun accessDifferential(
        grant: ClinicianConsentGrant,
        request: ClinicianAccessExpansionRequest,
        today: String,
    ): ClinicianAccessDifferential? {
        if (!grant.isInForce(today)) return null
        val newTier = request.toTier
        if (grant.tier == ClinicianUseCaseTier.RESEARCH ||
            newTier == ClinicianUseCaseTier.RESEARCH
        ) {
            return null
        }
        if (newTier.rank < grant.tier.rank) return null

        val useCaseIds = request.useCaseIds.orEmpty().toSet()
        if (useCaseIds.isEmpty()) return null
        val offered = useCasesAvailableFor(newTier).map { it.id }.toSet()
        if (!offered.containsAll(useCaseIds)) return null

        val current = grant.approvedElements
        val target = newTier.uhdrElements
        if (!target.containsAll(current)) return null

        val newlyVisible = (minimumNecessaryElements(useCaseIds) intersect target) - current
        if (newlyVisible.isEmpty()) return null

        return ClinicianAccessDifferential(
            grantId = grant.id,
            clinicianName = grant.clinicianName,
            organization = grant.clinicianOrganization,
            fromTier = grant.tier,
            toTier = newTier,
            useCaseIds = useCaseIds,
            newlyVisibleElements = newlyVisible,
            alreadyVisibleElements = current,
            stillNotAccessibleElements = UHDRElement.entries.toSet() - current - newlyVisible,
        )
    }

    /**
     * Whether [grant] may be recorded as a new grant (`REQ-CONSENT-15` (g), `OI-PRIV-64`): it is built from use cases the
     * tier offers, and every scope it carries reaches only what those use cases need within the tier. The research tier
     * carries no use case and no element (`REQ-CONSENT-15` (c)). A grant made in code without a scope, or with an
     * element no selected use case needs, is refused, because the element list would then be the clinician's selection,
     * not the person's use cases.
     */
    fun isWellFormedNewGrant(grant: ClinicianConsentGrant): Boolean {
        if (!grant.isActive || grant.revokedOnDay != null) return false
        val ids = grant.useCaseIds?.toSet() ?: return false
        val scopes = grant.accessScopes ?: return false
        if (scopes.isEmpty()) return false
        val offered = useCasesAvailableFor(grant.tier).map { it.id }.toSet()
        if (grant.tier != ClinicianUseCaseTier.RESEARCH && ids.isEmpty()) return false
        if (!offered.containsAll(ids)) return false
        val allowed = if (grant.tier == ClinicianUseCaseTier.RESEARCH) {
            emptySet()
        } else {
            minimumNecessaryElements(ids) intersect grant.tier.uhdrElements
        }
        return scopes.all { allowed.containsAll(it.elements) && isIsoDay(it.effectiveFromDay) }
    }

    // ── Study descriptor ingestion (§6.3 per-project workflow) ───────────

    // §5.3's anonymisation floors are `ConsentLimits` (common/npps/constants.json), generated for every runtime.

    /**
     * UHDR elements no research extract may carry, whatever `k` a descriptor declares.
     *
     * §5.3 promises a k-anonymous extract. k-anonymity groups records that share their quasi-identifiers, and a
     * recorded signal is one the person alone has: there is no group of ten to put it in without ceasing to be
     * the signal. A study that asks for one is asking for something the device cannot anonymise, so the person
     * is not asked about it (`REQ-CONSENT-09` h).
     *
     * **The three recorded signals are handled alike** (the principal, 2026-11-18: a person expects EEG, PPG and
     * HRV data to be treated the same way). The argument above was made for EEG waveforms and holds for the PPG
     * optical signal and the HRV time series, so all three are here. A signal element added later joins them.
     */
    /** [day] is a real calendar day written "yyyy-MM-dd", the only form that orders as text. */
    fun isIsoDay(day: String): Boolean =
        runCatching { kotlinx.datetime.LocalDate.parse(day).toString() == day }.getOrDefault(false)

    /**
     * Whether a descriptor or invitation that expires on [expiresOnDay] may still be acted on [today].
     * The expiry day is the last day. No expiry (an invitation stored before descriptors carried one)
     * does not bar it.
     */
    fun notExpired(expiresOnDay: String?, today: String): Boolean = expiresOnDay == null || today <= expiresOnDay

    val ELEMENTS_NO_EXTRACT_MAY_CARRY: Set<UHDRElement> = setOf(
        UHDRElement.EEG_WAVEFORMS, UHDRElement.PPG_OPTICAL_SIGNAL, UHDRElement.HRV_TIME_SERIES,
    )

    /**
     * Whether [consent] covers a study in [categories]: blanket consent is on, or one of the study's categories
     * is consented, and blanket consent has not been withdrawn (which outranks the L2 ticks, §6.0).
     *
     * One rule for the three places it is needed: a participation stays active only while this holds
     * (`REQ-CONSENT-19`), a consent request can be accepted only while it holds, and the dashboard offers only
     * the invitations for which it holds. [categories] is null when the study's categories are not on record,
     * and then only blanket consent covers it.
     */
    fun covers(consent: ResearchConsentState, categories: List<ResearchCategory>?): Boolean =
        !consent.blanketConsentWithdrawn &&
            (consent.blanketConsentGranted || categories.orEmpty().any { consent.categoryConsents[it] == true })

    /**
     * The **ingestion gate**: whether a signed study descriptor may become something the user
     * sees, and if so which of §6.2's two postures it arrives in.
     *
     * Pure, and deliberately separate from the store: the store holds the state, this holds the
     * policy, in the same way [accessDifferential] holds §6.1's.
     *
     * **Order is load-bearing.** The signature is checked first, so nothing a descriptor *claims*
     * — its categories, its element list, its title — can influence any later step of an
     * unverified descriptor. Then §5.3's floors and what the study asks for, which are properties
     * of the study itself. Only then the user's consent state, which is the part that varies per
     * device, and first of all whether that state could be read.
     *
     * **Suppression is not a descriptor field** (the principal, 2026-11-19; `OI-PRIV-06` point 5).
     * The rule is the device's and is the same for every study: a record whose group is smaller
     * than `k` is dropped, never sent (`REQ-CONSENT-13` e, with the extractor, `OI-CONSENT-07`).
     * So there is nothing about suppression for this gate to check.
     *
     * @param verification the result of checking the descriptor's signature.
     * @param consent the user's current research consent (L1–L4).
     * @param studyAlreadyDecided whether this study ID has already been answered, joined or
     *   withdrawn from on this device.
     * @param today the day the descriptor is being admitted on ("yyyy-MM-dd"), for its expiry.
     * @param consentRecordReadable false while a stored consent record could not be read and the
     *   person has not decided again (`REQ-CONSENT-20`).
     */
    fun admit(
        descriptor: StudyDescriptor,
        verification: StudyDescriptorVerification,
        consent: ResearchConsentState,
        studyAlreadyDecided: Boolean,
        consentRecordReadable: Boolean,
        today: String,
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

        if (descriptor.kAnonymity < ConsentLimits.MINIMUM_K_ANONYMITY ||
            descriptor.dateRoundingDays < ConsentLimits.MINIMUM_DATE_ROUNDING_DAYS
        ) {
            return StudyDescriptorAdmission.Refused(
                StudyDescriptorAdmission.Reason.ANONYMISATION_BELOW_FLOOR,
            )
        }

        // The issuer signs how long the descriptor stands. An expiry that is not a day, or that
        // precedes the issue day, is not one the device can hold the issuer to.
        if (!isIsoDay(descriptor.expiresOnDay) || descriptor.expiresOnDay < descriptor.issuedOnDay ||
            !notExpired(descriptor.expiresOnDay, today)
        ) {
            return StudyDescriptorAdmission.Refused(StudyDescriptorAdmission.Reason.DESCRIPTOR_EXPIRED)
        }

        // An element that cannot be made k-anonymous makes the declared k meaningless for it.
        if (descriptor.requestedElements.any { it in ELEMENTS_NO_EXTRACT_MAY_CARRY }) {
            return StudyDescriptorAdmission.Refused(
                StudyDescriptorAdmission.Reason.REQUESTS_ELEMENT_NO_EXTRACT_MAY_CARRY,
            )
        }

        // An unreadable record reads as defaults, which is "never asked". The person may have said
        // stop, so until they decide again nothing is admitted, and the refusal says why.
        if (!consentRecordReadable) {
            return StudyDescriptorAdmission.Refused(
                StudyDescriptorAdmission.Reason.CONSENT_RECORD_UNREADABLE,
            )
        }

        // §6.0: withdrawing blanket consent "stops ALL research data flows" — all of them, not
        // only the pre-approved ones. Checked before hasAnyResearchConsent, because a withdrawn
        // user often still *has* consent by that test: withdrawal does not un-tick the nine L2
        // categories, and those stale checkboxes would otherwise keep admitting studies.
        if (consent.blanketConsentWithdrawn) {
            return StudyDescriptorAdmission.Refused(
                StudyDescriptorAdmission.Reason.RESEARCH_CONSENT_WITHDRAWN,
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
        expiresOnDay = descriptor.expiresOnDay,
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
    /** The use cases the expansion is derived from, the person's terms for what it adds (§6.1). */
    val useCaseIds: Set<String>,
    val newlyVisibleElements: Set<UHDRElement>,
    val alreadyVisibleElements: Set<UHDRElement>,
    val stillNotAccessibleElements: Set<UHDRElement>,
)
