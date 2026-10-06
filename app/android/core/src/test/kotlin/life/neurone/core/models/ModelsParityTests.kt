package life.neurone.core.models

import life.neurone.core.protocol.BleTransfer
import life.neurone.core.protocol.GattUuidStrings
import life.neurone.core.ble.CalibrationOpcode
import life.neurone.core.ble.OtaOpcode
import java.io.File
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue
import java.util.UUID

// Parity tests — these values are wire/UUID contracts shared with the hub
// firmware and the iOS app (GATTCharacteristics.swift, SessionState.swift,
// OTAModels.swift). A failure here means platform divergence.

class ModelsParityTests {

    // ── GATT UUIDs (ISC-9, ISC-10): the generated strings parse to the contract values ─────────

    @Test
    fun gattUuidStringsMatchContract() {
        assertEquals("4e455550-0001-1000-8000-00805f9b34fb", UUID.fromString(GattUuidStrings.SERVICE_ID).toString())
        assertEquals("4e455550-0002-1000-8000-00805f9b34fb", UUID.fromString(GattUuidStrings.SESSION_STATE_ID).toString())
        assertEquals("4e455550-0003-1000-8000-00805f9b34fb", UUID.fromString(GattUuidStrings.SESSION_STATUS_ID).toString())
        assertEquals("4e455550-0004-1000-8000-00805f9b34fb", UUID.fromString(GattUuidStrings.HRV_COHERENCE_ID).toString())
        assertEquals("4e455550-0005-1000-8000-00805f9b34fb", UUID.fromString(GattUuidStrings.PACER_PHASE_ID).toString())
        assertEquals("4e455550-0006-1000-8000-00805f9b34fb", UUID.fromString(GattUuidStrings.IMPEDANCE_RESULT_ID).toString())
        assertEquals("4e455550-0007-1000-8000-00805f9b34fb", UUID.fromString(GattUuidStrings.CONSUMABLE_STATUS_ID).toString())
        assertEquals("4e455550-0008-1000-8000-00805f9b34fb", UUID.fromString(GattUuidStrings.PROTOCOL_UPLOAD_ID).toString())
        assertEquals("4e455550-0009-1000-8000-00805f9b34fb", UUID.fromString(GattUuidStrings.EDF_REQUEST_ID).toString())
        assertEquals("4e455550-000a-1000-8000-00805f9b34fb", UUID.fromString(GattUuidStrings.OTA_COMMAND_ID).toString())
        assertEquals("4e455550-000b-1000-8000-00805f9b34fb", UUID.fromString(GattUuidStrings.OTA_STATUS_ID).toString())
        assertEquals("4e455550-000c-1000-8000-00805f9b34fb", UUID.fromString(GattUuidStrings.CALIBRATION_CMD_ID).toString())
        assertEquals("4e455550-000d-1000-8000-00805f9b34fb", UUID.fromString(GattUuidStrings.ZONE_MODULE_STATUS_ID).toString())
        assertEquals("4e455550-000e-1000-8000-00805f9b34fb", UUID.fromString(GattUuidStrings.SHDR_UPLOAD_STATUS_ID).toString())
        assertEquals("4e455550-000f-1000-8000-00805f9b34fb", UUID.fromString(GattUuidStrings.SESSION_STOP_ID).toString())
        assertEquals("4e455550-0010-1000-8000-00805f9b34fb", UUID.fromString(GattUuidStrings.WARRANTY_TOKEN_ID).toString())
        assertEquals("4e455550-0011-1000-8000-00805f9b34fb", UUID.fromString(GattUuidStrings.FIRMWARE_VERSION_ID).toString())
        assertEquals("4e455550-0012-1000-8000-00805f9b34fb", UUID.fromString(GattUuidStrings.SOCKET_MAP_ID).toString())
    }

