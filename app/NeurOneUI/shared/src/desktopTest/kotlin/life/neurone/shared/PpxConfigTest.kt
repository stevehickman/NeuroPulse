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

/** Why the helmet holds a session paused, and the Stop / Resume question for a halt it made on its own (SWR-SS-09). */
@OptIn(ExperimentalCoroutinesApi::class)
class HoldCauseTest {
    private class Central : BleCentral {
        override val adapterState = AdapterState.ON
        val log = mutableListOf<String>()
        override fun setListener(listener: BleCentralListener) {}
        override fun startScan(serviceUuid: UUID) {}
        override fun stopScan() {}
        override fun connect(deviceId: String) {}
        override fun disconnect() {}
        override fun discoverCharacteristics(serviceUuid: UUID, uuids: List<UUID>) {}
        override fun enableNotifications(uuid: UUID) { log += "notify:$uuid" }
        override fun read(uuid: UUID) { log += "read:$uuid" }
        override fun write(uuid: UUID, value: ByteArray) {}
    }

    private val holdId = UUID.fromString(GattUuidStrings.SESSION_HOLD_ID)

    // @verifies SWR-SS-09
    @Test fun the_wire_frame_is_one_byte_and_a_byte_it_does_not_know_is_not_believed() {
        assertEquals(life.neurone.core.models.HoldCause.NONE, life.neurone.core.models.HoldCause.fromWire(byteArrayOf(0)))
        assertEquals(life.neurone.core.models.HoldCause.USER_PAUSE, life.neurone.core.models.HoldCause.fromWire(byteArrayOf(1)))
        assertEquals(life.neurone.core.models.HoldCause.USER_PANIC, life.neurone.core.models.HoldCause.fromWire(byteArrayOf(2)))
        assertEquals(life.neurone.core.models.HoldCause.DETECTOR_DISCHARGE, life.neurone.core.models.HoldCause.fromWire(byteArrayOf(3)))
        assertEquals(life.neurone.core.models.HoldCause.DETECTOR_BLIND, life.neurone.core.models.HoldCause.fromWire(byteArrayOf(4)))
        assertNull(life.neurone.core.models.HoldCause.fromWire(byteArrayOf(5)), "a cause it does not know")
        assertNull(life.neurone.core.models.HoldCause.fromWire(byteArrayOf()), "empty")
        assertNull(life.neurone.core.models.HoldCause.fromWire(byteArrayOf(1, 0)), "too long")
    }

    // @verifies SWR-SS-09
    @Test fun only_the_helmets_own_halts_are_automated() {
        val c = life.neurone.core.models.HoldCause.entries
        assertEquals(setOf(life.neurone.core.models.HoldCause.DETECTOR_DISCHARGE, life.neurone.core.models.HoldCause.DETECTOR_BLIND), c.filter { it.isAutomated }.toSet())
    }

    // @verifies SWR-SS-09
    @Test fun the_cause_is_read_and_subscribed_at_link_followed_on_notification_and_forgotten_with_the_link() = runTest {
        val central = Central()
        val manager = NeurOneGattManager(central, TestScope(testScheduler))
        assertNull(manager.holdCause.value)
        manager.onCharacteristicsDiscovered(setOf(holdId))
        assertTrue("notify:$holdId" in central.log && "read:$holdId" in central.log)
        manager.onCharacteristicChanged(holdId, byteArrayOf(3))
        assertEquals(life.neurone.core.models.HoldCause.DETECTOR_DISCHARGE, manager.holdCause.value)
        manager.onCharacteristicChanged(holdId, byteArrayOf(9))
        assertNull(manager.holdCause.value, "a byte it does not know is not believed")
        manager.onCharacteristicChanged(holdId, byteArrayOf(4))
        manager.onDisconnected("hub")
        assertNull(manager.holdCause.value)
    }

    // @verifies SWR-SS-09
    @Test fun the_dialog_is_put_for_the_helmets_own_halt_once_and_never_for_a_pause_the_person_made() {
        val c = life.neurone.core.models.HoldCause
        assertFalse(life.neurone.shared.ui.holdDialogShown(false, c.USER_PAUSE, true, false), "a pause the person made")
        assertFalse(life.neurone.shared.ui.holdDialogShown(false, c.USER_PANIC, true, false), "a panic they pressed: the dialog is the press's")
        assertTrue(life.neurone.shared.ui.holdDialogShown(true, c.USER_PANIC, true, false), "the press asks")
        assertTrue(life.neurone.shared.ui.holdDialogShown(false, c.DETECTOR_DISCHARGE, true, false), "the helmet's discharge halt is put")
        assertTrue(life.neurone.shared.ui.holdDialogShown(false, c.DETECTOR_BLIND, true, false), "and its blindness halt")
        assertFalse(life.neurone.shared.ui.holdDialogShown(false, c.DETECTOR_DISCHARGE, true, true), "not again once answered")
        assertFalse(life.neurone.shared.ui.holdDialogShown(false, c.DETECTOR_DISCHARGE, false, false), "not until the helmet says the session is held")
        assertFalse(life.neurone.shared.ui.holdDialogShown(false, null, true, false), "a cause not known is not a reason")
    }
}
