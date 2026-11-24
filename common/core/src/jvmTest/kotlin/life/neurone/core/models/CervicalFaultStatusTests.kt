package life.neurone.core.models

import life.neurone.core.protocol.GattUuidStrings
import life.neurone.core.ble.GattParser
import life.neurone.core.protocol.ProfileTagState
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue
import java.util.UUID

// Parity with iOS GATTParserTests (CVNS_FAULT_STATUS + ledger) — NP-SW-FAULTMSG-001 P3/P4
// and the per-user cardiac scope.
class CervicalFaultStatusTests {

    /** Header [version, state, n, flags] + n × [counter LE u32, kind, side mask, 0, 0]. */
    private fun frame(state: Int, records: List<Triple<Long, Int, Int>>, flags: Int = 0x80): ByteArray {
        val b = mutableListOf(0x02, state, records.size, flags)
        for ((c, k, m) in records) {
            b += listOf((c and 0xFF).toInt(), ((c shr 8) and 0xFF).toInt(),
                ((c shr 16) and 0xFF).toInt(), ((c shr 24) and 0xFF).toInt(), k, m, 0, 0)
        }
        return ByteArray(b.size) { b[it].toByte() }
    }

    // @verifies REQ-SW03-03
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

    // @verifies REQ-SW03-03
    @Test
    fun counterIsUnsigned() {
        val s = assertNotNull(GattParser.parseCervicalFaultStatus(frame(0, listOf(Triple(0xFFFF_FFFEL, 1, 0)))))
        assertEquals(0xFFFF_FFFEL, s.records.single().sessionCounter)
    }

    // @verifies REQ-SW03-03
    @Test
    fun rejectsMalformed() {
        assertNull(CervicalFaultStatus.fromWire(byteArrayOf(0x01, 0, 0, 0)), "version 1 (never deployed) is refused")
        assertNull(CervicalFaultStatus.fromWire(byteArrayOf(0x03, 0, 0, 0)), "version")
        assertNull(CervicalFaultStatus.fromWire(byteArrayOf(0x02, 9, 0, 0)), "state")
        assertNull(CervicalFaultStatus.fromWire(frame(0, listOf(Triple(1L, 9, 0)))), "kind")
        assertNull(CervicalFaultStatus.fromWire(frame(0, listOf(Triple(1L, 1, 0x01)))), "side on non-pad")
        assertNull(CervicalFaultStatus.fromWire(frame(0, listOf(Triple(1L, 4, 0x04)))), "reserved side bit")
        val short = frame(0, listOf(Triple(1L, 1, 0)))
        assertNull(CervicalFaultStatus.fromWire(short.copyOf(short.size - 1)), "length")
        assertNull(CervicalFaultStatus.fromWire(frame(0, (1L..5L).map { Triple(it, 1, 0) })), "n > 4")
    }

    // @verifies REQ-SW03-03
    @Test
    fun flagsCarryPerUserBlockAndBlanketWarning() {
        fun flags(f: Int) = CervicalFaultStatus.fromWire(frame(0, emptyList(), f))
        assertEquals(false, flags(0x80)?.outstanding)
        assertEquals(true, flags(0x83)?.userBlocked)
        assertEquals(false, flags(0x83)?.outstandingForAnotherUser, "it is this user's own block")
        assertEquals(true, flags(0x82)?.outstandingForAnotherUser, "someone else's cutoff")
        assertNull(flags(0x04), "reserved flag bit")
        assertNull(flags(0x40), "reserved flag bit")
    }

    // @verifies REQ-FAULTMSG-01#d
    @Test
    fun nothingOutstandingIsKnownOnlyFromAValidReport() {
        fun flags(f: Int) = CervicalFaultStatus.fromWire(frame(0, emptyList(), f))
        assertEquals(true, flags(0x80)?.knownNothingOutstanding, "a valid report of nothing outstanding")
        assertEquals(false, flags(0x00)?.knownNothingOutstanding, "no valid report: unknown, not nothing")
        assertEquals(false, flags(0x00)?.reportValid)
        assertEquals(false, flags(0x82)?.knownNothingOutstanding, "someone is held")
        assertNull(flags(0x02), "a flag without the valid bit is not the hub's answer")
        assertNull(flags(0x01), "a flag without the valid bit is not the hub's answer")
    }

