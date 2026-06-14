// TODO(localisation): strings below should use NSLocalizedString — see en.lproj/Localizable.strings
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
            // swiftlint:disable:next line_length
            description: "Your clinician can see when you completed sessions and which protocols you used. They cannot see your brainwave recordings or physiological signals.",
            requiredElements: [.sessionTimestamps, .sessionDuration, .protocolParameters],
            tier: .monitor
        ),
        ClinicalUseCase(
            id: "eeg_review",
            title: "EEG Review",
            // swiftlint:disable:next line_length
            description: "Your clinician can review your brainwave recordings from sessions to assess neurofeedback performance and protocol effectiveness.",
            // swiftlint:disable:next line_length
            requiredElements: [.eegWaveforms, .neurofeedbackScores, .pbmDoseLogs, .sessionTimestamps, .sessionDuration, .protocolParameters],
            tier: .assess
        ),
        ClinicalUseCase(
            id: "hrv_outcomes",
            title: "HRV + Outcome Tracking",
            // swiftlint:disable:next line_length
            description: "Your clinician can see your heart rate variability, breathing coherence scores, and any outcomes you've logged. Includes all EEG access.",
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
        organisation: String,
        selectedUseCaseIDs: Set<String>,
        tier: ClinicianUseCaseTier
    ) -> ConsentDocument {
        let selectedCases = useCaseLibrary.filter { selectedUseCaseIDs.contains($0.id) }
        let elements = minimumNecessaryElements(for: selectedUseCaseIDs)
        let cannotSee = UHDRElement.allCases.filter { !elements.contains($0) }

        return ConsentDocument(
            clinicianName: clinicianName,
            organisation: organisation,
            tier: tier,
            approvedUseCases: selectedCases,
            approvedElements: elements,
            cannotAccessElements: Set(cannotSee),
            generatedAt: Date()
        )
    }

    // Plain-language irreversibility notice — displayed at L3 blanket consent and per-study invitations.
    static let irreversibilityNotice = """
    Once your anonymised data has been included in a published study, it cannot be individually \
    withdrawn from that dataset. This is a fundamental property of aggregate anonymised data \
    required by Common Rule (45 CFR 46).

    However, because NeuroPulse anonymises your data fresh from your device for each study, \
    withdrawing consent immediately and permanently stops any further data from flowing to any \
    future dataset — including data from sessions that occurred before your withdrawal.

    Your session recordings remain on your device under your sole control at all times.
    """
}

// MARK: - Consent document

struct ConsentDocument {
    var clinicianName: String
    var organisation: String
    var tier: ClinicianUseCaseTier
    var approvedUseCases: [ClinicalUseCase]
    var approvedElements: Set<UHDRElement>
    var cannotAccessElements: Set<UHDRElement>
    var generatedAt: Date

    var plainLanguageSummary: String {
        let canSee = approvedElements.map(\.rawValue).sorted().joined(separator: "\n• ")
        let cannot = cannotAccessElements.map(\.rawValue).sorted().joined(separator: "\n• ")
        return """
        \(clinicianName) at \(organisation) is requesting access to monitor your NeuroPulse sessions.

        WHAT THEY CAN SEE:
        • \(canSee.isEmpty ? "Nothing (no consent selected)" : canSee)

        WHAT THEY CANNOT SEE:
        • \(cannot.isEmpty ? "All elements approved" : cannot)

        Pricing tier: \(tier.monthlyPrice)
        You can revoke this access at any time from the Consent tab.
        """
    }
}
