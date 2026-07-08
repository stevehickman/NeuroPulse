import Foundation

// MARK: - Per-Modality Limit Structs
// All fields are Optional — nil means "no limit at this tier; defer to lower priority or hardware max".

struct NPPBMTranscranialLimits: Codable, Equatable {
    var maxIntensityPercent: Double?        // 0–100
    var maxFrequencyHz: Double?
    var maxDutyCyclePercent: Int?           // ≤25 always enforced by hardware, this adds a tighter ceiling
    var maxSessionDoseJCm2: Double?         // J/cm² per zone per session
    var maxDailyDoseJCm2: Double?           // J/cm² per zone per day
}

struct NPPBMIntranasalLimits: Codable, Equatable {
    var maxIntensityPercent: Double?
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
    var maxVolumePercent: Double?
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
            case .global:     return "Global"
            case .helmet:     return "Helmet"
            case .individual: return "Individual"
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
        case .hardware:   return "Hardware Limit"
        case .global_:    return "Global Limit"
        case .helmet:     return "Helmet Limit"
        case .individual: return "Individual Limit"
        }
    }
}

struct NPPBMTranscranialSources: Equatable {
    var maxIntensityPercent: NPLimitSource?
    var maxFrequencyHz: NPLimitSource?
    var maxDutyCyclePercent: NPLimitSource?
    var maxSessionDoseJCm2: NPLimitSource?
    var maxDailyDoseJCm2: NPLimitSource?
}

struct NPPBMIntranasalSources: Equatable {
    var maxIntensityPercent: NPLimitSource?
    var maxSessionDoseJCm2: NPLimitSource?
    var maxSessionDurationSeconds: NPLimitSource?
}

struct NPEEGNeurofeedbackSources: Equatable {
    var allowedBands: NPLimitSource?
    var requireClosedLoop: NPLimitSource?
}

struct NPBESTacsSources: Equatable {
    var maxIntensityMilliamps: NPLimitSource?
    var maxFrequencyHz: NPLimitSource?
    var minFrequencyHz: NPLimitSource?
    var maxSessionDurationSeconds: NPLimitSource?
    var maxSessionsPerDay: NPLimitSource?
}

struct NPTDCSSources: Equatable {
    var maxIntensityMilliamps: NPLimitSource?
    var maxSessionDurationSeconds: NPLimitSource?
    var maxSessionsPerDay: NPLimitSource?
}

struct NPVNSHRVSources: Equatable {
    var maxIntensityMilliamps: NPLimitSource?
    var maxFrequencyHz: NPLimitSource?
    var maxSessionDurationSeconds: NPLimitSource?
    var allowedProtocols: NPLimitSource?
}

struct NPAudioEntrainmentSources: Equatable {
    var maxVolumePercent: NPLimitSource?
    var maxBinauralBeatsHz: NPLimitSource?
    var maxIsochronicTonesHz: NPLimitSource?
}

struct NPVisualStimSources: Equatable {
    var maxFrequencyHz: NPLimitSource?
    var minFrequencyHz: NPLimitSource?
    var allowedModes: NPLimitSource?
    var blockHighRiskRange: NPLimitSource?
}

struct NPTMSSources: Equatable {
    var maxIntensityPercentMT: NPLimitSource?
    var maxPulsesPerSession: NPLimitSource?
    var maxPulsesPerDay: NPLimitSource?
    var maxSessionsPerWeek: NPLimitSource?
    var allowedProtocols: NPLimitSource?
    var allowedTargets: NPLimitSource?
}

struct NPDeepPBMSources: Equatable {
    var maxIntensityMWcm2: NPLimitSource?
    var maxSessionDurationSeconds: NPLimitSource?
}

struct NPClinicalTacsSources: Equatable {
    var maxIntensityMilliamps: NPLimitSource?
    var maxSessionDurationSeconds: NPLimitSource?
}

struct NPHDTdcsSources: Equatable {
    var maxIntensityMilliamps: NPLimitSource?
    var maxSessionDurationSeconds: NPLimitSource?
    var allowedMontages: NPLimitSource?
}

