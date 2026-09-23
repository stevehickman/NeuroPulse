package life.neurone.core.models

import life.neurone.core.ble.GattParser
import life.neurone.core.ble.GattUuids
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

// Parity with iOS GATTParserTests (CVNS_FAULT_STATUS + ledger) — NP-SW-FAULTMSG-001 P3/P4
// and the per-user cardiac scope.
class CervicalFaultStatusTests {

    /** Header [version, state, n, flags] + n × [counter LE u32, kind, side mask, 0, 0]. */
    private fun frame(state: Int, records: List<Triple<Long, Int, Int>>, flags: Int = 0): ByteArray {
        val b = mutableListOf(0x01, state, records.size, flags)
        for ((c, k, m) in records) {
            b += listOf((c and 0xFF).toInt(), ((c shr 8) and 0xFF).toInt(),
                ((c shr 16) and 0xFF).toInt(), ((c shr 24) and 0xFF).toInt(), k, m, 0, 0)
        }
        return ByteArray(b.size) { b[it].toByte() }
    }

    @Test
    fun parsesRecordsAndMessages() {
        val s = assertNotNull(GattParser.parseCervicalFaultStatus(
            frame(2, listOf(Triple(41L, 2, 0), Triple(42L, 1, 0), Triple(43L, 4, 0x02)))))
        assertEquals(CervicalFaultStatus.ReenableState.AWAIT_CONFIRM, s.reenableState)
        assertEquals(
            listOf(CervicalFaultRecord.Kind.SIGNAL_LOST, CervicalFaultRecord.Kind.HEART_RATE_CHANGE,
                CervicalFaultRecord.Kind.PAD_CONTACT),
            s.records.map { it.kind })
        assertEquals(CervicalFaultRecord.Message.HR_CHANGE, s.records[1].message)
        assertEquals(CervicalFaultRecord.Message.PAD_RIGHT, s.records[2].message)
    }

    @Test
    fun counterIsUnsigned() {
        val s = assertNotNull(GattParser.parseCervicalFaultStatus(frame(0, listOf(Triple(0xFFFF_FFFEL, 1, 0)))))
        assertEquals(0xFFFF_FFFEL, s.records.single().sessionCounter)
    }

    @Test
    fun rejectsMalformed() {
        assertNull(CervicalFaultStatus.fromWire(byteArrayOf(0x02, 0, 0, 0)), "version")
        assertNull(CervicalFaultStatus.fromWire(byteArrayOf(0x01, 9, 0, 0)), "state")
        assertNull(CervicalFaultStatus.fromWire(frame(0, listOf(Triple(1L, 9, 0)))), "kind")
        assertNull(CervicalFaultStatus.fromWire(frame(0, listOf(Triple(1L, 1, 0x01)))), "side on non-pad")
        assertNull(CervicalFaultStatus.fromWire(frame(0, listOf(Triple(1L, 4, 0x04)))), "reserved side bit")
        val short = frame(0, listOf(Triple(1L, 1, 0)))
        assertNull(CervicalFaultStatus.fromWire(short.copyOf(short.size - 1)), "length")
        assertNull(CervicalFaultStatus.fromWire(frame(0, (1L..5L).map { Triple(it, 1, 0) })), "n > 4")
    }

    @Test
    fun flagsCarryPerUserBlockAndBlanketWarning() {
        fun flags(f: Int) = CervicalFaultStatus.fromWire(frame(0, emptyList(), f))
        assertEquals(false, flags(0x00)?.outstanding)
        assertEquals(true, flags(0x03)?.userBlocked)
        assertEquals(false, flags(0x03)?.outstandingForAnotherUser, "it is this user's own block")
        assertEquals(true, flags(0x02)?.outstandingForAnotherUser, "someone else's cutoff")
        assertNull(flags(0x04), "reserved flag bit")
        assertNull(flags(0x80), "reserved flag bit")
    }

    @Test
    fun activeUserTagMatchesIosAndAvoidsReservedValues() {
        assertNull(ActiveUserTag.from(null))
        assertNull(ActiveUserTag.from("not-a-uuid"), "an id the device cannot be told is not sent")
        assertEquals(0x0102_0304L, ActiveUserTag.from("04030201-0000-4000-8000-000000000000"),
            "first four bytes, little-endian — same derivation as iOS")
        assertEquals(1L, ActiveUserTag.from("00000000-0000-4000-8000-000000000000"))
        assertEquals(0xFFFF_FFFEL, ActiveUserTag.from("ffffffff-0000-4000-8000-000000000000"))
        assertEquals(listOf<Byte>(0x01, 0x02, 0x03, 0x04), ActiveUserTag.toWire(0x0403_0201L).toList())
    }

    @Test
    fun ledgerAcknowledgementAndCounterReset() {
        val s = assertNotNull(CervicalFaultStatus.fromWire(frame(0, listOf(Triple(10L, 2, 0), Triple(11L, 1, 0)))))
        var ledger = CervicalFaultLedger()
        assertEquals(2, ledger.unacknowledged(s.records).size)
        assertTrue(ledger.blocksCervicalRestart(s.records))

        ledger = ledger.acknowledge(s.records)
        assertTrue(ledger.unacknowledged(s.records).isEmpty())
        assertFalse(ledger.blocksCervicalRestart(s.records))

        val newer = assertNotNull(CervicalFaultStatus.fromWire(frame(0, listOf(Triple(11L, 1, 0), Triple(12L, 2, 0)))))
        assertEquals(listOf(12L), ledger.unacknowledged(newer.records).map { it.sessionCounter })

        val other = assertNotNull(CervicalFaultStatus.fromWire(frame(0, listOf(Triple(3L, 1, 0)))))
        assertEquals(1, ledger.unacknowledged(other.records).size, "lower counter: another device")
    }

    @Test
    fun faultCharacteristicsAreNotRequired() {
        assertFalse(GattUuids.all.contains(GattUuids.cvnsFaultStatus))
        assertFalse(GattUuids.all.contains(GattUuids.cvnsReenableConfirm))
        assertFalse(GattUuids.all.contains(GattUuids.activeUser))
    }
}
