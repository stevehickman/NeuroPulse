package com.neurone.core.ble

import java.util.UUID

// Port of app/ios/NeurOne/BLE/GATTCharacteristics.swift (NPUUID).
// UUIDs are placeholders — replace at firmware BLE implementation stage (OI-WA-03).
// These strings must remain byte-identical to the iOS app and hub firmware.

object GattUuids {
    val service: UUID          = UUID.fromString("4E455550-0001-1000-8000-00805F9B34FB")

    // Notify-only characteristics — match NP-APP-ROADMAP-001 §5
    val sessionState: UUID     = UUID.fromString("4E455550-0002-1000-8000-00805F9B34FB") // NOTIFY 4B
    val sessionStatus: UUID    = UUID.fromString("4E455550-0003-1000-8000-00805F9B34FB") // NOTIFY 2B
    val hrvCoherence: UUID     = UUID.fromString("4E455550-0004-1000-8000-00805F9B34FB") // NOTIFY 4B
    val pacerPhase: UUID       = UUID.fromString("4E455550-0005-1000-8000-00805F9B34FB") // NOTIFY 2B
    val impedanceResult: UUID  = UUID.fromString("4E455550-0006-1000-8000-00805F9B34FB") // NOTIFY 2B
    val consumableStatus: UUID = UUID.fromString("4E455550-0007-1000-8000-00805F9B34FB") // READ/NOTIFY 8B

    // Write characteristics — Mode 2 protocol upload, Mode 4 EDF request, OTA, calibration
    val protocolUpload: UUID   = UUID.fromString("4E455550-0008-1000-8000-00805F9B34FB") // WRITE (signed blob)
    val edfRequest: UUID       = UUID.fromString("4E455550-0009-1000-8000-00805F9B34FB") // WRITE (trigger EDF+)
    val otaCommand: UUID       = UUID.fromString("4E455550-000A-1000-8000-00805F9B34FB") // WRITE/NOTIFY
    val otaStatus: UUID        = UUID.fromString("4E455550-000B-1000-8000-00805F9B34FB") // NOTIFY
    val calibrationCmd: UUID   = UUID.fromString("4E455550-000C-1000-8000-00805F9B34FB") // WRITE
    val zoneModuleStatus: UUID = UUID.fromString("4E455550-000D-1000-8000-00805F9B34FB") // READ/NOTIFY 5B
    val shdrUploadStatus: UUID = UUID.fromString("4E455550-000E-1000-8000-00805F9B34FB") // NOTIFY
    val sessionStop: UUID      = UUID.fromString("4E455550-000F-1000-8000-00805F9B34FB") // WRITE 1B (0x01 = stop)

    // Hub-provisioned TRNG warranty token — READ 32B, SHDR-linked opaque token
    // (NP-FW-EMMC-002 Rev A §A, OI-WA-03). NOT in `all` — hub firmware not yet
    // implemented; omitting it prevents allCharacteristicsResolved from blocking.
    val warrantyToken: UUID    = UUID.fromString("4E455550-0010-1000-8000-00805F9B34FB") // READ 32B

    // Current hub firmware version — READ/NOTIFY 4B little-endian uint32.
    // NOT in `all` — optional until hub firmware ships it (OI-WA-03).
    val firmwareVersion: UUID  = UUID.fromString("4E455550-0011-1000-8000-00805F9B34FB") // READ/NOTIFY 4B

    // All characteristics required for a fully-operational session.
    // warrantyToken and firmwareVersion are deliberately omitted (optional until
    // hub firmware ships them — OI-WA-03). Mirrors iOS NPUUID.all exactly.
    val all: List<UUID> = listOf(
        sessionState, sessionStatus, hrvCoherence, pacerPhase,
        impedanceResult, consumableStatus, protocolUpload, edfRequest,
        otaCommand, otaStatus, calibrationCmd, zoneModuleStatus, shdrUploadStatus,
        sessionStop,
    )
}

// OTA command opcodes (app → hub via OTA_COMMAND write characteristic).
// Wire values frozen — hub firmware contract; must match iOS OTAOpcode.
enum class OtaOpcode(val rawValue: Int) {
    INITIATE(0x01),          // Prepare Scratch partition for receive
    CHUNK(0x02),             // Firmware chunk (2B little-endian index + data)
    VERIFY(0x03),            // Hub runs np_ota_verify_scratch() — Ed25519 + SHA-256
    COMMIT(0x04),            // Hub writes inactive bank, stages boot swap, resets
    ABORT(0x05),             // Cancel in-flight OTA; hub returns to idle
    SAFETY_MCU_BEGIN(0x10),  // Safety MCU update — requires explicit user confirmation
    SAFETY_MCU_CHUNK(0x11),
    SAFETY_MCU_COMMIT(0x12),
}

// Calibration command opcodes — must match iOS CalibrationOpcode.
enum class CalibrationOpcode(val rawValue: Int) {
    IMPEDANCE_CHECK(0x01),    // Trigger ADS1299 electrode impedance check
    ADS1299_SELF_CAL(0x02),   // Trigger ADS1299 internal reference self-calibration
    ZONE_ID_REFRESH(0x03),    // Re-read all ZONE_ID resistors
    FLUXGATE_NULL_ZERO(0x04), // Fluxgate zero-field nulling
}