struct NPCervicalVnsSources: Equatable {
    var maxIntensityMilliamps: NPLimitSource?
    var maxSessionDurationSeconds: NPLimitSource?
}

struct NPVibrotactileSources: Equatable {
    var maxIntensityG: NPLimitSource?
    var maxSessionDurationSeconds: NPLimitSource?
}

struct NPLimitSourceMap: Equatable {
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
    static func resolve(
        global: NPLimitsSet?,
        helmet: NPLimitsSet?,
        individual: NPLimitsSet?
    ) -> (limits: NPLimitsSet, sources: NPLimitSourceMap) {
        var r = NPLimitsSet(name: "Resolved", level: .global)
        var src = NPLimitSourceMap()

        (r.pbmTranscranial, src.pbmTranscranial) = mergePBMTranscranial(
            individual?.pbmTranscranial, helmet?.pbmTranscranial, global?.pbmTranscranial)
        (r.pbmIntranasal, src.pbmIntranasal) = mergePBMIntranasal(
            individual?.pbmIntranasal, helmet?.pbmIntranasal, global?.pbmIntranasal)
        (r.eegNeurofeedback, src.eegNeurofeedback) = mergeEEG(
            individual?.eegNeurofeedback, helmet?.eegNeurofeedback, global?.eegNeurofeedback)
        (r.besTacs, src.besTacs) = mergeBESTacs(
            individual?.besTacs, helmet?.besTacs, global?.besTacs)
        (r.tdcs, src.tdcs) = mergeTDCS(
            individual?.tdcs, helmet?.tdcs, global?.tdcs)
        (r.vnsHrv, src.vnsHrv) = mergeVNS(
            individual?.vnsHrv, helmet?.vnsHrv, global?.vnsHrv)
        (r.audioEntrainment, src.audioEntrainment) = mergeAudio(
            individual?.audioEntrainment, helmet?.audioEntrainment, global?.audioEntrainment)
        (r.visualStimulation, src.visualStimulation) = mergeVisual(
            individual?.visualStimulation, helmet?.visualStimulation, global?.visualStimulation)
        (r.tms, src.tms) = mergeTMS(
            individual?.tms, helmet?.tms, global?.tms)
        (r.pbmDeep1170nm, src.pbmDeep1170nm) = mergeDeepPBM(
            individual?.pbmDeep1170nm, helmet?.pbmDeep1170nm, global?.pbmDeep1170nm)
        (r.clinicalTacs, src.clinicalTacs) = mergeClinicalTacs(
            individual?.clinicalTacs, helmet?.clinicalTacs, global?.clinicalTacs)
        (r.hdTdcs, src.hdTdcs) = mergeHDTdcs(
            individual?.hdTdcs, helmet?.hdTdcs, global?.hdTdcs)
        (r.cervicalVns, src.cervicalVns) = mergeCervicalVns(
            individual?.cervicalVns, helmet?.cervicalVns, global?.cervicalVns)
        (r.vibrotactile40hz, src.vibrotactile40hz) = mergeVibrotactile(
            individual?.vibrotactile40hz, helmet?.vibrotactile40hz, global?.vibrotactile40hz)

        return (r, src)
    }

    // MARK: Per-field source helper
    // Returns .individual if individual provided the value, .helmet if helmet did, .global_ otherwise.
    private static func src<T: Equatable>(
        _ i: T?, _ h: T?, _ g: T?
    ) -> NPLimitSource? {
        if i != nil { return .individual }
        if h != nil { return .helmet }
        if g != nil { return .global_ }
        return nil
    }

    // MARK: Merge helpers — one per modality

