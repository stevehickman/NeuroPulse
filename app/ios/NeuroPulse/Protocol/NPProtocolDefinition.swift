import Foundation

// MARK: - Interval Config

struct NPIntervalConfig: Codable, Equatable {
    /// 0 = continuous (no pulsing at the protocol scheduling level)
    var intervalOnSeconds: Int
    var intervalOffSeconds: Int
    /// nil = run until session end
    var repeatCount: Int?

    static let continuous = NPIntervalConfig(intervalOnSeconds: 0, intervalOffSeconds: 0, repeatCount: nil)

    var isContinuous: Bool { intervalOnSeconds == 0 }
}

// MARK: - Per-Modality Parameter Structs

// MARK: PBM Transcranial

struct NPPBMTranscranialParams: Codable, Equatable {
    enum ZoneSelection: String, Codable, CaseIterable, Equatable {
        case all
        case front
        case rear
        case custom

        var displayName: String {
            switch self {
            case .all:    return "All Zones (1–5)"
            case .front:  return "Front (1–3)"
            case .rear:   return "Rear (3–5)"
            case .custom: return "Custom"
            }
        }

        var defaultZones: [Int] {
            switch self {
            case .all:    return [0, 1, 2, 3, 4]
            case .front:  return [0, 1, 2]
            case .rear:   return [2, 3, 4]
            case .custom: return []
            }
        }
    }

    enum Wavelength: String, Codable, CaseIterable, Equatable {
        case base660_808nm    = "660_808nm"
        case smart1064nm      = "1064nm"
        case tri660_808_1064  = "660_808_1064nm"

        var displayName: String {
            switch self {
            case .base660_808nm:   return "660 + 808nm (base)"
            case .smart1064nm:     return "1064nm (Smart Module)"
            case .tri660_808_1064: return "660 + 808 + 1064nm (Smart Module)"
            }
        }

        var requiresSmartModule: Bool {
            self == .smart1064nm || self == .tri660_808_1064
        }
    }

    var zones: ZoneSelection = .all
    var customZones: [Int]? = nil
    var wavelength: Wavelength = .base660_808nm
    var intensityPercent: Double = 75
    var frequencyHz: Double = 20        // 0 = CW
    var dutyCyclePercent: Int = 25      // ≤25, only shown when frequencyHz > 0

    var resolvedZones: [Int] {
        if zones == .custom, let cz = customZones { return cz }
        return zones.defaultZones
    }
}

// MARK: PBM Intranasal

struct NPPBMIntranasalParams: Codable, Equatable {
    var intensityPercent: Double = 60
    var frequencyHz: Double = 40
    var dutyCyclePercent: Int = 25
}

// MARK: EEG Neurofeedback

struct NPEEGNeurofeedbackParams: Codable, Equatable {
    enum ChannelSelection: String, Codable, CaseIterable, Equatable {
        case all
        case front
        case central
        case custom

        var displayName: String {
            switch self {
            case .all:     return "All 8 Channels"
            case .front:   return "Frontal (Fp1, Fp2, F3, F4)"
            case .central: return "Central (C3, C4, P3, P4)"
            case .custom:  return "Custom"
            }
        }

        var defaultChannels: [String] {
            switch self {
            case .all:     return ["Fp1","Fp2","F3","F4","C3","C4","P3","P4"]
            case .front:   return ["Fp1","Fp2","F3","F4"]
            case .central: return ["C3","C4","P3","P4"]
            case .custom:  return []
            }
        }
    }

    enum EEGBand: String, Codable, CaseIterable, Equatable {
        case delta
        case theta
        case alpha
        case beta
        case gamma
        case alphaTheta
        case gammaTheta

        var displayName: String {
            switch self {
            case .delta:      return "Delta (Sleep)"
            case .theta:      return "Theta (Memory)"
            case .alpha:      return "Alpha (Calm)"
            case .beta:       return "Beta (Focus)"
            case .gamma:      return "Gamma (Clarity)"
            case .alphaTheta: return "Alpha-Theta"
            case .gammaTheta: return "Gamma-Theta (Coupled)"
            }
        }

