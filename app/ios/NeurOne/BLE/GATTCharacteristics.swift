import CoreBluetooth

// UUIDs are placeholders — replace at firmware BLE implementation stage (OI-WA-03).
// Format: 128-bit custom service / characteristic UUIDs.

enum NPUUID {
    static let service          = CBUUID(string: "4E455550-0001-1000-8000-00805F9B34FB")

    // Notify-only characteristics — match NP-APP-ROADMAP-001 §5
    static let sessionState     = CBUUID(string: "4E455550-0002-1000-8000-00805F9B34FB") // NOTIFY 4B
    static let sessionStatus    = CBUUID(string: "4E455550-0003-1000-8000-00805F9B34FB") // NOTIFY 2B
    static let hrvCoherence     = CBUUID(string: "4E455550-0004-1000-8000-00805F9B34FB") // NOTIFY 4B
    static let pacerPhase       = CBUUID(string: "4E455550-0005-1000-8000-00805F9B34FB") // NOTIFY 2B
    static let impedanceResult  = CBUUID(string: "4E455550-0006-1000-8000-00805F9B34FB") // NOTIFY 2B
    static let consumableStatus = CBUUID(string: "4E455550-0007-1000-8000-00805F9B34FB") // READ/NOTIFY 8B

    // Write characteristics — Mode 2 protocol upload, Mode 4 EDF request, OTA, calibration
    static let protocolUpload   = CBUUID(string: "4E455550-0008-1000-8000-00805F9B34FB") // WRITE (signed blob)
    static let edfRequest       = CBUUID(string: "4E455550-0009-1000-8000-00805F9B34FB") // WRITE (trigger EDF+ download)
    static let otaCommand       = CBUUID(string: "4E455550-000A-1000-8000-00805F9B34FB") // WRITE/NOTIFY
    static let otaStatus        = CBUUID(string: "4E455550-000B-1000-8000-00805F9B34FB") // NOTIFY
    static let calibrationCmd   = CBUUID(string: "4E455550-000C-1000-8000-00805F9B34FB") // WRITE
    static let zoneModuleStatus = CBUUID(string: "4E455550-000D-1000-8000-00805F9B34FB") // READ/NOTIFY 5B
    static let shdrUploadStatus = CBUUID(string: "4E455550-000E-1000-8000-00805F9B34FB") // NOTIFY

    // PENDING FIRMWARE CONFIRMATION — placeholder UUID for session stop command.
    // Real UUID will come from hub BLE firmware implementation (OI-WA-03).
    static let sessionStop      = CBUUID(string: "4E455550-000F-1000-8000-00805F9B34FB") // WRITE 1B (0x01 = stop)

    // Hub-provisioned TRNG warranty token — READ 32B, SHDR-linked opaque token.
    // Replaces Keychain-generated random token in SHDRUploader once hub firmware is available
    // (NP-FW-EMMC-002 Rev A §A, OI-WA-03).
    // NOT included in NPUUID.all — hub firmware not yet implemented; omitting it prevents
    // allCharacteristicsResolved from blocking until hub ships this characteristic.
    static let warrantyToken    = CBUUID(string: "4E455550-0010-1000-8000-00805F9B34FB") // READ 32B

    // Current hub firmware version — READ/NOTIFY 4B little-endian uint32.
    // Encoding: bits [23:16]=major, [15:8]=minor, [7:0]=patch.
    // NOT included in NPUUID.all — optional until hub firmware ships it (OI-WA-03).
    // OTAView shows "Unknown" when nil; does not block allCharacteristicsResolved.
    static let firmwareVersion  = CBUUID(string: "4E455550-0011-1000-8000-00805F9B34FB") // READ/NOTIFY 4B

    // All characteristics required for a fully-operational session.
    // warrantyToken and firmwareVersion are deliberately omitted — both are optional
    // until hub firmware ships them (OI-WA-03).
    static let all: [CBUUID] = [
        sessionState, sessionStatus, hrvCoherence, pacerPhase,
        impedanceResult, consumableStatus, protocolUpload, edfRequest,
        otaCommand, otaStatus, calibrationCmd, zoneModuleStatus, shdrUploadStatus,
        sessionStop
    ]
}

// MARK: - OTA command opcodes (app → hub via OTA_COMMAND write characteristic)
//
// Opcode sequence for a successful main-processor OTA:
//   1. initiate  → hub clears Scratch partition and prepares to receive
//   2. chunk (×N) → firmware data in 496-byte segments (2B index + data)
//   3. verify    → hub runs Ed25519 + SHA-256; hub reports .verified or .failed via OTA_STATUS
//   4. commit    → hub writes inactive bank, stages boot swap, resets
//   5. (hub reboots, reconnects, reports .complete via OTA_STATUS)