    private static func mergePBMTranscranial(
        _ i: NPPBMTranscranialLimits?, _ h: NPPBMTranscranialLimits?, _ g: NPPBMTranscranialLimits?
    ) -> (NPPBMTranscranialLimits?, NPPBMTranscranialSources?) {
        guard i != nil || h != nil || g != nil else { return (nil, nil) }
        let lim = NPPBMTranscranialLimits(
            maxIntensityPercent:  i?.maxIntensityPercent  ?? h?.maxIntensityPercent  ?? g?.maxIntensityPercent,
            maxFrequencyHz:       i?.maxFrequencyHz       ?? h?.maxFrequencyHz       ?? g?.maxFrequencyHz,
            maxDutyCyclePercent:  i?.maxDutyCyclePercent  ?? h?.maxDutyCyclePercent  ?? g?.maxDutyCyclePercent,
            maxSessionDoseJCm2:   i?.maxSessionDoseJCm2   ?? h?.maxSessionDoseJCm2   ?? g?.maxSessionDoseJCm2,
            maxDailyDoseJCm2:     i?.maxDailyDoseJCm2     ?? h?.maxDailyDoseJCm2     ?? g?.maxDailyDoseJCm2
        )
        let sources = NPPBMTranscranialSources(
            maxIntensityPercent:  src(i?.maxIntensityPercent,  h?.maxIntensityPercent,  g?.maxIntensityPercent),
            maxFrequencyHz:       src(i?.maxFrequencyHz,       h?.maxFrequencyHz,       g?.maxFrequencyHz),
            maxDutyCyclePercent:  src(i?.maxDutyCyclePercent,  h?.maxDutyCyclePercent,  g?.maxDutyCyclePercent),
            maxSessionDoseJCm2:   src(i?.maxSessionDoseJCm2,   h?.maxSessionDoseJCm2,   g?.maxSessionDoseJCm2),
            maxDailyDoseJCm2:     src(i?.maxDailyDoseJCm2,     h?.maxDailyDoseJCm2,     g?.maxDailyDoseJCm2)
        )
        return (lim, sources)
    }

    private static func mergePBMIntranasal(
        _ i: NPPBMIntranasalLimits?, _ h: NPPBMIntranasalLimits?, _ g: NPPBMIntranasalLimits?
    ) -> (NPPBMIntranasalLimits?, NPPBMIntranasalSources?) {
        guard i != nil || h != nil || g != nil else { return (nil, nil) }
        let lim = NPPBMIntranasalLimits(
            maxIntensityPercent:        i?.maxIntensityPercent        ?? h?.maxIntensityPercent        ?? g?.maxIntensityPercent,
            maxSessionDoseJCm2:         i?.maxSessionDoseJCm2         ?? h?.maxSessionDoseJCm2         ?? g?.maxSessionDoseJCm2,
            maxSessionDurationSeconds:  i?.maxSessionDurationSeconds  ?? h?.maxSessionDurationSeconds  ?? g?.maxSessionDurationSeconds
        )
        let sources = NPPBMIntranasalSources(
            maxIntensityPercent:        src(i?.maxIntensityPercent,        h?.maxIntensityPercent,        g?.maxIntensityPercent),
            maxSessionDoseJCm2:         src(i?.maxSessionDoseJCm2,         h?.maxSessionDoseJCm2,         g?.maxSessionDoseJCm2),
            maxSessionDurationSeconds:  src(i?.maxSessionDurationSeconds,  h?.maxSessionDurationSeconds,  g?.maxSessionDurationSeconds)
        )
        return (lim, sources)
    }

    private static func mergeEEG(
        _ i: NPEEGNeurofeedbackLimits?, _ h: NPEEGNeurofeedbackLimits?, _ g: NPEEGNeurofeedbackLimits?
    ) -> (NPEEGNeurofeedbackLimits?, NPEEGNeurofeedbackSources?) {
        guard i != nil || h != nil || g != nil else { return (nil, nil) }
        let lim = NPEEGNeurofeedbackLimits(
            allowedBands:      i?.allowedBands      ?? h?.allowedBands      ?? g?.allowedBands,
            requireClosedLoop: i?.requireClosedLoop ?? h?.requireClosedLoop ?? g?.requireClosedLoop
        )
        let sources = NPEEGNeurofeedbackSources(
            allowedBands:      src(i?.allowedBands,      h?.allowedBands,      g?.allowedBands),
            requireClosedLoop: src(i?.requireClosedLoop, h?.requireClosedLoop, g?.requireClosedLoop)
        )
        return (lim, sources)
    }

