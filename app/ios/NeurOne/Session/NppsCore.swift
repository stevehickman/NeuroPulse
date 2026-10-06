import Foundation
import NeurOneNppsCore

// The shared NPPS core (common/npps-core, OI-NPPS-CORE-01) as iOS sees it: one lexer, parser and
// hub-descriptor compiler written once in Rust, linked as the static XCFramework that
// scripts/build-npps-xcframework.sh builds from common/npps-ffi. This file marshals and nothing
// else. What an input means is decided in the core, so it means the same here as on every other
// runtime, and a refusal carries the message the web parser or compiler gives.
//
// Nothing in the app calls this yet: HubDescriptorCompiler and NPPSParser are still the Swift ports.

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
    }

    private typealias Entry = (
        UnsafePointer<UInt8>?, Int, UnsafeMutablePointer<UnsafeMutablePointer<UInt8>?>?, UnsafeMutablePointer<Int>?
    ) -> Int32

    /// Parse NPPS text into the core's JSON: an array of `{"kind":"single","protocol":{…}}` entries, or
    /// `{"kind":"skipped","what":"zone"}` for a block the core does not interpret yet.
    static func parse(_ source: String) throws -> Data {
        try call(npps_parse_json, Data(source.utf8))
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
            "wavelengthRules": options.wavelengthRules.map { $0 as Any } ?? NSNull()
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
            throw Refusal(message: String(decoding: bytes, as: UTF8.self))
        default:
            throw Refusal(message: "the NPPS core was given an argument it cannot read")
        }
    }
}
