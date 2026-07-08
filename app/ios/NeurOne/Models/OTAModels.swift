import Foundation

// OTA firmware update state model.
// Dual-bank OTA: hub has Bank A (current) and Bank B (staging).
// Per NP-FW-EMMC-001 Rev A §8 and BLE OTA protocol spec.

// MARK: - OTA phase (matches hub OTA_STATUS notify byte 0)

enum OTAPhase: UInt8, CustomStringConvertible {
    case idle           = 0x00
    case preparing      = 0x01  // Hub clearing Scratch partition
    case transferring   = 0x02  // Receiving firmware chunks
    case verifying      = 0x03  // Hub computing SHA-256 + Ed25519 check
    case verified       = 0x04  // Signature OK; awaiting COMMIT opcode from app
    case applying       = 0x05  // Writing inactive bank + staging boot swap + about to reset
    case complete       = 0x06  // Post-reboot, running from new bank
    case failed         = 0xFF

    init(rawPacketByte: UInt8) {
        self = OTAPhase(rawValue: rawPacketByte) ?? .idle
    }

    var description: String {
        switch self {
        case .idle:         return "Ready"
        case .preparing:    return "Preparing update…"
        case .transferring: return "Transferring firmware…"
        case .verifying:    return "Verifying signature…"
        case .verified:     return "Signature verified"
        case .applying:     return "Applying update — do not disconnect"
        case .complete:     return "Update complete"
        case .failed:       return "Update failed"
        }
    }

    var isTerminal: Bool { self == .complete || self == .failed }
    var isBusy: Bool {
        switch self {
        case .idle, .complete, .failed: return false
        default: return true
        }
    }
}

// MARK: - Firmware version (semantic versioning, Comparable)

struct FirmwareVersion: Comparable, Equatable, Hashable, CustomStringConvertible {
    let major: Int
    let minor: Int
    let patch: Int

    init?(_ string: String) {
        let parts = string.split(separator: ".").compactMap { Int($0) }
        guard parts.count == 3,
              parts[0] >= 0, parts[1] >= 0, parts[2] >= 0 else { return nil }
        (major, minor, patch) = (parts[0], parts[1], parts[2])
    }

    // Parse from hub GATT firmware_version characteristic (uint32, little-endian).
    // Encoding: bits [23:16] = major, [15:8] = minor, [7:0] = patch.
    init?(gattBytes: Data) {
        guard gattBytes.count >= 4 else { return nil }
        let raw = gattBytes.withUnsafeBytes { $0.loadUnaligned(fromByteOffset: 0, as: UInt32.self) }
        let littleEndian = UInt32(littleEndian: raw)
        major = Int((littleEndian >> 16) & 0xFF)
        minor = Int((littleEndian >> 8)  & 0xFF)
        patch = Int( littleEndian        & 0xFF)
    }

    var description: String { "\(major).\(minor).\(patch)" }

    static func < (lhs: Self, rhs: Self) -> Bool {
        if lhs.major != rhs.major { return lhs.major < rhs.major }
        if lhs.minor != rhs.minor { return lhs.minor < rhs.minor }
        return lhs.patch < rhs.patch
    }
}

// MARK: - Firmware manifest (decoded from distribution endpoint)

struct FirmwareManifest: Decodable {
    let version: String
    let releaseDate: String         // ISO 8601 date string, e.g. "2026-06-09"
    let imageSizeBytes: Int
    let sha256Hex: String           // Expected SHA-256 of the firmware binary
    let ed25519KeyFingerprint: String
    let isSafetyMCUFirmware: Bool
    let releaseNotes: String

    func toFirmwareImage() -> FirmwareImage? {
        let formatter = ISO8601DateFormatter()
        formatter.formatOptions = [.withFullDate]
        guard let date = formatter.date(from: releaseDate) else { return nil }
        return FirmwareImage(
            version: version,
            buildDate: date,
            imageSizeBytes: imageSizeBytes,
            ed25519PublicKeyFingerprint: ed25519KeyFingerprint,
            isSafetyMCUFirmware: isSafetyMCUFirmware,
            releaseNotes: releaseNotes
        )
    }
}

// MARK: - Firmware image (display model)

struct FirmwareImage: Identifiable {
    var id: UUID = UUID()
    var version: String
    var buildDate: Date
    var imageSizeBytes: Int
    var ed25519PublicKeyFingerprint: String  // hex — shown to user before confirmation
    var isSafetyMCUFirmware: Bool           // requires explicit separate user confirmation
    var releaseNotes: String
}

// MARK: - OTA session (transfer progress tracking)

struct OTASession {
    var image: FirmwareImage
    var startedAt: Date = Date()
    var completedAt: Date?

    // Chunk-level tracking
    var totalChunks: Int = 0
    var sentChunks: Int = 0

    // Byte-level tracking (ISC-110: bytes/total bytes progress bar)
    var totalBytes: Int = 0
    var sentBytes: Int = 0

    // ATT_MTU(512) − 3B ATT header − 1B OTA opcode − 2B chunk index = 506 max.
    // Using 496 leaves a safe margin for GATT stack overhead variations.
    static let chunkSize = 496

    var isActive: Bool { completedAt == nil && sentBytes > 0 }
    var bytesProgress: Double { totalBytes > 0 ? Double(sentBytes) / Double(totalBytes) : 0 }

    static func formattedBytes(_ bytes: Int) -> String {
        // Integer round-half-up avoids banker's rounding producing "0 KB" for sub-1 KB values.
        let kb = (bytes + 512) / 1024
        if kb < 1000 { return "\(max(1, kb)) KB" }
        return String(format: "%.1f MB", Double(bytes) / (1024.0 * 1024.0))
    }
}