    private val serial = "NP-0001".encodeToByteArray()
    private val legacyId = "04030201-0000-4000-8000-000000000000"
    private val fresh = ProfileTagState(secret = "11".repeat(32), legacy = false)
    private val old = fresh.copy(legacy = true)
    private fun status(flags: Int) = assertNotNull(CervicalFaultStatus.fromWire(frame(0, emptyList(), flags)))

    // @verifies REQ-FAULTMSG-01#a
    @Test
    fun theTagIsThePerHelmetHmacFromTheSharedCore() {
        assertEquals(0x25DD_4FD5L, ActiveUserTag.derived(fresh, serial), "the core's reference vector")
        assertTrue(ActiveUserTag.derived(fresh, "NP-0002".encodeToByteArray()) != 0x25DD_4FD5L, "another helmet, another tag")
        assertNull(ActiveUserTag.derived(fresh, ByteArray(0)), "no helmet, no tag")
        assertNull(ActiveUserTag.derived(fresh.copy(secret = "zz"), serial), "a malformed secret names nobody")
        assertEquals(listOf<Byte>(0x01, 0x02, 0x03, 0x04), ActiveUserTag.toWire(0x0403_0201L).toList())
    }

    // @verifies REQ-FAULTMSG-01#c
    @Test
    fun aLegacyProfileKeepsItsOldTagUntilAValidReportSaysNobodyIsHeld() {
        val legacyTag = 0x0102_0304L
        assertEquals(legacyTag, ActiveUserTag.legacy(legacyId))
        assertEquals(ActiveUserTag.Choice(legacyTag), ActiveUserTag.choose(legacyId, old, serial, null), "no status yet")
        assertEquals(ActiveUserTag.Choice(legacyTag), ActiveUserTag.choose(legacyId, old, serial, status(0x00)), "no valid report")
        assertEquals(ActiveUserTag.Choice(legacyTag), ActiveUserTag.choose(legacyId, old, serial, status(0x82)), "someone is held")
        assertEquals(ActiveUserTag.Choice(legacyTag), ActiveUserTag.choose(legacyId, old, serial, status(0x83)), "this person is held")
        assertEquals(ActiveUserTag.Choice(0x25DD_4FD5L, moveTo = 0x25DD_4FD5L), ActiveUserTag.choose(legacyId, old, serial, status(0x80)),
            "a valid report of nothing outstanding: move, recording it first")
        val moved = old.copy(migrated = listOf(0x25DD_4FD5L))
        assertEquals(ActiveUserTag.Choice(0x25DD_4FD5L), ActiveUserTag.choose(legacyId, moved, serial, status(0x83)),
            "once moved, the old tag is never named to that helmet again, held or not")
        assertEquals(ActiveUserTag.Choice(legacyTag), ActiveUserTag.choose(legacyId, moved, "NP-0002".encodeToByteArray(), null),
            "a move is per helmet")
    }

    // @verifies REQ-FAULTMSG-01#c
    @Test
    fun aProfileMadeSinceTheChangeIsNeverNamedByAnOldTag() {
        assertEquals(ActiveUserTag.Choice(0x25DD_4FD5L), ActiveUserTag.choose(legacyId, fresh, serial, null))
        assertEquals(ActiveUserTag.Choice(0x25DD_4FD5L), ActiveUserTag.choose(legacyId, fresh, serial, status(0x82)))
    }

    // @verifies REQ-SW03-03
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

    // @verifies REQ-SW03-03
    @Test
    fun faultCharacteristicsAreNotRequired() {
        assertFalse(GattUuidStrings.REQUIRED_IDS.map(UUID::fromString).contains(UUID.fromString(GattUuidStrings.CVNS_FAULT_STATUS_ID)))
        assertFalse(GattUuidStrings.REQUIRED_IDS.map(UUID::fromString).contains(UUID.fromString(GattUuidStrings.CVNS_REENABLE_CONFIRM_ID)))
        assertFalse(GattUuidStrings.REQUIRED_IDS.map(UUID::fromString).contains(UUID.fromString(GattUuidStrings.ACTIVE_USER_ID)))
    }
}