        var hzRange: ClosedRange<Double> {
            switch self {
            case .delta:      return 0.5...4
            case .theta:      return 4...8
            case .alpha:      return 8...13
            case .beta:       return 13...30
            case .gamma:      return 30...100
            case .alphaTheta: return 4...13
            case .gammaTheta: return 4...100
            }
        }
    }

    var channels: ChannelSelection = .all
    var customChannels: [String]? = nil
    var band: EEGBand = .alpha
    var closedLoopEnabled: Bool = true

    var resolvedChannels: [String] {
        if channels == .custom, let cc = customChannels { return cc }
        return channels.defaultChannels
    }
}

// MARK: BES / tACS

struct NPBESTacsParams: Codable, Equatable {
    enum Waveform: String, Codable, CaseIterable, Equatable {
        case sinusoidal
        case square
        case triangular

        var displayName: String { rawValue.capitalized }
    }

    var frequencyHz: Double = 20         // 0.5–40 Hz
    var intensityMilliamps: Double = 0.8 // ≤1 mA
    var waveform: Waveform = .sinusoidal
}

// MARK: tDCS

struct NPTDCSParams: Codable, Equatable {
    var intensityMilliamps: Double = 1.0         // 0.1–2 mA
    var electrodePairs: [[String]] = [["Fp1","P3"]]
    var rampSeconds: Int = 30                    // hardware-enforced, display only
}

// MARK: VNS + HRV

struct NPVNSHRVParams: Codable, Equatable {
    enum HRVProtocol: String, Codable, CaseIterable, Equatable {
        case standalone
        case tavnsSync
        case eegBiofeedback
        case combinedPBM

        var displayName: String {
            switch self {
            case .standalone:      return "Standalone Coherence Training"
            case .tavnsSync:       return "HRV + taVNS Synchronised"
            case .eegBiofeedback:  return "HRV + EEG Dual Biofeedback"
            case .combinedPBM:     return "HRV + PBM Combined"
            }
        }
    }

    var frequencyHz: Double = 25              // 1–25 Hz
    var intensityMilliamps: Double = 1.5      // ≤2 mA
    var hrvProtocol: HRVProtocol = .standalone
    var resonanceBreathingRate: Double = 6.0  // breaths/min (4–7)
}

// MARK: Audio Entrainment

struct NPAudioEntrainmentParams: Codable, Equatable {
    enum NoiseType: String, Codable, CaseIterable, Equatable {
        case pink
        case brown

        var displayName: String { rawValue.capitalized + " Noise" }
    }

    var binauralBeatsHz: Double? = 20
    var isochronicTonesHz: Double? = nil
    var noiseType: NoiseType? = .pink
    var carrierHz: Double = 440
    var volumePercent: Double = 60
    var eegAdaptive: Bool = true
    var boneConductionPacer: Bool = true
}

// MARK: Visual Stimulation

struct NPVisualStimParams: Codable, Equatable {
    enum VisualMode: String, Codable, CaseIterable, Equatable {
        case binocular
        case emdr
        case retinalPBM
        case modeF

        var displayName: String {
            switch self {
            case .binocular:  return "Binocular Flicker"
            case .emdr:       return "EMDR L/R Alternation"
            case .retinalPBM: return "Retinal PBM"
            case .modeF:      return "Mode F (Invisible NIR)"
            }
        }
    }

    var frequencyHz: Double = 40        // 0.5–100 Hz
    var mode: VisualMode = .binocular
    var emdrCadenceHz: Double = 1.0
    var enableModeF: Bool = false
}

// MARK: T2 — 21-ch qEEG

struct NPqEEG21chParams: Codable, Equatable {
    enum Montage: String, Codable, CaseIterable, Equatable {
        case standard1020 = "standard_1020"
        case custom

        var displayName: String {
            switch self {
            case .standard1020: return "Standard 10-20 + FC3/FC4/Oz/A1/A2"
            case .custom:       return "Custom Montage"
            }
        }
    }

    enum Reference: String, Codable, CaseIterable, Equatable {
        case linkedEar = "linked_ear"
        case cz
        case average

        var displayName: String {
            switch self {
            case .linkedEar: return "Linked Ear (A1+A2)"
            case .cz:        return "Cz Reference"
            case .average:   return "Average Reference"
            }
        }
    }

