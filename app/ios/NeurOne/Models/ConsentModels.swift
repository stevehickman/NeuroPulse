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

// MARK: - Clinician access scope (§6.1 expansion workflow)

/// One access decision on a clinician grant: a set of UHDR elements, the point from which the
/// clinician may see data carrying them, and — as a **separate** decision — whether that approval
/// also reached backwards over data already recorded.
///
/// §6.1 requires retroactive and prospective access to be presented as separate consent decisions
/// "even if made simultaneously". They therefore cannot collapse into one field on the grant:
/// `tier.uhdrElements` is timeless, so raising a grant's tier by itself hands the clinician the
/// new elements over every session ever recorded. A user who approves an expansion going forward
/// and refuses it over history has taken a position the record has to be able to hold, and a
/// tier alone cannot hold it.
struct ClinicianAccessScope: Codable, Equatable {
    /// The elements this one decision covers — for an expansion, only the newly added ones.
    var elements: Set<UHDRElement>
    /// Prospective decision: data recorded at or after this instant is in scope.
    var effectiveFrom: Date
    /// Retroactive decision, taken separately: data recorded BEFORE `effectiveFrom` is in scope too.
    var includesPriorData: Bool
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

    /// The access decisions this grant is made of, newest last. Optional because a grant written
    /// before the §6.1 expansion workflow existed has none; `nil` is "never went through the
    /// workflow", which is not the same as "went through it and came out with no access".
    /// Read it through `effectiveScopes`, never directly.
    var accessScopes: [ClinicianAccessScope]?

    init(
        id: UUID,
        clinicianName: String,
        clinicianOrganization: String,
        tier: ClinicianUseCaseTier,
        grantedAt: Date,
        expiresAt: Date? = nil,
        isActive: Bool = true,
        accessScopes: [ClinicianAccessScope]? = nil
    ) {
        self.id = id
        self.clinicianName = clinicianName
        self.clinicianOrganization = clinicianOrganization
        self.tier = tier
        self.grantedAt = grantedAt
        self.expiresAt = expiresAt
        self.isActive = isActive
        self.accessScopes = accessScopes
    }

    /// The scopes as decided, or the pre-workflow equivalent for a grant that has none: the
    /// tier's elements, effective from the grant date, with prior data included.
    ///
    /// That fallback is deliberately what a bare `tier` has always meant, so introducing the
    /// workflow does not silently re-scope an existing grant. Whether an *initial* grant should
    /// default to prior data at all is a real question, and a separate one from expansion —
    /// `OI-CONSENT-06`.
    var effectiveScopes: [ClinicianAccessScope] {
        accessScopes ?? [
            ClinicianAccessScope(
                elements: tier.uhdrElements,
                effectiveFrom: grantedAt,
                includesPriorData: true
            )
        ]
    }

    /// Everything this grant reaches, ignoring when the data was recorded. This is the
    /// dashboard's "what can they see" line; for the time-aware answer use
    /// `elementsVisible(forDataRecordedAt:)`.
    var approvedElements: Set<UHDRElement> {
        effectiveScopes.reduce(into: Set<UHDRElement>()) { $0.formUnion($1.elements) }
    }

    /// Elements the clinician may see in data recorded at `date` — the predicate that makes the
    /// retroactive decision mean something. A scope contributes when the data is at or after its
    /// effective instant, or when the user separately approved prior data for it.
    func elementsVisible(forDataRecordedAt date: Date) -> Set<UHDRElement> {
        effectiveScopes.reduce(into: Set<UHDRElement>()) { acc, scope in
            if date >= scope.effectiveFrom || scope.includesPriorData {
                acc.formUnion(scope.elements)
            }
        }
    }

    /// Elements this grant reaches only from some point onwards — what the user kept out of the
    /// clinician's view of their earlier sessions. Surfaced on the dashboard so a "going forward
    /// only" answer stays visible after the decision, rather than vanishing into the record.
    var forwardOnlyElements: Set<UHDRElement> {
        effectiveScopes
            .filter { !$0.includesPriorData }
            .reduce(into: Set<UHDRElement>()) { $0.formUnion($1.elements) }
    }
}

// MARK: - Clinician access expansion request (§6.1)

/// A clinician's request to widen an existing grant, held until the user decides.
///
/// §6.1's workflow is *differential consent document → persistent user notification → user
/// approves / denies / asks questions → retroactive access is a separate decision*. This type is
/// the persistent half: it survives app restarts because a notification the user can lose by
/// backgrounding the app is not a notification, and it stays pending after a question is sent,
/// because asking is not deciding.
struct ClinicianAccessExpansionRequest: Codable, Identifiable {
    var id: UUID
    var grantID: UUID
    var fromTier: ClinicianUseCaseTier
    var toTier: ClinicianUseCaseTier
    var requestedAt: Date
    var decision: ExpansionDecision?

    /// The two §6.1 decisions are kept apart in the record, not merged into one Bool: `approved`
    /// carries the prospective answer, and `includesPriorData` carries the retroactive one that
    /// was asked separately after it.
    enum ExpansionDecision: Codable, Equatable {
        case approved(at: Date, includesPriorData: Bool)
        case denied(at: Date)
        case questionSent(at: Date)
    }

    /// Still awaiting a decision. A sent question leaves the request pending — the user has not
    /// approved or denied anything, and the notification must not disappear as though they had.
    var isPending: Bool {
        switch decision {
        case .none, .some(.questionSent): return true
        case .some(.approved), .some(.denied): return false
        }
    }

    var questionSentAt: Date? {
        if case let .some(.questionSent(at)) = decision { return at }
        return nil
    }
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
