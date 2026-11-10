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
        assertEquals(listOf(HubBridgeCommand.Init, HubBridgeCommand.Table), bridge.sent, "the twin is brought up, then its table asked for")
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
        bridge.report(HubBridgeEvent.WriteResult(protocolUpload, att = 0x0a))
        assertEquals(listOf(Triple(SimulatedBleCentral.uuidOf(protocolUpload), 0x0a, false)), refusals)
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
    @Test fun the_apps_gatt_manager_connects_to_it_and_an_upload_write_is_refused_as_the_hub_refuses_it() = runTest {
        val bridge = FakeBridge()
        val central = SimulatedBleCentral(bridge)
        val refusals = mutableListOf<Pair<Int, Int>>()
        central.onAttRefusal = { u, att, _ -> refusals += SimulatedBleCentral.idOf(u) to att }
        val manager = NeurOneGattManager(central, TestScope(testScheduler))
        bridge.report(HubBridgeEvent.Opened)
        assertEquals(ConnectionState.CONNECTING, manager.connectionState.value, "the manager scans, finds the simulated hub and connects")
        // The twin's table: what np_gatt_server.c publishes (no protocol upload, no firmware version).
        val published = listOf(0x07, 0x10, 0x17, 0x14, 0x15, 0x16, 0x18, 0x0f, 0x1a, 0x1c, 0x1d, 0x1b, 0x1e, 0x20, 0x21)
        table(*published.toIntArray()).forEach(bridge::report)
        assertEquals(ConnectionState.CONNECTED, manager.connectionState.value)
        manager.writeProtocolChunk(byteArrayOf(0x04, 0x01, 0x02))
        assertEquals(HubBridgeCommand.Write(protocolUpload, "040102"), bridge.sent.last(), "the app writes its upload chunk to the upload characteristic")
        bridge.report(HubBridgeEvent.WriteResult(protocolUpload, att = 0x0a))
        assertEquals(listOf(protocolUpload to 0x0a), refusals, "the hub does not publish the upload, so it is not found")
    }
}