    var montage: Montage = .standard1020
    var sloretaEnabled: Bool = true
    var reference: Reference = .linkedEar
}

// MARK: T2 — TMS

struct NPTMSParams: Codable, Equatable {
    enum TMSProtocol: String, Codable, CaseIterable, Equatable {
        case rTMS
        case TBS
        case iTBS

        var displayName: String {
            switch self {
            case .rTMS: return "rTMS (Repetitive)"
            case .TBS:  return "TBS (Theta Burst)"
            case .iTBS: return "iTBS (Intermittent TBS)"
            }
        }
    }

    enum TMSTarget: String, Codable, CaseIterable, Equatable {
        case dlpfc_l = "DLPFC_L"
        case dlpfc_r = "DLPFC_R"
        case vlpfc_l = "VLPFC_L"
        case acc     = "ACC"
        case mpfc    = "MPFC"
        case m1_l    = "M1_L"
        case m1_r    = "M1_R"

        var displayName: String {
            switch self {
            case .dlpfc_l: return "Left DLPFC"
            case .dlpfc_r: return "Right DLPFC"
            case .vlpfc_l: return "Left VLPFC"
            case .acc:     return "Anterior Cingulate (ACC)"
            case .mpfc:    return "Medial PFC"
            case .m1_l:    return "Left Motor Cortex (M1)"
            case .m1_r:    return "Right Motor Cortex (M1)"
            }
        }
    }

    var tmsProtocol: TMSProtocol = .rTMS
    var frequencyHz: Double = 10
    var intensityPercentMT: Int = 80
    var target: TMSTarget = .dlpfc_l
    var pulseCount: Int = 1200
}

// MARK: T2 — Deep PBM 1170nm

struct NPDeepPBM1170Params: Codable, Equatable {
    var intensityMWcm2: Double = 500
    var frequencyHz: Double = 40
    var dutyCyclePercent: Int = 25
}

// MARK: T2 — Clinical tACS

struct NPClinicalTacsParams: Codable, Equatable {
    var frequencyHz: Double = 40
    var intensityMilliamps: Double = 2.0   // ≤4 mA
    var channelCount: Int = 8
    var waveform: NPBESTacsParams.Waveform = .sinusoidal
}

// MARK: T2 — HD-tDCS

struct NPHDTdcsParams: Codable, Equatable {
    enum Montage: String, Codable, CaseIterable, Equatable {
        case ring4x1       = "4x1_ring"
        case bilateral4x1  = "bilateral_4x1"
        case standard2el   = "standard_2_electrode"

        var displayName: String {
            switch self {
            case .ring4x1:      return "4×1 Ring (Most Focal)"
            case .bilateral4x1: return "Bilateral 4×1"
            case .standard2el:  return "Standard 2-Electrode"
            }
        }
    }

    var target: NPTMSParams.TMSTarget = .dlpfc_l
    var montage: Montage = .ring4x1
    var intensityMilliamps: Double = 1.5
}

// MARK: T2 — Cervical VNS

struct NPCervicalVnsParams: Codable, Equatable {
    // Note: cardiac interlock is always enforced by safety MCU — not user-configurable
    var frequencyHz: Double = 25
    var intensityMilliamps: Double = 1.5   // ≤2 mA (vs predicate ≤24 mA — more conservative)
}

// MARK: Accessory — Vibrotactile 40Hz

struct NPVibrotactileParams: Codable, Equatable {
    var frequencyHz: Double = 40        // locked at 40Hz, display only
    var intensityG: Double = 0.9        // 0.6–1.2G
    var syncToAudio: Bool = true
    var syncToVisual: Bool = true
}

// MARK: - NPModalityParams (discriminated union)

enum NPModalityParams: Codable, Equatable {