enum OTAOpcode: UInt8 {
    case initiate        = 0x01  // Prepare Scratch partition for receive
    case chunk           = 0x02  // Firmware chunk (2B little-endian index + data)
    case verify          = 0x03  // Hub runs np_ota_verify_scratch() — Ed25519 + SHA-256
    case commit          = 0x04  // Hub writes inactive bank, stages boot swap, resets
    case abort           = 0x05  // Cancel in-flight OTA; hub returns to idle
    case safetyMCUBegin  = 0x10  // Safety MCU update — requires explicit user confirmation
    case safetyMCUChunk  = 0x11
    case safetyMCUCommit = 0x12
}

// MARK: - Calibration command opcodes

enum CalibrationOpcode: UInt8 {
    case impedanceCheck   = 0x01  // Trigger ADS1299 electrode impedance check
    case ads1299SelfCal   = 0x02  // Trigger ADS1299 internal reference self-calibration
    case zoneIDRefresh    = 0x03  // Re-read all ZONE_ID resistors
    case fluxgateNullZero = 0x04  // Fluxgate zero-field nulling
}

// MARK: - Characteristic parsers (little-endian, matching hub firmware layout)

struct GATTParser {

    /// SESSION_STATE: uint32 — Unix epoch milliseconds
    static func parseSessionState(_ data: Data) -> UInt32? {
        guard data.count >= 4 else { return nil }
        return data.withUnsafeBytes { $0.loadUnaligned(fromByteOffset: 0, as: UInt32.self) }
    }

    /// SESSION_STATUS: uint8 protocolID + uint8 statusFlags
    static func parseSessionStatus(_ data: Data) -> (protocolID: UInt8, status: SessionStatus)? {
        guard data.count >= 2 else { return nil }
        let pid = data[0]
        guard let status = SessionStatus(rawValue: data[1]) else { return nil }
        return (pid, status)
    }

    /// HRV_COHERENCE: uint16 coherence×100 + uint16 RMSSD ms
    static func parseHRVCoherence(_ data: Data) -> HRVData? {
        guard data.count >= 4 else { return nil }
        let cohRaw = data.withUnsafeBytes { $0.loadUnaligned(fromByteOffset: 0, as: UInt16.self) }
        let rmssd  = data.withUnsafeBytes { $0.loadUnaligned(fromByteOffset: 2, as: UInt16.self) }
        return HRVData(coherenceScore: Float(cohRaw) / 100.0, rmssdMilliseconds: rmssd)
    }

    /// PACER_PHASE: uint8 phase + uint8 elapsed%
    static func parsePacerPhase(_ data: Data) -> (phase: PacerPhase, percent: UInt8)? {
        guard data.count >= 2 else { return nil }
        guard let phase = PacerPhase(rawValue: data[0]) else { return nil }
        return (phase, data[1])
    }

    /// IMPEDANCE_RESULT: uint16 bitmask (bit n = electrode n passed)
    static func parseImpedanceResult(_ data: Data) -> UInt16? {
        guard data.count >= 2 else { return nil }
        return data.withUnsafeBytes { $0.loadUnaligned(fromByteOffset: 0, as: UInt16.self) }
    }

    /// CONSUMABLE_STATUS: 4 × uint16 session counts (intranasal, hydrogel, VNS, audio)
    static func parseConsumableStatus(_ data: Data) -> [UInt16]? {
        guard data.count >= 8 else { return nil }
        return (0..<4).map { i in
            data.withUnsafeBytes { $0.loadUnaligned(fromByteOffset: i * 2, as: UInt16.self) }
        }
    }

    /// ZONE_MODULE_STATUS: 5 bytes — one uint8 per zone slot (0=absent, 1–5=zone ID confirmed)
    static func parseZoneModuleStatus(_ data: Data) -> [UInt8]? {
        guard data.count >= 5 else { return nil }
        return (0..<5).map { data[$0] }
    }

    /// FIRMWARE_VERSION: uint32 little-endian — bits [23:16]=major [15:8]=minor [7:0]=patch
    static func parseFirmwareVersion(_ data: Data) -> FirmwareVersion? {
        FirmwareVersion(gattBytes: data)
    }

    /// OTA_STATUS: uint8 phase + uint8 progressPercent + uint16 errorCode
    static func parseOTAStatus(_ data: Data) -> OTAStatusPacket? {
        guard data.count >= 4 else { return nil }
        let phase    = data[0]
        let progress = data[1]
        let errCode  = data.withUnsafeBytes { $0.loadUnaligned(fromByteOffset: 2, as: UInt16.self) }
        return OTAStatusPacket(phaseRaw: phase, progressPercent: progress, errorCode: errCode)
    }
}

struct OTAStatusPacket {
    let phaseRaw: UInt8
    let progressPercent: UInt8
    let errorCode: UInt16
    var isError: Bool { errorCode != 0 }
}

// (Duplicate GATTParser struct removed — canonical definition is above.)
