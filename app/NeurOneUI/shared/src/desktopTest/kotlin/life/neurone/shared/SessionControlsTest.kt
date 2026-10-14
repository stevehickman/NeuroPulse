package life.neurone.shared

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.runTest
import life.neurone.core.common.UUID
import life.neurone.core.models.HubSessionState
import life.neurone.core.protocol.GattUuidStrings
import life.neurone.shared.ble.AdapterState
import life.neurone.shared.ble.BleCentral
import life.neurone.shared.ble.BleCentralListener
import life.neurone.shared.ble.NeurOneGattManager
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull

/** The app's pause, resume and stop requests, and the helmet's progress figures it shows (SWR-SS-05, -06, -07). */
@OptIn(ExperimentalCoroutinesApi::class)
class SessionControlsTest {
    private class RecordingCentral : BleCentral {
        override val adapterState = AdapterState.ON
        val writes = mutableListOf<Pair<UUID, List<Byte>>>()
        override fun setListener(listener: BleCentralListener) {}
        override fun startScan(serviceUuid: UUID) {}
        override fun stopScan() {}
        override fun connect(deviceId: String) {}
        override fun disconnect() {}
        override fun discoverCharacteristics(serviceUuid: UUID, uuids: List<UUID>) {}
        override fun enableNotifications(uuid: UUID) {}
        override fun read(uuid: UUID) {}
        override fun write(uuid: UUID, value: ByteArray) { writes += uuid to value.toList() }
    }

    private val progressId = UUID.fromString(GattUuidStrings.SESSION_PROGRESS_ID)
    private val pauseId = UUID.fromString(GattUuidStrings.SESSION_PAUSE_ID)
    private val stopId = UUID.fromString(GattUuidStrings.SESSION_STOP_ID)

    // @verifies SWR-SS-05
    @Test fun pause_and_resume_write_the_two_codes_of_the_pause_characteristic() = runTest {
        val central = RecordingCentral()
        val manager = NeurOneGattManager(central, TestScope(testScheduler))
        manager.requestSessionPause()
        manager.requestSessionResume()
        assertEquals(listOf(pauseId to listOf<Byte>(0x01), pauseId to listOf<Byte>(0x02)), central.writes)
    }

    // @verifies SWR-SS-06
    @Test fun terminate_writes_the_stop_characteristic() = runTest {
        val central = RecordingCentral()
        NeurOneGattManager(central, TestScope(testScheduler)).requestSessionStop()
        assertEquals(listOf(stopId to listOf<Byte>(0x01)), central.writes)
    }

    // @verifies SWR-SS-07
    @Test fun a_progress_frame_shows_the_helmets_figures_and_a_dropped_link_clears_them() = runTest {
        val manager = NeurOneGattManager(RecordingCentral(), TestScope(testScheduler))
        val frame = ByteArray(22)
        frame[0] = 4                      // paused
        frame[4] = 0x10; frame[5] = 0x27  // elapsed 10 000 ms
        frame[8] = 0x20; frame[9] = 0x4E  // remaining 20 000 ms
        frame[12] = 0x30; frame[13] = 0x75 // total 30 000 ms
        frame[16] = 2                      // held
        manager.onCharacteristicChanged(progressId, frame)
        val p = assertNotNull(manager.progress.value)
        assertEquals(HubSessionState.PAUSED, p.state)
        assertEquals(10_000L, p.elapsedMs)
        assertEquals(20_000L, p.remainingMs)

        manager.onCharacteristicChanged(progressId, ByteArray(5)) // malformed: nothing is partly believed
        assertNull(manager.progress.value)

        manager.onCharacteristicChanged(progressId, frame)
        manager.onDisconnected("hub")
        assertNull(manager.progress.value)
    }
}

/** The helmet's hardware version is read from the helmet and not assumed (SWR-HV-06, SWR-HV-07). */
@OptIn(ExperimentalCoroutinesApi::class)
class HardwareVersionTest {
    private class InertCentral : BleCentral {
        override val adapterState = AdapterState.ON
        override fun setListener(listener: BleCentralListener) {}
        override fun startScan(serviceUuid: UUID) {}
        override fun stopScan() {}
        override fun connect(deviceId: String) {}
        override fun disconnect() {}
        override fun discoverCharacteristics(serviceUuid: UUID, uuids: List<UUID>) {}
        override fun enableNotifications(uuid: UUID) {}
        override fun read(uuid: UUID) {}
        override fun write(uuid: UUID, value: ByteArray) {}
    }

    private val versionId = UUID.fromString(GattUuidStrings.HARDWARE_VERSION_ID)

    // @verifies SWR-HV-07
    @Test fun a_helmet_that_states_none_is_version_one() = runTest {
        val manager = NeurOneGattManager(InertCentral(), TestScope(testScheduler))
        assertEquals(1, manager.hardwareVersion.value)
    }

    // @verifies SWR-HV-06
    @Test fun the_version_the_helmet_reports_is_the_one_the_gate_uses_and_a_bad_byte_is_not_believed() = runTest {
        val manager = NeurOneGattManager(InertCentral(), TestScope(testScheduler))
        manager.onCharacteristicRead(versionId, byteArrayOf(2))
        assertEquals(2, manager.hardwareVersion.value)
        manager.onCharacteristicRead(versionId, byteArrayOf(0))
        assertEquals(2, manager.hardwareVersion.value, "an out-of-range byte changes nothing")
        manager.onCharacteristicRead(versionId, byteArrayOf(3, 3))
        assertEquals(2, manager.hardwareVersion.value, "a frame of the wrong length changes nothing")
    }

    // @verifies SWR-HV-06
    @Test fun the_next_link_may_be_another_helmet_so_a_dropped_link_forgets_the_version() = runTest {
        val manager = NeurOneGattManager(InertCentral(), TestScope(testScheduler))
        manager.onCharacteristicRead(versionId, byteArrayOf(5))
        manager.onDisconnected("hub")
        assertEquals(1, manager.hardwareVersion.value)
    }
}
