import Foundation

// Clinical consent engine — CLAUDE.md §6.
// Use case library maps clinician roles to minimum necessary UHDR elements.
// Users see plain-language use cases and consent decisions; they never select raw data elements.

// MARK: - Use case library

/// One plain-language thing a clinician may do with a grant, and the UHDR elements doing it
/// needs. §6.1's key principle is that this is the unit the user consents to — the elements are
/// derived from it, never picked directly.
///
/// The library carries **keys, not text** (CLAUDE.md §17): the same table is read by Android's
/// pure-JVM `:core` module, which cannot reference `R.string` at all, so English in the table
/// would be English no platform could translate. Each platform resolves at the point of render.
struct ClinicalUseCase: Identifiable {
    var id: String
    /// `CLINICIAN_USECASE_<ID>_NAME` — resolve with `title`, never render this.
    var titleKey: String
    /// `CLINICIAN_USECASE_<ID>_DESC` — resolve with `useCaseDescription`.
    var descriptionKey: String
    var requiredElements: Set<UHDRElement>
    /// The lowest tier that may select this use case. A use case is offered only at or above it;
    /// the tier remains the ceiling, and the use cases choose within it.
    var tier: ClinicianUseCaseTier

    /// `stringLiteral:` is the only initialiser `String.LocalizationValue` offers for a value
    /// that is not written as a literal — the key here comes from the table, by design.
    var title: String { String(localized: String.LocalizationValue(stringLiteral: titleKey)) }
    var useCaseDescription: String {
        String(localized: String.LocalizationValue(stringLiteral: descriptionKey))
    }
}

enum ConsentEngine {

    // All available clinical use cases — clinicians select from this library.
    // Must stay identical to Android's `ConsentEngine.useCaseLibrary`: a grant records the IDs it
    // was made from, and the two platforms read each other's grants.
    static let useCaseLibrary: [ClinicalUseCase] = [
        ClinicalUseCase(
            id: "adherence_monitoring",
            titleKey: "CLINICIAN_USECASE_ADHERENCE_MONITORING_NAME",
            descriptionKey: "CLINICIAN_USECASE_ADHERENCE_MONITORING_DESC",
            requiredElements: [.sessionTimestamps, .sessionDuration, .protocolParameters],
            tier: .monitor
        ),
        ClinicalUseCase(
            id: "eeg_review",
            titleKey: "CLINICIAN_USECASE_EEG_REVIEW_NAME",
            descriptionKey: "CLINICIAN_USECASE_EEG_REVIEW_DESC",
            requiredElements: [.eegWaveforms, .neurofeedbackScores, .pbmDoseLogs,
                               .sessionTimestamps, .sessionDuration, .protocolParameters],
            tier: .assess
        ),
        ClinicalUseCase(
            id: "hrv_outcomes",
            titleKey: "CLINICIAN_USECASE_HRV_OUTCOMES_NAME",
            descriptionKey: "CLINICIAN_USECASE_HRV_OUTCOMES_DESC",
            requiredElements: [.eegWaveforms, .neurofeedbackScores, .pbmDoseLogs,
                               .hrvTimeSeries, .ppgOpticalSignal, .closedLoopEvents,
                               .outcomeLogs, .sessionTimestamps, .sessionDuration, .protocolParameters],
            tier: .fullClinical
        ),
    ]

    // Determine minimum necessary UHDR elements for a set of selected use cases.
    static func minimumNecessaryElements(for selectedUseCaseIDs: Set<String>) -> Set<UHDRElement> {
        useCaseLibrary
            .filter { selectedUseCaseIDs.contains($0.id) }
            .reduce(into: Set<UHDRElement>()) { $0.formUnion($1.requiredElements) }
    }

    /// The use cases a grant at `tier` may be built from: those whose own tier it reaches.
    ///
    /// The tier is the ceiling and the subscription — it is what the clinician pays for — and the
    /// use cases choose within it. Offering one the tier cannot carry would let the user approve
    /// access the grant then silently clamps away, which is the honesty defect `OI-CONSENT-04` was
    /// raised about, inverted.
    ///
    /// `.research` is deliberately empty rather than everything: its elements are IRB-defined per
    /// study descriptor (§6.3), not derivable from a library, and the same emptiness
    /// `accessDifferential` refuses to read as a set must not be read as one here either.
    static func useCases(availableFor tier: ClinicianUseCaseTier) -> [ClinicalUseCase] {
        guard tier != .research else { return [] }
        return useCaseLibrary.filter { $0.tier.rank <= tier.rank }
    }