    case pbmTranscranial(NPPBMTranscranialParams)
    case pbmIntranasal(NPPBMIntranasalParams)
    case eegNeurofeedback(NPEEGNeurofeedbackParams)
    case besTacs(NPBESTacsParams)
    case tdcs(NPTDCSParams)
    case vnsHRV(NPVNSHRVParams)
    case audioEntrainment(NPAudioEntrainmentParams)
    case visualStimulation(NPVisualStimParams)
    case qeeg21ch(NPqEEG21chParams)
    case tms(NPTMSParams)
    case pbmDeep1170nm(NPDeepPBM1170Params)
    case clinicalTacs(NPClinicalTacsParams)
    case hdTdcs(NPHDTdcsParams)
    case cervicalVns(NPCervicalVnsParams)
    case vibrotactile40hz(NPVibrotactileParams)

    // MARK: Codable — discriminated union

    private enum CodingKeys: String, CodingKey {
        case type, params
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        switch self {
        case .pbmTranscranial(let p):
            try container.encode("pbm_transcranial", forKey: .type)
            try container.encode(p, forKey: .params)
        case .pbmIntranasal(let p):
            try container.encode("pbm_intranasal", forKey: .type)
            try container.encode(p, forKey: .params)
        case .eegNeurofeedback(let p):
            try container.encode("eeg_neurofeedback", forKey: .type)
            try container.encode(p, forKey: .params)
        case .besTacs(let p):
            try container.encode("bes_tacs", forKey: .type)
            try container.encode(p, forKey: .params)
        case .tdcs(let p):
            try container.encode("tdcs", forKey: .type)
            try container.encode(p, forKey: .params)
        case .vnsHRV(let p):
            try container.encode("vns_hrv", forKey: .type)
            try container.encode(p, forKey: .params)
        case .audioEntrainment(let p):
            try container.encode("audio_entrainment", forKey: .type)
            try container.encode(p, forKey: .params)
        case .visualStimulation(let p):
            try container.encode("visual_stimulation", forKey: .type)
            try container.encode(p, forKey: .params)
        case .qeeg21ch(let p):
            try container.encode("qeeg_21ch", forKey: .type)
            try container.encode(p, forKey: .params)
        case .tms(let p):
            try container.encode("tms", forKey: .type)
            try container.encode(p, forKey: .params)
        case .pbmDeep1170nm(let p):
            try container.encode("pbm_deep_1170nm", forKey: .type)
            try container.encode(p, forKey: .params)
        case .clinicalTacs(let p):
            try container.encode("clinical_tacs", forKey: .type)
            try container.encode(p, forKey: .params)
        case .hdTdcs(let p):
            try container.encode("hd_tdcs", forKey: .type)
            try container.encode(p, forKey: .params)
        case .cervicalVns(let p):
            try container.encode("cervical_vns", forKey: .type)
            try container.encode(p, forKey: .params)
        case .vibrotactile40hz(let p):
            try container.encode("vibrotactile_40hz", forKey: .type)
            try container.encode(p, forKey: .params)
        }
    }

    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        let type = try container.decode(String.self, forKey: .type)
        switch type {
        case "pbm_transcranial":
            self = .pbmTranscranial(try container.decode(NPPBMTranscranialParams.self, forKey: .params))
        case "pbm_intranasal":
            self = .pbmIntranasal(try container.decode(NPPBMIntranasalParams.self, forKey: .params))
        case "eeg_neurofeedback":
            self = .eegNeurofeedback(try container.decode(NPEEGNeurofeedbackParams.self, forKey: .params))
        case "bes_tacs":
            self = .besTacs(try container.decode(NPBESTacsParams.self, forKey: .params))
        case "tdcs":
            self = .tdcs(try container.decode(NPTDCSParams.self, forKey: .params))
        case "vns_hrv":
            self = .vnsHRV(try container.decode(NPVNSHRVParams.self, forKey: .params))
        case "audio_entrainment":
            self = .audioEntrainment(try container.decode(NPAudioEntrainmentParams.self, forKey: .params))
        case "visual_stimulation":
            self = .visualStimulation(try container.decode(NPVisualStimParams.self, forKey: .params))
        case "qeeg_21ch":
            self = .qeeg21ch(try container.decode(NPqEEG21chParams.self, forKey: .params))
        case "tms":
            self = .tms(try container.decode(NPTMSParams.self, forKey: .params))
        case "pbm_deep_1170nm":
            self = .pbmDeep1170nm(try container.decode(NPDeepPBM1170Params.self, forKey: .params))
        case "clinical_tacs":
            self = .clinicalTacs(try container.decode(NPClinicalTacsParams.self, forKey: .params))
        case "hd_tdcs":
            self = .hdTdcs(try container.decode(NPHDTdcsParams.self, forKey: .params))
        case "cervical_vns":
            self = .cervicalVns(try container.decode(NPCervicalVnsParams.self, forKey: .params))
        case "vibrotactile_40hz":
            self = .vibrotactile40hz(try container.decode(NPVibrotactileParams.self, forKey: .params))
        default:
            throw DecodingError.dataCorruptedError(
                forKey: .type,
                in: container,
                debugDescription: "Unknown modality type: \(type)"
            )
        }
    }

    // MARK: Convenience

    var modalityType: NPModalityType {
        switch self {
        case .pbmTranscranial:  return .pbmTranscranial
        case .pbmIntranasal:    return .pbmIntranasal
        case .eegNeurofeedback: return .eegNeurofeedback
        case .besTacs:          return .besTacs
        case .tdcs:             return .tdcs
        case .vnsHRV:           return .vnsHRV
        case .audioEntrainment: return .audioEntrainment
        case .visualStimulation:return .visualStimulation
        case .qeeg21ch:         return .qeeg21ch
        case .tms:              return .tms
        case .pbmDeep1170nm:    return .pbmDeep1170nm
        case .clinicalTacs:     return .clinicalTacs
        case .hdTdcs:           return .hdTdcs
        case .cervicalVns:      return .cervicalVns
        case .vibrotactile40hz: return .vibrotactile40hz
        }
    }

    static func defaultParams(for type: NPModalityType) -> NPModalityParams {
        switch type {
        case .pbmTranscranial:   return .pbmTranscranial(NPPBMTranscranialParams())
        case .pbmIntranasal:     return .pbmIntranasal(NPPBMIntranasalParams())
        case .eegNeurofeedback:  return .eegNeurofeedback(NPEEGNeurofeedbackParams())
        case .besTacs:           return .besTacs(NPBESTacsParams())
        case .tdcs:              return .tdcs(NPTDCSParams())
        case .vnsHRV:            return .vnsHRV(NPVNSHRVParams())
        case .audioEntrainment:  return .audioEntrainment(NPAudioEntrainmentParams())
        case .visualStimulation: return .visualStimulation(NPVisualStimParams())
        case .qeeg21ch:          return .qeeg21ch(NPqEEG21chParams())
        case .tms:               return .tms(NPTMSParams())
        case .pbmDeep1170nm:     return .pbmDeep1170nm(NPDeepPBM1170Params())
        case .clinicalTacs:      return .clinicalTacs(NPClinicalTacsParams())
        case .hdTdcs:            return .hdTdcs(NPHDTdcsParams())
        case .cervicalVns:       return .cervicalVns(NPCervicalVnsParams())
        case .vibrotactile40hz:  return .vibrotactile40hz(NPVibrotactileParams())
        }
    }
}

