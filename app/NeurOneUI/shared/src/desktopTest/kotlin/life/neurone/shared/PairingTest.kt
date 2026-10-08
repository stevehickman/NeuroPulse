package life.neurone.shared

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.advanceTimeBy
import kotlinx.coroutines.test.runTest
import life.neurone.core.common.UUID
import life.neurone.shared.ble.AdapterState
import life.neurone.shared.ble.BleCentral
import life.neurone.shared.ble.BleCentralListener
import life.neurone.core.protocol.GattUuidStrings
import life.neurone.shared.ble.NeurOneGattManager
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** A refused `DEVICE_SERIAL` read on a connected link means the hub is not paired (`OI-UI-KMP-03`). */
@OptIn(ExperimentalCoroutinesApi::class)
class PairingTest {
    private class FakeCentral : BleCentral {
        override val adapterState = AdapterState.ON
        val reads = mutableListOf<UUID>()
        override fun setListener(listener: BleCentralListener) {}
        override fun startScan(serviceUuid: UUID) {}
        override fun stopScan() {}
        override fun connect(deviceId: String) {}
        override fun disconnect() {}
        override fun discoverCharacteristics(serviceUuid: UUID, uuids: List<UUID>) {}
        override fun enableNotifications(uuid: UUID) {}
        override fun read(uuid: UUID) { reads += uuid }
        override fun write(uuid: UUID, value: ByteArray) {}
    }

    private val serial = UUID.fromString(GattUuidStrings.DEVICE_SERIAL_ID)

    @Test fun a_refused_serial_read_raises_pairing_and_is_retried_until_it_succeeds() = runTest {
        val central = FakeCentral()
        val manager = NeurOneGattManager(central, TestScope(testScheduler))
        manager.onDeviceFound("hub")
        manager.onConnected("hub")

        manager.onCharacteristicReadFailed(serial)
        assertTrue(manager.pairingRequired.value)

        testScheduler.advanceTimeBy(5_001)
        assertEquals(listOf(serial), central.reads)

        manager.onCharacteristicRead(serial, ByteArray(32))
        assertFalse(manager.pairingRequired.value)
        testScheduler.advanceTimeBy(60_000)
        assertEquals(1, central.reads.size)
    }

    @Test fun another_refused_read_or_a_dropped_link_raises_nothing() = runTest {
        val manager = NeurOneGattManager(FakeCentral(), TestScope(testScheduler))
        manager.onCharacteristicReadFailed(serial)
        assertFalse(manager.pairingRequired.value) // not connected

        manager.onConnected("hub")
        manager.onCharacteristicReadFailed(UUID.fromString(GattUuidStrings.FIRMWARE_VERSION_ID))
        assertFalse(manager.pairingRequired.value)

        manager.onCharacteristicReadFailed(serial)
        manager.onDisconnected("hub")
        assertFalse(manager.pairingRequired.value)
    }
}