    @Test
    fun gattUuidsAllContainsExactlyFourteenAndExcludesOptionals() {
        assertEquals(14, GattUuidStrings.REQUIRED_IDS.map(UUID::fromString).size)
        assertFalse(UUID.fromString(GattUuidStrings.WARRANTY_TOKEN_ID) in GattUuidStrings.REQUIRED_IDS.map(UUID::fromString))
        assertFalse(UUID.fromString(GattUuidStrings.FIRMWARE_VERSION_ID) in GattUuidStrings.REQUIRED_IDS.map(UUID::fromString))
        assertTrue(UUID.fromString(GattUuidStrings.SESSION_STOP_ID) in GattUuidStrings.REQUIRED_IDS.map(UUID::fromString))
    }

    // ── Session enums raw values (ISC-15) ────────────────────────────────

    @Test
    fun sessionStatusRawValuesMatchFirmware() {
        assertEquals(0, SessionStatus.IDLE.rawValue)
        assertEquals(1, SessionStatus.RUNNING.rawValue)
        assertEquals(2, SessionStatus.PAUSED.rawValue)
        assertEquals(3, SessionStatus.COMPLETED.rawValue)
        assertNull(SessionStatus.from(4))
    }

    @Test
    fun pacerPhaseRawValuesMatchFirmware() {
        assertEquals(0, PacerPhase.INHALE.rawValue)
        assertEquals(1, PacerPhase.EXHALE.rawValue)
    }

    @Test
    fun sessionStateEmptyMatchesIosDefaults() {
        val e = SessionState.EMPTY
        assertEquals(0L, e.epoch)
        assertEquals(SessionStatus.IDLE, e.status)
        assertEquals(listOf(0, 0, 0, 0), e.consumableSessionCounts)
    }

    // ── OTA opcodes frozen wire values (ISC-14) ──────────────────────────

    @Test
    fun otaOpcodesMatchFrozenWireValues() {
        assertEquals(0x01, OtaOpcode.INITIATE.rawValue)
        assertEquals(0x02, OtaOpcode.CHUNK.rawValue)
        assertEquals(0x03, OtaOpcode.VERIFY.rawValue)
        assertEquals(0x04, OtaOpcode.COMMIT.rawValue)
        assertEquals(0x05, OtaOpcode.ABORT.rawValue)
        assertEquals(0x10, OtaOpcode.SAFETY_MCU_BEGIN.rawValue)
        assertEquals(0x11, OtaOpcode.SAFETY_MCU_CHUNK.rawValue)
        assertEquals(0x12, OtaOpcode.SAFETY_MCU_COMMIT.rawValue)
    }

    @Test
    fun calibrationOpcodesMatchFrozenWireValues() {
        assertEquals(0x01, CalibrationOpcode.IMPEDANCE_CHECK.rawValue)
        assertEquals(0x02, CalibrationOpcode.ADS1299_SELF_CAL.rawValue)
        assertEquals(0x03, CalibrationOpcode.ZONE_ID_REFRESH.rawValue)
        assertEquals(0x04, CalibrationOpcode.FLUXGATE_NULL_ZERO.rawValue)
    }

    // ── OtaPhase semantics (ISC-17) ──────────────────────────────────────

    @Test
    fun otaPhaseVerifiedIsFourAndFailedIsFF() {
        assertEquals(0x04, OtaPhase.VERIFIED.rawValue)
        assertEquals(0xFF, OtaPhase.FAILED.rawValue)
    }

    /**
     * The hub firmware does not emit OTA_STATUS yet, so the iOS `OTAPhase` enum is the only
     * other statement of this wire table. Read it from source rather than restating it here:
     * a hard-coded copy is how Android drifted to RECEIVING/COMMITTING/REBOOTING on 0x01, 0x03
     * and 0x05 while this file still passed.
     */
    @Test
    fun otaPhaseTableMatchesIosOtaPhaseSource() {
        val swift = File(repoRoot(), "app/ios/NeurOne/Models/OTAModels.swift").readText()
        val body = swift.substringAfter("enum OTAPhase").substringBefore("init(rawPacketByte")
        val ios = Regex("""case\s+(\w+)\s*=\s*0x([0-9A-Fa-f]{2})""").findAll(body)
            .associate { it.groupValues[1].uppercase() to it.groupValues[2].toInt(16) }

        // Guard against the regex silently matching nothing (or a truncated enum).
        assertEquals(8, ios.size, "parsed iOS OTAPhase cases: $ios")
        assertEquals(ios, OtaPhase.entries.associate { it.name to it.rawValue })
    }

