package life.neurone.desktop

import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.common.UUID
import life.neurone.shared.ble.AdapterState
import life.neurone.shared.ble.BleCentralListener
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.TimeUnit
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals

class DesktopBleCentralTest {

    /** A radio that records commands and plays back the events a test queues. */
    private class FakeLink : BtleLink {
        val events = LinkedBlockingQueue<ByteArray>()
        val commands = mutableListOf<String>()
        @Volatile var closed = false
        override fun poll(timeoutMs: Int): ByteArray? = events.poll(timeoutMs.toLong(), TimeUnit.MILLISECONDS)
        override fun refresh() { commands += "refresh" }
        override fun startScan(service: String) { commands += "scan $service" }
        override fun stopScan() { commands += "stopScan" }
        override fun connect(device: String) { commands += "connect $device" }
        override fun disconnect() { commands += "disconnect" }
        override fun discover(service: String, wanted: List<String>) { commands += "discover $service ${wanted.joinToString(",")}" }
        override fun enableNotifications(characteristic: String) { commands += "notify $characteristic" }
        override fun read(characteristic: String) { commands += "read $characteristic" }
        override fun write(characteristic: String, value: ByteArray) { commands += "write $characteristic ${value.toList()}" }
        override fun close() { closed = true }
    }

    private class Recorder : BleCentralListener {
        val log = LinkedBlockingQueue<String>()
        var lastChanged: ByteArray? = null
        override fun onAdapterStateChanged(state: AdapterState) { log += "adapter $state" }
        override fun onDeviceFound(deviceId: String) { log += "found $deviceId" }
        override fun onConnected(deviceId: String) { log += "connected $deviceId" }
        override fun onDisconnected(deviceId: String) { log += "disconnected $deviceId" }
        override fun onCharacteristicsDiscovered(characteristics: Set<UUID>) { log += "discovered ${characteristics.map(UUID::toString).sorted()}" }
        override fun onCharacteristicChanged(uuid: UUID, value: ByteArray) { lastChanged = value; log += "changed $uuid" }
        override fun onCharacteristicRead(uuid: UUID, value: ByteArray) { log += "read $uuid ${value.toList()}" }
        override fun onCharacteristicReadFailed(uuid: UUID) { log += "readFailed $uuid" }
    }

    private val a = UUID.fromString("00000001-0000-1000-8000-00805f9b34fb")
    private val b = UUID.fromString("0000abcd-1234-5678-9abc-def012345678")

    private fun bytes(uuid: UUID) = uuid.toBytes()

    private fun Recorder.next(): String = log.poll(2, TimeUnit.SECONDS) ?: error("no event within 2 s")

    @Test fun commands_reach_the_radio_in_the_wire_form_the_library_parses() {
        val link = FakeLink()
        val central = DesktopBleCentral(link)
        central.startScan(a)
        central.connect("dev-1")
        central.discoverCharacteristics(a, listOf(a, b))
        central.enableNotifications(b)
        central.read(a)
        central.write(b, byteArrayOf(1, 2, 3))
        central.stopScan()
        central.disconnect()
        assertEquals(
            listOf(
                "refresh", "scan $a", "connect dev-1", "discover $a $a,$b", "notify $b", "read $a", "write $b [1, 2, 3]",
                "stopScan", "disconnect",
            ),
            link.commands,
        )
        central.close()
        assertEquals(true, link.closed)
    }

    @Test fun events_from_the_library_become_listener_calls() {
        val link = FakeLink()
        val central = DesktopBleCentral(link)
        val rec = Recorder()
        central.setListener(rec)
        assertEquals("adapter UNKNOWN", rec.next())

        link.events += byteArrayOf(1, 3)
        assertEquals("adapter ON", rec.next())
        assertEquals(AdapterState.ON, central.adapterState)

        link.events += byteArrayOf(2) + "dev-1".encodeToByteArray()
        assertEquals("found dev-1", rec.next())
        link.events += byteArrayOf(3) + "dev-1".encodeToByteArray()
        assertEquals("connected dev-1", rec.next())
        link.events += byteArrayOf(5) + bytes(a) + bytes(b)
        assertEquals("discovered ${listOf(a, b).map(UUID::toString).sorted()}", rec.next())
        link.events += byteArrayOf(6) + bytes(b) + byteArrayOf(9, 8)
        assertEquals("changed $b", rec.next())
        assertContentEquals(byteArrayOf(9, 8), rec.lastChanged)
        link.events += byteArrayOf(7) + bytes(a) + byteArrayOf(5)
        assertEquals("read $a [5]", rec.next())
        link.events += byteArrayOf(4) + "dev-1".encodeToByteArray()
        assertEquals("disconnected dev-1", rec.next())
        central.close()
    }

    @Test fun a_malformed_event_does_not_end_the_event_thread() {
        val link = FakeLink()
        val central = DesktopBleCentral(link)
        val rec = Recorder()
        central.setListener(rec)
        rec.next()
        link.events += byteArrayOf(6, 1, 2) // a characteristic event too short to hold a uuid
        link.events += byteArrayOf(2) + "after".encodeToByteArray()
        assertEquals("found after", rec.next())
        central.close()
    }

    @Test fun an_adapter_code_outside_the_enum_reads_as_unknown() {
        val link = FakeLink()
        val central = DesktopBleCentral(link)
        val rec = Recorder()
        central.setListener(rec)
        rec.next()
        link.events += byteArrayOf(1, 9)
        assertEquals("adapter UNKNOWN", rec.next())
        central.close()
    }

    @Test fun a_refused_read_reaches_the_listener() {
        val link = FakeLink()
        val central = DesktopBleCentral(link)
        val rec = Recorder()
        central.setListener(rec)
        rec.next()
        link.events += byteArrayOf(8) + bytes(a)
        assertEquals("readFailed $a", rec.next())
        central.close()
    }

    @Test fun the_last_hub_is_remembered_and_preferred() {
        val store = InMemoryKeyValueStore()
        var clock = 0L
        val link = FakeLink()
        val central = DesktopBleCentral(link, store, preferenceWindowMs = 1_000, now = { clock })
        val rec = Recorder()
        central.setListener(rec)
        rec.next()

        // Nothing remembered: the first device goes straight through, and connecting remembers it.
        link.events += byteArrayOf(2) + "hub-1".encodeToByteArray()
        assertEquals("found hub-1", rec.next())
        link.events += byteArrayOf(3) + "hub-1".encodeToByteArray()
        assertEquals("connected hub-1", rec.next())
        assertEquals("hub-1", store.getString("ble.lastHubId"))

        // Another device is held back, and the remembered one overrides it when it shows up.
        link.events += byteArrayOf(2) + "hub-2".encodeToByteArray()
        link.events += byteArrayOf(2) + "hub-1".encodeToByteArray()
        assertEquals("found hub-1", rec.next())

        // With the remembered hub absent, the other is released after the window, so a new hub is never refused.
        central.startScan(a)
        link.events += byteArrayOf(2) + "hub-2".encodeToByteArray()
        assertEquals(null, rec.log.poll(600, TimeUnit.MILLISECONDS))
        clock = 1_001
        assertEquals("found hub-2", rec.next())

        central.forgetHub()
        assertEquals(null, store.getString("ble.lastHubId"))
        central.close()
    }
}