    /// The initial grant's access decision (`OI-CONSENT-04` + `OI-CONSENT-06`).
    ///
    /// Two things are load-bearing about deriving it here rather than at the point of storage.
    ///
    /// **The elements are frozen into the grant, not recomputed from the IDs.** A grant records
    /// `useCaseIDs` because §6.1 makes the use case the thing the user consented to, but what the
    /// clinician may see is the element set as it stood that day. Re-deriving it on read would
    /// mean editing `useCaseLibrary` — adding an element to an existing use case, say — widens
    /// every grant already made, retroactively, with nobody asked.
    ///
    /// **The result is clamped to the tier.** `useCases(availableFor:)` already filters, so the
    /// intersection is a no-op on any selection the UI can produce; it is here so a caller that
    /// passes an unfiltered set cannot exceed the ceiling the subscription was sold at.
    static func initialAccessScope(
        useCaseIDs: Set<String>,
        tier: ClinicianUseCaseTier,
        grantedAt: Date,
        includesPriorData: Bool
    ) -> ClinicianAccessScope {
        let derived = minimumNecessaryElements(for: useCaseIDs)
        return ClinicianAccessScope(
            elements: tier == .research ? [] : derived.intersection(tier.uhdrElements),
            effectiveFrom: grantedAt,
            includesPriorData: includesPriorData
        )
    }

    /// The consent document for an **initial** grant — §6.1's "plain-language document stating
    /// what the clinician CAN and CANNOT learn", built from the use cases the user picked.
    ///
    /// The element sets are clamped to the tier for the reason `initialAccessScope` clamps: what
    /// this document promises and what the grant then stores must be the same set, or the document
    /// is describing a grant that will not be made.
    static func consentDocument(
        clinicianName: String,
        organization: String,
        selectedUseCaseIDs: Set<String>,
        tier: ClinicianUseCaseTier
    ) -> ConsentDocument {
        let selectedCases = useCaseLibrary.filter { selectedUseCaseIDs.contains($0.id) }
        let elements = initialAccessScope(
            useCaseIDs: selectedUseCaseIDs,
            tier: tier,
            grantedAt: Date(),
            includesPriorData: false
        ).elements
        let cannotSee = UHDRElement.allCases.filter { !elements.contains($0) }

        return ConsentDocument(
            clinicianName: clinicianName,
            organization: organization,
            tier: tier,
            approvedUseCases: selectedCases,
            approvedElements: elements,
            cannotAccessElements: Set(cannotSee),
            generatedAt: Date()
        )
    }

    // Plain-language irreversibility notice — displayed at L3 blanket consent and per-study invitations.
    static var irreversibilityNotice: String { String(localized: "CONSENT_IRREVERSIBILITY_NOTICE") }

    /// The §6.1 **differential consent document** for widening an existing grant: what the change
    /// would newly show, what it already shows, and what stays out of reach either way.
    ///
    /// Returns `nil` when the change is not an expansion, and the caller must treat that as a
    /// refusal rather than a formatting problem. Three cases are not expansions:
    ///
    ///  - **Nothing new.** The target tier adds no element the grant does not already reach, so
    ///    there is nothing to consent to.
    ///  - **Something lost.** The target tier does not contain everything the grant already
    ///    reaches. That is a re-scope, not a widening, and describing it with an expansion
    ///    document would tell the user only about what they are gaining.
    ///  - **Either side is `.research`.** `ClinicianUseCaseTier.research.uhdrElements` is empty
    ///    because a research tier's elements are IRB-defined per study descriptor, not derivable
    ///    from the tier. Treating that emptiness as a set would describe a research grant as
    ///    seeing nothing and a move away from one as adding everything; both are wrong. Research
    ///    access changes go through the per-study consent path (§6.3), not this one.
    ///
    /// The document carries no prose of its own: the elements are the substance, and the wording
    /// around them belongs to whichever platform is rendering it (CLAUDE.md §17).
    static func accessDifferential(
        for grant: ClinicianConsentGrant,
        expandingTo newTier: ClinicianUseCaseTier
    ) -> ClinicianAccessDifferential? {
        guard grant.tier != .research, newTier != .research else { return nil }

        let current = grant.approvedElements
        let target = newTier.uhdrElements
        guard current.isSubset(of: target) else { return nil }

        let newlyVisible = target.subtracting(current)
        guard !newlyVisible.isEmpty else { return nil }

        return ClinicianAccessDifferential(
            grantID: grant.id,
            clinicianName: grant.clinicianName,
            organization: grant.clinicianOrganization,
            fromTier: grant.tier,
            toTier: newTier,
            newlyVisibleElements: newlyVisible,
            alreadyVisibleElements: current,
            stillNotAccessibleElements: Set(UHDRElement.allCases).subtracting(target)
        )
    }

    // MARK: - Study descriptor ingestion (§6.3 per-project workflow)

    /// §5.3's anonymisation floors. They are locked, and the device is the only place they can
    /// be enforced, because §5.3 puts the anonymisation on the device: a descriptor that asks
    /// for weaker anonymisation than this is not one a user may be asked to consent to.
    static let minimumKAnonymity = 10
    static let minimumDateRoundingDays = 7