    @Test
    fun otaPhaseBusyAndTerminalSemanticsMatchIos() {
        // iOS: isTerminal = complete || failed; isBusy = anything but idle/complete/failed.
        val notBusy = setOf(OtaPhase.IDLE, OtaPhase.COMPLETE, OtaPhase.FAILED)
        val terminal = setOf(OtaPhase.COMPLETE, OtaPhase.FAILED)
        for (p in OtaPhase.entries) {
            assertEquals(p !in notBusy, p.isBusy, "$p.isBusy")
            assertEquals(p in terminal, p.isTerminal, "$p.isTerminal")
        }
        // VERIFIED is mid-update (awaiting the app's COMMIT), not settled.
        assertTrue(OtaPhase.VERIFIED.isBusy)
        assertFalse(OtaPhase.VERIFIED.isTerminal)
    }

    // ── FirmwareVersion (ISC-16) ─────────────────────────────────────────

    @Test
    fun firmwareVersionComparableOrdering() {
        val a = FirmwareVersion.parse("1.2.3")!!
        val b = FirmwareVersion.parse("1.3.0")!!
        val c = FirmwareVersion.parse("2.0.0")!!
        assertTrue(a < b)
        assertTrue(b < c)
        assertEquals(a, FirmwareVersion(1, 2, 3))
        assertNull(FirmwareVersion.parse("1.2"))
        assertNull(FirmwareVersion.parse("a.b.c"))
    }

    @Test
    fun otaSessionFormattedBytesRoundsHalfUp() {
        assertEquals("512 B", OtaSession.formattedBytes(512))
        assertEquals("1 KB", OtaSession.formattedBytes(1024))
        // 1536 B = 1.5 KB → rounds half up to 2 KB (not banker's "0/1 KB")
        assertEquals("2 KB", OtaSession.formattedBytes(1536))
        assertEquals(496, BleTransfer.OTA_CHUNK_SIZE)
    }

    // ── Clinician tier → UHDR element mapping (ISC-18) ───────────────────

    @Test
    fun clinicianTierElementMappingsMatchIos() {
        assertEquals(
            setOf(
                UHDRElement.SESSION_TIMESTAMPS, UHDRElement.SESSION_DURATION,
                UHDRElement.PROTOCOL_PARAMETERS,
            ),
            ClinicianUseCaseTier.MONITOR.uhdrElements,
        )
        assertEquals(6, ClinicianUseCaseTier.ASSESS.uhdrElements.size)
        assertEquals(10, ClinicianUseCaseTier.FULL_CLINICAL.uhdrElements.size)
        assertTrue(ClinicianUseCaseTier.RESEARCH.uhdrElements.isEmpty())
        assertEquals(ClinicianUseCaseTier.FULL_CLINICAL, UHDRElement.HRV_TIME_SERIES.minimumTier)
        assertEquals(ClinicianUseCaseTier.ASSESS, UHDRElement.EEG_WAVEFORMS.minimumTier)
        assertEquals(ClinicianUseCaseTier.MONITOR, UHDRElement.SESSION_DURATION.minimumTier)
    }

    private fun repoRoot(): File {
        var dir = File(System.getProperty("user.dir")).absoluteFile
        while (!File(dir, "app/ios/NeurOne/Models/OTAModels.swift").exists()) {
            dir = dir.parentFile
                ?: error("could not locate the repo root from ${System.getProperty("user.dir")}")
        }
        return dir
    }
}
