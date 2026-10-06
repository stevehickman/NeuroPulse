import Foundation
import NeurOneNppsCore

// The shared NPPS core (common/npps-core, OI-NPPS-CORE-01) as iOS sees it: one lexer, parser and
// hub-descriptor compiler written once in Rust, linked as the static XCFramework that
// scripts/build-npps-xcframework.sh builds from common/npps-ffi. This file marshals and nothing
// else. What an input means is decided in the core, so it means the same here as on every other
// runtime, and a refusal carries the message the web parser or compiler gives.
//
// HubDescriptorCompiler calls this for every descriptor it writes, and NPPSParser (Protocol/NPPSParser.swift) for
// every `.npps` file it reads.

enum NppsCore {
    /// The core refused the input. `message` is the web reference's, `Line N: …` for a parse.
    struct Refusal: Error, LocalizedError, Equatable {
        let message: String
        var errorDescription: String? { message }
    }

    /// What a compile needs that is not in the protocol. Everything non-deterministic is an input.
    struct CompileOptions {
        /// Zone name → its 1-based socket ids.
        var zones: [String: [Int]]?
        var clinicianSockets: [Int]?
        var deviceSerial: Data?
        var nowUnix: Int
        var sessionUUID: Data
        /// Channel windows overriding the shipped defaults, as the core's JSON; nil keeps the defaults.
        var wavelengthRules: [String: Any]?
        /// Mode 3: sets NP_PROTO_FLAG_AUTONOMOUS in the header.
        var autonomous = false
    }

    private typealias Entry = (
        UnsafePointer<UInt8>?, Int, UnsafeMutablePointer<UnsafeMutablePointer<UInt8>?>?, UnsafeMutablePointer<Int>?
    ) -> Int32

    /// Parse NPPS text into everything the file declares, as the core's JSON: `{"entries":[{"kind":"single",
    /// "protocol":{…}} | {"kind":"composite","composite":{…}}], "zones":[…], "conditions":[…],
    /// "wavelengthRules":[…], "limits":[…]}`. Ids and timestamps are left out; the caller supplies them.
    static func parse(_ source: String) throws -> Data {
        try call(npps_parse_json, Data(source.utf8))
    }

    /// Fold parse results (`{"files":[…]}`, in load order) into one namespace and check its references:
    /// `{"entries", "zones", "conditions", "errors", "referenceErrors"}`. A name two files define is left
    /// undefined and reported in `errors`.
    static func namespace(requestJSON: Data) throws -> Data {
        try call(npps_namespace_json, requestJSON)
    }

    /// Write models as `.npps` text (`{"items":[…]}`, each item one of the shapes `parse` returns: `{"kind":"single",
    /// "protocol":{…}}`, `{"kind":"composite",…}`, `{"kind":"zone",…}`, `{"kind":"condition",…}`,
    /// `{"kind":"wavelengthRules",…}`, `{"kind":"limits",…}`). Items are separated by a blank line. A model that cannot
    /// be written (a zone holding an id that is not a socket) is refused with the core's message rather than written
    /// into a file the parser would reject.
    static func serialize(requestJSON: Data) throws -> Data {
        try call(npps_serialize_json, requestJSON)
    }

    /// Validate an entry against resolved limits: `{"issues":[…], "isValid", "hasWarnings"}`. The core returns locale
    /// keys and arguments, never text; `NPValidationText` (NPProtocolValidator.swift) turns them into words. The
    /// request is `{"entry":…, "limits":…, "allProtocols":[…]|null, "zones":{…}|null, "limitSources":{…}|null}`.
    static func validate(requestJSON: Data) throws -> Data {
        try call(npps_validate_json, requestJSON)
    }

    /// Resolve three limit sets (nil for a tier that does not exist), most specific first, field by field, and say which tier
    /// each value came from. `{"limits": {level, <modality blocks>}, "sources": {<modalityProperty>: {<limitField>: tier}}}`
    /// is decoded into the app's types.
    static func resolveLimits(
        global: NPLimitsSet?, helmet: NPLimitsSet?, individual: NPLimitsSet?
    ) throws -> (limits: NPLimitsSet, sources: NPLimitSourceMap) {
        let request: [String: Any] = [
            "global": global?.nppsCoreJSON() ?? NSNull(),
            "helmet": helmet?.nppsCoreJSON() ?? NSNull(),
            "individual": individual?.nppsCoreJSON() ?? NSNull()
        ]
        let out = try call(npps_resolve_limits_json, JSONSerialization.data(withJSONObject: request))
        let result = try JSONDecoder().decode(NppsResolvedLimits.self, from: out)
        var limits = NPLimitsSet(name: "Resolved", level: .global)
        let blocks = result.limits
        limits.pbmTranscranial = blocks.pbmTranscranial
        limits.pbmIntranasal = blocks.pbmIntranasal
        limits.eegNeurofeedback = blocks.eegNeurofeedback
        limits.besTacs = blocks.besTacs
        limits.tdcs = blocks.tdcs
        limits.vnsHrv = blocks.vnsHrv
        limits.audioEntrainment = blocks.audioEntrainment
        limits.visualStimulation = blocks.visualStimulation
        limits.tms = blocks.tms
        limits.pbmDeep1170nm = blocks.pbmDeep1170nm
        limits.clinicalTacs = blocks.clinicalTacs
        limits.hdTdcs = blocks.hdTdcs
        limits.cervicalVns = blocks.cervicalVns
        limits.vibrotactile40hz = blocks.vibrotactile40hz
        return (limits, result.sources)
    }

