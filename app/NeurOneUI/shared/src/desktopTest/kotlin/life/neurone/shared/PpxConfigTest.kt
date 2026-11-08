package life.neurone.shared

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.runTest
import life.neurone.core.common.UUID
import life.neurone.core.models.PpxConfig
import life.neurone.core.models.PpxEvidence
import life.neurone.core.protocol.GattUuidStrings
import life.neurone.shared.ble.AdapterState
import life.neurone.shared.ble.BleCentral
import life.neurone.shared.ble.BleCentralListener
import life.neurone.shared.ble.NeurOneGattManager
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The seizure-response detector's two clinician switches, read from and written to the helmet (REQ-FWHUB-71). */
@OptIn(ExperimentalCoroutinesApi::class)
class PpxConfigTest {
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

    private val configId = UUID.fromString(GattUuidStrings.PPX_CONFIG_ID)

    // @verifies REQ-FWHUB-71
    @Test fun the_wire_frame_is_two_bytes_and_anything_else_is_not_believed() {
        assertEquals(PpxConfig(true, false, PpxEvidence.NONE), PpxConfig.fromWire(byteArrayOf(0x01, 0x00)))
        assertEquals(PpxConfig(false, true, PpxEvidence.BENCH), PpxConfig.fromWire(byteArrayOf(0x02, 0x01)))
        assertEquals(PpxConfig(true, true, PpxEvidence.CLINICAL), PpxConfig.fromWire(byteArrayOf(0x03, 0x02)))
        assertNull(PpxConfig.fromWire(byteArrayOf(0x01)), "too short")
        assertNull(PpxConfig.fromWire(byteArrayOf(0x01, 0x00, 0x00)), "too long")
        assertNull(PpxConfig.fromWire(byteArrayOf(0x04, 0x00)), "a flag bit the app does not know")
        assertNull(PpxConfig.fromWire(byteArrayOf(0x01, 0x03)), "an evidence level the app does not know")
    }

    // @verifies REQ-FWHUB-71
    @Test fun the_write_is_one_byte_of_flags() {
        assertContentEquals(byteArrayOf(0x00), PpxConfig.toWire(false, false))
        assertContentEquals(byteArrayOf(0x01), PpxConfig.toWire(true, false))
        assertContentEquals(byteArrayOf(0x02), PpxConfig.toWire(false, true))
        assertContentEquals(byteArrayOf(0x03), PpxConfig.toWire(true, true))
    }

    // @verifies REQ-FWHUB-71
    @Test fun a_helmet_that_does_not_publish_it_offers_nothing_and_takes_no_write() = runTest {
        val central = RecordingCentral()
        val manager = NeurOneGattManager(central, TestScope(testScheduler))
        assertNull(manager.ppxConfig.value)
        assertFalse(manager.ppxConfigPublished.value)
        assertFalse(manager.writePpxConfig(true, false))
        assertTrue(central.writes.isEmpty())
    }

    // @verifies REQ-FWHUB-71
    @Test fun a_helmet_that_publishes_it_is_read_at_link_and_the_value_is_the_helmets_not_the_apps() = runTest {
        val central = RecordingCentral()
        val manager = NeurOneGattManager(central, TestScope(testScheduler))
        manager.onCharacteristicsDiscovered(setOf(configId))
        assertTrue(manager.ppxConfigPublished.value)
        assertTrue("read" in central.log)
        manager.onCharacteristicRead(configId, byteArrayOf(0x01, 0x00))
        assertEquals(PpxConfig(true, false, PpxEvidence.NONE), assertNotNull(manager.ppxConfig.value))
        manager.onCharacteristicRead(configId, byteArrayOf(1, 2, 3))
        assertNull(manager.ppxConfig.value, "a malformed frame is not believed")
    }

    // @verifies REQ-FWHUB-71
    @Test fun a_write_sends_the_flags_byte_and_is_followed_by_a_read() = runTest {
        val central = RecordingCentral()
        val manager = NeurOneGattManager(central, TestScope(testScheduler))
        manager.onCharacteristicsDiscovered(setOf(configId))
        central.log.clear()
        assertTrue(manager.writePpxConfig(stopOnOccipitalOnly = false, manualOverride = true))
        assertEquals(listOf(configId to listOf<Byte>(0x02)), central.writes)
        assertEquals(listOf("write", "read"), central.log, "the screen then shows what the helmet holds")
    }

    // @verifies REQ-FWHUB-71
    @Test fun a_dropped_link_forgets_the_settings_since_the_next_helmet_may_be_another() = runTest {
        val central = RecordingCentral()
        val manager = NeurOneGattManager(central, TestScope(testScheduler))
        manager.onCharacteristicsDiscovered(setOf(configId))
        manager.onCharacteristicRead(configId, byteArrayOf(0x03, 0x01))
        manager.onDisconnected("hub")
        assertNull(manager.ppxConfig.value)
        assertFalse(manager.ppxConfigPublished.value)
    }
}
