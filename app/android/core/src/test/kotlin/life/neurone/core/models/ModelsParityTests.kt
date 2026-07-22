package life.neurone.core.models

import life.neurone.core.ble.CalibrationOpcode
import life.neurone.core.ble.GattUuids
import life.neurone.core.ble.OtaOpcode
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

// Parity tests — these values are wire/UUID contracts shared with the hub
// firmware and the iOS app (GATTCharacteristics.swift, SessionState.swift,
// OTAModels.swift). A failure here means platform divergence.

class ModelsParityTests {

    // ── GATT UUIDs byte-identical to iOS NPUUID (ISC-9, ISC-10) ─────────

    @Test
    fun gattUuidsMatchIosNpuuidStrings() {
        assertEquals("4e455550-0001-1000-8000-00805f9b34fb", GattUuids.service.toString())
        assertEquals("4e455550-0002-1000-8000-00805f9b34fb", GattUuids.sessionState.toString())
        assertEquals("4e455550-0003-1000-8000-00805f9b34fb", GattUuids.sessionStatus.toString())
        assertEquals("4e455550-0004-1000-8000-00805f9b34fb", GattUuids.hrvCoherence.toString())
        assertEquals("4e455550-0005-1000-8000-00805f9b34fb", GattUuids.pacerPhase.toString())
        assertEquals("4e455550-0006-1000-8000-00805f9b34fb", GattUuids.impedanceResult.toString())
        assertEquals("4e455550-0007-1000-8000-00805f9b34fb", GattUuids.consumableStatus.toString())
        assertEquals("4e455550-0008-1000-8000-00805f9b34fb", GattUuids.protocolUpload.toString())
        assertEquals("4e455550-0009-1000-8000-00805f9b34fb", GattUuids.edfRequest.toString())
        assertEquals("4e455550-000a-1000-8000-00805f9b34fb", GattUuids.otaCommand.toString())
        assertEquals("4e455550-000b-1000-8000-00805f9b34fb", GattUuids.otaStatus.toString())
        assertEquals("4e455550-000c-1000-8000-00805f9b34fb", GattUuids.calibrationCmd.toString())
        assertEquals("4e455550-000d-1000-8000-00805f9b34fb", GattUuids.zoneModuleStatus.toString())
        assertEquals("4e455550-000e-1000-8000-00805f9b34fb", GattUuids.shdrUploadStatus.toString())
        assertEquals("4e455550-000f-1000-8000-00805f9b34fb", GattUuids.sessionStop.toString())
        assertEquals("4e455550-0010-1000-8000-00805f9b34fb", GattUuids.warrantyToken.toString())
        assertEquals("4e455550-0011-1000-8000-00805f9b34fb", GattUuids.firmwareVersion.toString())
    }

    @Test
    fun gattUuidsAllContainsExactlyFourteenAndExcludesOptionals() {
        assertEquals(14, GattUuids.all.size)
        assertFalse(GattUuids.warrantyToken in GattUuids.all)
        assertFalse(GattUuids.firmwareVersion in GattUuids.all)
        assertTrue(GattUuids.sessionStop in GattUuids.all)
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

    @Test
    fun otaPhaseBusyAndTerminalSemantics() {
        assertTrue(OtaPhase.RECEIVING.isBusy)
        assertTrue(OtaPhase.VERIFYING.isBusy)
        assertFalse(OtaPhase.VERIFIED.isBusy)
        assertTrue(OtaPhase.COMPLETE.isTerminal)
        assertTrue(OtaPhase.FAILED.isTerminal)
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
        assertEquals(496, OtaSession.CHUNK_SIZE)
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
}
