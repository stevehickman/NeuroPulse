import CoreBluetooth

// The characteristic ids and UUID strings are `GattIds` / `GattUuidStrings` (NppsConstants.generated.swift), generated from
// common/npps/constants.json, the one source the Android, web and Windows apps and the hub firmware read too. A UUID is
// `CBUUID(string: GattUuidStrings.SESSION_STATE_ID)`; the characteristics every hub must publish are `REQUIRED_IDS`.
// UUIDs are placeholders — replace at firmware BLE implementation stage (OI-WA-03).

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

    /// CVNS_PAD_STATUS: uint8 failed mask + uint8 check + 2 × uint8 pad side. nil if malformed.
    static func parseCervicalPadStatus(_ data: Data) -> CervicalPadStatus? {
        CervicalPadStatus(wire: data)
    }

    /// CVNS_FAULT_STATUS: version + re-enable state + n × 8-byte fault record. nil if malformed.
    static func parseCervicalFaultStatus(_ data: Data) -> CervicalFaultStatus? {
        CervicalFaultStatus(wire: data)
    }

    /// CONSUMABLE_STATUS: 4 × uint16 session counts (intranasal, hydrogel, VNS, audio)
    static func parseConsumableStatus(_ data: Data) -> [UInt16]? {
        guard data.count >= 8 else { return nil }
        return (0..<4).map { i in
            data.withUnsafeBytes { $0.loadUnaligned(fromByteOffset: i * 2, as: UInt16.self) }
        }
    }

    /// ZONE_MODULE_STATUS: socket-keyed status frame (the dynamic half).
    ///
    /// Wire contract: `firmware/zone_announce/include/np_zone_notify.h`, pinned by
    /// `np_zone_notify_tests.c`. Header (4B): version, flags, fragment index,
    /// record count. Records (3B each): socket id (1-based), module type, flags.
    ///
    /// No anatomy — that is in the socket map, because where a socket is cannot
    /// change when a module is swapped.
    ///
    /// Returns nil for any frame that is not exactly well-formed — wrong version,
    /// wrong kind bit, short header, a record count the body cannot satisfy, or a
    /// socket id outside the addressing domain. Presence gates safety-critical
    /// placement checks, so a malformed frame is discarded rather than partially
    /// believed.
    static func parseZoneModuleStatus(_ data: Data) -> ZoneModuleFrame? {
        ZoneModuleFrame(data)
    }

    /// SOCKET_MAP: the helmet's permanent socket geometry (the static half).
    ///
    /// Same header; records are 7B: socket id (1-based), packed lobe|side, flags,
    /// x_mm and y_mm as int16 little-endian. Read once when the app links.
    static func parseSocketMap(_ data: Data) -> SocketMapFrame? {
        SocketMapFrame(data)
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