    /// The **ingestion gate**: whether a signed study descriptor may become something the user
    /// sees, and if so which of §6.2's two postures it arrives in.
    ///
    /// Pure, and deliberately separate from the store: the store holds the state, this holds the
    /// policy, in the same way `accessDifferential` holds §6.1's.
    ///
    /// **Order is load-bearing.** The signature is checked first, so nothing a descriptor
    /// *claims* — its categories, its element list, its title — can influence any later step of
    /// an unverified descriptor. Then §5.3's floors, which are properties of the study itself.
    /// Only then the user's consent state, which is the part that varies per device.
    ///
    /// - Parameters:
    ///   - verification: the result of checking the descriptor's signature.
    ///   - consent: the user's current research consent (L1–L4).
    ///   - studyAlreadyDecided: whether this study ID has already been answered, joined or
    ///     withdrawn from on this device.
    static func admit(
        descriptor: StudyDescriptor,
        verification: StudyDescriptorVerification,
        consent: ResearchConsentState,
        studyAlreadyDecided: Bool
    ) -> StudyDescriptorAdmission {
        let descriptorHash: String
        switch verification {
        case .unavailable: return .refused(.verifierUnavailable)
        case .rejected:    return .refused(.signatureInvalid)
        case let .verified(hash): descriptorHash = hash
        }

        guard descriptor.kAnonymity >= minimumKAnonymity,
              descriptor.dateRoundingDays >= minimumDateRoundingDays
        else { return .refused(.anonymisationBelowFloor) }

        // §6.0: withdrawing blanket consent "stops ALL research data flows" — all of them, not
        // only the pre-approved ones. Checked before `hasAnyResearchConsent`, because a withdrawn
        // user often still *has* consent by that test: withdrawal does not un-tick the nine L2
        // categories, and those stale checkboxes would otherwise keep admitting studies.
        guard !consent.blanketConsentWithdrawn else { return .refused(.researchConsentWithdrawn) }

        guard consent.hasAnyResearchConsent else { return .refused(.noResearchConsent) }

        // L1 is the shared precondition for all three delivery paths — per-study invitations,
        // per-study engagement notifications and results notifications (§6.2.1) — so it gates
        // both postures, not just the one that asks a question.
        guard consent.contactConsentGranted else { return .refused(.noContactConsent) }

        guard !studyAlreadyDecided else { return .refused(.studyAlreadyDecided) }

        // L3 is posture, L2 is scope (§6.2.2). Blanket consent means this study is pre-approved
        // and the user is told rather than asked; without it, at least one of the study's
        // categories must be one the user opted into.
        if consent.blanketConsentGranted {
            return .admitted(posture: .engagementNotification, descriptorHash: descriptorHash)
        }
        let consentedCategory = descriptor.researchCategories.contains { consent.categoryConsents[$0] == true }
        guard consentedCategory else { return .refused(.categoryNotConsented) }
        return .admitted(posture: .consentRequest, descriptorHash: descriptorHash)
    }

    /// Build the invitation the user reads from a verified descriptor.
    ///
    /// Everything the consent surface needs beyond the study's own facts is derived here or at
    /// render time: `cannotLearn` is the complement of the approved elements, and the
    /// irreversibility notice §6.3 step 4 requires comes from `irreversibilityNotice` — a locale
    /// key — rather than travelling with the descriptor as prose.
    static func invitation(
        from descriptor: StudyDescriptor,
        posture: StudyInvitation.Posture,
        descriptorHash: String,
        receivedAt: Date
    ) -> StudyInvitation {
        StudyInvitation(
            id: UUID(),
            studyID: descriptor.studyID,
            studyTitle: descriptor.studyTitle,
            researchCategories: descriptor.researchCategories,
            approvedElements: descriptor.requestedElements,
            descriptorHash: descriptorHash,
            kAnonymity: descriptor.kAnonymity,
            dateRoundingDays: descriptor.dateRoundingDays,
            posture: posture,
            receivedAt: receivedAt,
            decision: nil
        )
    }
}

// MARK: - Differential consent document (§6.1)

/// What a proposed expansion would change, element by element. The retroactive decision applies
/// to `newlyVisibleElements` only: the elements already in the grant keep whatever history
/// posture they were granted under, and re-asking about them would invite the user to widen
/// access they were not asked about.
struct ClinicianAccessDifferential {
    var grantID: UUID
    var clinicianName: String
    var organization: String
    var fromTier: ClinicianUseCaseTier
    var toTier: ClinicianUseCaseTier
    var newlyVisibleElements: Set<UHDRElement>
    var alreadyVisibleElements: Set<UHDRElement>
    var stillNotAccessibleElements: Set<UHDRElement>
}

// MARK: - Consent document

/// What an initial grant would let a clinician see, and what it would not — §6.1's per-element
/// plain-language document, for the grant the expansion differential's counterpart covers.
///
/// It carries **no prose of its own.** It used to: a `plainLanguageSummary` assembled an English
/// paragraph from the element `rawValue`s, which are the persisted Codable values rather than
/// display text, and which CLAUDE.md §17 forbids a source file from writing at all. The elements
/// are the substance; the wording around them belongs to whichever platform renders it — the same
/// rule `ClinicianAccessDifferential` was built under.
struct ConsentDocument {
    var clinicianName: String
    var organization: String
    var tier: ClinicianUseCaseTier
    var approvedUseCases: [ClinicalUseCase]
    var approvedElements: Set<UHDRElement>
    var cannotAccessElements: Set<UHDRElement>
    var generatedAt: Date
}
