package life.neurone.shared

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.advanceTimeBy
import kotlinx.coroutines.test.runTest
import life.neurone.core.common.UUID
import life.neurone.core.protocol.GattUuidStrings
import life.neurone.shared.ble.AdapterState
import life.neurone.shared.ble.BleCentral
import life.neurone.shared.ble.BleCentralListener
import life.neurone.shared.ble.ConnectionState
import life.neurone.shared.ble.NeurOneGattManager
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * How the BLE manager scans, reconnects and forgets (`REQ-SW03-16`). Ported from the SwiftUI app's `NeurOneGATTManagerTests`, which drove a
 * mock central; the Kotlin manager takes the same seam, so the cases carry over.
 */
@OptIn(ExperimentalCoroutinesApi::class)
class GattConnectionTest {
    private class FakeCentral(override var adapterState: AdapterState) : BleCentral {
        var scans = 0
        val scanned = mutableListOf<UUID>()
        override fun setListener(listener: BleCentralListener) {}
        override fun startScan(serviceUuid: UUID) { scans++; scanned += serviceUuid }
        override fun stopScan() {}
        override fun connect(deviceId: String) {}
        override fun disconnect() {}
        override fun discoverCharacteristics(serviceUuid: UUID, uuids: List<UUID>) {}
        override fun enableNotifications(uuid: UUID) {}
        override fun read(uuid: UUID) {}
        override fun write(uuid: UUID, value: ByteArray) {}
    }

    private val allRequired = GattUuidStrings.REQUIRED_IDS.map(UUID::fromString).toSet()

    // @verifies REQ-SW03-16
    @Test fun it_scans_for_the_neurone_service_once_bluetooth_is_on() = runTest {
        val central = FakeCentral(AdapterState.OFF)
        val manager = NeurOneGattManager(central, TestScope(testScheduler))
        assertEquals(ConnectionState.DISCONNECTED, manager.connectionState.value)
        assertEquals(0, central.scans, "no scan before Bluetooth is available")

        central.adapterState = AdapterState.ON
        manager.onAdapterStateChanged(AdapterState.ON)
        assertEquals(ConnectionState.SCANNING, manager.connectionState.value)
        assertEquals(1, central.scans)
        assertEquals(listOf(UUID.fromString(GattUuidStrings.SERVICE_ID)), central.scanned, "the scan is filtered to the NeurOne service")
    }

    // @verifies REQ-SW03-16
    @Test fun a_second_power_on_scans_again() = runTest {
        val central = FakeCentral(AdapterState.ON)
        val manager = NeurOneGattManager(central, TestScope(testScheduler))
        assertEquals(1, central.scans)

        manager.onAdapterStateChanged(AdapterState.OFF)
        manager.onAdapterStateChanged(AdapterState.ON)
        assertEquals(2, central.scans)
        assertEquals(ConnectionState.SCANNING, manager.connectionState.value)
    }

    // @verifies REQ-SW03-16
    @Test fun it_does_not_scan_while_bluetooth_is_off_and_goes_back_to_disconnected_when_it_goes_off() = runTest {
        val off = FakeCentral(AdapterState.OFF)
        val offManager = NeurOneGattManager(off, TestScope(testScheduler))
        assertEquals(0, off.scans)
        assertEquals(ConnectionState.DISCONNECTED, offManager.connectionState.value)

        val central = FakeCentral(AdapterState.ON)
        val manager = NeurOneGattManager(central, TestScope(testScheduler))
        assertEquals(ConnectionState.SCANNING, manager.connectionState.value)
        manager.onAdapterStateChanged(AdapterState.OFF)
        assertEquals(ConnectionState.DISCONNECTED, manager.connectionState.value)
    }

