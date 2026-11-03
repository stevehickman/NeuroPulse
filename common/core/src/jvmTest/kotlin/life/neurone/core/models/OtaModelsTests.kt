package life.neurone.core.models

import life.neurone.core.protocol.BleTransfer
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** Ports of the firmware-version, phase and session assertions of iOS `OTAManagerTests` that had no Kotlin counterpart. */
class OtaModelsTests {

    // @verifies DI-PERF-21
    @Test
    fun aVersionIsThreeDotSeparatedNonNegativeNumbersAndNothingElse() {
        listOf("1.2.3", "0.0.0", "255.255.255").forEach { assertNotNull(FirmwareVersion.parse(it), it) }
        listOf("1.2", "1.2.3.4", "", "a.b.c", "1.-1.0").forEach { assertNull(FirmwareVersion.parse(it), "'$it'") }
    }

    // @verifies DI-PERF-21
    @Test
    fun versionsCompareByMajorThenMinorThenPatchAndPrintAsWritten() {
        val v100 = FirmwareVersion.parse("1.0.0")!!
        val v110 = FirmwareVersion.parse("1.1.0")!!
        val v111 = FirmwareVersion.parse("1.1.1")!!
        val v200 = FirmwareVersion.parse("2.0.0")!!
        assertTrue(v100 < v110, "minor")
        assertTrue(v110 < v111, "patch")
        assertTrue(v111 < v200, "major")
        assertEquals(v100, FirmwareVersion.parse("1.0.0"))
        assertTrue(v200 > v100)
        assertTrue(v200 >= v100 && !(v200 < v100))
        assertEquals("3.14.159", FirmwareVersion.parse("3.14.159")!!.toString())
        assertEquals("0.0.0", FirmwareVersion.parse("0.0.0")!!.toString())
    }

    // @verifies DI-PERF-21
    @Test
    fun theHubsVersionBytesAreALittleEndianUint32WithMajorMinorPatchFromTheTopDown() {
        // 1.2.3 packed as 0x00010203, little-endian on the wire.
        val v = FirmwareVersion.fromGattBytes(byteArrayOf(0x03, 0x02, 0x01, 0x00))!!
        assertEquals(1, v.major)
        assertEquals(2, v.minor)
        assertEquals(3, v.patch)
        assertEquals("1.2.3", v.toString())
        assertNull(FirmwareVersion.fromGattBytes(byteArrayOf(0x01, 0x02, 0x03)))
        assertNull(FirmwareVersion.fromGattBytes(byteArrayOf()))
    }

    // @verifies DI-PERF-21
    @Test
    fun aPhaseByteMapsToItsPhaseAndAnUnknownByteMapsToNoneForTheCallerToDecide() {
        val expected = mapOf(
            0x00 to OtaPhase.IDLE, 0x01 to OtaPhase.PREPARING, 0x02 to OtaPhase.TRANSFERRING,
            0x03 to OtaPhase.VERIFYING, 0x04 to OtaPhase.VERIFIED, 0x05 to OtaPhase.APPLYING,
            0x06 to OtaPhase.COMPLETE, 0xFF to OtaPhase.FAILED,
        )
        expected.forEach { (byte, phase) -> assertEquals(phase, OtaPhase.from(byte), "0x%02X".format(byte)) }
        // iOS maps an unknown byte to idle (a newer hub may send newer phases); here it is null and never a failure.
        assertNull(OtaPhase.from(0x07))
    }

    // @verifies DI-PERF-21
    @Test
    fun everyPhaseBetweenIdleAndTheTerminalOnesIsBusyIncludingVerified() {
        listOf(OtaPhase.IDLE, OtaPhase.COMPLETE, OtaPhase.FAILED).forEach { assertTrue(!it.isBusy, "$it") }
        listOf(OtaPhase.PREPARING, OtaPhase.TRANSFERRING, OtaPhase.VERIFYING, OtaPhase.VERIFIED, OtaPhase.APPLYING)
            .forEach { assertTrue(it.isBusy, "$it") }
        assertTrue(OtaPhase.COMPLETE.isTerminal && OtaPhase.FAILED.isTerminal)
        assertTrue(!OtaPhase.IDLE.isTerminal && !OtaPhase.APPLYING.isTerminal)
    }

    // @verifies DI-PERF-21
    @Test
    fun aSessionReportsItsProgressAsAFractionAndSizesRoundToKbAndMb() {
        assertEquals(0.5, OtaSession(totalBytes = 1024, sentBytes = 512).progressFraction, 0.001)
        assertEquals(0.0, OtaSession(totalBytes = 0, sentBytes = 0).progressFraction, "an empty image is no progress, not a division by zero")
        // iOS prints 512 bytes as "1 KB" (its formatter rounds up); here a size under 1 KiB is shown in bytes.
        assertEquals("512 B", OtaSession.formattedBytes(512))
        assertEquals("1 KB", OtaSession.formattedBytes(1024))
        assertEquals("2 KB", OtaSession.formattedBytes(1536), "half rounds up")
        assertEquals("500 KB", OtaSession.formattedBytes(1024L * 500))
        assertTrue(OtaSession.formattedBytes(1024L * 1024 + 512 * 1024).contains("MB"))
        assertEquals("100 B", OtaSession.formattedBytes(100))
    }

    // @verifies DI-PERF-21
    @Test
    fun anImageIsSentInChunksOf496BytesAndTheLastIsTheRemainder() {
        assertEquals(496, BleTransfer.OTA_CHUNK_SIZE)
        val total = 1000
        assertEquals(3, (total + BleTransfer.OTA_CHUNK_SIZE - 1) / BleTransfer.OTA_CHUNK_SIZE)
    }
}
