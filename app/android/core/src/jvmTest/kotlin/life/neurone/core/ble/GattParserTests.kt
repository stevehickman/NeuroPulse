package life.neurone.core.ble

import life.neurone.core.protocol.GattUuidStrings
import life.neurone.core.models.CervicalPadStatus
import life.neurone.core.models.OtaPhase
import life.neurone.core.models.HubSessionState
import life.neurone.core.models.PacerPhase
import life.neurone.core.models.RampPhase
import life.neurone.core.models.SessionProgress
import life.neurone.core.models.SessionStatus
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue
import java.util.UUID

class GattParserTests {

    // ── Canonical byte sequences (little-endian, matching hub firmware) ──

    // @verifies REQ-SW03-01
    @Test
    fun parseSessionStateDecodesLittleEndianUInt32() {
        // 0x0001E240 = 123456
        val data = byteArrayOf(0x40, 0xE2.toByte(), 0x01, 0x00)
        assertEquals(123_456L, GattParser.parseSessionState(data))
    }

    // @verifies REQ-SW03-01
    @Test
    fun parseSessionStateHandlesHighBitWithoutSignExtension() {
        val data = byteArrayOf(
            0xFF.toByte(), 0xFF.toByte(), 0xFF.toByte(), 0xFF.toByte(),
        )
        assertEquals(0xFFFFFFFFL, GattParser.parseSessionState(data))
    }

    // @verifies REQ-SW03-01
    @Test
    fun parseSessionStatusDecodesProtocolIdAndStatus() {
        val result = GattParser.parseSessionStatus(byteArrayOf(7, 1))
        assertEquals(7, result?.first)
        assertEquals(SessionStatus.RUNNING, result?.second)
    }

    // @verifies REQ-SW03-01
    @Test
    fun parseSessionStatusReturnsNullOnUnknownStatus() {
        assertNull(GattParser.parseSessionStatus(byteArrayOf(7, 99)))
    }

    // @verifies REQ-SW03-01
    @Test
    fun parseHrvCoherenceScalesByOneHundred() {
        // coherence×100 = 730 (0x02DA), RMSSD = 42
        val data = byteArrayOf(0xDA.toByte(), 0x02, 42, 0x00)
        val hrv = GattParser.parseHrvCoherence(data)
        assertEquals(7.30f, hrv!!.coherenceScore, 0.001f)
        assertEquals(42, hrv.rmssdMilliseconds)
    }

    // @verifies REQ-SW03-01
    @Test
    fun parsePacerPhaseDecodesPhaseAndPercent() {
        val result = GattParser.parsePacerPhase(byteArrayOf(1, 55))
        assertEquals(PacerPhase.EXHALE, result?.first)
        assertEquals(55, result?.second)
    }

    // @verifies REQ-SW03-01
    @Test
    fun parseImpedanceResultDecodesBitmask() {
        // 0x00FF — all 8 electrodes passed
        assertEquals(0xFF, GattParser.parseImpedanceResult(byteArrayOf(0xFF.toByte(), 0x00)))
    }

    // @verifies REQ-SW03-04
    @Test
    fun parseConsumableStatusDecodesFourCounts() {
        val data = byteArrayOf(1, 0, 2, 0, 3, 0, 0x2C, 0x01) // 1, 2, 3, 300
        assertEquals(listOf(1, 2, 3, 300), GattParser.parseConsumableStatus(data))
    }

    // @verifies REQ-SW03-02
    @Test
    fun parseZoneModuleStatusRejectsRetiredFiveSlotPayload() {
        // The retired one-byte-per-slot payload has no version byte, so its first slot is read as
        // the version and anything but 0x02 is rejected.
        assertNull(GattParser.parseZoneModuleStatus(byteArrayOf(1, 2, 0, 4, 5)))
        assertNull(GattParser.parseZoneModuleStatus(byteArrayOf(0, 0, 0, 0, 0)))
        // A first slot of 2 passes the version check, but 5 bytes cannot hold a 4-byte header
        // plus one 3-byte record: it is rejected, or at most decodes to zero records.
        assertNull(GattParser.parseZoneModuleStatus(byteArrayOf(2, 2, 3, 4, 5)))
        assertEquals(
            emptyList(),
            GattParser.parseZoneModuleStatus(byteArrayOf(2, 2, 0, 0, 5))?.records ?: emptyList(),
        )
    }

    // @verifies REQ-SW03-02
    @Test
    fun parseZoneModuleStatusDecodesSocketKeyedRecords() {
        // v2, delta+last, fragment 0, 1 record: socket 47, EEG, present.
        val frame = GattParser.parseZoneModuleStatus(byteArrayOf(0x02, 0x02, 0x00, 0x01, 47, 0x02, 0x01))!!
        assertEquals(listOf(47), frame.records.map { it.socketId })
        assertTrue(frame.records.single().isPresent)
    }

    // @verifies REQ-SW03-01
    @Test
    fun parseFirmwareVersionDecodesBitfields() {
        // major=1, minor=2, patch=3 → raw = 0x00010203, LE bytes: 03 02 01 00
        val v = GattParser.parseFirmwareVersion(byteArrayOf(0x03, 0x02, 0x01, 0x00))
        assertEquals("1.2.3", v.toString())
    }

