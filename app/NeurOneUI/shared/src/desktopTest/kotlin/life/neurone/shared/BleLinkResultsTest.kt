package life.neurone.shared

import life.neurone.core.common.UUID
import life.neurone.shared.ble.AdapterState
import life.neurone.shared.ble.BleCentralListener
import life.neurone.shared.ble.onReadResult
import life.neurone.shared.ble.onWebBleEvent
import life.neurone.shared.ble.onWriteResult
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals

/**
 * What the Android and browser centrals do with a refused read or write, run against a recording listener. The platform
 * callbacks (a GATT status code, a rejected promise) are translated into these calls by `AndroidBleCentral` and the
 * `WebBleCentral` script; the rule they share is here (`OI-PRIV-17`). Neither central itself is run: no Android SDK, no browser.
 */
class BleLinkResultsTest {
    private class Recorder : BleCentralListener {
        val events = mutableListOf<String>()
        var lastValue: ByteArray? = null
        override fun onAdapterStateChanged(state: AdapterState) { events += "adapter" }
        override fun onDeviceFound(deviceId: String) { events += "found" }
        override fun onConnected(deviceId: String) { events += "connected:$deviceId" }
        override fun onDisconnected(deviceId: String) { events += "disconnected:$deviceId" }
        override fun onCharacteristicsDiscovered(characteristics: Set<UUID>) { events += "discovered:${characteristics.size}" }
        override fun onCharacteristicChanged(uuid: UUID, value: ByteArray) { events += "changed:$uuid"; lastValue = value }
        override fun onCharacteristicRead(uuid: UUID, value: ByteArray) { events += "read:$uuid"; lastValue = value }
        override fun onCharacteristicReadFailed(uuid: UUID) { events += "readFailed:$uuid" }
        override fun onCharacteristicWriteFailed(uuid: UUID) { events += "writeFailed:$uuid" }
    }

    private val serial = UUID.fromString("0000a017-0000-1000-8000-00805f9b34fb")

    // @verifies REQ-SW03-31#a
    @Test fun a_refused_read_is_a_failure_and_never_a_value() {
        val l = Recorder()
        l.onReadResult(serial, ByteArray(32) { 7 }, succeeded = false)
        assertEquals(listOf("readFailed:$serial"), l.events, "a stale value rides along with a failed status and must not pass for an answer")
        assertEquals(null, l.lastValue)
        l.onReadResult(serial, ByteArray(32) { 7 }, succeeded = true)
        assertEquals("read:$serial", l.events.last())
    }

    // @verifies REQ-SW03-31#a
    @Test fun a_refused_write_is_reported_and_a_good_one_is_silent() {
        val l = Recorder()
        l.onWriteResult(serial, succeeded = true)
        assertEquals(emptyList(), l.events)
        l.onWriteResult(serial, succeeded = false)
        assertEquals(listOf("writeFailed:$serial"), l.events)
    }

    // @verifies REQ-SW03-31#a
    @Test fun the_browsers_failure_kinds_reach_the_listener_with_the_characteristic_only() {
        val l = Recorder()
        l.onWebBleEvent("readFailed", serial.toString(), "")
        l.onWebBleEvent("writeFailed", serial.toString(), "")
        assertEquals(listOf("readFailed:$serial", "writeFailed:$serial"), l.events)
        // The kinds that are not an answer leave the manager alone, so the next click retries.
        l.onWebBleEvent("cancelled", "the user closed the chooser", "")
        l.onWebBleEvent("error", "NetworkError: GATT operation failed", "")
        assertEquals(2, l.events.size)
    }

    // @verifies REQ-SW03-31#a
    @Test fun the_browsers_good_answers_still_arrive_decoded() {
        val l = Recorder()
        l.onWebBleEvent("read", serial.toString(), "0aff10")
        assertEquals(listOf("read:$serial"), l.events)
        assertContentEquals(byteArrayOf(0x0a, 0xff.toByte(), 0x10), l.lastValue)
        l.onWebBleEvent("discovered", "", "")
        l.onWebBleEvent("connected", "hub", "")
        assertEquals(listOf("read:$serial", "discovered:0", "connected:hub"), l.events)
    }
}