    // @verifies REQ-SW03-16
    @Test fun a_disconnect_schedules_one_reconnect_scan_after_two_seconds_only_while_bluetooth_is_on() = runTest {
        val central = FakeCentral(AdapterState.ON)
        val manager = NeurOneGattManager(central, TestScope(testScheduler))
        assertEquals(1, central.scans)

        manager.onDisconnected("hub")
        assertEquals(ConnectionState.DISCONNECTED, manager.connectionState.value, "disconnected at once")
        testScheduler.advanceTimeBy(1_999)
        assertEquals(1, central.scans, "not before the delay")
        testScheduler.advanceTimeBy(2)
        assertEquals(2, central.scans, "then it scans again")

        // Bluetooth goes off before the next disconnect: the timer fires and finds nothing to do.
        central.adapterState = AdapterState.OFF
        manager.onAdapterStateChanged(AdapterState.OFF)
        manager.onDisconnected("hub")
        testScheduler.advanceTimeBy(2_100)
        assertEquals(2, central.scans, "no reconnect scan while Bluetooth is off")
    }

    // @verifies REQ-SW03-16
    @Test fun bluetooth_is_reported_unavailable_when_off_unauthorised_or_unknown_and_available_when_on() = runTest {
        for (state in listOf(AdapterState.OFF, AdapterState.UNAUTHORIZED, AdapterState.UNKNOWN)) {
            val manager = NeurOneGattManager(FakeCentral(state), TestScope(testScheduler))
            assertTrue(manager.bluetoothUnavailable.value, "unavailable when $state")
        }
        val manager = NeurOneGattManager(FakeCentral(AdapterState.ON), TestScope(testScheduler))
        assertFalse(manager.bluetoothUnavailable.value)
    }

    // @verifies REQ-SW03-16
    @Test fun the_characteristics_are_resolved_only_when_every_required_one_is_present() = runTest {
        val manager = NeurOneGattManager(FakeCentral(AdapterState.ON), TestScope(testScheduler))
        assertFalse(manager.allCharacteristicsResolved.value, "starts unresolved")

        manager.onCharacteristicsDiscovered(allRequired - UUID.fromString(GattUuidStrings.SESSION_STOP_ID))
        assertFalse(manager.allCharacteristicsResolved.value, "one missing is not enough")

        manager.onCharacteristicsDiscovered(allRequired)
        assertTrue(manager.allCharacteristicsResolved.value)
        assertTrue(UUID.fromString(GattUuidStrings.SESSION_STATUS_ID) in allRequired, "SESSION_STATUS is discovered so it can be read on reconnect")
    }

    // @verifies REQ-SHDR-02
    @Test fun the_warranty_token_is_absent_at_first_kept_whole_at_32_bytes_and_a_short_one_is_ignored() = runTest {
        val manager = NeurOneGattManager(FakeCentral(AdapterState.ON), TestScope(testScheduler))
        val id = UUID.fromString(GattUuidStrings.WARRANTY_TOKEN_ID)
        assertNull(manager.warrantyToken.value)

        manager.onCharacteristicRead(id, ByteArray(16) { 0x7F })
        assertNull(manager.warrantyToken.value, "a corrupt, short read must not become the token")

        val token = ByteArray(32) { 0xAB.toByte() }
        manager.onCharacteristicRead(id, token)
        assertEquals(token.toList(), manager.warrantyToken.value!!.toList())
    }

    // @verifies REQ-SHDR-02
    @Test fun a_longer_warranty_token_payload_is_cut_to_its_first_32_bytes() = runTest {
        val manager = NeurOneGattManager(FakeCentral(AdapterState.ON), TestScope(testScheduler))
        manager.onCharacteristicRead(UUID.fromString(GattUuidStrings.WARRANTY_TOKEN_ID), ByteArray(40) { it.toByte() })
        assertEquals((0 until 32).map { it.toByte() }, manager.warrantyToken.value!!.toList())
    }

    private fun zoneFrame(snapshot: Boolean, last: Boolean, fragment: Int = 0, vararg records: Triple<Int, Int, Int>): ByteArray {
        val flags = (if (snapshot) 0x01 else 0) or (if (last) 0x02 else 0)
        val out = mutableListOf(0x02, flags, fragment, records.size)
        records.forEach { out += listOf(it.first, it.second, it.third) }
        return ByteArray(out.size) { out[it].toByte() }
    }

