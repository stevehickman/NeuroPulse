package life.neurone.shared

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.runTest
import life.neurone.core.common.UUID
import life.neurone.core.protocol.GattUuidStrings
import life.neurone.shared.ble.AdapterState
import life.neurone.shared.ble.BleCentralListener
import life.neurone.shared.ble.ConnectionState
import life.neurone.shared.ble.HubBridge
import life.neurone.shared.ble.HubBridgeCommand
import life.neurone.shared.ble.HubBridgeEvent
import life.neurone.shared.ble.NeurOneGattManager
import life.neurone.shared.ble.SimulatedBleCentral
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/**
 * The simulated hub's BLE central (NP-SIM-FW-001 §27, `REQ-SIMFW-19` (c)): everything it tells the app comes from what the twin reported
 * over the bridge, and nothing is made up. The bridge here is a fake that records what the central sent and plays back what a twin would
 * report; the twin itself is the node test of `fw-bridge.test.js`.
 */
@OptIn(ExperimentalCoroutinesApi::class)
class SimulatedBleCentralTest {

    private class FakeBridge : HubBridge {
        var onEvent: (HubBridgeEvent) -> Unit = {}
        var isOpen = true
        val sent = mutableListOf<HubBridgeCommand>()
        override fun setReceiver(receiver: (HubBridgeEvent) -> Unit) { onEvent = receiver }
        override fun send(command: HubBridgeCommand): Boolean { if (!isOpen) return false; sent += command; return true }
        fun report(vararg events: HubBridgeEvent) = events.forEach(onEvent)
    }

    private class Recorder : BleCentralListener {
        val found = mutableListOf<String>()
        val connected = mutableListOf<String>()
        val disconnected = mutableListOf<String>()
        var discovered: Set<UUID>? = null
        val reads = mutableListOf<Pair<UUID, List<Byte>>>()
        val changes = mutableListOf<Pair<UUID, List<Byte>>>()
        override fun onAdapterStateChanged(state: AdapterState) {}
        override fun onDeviceFound(deviceId: String) { found += deviceId }
        override fun onConnected(deviceId: String) { connected += deviceId }
        override fun onDisconnected(deviceId: String) { disconnected += deviceId }
        override fun onCharacteristicsDiscovered(characteristics: Set<UUID>) { discovered = characteristics }
        override fun onCharacteristicChanged(uuid: UUID, value: ByteArray) { changes += uuid to value.toList() }
        override fun onCharacteristicRead(uuid: UUID, value: ByteArray) { reads += uuid to value.toList() }
    }

    private val service = UUID.fromString(GattUuidStrings.SERVICE_ID)
    private val uploadCheck = 0x18
    private val warrantyToken = 0x10
    private val firmwareVersion = 0x11
    private val protocolUpload = 0x08

    /** The hub's published table as the twin reports it: the upload check and the warranty token, and neither the firmware version nor the upload. */
    private fun table(vararg ids: Int): List<HubBridgeEvent.Characteristic> =
        ids.mapIndexed { i, id -> HubBridgeEvent.Characteristic(index = i, count = ids.size, id = id, props = 1) }

    private fun connected(bridge: FakeBridge, central: SimulatedBleCentral, rec: Recorder, vararg ids: Int) {
        central.setListener(rec)
        bridge.report(HubBridgeEvent.Opened)
        central.startScan(service)
        central.connect(SimulatedBleCentral.SIMULATED_HUB_ID)
        table(*ids).forEach(bridge::report)
    }

    // @verifies REQ-SIMFW-19#c
    @Test fun a_characteristic_id_is_the_four_digits_of_its_uuid_and_back() {
        assertEquals(UUID.fromString(GattUuidStrings.UPLOAD_CHECK_ID), SimulatedBleCentral.uuidOf(uploadCheck))
        assertEquals(UUID.fromString(GattUuidStrings.PROTOCOL_UPLOAD_ID), SimulatedBleCentral.uuidOf(protocolUpload))
        assertEquals(uploadCheck, SimulatedBleCentral.idOf(UUID.fromString(GattUuidStrings.UPLOAD_CHECK_ID)))
        assertEquals(0x0001, SimulatedBleCentral.idOf(service))
    }

    // @verifies REQ-SIMFW-19#c
    @Test fun it_is_found_once_the_link_to_the_simulator_is_open_and_connects_when_the_hubs_table_has_arrived() {
        val bridge = FakeBridge()
        val central = SimulatedBleCentral(bridge)
        val rec = Recorder()
        central.setListener(rec)
        central.startScan(service)
        assertTrue(rec.found.isEmpty(), "nothing is found before the link to the simulator is open")
        bridge.report(HubBridgeEvent.Opened)
        assertEquals(listOf(SimulatedBleCentral.SIMULATED_HUB_ID), rec.found)
        central.connect(SimulatedBleCentral.SIMULATED_HUB_ID)
        assertEquals(listOf(HubBridgeCommand.Init, HubBridgeCommand.Table, HubBridgeCommand.Link(true)), bridge.sent, "the twin is brought up, its table asked for, and the bonded link reported encrypted")
        assertTrue(rec.connected.isEmpty(), "not connected until the whole table has arrived")
        bridge.report(*table(uploadCheck, warrantyToken).toTypedArray())
        assertEquals(listOf(SimulatedBleCentral.SIMULATED_HUB_ID), rec.connected)
    }