    // @verifies REQ-SW03-01
    @Test
    fun parseOtaStatusDecodesPhaseProgressAndError() {
        val pkt = GattParser.parseOtaStatus(byteArrayOf(0x04, 80, 0x00, 0x00))
        assertEquals(OtaPhase.VERIFIED, pkt!!.phase)
        assertEquals(80, pkt.progressPercent)
        assertEquals(false, pkt.isError)
    }

    // @verifies REQ-SW03-01
    @Test
    fun parseOtaStatusDecodesEveryPhaseByte() {
        // Same bytes as iOS GATTParserTests.testParseOTAStatus: 0x02 is TRANSFERRING, and a
        // 0x05 packet carrying error 0x0102 is APPLYING.
        val expected = mapOf(
            0x00 to OtaPhase.IDLE, 0x01 to OtaPhase.PREPARING, 0x02 to OtaPhase.TRANSFERRING,
            0x03 to OtaPhase.VERIFYING, 0x04 to OtaPhase.VERIFIED, 0x05 to OtaPhase.APPLYING,
            0x06 to OtaPhase.COMPLETE, 0xFF to OtaPhase.FAILED,
        )
        for ((byte, phase) in expected) {
            val pkt = GattParser.parseOtaStatus(byteArrayOf(byte.toByte(), 0x37, 0x00, 0x00))
            assertEquals(phase, pkt!!.phase, "phase byte 0x%02X".format(byte))
        }
        val err = GattParser.parseOtaStatus(byteArrayOf(0x05, 0x00, 0x02, 0x01))!!
        assertEquals(OtaPhase.APPLYING, err.phase)
        assertEquals(0x0102, err.errorCode)
        assertTrue(err.isError)
        assertNull(GattParser.parseOtaStatus(byteArrayOf(0x07, 0, 0, 0))!!.phase)
    }

    // ── Short input returns null, never throws (per-parser) ─────────────

    // @verifies REQ-SW03-01
    @Test
    fun allParsersReturnNullOnShortInput() {
        val short = byteArrayOf(0x01)
        assertNull(GattParser.parseSessionState(short))
        assertNull(GattParser.parseSessionStatus(byteArrayOf()))
        assertNull(GattParser.parseHrvCoherence(short))
        assertNull(GattParser.parsePacerPhase(byteArrayOf()))
        assertNull(GattParser.parseImpedanceResult(short))
        assertNull(GattParser.parseConsumableStatus(byteArrayOf(1, 2, 3)))
        assertNull(GattParser.parseZoneModuleStatus(byteArrayOf(1, 2)))
        assertNull(GattParser.parseFirmwareVersion(short))
        assertNull(GattParser.parseOtaStatus(byteArrayOf(1, 2)))
    }

    // ── CVNS_PAD_STATUS (mask + check + per-electrode side) — OI-ACC-07 ──
    // Side codes: 0x01 = left of neck, 0x02 = right. Bilateral montage = [left, right].

    private fun pad(vararg b: Int) = ByteArray(b.size) { b[it].toByte() }

    // @verifies REQ-SW03-03
    @Test
    fun parseCervicalPadStatusNamesTheFailingSide() {
        val status = GattParser.parseCervicalPadStatus(pad(0x02, 0x00, 0x01, 0x02))
        assertEquals(
            CervicalPadStatus(listOf(CervicalPadStatus.NeckSide.RIGHT), CervicalPadStatus.Check.PRE_ENABLE),
            status,
        )
        assertEquals(CervicalPadStatus.Message.PRE_RIGHT, status?.message)
        assertEquals(setOf(CervicalPadStatus.NeckSide.RIGHT), status?.failingSides)
    }

    // @verifies REQ-SW03-03
    @Test
    fun parseCervicalPadStatusMessageVariants() {
        fun m(vararg b: Int) = GattParser.parseCervicalPadStatus(pad(*b))?.message
        assertEquals(CervicalPadStatus.Message.PRE_LEFT, m(0x01, 0x00, 0x01, 0x02))
        assertEquals(CervicalPadStatus.Message.PRE_BOTH, m(0x03, 0x00, 0x01, 0x02))
        assertEquals(CervicalPadStatus.Message.MID_LEFT, m(0x01, 0x01, 0x01, 0x02))
        assertEquals(CervicalPadStatus.Message.MID_RIGHT, m(0x02, 0x01, 0x01, 0x02))
        assertEquals(CervicalPadStatus.Message.MID_BOTH, m(0x03, 0x01, 0x01, 0x02))
    }

    /** Unilateral: both pads on one side, which comes from the hub — electrode 1 is on the RIGHT here. */
    // @verifies REQ-SW03-03
    @Test
    fun parseCervicalPadStatusUnilateralTakesSideFromHub() {
        assertEquals(
            CervicalPadStatus.Message.PRE_RIGHT,
            GattParser.parseCervicalPadStatus(pad(0x01, 0x00, 0x02, 0x02))?.message,
        )
        val both = GattParser.parseCervicalPadStatus(pad(0x03, 0x00, 0x02, 0x02))
        assertEquals(CervicalPadStatus.Message.PRE_BOTH, both?.message)
        assertEquals(setOf(CervicalPadStatus.NeckSide.RIGHT), both?.failingSides)
    }

