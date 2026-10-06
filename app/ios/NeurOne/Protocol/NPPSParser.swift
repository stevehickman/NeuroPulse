import Foundation

// NPPS parsing on iOS: the shared NPPS core (common/npps-core, OI-NPPS-CORE-01), through the C ABI.
//
// This used to be a 1,500-line hand-written lexer and parser, one of five that drifted. There is one
// parser now, and it is in Rust: what an `.npps` file means, and what refuses it, is decided there and
// is the same on every runtime. What is left here is not language: building the Swift models from the
// core's JSON, which means supplying what the core leaves out because it is not the same twice (a random
// id for a protocol that states none) and the Swift spelling of each field.

// MARK: - NPPS Error

/// A refusal: the core's message, `Line N: …` when it names a line.
struct NPPSError: Error, LocalizedError {
    let message: String
    let line: Int

    var errorDescription: String? { "Line \(line): \(message)" }
}

// MARK: - Source carrier

/// What the core's parser reads. The lexer is the core's: it runs inside `NPPSParser.parse`, so a lexical
/// refusal is thrown from there rather than from `tokenize()`.
struct NPPSTokens {
    let source: String
}

/// Carries the text from the call sites that still write `NPPSLexer(text)` then `NPPSParser(tokens)` to the
/// core. There is no lexer in Swift any more; this keeps the one place the pair is written.
struct NPPSLexer {
    private let source: String

    init(_ text: String) {
        source = text
    }

    mutating func tokenize() throws -> NPPSTokens {
        NPPSTokens(source: source)
    }
}

// MARK: - Parser

/// Protocols and composites come back in file order, then the file's zones, conditions and limits sets in
/// file order (the core reports each kind in its own list, so their order across kinds is not kept).
/// Wavelength rules are parsed and validated by the core and not consumed here: this runtime compiles
/// against the shipped default rules only.
struct NPPSParser {
    private let source: String

    init(_ tokens: NPPSTokens) {
        source = tokens.source
    }

    mutating func parse() throws -> [NPProtocolEntry] {
        let data: Data
        do {
            data = try NppsCore.parse(source)
        } catch let refusal as NppsCore.Refusal {
            throw Self.error(from: refusal.message)
        }
        guard let parsed = try JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            throw NPPSError(message: "the NPPS core returned something that is not an object", line: 0)
        }
        return try NPPSModels.entries(from: parsed)
    }

    /// `Line N: text` becomes an `NPPSError` with the line; anything else keeps line 0.
    static func error(from message: String) -> NPPSError {
        guard message.hasPrefix("Line "), let colon = message.range(of: ": "),
              let line = Int(message[message.index(message.startIndex, offsetBy: 5)..<colon.lowerBound]) else {
            return NPPSError(message: message, line: 0)
        }
        return NPPSError(message: String(message[colon.upperBound...]), line: line)
    }
}

// MARK: - The core's JSON as models

private typealias JSON = [String: Any]

private extension Dictionary where Key == String, Value == Any {
    func str(_ key: String, _ fallback: String = "") -> String { (self[key] as? String) ?? fallback }
    func optStr(_ key: String) -> String? { self[key] as? String }
    func dbl(_ key: String, _ fallback: Double) -> Double { (self[key] as? NSNumber)?.doubleValue ?? fallback }
    func optDbl(_ key: String) -> Double? { (self[key] as? NSNumber)?.doubleValue }
    func int(_ key: String, _ fallback: Int) -> Int { (self[key] as? NSNumber)?.intValue ?? fallback }
    func optInt(_ key: String) -> Int? { (self[key] as? NSNumber)?.intValue }
    func bool(_ key: String, _ fallback: Bool) -> Bool { (self[key] as? Bool) ?? fallback }
    func optBool(_ key: String) -> Bool? { self[key] as? Bool }
    func strList(_ key: String) -> [String]? { self[key] as? [String] }
    func objects(_ key: String) -> [JSON] { (self[key] as? [JSON]) ?? [] }
    func object(_ key: String) -> JSON? { self[key] as? JSON }
}

