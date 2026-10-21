package life.neurone.shared

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.launch
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.runCurrent
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
import kotlin.test.assertTrue

/** What starts a fleet upload attempt: the hub's own notification of 0x01, every time it sends one (REQ-SHDR-01 (a)). */
@OptIn(ExperimentalCoroutinesApi::class)
class ShdrUploadSignalTest {
    private class Central : BleCentral {
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

    private val status = UUID.fromString(GattUuidStrings.SHDR_UPLOAD_STATUS_ID)

    // @verifies REQ-SHDR-01#a
    @Test fun every_pending_notification_is_an_attempt_and_a_repeat_is_a_retry() = runTest {
        val manager = NeurOneGattManager(Central(), TestScope(testScheduler))
        var attempts = 0
        val job = backgroundScope.launch { manager.shdrUploadSignal.collect { attempts++ } }
        runCurrent()
        manager.onCharacteristicChanged(status, byteArrayOf(0x01))
        runCurrent()
        manager.onCharacteristicChanged(status, byteArrayOf(0x01)) // the hub asks again: the first attempt did not take the record
        runCurrent()
        assertEquals(2, attempts)
        assertTrue(manager.shdrUploadPending.value)
        job.cancel()
    }

    // @verifies REQ-SHDR-01#a
    @Test fun a_false_or_empty_value_starts_nothing() = runTest {
        val manager = NeurOneGattManager(Central(), TestScope(testScheduler))
        var attempts = 0
        val job = backgroundScope.launch { manager.shdrUploadSignal.collect { attempts++ } }
        runCurrent()
        manager.onCharacteristicChanged(status, byteArrayOf(0x00))
        manager.onCharacteristicChanged(status, ByteArray(0))
        manager.onCharacteristicChanged(status, byteArrayOf(0x02))
        runCurrent()
        assertEquals(0, attempts)
        assertFalse(manager.shdrUploadPending.value)
        job.cancel()
    }
}
