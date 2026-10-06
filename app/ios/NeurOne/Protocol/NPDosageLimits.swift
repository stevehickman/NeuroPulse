import Foundation

// MARK: - Per-Modality Limit Structs
// All fields are Optional — nil means "no limit at this tier; defer to lower priority or hardware max".

struct NPPBMTranscranialLimits: Codable, Equatable {
    var maxIrradianceMWcm2: Double?        // 0–100
    var maxFrequencyHz: Double?
    var maxDutyCyclePercent: Int?           // ≤25 always enforced by hardware, this adds a tighter ceiling
    var maxSessionDoseJCm2: Double?         // J/cm² per zone per session
    var maxDailyDoseJCm2: Double?           // J/cm² per zone per day
}

struct NPPBMIntranasalLimits: Codable, Equatable {
    var maxIrradianceMWcm2: Double?
    var maxSessionDoseJCm2: Double?
    var maxSessionDurationSeconds: Int?
}

struct NPEEGNeurofeedbackLimits: Codable, Equatable {
    var allowedBands: [String]?             // NPEEGNeurofeedbackParams.EEGBand.rawValue whitelist; nil = all
    var requireClosedLoop: Bool?
}

struct NPBESTacsLimits: Codable, Equatable {
    var maxIntensityMilliamps: Double?
    var maxFrequencyHz: Double?
    var minFrequencyHz: Double?
    var maxSessionDurationSeconds: Int?
    var maxSessionsPerDay: Int?
}

struct NPTDCSLimits: Codable, Equatable {
    var maxIntensityMilliamps: Double?
    var maxSessionDurationSeconds: Int?
    var maxSessionsPerDay: Int?
}

struct NPVNSHRVLimits: Codable, Equatable {
    var maxIntensityMilliamps: Double?
    var maxFrequencyHz: Double?
    var maxSessionDurationSeconds: Int?
    var allowedProtocols: [String]?         // NPVNSHRVParams.HRVProtocol.rawValue whitelist
}

struct NPAudioEntrainmentLimits: Codable, Equatable {
    var maxVolumeDb: Double?
    var maxBinauralBeatsHz: Double?
    var maxIsochronicTonesHz: Double?
}

struct NPVisualStimLimits: Codable, Equatable {
    var maxFrequencyHz: Double?
    var minFrequencyHz: Double?
    var allowedModes: [String]?             // NPVisualStimParams.VisualMode.rawValue whitelist
    var blockHighRiskRange: Bool?           // true → error if 3–30 Hz (default: warning only)
}

struct NPTMSLimits: Codable, Equatable {
    var maxIntensityPercentMT: Int?
    var maxPulsesPerSession: Int?
    var maxPulsesPerDay: Int?
    var maxSessionsPerWeek: Int?
    var allowedProtocols: [String]?         // NPTMSParams.TMSProtocol.rawValue whitelist
    var allowedTargets: [String]?           // NPTMSParams.TMSTarget.rawValue whitelist
}

struct NPDeepPBMLimits: Codable, Equatable {
    var maxIntensityMWcm2: Double?
    var maxSessionDurationSeconds: Int?
}

struct NPClinicalTacsLimits: Codable, Equatable {
    var maxIntensityMilliamps: Double?
    var maxSessionDurationSeconds: Int?
}

struct NPHDTdcsLimits: Codable, Equatable {
    var maxIntensityMilliamps: Double?      // per electrode
    var maxSessionDurationSeconds: Int?
    var allowedMontages: [String]?          // NPHDTdcsParams.Montage.rawValue whitelist
}

struct NPCervicalVnsLimits: Codable, Equatable {
    var maxIntensityMilliamps: Double?
    var maxSessionDurationSeconds: Int?
    // cardiacInterlock is always enforced by safety MCU — not configurable via limits
}

struct NPVibrotactileLimits: Codable, Equatable {
    var maxIntensityG: Double?
    var maxSessionDurationSeconds: Int?
}

// MARK: - NPIndividualProfile

struct NPIndividualProfile: Codable, Identifiable, Equatable {
    var id: UUID = UUID()
    var name: String
    var notes: String = ""
    var dateOfBirth: Date? = nil
    var createdAt: Date = Date()
}