enum NPPSModels {

    fileprivate static func entries(from parsed: JSON) throws -> [NPProtocolEntry] {
        var out: [NPProtocolEntry] = []
        for entry in parsed.objects("entries") {
            if let protocolJSON = entry.object("protocol") {
                out.append(.single(try protocolOf(protocolJSON)))
            } else if let compositeJSON = entry.object("composite") {
                out.append(.composite(compositeOf(compositeJSON)))
            }
        }
        for zone in parsed.objects("zones") { out.append(.zone(zoneOf(zone))) }
        for condition in parsed.objects("conditions") { out.append(.condition(conditionOf(condition))) }
        for limits in parsed.objects("limits") { out.append(.limits(limitsOf(limits))) }
        return out
    }

    private static func idOrNew(_ id: String?) -> UUID {
        id.flatMap { UUID(uuidString: $0) } ?? UUID()
    }

    private static func referencesOf(_ json: JSON) -> [NPProtocolReference] {
        (json["references"] as? [Any] ?? []).compactMap { ref in
            if let url = ref as? String { return NPProtocolReference(url: url, label: nil) }
            if let object = ref as? JSON { return NPProtocolReference(url: object.str("url"), label: object.optStr("label")) }
            return nil
        }
    }

    private static func protocolOf(_ json: JSON) throws -> NPProtocolDefinition {
        let timing = json.object("timingMode") ?? [:]
        let mode: NPProtocolDefinition.TimingMode = timing.str("type") == "interval_count"
            ? .intervalCount(timing.int("count", 1))
            : .duration(timing.int("seconds", 20 * 60))
        return NPProtocolDefinition(
            id: idOrNew(json.optStr("id")),
            name: json.str("name"),
            description: json.str("description"),
            author: json.str("author", "NeurOne"),
            version: json.str("version", "1.0"),
            tags: json.strList("tags") ?? [],
            isPredefined: json.bool("isPredefined", false),
            isReadOnly: json.bool("isReadOnly", false),
            timingMode: mode,
            modalities: try json.objects("modalities").map(modalityOf),
            conditions: json.strList("conditions") ?? [],
            references: referencesOf(json)
        )
    }

    private static func compositeOf(_ json: JSON) -> NPCompositeProtocol {
        var composite = NPCompositeProtocol(name: json.str("name"))
        composite.id = idOrNew(json.optStr("id"))
        composite.description = json.str("description")
        composite.author = json.str("author", "NeurOne")
        composite.version = json.str("version", "1.0")
        composite.tags = json.strList("tags") ?? []
        composite.isPredefined = json.bool("isPredefined", false)
        composite.isReadOnly = json.bool("isReadOnly", false)
        composite.layers = json.objects("layers").map { layer in
            NPCompositeLayer(
                protocolName: layer.str("protocolName"),
                startOffsetSeconds: layer.int("startOffsetSeconds", 0),
                durationSeconds: layer.optInt("durationSeconds"),
                intensityScale: layer.dbl("intensityScale", 1.0)
            )
        }
        composite.conflictResolution = NPCompositeProtocol.ConflictResolution(rawValue: json.str("conflictResolution")) ?? .merge
        composite.conditions = json.strList("conditions") ?? []
        composite.references = referencesOf(json)
        return composite
    }

    private static func modalityOf(_ json: JSON) throws -> NPProtocolModality {
        let interval = json.object("interval") ?? [:]
        return NPProtocolModality(
            params: try paramsOf(type: json.str("type"), json.object("params") ?? [:]),
            interval: NPIntervalConfig(
                intervalOnSeconds: interval.int("intervalOnSeconds", 0),
                intervalOffSeconds: interval.int("intervalOffSeconds", 0),
                repeatCount: interval.optInt("repeatCount"),
                startOffsetSeconds: interval.optInt("startOffsetSeconds")
            ),
            enabled: json.bool("enabled", true)
        )
    }

