package life.neurone.core.ble

import java.util.UUID

// Port of app/ios/NeurOne/BLE/GATTCharacteristics.swift (NPUUID).
// UUIDs are placeholders — replace at firmware BLE implementation stage (OI-WA-03).
// These strings must remain byte-identical to the iOS app and hub firmware.

object GattUuids {
    val service: UUID          = UUID.fromString("4E455550-0001-1000-8000-00805F9B34FB")

    // Notify-only characteristics — match NP-APP-ROADMAP-001 §5
    val sessionState: UUID     = UUID.fromString("4E455550-0002-1000-8000-00805F9B34FB") // NOTIFY 4B
    val sessionStatus: UUID    = UUID.fromString("4E455550-0003-1000-8000-00805F9B34FB") // NOTIFY 4B
    val hrvCoherence: UUID     = UUID.fromString("4E455550-0004-1000-8000-00805F9B34FB") // NOTIFY 4B
    val pacerPhase: UUID       = UUID.fromString("4E455550-0005-1000-8000-00805F9B34FB") // NOTIFY 4B
    val impedanceResult: UUID  = UUID.fromString("4E455550-0006-1000-8000-00805F9B34FB") // NOTIFY 4B
    // + WRITE 1B: the kind index, zeroes that count on replacement (OI-ACC-08, ConsumableReset.kt)
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

    // Cervical VNS gel pad contact result — NOTIFY 4B (failed mask, check, side of each pad). T2 only.
    // Words the hub's pad refusal for the wearer (OI-ACC-07); frame in CervicalPadStatus.
    // UHDR-class, display only. NOT in `all` — the hub does not ship it yet, and a T1 hub has
    // no cervical accessory, so its absence must never block allCharacteristicsResolved.
    val cvnsPadStatus: UUID    = UUID.fromString("4E455550-0013-1000-8000-00805F9B34FB") // NOTIFY 4B

    // Cervical VNS offline-fault summary + hub re-enable state — READ/NOTIFY, 4 + 8n bytes
    // (see CervicalFaultStatus). And the wearer's re-enable confirmation after a cardiac
    // cutoff — WRITE, 1 byte 0x01; the hub accepts it only while awaiting one
    // (np_hub_cvns_reenable_confirm). NP-SW-FAULTMSG-001 P3/P4. Both T2 only and NOT in
    // `all`, for the same reasons as cvnsPadStatus.
    val cvnsFaultStatus: UUID     = UUID.fromString("4E455550-0014-1000-8000-00805F9B34FB") // READ/NOTIFY
    val cvnsReenableConfirm: UUID = UUID.fromString("4E455550-0015-1000-8000-00805F9B34FB") // WRITE 1B

    // Which person is using the device — WRITE 4B, little-endian opaque tag (ActiveUserTag),
    // forwarded to the safety MCU so a cardiac cutoff is held for that person only. Optional,
    // NOT in `all`.
    val activeUser: UUID          = UUID.fromString("4E455550-0016-1000-8000-00805F9B34FB") // WRITE 4B

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
