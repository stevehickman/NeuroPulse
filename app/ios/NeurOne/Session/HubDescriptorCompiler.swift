import Foundation

// The hub session descriptor (OI-AND-WIRE-01, OI-NPPS-CORE-01).
//
// There is ONE wire format: the binary descriptor of NP-FW-HUB-001 §4, which
// firmware/hub_control/src/np_protocol.c parses. It has ONE writer: the shared NPPS core
// (common/npps-core), which this file calls through NppsCore. This used to be a Swift port of the
// web compiler, one of four hand-written ports that drifted; it now maps the iOS models to the core's
// protocol shape (NppsCoreMapping.swift), hands over the clock, the session UUID and the zone
// namespace, and signs the result. What a protocol compiles to, and what refuses it, is decided in
// the core and is the same on every runtime. app/NeurOneShared/TestData/hub-descriptor-golden.json
// holds the web compiler's output for the same definitions; HubDescriptorCompilerTests diffs this
// file against it, so the core is held to the web reference from here as well as from Rust.
//
// A refusal is an NPHubCompileError carrying the core's message.
//
// Layout (little-endian, packed):
//   [64]  header   magic u32 | version u16 | flags u8 | cmd_count u8 | uuid[16] |
//                  compiled_at_unix u32 | serial[32] | session_duration_ms u32
//   [N]   commands cmd_hdr(14) + target[target_len] + params[params_len]
//   [64]  Ed25519 signature over everything before it (the raw region, not a digest)

/// A compiled descriptor. `blob` is the whole wire image; the hub verifies it as is.
struct HubDescriptor {
    let blob: Data
    let sessionUUID: Data
    let isT2: Bool
    let cmdCount: Int
    /// First 8 bytes of the signing public key, hex, for hub key pinning. Empty when unsigned.
    let publicKeyFingerprint: String
}

/// A request the descriptor cannot express or the hardware cannot reach. Refused, never reshaped.
struct NPHubCompileError: Error, LocalizedError, Equatable {
    let message: String
    var errorDescription: String? { message }
}

enum HubDescriptorCompiler {

    static let UUID_LEN = 16
    static let SIG_LEN = 64
    static let FLAG_T2_TIER: UInt8 = 1 << 0      // NP_PROTO_FLAG_T2_TIER (app-computed, carries no authority)

    // MARK: - Public entry points

    /// Compile and sign. Ed25519 covers the raw signed region (NP-FW-HUB-001 §4.1), and the
    /// signature fills the last 64 bytes.
    ///
    /// - Parameters:
    ///   - deviceSerial: the 32-byte replay guard the hub checks (§4.2). Omitted only on the
    ///     bench: a hub with a provisioned serial refuses the descriptor (OI-AND-WIRE-02).
    ///   - clinicianSockets: operator-chosen 1-based socket ids for `clinician_selected` targets.
    static func compile(
        _ definition: NPProtocolDefinition,
        deviceSerial: Data? = nil,
        clinicianSockets: [Int]? = nil,
        autonomous: Bool = false,
        sign: (Data) throws -> (signature: Data, fingerprint: String) = SessionProtocolSigner.sign
    ) throws -> HubDescriptor {
        try signed(try build(definition, deviceSerial: deviceSerial, clinicianSockets: clinicianSockets,
                             autonomous: autonomous), using: sign)
    }

    /// Fill the trailing signature of an unsigned descriptor. Split from `compile` so the caller can
    /// tell a compile refusal from a signing failure.
    static func signed(
        _ unsigned: HubDescriptor,
        using sign: (Data) throws -> (signature: Data, fingerprint: String) = SessionProtocolSigner.sign
    ) throws -> HubDescriptor {
        let region = Data(unsigned.blob.prefix(unsigned.blob.count - SIG_LEN))
        let result = try sign(region)
        guard result.signature.count == SIG_LEN else {
            throw NPHubCompileError(message: "Ed25519 signature must be \(SIG_LEN) bytes, got \(result.signature.count).")
        }
        return HubDescriptor(blob: region + result.signature, sessionUUID: unsigned.sessionUUID,
                             isT2: unsigned.isT2, cmdCount: unsigned.cmdCount,
                             publicKeyFingerprint: result.fingerprint)
    }

    /// The descriptor with a zeroed signature: the web compiler's output, which the golden test diffs.
    /// `now` and `sessionUUID` are injectable so the bytes are reproducible.
    static func build(
        _ definition: NPProtocolDefinition,
        deviceSerial: Data? = nil,
        clinicianSockets: [Int]? = nil,
        autonomous: Bool = false,
        now: Date = Date(),
        sessionUUID: [UInt8]? = nil
    ) throws -> HubDescriptor {
        let uuid = sessionUUID ?? (0..<UUID_LEN).map { _ in UInt8.random(in: 0...255) }
        precondition(uuid.count == UUID_LEN)
        // The zone namespace is the loaded .npps one (NP-NPPS-REF-001 §8). A name it does not hold is
        // left out, and the core refuses the protocol naming it.
        var zones: [String: [Int]] = [:]
        for name in definition.nppsNamedZoneRefs {
            if let sockets = NPZoneRegistry.sockets(forZone: name) { zones[name] = sockets }
        }
        let options = NppsCore.CompileOptions(
            zones: zones, clinicianSockets: clinicianSockets, deviceSerial: deviceSerial,
            nowUnix: Int(now.timeIntervalSince1970), sessionUUID: Data(uuid), wavelengthRules: nil,
            autonomous: autonomous)
        let blob: Data
        do {
            blob = try NppsCore.compile(protocolJSON: try definition.nppsCoreJSON(), options: options)
        } catch let refusal as NppsCore.Refusal {
            throw NPHubCompileError(message: refusal.message)
        }
        return HubDescriptor(blob: blob, sessionUUID: Data(uuid), isT2: blob[blob.startIndex + 6] & FLAG_T2_TIER != 0,
                             cmdCount: Int(blob[blob.startIndex + 7]), publicKeyFingerprint: "")
    }
}

// MARK: - Refusals (NP-NPPS-REF-001 §4.1a)
//
// Compiler diagnostics, English like hubCompiler.ts's on the web.

/// Why a PBM block's wavelength cannot be delivered. Thrown by the compiler.
struct NPWavelengthRefusal: Error, LocalizedError, Equatable {
    let value: String
    let reason: NPWavelengthRules.Refusal

    var errorDescription: String? {
        switch reason {
        case .retired:
            return NPWavelengthRules.retiredMessage(value)
        case .invalid:
            return "PBM wavelength '\(value)' is not a wavelength: write one value such as \"810nm\"."
        case .unmapped:
            return "No emitter channel delivers \(value) under the wavelength rules in force. " +
                "Refused, not moved to the nearest channel."
        }
    }
}

extension NPWavelengthRules {
    /// The refusal text for a retired name, naming the blocks that replace it.
    static func retiredMessage(_ value: String) -> String {
        let blocks = (retired[value] ?? []).map { "\"\($0)\"" }.joined(separator: " and ")
        return "wavelength \"\(value)\" is retired: it welded independent emitters into one block. "
            + "Write one block per wavelength (\(blocks)), each with its own irradiance."
    }
}