    private static func paramsOf(type: String, _ p: JSON) throws -> NPModalityParams {
        switch type {
        case "pbm_transcranial": return .pbmTranscranial(pbmTranscranial(p))
        case "pbm_intranasal": return .pbmIntranasal(pbmIntranasal(p))
        case "eeg_neurofeedback": return .eegNeurofeedback(eeg(p))
        case "bes_tacs": return .besTacs(besTacs(p))
        case "tdcs": return .tdcs(tdcs(p))
        case "vns_hrv": return .vnsHRV(vnsHrv(p))
        case "audio_entrainment": return .audioEntrainment(audio(p))
        case "visual_stimulation": return .visualStimulation(visual(p))
        case "qeeg_21ch": return .qeeg21ch(qeeg(p))
        case "tms": return .tms(tms(p))
        case "pbm_deep_1170nm": return .pbmDeep1170nm(deep(p))
        case "clinical_tacs": return .clinicalTacs(clinicalTacs(p))
        case "hd_tdcs": return .hdTdcs(hdTdcs(p))
        case "cervical_vns": return .cervicalVns(cervical(p))
        case "vibrotactile_40hz": return .vibrotactile40hz(vibrotactile(p))
        default: throw NPPSError(message: "Unknown modality block type: '\(type)'", line: 0)
        }
    }

    private static func pbmTranscranial(_ p: JSON) -> NPPBMTranscranialParams {
        var out = NPPBMTranscranialParams()
        out.target = p.str("zones") == "clinician_selected" ? .clinicianSelected : .named(p.strList("zoneRefs") ?? ["All"])
        out.wavelength = NPPBMTranscranialParams.Wavelength(rawValue: p.str("wavelength", "808nm"))
        out.irradianceMWcm2 = p.dbl("irradianceMWcm2", 300)
        out.frequencyHz = p.dbl("frequencyHz", 40)
        out.dutyCyclePercent = p.int("dutyCyclePercent", 25)
        return out
    }

    private static func pbmIntranasal(_ p: JSON) -> NPPBMIntranasalParams {
        var out = NPPBMIntranasalParams()
        out.wavelength = NPPBMTranscranialParams.Wavelength(rawValue: p.str("wavelength", "660nm"))
        out.irradianceMWcm2 = p.dbl("irradianceMWcm2", 60)
        out.frequencyHz = p.dbl("frequencyHz", 10)
        out.dutyCyclePercent = p.int("dutyCyclePercent", 50)
        return out
    }

    private static func eeg(_ p: JSON) -> NPEEGNeurofeedbackParams {
        var out = NPEEGNeurofeedbackParams()
        out.channels = NPEEGNeurofeedbackParams.ChannelSelection(rawValue: p.str("channels")) ?? .all
        out.customChannels = p.strList("customChannels")
        switch p.str("band") {
        case "alpha_theta": out.band = .alphaTheta
        case "gamma_theta": out.band = .gammaTheta
        case let band: out.band = NPEEGNeurofeedbackParams.EEGBand(rawValue: band) ?? .alpha
        }
        out.closedLoopEnabled = p.bool("closedLoopEnabled", true)
        return out
    }

    private static func besTacs(_ p: JSON) -> NPBESTacsParams {
        var out = NPBESTacsParams()
        out.frequencyHz = p.dbl("frequencyHz", 40)
        out.intensityMilliamps = p.dbl("intensityMilliamps", 0.5)
        out.waveform = NPBESTacsParams.Waveform(rawValue: p.str("waveform")) ?? .sinusoidal
        return out
    }

    private static func tdcs(_ p: JSON) -> NPTDCSParams {
        var out = NPTDCSParams()
        out.intensityMilliamps = p.dbl("intensityMilliamps", 1.0)
        if let pairs = p["electrodePairs"] as? [[String]] { out.electrodePairs = pairs }
        out.rampSeconds = p.int("rampSeconds", 30)
        out.electrodeAreaCm2 = p.dbl("electrodeAreaCm2", NPHardwareLimits.tdcsDefaultElectrodeAreaCm2)
        return out
    }

