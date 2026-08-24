import CryptoKit
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

/// Where a PBM transcranial command lands on the helmet lattice.
///
/// Mirrors the web app's `PBMTranscranialParams.zones` + `zoneRefs`
/// (app/web/src/types/protocol.ts), expressed as a sum type because the payload
/// differs per case and "selector plus an optional side field" made invalid
/// combinations representable.
///
/// There are exactly two ways to name a target and no third. The five-slot
/// selectors (`all` / `front` / `rear` / numeric indices) are gone rather than
/// retained-but-refused: there are no existing users, so nothing needs to keep
/// parsing them. Should a corpus of old files ever need moving, that is a
/// one-shot conversion tool, not a permanent branch in the parser.
enum NPPBMTarget: Codable, Equatable {

    /// Named zones from protocols/predefined/00-zones.npps, as authored. Zones
    /// overlap at the midline by design; the mask deduplicates.
    case named([String])

    /// The target is patient-specific and CANNOT be predefined — the operator
    /// chooses sockets before the protocol can run (e.g. perilesional cortex in
    /// post-stroke rehab).
    case clinicianSelected
}

// MARK: PBM target resolution

extension NPPBMTarget {

    /// Resolve to the firmware socket bitmap, or throw with a message naming what
    /// to fix.
    ///
    /// - Parameter clinicianSockets: operator-chosen 1-based socket ids, required
    ///   only by `.clinicianSelected`.
    func resolve(clinicianSockets: [Int]? = nil) throws -> NPSocketMask {
        switch self {
        case .named(let names):
            guard !names.isEmpty else {
                throw NPSocketTargetError.emptyTarget(target: "zones: []")
            }
            // Union across zones, then build one mask: two zones sharing a midline
            // socket must dose it once, and a bit set twice is still one bit.
            var sockets: [Int] = []
            for name in names {
                // From the loaded .npps namespace — the only source of a zone
                // (NP-NPPS-REF-001 §8). A user-defined zone resolves here on
                // exactly the same footing as a shipped one.
                guard let zoneSockets = NPZoneRegistry.sockets(forZone: name) else {
                    throw NPSocketTargetError.unknownZone(name: name)
                }
                sockets.append(contentsOf: zoneSockets)
            }
            let mask = try NPSocketMask(
                sockets: sockets,
                source: "zone \(names.map { "\"\($0)\"" }.joined(separator: " + "))"
            )
            guard !mask.isEmpty else {
                throw NPSocketTargetError.emptyTarget(target: "zones: \(names.joined(separator: ", "))")
            }
            return mask

        case .clinicianSelected:
            guard let chosen = clinicianSockets, !chosen.isEmpty else {
                throw NPSocketTargetError.clinicianSelectionMissing
            }
            return try NPSocketMask(sockets: chosen, source: "the operator's selection")
        }
    }

    /// Short human-readable form, for pickers and validation messages.
    var displayName: String {
        switch self {
        case .named(let names):
            return names.isEmpty ? "No zones" : names.joined(separator: " + ")
        case .clinicianSelected:
            return "Clinician-selected sockets"
        }
    }
}

struct NPPBMTranscranialParams: Codable, Equatable {

    enum Wavelength: String, Codable, CaseIterable, Equatable, Identifiable {
        var id: String { rawValue }
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

    /// Defaults to the whole vault. A default that resolves is deliberate: the
    /// old default was the five-slot `.all`, which silently became "every module"
    /// for any target the parser did not recognise.
    var target: NPPBMTarget = .named(["All"])
    var wavelength: Wavelength = .base660_808nm
    var intensityPercent: Double = 75
    var frequencyHz: Double = 20        // 0 = CW
    var dutyCyclePercent: Int = 25      // ≤25, only shown when frequencyHz > 0

