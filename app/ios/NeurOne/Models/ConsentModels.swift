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

// MARK: - Signed study descriptor (§5.3 step 1)

/// A study as NeurOne's research service issues it: what it wants, how it undertakes to
/// anonymise it, and a signature over all of that.
///
/// **This, not `StudyInvitation`, is what enters the device.** The invitation the user reads is
/// *derived* from a descriptor on-device, after the signature verifies — so every sentence on the
/// consent surface is one the app wrote, and the party asking for access supplies facts about the
/// study rather than the prose describing it. The descriptor used to be skipped entirely:
/// `addInvitation` took a finished invitation, plain-language "what they cannot see" list and all,
/// on trust from whatever called it (`OI-CONSENT-03`).
struct StudyDescriptor: Codable, Equatable {
    var studyID: String
    /// The study's own name. The one field that is server-supplied text and stays that way: it
    /// names a specific study the way a part number names a part (CLAUDE.md §17).
    var studyTitle: String
    var researchCategories: [ResearchCategory]
    /// The UHDR elements the study asks for. What it *cannot* see is derived from this, not
    /// asserted alongside it — see `StudyInvitation.cannotLearn`.
    var requestedElements: Set<UHDRElement>
    /// §5.3 anonymisation parameters, checked against their floors at ingestion. The device is
    /// the only place they can be checked, because §5.3 puts the anonymisation on the device.
    var kAnonymity: Int
    var dateRoundingDays: Int
    var issuedAt: Date
    /// Detached signature over the canonical descriptor bytes. Never parsed here: it is the
    /// verifier's input, and nothing derived from an unverified descriptor may reach the user.
    var signature: String
}

// MARK: - Study descriptor verification (§5.3 step 1)

/// The result of checking a descriptor's signature.
///
/// `unavailable` is a third answer rather than a flavour of `rejected` because "we cannot check"
/// and "we checked and it is forged" are different facts about the world, and the device says
/// which. Both refuse ingestion.
enum StudyDescriptorVerification: Equatable {
    /// Signature checks out. Carries the SHA-256 of the exact bytes that were signed — the §5.3
    /// audit-trail hash, produced by the only component that has those bytes.
    case verified(descriptorHash: String)
    case rejected
    /// No signing key is configured, so no descriptor can be checked at all.
    case unavailable
}

/// Port for descriptor signature verification. §5.3 locks that study descriptors are
/// cryptographically signed; the key distribution and the fetch that carries them belong to the
/// NeurOne study service, which does not exist yet (`OI-CONSENT-07`).
protocol StudyDescriptorVerifier {
    func verify(_ descriptor: StudyDescriptor) -> StudyDescriptorVerification
}

/// The default verifier, and the reason `ConsentStore` takes one at all: it verifies nothing and
/// admits nothing.
///
/// A store that ingested descriptors without a verifier would let whoever builds the transport
/// decide whether §5.3's signature is checked, and the store could not tell a verified descriptor
/// from a fabricated one. With this as the default the decision is not theirs to skip: until a
/// real verifier is injected, ingestion refuses everything and says why.
struct RefusingStudyDescriptorVerifier: StudyDescriptorVerifier {
    func verify(_ descriptor: StudyDescriptor) -> StudyDescriptorVerification { .unavailable }
}

// MARK: - Study descriptor ingestion outcome (§6.3)

/// What the device did with a descriptor. Every refusal names its reason, because "nothing
/// appeared" is the one outcome a user cannot distinguish from "nothing was sent".
enum StudyDescriptorAdmission: Equatable {
    /// Admitted, as an invitation of this posture.
    case admitted(posture: StudyInvitation.Posture, descriptorHash: String)
    case refused(Refusal)

    enum Refusal: String, Equatable, Codable {
        /// No signing key configured — the state of every device today (`OI-CONSENT-07`).
        case verifierUnavailable
        case signatureInvalid
        /// k < 10 or date rounding < 1 week (§5.3). A study whose anonymisation is below the
        /// locked floor is not one the user may be asked to consent to.
        case anonymisationBelowFloor
        /// L1 is the shared precondition for every delivery path, invitations and engagement
        /// notifications alike (§6.2.1).
        case noContactConsent
        /// No L2 category on this descriptor is consented, and L3 is off.
        case categoryNotConsented
        /// The user holds no research consent at any layer.
        case noResearchConsent
        /// This study was already decided or already withdrawn from. §5.3 and §6.3 step 6 make
        /// withdrawal block future descriptor processing; re-asking would be the device
        /// forgetting an answer the user already gave.
        case studyAlreadyDecided
    }
}

// MARK: - Per-project consent notification (§6.3)

/// A study as the user sees it — derived from a verified `StudyDescriptor`, never received
/// ready-made.
struct StudyInvitation: Codable, Identifiable, Equatable {
    var id: UUID
    var studyID: String
    var studyTitle: String
    var researchCategories: [ResearchCategory]
    var approvedElements: Set<UHDRElement>
    /// SHA-256 of the signed descriptor this was derived from (§5.3 audit trail). Carried onto
    /// the participation record on acceptance, which used to record the literal string
    /// `"pending"` because there was no descriptor to hash.
    var descriptorHash: String
    var kAnonymity: Int
    var dateRoundingDays: Int
    var posture: Posture
    var receivedAt: Date
    var decision: StudyDecision?

    /// **Which question the device is putting to the user, and what silence means.**
    ///
    /// §6.2 locks that an L3 user "still receives per-study *engagement* notifications, **not
    /// consent requests**" — they pre-approved NeurOne-reviewed research and may opt out per
    /// study. So the same study reaches an L2 user and an L3 user as two different things, and
    /// the default on no answer is opposite: an unanswered consent request means *not
    /// participating*, an unread engagement notification means *participating*.
    ///
    /// One shape for both would have to pick one of those defaults for everyone. That is the
    /// reason this type could not simply be persisted as it was.
    enum Posture: String, Codable, Equatable {
        /// L2 path: the user is being asked, and is not in the study until they say yes.
        case consentRequest
        /// L3 path: the user pre-approved this study, is in it already, and is being told —
        /// with a per-study opt-out that goes through `withdrawFromStudy`.
        case engagementNotification
    }

    /// §6.3 step 4's three responses: Yes / No / Ask a question.
    enum StudyDecision: Codable, Equatable {
        case accepted(at: Date)
        case declined(at: Date)
        case questionSent(at: Date)
    }

    /// The elements this study cannot see — **derived here, never asserted by the descriptor.**
    ///
    /// §6.3 step 3 requires the invitation to be "explicit about what researchers CAN and CANNOT
    /// see". Taking the second half as prose from the party asking for access lets that party
    /// write it, and it drifts from the first half by construction the moment either changes.
    /// It is the complement of `approvedElements`, so the app computes it.
    var cannotLearn: Set<UHDRElement> {
        Set(UHDRElement.allCases).subtracting(approvedElements)
    }

    /// Still in the user's inbox. A sent question leaves it open — asking is not deciding, the
    /// same rule `ClinicianAccessExpansionRequest.isPending` holds for §6.1.
    var isOpen: Bool {
        switch decision {
        case .none, .some(.questionSent): return true
        case .some(.accepted), .some(.declined): return false
        }
    }

    /// Whether an unanswered invitation means the user is in the study. True only for an
    /// engagement notification, where L3 already answered.
    var participatesWithoutAnswer: Bool { posture == .engagementNotification }

    var questionSentAt: Date? {
        if case let .some(.questionSent(at)) = decision { return at }
        return nil
    }
}