    private fun manager(testScope: TestScope) = NeurOneGattManager(FakeCentral(AdapterState.ON), TestScope(testScope.testScheduler))

    // @verifies REQ-SW03-02
    @Test fun a_snapshot_populates_the_inventory_with_sockets_far_beyond_five() = runTest {
        val m = manager(this)
        m.applyZoneModuleStatus(zoneFrame(true, true, 0, Triple(12, 2, 0x01), Triple(47, 1, 0x01), Triple(128, 1, 0x00)))
        assertEquals(listOf(12, 47, 128), m.zoneModules.value.orderedSockets.map { it.socketId })
        assertEquals(2, m.zoneModules.value.presentSockets.size)
        assertTrue(m.zoneModules.value.isPresent(47))
        assertFalse(m.zoneModules.value.isPresent(128))
    }

    // @verifies REQ-SW03-02
    @Test fun a_delta_changes_only_the_socket_it_names_and_a_snapshot_replaces_rather_than_merges() = runTest {
        val m = manager(this)
        m.applyZoneModuleStatus(zoneFrame(true, true, 0, Triple(12, 2, 0x01), Triple(47, 1, 0x01)))
        m.applyZoneModuleStatus(zoneFrame(false, true, 0, Triple(12, 0, 0x00)))
        assertFalse(m.zoneModules.value.isPresent(12))
        assertTrue(m.zoneModules.value.isPresent(47), "a delta must not disturb sockets it does not name")

        m.applyZoneModuleStatus(zoneFrame(true, true, 0, Triple(5, 1, 0x01)))
        assertEquals(listOf(5), m.zoneModules.value.orderedSockets.map { it.socketId }, "a snapshot is a whole inventory claim")
    }

    // @verifies REQ-SW03-02
    @Test fun a_malformed_zone_frame_is_dropped_whole_and_leaves_the_inventory_untouched() = runTest {
        val m = manager(this)
        m.applyZoneModuleStatus(zoneFrame(true, true, 0, Triple(12, 2, 0x01)))
        m.applyZoneModuleStatus(byteArrayOf(0x03, 0x03, 0x00, 0x01, 0x05, 0x01, 0x01))   // wrong version
        m.applyZoneModuleStatus(byteArrayOf(0x02, 0x03, 0x00, 0x04, 0x05))               // truncated body
        assertEquals(listOf(12), m.zoneModules.value.orderedSockets.map { it.socketId })
    }

    // @verifies REQ-SW03-02
    @Test fun a_part_received_snapshot_is_not_applied_until_its_last_fragment_arrives() = runTest {
        val m = manager(this)
        m.applyZoneModuleStatus(zoneFrame(true, false, 0, Triple(1, 1, 0x01)))
        assertTrue(m.zoneModules.value.isEmpty, "a part-received snapshot must not be applied")
        m.applyZoneModuleStatus(zoneFrame(true, true, 1, Triple(2, 1, 0x01)))
        assertEquals(listOf(1, 2), m.zoneModules.value.orderedSockets.map { it.socketId })
    }

    // @verifies REQ-SW03-02
    @Test fun the_zone_inventory_is_cleared_on_disconnect_so_a_pulled_module_cannot_still_read_present() = runTest {
        val m = NeurOneGattManager(FakeCentral(AdapterState.ON), TestScope(testScheduler))
        m.onDeviceFound("hub")
        m.onConnected("hub")
        m.applyZoneModuleStatus(zoneFrame(true, true, 0, Triple(12, 2, 0x01)))
        assertFalse(m.zoneModules.value.isEmpty)
        m.onDisconnected("hub")
        assertTrue(m.zoneModules.value.isEmpty)
    }

