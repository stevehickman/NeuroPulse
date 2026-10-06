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
        case 0:
            return bytes
        case 1, 2:
            throw Refusal(message: String(bytes: bytes, encoding: .utf8) ?? "the NPPS core refused the input")
        default:
            throw Refusal(message: "the NPPS core was given an argument it cannot read")
        }
    }
}