    private static func mergeBESTacs(
        _ i: NPBESTacsLimits?, _ h: NPBESTacsLimits?, _ g: NPBESTacsLimits?
    ) -> (NPBESTacsLimits?, NPBESTacsSources?) {
        guard i != nil || h != nil || g != nil else { return (nil, nil) }
        let lim = NPBESTacsLimits(
            maxIntensityMilliamps:     i?.maxIntensityMilliamps     ?? h?.maxIntensityMilliamps     ?? g?.maxIntensityMilliamps,
            maxFrequencyHz:            i?.maxFrequencyHz            ?? h?.maxFrequencyHz            ?? g?.maxFrequencyHz,
            minFrequencyHz:            i?.minFrequencyHz            ?? h?.minFrequencyHz            ?? g?.minFrequencyHz,
            maxSessionDurationSeconds: i?.maxSessionDurationSeconds ?? h?.maxSessionDurationSeconds ?? g?.maxSessionDurationSeconds,
            maxSessionsPerDay:         i?.maxSessionsPerDay         ?? h?.maxSessionsPerDay         ?? g?.maxSessionsPerDay
        )
        let sources = NPBESTacsSources(
            maxIntensityMilliamps:     src(i?.maxIntensityMilliamps,     h?.maxIntensityMilliamps,     g?.maxIntensityMilliamps),
            maxFrequencyHz:            src(i?.maxFrequencyHz,            h?.maxFrequencyHz,            g?.maxFrequencyHz),
            minFrequencyHz:            src(i?.minFrequencyHz,            h?.minFrequencyHz,            g?.minFrequencyHz),
            maxSessionDurationSeconds: src(i?.maxSessionDurationSeconds, h?.maxSessionDurationSeconds, g?.maxSessionDurationSeconds),
            maxSessionsPerDay:         src(i?.maxSessionsPerDay,         h?.maxSessionsPerDay,         g?.maxSessionsPerDay)
        )
        return (lim, sources)
    }

    private static func mergeTDCS(
        _ i: NPTDCSLimits?, _ h: NPTDCSLimits?, _ g: NPTDCSLimits?
    ) -> (NPTDCSLimits?, NPTDCSSources?) {
        guard i != nil || h != nil || g != nil else { return (nil, nil) }
        let lim = NPTDCSLimits(
            maxIntensityMilliamps:     i?.maxIntensityMilliamps     ?? h?.maxIntensityMilliamps     ?? g?.maxIntensityMilliamps,
            maxSessionDurationSeconds: i?.maxSessionDurationSeconds ?? h?.maxSessionDurationSeconds ?? g?.maxSessionDurationSeconds,
            maxSessionsPerDay:         i?.maxSessionsPerDay         ?? h?.maxSessionsPerDay         ?? g?.maxSessionsPerDay
        )
        let sources = NPTDCSSources(
            maxIntensityMilliamps:     src(i?.maxIntensityMilliamps,     h?.maxIntensityMilliamps,     g?.maxIntensityMilliamps),
            maxSessionDurationSeconds: src(i?.maxSessionDurationSeconds, h?.maxSessionDurationSeconds, g?.maxSessionDurationSeconds),
            maxSessionsPerDay:         src(i?.maxSessionsPerDay,         h?.maxSessionsPerDay,         g?.maxSessionsPerDay)
        )
        return (lim, sources)
    }

