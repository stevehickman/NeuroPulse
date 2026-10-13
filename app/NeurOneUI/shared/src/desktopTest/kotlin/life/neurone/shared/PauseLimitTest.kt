package life.neurone.shared

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.runTest
import life.neurone.core.common.UUID
import life.neurone.core.protocol.GattUuidStrings
import life.neurone.shared.ble.AdapterState
import life.neurone.shared.ble.BleCentral
import life.neurone.shared.ble.BleCentralListener
import life.neurone.shared.ble.NeurOneGattManager
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The hardware owner's pause time limit, read from and written to the helmet (SWR-SS-05, SWR-WO-01). */
@OptIn(ExperimentalCoroutinesApi::class)
class PauseLimitTest {
    private class RecordingCentral : BleCentral {
        override val adapterState = AdapterState.ON
        val log = mutableListOf<String>()
        val writes = mutableListOf<Pair<UUID, List<Byte>>>()
        override fun setListener(listener: BleCentralListener) {}
        override fun startScan(serviceUuid: UUID) {}
        override fun stopScan() {}
        override fun connect(deviceId: String) {}
        override fun disconnect() {}
        override fun discoverCharacteristics(serviceUuid: UUID, uuids: List<UUID>) {}
        override fun enableNotifications(uuid: UUID) {}
        override fun read(uuid: UUID) { log += "read" }
        override fun write(uuid: UUID, value: ByteArray) { log += "write"; writes += uuid to value.toList() }
    }

    private val limitId = UUID.fromString(GattUuidStrings.PAUSE_LIMIT_ID)

    // @verifies SWR-SS-05
    @Test fun the_limit_is_unknown_until_the_helmet_is_read_and_a_helmet_without_it_offers_nothing() = runTest {
        val central = RecordingCentral()
        val manager = NeurOneGattManager(central, TestScope(testScheduler))
        assertNull(manager.pauseLimitSeconds.value)
        assertFalse(manager.pauseLimitPublished.value)
        assertFalse(manager.writePauseLimit(600L), "a helmet that does not publish it takes no write")
        assertTrue(central.writes.isEmpty())
    }

    // @verifies SWR-SS-05
    @Test fun a_helmet_that_publishes_it_is_read_at_link_and_the_value_is_the_helmets_not_the_apps() = runTest {
        val central = RecordingCentral()
        val manager = NeurOneGattManager(central, TestScope(testScheduler))
        manager.onCharacteristicsDiscovered(setOf(limitId))
        assertTrue(manager.pauseLimitPublished.value)
        assertTrue("read" in central.log)
        manager.onCharacteristicRead(limitId, byteArrayOf(0x2C, 0x01, 0, 0))
        assertEquals(300L, manager.pauseLimitSeconds.value)
        manager.onCharacteristicRead(limitId, byteArrayOf(1, 2, 3))
        assertNull(manager.pauseLimitSeconds.value, "a malformed frame is not believed")
    }

    // @verifies SWR-SS-05
    @Test fun a_write_sends_four_little_endian_bytes_and_is_followed_by_a_read() = runTest {
        val central = RecordingCentral()
        val manager = NeurOneGattManager(central, TestScope(testScheduler))
        manager.onCharacteristicsDiscovered(setOf(limitId))
        central.log.clear()
        assertTrue(manager.writePauseLimit(1_800L))
        assertEquals(listOf(limitId to listOf<Byte>(0x08, 0x07, 0, 0)), central.writes)
        assertEquals(listOf("write", "read"), central.log, "the screen then shows what the helmet holds")
        assertFalse(manager.writePauseLimit(-1L))
        assertFalse(manager.writePauseLimit(0x1_0000_0000L))
        assertEquals(1, central.writes.size, "a value that does not fit is never sent")
    }

    // @verifies SWR-SS-05
    @Test fun a_dropped_link_forgets_the_limit_since_the_next_helmet_may_be_another() = runTest {
        val central = RecordingCentral()
        val manager = NeurOneGattManager(central, TestScope(testScheduler))
        manager.onCharacteristicsDiscovered(setOf(limitId))
        manager.onCharacteristicRead(limitId, byteArrayOf(60, 0, 0, 0))
        manager.onDisconnected("hub")
        assertNull(manager.pauseLimitSeconds.value)
        assertFalse(manager.pauseLimitPublished.value)
    }
}