    // @verifies REQ-SIMFW-19#c
    @Test fun discovery_returns_only_what_the_hub_publishes_and_the_app_asked_for() {
        val bridge = FakeBridge()
        val central = SimulatedBleCentral(bridge)
        val rec = Recorder()
        connected(bridge, central, rec, uploadCheck, warrantyToken)
        val asked = listOf(uploadCheck, warrantyToken, firmwareVersion, protocolUpload).map(SimulatedBleCentral::uuidOf)
        central.discoverCharacteristics(service, asked)
        assertEquals(setOf(uploadCheck, warrantyToken).map(SimulatedBleCentral::uuidOf).toSet(), rec.discovered,
            "the firmware version and the protocol upload are not published by the hub, so they are not discovered")
    }

    // @verifies REQ-SIMFW-19#c
    @Test fun a_read_gives_the_hubs_bytes_and_a_refusal_is_the_hubs_status_with_no_value() {
        val bridge = FakeBridge()
        val central = SimulatedBleCentral(bridge)
        val rec = Recorder()
        val refusals = mutableListOf<Triple<UUID, Int, Boolean>>()
        central.onAttRefusal = { u, att, wasRead -> refusals += Triple(u, att, wasRead) }
        connected(bridge, central, rec, uploadCheck)
        central.read(SimulatedBleCentral.uuidOf(uploadCheck))
        assertEquals(HubBridgeCommand.Read(uploadCheck, 0), bridge.sent.last())
        bridge.report(HubBridgeEvent.ReadResult(uploadCheck, att = 0, hex = "0001ab"))
        assertEquals(listOf(SimulatedBleCentral.uuidOf(uploadCheck) to listOf<Byte>(0x00, 0x01, 0xab.toByte())), rec.reads)
        bridge.report(HubBridgeEvent.ReadResult(firmwareVersion, att = 0x0a, hex = ""))
        assertEquals(1, rec.reads.size, "a refused read gives the app no value")
        assertEquals(listOf(Triple(SimulatedBleCentral.uuidOf(firmwareVersion), 0x0a, true)), refusals)
    }

    // @verifies REQ-SIMFW-19#c
    @Test fun a_write_goes_as_hex_and_the_hubs_refusal_is_reported() {
        val bridge = FakeBridge()
        val central = SimulatedBleCentral(bridge)
        val rec = Recorder()
        val refusals = mutableListOf<Triple<UUID, Int, Boolean>>()
        central.onAttRefusal = { u, att, wasRead -> refusals += Triple(u, att, wasRead) }
        connected(bridge, central, rec, uploadCheck)
        central.write(SimulatedBleCentral.uuidOf(protocolUpload), byteArrayOf(0x04, 0x0f, 0xa0.toByte()))
        assertEquals(HubBridgeCommand.Write(protocolUpload, "040fa0"), bridge.sent.last())
        bridge.report(HubBridgeEvent.WriteResult(protocolUpload, att = 0x80))   // NP_ATT_APP_REFUSED: the hub's transport refused the bytes
        assertEquals(listOf(Triple(SimulatedBleCentral.uuidOf(protocolUpload), 0x80, false)), refusals)
        bridge.report(HubBridgeEvent.WriteResult(protocolUpload, att = 0))
        assertEquals(1, refusals.size, "a write the hub took is not a refusal")
    }

    // @verifies REQ-SIMFW-19#c
    @Test fun a_notification_reaches_only_a_characteristic_the_app_subscribed_to() {
        val bridge = FakeBridge()
        val central = SimulatedBleCentral(bridge)
        val rec = Recorder()
        connected(bridge, central, rec, uploadCheck, warrantyToken)
        central.enableNotifications(SimulatedBleCentral.uuidOf(uploadCheck))
        central.enableNotifications(SimulatedBleCentral.uuidOf(firmwareVersion))   // not published: nothing to subscribe to
        bridge.report(HubBridgeEvent.Notification(uploadCheck, "0102"), HubBridgeEvent.Notification(warrantyToken, "ff"),
            HubBridgeEvent.Notification(firmwareVersion, "01"))
        assertEquals(listOf(SimulatedBleCentral.uuidOf(uploadCheck) to listOf<Byte>(1, 2)), rec.changes)
    }