    private static func mergeVNS(
        _ i: NPVNSHRVLimits?, _ h: NPVNSHRVLimits?, _ g: NPVNSHRVLimits?
    ) -> (NPVNSHRVLimits?, NPVNSHRVSources?) {
        guard i != nil || h != nil || g != nil else { return (nil, nil) }
        let lim = NPVNSHRVLimits(
            maxIntensityMilliamps:     i?.maxIntensityMilliamps     ?? h?.maxIntensityMilliamps     ?? g?.maxIntensityMilliamps,
            maxFrequencyHz:            i?.maxFrequencyHz            ?? h?.maxFrequencyHz            ?? g?.maxFrequencyHz,
            maxSessionDurationSeconds: i?.maxSessionDurationSeconds ?? h?.maxSessionDurationSeconds ?? g?.maxSessionDurationSeconds,
            allowedProtocols:          i?.allowedProtocols          ?? h?.allowedProtocols          ?? g?.allowedProtocols
        )
        let sources = NPVNSHRVSources(
            maxIntensityMilliamps:     src(i?.maxIntensityMilliamps,     h?.maxIntensityMilliamps,     g?.maxIntensityMilliamps),
            maxFrequencyHz:            src(i?.maxFrequencyHz,            h?.maxFrequencyHz,            g?.maxFrequencyHz),
            maxSessionDurationSeconds: src(i?.maxSessionDurationSeconds, h?.maxSessionDurationSeconds, g?.maxSessionDurationSeconds),
            allowedProtocols:          src(i?.allowedProtocols,          h?.allowedProtocols,          g?.allowedProtocols)
        )
        return (lim, sources)
    }

    private static func mergeAudio(
        _ i: NPAudioEntrainmentLimits?, _ h: NPAudioEntrainmentLimits?, _ g: NPAudioEntrainmentLimits?
    ) -> (NPAudioEntrainmentLimits?, NPAudioEntrainmentSources?) {
        guard i != nil || h != nil || g != nil else { return (nil, nil) }
        let lim = NPAudioEntrainmentLimits(
            maxVolumePercent:      i?.maxVolumePercent      ?? h?.maxVolumePercent      ?? g?.maxVolumePercent,
            maxBinauralBeatsHz:    i?.maxBinauralBeatsHz    ?? h?.maxBinauralBeatsHz    ?? g?.maxBinauralBeatsHz,
            maxIsochronicTonesHz:  i?.maxIsochronicTonesHz  ?? h?.maxIsochronicTonesHz  ?? g?.maxIsochronicTonesHz
        )
        let sources = NPAudioEntrainmentSources(
            maxVolumePercent:      src(i?.maxVolumePercent,      h?.maxVolumePercent,      g?.maxVolumePercent),
            maxBinauralBeatsHz:    src(i?.maxBinauralBeatsHz,    h?.maxBinauralBeatsHz,    g?.maxBinauralBeatsHz),
            maxIsochronicTonesHz:  src(i?.maxIsochronicTonesHz,  h?.maxIsochronicTonesHz,  g?.maxIsochronicTonesHz)
        )
        return (lim, sources)
    }

    private static func mergeVisual(
        _ i: NPVisualStimLimits?, _ h: NPVisualStimLimits?, _ g: NPVisualStimLimits?
    ) -> (NPVisualStimLimits?, NPVisualStimSources?) {
        guard i != nil || h != nil || g != nil else { return (nil, nil) }
        let lim = NPVisualStimLimits(
            maxFrequencyHz:      i?.maxFrequencyHz      ?? h?.maxFrequencyHz      ?? g?.maxFrequencyHz,
            minFrequencyHz:      i?.minFrequencyHz      ?? h?.minFrequencyHz      ?? g?.minFrequencyHz,
            allowedModes:        i?.allowedModes        ?? h?.allowedModes        ?? g?.allowedModes,
            blockHighRiskRange:  i?.blockHighRiskRange  ?? h?.blockHighRiskRange  ?? g?.blockHighRiskRange
        )
        let sources = NPVisualStimSources(
            maxFrequencyHz:      src(i?.maxFrequencyHz,      h?.maxFrequencyHz,      g?.maxFrequencyHz),
            minFrequencyHz:      src(i?.minFrequencyHz,      h?.minFrequencyHz,      g?.minFrequencyHz),
            allowedModes:        src(i?.allowedModes,        h?.allowedModes,        g?.allowedModes),
            blockHighRiskRange:  src(i?.blockHighRiskRange,  h?.blockHighRiskRange,  g?.blockHighRiskRange)
        )
        return (lim, sources)
    }