    private static func vnsHrv(_ p: JSON) -> NPVNSHRVParams {
        var out = NPVNSHRVParams()
        out.frequencyHz = p.dbl("frequencyHz", 25)
        out.intensityMilliamps = p.dbl("intensityMilliamps", 0.5)
        switch p.str("hrvProtocol") {
        case "tavns_sync": out.hrvProtocol = .tavnsSync
        case "eeg_biofeedback": out.hrvProtocol = .eegBiofeedback
        case "combined_pbm": out.hrvProtocol = .combinedPBM
        case let name: out.hrvProtocol = NPVNSHRVParams.HRVProtocol(rawValue: name) ?? .standalone
        }
        out.resonanceBreathingRate = p.dbl("resonanceBreathingRate", 6)
        return out
    }

    private static func audio(_ p: JSON) -> NPAudioEntrainmentParams {
        var out = NPAudioEntrainmentParams()
        out.binauralBeatsHz = p.optDbl("binauralBeatsHz")
        out.isochronicTonesHz = p.optDbl("isochronicTonesHz")
        out.noiseType = p.optStr("noiseType").flatMap { NPAudioEntrainmentParams.NoiseType(rawValue: $0) }
        out.carrierHz = p.dbl("carrierHz", 200)
        out.volumeDb = p.dbl("volumeDb", 75)
        out.eegAdaptive = p.bool("eegAdaptive", true)
        out.boneConductionPacer = p.bool("boneConductionPacer", false)
        return out
    }

    private static func visual(_ p: JSON) -> NPVisualStimParams {
        var out = NPVisualStimParams()
        out.frequencyHz = p.dbl("frequencyHz", 40)
        out.mode = NPVisualStimParams.VisualMode(rawValue: p.str("mode")) ?? .binocular
        out.emdrCadenceHz = p.dbl("emdrCadenceHz", 1)
        out.enableModeF = p.bool("enableModeF", false)
        return out
    }

    private static func qeeg(_ p: JSON) -> NPqEEG21chParams {
        var out = NPqEEG21chParams()
        out.montage = NPqEEG21chParams.Montage(rawValue: p.str("montage")) ?? .standard1020
        out.sloretaEnabled = p.bool("sloretaEnabled", true)
        out.reference = NPqEEG21chParams.Reference(rawValue: p.str("reference")) ?? .linkedEar
        return out
    }

    private static func tms(_ p: JSON) -> NPTMSParams {
        var out = NPTMSParams()
        out.tmsProtocol = NPTMSParams.TMSProtocol(rawValue: p.str("tmsProtocol")) ?? .rTMS
        out.frequencyHz = p.dbl("frequencyHz", 10)
        out.intensityPercentMT = p.int("intensityPercentMT", 110)
        out.target = NPTMSParams.TMSTarget(rawValue: p.str("target")) ?? .dlpfc_l
        out.pulseCount = p.int("pulseCount", 3000)
        return out
    }

    private static func deep(_ p: JSON) -> NPDeepPBM1170Params {
        var out = NPDeepPBM1170Params()
        out.intensityMWcm2 = p.dbl("intensityMWcm2", 500)
        out.frequencyHz = p.dbl("frequencyHz", 10)
        out.dutyCyclePercent = p.int("dutyCyclePercent", 50)
        return out
    }

    private static func clinicalTacs(_ p: JSON) -> NPClinicalTacsParams {
        var out = NPClinicalTacsParams()
        out.frequencyHz = p.dbl("frequencyHz", 40)
        out.intensityMilliamps = p.dbl("intensityMilliamps", 2)
        out.channelCount = p.int("channelCount", 8)
        out.waveform = NPBESTacsParams.Waveform(rawValue: p.str("waveform")) ?? .sinusoidal
        return out
    }