    /// The sockets this modality drives, as the firmware bitmap. Throws — with a
    /// message naming the zone or selector at fault — rather than falling back to
    /// any default, because a silently-substituted target is wrong-site
    /// stimulation.
    func resolveSocketMask(clinicianSockets: [Int]? = nil) throws -> NPSocketMask {
        try target.resolve(clinicianSockets: clinicianSockets)
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
    enum ChannelSelection: String, Codable, CaseIterable, Equatable, Identifiable {
        var id: String { rawValue }
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

    enum EEGBand: String, Codable, CaseIterable, Equatable, Identifiable {
        var id: String { rawValue }
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
    enum Waveform: String, Codable, CaseIterable, Equatable, Identifiable {
        var id: String { rawValue }
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
    enum HRVProtocol: String, Codable, CaseIterable, Equatable, Identifiable {
        var id: String { rawValue }
        case standalone
        case tavnsSync
        case eegBiofeedback
        case combinedPBM

        var displayName: String {
            switch self {
            case .standalone:      return "Standalone Coherence Training"
            case .tavnsSync:       return "HRV + taVNS Synchronized"
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
    enum NoiseType: String, Codable, CaseIterable, Equatable, Identifiable {
        var id: String { rawValue }
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
    /// The four visual delivery modes (NP-NPPS-REF-001 §4.8).
    ///
    /// `rawValue` is the **NPPS token**, which is what the parser reads and the
    /// serializer writes. It used to be the Swift case name — `retinalPBM`,
    /// `modeF` — while the parser only accepted `retinal_pbm` and `mode_f`, so
    /// those two modes serialized to text this parser could not read back.
    ///
    /// The session descriptor uses a *different* vocabulary; see
    /// ``sessionWireName``. Conflating the two is what caused the bug.
    enum VisualMode: String, Codable, CaseIterable, Equatable, Identifiable {
        var id: String { rawValue }
        case binocular  = "binocular"
        case emdr       = "emdr"
        case retinalPBM = "retinal_pbm"
        case modeF      = "mode_f"

        var displayName: String {
            switch self {
            case .binocular:  return "Binocular Flicker"
            case .emdr:       return "EMDR L/R Alternation"
            case .retinalPBM: return "Retinal PBM"
            case .modeF:      return "Mode F (Invisible NIR)"
            }
        }

        /// The mode name in the session descriptor, whose vocabulary is
        /// `binocular` / `emdr` / `retinalPBM` (see `SessionProtocol.mode`) —
        /// not the NPPS token. Mode F rides the retinalPBM path with the NIR
        /// flag set, exactly as the Windows compiler already maps it, so it has
        /// no separate name here; `modeF` was never a legal value in that field.
        var sessionWireName: String {
            switch self {
            case .binocular:            return "binocular"
            case .emdr:                 return "emdr"
            case .retinalPBM, .modeF:   return "retinalPBM"
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
    enum Montage: String, Codable, CaseIterable, Equatable, Identifiable {
        var id: String { rawValue }
        case standard1020 = "standard_1020"
        case custom

        var displayName: String {
            switch self {
            case .standard1020: return "Standard 10-20 + FC3/FC4/Oz/A1/A2"
            case .custom:       return "Custom Montage"
            }
        }
    }

    enum Reference: String, Codable, CaseIterable, Equatable, Identifiable {
        var id: String { rawValue }
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
    enum TMSProtocol: String, Codable, CaseIterable, Equatable, Identifiable {
        var id: String { rawValue }
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

    enum TMSTarget: String, Codable, CaseIterable, Equatable, Identifiable {
        var id: String { rawValue }
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
    enum Montage: String, Codable, CaseIterable, Equatable, Identifiable {
        var id: String { rawValue }
        case ring4x1       = "ring_4x1"
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
    var author: String = "NeurOne"
    var version: String = "1.0"
    var tags: [String] = []
    var createdAt: Date = Date()
    var modifiedAt: Date = Date()
    var isPredefined: Bool = false
    var isReadOnly: Bool = false
    var timingMode: TimingMode = .duration(20 * 60)
    var modalities: [NPProtocolModality] = []

    /// Clinical conditions this protocol targets (§3). Each name must resolve
    /// to a loaded `condition` block — see `validateNamespaceReferences`.
    var conditions: [String] = []

    /// Evidence and applicability links (§3).
    var references: [NPProtocolReference] = []

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

    /// True when the protocol reads the user's EEG to record neurofeedback or to
    /// close the stimulation loop — i.e. it depends on brainwave data. Gated on the
    /// BIPA biometric written release (ISC-90): a user who declines consent must not
    /// be able to run any of these. Covers the explicit EEG/qEEG modalities plus the
    /// three EEG-adaptive couplings (EEG closed-loop, audio EEG-adaptive, HRV+EEG).
    var isEEGDependent: Bool {
        modalities.filter { $0.enabled }.contains { mod in
            switch mod.params {
            case .eegNeurofeedback, .qeeg21ch:
                return true                                  // any EEG recording is brainwave data
            case .audioEntrainment(let p):
                return p.eegAdaptive
            case .vnsHRV(let p):
                return p.hrvProtocol == .eegBiofeedback
            default:
                return false
            }
        }
    }

    var totalDurationSeconds: Int? {
        if case .duration(let s) = timingMode { return s }
        return nil
    }

    // MARK: Codable — custom for TimingMode

    private enum CodingKeys: String, CodingKey {
        case id, name, description, author, version, tags
        case createdAt, modifiedAt, isPredefined, isReadOnly, modalities
        case timingType, timingValue
        case conditions, references
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
        try c.encode(isReadOnly, forKey: .isReadOnly)
        try c.encode(modalities, forKey: .modalities)
        try c.encode(conditions, forKey: .conditions)
        try c.encode(references, forKey: .references)
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
        author      = try c.decodeIfPresent(String.self, forKey: .author) ?? "NeurOne"
        version     = try c.decodeIfPresent(String.self, forKey: .version) ?? "1.0"
        tags        = try c.decodeIfPresent([String].self, forKey: .tags) ?? []
        createdAt   = try c.decodeIfPresent(Date.self, forKey: .createdAt) ?? Date()
        modifiedAt  = try c.decodeIfPresent(Date.self, forKey: .modifiedAt) ?? Date()
        isPredefined = try c.decodeIfPresent(Bool.self, forKey: .isPredefined) ?? false
        isReadOnly   = try c.decodeIfPresent(Bool.self, forKey: .isReadOnly) ?? false
        modalities  = try c.decodeIfPresent([NPProtocolModality].self, forKey: .modalities) ?? []
        // decodeIfPresent: entries persisted before these fields existed decode
        // to empty rather than failing the whole protocol.
        conditions  = try c.decodeIfPresent([String].self, forKey: .conditions) ?? []
        references  = try c.decodeIfPresent([NPProtocolReference].self, forKey: .references) ?? []
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
         author: String = "NeurOne",
         version: String = "1.0",
         tags: [String] = [],
         createdAt: Date = Date(),
         modifiedAt: Date = Date(),
         isPredefined: Bool = false,
         isReadOnly: Bool = false,
         timingMode: TimingMode = .duration(20 * 60),
         modalities: [NPProtocolModality] = [],
         conditions: [String] = [],
         references: [NPProtocolReference] = []) {
        self.id = id
        self.name = name
        self.description = description
        self.author = author
        self.version = version
        self.tags = tags
        self.createdAt = createdAt
        self.modifiedAt = modifiedAt
        self.isPredefined = isPredefined
        self.isReadOnly = isReadOnly
        self.timingMode = timingMode
        self.modalities = modalities
        self.conditions = conditions
        self.references = references
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
    enum ConflictResolution: String, Codable, CaseIterable, Equatable, Identifiable {
        var id: String { rawValue }
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
    var author: String = "NeurOne"
    var version: String = "1.0"
    var tags: [String] = []
    var createdAt: Date = Date()
    var modifiedAt: Date = Date()
    var isPredefined: Bool = false
    var isReadOnly: Bool = false
    var layers: [NPCompositeLayer] = []
    var conflictResolution: ConflictResolution = .merge
    var conditions: [String] = []
    var references: [NPProtocolReference] = []
}

// MARK: - NPProtocolEntry

// MARK: - Zone definition (named set of modules)

/// A zone is a named SET OF MODULES, defined as an explicit list of socket
/// (major) addresses. Listing sockets directly makes arbitrary, non-contiguous
/// zones definable; an optional element-type filter restricts which elements
/// within those modules the zone selects. See NP-NPPS-REF-001 §8.
///
/// `sockets` is a SET: duplicates collapse and the list is sorted at parse
/// time, so unioning the two hemisphere zones of a lobe counts a shared midline
/// socket once.
struct NPZoneDefinition: Codable, Equatable {
    var name: String
    var sockets: [Int]
    var id: String?
    var description: String?
    /// Element-type names mirror the firmware `np_elem_type_t`. Unknown names
    /// are preserved rather than rejected: the element table is
    /// hardware-revision data, so an unfamiliar type is forward compatibility.
    var types: [String]?
    var excludeTypes: Bool = false
    /// Presence of an `id` marks the zone as shipped/read-only.
    var isPredefined: Bool = false
}

// NOTE: `NPConditionDefinition` is NOT declared here. It already exists in
// NPConditionDefinition.swift, alongside NPResolvedCondition and the link
// policy that consumes it; a second declaration here made the name ambiguous
// for every file that referenced it. The parser builds instances of that type.

/// A `references` entry: a bare URL/path, or a labelled link (§2).
struct NPProtocolReference: Codable, Equatable {
    var url: String
    var label: String?

    /// What a link should read as: the label when one was authored.
    var displayText: String { label ?? url }
}

enum NPProtocolEntry: Identifiable, Equatable, Codable {
    case single(NPProtocolDefinition)
    case composite(NPCompositeProtocol)
    case limits(NPLimitsSet)
    /// A `zone` block: a named set of modules (NP-NPPS-REF-001 §8).
    case zone(NPZoneDefinition)
    /// A `condition` block: name -> external definition link (§9).
    case condition(NPConditionDefinition)

    // MARK: Identifiable
    var id: UUID {
        switch self {
        case .single(let p):    return p.id
        case .composite(let c): return c.id
        case .limits(let l):    return l.id
        // Definitions are namespace entries keyed by name, never library items,
        // so a stable synthetic id derived from the name is enough.
        case .zone(let z):      return NPProtocolEntry.syntheticID("zone:\(z.name)")
        case .condition(let c): return NPProtocolEntry.syntheticID("condition:\(c.name)")
        }
    }

    var name: String {
        switch self {
        case .single(let p):    return p.name
        case .composite(let c): return c.name
        case .limits(let l):    return l.name
        case .zone(let z):      return z.name
        case .condition(let c): return c.name
        }
    }

    var description: String {
        switch self {
        case .single(let p):    return p.description
        case .composite(let c): return c.description
        case .limits(let l):    return l.description
        case .zone(let z):      return z.description ?? ""
        case .condition(let c): return c.description ?? ""
        }
    }

    var tags: [String] {
        switch self {
        case .single(let p):    return p.tags
        case .composite(let c): return c.tags
        case .limits, .zone, .condition: return []
        }
    }

    var isPredefined: Bool {
        switch self {
        case .single(let p):    return p.isPredefined
        case .composite(let c): return c.isPredefined
        case .limits:           return false
        // A shipped definition carries an id; a user-authored one does not.
        case .zone(let z):      return z.isPredefined
        case .condition(let c): return c.id != nil
        }
    }

    var isReadOnly: Bool {
        switch self {
        case .single(let p):    return p.isReadOnly
        case .composite(let c): return c.isReadOnly
        case .limits:           return false
        case .zone(let z):      return z.isPredefined
        case .condition(let c): return c.id != nil
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

    /// True for `zone` and `condition` entries: namespace definitions that are
    /// referenced by name and never appear in a protocol list.
    var isDefinition: Bool {
        switch self {
        case .zone, .condition: return true
        default:                return false
        }
    }

    /// Deterministic id for a definition, so the same name always yields the
    /// same value across launches. UUID v5-style: a namespaced SHA-256 prefix.
    private static func syntheticID(_ key: String) -> UUID {
        var bytes = Array(SHA256.hash(data: Data(key.utf8)).prefix(16))
        bytes[6] = (bytes[6] & 0x0F) | 0x50   // version 5
        bytes[8] = (bytes[8] & 0x3F) | 0x80   // RFC 4122 variant
        return UUID(uuid: (
            bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
            bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]
        ))
    }

    var requiredModalityTypes: Set<NPModalityType> {
        switch self {
        case .single(let p):
            return p.requiredModalityTypes
        case .composite:
            // Composite modality requirements are resolved at runtime against the library
            return Set()
        case .limits, .zone, .condition:
            // A definition is referenced by protocols, never run.
            return Set()
        }
    }

    /// EEG-dependency for a single protocol. Composites resolve their members at
    /// runtime against the library (see `NPProtocolLibrary.isEEGDependent(_:)`),
    /// so this returns false for the `.composite` case on its own.
    var isEEGDependent: Bool {
        switch self {
        case .single(let p): return p.isEEGDependent
        case .composite:     return false
        case .limits, .zone, .condition: return false
        }
    }

    func duplicated(newName: String) -> NPProtocolEntry {
        switch self {
        case .single(var p):
            p.id = UUID()
            p.name = newName
            p.isPredefined = false
            p.isReadOnly = false
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
            c.isReadOnly = false
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
        case .zone(var z):
            // A definition is keyed by name, so duplicating one means renaming
            // it; there is no id to re-mint, and the copy is user-authored, so
            // it loses the shipped marker.
            z.name = newName
            z.id = nil
            z.isPredefined = false
            return .zone(z)
        case .condition(let c):
            return .condition(NPConditionDefinition(
                name: newName, id: nil, link: c.link, code: c.code, description: c.description
            ))
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
        case .zone(let zone):
            try c.encode("zone", forKey: .type)
            try c.encode(zone, forKey: .value)
        case .condition(let cond):
            try c.encode("condition", forKey: .type)
            try c.encode(cond, forKey: .value)
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
        case "zone":
            self = .zone(try c.decode(NPZoneDefinition.self, forKey: .value))
        case "condition":
            self = .condition(try c.decode(NPConditionDefinition.self, forKey: .value))
        default:
            throw DecodingError.dataCorruptedError(
                forKey: .type, in: c,
                debugDescription: "Unknown entry type: \(type)"
            )
        }
    }
}
