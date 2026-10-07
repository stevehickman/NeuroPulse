package life.neurone.core.ble

// The characteristic ids and UUID strings are `GattIds` / `GattUuidStrings` (NppsConstants.generated.kt), generated from
// common/npps/constants.json, the one source iOS, the web, Windows and the hub firmware read too. A UUID is
// `UUID.fromString(GattUuidStrings.SESSION_STATE_ID)`; the characteristics every hub must publish are `REQUIRED_IDS`.
// UUIDs are placeholders — replace at firmware BLE implementation stage (OI-WA-03).

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