    private static func mergeTMS(
        _ i: NPTMSLimits?, _ h: NPTMSLimits?, _ g: NPTMSLimits?
    ) -> (NPTMSLimits?, NPTMSSources?) {
        guard i != nil || h != nil || g != nil else { return (nil, nil) }
        let lim = NPTMSLimits(
            maxIntensityPercentMT: i?.maxIntensityPercentMT ?? h?.maxIntensityPercentMT ?? g?.maxIntensityPercentMT,
            maxPulsesPerSession:   i?.maxPulsesPerSession   ?? h?.maxPulsesPerSession   ?? g?.maxPulsesPerSession,
            maxPulsesPerDay:       i?.maxPulsesPerDay       ?? h?.maxPulsesPerDay       ?? g?.maxPulsesPerDay,
            maxSessionsPerWeek:    i?.maxSessionsPerWeek    ?? h?.maxSessionsPerWeek    ?? g?.maxSessionsPerWeek,
            allowedProtocols:      i?.allowedProtocols      ?? h?.allowedProtocols      ?? g?.allowedProtocols,
            allowedTargets:        i?.allowedTargets        ?? h?.allowedTargets        ?? g?.allowedTargets
        )
        let sources = NPTMSSources(
            maxIntensityPercentMT: src(i?.maxIntensityPercentMT, h?.maxIntensityPercentMT, g?.maxIntensityPercentMT),
            maxPulsesPerSession:   src(i?.maxPulsesPerSession,   h?.maxPulsesPerSession,   g?.maxPulsesPerSession),
            maxPulsesPerDay:       src(i?.maxPulsesPerDay,       h?.maxPulsesPerDay,       g?.maxPulsesPerDay),
            maxSessionsPerWeek:    src(i?.maxSessionsPerWeek,    h?.maxSessionsPerWeek,    g?.maxSessionsPerWeek),
            allowedProtocols:      src(i?.allowedProtocols,      h?.allowedProtocols,      g?.allowedProtocols),
            allowedTargets:        src(i?.allowedTargets,        h?.allowedTargets,        g?.allowedTargets)
        )
        return (lim, sources)
    }

    private static func mergeDeepPBM(
        _ i: NPDeepPBMLimits?, _ h: NPDeepPBMLimits?, _ g: NPDeepPBMLimits?
    ) -> (NPDeepPBMLimits?, NPDeepPBMSources?) {
        guard i != nil || h != nil || g != nil else { return (nil, nil) }
        let lim = NPDeepPBMLimits(
            maxIntensityMWcm2:         i?.maxIntensityMWcm2         ?? h?.maxIntensityMWcm2         ?? g?.maxIntensityMWcm2,
            maxSessionDurationSeconds: i?.maxSessionDurationSeconds ?? h?.maxSessionDurationSeconds ?? g?.maxSessionDurationSeconds
        )
        let sources = NPDeepPBMSources(
            maxIntensityMWcm2:         src(i?.maxIntensityMWcm2,         h?.maxIntensityMWcm2,         g?.maxIntensityMWcm2),
            maxSessionDurationSeconds: src(i?.maxSessionDurationSeconds, h?.maxSessionDurationSeconds, g?.maxSessionDurationSeconds)
        )
        return (lim, sources)
    }

    private static func mergeClinicalTacs(
        _ i: NPClinicalTacsLimits?, _ h: NPClinicalTacsLimits?, _ g: NPClinicalTacsLimits?
    ) -> (NPClinicalTacsLimits?, NPClinicalTacsSources?) {
        guard i != nil || h != nil || g != nil else { return (nil, nil) }
        let lim = NPClinicalTacsLimits(
            maxIntensityMilliamps:     i?.maxIntensityMilliamps     ?? h?.maxIntensityMilliamps     ?? g?.maxIntensityMilliamps,
            maxSessionDurationSeconds: i?.maxSessionDurationSeconds ?? h?.maxSessionDurationSeconds ?? g?.maxSessionDurationSeconds
        )
        let sources = NPClinicalTacsSources(
            maxIntensityMilliamps:     src(i?.maxIntensityMilliamps,     h?.maxIntensityMilliamps,     g?.maxIntensityMilliamps),
            maxSessionDurationSeconds: src(i?.maxSessionDurationSeconds, h?.maxSessionDurationSeconds, g?.maxSessionDurationSeconds)
        )
        return (lim, sources)
    }