// MARK: - NPProtocolModality

struct NPProtocolModality: Codable, Identifiable, Equatable {
    var id: UUID = UUID()
    var params: NPModalityParams
    var interval: NPIntervalConfig = .continuous
    var enabled: Bool = true

    var modalityType: NPModalityType { params.modalityType }
}

// MARK: - NPProtocolDefinition

struct NPProtocolDefinition: Codable, Identifiable, Equatable {
    enum TimingMode: Equatable {
        case duration(Int)        // total seconds
        case intervalCount(Int)   // number of interval cycles

        var displayString: String {
            switch self {
            case .duration(let s):
                let m = s / 60
                let sec = s % 60
                return sec == 0 ? "\(m)m" : "\(m)m \(sec)s"
            case .intervalCount(let n):
                return "\(n) intervals"
            }
        }
    }

    var id: UUID = UUID()
    var name: String
    var description: String = ""
    var author: String = "NeuroPulse"
    var version: String = "1.0"
    var tags: [String] = []
    var createdAt: Date = Date()
    var modifiedAt: Date = Date()
    var isPredefined: Bool = false
    var timingMode: TimingMode = .duration(20 * 60)
    var modalities: [NPProtocolModality] = []

    // MARK: Computed

    var requiredModalityTypes: Set<NPModalityType> {
        Set(modalities.filter { $0.enabled }.map { $0.modalityType })
    }