    // @verifies REQ-SW03-03
    @Test fun a_pad_failure_raises_the_alert_a_clean_check_clears_it_and_a_malformed_frame_changes_nothing() = runTest {
        val m = manager(this)
        val pad = UUID.fromString(GattUuidStrings.CVNS_PAD_STATUS_ID)
        assertNull(m.cervicalPadAlert.value)

        m.onCharacteristicChanged(pad, byteArrayOf(0x01, 0x00, 0x01, 0x02))
        val raised = m.cervicalPadAlert.value
        assertNotNull(raised)
        assertTrue(raised.hasFailure)

        m.onCharacteristicChanged(pad, byteArrayOf(0x80.toByte(), 0x00, 0x01, 0x02))   // reserved bit set: discarded
        assertEquals(raised, m.cervicalPadAlert.value)

        m.onCharacteristicChanged(pad, byteArrayOf(0x00, 0x00, 0x01, 0x02))
        assertNull(m.cervicalPadAlert.value, "both pads passing clears the alert")
    }

    // @verifies REQ-SW03-03
    @Test fun an_acknowledged_pad_alert_is_raised_again_by_the_next_failure() = runTest {
        val m = manager(this)
        val pad = UUID.fromString(GattUuidStrings.CVNS_PAD_STATUS_ID)
        m.onCharacteristicChanged(pad, byteArrayOf(0x03, 0x01, 0x01, 0x02))
        m.acknowledgeCervicalPadAlert()
        assertNull(m.cervicalPadAlert.value)
        m.onCharacteristicChanged(pad, byteArrayOf(0x03, 0x01, 0x01, 0x02))
        assertNotNull(m.cervicalPadAlert.value, "a new failure after acknowledgement must alert again")
    }

    // @verifies REQ-SW03-03
    @Test fun the_pad_alert_does_not_survive_a_disconnect_and_the_pad_characteristic_is_not_required() = runTest {
        val m = NeurOneGattManager(FakeCentral(AdapterState.ON), TestScope(testScheduler))
        m.onDeviceFound("hub")
        m.onConnected("hub")
        m.onCharacteristicChanged(UUID.fromString(GattUuidStrings.CVNS_PAD_STATUS_ID), byteArrayOf(0x02, 0x00, 0x01, 0x02))
        assertNotNull(m.cervicalPadAlert.value)
        m.onDisconnected("hub")
        assertNull(m.cervicalPadAlert.value)
        assertFalse(GattUuidStrings.CVNS_PAD_STATUS_ID in GattUuidStrings.REQUIRED_IDS)
    }

    // @verifies REQ-SW03-16
    @Test fun a_disconnect_clears_what_the_last_hub_reported() = runTest {
        val manager = NeurOneGattManager(FakeCentral(AdapterState.ON), TestScope(testScheduler))
        manager.onDeviceFound("hub")
        manager.onConnected("hub")
        manager.onCharacteristicsDiscovered(allRequired)
        manager.onCharacteristicRead(UUID.fromString(GattUuidStrings.WARRANTY_TOKEN_ID), ByteArray(32) { 7 })
        manager.onCharacteristicChanged(UUID.fromString(GattUuidStrings.SESSION_STATUS_ID), byteArrayOf(5, 1))
        assertEquals("hub", manager.hubIdentifier.value)
        assertTrue(manager.allCharacteristicsResolved.value)
        assertNotNull(manager.warrantyToken.value)
        assertEquals(5, manager.session.value.protocolId)

        manager.onDisconnected("hub")

        // The next hub must not be shown with the previous one's session, token or identity.
        assertFalse(manager.allCharacteristicsResolved.value)
        assertNull(manager.hubIdentifier.value)
        assertNull(manager.warrantyToken.value)
        assertNull(manager.hubFirmwareVersion.value)
        assertEquals(life.neurone.core.models.SessionState.EMPTY, manager.session.value)
        assertEquals(life.neurone.core.models.ZoneModuleConfiguration.EMPTY, manager.zoneModules.value)
        assertEquals(life.neurone.core.models.SocketMap.EMPTY, manager.socketMap.value)
        assertNull(manager.cervicalFaultStatus.value)
    }
}
