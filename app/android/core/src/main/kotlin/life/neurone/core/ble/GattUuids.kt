package life.neurone.core.ble

import java.util.UUID
import life.neurone.core.protocol.GattUuidStrings

// Port of app/ios/NeurOne/BLE/GATTCharacteristics.swift (NPUUID).
// UUIDs are placeholders — replace at firmware BLE implementation stage (OI-WA-03).
// The strings are generated from common/npps/constants.json (group GattIds), the one source iOS, the web, Windows and the hub
// firmware read too, so they cannot drift.

object GattUuids {
    val service: UUID          = UUID.fromString(GattUuidStrings.SERVICE)

    // Notify-only characteristics — match NP-APP-ROADMAP-001 §5
    val sessionState: UUID     = UUID.fromString(GattUuidStrings.SESSION_STATE) // NOTIFY 4B
    val sessionStatus: UUID    = UUID.fromString(GattUuidStrings.SESSION_STATUS) // NOTIFY 4B
    val hrvCoherence: UUID     = UUID.fromString(GattUuidStrings.HRV_COHERENCE) // NOTIFY 4B
    val pacerPhase: UUID       = UUID.fromString(GattUuidStrings.PACER_PHASE) // NOTIFY 4B
    val impedanceResult: UUID  = UUID.fromString(GattUuidStrings.IMPEDANCE_RESULT) // NOTIFY 4B
    // + WRITE 1B: the kind index, zeroes that count on replacement (OI-ACC-08, ConsumableReset.kt)
    val consumableStatus: UUID = UUID.fromString(GattUuidStrings.CONSUMABLE_STATUS) // READ/NOTIFY 8B

    // Write characteristics — Mode 2 protocol upload, Mode 4 EDF request, OTA, calibration
    val protocolUpload: UUID   = UUID.fromString(GattUuidStrings.PROTOCOL_UPLOAD) // WRITE (signed blob)
    val edfRequest: UUID       = UUID.fromString(GattUuidStrings.EDF_REQUEST) // WRITE (trigger EDF+)
    val otaCommand: UUID       = UUID.fromString(GattUuidStrings.OTA_COMMAND) // WRITE/NOTIFY
    val otaStatus: UUID        = UUID.fromString(GattUuidStrings.OTA_STATUS) // NOTIFY
    val calibrationCmd: UUID   = UUID.fromString(GattUuidStrings.CALIBRATION_CMD) // WRITE
    // READ/NOTIFY — socket-keyed status frames (np_zone_notify.h v2; ZoneModuleFrame.kt)
    val zoneModuleStatus: UUID = UUID.fromString(GattUuidStrings.ZONE_MODULE_STATUS)
    val shdrUploadStatus: UUID = UUID.fromString(GattUuidStrings.SHDR_UPLOAD_STATUS) // NOTIFY
    val sessionStop: UUID      = UUID.fromString(GattUuidStrings.SESSION_STOP) // WRITE 1B (0x01 = stop)

    // Hub-provisioned TRNG warranty token — READ 32B, SHDR-linked opaque token
    // (NP-FW-EMMC-002 Rev A §A, OI-WA-03). NOT in `all` — hub firmware not yet
    // implemented; omitting it prevents allCharacteristicsResolved from blocking.
    val warrantyToken: UUID    = UUID.fromString(GattUuidStrings.WARRANTY_TOKEN) // READ 32B

    // Socket geometry — READ/NOTIFY, fragmented 8-byte records (np_zone_notify.h v2). Read once
    // at link; subscribed so a lattice-changing OTA can re-publish. SHDR-class. NOT in `all` —
    // optional until hub firmware ships it (OI-WA-03). Mirrors iOS NPUUID.socketMap.
    val socketMap: UUID        = UUID.fromString(GattUuidStrings.SOCKET_MAP) // READ/NOTIFY

    // Current hub firmware version — READ/NOTIFY 4B little-endian uint32.
    // NOT in `all` — optional until hub firmware ships it (OI-WA-03).
    val firmwareVersion: UUID  = UUID.fromString(GattUuidStrings.FIRMWARE_VERSION) // READ/NOTIFY 4B

    // Cervical VNS gel pad contact result — NOTIFY 4B (failed mask, check, side of each pad). T2 only.
    // Words the hub's pad refusal for the wearer (OI-ACC-07); frame in CervicalPadStatus.
    // UHDR-class, display only. NOT in `all` — the hub does not ship it yet, and a T1 hub has
    // no cervical accessory, so its absence must never block allCharacteristicsResolved.
    val cvnsPadStatus: UUID    = UUID.fromString(GattUuidStrings.CVNS_PAD_STATUS) // NOTIFY 4B

    // Cervical VNS offline-fault summary + hub re-enable state — READ/NOTIFY, 4 + 8n bytes
    // (see CervicalFaultStatus). And the wearer's re-enable confirmation after a cardiac
    // cutoff — WRITE, 1 byte 0x01; the hub accepts it only while awaiting one
    // (np_hub_cvns_reenable_confirm). NP-SW-FAULTMSG-001 P3/P4. Both T2 only and NOT in
    // `all`, for the same reasons as cvnsPadStatus.
    val cvnsFaultStatus: UUID     = UUID.fromString(GattUuidStrings.CVNS_FAULT_STATUS) // READ/NOTIFY
    val cvnsReenableConfirm: UUID = UUID.fromString(GattUuidStrings.CVNS_REENABLE_CONFIRM) // WRITE 1B

    // Which person is using the device — WRITE 4B, little-endian opaque tag (ActiveUserTag),
    // forwarded to the safety MCU so a cardiac cutoff is held for that person only. Optional,
    // NOT in `all`.
    val activeUser: UUID          = UUID.fromString(GattUuidStrings.ACTIVE_USER) // WRITE 4B

    // The hub's 32-byte replay-guard serial — READ 32B, encrypted link only (NP-FW-HUB-001 §4.2).
    // Stamped into the descriptor header so the hub does not refuse it WRONG_DEVICE
    // (OI-AND-WIRE-02). Never persisted, never uploaded. NOT in `all`: READ-only, and `all` is the notify set.
    val deviceSerial: UUID        = UUID.fromString(GattUuidStrings.DEVICE_SERIAL) // READ 32B

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
