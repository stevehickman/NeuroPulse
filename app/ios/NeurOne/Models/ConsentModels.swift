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

    /// Display text. `rawValue` stays the persisted Codable value: it is written
    /// into stored consent records, so it cannot double as the thing on screen.
    var displayName: String {
        switch self {
        case .eegWaveforms: return String(localized: "CONSENT_ELEMENT_EEGWAVEFORMS")
        case .hrvTimeSeries: return String(localized: "CONSENT_ELEMENT_HRVTIMESERIES")
        case .ppgOpticalSignal: return String(localized: "CONSENT_ELEMENT_PPGOPTICALSIGNAL")
        case .neurofeedbackScores: return String(localized: "CONSENT_ELEMENT_NEUROFEEDBACKSCORES")
        case .sessionTimestamps: return String(localized: "CONSENT_ELEMENT_SESSIONTIMESTAMPS")
        case .sessionDuration: return String(localized: "CONSENT_ELEMENT_SESSIONDURATION")
        case .protocolParameters: return String(localized: "CONSENT_ELEMENT_PROTOCOLPARAMETERS")
        case .closedLoopEvents: return String(localized: "CONSENT_ELEMENT_CLOSEDLOOPEVENTS")
        case .pbmDoseLogs: return String(localized: "CONSENT_ELEMENT_PBMDOSELOGS")
        case .outcomeLogs: return String(localized: "CONSENT_ELEMENT_OUTCOMELOGS")
        case .eyeStateLogs: return String(localized: "CONSENT_ELEMENT_EYESTATELOGS")
        }
    }

    var plainLanguageDescription: String {
        switch self {
        case .eegWaveforms:
            return String(localized: "CONSENT_ELEMENT_DESC_EEGWAVEFORMS")
        case .hrvTimeSeries:
            return String(localized: "CONSENT_ELEMENT_DESC_HRVTIMESERIES")
        case .ppgOpticalSignal:
            return String(localized: "CONSENT_ELEMENT_DESC_PPGOPTICALSIGNAL")
        case .neurofeedbackScores:
            return String(localized: "CONSENT_ELEMENT_DESC_NEUROFEEDBACKSCORES")
        case .sessionTimestamps:
            return String(localized: "CONSENT_ELEMENT_DESC_SESSIONTIMESTAMPS")
        case .sessionDuration:
            return String(localized: "CONSENT_ELEMENT_DESC_SESSIONDURATION")
        case .protocolParameters:
            return String(localized: "CONSENT_ELEMENT_DESC_PROTOCOLPARAMETERS")
        case .closedLoopEvents:
            return String(localized: "CONSENT_ELEMENT_DESC_CLOSEDLOOPEVENTS")
        case .pbmDoseLogs:
            return String(localized: "CONSENT_ELEMENT_DESC_PBMDOSELOGS")
        case .outcomeLogs:
            return String(localized: "CONSENT_ELEMENT_DESC_OUTCOMELOGS")
        case .eyeStateLogs:
            return String(localized: "CONSENT_ELEMENT_DESC_EYESTATELOGS")
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

    /// Display text. `rawValue` stays the persisted Codable value: it is written
    /// into stored consent records, so it cannot double as the thing on screen.
    var displayName: String {
        switch self {
        case .weekly: return String(localized: "CONSENT_FREQ_WEEKLY")
        case .monthly: return String(localized: "CONSENT_FREQ_MONTHLY")
        case .quarterly: return String(localized: "CONSENT_FREQ_QUARTERLY")
        }
    }
}

// MARK: - Research consent layers (CLAUDE.md §6.2)

/// The four consent layers. Layers are not screens: since CLAUDE.md Rev 37 they are presented
/// across two screens (S1 = L4 + L1, S2 = L2 + L3), but the layer identities are what this
/// type, the withdrawal surfaces and the document set all key on.
///
/// L2 and L3 are orthogonal and must never be collapsed into one field. L2 is **scope** (which
/// research areas); L3 is **posture** (ask-me-each-time versus pre-approved). "All nine
/// categories, but ask me about each study" is a real and distinct position, and it is
/// expressible only while both survive (§6.2.2).
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

    /// True when every research category is selected. Drives the Select-all affordance's
    /// checked state (§6.2.3). It says nothing about `blanketConsentGranted` — selecting all
    /// nine categories is L2 scope, not L3 posture.
    var allCategoriesSelected: Bool {
        ResearchCategory.allCases.allSatisfy { categoryConsents[$0] == true }
    }

    /// Set or clear all nine categories at once.
    ///
    /// Deliberately does NOT touch `blanketConsentGranted` in either direction. Ticking nine
    /// boxes expresses breadth of interest; the blanket toggle surrenders the right to be
    /// consulted, and inferring the second from the first attributes to the user a decision
    /// they did not make (§6.2.3). Coupling them here would also mean a later un-tick could
    /// trigger the research-analytics teardown that only blanket withdrawal is supposed to
    /// cause.
    mutating func setAllCategories(_ granted: Bool) {
        ResearchCategory.allCases.forEach { categoryConsents[$0] = granted }
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

    /// Display text. `rawValue` stays the persisted Codable value: it is written
    /// into stored consent records, so it cannot double as the thing on screen.
    var displayName: String {
        switch self {
        case .alzheimersAndDementia: return String(localized: "CONSENT_CATEGORY_ALZHEIMERSANDDEMENTIA")
        case .depression: return String(localized: "CONSENT_CATEGORY_DEPRESSION")
        case .ptsd: return String(localized: "CONSENT_CATEGORY_PTSD")
        case .tbi: return String(localized: "CONSENT_CATEGORY_TBI")
        case .sleep: return String(localized: "CONSENT_CATEGORY_SLEEP")
        case .attention: return String(localized: "CONSENT_CATEGORY_ATTENTION")
        case .parkinsons: return String(localized: "CONSENT_CATEGORY_PARKINSONS")
        case .healthyAgeing: return String(localized: "CONSENT_CATEGORY_HEALTHYAGEING")
        case .visualHealth: return String(localized: "CONSENT_CATEGORY_VISUALHEALTH")
        }
    }
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