    private static func hdTdcs(_ p: JSON) -> NPHDTdcsParams {
        var out = NPHDTdcsParams()
        out.target = NPTMSParams.TMSTarget(rawValue: p.str("target")) ?? .dlpfc_l
        out.montage = NPHDTdcsParams.Montage(rawValue: p.str("montage")) ?? .ring4x1
        out.intensityMilliamps = p.dbl("intensityMilliamps", 1.5)
        return out
    }

    private static func cervical(_ p: JSON) -> NPCervicalVnsParams {
        var out = NPCervicalVnsParams()
        out.frequencyHz = p.dbl("frequencyHz", 25)
        out.intensityMilliamps = p.dbl("intensityMilliamps", 1.0)
        return out
    }

    private static func vibrotactile(_ p: JSON) -> NPVibrotactileParams {
        var out = NPVibrotactileParams()
        out.intensityG = p.dbl("intensityG", 0.9)
        out.syncToAudio = p.bool("syncToAudio", true)
        out.syncToVisual = p.bool("syncToVisual", true)
        return out
    }

    // MARK: zones, conditions, limits

    private static func zoneOf(_ json: JSON) -> NPZoneDefinition {
        var zone = NPZoneDefinition(name: json.str("name"), sockets: (json["sockets"] as? [NSNumber])?.map { $0.intValue } ?? [])
        zone.id = json.optStr("id")
        zone.description = json.optStr("description")
        zone.types = json.strList("types")
        zone.excludeTypes = json.bool("excludeTypes", false)
        zone.isPredefined = json.bool("isPredefined", false)
        return zone
    }

    private static func conditionOf(_ json: JSON) -> NPConditionDefinition {
        NPConditionDefinition(
            name: json.str("name"), id: json.optStr("id"), link: json.str("link"),
            code: json.optStr("code"), description: json.optStr("description")
        )
    }

    private static func limitsOf(_ json: JSON) -> NPLimitsSet {
        let level = NPLimitsSet.LimitLevel(rawValue: json.str("level")) ?? .global
        var set = NPLimitsSet(name: json.str("name"), level: level)
        set.description = json.str("description")
        set.helmetId = json.optStr("helmetId")
        set.individualId = json.optStr("individualId").flatMap { UUID(uuidString: $0) }
        set.pbmTranscranial = json.object("pbmTranscranial").map(pbmTranscranialLimits)
        set.pbmIntranasal = json.object("pbmIntranasal").map(pbmIntranasalLimits)
        set.eegNeurofeedback = json.object("eegNeurofeedback").map {
            NPEEGNeurofeedbackLimits(allowedBands: $0.strList("allowedBands"), requireClosedLoop: $0.optBool("requireClosedLoop"))
        }
        set.besTacs = json.object("besTacs").map(besTacsLimits)
        set.tdcs = json.object("tdcs").map {
            NPTDCSLimits(maxIntensityMilliamps: $0.optDbl("maxIntensityMilliamps"),
                         maxSessionDurationSeconds: $0.optInt("maxSessionDurationSeconds"),
                         maxSessionsPerDay: $0.optInt("maxSessionsPerDay"))
        }
        set.vnsHrv = json.object("vnsHrv").map {
            NPVNSHRVLimits(maxIntensityMilliamps: $0.optDbl("maxIntensityMilliamps"), maxFrequencyHz: $0.optDbl("maxFrequencyHz"),
                           maxSessionDurationSeconds: $0.optInt("maxSessionDurationSeconds"),
                           allowedProtocols: $0.strList("allowedProtocols"))
        }
        set.audioEntrainment = json.object("audioEntrainment").map {
            NPAudioEntrainmentLimits(maxVolumeDb: $0.optDbl("maxVolumeDb"), maxBinauralBeatsHz: $0.optDbl("maxBinauralBeatsHz"),
                                     maxIsochronicTonesHz: $0.optDbl("maxIsochronicTonesHz"))
        }
        set.visualStimulation = json.object("visualStimulation").map {
            NPVisualStimLimits(maxFrequencyHz: $0.optDbl("maxFrequencyHz"), minFrequencyHz: $0.optDbl("minFrequencyHz"),
                               allowedModes: $0.strList("allowedModes"), blockHighRiskRange: $0.optBool("blockHighRiskRange"))
        }
        set.tms = json.object("tms").map(tmsLimits)
        set.pbmDeep1170nm = json.object("pbmDeep1170nm").map {
            NPDeepPBMLimits(maxIntensityMWcm2: $0.optDbl("maxIntensityMWcm2"),
                            maxSessionDurationSeconds: $0.optInt("maxSessionDurationSeconds"))
        }
        set.clinicalTacs = json.object("clinicalTacs").map {
            NPClinicalTacsLimits(maxIntensityMilliamps: $0.optDbl("maxIntensityMilliamps"),
                                 maxSessionDurationSeconds: $0.optInt("maxSessionDurationSeconds"))
        }
        set.hdTdcs = json.object("hdTdcs").map {
            NPHDTdcsLimits(maxIntensityMilliamps: $0.optDbl("maxIntensityMilliamps"),
                           maxSessionDurationSeconds: $0.optInt("maxSessionDurationSeconds"),
                           allowedMontages: $0.strList("allowedMontages"))
        }
        set.cervicalVns = json.object("cervicalVns").map {
            NPCervicalVnsLimits(maxIntensityMilliamps: $0.optDbl("maxIntensityMilliamps"),
                                maxSessionDurationSeconds: $0.optInt("maxSessionDurationSeconds"))
        }
        set.vibrotactile40hz = json.object("vibrotactile40hz").map {
            NPVibrotactileLimits(maxIntensityG: $0.optDbl("maxIntensityG"),
                                 maxSessionDurationSeconds: $0.optInt("maxSessionDurationSeconds"))
        }
        return set
    }

