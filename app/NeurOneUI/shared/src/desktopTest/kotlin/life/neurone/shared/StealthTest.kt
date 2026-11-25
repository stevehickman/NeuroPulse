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

/** Stealth mode, as the app sends it to the helmet and reads it back (SWR-IND-01, SWR-IND-02; OI-PRIV-37). */
@OptIn(ExperimentalCoroutinesApi::class)
class StealthTest {
    private class RecordingCentral : BleCentral {
        override val adapterState = AdapterState.ON
        val reads = mutableListOf<UUID>()
        val writes = mutableListOf<Pair<UUID, List<Byte>>>()
        override fun setListener(listener: BleCentralListener) {}
        override fun startScan(serviceUuid: UUID) {}
        override fun stopScan() {}
        override fun connect(deviceId: String) {}
        override fun disconnect() {}
        override fun discoverCharacteristics(serviceUuid: UUID, uuids: List<UUID>) {}
        override fun enableNotifications(uuid: UUID) {}
        override fun read(uuid: UUID) { reads += uuid }
        override fun write(uuid: UUID, value: ByteArray) { writes += uuid to value.toList() }
    }

    private val stealthId = UUID.fromString(GattUuidStrings.STEALTH_ID)
    private val serialId = UUID.fromString(GattUuidStrings.DEVICE_SERIAL_ID)

    private fun connected(central: RecordingCentral, manager: NeurOneGattManager) {
        manager.onDeviceFound("hub")
        manager.onConnected("hub")
        manager.onCharacteristicsDiscovered(setOf(stealthId, serialId))
    }

    private fun stealthWrites(central: RecordingCentral) = central.writes.filter { it.first == stealthId }.map { it.second }

    // @verifies SWR-IND-01#a
    @Test fun the_choice_waits_for_an_encrypted_link_and_is_then_sent_and_read_back() = runTest {
        val central = RecordingCentral()
        val manager = NeurOneGattManager(central, TestScope(testScheduler))
        manager.desiredStealth = true
        connected(central, manager)
        assertTrue(manager.stealthPublished.value)
        assertTrue(stealthWrites(central).isEmpty(), "nothing is written before the encrypted-only read has succeeded (CLAUDE.md §23)")
        manager.onCharacteristicRead(serialId, ByteArray(32))
        assertEquals(listOf(listOf<Byte>(0x01)), stealthWrites(central))
        assertEquals(stealthId, central.reads.last(), "the write is followed by a read, so what is shown is what the helmet holds")
        manager.onCharacteristicRead(stealthId, byteArrayOf(0x01))
        assertEquals(true, manager.stealthOnHub.value)
    }

    // @verifies SWR-IND-01#a
    @Test fun the_choice_is_sent_again_at_every_link_because_the_helmet_boots_with_it_off() = runTest {
        val central = RecordingCentral()
        val manager = NeurOneGattManager(central, TestScope(testScheduler))
        manager.desiredStealth = true
        connected(central, manager)
        manager.onCharacteristicRead(serialId, ByteArray(32))
        manager.onDisconnected("hub")
        assertNull(manager.stealthOnHub.value, "what the last helmet held is not shown once the link is gone")
        assertFalse(manager.stealthPublished.value)
        connected(central, manager)
        manager.onCharacteristicRead(serialId, ByteArray(32))
        assertEquals(2, stealthWrites(central).size)
    }

    // @verifies SWR-IND-01#a
    @Test fun a_change_reaches_a_connected_helmet_at_once_and_no_choice_sends_nothing() = runTest {
        val central = RecordingCentral()
        val manager = NeurOneGattManager(central, TestScope(testScheduler))
        connected(central, manager)
        manager.onCharacteristicRead(serialId, ByteArray(32))
        assertTrue(stealthWrites(central).isEmpty(), "with no one named and no lock, the helmet keeps what it holds")
        manager.desiredStealth = true
        manager.desiredStealth = false
        assertEquals(listOf(listOf<Byte>(0x01), listOf<Byte>(0x00)), stealthWrites(central))
    }

    // @verifies SWR-IND-01#c
    @Test fun a_helmet_that_does_not_publish_it_takes_no_write_and_a_malformed_read_is_not_believed() = runTest {
        val central = RecordingCentral()
        val manager = NeurOneGattManager(central, TestScope(testScheduler))
        manager.onDeviceFound("hub")
        manager.onConnected("hub")
        manager.onCharacteristicsDiscovered(setOf(serialId))
        manager.onCharacteristicRead(serialId, ByteArray(32))
        manager.desiredStealth = true
        assertTrue(stealthWrites(central).isEmpty())
        assertFalse(manager.stealthPublished.value)
        manager.onCharacteristicRead(stealthId, byteArrayOf(0x03))
        assertNull(manager.stealthOnHub.value)
    }
}