// MARK: - NPLimitsSet

struct NPLimitsSet: Codable, Identifiable, Equatable {
    var id: UUID = UUID()
    var name: String
    var description: String = ""
    var createdAt: Date = Date()
    var modifiedAt: Date = Date()
    var level: LimitLevel
    var helmetId: String? = nil            // non-nil when level == .helmet
    var individualId: UUID? = nil          // non-nil when level == .individual

    enum LimitLevel: String, Codable, CaseIterable {
        case global     = "global"
        case helmet     = "helmet"
        case individual = "individual"

        var displayName: String {
            switch self {
            case .global:     return String(localized: "VALIDATE_SOURCE_GLOBAL")
            case .helmet:     return String(localized: "VALIDATE_SOURCE_HELMET")
            case .individual: return String(localized: "VALIDATE_SOURCE_INDIVIDUAL")
            }
        }
    }

    // MARK: Per-modality limits — nil = no limits configured at this tier
    var pbmTranscranial: NPPBMTranscranialLimits?
    var pbmIntranasal: NPPBMIntranasalLimits?
    var eegNeurofeedback: NPEEGNeurofeedbackLimits?
    var besTacs: NPBESTacsLimits?
    var tdcs: NPTDCSLimits?
    var vnsHrv: NPVNSHRVLimits?
    var audioEntrainment: NPAudioEntrainmentLimits?
    var visualStimulation: NPVisualStimLimits?
    var tms: NPTMSLimits?
    var pbmDeep1170nm: NPDeepPBMLimits?
    var clinicalTacs: NPClinicalTacsLimits?
    var hdTdcs: NPHDTdcsLimits?
    var cervicalVns: NPCervicalVnsLimits?
    var vibrotactile40hz: NPVibrotactileLimits?

    // MARK: Convenience
    static var unlimited: NPLimitsSet {
        NPLimitsSet(name: "Unlimited", level: .global)
    }
}

// MARK: - NPLimitSourceMap
// Records which tier provided each resolved field value.
// Parallel struct to NPLimitsSet's modality structs, using LimitSource? instead of limit values.

enum NPLimitSource: CustomStringConvertible, Equatable {
    case hardware
    case global_
    case helmet
    case individual

    var description: String {
        switch self {
        case .hardware:   return String(localized: "VALIDATE_SOURCE_HARDWARE_LABEL")
        case .global_:    return String(localized: "VALIDATE_SOURCE_GLOBAL_LABEL")
        case .helmet:     return String(localized: "VALIDATE_SOURCE_HELMET_LABEL")
        case .individual: return String(localized: "VALIDATE_SOURCE_INDIVIDUAL_LABEL")
        }
    }
}

/// The source map crosses the core's JSON as the tier's name (`"global"`, `"helmet"`, `"individual"`).
extension NPLimitSource: Codable {
    init(from decoder: Decoder) throws {
        switch try decoder.singleValueContainer().decode(String.self) {
        case "hardware": self = .hardware
        case "helmet": self = .helmet
        case "individual": self = .individual
        default: self = .global_
        }
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.singleValueContainer()
        try container.encode(nppsCoreName)
    }
}

struct NPPBMTranscranialSources: Codable, Equatable {
    var maxIrradianceMWcm2: NPLimitSource?
    var maxFrequencyHz: NPLimitSource?
    var maxDutyCyclePercent: NPLimitSource?
    var maxSessionDoseJCm2: NPLimitSource?
    var maxDailyDoseJCm2: NPLimitSource?
}

struct NPPBMIntranasalSources: Codable, Equatable {
    var maxIrradianceMWcm2: NPLimitSource?
    var maxSessionDoseJCm2: NPLimitSource?
    var maxSessionDurationSeconds: NPLimitSource?
}

struct NPEEGNeurofeedbackSources: Codable, Equatable {
    var allowedBands: NPLimitSource?
    var requireClosedLoop: NPLimitSource?
}

struct NPBESTacsSources: Codable, Equatable {
    var maxIntensityMilliamps: NPLimitSource?
    var maxFrequencyHz: NPLimitSource?
    var minFrequencyHz: NPLimitSource?
    var maxSessionDurationSeconds: NPLimitSource?
    var maxSessionsPerDay: NPLimitSource?
}