    private static func pbmTranscranialLimits(_ l: JSON) -> NPPBMTranscranialLimits {
        NPPBMTranscranialLimits(
            maxIrradianceMWcm2: l.optDbl("maxIrradianceMWcm2"), maxFrequencyHz: l.optDbl("maxFrequencyHz"),
            maxDutyCyclePercent: l.optInt("maxDutyCyclePercent"), maxSessionDoseJCm2: l.optDbl("maxSessionDoseJCm2"),
            maxDailyDoseJCm2: l.optDbl("maxDailyDoseJCm2")
        )
    }

    private static func pbmIntranasalLimits(_ l: JSON) -> NPPBMIntranasalLimits {
        NPPBMIntranasalLimits(
            maxIrradianceMWcm2: l.optDbl("maxIrradianceMWcm2"), maxSessionDoseJCm2: l.optDbl("maxSessionDoseJCm2"),
            maxSessionDurationSeconds: l.optInt("maxSessionDurationSeconds")
        )
    }

    private static func besTacsLimits(_ l: JSON) -> NPBESTacsLimits {
        NPBESTacsLimits(
            maxIntensityMilliamps: l.optDbl("maxIntensityMilliamps"), maxFrequencyHz: l.optDbl("maxFrequencyHz"),
            minFrequencyHz: l.optDbl("minFrequencyHz"), maxSessionDurationSeconds: l.optInt("maxSessionDurationSeconds"),
            maxSessionsPerDay: l.optInt("maxSessionsPerDay")
        )
    }

    private static func tmsLimits(_ l: JSON) -> NPTMSLimits {
        NPTMSLimits(
            maxIntensityPercentMT: l.optInt("maxIntensityPercentMT"), maxPulsesPerSession: l.optInt("maxPulsesPerSession"),
            maxPulsesPerDay: l.optInt("maxPulsesPerDay"), maxSessionsPerWeek: l.optInt("maxSessionsPerWeek"),
            allowedProtocols: l.strList("allowedProtocols"), allowedTargets: l.strList("allowedTargets")
        )
    }
}