    func requires1064SmartModule() -> Bool {
        modalities.filter { $0.enabled }.contains { mod in
            if case .pbmTranscranial(let p) = mod.params {
                return p.wavelength.requiresSmartModule
            }
            return false
        }
    }

    var totalDurationSeconds: Int? {
        if case .duration(let s) = timingMode { return s }
        return nil
    }

    // MARK: Codable — custom for TimingMode

    private enum CodingKeys: String, CodingKey {
        case id, name, description, author, version, tags
        case createdAt, modifiedAt, isPredefined, modalities
        case timingType, timingValue
    }

    func encode(to encoder: Encoder) throws {
        var c = encoder.container(keyedBy: CodingKeys.self)
        try c.encode(id, forKey: .id)
        try c.encode(name, forKey: .name)
        try c.encode(description, forKey: .description)
        try c.encode(author, forKey: .author)
        try c.encode(version, forKey: .version)
        try c.encode(tags, forKey: .tags)
        try c.encode(createdAt, forKey: .createdAt)
        try c.encode(modifiedAt, forKey: .modifiedAt)
        try c.encode(isPredefined, forKey: .isPredefined)
        try c.encode(modalities, forKey: .modalities)
        switch timingMode {
        case .duration(let v):
            try c.encode("duration", forKey: .timingType)
            try c.encode(v, forKey: .timingValue)
        case .intervalCount(let v):
            try c.encode("interval_count", forKey: .timingType)
            try c.encode(v, forKey: .timingValue)
        }
    }

    init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        id          = try c.decode(UUID.self, forKey: .id)
        name        = try c.decode(String.self, forKey: .name)
        description = try c.decodeIfPresent(String.self, forKey: .description) ?? ""
        author      = try c.decodeIfPresent(String.self, forKey: .author) ?? "NeuroPulse"
        version     = try c.decodeIfPresent(String.self, forKey: .version) ?? "1.0"
        tags        = try c.decodeIfPresent([String].self, forKey: .tags) ?? []
        createdAt   = try c.decodeIfPresent(Date.self, forKey: .createdAt) ?? Date()
        modifiedAt  = try c.decodeIfPresent(Date.self, forKey: .modifiedAt) ?? Date()
        isPredefined = try c.decodeIfPresent(Bool.self, forKey: .isPredefined) ?? false
        modalities  = try c.decodeIfPresent([NPProtocolModality].self, forKey: .modalities) ?? []
        let timingType = try c.decodeIfPresent(String.self, forKey: .timingType) ?? "duration"
        let timingValue = try c.decodeIfPresent(Int.self, forKey: .timingValue) ?? 1200
        if timingType == "interval_count" {
            timingMode = .intervalCount(timingValue)
        } else {
            timingMode = .duration(timingValue)
        }
    }

    init(id: UUID = UUID(),
         name: String,
         description: String = "",
         author: String = "NeuroPulse",
         version: String = "1.0",
         tags: [String] = [],
         createdAt: Date = Date(),
         modifiedAt: Date = Date(),
         isPredefined: Bool = false,
         timingMode: TimingMode = .duration(20 * 60),
         modalities: [NPProtocolModality] = []) {
        self.id = id
        self.name = name
        self.description = description
        self.author = author
        self.version = version
        self.tags = tags
        self.createdAt = createdAt
        self.modifiedAt = modifiedAt
        self.isPredefined = isPredefined
        self.timingMode = timingMode
        self.modalities = modalities
    }
}

// MARK: - NPCompositeLayer

struct NPCompositeLayer: Codable, Identifiable, Equatable {
    var id: UUID = UUID()
    var protocolName: String
    var startOffsetSeconds: Int = 0
    var durationSeconds: Int? = nil   // nil = full protocol duration
    var intensityScale: Double = 1.0
}

// MARK: - NPCompositeProtocol

struct NPCompositeProtocol: Codable, Identifiable, Equatable {
    enum ConflictResolution: String, Codable, CaseIterable, Equatable {
        case merge
        case sequential
        case override