    // @verifies REQ-FWHUB-80#d
    @Test fun the_central_reports_the_write_a_link_carries_which_is_the_mtu_less_three_and_at_most_512() = runTest {
        val bridge = FakeBridge()
        assertEquals(244, SimulatedBleCentral(bridge).writeLength, "the default MTU of 247")
        assertEquals(20, SimulatedBleCentral(bridge, 23).writeLength, "the default ATT_MTU of 23")
        assertEquals(512, SimulatedBleCentral(bridge, 517).writeLength, "the largest MTU, cut at the hub's ceiling")
        assertEquals(1, SimulatedBleCentral(bridge, 4).writeLength)
        val central = SimulatedBleCentral(bridge)
        val manager = NeurOneGattManager(central, TestScope(testScheduler))
        central.attMtu = 188
        assertEquals(185, manager.writeLength, "the manager reports what its central does")
        // A central that cannot tell answers the default ATT_MTU's 20.
        val inert = object : life.neurone.shared.ble.BleCentral {
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
        assertEquals(20, inert.writeLength)
        assertEquals(20, NeurOneGattManager(inert, TestScope(testScheduler)).writeLength)
    }

    // @verifies REQ-SIMFW-19#c
    @Test fun a_disconnect_of_a_link_that_was_up_tells_the_hub_and_one_that_never_connected_tells_it_nothing() {
        val bridge = FakeBridge()
        val central = SimulatedBleCentral(bridge)
        central.disconnect()
        assertTrue(bridge.sent.isEmpty(), "nothing was connected, so nothing is said")
        val rec = Recorder()
        connected(bridge, central, rec, protocolUpload)
        central.disconnect()
        assertEquals(HubBridgeCommand.Disconnect, bridge.sent.last(), "the hub is told the link is down (np_gatt_on_disconnect)")
        assertEquals(listOf(SimulatedBleCentral.SIMULATED_HUB_ID), rec.disconnected)
    }

    // @verifies REQ-SIMFW-19#c
    @Test fun losing_the_link_to_the_simulator_is_a_disconnect_and_a_send_with_no_link_is_not_made() {
        val bridge = FakeBridge()
        val central = SimulatedBleCentral(bridge)
        val rec = Recorder()
        connected(bridge, central, rec, uploadCheck)
        bridge.report(HubBridgeEvent.Closed)
        assertEquals(listOf(SimulatedBleCentral.SIMULATED_HUB_ID), rec.disconnected)
        bridge.isOpen = false
        val before = bridge.sent.size
        central.read(SimulatedBleCentral.uuidOf(uploadCheck))
        assertEquals(before, bridge.sent.size, "a command to a closed link is not made")
        assertContentEquals(listOf(HubBridgeCommand.Init, HubBridgeCommand.Table), bridge.sent.take(2))
        assertFalse(rec.connected.isEmpty())
    }

    // @verifies REQ-SIMFW-19#c
    // @verifies REQ-SIMFW-19#c
    @Test fun the_apps_gatt_manager_connects_to_it_and_the_chunkers_writes_reach_the_upload_characteristic_in_order() = runTest {
        val bridge = FakeBridge()
        val central = SimulatedBleCentral(bridge)
        val refusals = mutableListOf<Pair<Int, Int>>()
        central.onAttRefusal = { u, att, _ -> refusals += SimulatedBleCentral.idOf(u) to att }
        val manager = NeurOneGattManager(central, TestScope(testScheduler))
        bridge.report(HubBridgeEvent.Opened)
        assertEquals(ConnectionState.CONNECTING, manager.connectionState.value, "the manager scans, finds the simulated hub and connects")
        // The twin's table: what np_gatt_server.c publishes, the protocol upload (0x08) among it (no firmware version).
        val published = listOf(0x07, 0x08, 0x10, 0x17, 0x14, 0x15, 0x16, 0x18, 0x0f, 0x1a, 0x1c, 0x1d, 0x1b, 0x1e, 0x20, 0x21)
        table(*published.toIntArray()).forEach(bridge::report)
        assertEquals(ConnectionState.CONNECTED, manager.connectionState.value)
        // The app's own chunker: the transport's stream, in writes of at most 512 bytes (REQ-FWHUB-80).
        val blob = ByteArray(1100) { (it * 7).toByte() }
        val chunks = life.neurone.core.session.ProtocolChunker.chunk(blob)
        chunks.forEach { manager.writeProtocolChunk(it) }
        val writes = bridge.sent.filterIsInstance<HubBridgeCommand.Write>()
        assertEquals(
            chunks.map { c -> HubBridgeCommand.Write(protocolUpload, c.joinToString("") { (it.toInt() and 0xFF).toString(16).padStart(2, '0') }) },
            writes, "every chunk goes to the upload characteristic, in order, as the bytes the chunker made",
        )
        bridge.report(HubBridgeEvent.WriteResult(protocolUpload, att = 0))
        assertTrue(refusals.isEmpty(), "a write the hub took is not a refusal")
        bridge.report(HubBridgeEvent.WriteResult(protocolUpload, att = 0x83))
        assertEquals(listOf(protocolUpload to 0x83), refusals, "the hub still holding the last blob (BUSY) is reported as the hub's refusal")
    }
}