    private static func mergeHDTdcs(
        _ i: NPHDTdcsLimits?, _ h: NPHDTdcsLimits?, _ g: NPHDTdcsLimits?
    ) -> (NPHDTdcsLimits?, NPHDTdcsSources?) {
        guard i != nil || h != nil || g != nil else { return (nil, nil) }
        let lim = NPHDTdcsLimits(
            maxIntensityMilliamps:     i?.maxIntensityMilliamps     ?? h?.maxIntensityMilliamps     ?? g?.maxIntensityMilliamps,
            maxSessionDurationSeconds: i?.maxSessionDurationSeconds ?? h?.maxSessionDurationSeconds ?? g?.maxSessionDurationSeconds,
            allowedMontages:           i?.allowedMontages           ?? h?.allowedMontages           ?? g?.allowedMontages
        )
        let sources = NPHDTdcsSources(
            maxIntensityMilliamps:     src(i?.maxIntensityMilliamps,     h?.maxIntensityMilliamps,     g?.maxIntensityMilliamps),
            maxSessionDurationSeconds: src(i?.maxSessionDurationSeconds, h?.maxSessionDurationSeconds, g?.maxSessionDurationSeconds),
            allowedMontages:           src(i?.allowedMontages,           h?.allowedMontages,           g?.allowedMontages)
        )
        return (lim, sources)
    }

    private static func mergeCervicalVns(
        _ i: NPCervicalVnsLimits?, _ h: NPCervicalVnsLimits?, _ g: NPCervicalVnsLimits?
    ) -> (NPCervicalVnsLimits?, NPCervicalVnsSources?) {
        guard i != nil || h != nil || g != nil else { return (nil, nil) }
        let lim = NPCervicalVnsLimits(
            maxIntensityMilliamps:     i?.maxIntensityMilliamps     ?? h?.maxIntensityMilliamps     ?? g?.maxIntensityMilliamps,
            maxSessionDurationSeconds: i?.maxSessionDurationSeconds ?? h?.maxSessionDurationSeconds ?? g?.maxSessionDurationSeconds
        )
        let sources = NPCervicalVnsSources(
            maxIntensityMilliamps:     src(i?.maxIntensityMilliamps,     h?.maxIntensityMilliamps,     g?.maxIntensityMilliamps),
            maxSessionDurationSeconds: src(i?.maxSessionDurationSeconds, h?.maxSessionDurationSeconds, g?.maxSessionDurationSeconds)
        )
        return (lim, sources)
    }

    private static func mergeVibrotactile(
        _ i: NPVibrotactileLimits?, _ h: NPVibrotactileLimits?, _ g: NPVibrotactileLimits?
    ) -> (NPVibrotactileLimits?, NPVibrotactileSources?) {
        guard i != nil || h != nil || g != nil else { return (nil, nil) }
        let lim = NPVibrotactileLimits(
            maxIntensityG:             i?.maxIntensityG             ?? h?.maxIntensityG             ?? g?.maxIntensityG,
            maxSessionDurationSeconds: i?.maxSessionDurationSeconds ?? h?.maxSessionDurationSeconds ?? g?.maxSessionDurationSeconds
        )
        let sources = NPVibrotactileSources(
            maxIntensityG:             src(i?.maxIntensityG,             h?.maxIntensityG,             g?.maxIntensityG),
            maxSessionDurationSeconds: src(i?.maxSessionDurationSeconds, h?.maxSessionDurationSeconds, g?.maxSessionDurationSeconds)
        )
        return (lim, sources)
    }
}
