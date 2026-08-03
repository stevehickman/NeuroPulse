import Foundation

// NPSocketMask — how the app says WHERE on the helmet a cranial command lands.
//
// The helmet is a hex lattice of ~80 sockets (NP-HEX-ZM-001), not the five zone-
// module slots the first cut of this app assumed. Zones are named, human-authored
// socket sets in protocols/predefined/00-zones.npps; targeting resolves a zone
// name to sockets and sockets to a bitmap.
//
// This file is a port of the web app's `socketSet.ts` + `hubCompiler.socketBitmap()`
// so the two producers cannot drift: iOS is what signs and uploads the session
// protocol (CLAUDE.md §4.2), so a divergence here is a wrong-site stimulation path,
// not a formatting difference.
//
// WHY A BITMAP AND NOT A LIST OF IDS. Under the inclusive zone-membership rule,
// a socket belongs to several zones at once — the midline sockets are listed in
// BOTH hemisphere zones of their region, by design. A protocol naming
// "Frontal Left" and "Frontal Right" therefore names socket 2 twice. In a list
// that is two entries and two drives of the same module: double J/cm² on the
// same patch of scalp. In a bitmap the duplicate is not expressible, so the
// dedup guarantee stops being something every producer has to remember.

// MARK: - Socket id validity

/// Why a value is not a usable socket id. `outOfRange` and `notFitted` are
/// distinct: the first cannot name a socket on any helmet of this revision, the
/// second means the lattice has a gap there. Today the lattice is contiguous so
/// `notFitted` never fires, but callers should not have to know that.
enum NPSocketIDProblem: Equatable {
    case outOfRange
    case notFitted
}

/// Socket id domain, derived from the generated zone table rather than hardcoded.
enum NPSocketID {

    /// Ids are 1-based in .npps files and in this app; firmware numbers from 0.
    /// The single conversion happens in `NPSocketMask`.
    static let numberingBase = 1

    static var minimum: Int { numberingBase }
    static var maximum: Int { SocketZones.socketCount }

    /// The inclusive id range (e.g. `1–80`), for error messages and UI hints.
    static var rangeLabel: String { "\(minimum)–\(maximum)" }

    static func problem(_ id: Int) -> NPSocketIDProblem? {
        if id < minimum || id > maximum { return .outOfRange }
        if SocketZones.bySocket[UInt8(id)] == nil { return .notFitted }
        return nil
    }

    static func isValid(_ id: Int) -> Bool { problem(id) == nil }
}

// MARK: - Targeting errors

/// Why a protocol's PBM target could not be turned into sockets.
///
/// Every case carries enough to name the offending zone or socket in the message,
/// because these surface to a clinician in the validator (NPProtocolValidator)
/// and "invalid target" names nothing they can go and fix.
enum NPSocketTargetError: Error, LocalizedError, Equatable {

    /// `zones: all` / `front` / `rear` — the retired five-slot selectors.
    case retiredSelector(selector: String, replacement: String)

    /// `zones: [1, 2]` or `zones: custom` + `custom_zones:` — retired numeric indices.
    case retiredNumeric(zoneIndices: [Int])

    /// A named zone this app build does not know.
    case unknownZone(name: String)

    /// `zones: clinician_selected` with no operator selection supplied.
    case clinicianSelectionMissing

    /// The target resolved, but to nothing.
    case emptyTarget(target: String)

    /// A socket id that is not on this helmet, and where it came from.
    case invalidSocket(id: Int, problem: NPSocketIDProblem, source: String)

    var errorDescription: String? {
        switch self {
        case .retiredSelector(let selector, let replacement):
            // Deliberately NOT auto-migrated. The old `front` was slot mask 0x07 —
            // frontal L/R PLUS parietal L — so the old front/rear split cut the
            // parietal pair in half. Mapping the selector onto sockets would
            // reproduce that bug rather than fix it, so the author re-states the
            // intent once and the file is correct from then on.
            return "PBM transcranial uses the retired zone selector '\(selector)', which addressed the "
                + "five legacy zone-module slots. Use the named zone \"\(replacement)\" instead "
                + "(zones: [\"\(replacement)\"]) — see protocols/predefined/00-zones.npps."

        case .retiredNumeric(let zoneIndices):
            // Two contradictory documented bases for the same value: the parser
            // documents 0-based legacy zone indices, 00-zones.npps documents
            // 1-based. One reading targets a whole zone away from the other.
            let listed = zoneIndices.map(String.init).joined(separator: ", ")
            return "PBM transcranial uses the retired numeric zone selector (custom_zones: [\(listed)]). "
                + "Its base is ambiguous — the parser documents 0-based indices, 00-zones.npps documents "
                + "1-based — so it cannot be migrated automatically. Replace it with named zones "
                + "(e.g. zones: [\"Frontal Left\", \"Frontal Right\"])."

        case .unknownZone(let name):
            return "PBM transcranial references zone \"\(name)\", which is not a zone this app build "
                + "knows. Zone names are authored in protocols/predefined/00-zones.npps."

        case .clinicianSelectionMissing:
            return "PBM transcranial target is 'clinician_selected': the operator must choose the "
                + "sockets before this protocol can run."

        case .emptyTarget(let target):
            // Not a no-op. A session that reports a delivered dose while lighting
            // nothing is worse than one that refuses to start.
            return "PBM transcranial target (\(target)) resolves to no sockets — a session would report "
                + "a delivered dose while lighting nothing. Resolve it to at least one socket."

        case .invalidSocket(let id, let problem, let source):
            let why = problem == .outOfRange
                ? "valid ids are \(NPSocketID.rangeLabel)"
                : "this lattice has no socket there"
            return "Socket \(id) named by \(source) is not a socket on this helmet (\(why))."
        }
    }
}

