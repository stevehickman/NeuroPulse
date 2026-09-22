package life.neurone.core.ble

import life.neurone.core.models.CervicalPadStatus
import life.neurone.core.models.OtaPhase
import life.neurone.core.models.PacerPhase
import life.neurone.core.models.SessionStatus
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

class GattParserTests {

    // ── Canonical byte sequences (little-endian, matching hub firmware) ──

    @Test
    fun parseSessionStateDecodesLittleEndianUInt32() {
        // 0x0001E240 = 123456
        val data = byteArrayOf(0x40, 0xE2.toByte(), 0x01, 0x00)
        assertEquals(123_456L, GattParser.parseSessionState(data))
    }

    @Test
    fun parseSessionStateHandlesHighBitWithoutSignExtension() {
        val data = byteArrayOf(
            0xFF.toByte(), 0xFF.toByte(), 0xFF.toByte(), 0xFF.toByte(),
        )
        assertEquals(0xFFFFFFFFL, GattParser.parseSessionState(data))
    }

    @Test
    fun parseSessionStatusDecodesProtocolIdAndStatus() {
        val result = GattParser.parseSessionStatus(byteArrayOf(7, 1))
        assertEquals(7, result?.first)
        assertEquals(SessionStatus.RUNNING, result?.second)
    }

    @Test
    fun parseSessionStatusReturnsNullOnUnknownStatus() {
        assertNull(GattParser.parseSessionStatus(byteArrayOf(7, 99)))
    }

    @Test
    fun parseHrvCoherenceScalesByOneHundred() {
        // coherence×100 = 730 (0x02DA), RMSSD = 42
        val data = byteArrayOf(0xDA.toByte(), 0x02, 42, 0x00)
        val hrv = GattParser.parseHrvCoherence(data)
        assertEquals(7.30f, hrv!!.coherenceScore, 0.001f)
        assertEquals(42, hrv.rmssdMilliseconds)
    }

    @Test
    fun parsePacerPhaseDecodesPhaseAndPercent() {
        val result = GattParser.parsePacerPhase(byteArrayOf(1, 55))
        assertEquals(PacerPhase.EXHALE, result?.first)
        assertEquals(55, result?.second)
    }

    @Test
    fun parseImpedanceResultDecodesBitmask() {
        // 0x00FF — all 8 electrodes passed
        assertEquals(0xFF, GattParser.parseImpedanceResult(byteArrayOf(0xFF.toByte(), 0x00)))
    }

    @Test
    fun parseConsumableStatusDecodesFourCounts() {
        val data = byteArrayOf(1, 0, 2, 0, 3, 0, 0x2C, 0x01) // 1, 2, 3, 300
        assertEquals(listOf(1, 2, 3, 300), GattParser.parseConsumableStatus(data))
    }

    @Test
    fun parseZoneModuleStatusDecodesFiveSlots() {
        assertEquals(
            listOf(1, 2, 0, 4, 5),
            GattParser.parseZoneModuleStatus(byteArrayOf(1, 2, 0, 4, 5)),
        )
    }

    @Test
    fun parseFirmwareVersionDecodesBitfields() {
        // major=1, minor=2, patch=3 → raw = 0x00010203, LE bytes: 03 02 01 00
        val v = GattParser.parseFirmwareVersion(byteArrayOf(0x03, 0x02, 0x01, 0x00))
        assertEquals("1.2.3", v.toString())
    }

    @Test
    fun parseOtaStatusDecodesPhaseProgressAndError() {
        val pkt = GattParser.parseOtaStatus(byteArrayOf(0x04, 80, 0x00, 0x00))
        assertEquals(OtaPhase.VERIFIED, pkt!!.phase)
        assertEquals(80, pkt.progressPercent)
        assertEquals(false, pkt.isError)
    }

    // ── Short input returns null, never throws (per-parser) ─────────────

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

    @Test
    fun parseCervicalPadStatusAllPassHasNoMessage() {
        val status = GattParser.parseCervicalPadStatus(pad(0x00, 0x00, 0x01, 0x02))
        assertEquals(false, status?.hasFailure)
        assertNull(status?.message)
    }

    @Test
    fun parseCervicalPadStatusRejectsMalformedFrames() {
        assertNull(GattParser.parseCervicalPadStatus(pad(0x01, 0x00, 0x01)))       // short
        assertNull(GattParser.parseCervicalPadStatus(pad(0x04, 0x00, 0x01, 0x02))) // reserved bit
        assertNull(GattParser.parseCervicalPadStatus(pad(0x80, 0x00, 0x01, 0x02))) // reserved bit
        assertNull(GattParser.parseCervicalPadStatus(pad(0x01, 0x02, 0x01, 0x02))) // unknown check
        assertNull(GattParser.parseCervicalPadStatus(pad(0x01, 0x00, 0x00, 0x02))) // unknown side
        assertNull(GattParser.parseCervicalPadStatus(pad(0x01, 0x00, 0x01, 0x03))) // unknown side
    }

    @Test
    fun cervicalPadStatusIsNotARequiredCharacteristic() {
        assertEquals(false, GattUuids.all.contains(GattUuids.cvnsPadStatus))
    }
}
