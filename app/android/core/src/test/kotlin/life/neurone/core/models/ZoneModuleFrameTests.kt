package life.neurone.core.models

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

// Socket-keyed zone module frames (np_zone_notify.h v2) — Android parity with iOS
// ZoneModuleFrame.swift. Replaces the retired five-slot model (NP-HFE-002 OI-HFE2-02).
class ZoneModuleFrameTests {

    private fun b(vararg v: Int) = ByteArray(v.size) { v[it].toByte() }

    private val SNAP = ZoneFrameFormat.FLAG_SNAPSHOT
    private val LAST = ZoneFrameFormat.FLAG_LAST
    private val MAP = ZoneFrameFormat.FLAG_MAP

    private fun status(flags: Int, fragment: Int, vararg records: Triple<Int, Int, Int>): ByteArray =
        b(0x02, flags, fragment, records.size, *records.flatMap { listOf(it.first, it.second, it.third) }.toIntArray())

    private fun statusFrame(flags: Int, fragment: Int, vararg records: Triple<Int, Int, Int>) =
        ZoneModuleFrame.fromWire(status(flags, fragment, *records))!!

    // ── Decoding ─────────────────────────────────────────────────────────

    @Test
    fun decodesStatusRecords() {
        val f = statusFrame(SNAP or LAST, 0, Triple(12, 1, 0x01), Triple(47, 2, 0x01), Triple(128, 0, 0x00))
        assertTrue(f.isSnapshot)
        assertTrue(f.isLastFragment)
        assertEquals(listOf(12, 47, 128), f.records.map { it.socketId })
        assertEquals(ZoneModuleType.PBM_BASE, f.records[0].moduleType)
        assertEquals(ZoneModuleType.EEG, f.records[1].moduleType)
        assertFalse(f.records[2].isPresent)
    }

    @Test
    fun faultClearsPresenceEvenIfHubSetsBoth() {
        val r = statusFrame(LAST, 0, Triple(5, 2, 0x03)).records.single()
        assertTrue(r.hasFault)
        assertFalse(r.isPresent, "a faulted module is never present")
    }

    @Test
    fun unknownModuleTypeDecodesAsUnknown() {
        assertEquals(ZoneModuleType.UNKNOWN, statusFrame(LAST, 0, Triple(5, 99, 0x01)).records.single().moduleType)
    }

    @Test
    fun rejectsWrongVersionKindOrLength() {
        assertNull(ZoneModuleFrame.fromWire(b(0x01, LAST, 0, 1, 5, 1, 1)), "version 1 never deployed")
        assertNull(ZoneModuleFrame.fromWire(b(0x02, LAST or MAP, 0, 1, 5, 1, 1)), "map frame on status path")
        assertNull(ZoneModuleFrame.fromWire(b(0x02, LAST, 0, 2, 5, 1, 1)), "count exceeds payload")
        assertNull(ZoneModuleFrame.fromWire(b(0x02, LAST, 0)), "short header")
    }

    @Test
    fun rejectsSocketIdOutsideDomain() {
        assertNull(ZoneModuleFrame.fromWire(status(LAST, 0, Triple(0, 1, 1))))
        assertNull(ZoneModuleFrame.fromWire(status(LAST, 0, Triple(129, 1, 1))))
    }

    @Test
    fun decodesSocketMapWithSignedCoordinates() {
        // socket 74, wired, x = +10, y = -3, z = -120 (int16 LE).
        val f = SocketMapFrame.fromWire(b(0x02, SNAP or LAST or MAP, 0, 1, 74, 0x01, 10, 0, 0xFD, 0xFF, 0x88, 0xFF))!!
        val d = f.descriptors.single()
        assertEquals(74, d.socketId)
        assertEquals(SocketPosition(10, -3, -120), d.position)
        assertTrue(d.isWiredInShell)
        assertNull(SocketMapFrame.fromWire(status(LAST, 0, Triple(5, 1, 1))), "status frame on map path")
    }

    // ── Configuration ────────────────────────────────────────────────────

    @Test
    fun snapshotReplacesAndDeltaMerges() {
        var c = ZoneModuleConfiguration.EMPTY
        c = c.applying(statusFrame(SNAP or LAST, 0, Triple(1, 1, 1), Triple(2, 2, 1)))
        c = c.applying(statusFrame(LAST, 0, Triple(2, 0, 0)))
        assertTrue(c.isPresent(1))
        assertFalse(c.isPresent(2))
        assertEquals(listOf(2), c.emptySockets.map { it.socketId })
        c = c.applying(statusFrame(SNAP or LAST, 0, Triple(9, 1, 1)))
        assertEquals(listOf(9), c.orderedSockets.map { it.socketId }, "a snapshot is a complete claim")
    }

    @Test
    fun representsMoreThanFiveSockets() {
        val records = (1..80).map { Triple(it, 1, 1) }.toTypedArray()
        val c = ZoneModuleConfiguration.EMPTY.applying(statusFrame(SNAP or LAST, 0, *records))
        assertEquals(80, c.presentSockets.size)
    }

    // ── Reassembly ───────────────────────────────────────────────────────

    @Test
    fun snapshotCommitsOnlyOnLastFragment() {
        val a = ZoneModuleFrameAssembler()
        assertNull(a.accept(statusFrame(SNAP, 0, Triple(1, 1, 1))))
        assertTrue(a.isAccumulating)
        val done = assertNotNull(a.accept(statusFrame(SNAP or LAST, 1, Triple(2, 1, 1))))
        assertEquals(listOf(1, 2), done.records.map { it.socketId })
        assertFalse(a.isAccumulating)
    }

    @Test
    fun outOfOrderFragmentAbandonsRun() {
        val a = ZoneModuleFrameAssembler()
        a.accept(statusFrame(SNAP, 0, Triple(1, 1, 1)))
        assertNull(a.accept(statusFrame(SNAP or LAST, 2, Triple(2, 1, 1))))
        assertFalse(a.isAccumulating)
    }

    @Test
    fun deltaDuringSnapshotRunWins() {
        val a = ZoneModuleFrameAssembler()
        a.accept(statusFrame(SNAP, 0, Triple(1, 1, 1)))
        assertNull(a.accept(statusFrame(LAST, 0, Triple(1, 0, 0))), "delta deferred mid-run")
        val done = a.accept(statusFrame(SNAP or LAST, 1, Triple(2, 1, 1)))!!
        val c = ZoneModuleConfiguration.EMPTY.applying(done)
        assertFalse(c.isPresent(1), "a tile pulled mid-snapshot is not resurrected")
        assertTrue(c.isPresent(2))
    }

    @Test
    fun deltaOutsideRunPassesThrough() {
        val f = statusFrame(LAST, 0, Triple(3, 1, 1))
        assertEquals(f, ZoneModuleFrameAssembler().accept(f))
    }

    @Test
    fun runLongerThanAddressingDomainIsAbandoned() {
        val a = SocketMapFrameAssembler()
        val d = SocketDescriptor(1, SocketPosition(0, 0, -1), true)
        assertNull(a.accept(SocketMapFrame(false, 0, List(100) { d })))
        assertNull(a.accept(SocketMapFrame(true, 1, List(29) { d })))
        assertFalse(a.isAccumulating)
    }
}