struct NPTDCSSources: Codable, Equatable {
    var maxIntensityMilliamps: NPLimitSource?
    var maxSessionDurationSeconds: NPLimitSource?
    var maxSessionsPerDay: NPLimitSource?
}

struct NPVNSHRVSources: Codable, Equatable {
    var maxIntensityMilliamps: NPLimitSource?
    var maxFrequencyHz: NPLimitSource?
    var maxSessionDurationSeconds: NPLimitSource?
    var allowedProtocols: NPLimitSource?
}

struct NPAudioEntrainmentSources: Codable, Equatable {
    var maxVolumeDb: NPLimitSource?
    var maxBinauralBeatsHz: NPLimitSource?
    var maxIsochronicTonesHz: NPLimitSource?
}

struct NPVisualStimSources: Codable, Equatable {
    var maxFrequencyHz: NPLimitSource?
    var minFrequencyHz: NPLimitSource?
    var allowedModes: NPLimitSource?
    var blockHighRiskRange: NPLimitSource?
}

struct NPTMSSources: Codable, Equatable {
    var maxIntensityPercentMT: NPLimitSource?
    var maxPulsesPerSession: NPLimitSource?
    var maxPulsesPerDay: NPLimitSource?
    var maxSessionsPerWeek: NPLimitSource?
    var allowedProtocols: NPLimitSource?
    var allowedTargets: NPLimitSource?
}

struct NPDeepPBMSources: Codable, Equatable {
    var maxIntensityMWcm2: NPLimitSource?
    var maxSessionDurationSeconds: NPLimitSource?
}

struct NPClinicalTacsSources: Codable, Equatable {
    var maxIntensityMilliamps: NPLimitSource?
    var maxSessionDurationSeconds: NPLimitSource?
}

struct NPHDTdcsSources: Codable, Equatable {
    var maxIntensityMilliamps: NPLimitSource?
    var maxSessionDurationSeconds: NPLimitSource?
    var allowedMontages: NPLimitSource?
}

struct NPCervicalVnsSources: Codable, Equatable {
    var maxIntensityMilliamps: NPLimitSource?
    var maxSessionDurationSeconds: NPLimitSource?
}

struct NPVibrotactileSources: Codable, Equatable {
    var maxIntensityG: NPLimitSource?
    var maxSessionDurationSeconds: NPLimitSource?
}

struct NPLimitSourceMap: Codable, Equatable {
    var pbmTranscranial: NPPBMTranscranialSources?
    var pbmIntranasal: NPPBMIntranasalSources?
    var eegNeurofeedback: NPEEGNeurofeedbackSources?
    var besTacs: NPBESTacsSources?
    var tdcs: NPTDCSSources?
    var vnsHrv: NPVNSHRVSources?
    var audioEntrainment: NPAudioEntrainmentSources?
    var visualStimulation: NPVisualStimSources?
    var tms: NPTMSSources?
    var pbmDeep1170nm: NPDeepPBMSources?
    var clinicalTacs: NPClinicalTacsSources?
    var hdTdcs: NPHDTdcsSources?
    var cervicalVns: NPCervicalVnsSources?
    var vibrotactile40hz: NPVibrotactileSources?
}
// MARK: - Three-tier resolution

extension NPLimitsSet {

    /// Resolves individual > helmet > global, field by field within each modality struct.
    /// Returns both the resolved limits and a parallel source map indicating which tier won each field.
    ///
    /// The rule is the shared NPPS core's (common/npps-core/src/resolve.rs, OI-NPPS-CORE-01), the same one the web and
    /// Android call; this used to be fourteen hand-written per-modality merges. A resolution that cannot run stops the
    /// app rather than return "no limits", which is what a silent fallback would mean to the validator.
    static func resolve(
        global: NPLimitsSet?,
        helmet: NPLimitsSet?,
        individual: NPLimitsSet?
    ) -> (limits: NPLimitsSet, sources: NPLimitSourceMap) {
        do {
            return try NppsCore.resolveLimits(global: global, helmet: helmet, individual: individual)
        } catch {
            fatalError("the NPPS core could not resolve limits: \(error.localizedDescription)")
        }
    }
}
