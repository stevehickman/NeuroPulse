// TODO(localization): strings below should use NSLocalizedString — see en.lproj/Localizable.strings
import Foundation

// Clinical consent engine — CLAUDE.md §6.
// Use case library maps clinician roles to minimum necessary UHDR elements.
// Users see plain-language use cases and consent decisions; they never select raw data elements.

// MARK: - Use case library

struct ClinicalUseCase: Identifiable {
    var id: String
    var title: String
    var description: String
    var requiredElements: Set<UHDRElement>
    var tier: ClinicianUseCaseTier
}

enum ConsentEngine {

    // All available clinical use cases — clinicians select from this library.
    static let useCaseLibrary: [ClinicalUseCase] = [
        ClinicalUseCase(
            id: "adherence_monitoring",
            title: "Adherence Monitoring",
            description: "Your clinician can see when you completed sessions and which protocols"
                + " you used. They cannot see your brainwave recordings or physiological signals.",
            requiredElements: [.sessionTimestamps, .sessionDuration, .protocolParameters],
            tier: .monitor
        ),
        ClinicalUseCase(
            id: "eeg_review",
            title: "EEG Review",
            description: "Your clinician can review your brainwave recordings from sessions to"
                + " assess neurofeedback performance and protocol effectiveness.",
            requiredElements: [.eegWaveforms, .neurofeedbackScores, .pbmDoseLogs,
                               .sessionTimestamps, .sessionDuration, .protocolParameters],
            tier: .assess
        ),
        ClinicalUseCase(
            id: "hrv_outcomes",
            title: "HRV + Outcome Tracking",
            description: "Your clinician can see your heart rate variability, breathing"
                + " coherence scores, and any outcomes you've logged. Includes all EEG access.",
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

    // Generate a plain-language consent document for the selected use cases.
    static func consentDocument(
        clinicianName: String,
        organization: String,
        selectedUseCaseIDs: Set<String>,
        tier: ClinicianUseCaseTier
    ) -> ConsentDocument {
        let selectedCases = useCaseLibrary.filter { selectedUseCaseIDs.contains($0.id) }
        let elements = minimumNecessaryElements(for: selectedUseCaseIDs)
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

struct ConsentDocument {
    var clinicianName: String
    var organization: String
    var tier: ClinicianUseCaseTier
    var approvedUseCases: [ClinicalUseCase]
    var approvedElements: Set<UHDRElement>
    var cannotAccessElements: Set<UHDRElement>
    var generatedAt: Date

    var plainLanguageSummary: String {
        let canSee = approvedElements.map(\.rawValue).sorted().joined(separator: "\n• ")
        let cannot = cannotAccessElements.map(\.rawValue).sorted().joined(separator: "\n• ")
        return """
        \(clinicianName) at \(organization) is requesting access to monitor your NeurOne sessions.

        WHAT THEY CAN SEE:
        • \(canSee.isEmpty ? "Nothing (no consent selected)" : canSee)

        WHAT THEY CANNOT SEE:
        • \(cannot.isEmpty ? "All elements approved" : cannot)

        Pricing tier: \(tier.monthlyPrice)
        You can revoke this access at any time from the Consent tab.
        """
    }
}