    /// Validate through the models: builds the request from the app's types (`NppsCoreMapping.swift`) and returns the
    /// core's result object (`{"issues":[…], "isValid", "hasWarnings"}`). `library` is what a composite's layers resolve
    /// against, nil to skip that check; `zones` is the namespace a PBM block's named zones resolve against.
    static func validate(
        entry: NPProtocolEntry, limits: NPLimitsSet, library: [NPProtocolEntry]?,
        zones: [String: [Int]], limitSources: NPLimitSourceMap
    ) throws -> [String: Any] {
        let request: [String: Any] = [
            "entry": entry.nppsCoreItem(),
            "limits": limits.nppsCoreJSON(),
            "allProtocols": library.map { $0.map { $0.nppsCoreItem() } as Any } ?? NSNull(),
            "zones": zones,
            "limitSources": limitSources.nppsCoreJSON()
        ]
        let out = try validate(requestJSON: JSONSerialization.data(withJSONObject: request))
        guard let json = try JSONSerialization.jsonObject(with: out) as? [String: Any] else {
            throw Refusal(message: "the validator returned something that is not a result")
        }
        return json
    }

    /// Compile a protocol (`{"timingMode":…,"modalities":[…]}`, the shape `parse` returns under
    /// `protocol`) into the NP-FW-HUB-001 §4 descriptor. The 64-byte signature slot at the end is zeroed:
    /// sign the region before it and write the signature in.
    static func compile(protocolJSON: Data, options: CompileOptions) throws -> Data {
        guard options.sessionUUID.count == 16 else {
            throw Refusal(message: "sessionUUID must be 16 bytes")
        }
        let def = try JSONSerialization.jsonObject(with: protocolJSON)
        let request: [String: Any] = [
            "def": def,
            "zones": options.zones.map { $0 as Any } ?? NSNull(),
            "clinicianSockets": options.clinicianSockets.map { $0 as Any } ?? NSNull(),
            "deviceSerialHex": options.deviceSerial.map { hex($0) as Any } ?? NSNull(),
            "nowUnix": options.nowUnix,
            "sessionUuidHex": hex(options.sessionUUID),
            "wavelengthRules": options.wavelengthRules.map { $0 as Any } ?? NSNull(),
            "autonomous": options.autonomous
        ]
        return try call(npps_compile_json, JSONSerialization.data(withJSONObject: request))
    }

    private static func hex(_ data: Data) -> String {
        data.map { String(format: "%02x", $0) }.joined()
    }

    private static func call(_ fn: Entry, _ input: Data) throws -> Data {
        // Never empty, so the pointer handed over is never nil (an empty source is a valid input).
        let storage = [UInt8](input) + [0]
        var out: UnsafeMutablePointer<UInt8>?
        var outLen = 0
        let code = storage.withUnsafeBufferPointer { fn($0.baseAddress, input.count, &out, &outLen) }
        defer { if let out { npps_free(out, outLen) } }
        let bytes = out.map { Data(bytes: $0, count: outLen) } ?? Data()
        switch code {
        case NppsStatus.OK:
            return bytes
        case NppsStatus.REFUSED, NppsStatus.INTERNAL_ERROR:
            throw Refusal(message: String(bytes: bytes, encoding: .utf8) ?? "the NPPS core refused the input")
        default:
            throw Refusal(message: "the NPPS core was given an argument it cannot read")
        }
    }
}

/// The core's resolve result. The block names are the core's, which are the property names of `NPLimitsSet`.
private struct NppsResolvedLimitBlocks: Decodable {
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
}

private struct NppsResolvedLimits: Decodable {
    var limits: NppsResolvedLimitBlocks
    var sources: NPLimitSourceMap
}