// MARK: - NPSocketMask

/// A set of helmet sockets, as the bitmap the firmware reads.
///
/// Mirrors `NP_PROTO_TARGET_SOCKET_MASK`: 16 bytes / 128 bits, LSB-first,
/// 0-based, sized to `NP_HEXMAP_MAX_SOCKETS` and NOT to the ~80 sockets this
/// shell wires — the wire format must not need a revision when a shell wires
/// more of the domain it already addresses.
struct NPSocketMask: Equatable, Hashable, Codable {

    /// == firmware `NP_HUB_SOCKET_MASK_BYTES`
    /// (firmware/hub_control/include/np_hub_config.h), and == the 1064nm module's
    /// `NP_PBM1064_SOCKET_MASK_BYTES`. Kept equal in VALUE without a shared
    /// definition, cross-referenced by comment on every side, the way
    /// np_pbm1064_config.h documents the same pairing; the tests assert the
    /// numeric match against a byte-level fixture.
    static let byteCount = 16

    /// Bit `n` set == firmware socket_id `n` targeted == app socket id `n + 1`.
    private(set) var bytes: [UInt8]

    /// The empty mask. Never valid as a command target — see `emptyTarget`.
    static let empty = NPSocketMask(bytes: [UInt8](repeating: 0, count: byteCount))

    private init(bytes: [UInt8]) {
        self.bytes = bytes
    }

    /// Build a mask from 1-based app socket ids.
    ///
    /// Two conversions happen here and nowhere else, which is the point of having
    /// a single initialiser: 1-based app id -> 0-based firmware socket_id, and
    /// set membership -> bit membership, so a socket named twice is delivered once.
    ///
    /// - Parameter source: what named these sockets, for the error message
    ///   (a zone name, or "the operator's selection").
    init(sockets: some Sequence<Int>, source: String) throws {
        var bytes = [UInt8](repeating: 0, count: Self.byteCount)
        for id in sockets {
            if let problem = NPSocketID.problem(id) {
                throw NPSocketTargetError.invalidSocket(id: id, problem: problem, source: source)
            }
            let bit = id - NPSocketID.numberingBase
            bytes[bit >> 3] |= UInt8(1 << (bit & 7))
        }
        self.bytes = bytes
    }

    /// The targeted socket ids, 1-based and ascending — deduplicated by
    /// construction. For display, tests and dose accounting; the wire carries
    /// `bytes`.
    var socketIDs: [Int] {
        var ids: [Int] = []
        for (index, byte) in bytes.enumerated() where byte != 0 {
            for bit in 0..<8 where byte & UInt8(1 << bit) != 0 {
                ids.append(index * 8 + bit + NPSocketID.numberingBase)
            }
        }
        return ids
    }

    var socketCount: Int { bytes.reduce(0) { $0 + $1.nonzeroBitCount } }

    var isEmpty: Bool { socketCount == 0 }

    /// Union with another mask. Deduplication is structural — a bit set twice is
    /// still one bit — which is exactly why overlapping zones are safe to union.
    func union(_ other: NPSocketMask) -> NPSocketMask {
        NPSocketMask(bytes: zip(bytes, other.bytes).map { $0 | $1 })
    }

    // MARK: Codable

    /// Encoded as a 32-character lowercase hex string rather than a JSON array of
    /// 16 numbers: it is a fixed-width bitmap, and the hex form makes a
    /// wrong-length blob obvious on inspection instead of plausible.
    var hexString: String {
        bytes.map { String(format: "%02x", $0) }.joined()
    }

    init(from decoder: Decoder) throws {
        let hex = try decoder.singleValueContainer().decode(String.self)
        guard hex.count == Self.byteCount * 2 else {
            throw DecodingError.dataCorrupted(.init(
                codingPath: decoder.codingPath,
                debugDescription: "Socket mask must be \(Self.byteCount * 2) hex characters "
                    + "(\(Self.byteCount) bytes); got \(hex.count)."
            ))
        }
        var bytes: [UInt8] = []
        bytes.reserveCapacity(Self.byteCount)
        var index = hex.startIndex
        while index < hex.endIndex {
            let next = hex.index(index, offsetBy: 2)
            guard let byte = UInt8(hex[index..<next], radix: 16) else {
                throw DecodingError.dataCorrupted(.init(
                    codingPath: decoder.codingPath,
                    debugDescription: "Socket mask is not hexadecimal: \(hex)"
                ))
            }
            bytes.append(byte)
            index = next
        }
        self.bytes = bytes
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.singleValueContainer()
        try container.encode(hexString)
    }
}