        var displayName: String {
            switch self {
            case .merge:      return "Merge (overlap simultaneous)"
            case .sequential: return "Sequential (no overlap)"
            case .override:   return "Override (later layer wins)"
            }
        }
    }

    var id: UUID = UUID()
    var name: String
    var description: String = ""
    var author: String = "NeuroPulse"
    var version: String = "1.0"
    var tags: [String] = []
    var createdAt: Date = Date()
    var modifiedAt: Date = Date()
    var isPredefined: Bool = false
    var layers: [NPCompositeLayer] = []
    var conflictResolution: ConflictResolution = .merge
}

// MARK: - NPProtocolEntry

enum NPProtocolEntry: Identifiable, Equatable, Codable {
    case single(NPProtocolDefinition)
    case composite(NPCompositeProtocol)
    case limits(NPLimitsSet)

    // MARK: Identifiable
    var id: UUID {
        switch self {
        case .single(let p):    return p.id
        case .composite(let c): return c.id
        case .limits(let l):    return l.id
        }
    }

    var name: String {
        switch self {
        case .single(let p):    return p.name
        case .composite(let c): return c.name
        case .limits(let l):    return l.name
        }
    }

    var description: String {
        switch self {
        case .single(let p):    return p.description
        case .composite(let c): return c.description
        case .limits(let l):    return l.description
        }
    }

    var tags: [String] {
        switch self {
        case .single(let p):    return p.tags
        case .composite(let c): return c.tags
        case .limits:           return []
        }
    }

    var isPredefined: Bool {
        switch self {
        case .single(let p):    return p.isPredefined
        case .composite(let c): return c.isPredefined
        case .limits:           return false
        }
    }

    var isComposite: Bool {
        if case .composite = self { return true }
        return false
    }

    var isLimits: Bool {
        if case .limits = self { return true }
        return false
    }

    var requiredModalityTypes: Set<NPModalityType> {
        switch self {
        case .single(let p):
            return p.requiredModalityTypes
        case .composite:
            // Composite modality requirements are resolved at runtime against the library
            return Set()
        case .limits:
            return Set()
        }
    }

    func duplicated(newName: String) -> NPProtocolEntry {
        switch self {
        case .single(var p):
            p.id = UUID()
            p.name = newName
            p.isPredefined = false
            p.createdAt = Date()
            p.modifiedAt = Date()
            // Re-assign IDs to all modalities
            p.modalities = p.modalities.map { mod in
                var m = mod
                m.id = UUID()
                return m
            }
            return .single(p)
        case .composite(var c):
            c.id = UUID()
            c.name = newName
            c.isPredefined = false
            c.createdAt = Date()
            c.modifiedAt = Date()
            c.layers = c.layers.map { layer in
                var l = layer
                l.id = UUID()
                return l
            }
            return .composite(c)
        case .limits(var l):
            l.id = UUID()
            l.name = newName
            l.createdAt = Date()
            l.modifiedAt = Date()
            return .limits(l)
        }
    }

    // MARK: Codable discriminant

    private enum CodingKeys: String, CodingKey { case type, value }

    func encode(to encoder: Encoder) throws {
        var c = encoder.container(keyedBy: CodingKeys.self)
        switch self {
        case .single(let p):
            try c.encode("single", forKey: .type)
            try c.encode(p, forKey: .value)
        case .composite(let comp):
            try c.encode("composite", forKey: .type)
            try c.encode(comp, forKey: .value)
        case .limits(let lim):
            try c.encode("limits", forKey: .type)
            try c.encode(lim, forKey: .value)
        }
    }

    init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        let type = try c.decode(String.self, forKey: .type)
        switch type {
        case "single":
            self = .single(try c.decode(NPProtocolDefinition.self, forKey: .value))
        case "composite":
            self = .composite(try c.decode(NPCompositeProtocol.self, forKey: .value))
        case "limits":
            self = .limits(try c.decode(NPLimitsSet.self, forKey: .value))
        default:
            throw DecodingError.dataCorruptedError(
                forKey: .type, in: c,
                debugDescription: "Unknown entry type: \(type)"
            )
        }
    }
}
