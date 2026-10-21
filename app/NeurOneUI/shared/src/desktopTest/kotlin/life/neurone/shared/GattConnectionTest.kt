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
