package life.neurone.core.consumable

import life.neurone.core.common.InMemoryKeyValueStore
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

// OI-ACC-08 (#381): the CONSUMABLE_STATUS reset write and its offline queue. The wire value is
// the one the hub's np_cons_on_reset_write() accepts (np_consumables_tests).
class ConsumableResetTests {

    @Test
    fun wireIsTheKindIndexForTheFourCountedKinds() {
        for (kind in ConsumableKind.entries) {
            assertContentEquals(byteArrayOf(kind.rawValue.toByte()), ConsumableResetWire.encode(kind.rawValue))
        }
        assertNull(ConsumableResetWire.encode(-1))
        assertNull(ConsumableResetWire.encode(ConsumableKind.entries.size))
    }

    @Test
    fun queueSurvivesARestartAndDrainsOnce() {
        val kv = InMemoryKeyValueStore()
        ConsumableResetQueue(kv).apply { add(3); add(0); add(3); add(7) }

        val restarted = ConsumableResetQueue(kv)
        assertEquals(listOf(0, 3), restarted.pending) // deduplicated, the unknown kind dropped

        val written = mutableListOf<Int>()
        restarted.drain { written += it.single().toInt() }
        assertEquals(listOf(0, 3), written)
        assertTrue(restarted.pending.isEmpty())

        restarted.drain { written += -1 } // nothing left: no second write
        assertEquals(listOf(0, 3), written)
    }
}