    // @verifies REQ-SW03-03
    @Test
    fun parseCervicalPadStatusAllPassHasNoMessage() {
        val status = GattParser.parseCervicalPadStatus(pad(0x00, 0x00, 0x01, 0x02))
        assertEquals(false, status?.hasFailure)
        assertNull(status?.message)
    }

    // @verifies REQ-SW03-03
    @Test
    fun parseCervicalPadStatusRejectsMalformedFrames() {
        assertNull(GattParser.parseCervicalPadStatus(pad(0x01, 0x00, 0x01)))       // short
        assertNull(GattParser.parseCervicalPadStatus(pad(0x04, 0x00, 0x01, 0x02))) // reserved bit
        assertNull(GattParser.parseCervicalPadStatus(pad(0x80, 0x00, 0x01, 0x02))) // reserved bit
        assertNull(GattParser.parseCervicalPadStatus(pad(0x01, 0x02, 0x01, 0x02))) // unknown check
        assertNull(GattParser.parseCervicalPadStatus(pad(0x01, 0x00, 0x00, 0x02))) // unknown side
        assertNull(GattParser.parseCervicalPadStatus(pad(0x01, 0x00, 0x01, 0x03))) // unknown side
    }

    // @verifies REQ-SW03-03
    @Test
    fun cervicalPadStatusIsNotARequiredCharacteristic() {
        assertEquals(false, GattUuidStrings.REQUIRED_IDS.map(UUID::fromString).contains(UUID.fromString(GattUuidStrings.CVNS_PAD_STATUS_ID)))
    }

    private fun le32(v: Long) = byteArrayOf(v.toByte(), (v shr 8).toByte(), (v shr 16).toByte(), (v shr 24).toByte())

    private fun progressFrame(
        state: Int, part: Int, mask: Int, elapsed: Long, remaining: Long, total: Long,
        ramp: Int, pct: Int, rampLeft: Long,
    ): ByteArray = byteArrayOf(state.toByte(), part.toByte(), mask.toByte(), (mask shr 8).toByte()) +
        le32(elapsed) + le32(remaining) + le32(total) + byteArrayOf(ramp.toByte(), pct.toByte()) + le32(rampLeft)

    // @verifies SWR-SS-07
    @Test
    fun parseSessionProgressDecodesTheHelmetsFigures() {
        val p = GattParser.parseSessionProgress(progressFrame(3, 2, 0x0105, 61_000, 539_000, 600_000, 0, 0, 0))!!
        assertEquals(HubSessionState.RUNNING, p.state)
        assertEquals(2, p.activePart)
        assertEquals(0x0105, p.activeModalityMask)
        assertEquals(61_000L, p.elapsedMs)
        assertEquals(539_000L, p.remainingMs)
        assertEquals(600_000L, p.totalMs)
        assertEquals(RampPhase.NONE, p.rampPhase)
    }

    // @verifies SWR-SS-07
    @Test
    fun noRunningPartAndAnUnknownRampTimeAreNullNotNumbers() {
        val p = GattParser.parseSessionProgress(progressFrame(4, 0xFF, 0, 10_000, 90_000, 100_000, 3, 40, 0xFFFFFFFFL))!!
        assertNull(p.activePart)
        assertNull(p.rampRemainingMs)
        assertEquals(RampPhase.PRECHECK, p.rampPhase)
        assertTrue(p.isPaused)
    }

    // @verifies SWR-SS-07
    @Test
    fun aMalformedProgressFrameIsDroppedWhole() {
        assertNull(GattParser.parseSessionProgress(ByteArray(21)))
        assertNull(GattParser.parseSessionProgress(ByteArray(23)))
        assertNull(GattParser.parseSessionProgress(progressFrame(99, 0, 0, 0, 0, 0, 0, 0, 0)), "an unknown state")
        assertNull(GattParser.parseSessionProgress(progressFrame(3, 0, 0, 0, 0, 0, 9, 0, 0)), "an unknown ramp phase")
    }

    // @verifies SWR-SS-07
    @Test
    fun elapsedIsShownAsAClock() {
        assertEquals("0:00", SessionProgress.formatClock(0))
        assertEquals("1:01", SessionProgress.formatClock(61_000))
        assertEquals("1:00:00", SessionProgress.formatClock(3_600_000))
        assertEquals("8:59:59", SessionProgress.formatClock(32_399_999))
    }

    // @verifies SWR-SS-05
    @Test
    fun aSessionIsInProgressWhileRunningPausedOrStoppingAndNotOtherwise() {
        fun active(state: Int) = GattParser.parseSessionProgress(progressFrame(state, 0, 0, 0, 0, 0, 0, 0, 0))!!.isActive
        assertTrue(active(3))
        assertTrue(active(4))
        assertTrue(active(5))
        assertEquals(false, active(0))
        assertEquals(false, active(6))
        assertEquals(false, active(7))
    }
}
