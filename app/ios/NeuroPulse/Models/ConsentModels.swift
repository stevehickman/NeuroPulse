import Foundation

// Clinical consent engine types — CLAUDE.md §6.
// Use cases map to minimum necessary UHDR elements; users select use cases, not data elements.

// MARK: - Clinician use case tiers

enum ClinicianUseCaseTier: String, CaseIterable, Codable {
    case monitor     = "Monitor"        // $49/mo — adherence, protocol compliance
    case assess      = "Assess"         // $149/mo — + EEG review, neurofeedback, efficacy
    case fullClinical = "Full Clinical" // $299/mo — + HRV, closed-loop events, outcomes
    case research    = "Research"       // $599/mo/study — IRB-defined custom

    var monthlyPrice: String {
        switch self {
        case .monitor:      return "$49/month/patient"
        case .assess:       return "$149/month/patient"
        case .fullClinical: return "$299/month/patient"
        case .research:     return "$599/month/study"
        }
    }

    // Minimum necessary UHDR elements for this tier
    var uhdrElements: Set<UHDRElement> {
        switch self {
        case .monitor:
            return [.sessionTimestamps, .sessionDuration, .protocolParameters]
        case .assess:
            return [.sessionTimestamps, .sessionDuration, .protocolParameters,
                    .eegWaveforms, .neurofeedbackScores, .pbmDoseLogs]
        case .fullClinical:
            return [.sessionTimestamps, .sessionDuration, .protocolParameters,
                    .eegWaveforms, .neurofeedbackScores, .pbmDoseLogs,
                    .hrvTimeSeries, .ppgOpticalSignal, .closedLoopEvents, .outcomeLogs]
        case .research:
            return []  // IRB-defined; populated per study descriptor
        }
    }
}

// MARK: - UHDR data elements (per NP-FW-EMMC-001 §12)

enum UHDRElement: String, CaseIterable, Codable {
    case eegWaveforms        = "EEG Waveforms"
    case hrvTimeSeries       = "HRV Time Series"
    case ppgOpticalSignal    = "PPG Optical Signal"
    case neurofeedbackScores = "Neurofeedback Performance Scores"
    case sessionTimestamps   = "Session Timestamps"
    case sessionDuration     = "Session Duration"
    case protocolParameters  = "Protocol Parameters"
    case closedLoopEvents    = "Closed-Loop Adaptation Events"
    case pbmDoseLogs         = "PBM Dose (J/cm²) Per Zone"
    case outcomeLogs         = "User-Entered Outcome Logs"
    case eyeStateLogs        = "Eye Open/Closed State"

    // Lowest clinician tier that may access this element (ISC-137).
    var minimumTier: ClinicianUseCaseTier {
        switch self {
        case .sessionTimestamps, .sessionDuration, .protocolParameters:
            return .monitor
        case .eegWaveforms, .neurofeedbackScores, .pbmDoseLogs:
            return .assess
        case .hrvTimeSeries, .ppgOpticalSignal, .closedLoopEvents, .outcomeLogs, .eyeStateLogs:
            return .fullClinical
        }
    }

    var plainLanguageDescription: String {
        switch self {
        case .eegWaveforms:
            return "Raw brainwave recordings from all electrodes during your sessions."
        case .hrvTimeSeries:
            return "Beat-by-beat heart rate variability measurements."
        case .ppgOpticalSignal:
            return "Pulse oximetry optical signal from the VNS clip."
        case .neurofeedbackScores:
            return "Your neurofeedback performance scores across sessions."
        case .sessionTimestamps:
            return "When you started and finished each session."
        case .sessionDuration:
            return "How long each session lasted."
        case .protocolParameters:
            return "Which stimulation protocols you used and at what settings."
        case .closedLoopEvents:
            return "Moments when your device automatically adjusted stimulation in response to your brainwaves."
        case .pbmDoseLogs:
            return "Light therapy dose delivered per zone in each session."
        case .outcomeLogs:
            return "Symptoms or outcomes you chose to log manually."
        case .eyeStateLogs:
            return "Whether your eyes were open or closed during visual stimulation sessions."
        }
    }
}

// MARK: - Clinician consent grant

struct ClinicianConsentGrant: Codable, Identifiable {
    var id: UUID
    var clinicianName: String
    var clinicianOrganization: String
    var tier: ClinicianUseCaseTier
    var grantedAt: Date
    var expiresAt: Date?         // nil = indefinite until revoked
    var isActive: Bool

    var approvedElements: Set<UHDRElement> { tier.uhdrElements }
}

// MARK: - Contact frequency (ISC-69 — three options)

enum ContactFrequency: String, CaseIterable, Codable {
    case weekly    = "Weekly"
    case monthly   = "Monthly"
    case quarterly = "Quarterly"
}

// MARK: - Research consent layers (CLAUDE.md §6.2)

struct ResearchConsentState: Codable {
    // L1: Contact consent
    var contactConsentGranted: Bool = false
    var contactMethod: String = ""
    var contactFrequency: ContactFrequency = .monthly
    var isPOAHolder: Bool = false
    var poaUploadedAt: Date? = nil
    var poaVerifiedAt: Date? = nil

    // L2: Category consent (9 research categories)
    var categoryConsents: [ResearchCategory: Bool] = {
        var d: [ResearchCategory: Bool] = [:]
        ResearchCategory.allCases.forEach { d[$0] = false }
        return d
    }()

    // L3: Blanket consent
    var blanketConsentGranted: Bool = false
    var blanketConsentGrantedAt: Date? = nil

    // L4: Results + community
    var resultsOptIn: Bool = false
    var suggestionPortalOptIn: Bool = false

    var hasAnyResearchConsent: Bool {
        contactConsentGranted || blanketConsentGranted ||
        categoryConsents.values.contains(true)
    }
}

enum ResearchCategory: String, CaseIterable, Codable {
    case alzheimersAndDementia = "Alzheimer's / Dementia"
    case depression            = "Depression"
    case ptsd                  = "PTSD"
    case tbi                   = "Traumatic Brain Injury"
    case sleep                 = "Sleep Disorders"
    case attention             = "Attention / ADHD"
    case parkinsons            = "Parkinson's Disease"
    case healthyAgeing         = "Healthy Ageing"
    case visualHealth          = "Visual Health"
}

// MARK: - Study participation record (audit trail — stored in SHDR, not UHDR)

struct StudyParticipationRecord: Codable, Identifiable {
    var id: UUID
    var studyID: String
    var descriptorHash: String   // SHA-256 of signed study descriptor
    var transmittedAt: Date
    var extractBytes: Int
    var withdrawnAt: Date?

    var isActive: Bool { withdrawnAt == nil }
}

// MARK: - Per-project consent notification

struct StudyInvitation: Identifiable {
    var id: UUID
    var studyID: String
    var studyTitle: String
    var researchCategories: [ResearchCategory]
    var approvedElements: Set<UHDRElement>
    var cannotLearn: [String]       // plain-language "what we CANNOT see"
    var irreversibilityNotice: String
    var receivedAt: Date
    var decision: StudyDecision?

    enum StudyDecision {
        case accepted(at: Date)
        case declined(at: Date)
        case questionSent(at: Date)
    }

    var hasNoDecision: Bool { decision == nil }
}
