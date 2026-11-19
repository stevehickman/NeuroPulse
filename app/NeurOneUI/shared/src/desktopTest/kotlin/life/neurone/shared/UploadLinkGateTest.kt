package life.neurone.shared

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.launch
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.UnconfinedTestDispatcher
import kotlinx.coroutines.test.runTest
import life.neurone.core.common.UUID
import life.neurone.core.protocol.GattUuidStrings
import life.neurone.core.protocol.NPModalityParams
import life.neurone.core.protocol.NPProtocolDefinition
import life.neurone.core.protocol.NPProtocolModality
import life.neurone.core.protocol.NPTDCSParams
import life.neurone.core.protocol.NPTimingMode
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.shared.ble.AdapterState
import life.neurone.shared.ble.BleCentral
import life.neurone.shared.ble.BleCentralListener
import life.neurone.shared.ble.HubBridge
import life.neurone.shared.ble.HubBridgeCommand
import life.neurone.shared.ble.HubBridgeEvent
import life.neurone.shared.ble.NeurOneGattManager
import life.neurone.shared.ble.SimulatedBleCentral
import life.neurone.shared.session.ProtocolUploader
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertIs
import kotlin.test.assertTrue

/**
 * No protocol is written until the hub has answered a read of its encrypted-only serial on the same link, and a write the
 * link refuses reaches the manager (`OI-PRIV-16`, `REQ-SW03-31`). The link here is a fake: what is tested is the order of
 * the app's own calls, not a radio.
 */
@OptIn(ExperimentalCoroutinesApi::class)
class UploadLinkGateTest {
    /** A link that records every read and write the app asks of it, in order. */
    private class FakeLink : BleCentral {
        override val adapterState = AdapterState.ON
        val calls = mutableListOf<String>()
        val writes get() = calls.filter { it.startsWith("write") }
        override fun setListener(listener: BleCentralListener) {}
        override fun startScan(serviceUuid: UUID) {}
        override fun stopScan() {}
        override fun connect(deviceId: String) {}
        override fun disconnect() {}
        override fun discoverCharacteristics(serviceUuid: UUID, uuids: List<UUID>) {}
        override fun enableNotifications(uuid: UUID) {}
        override fun read(uuid: UUID) { calls += "read $uuid" }
        override fun write(uuid: UUID, value: ByteArray) { calls += "write $uuid" }
    }

    private val serial = UUID.fromString(GattUuidStrings.DEVICE_SERIAL_ID)
    private val upload = UUID.fromString(GattUuidStrings.PROTOCOL_UPLOAD_ID)

    private fun tdcs() = NPProtocolDefinition(
        name = "tDCS",
        timingMode = NPTimingMode.Duration(20 * 60),
        modalities = listOf(
            NPProtocolModality(params = NPModalityParams.Tdcs(NPTDCSParams(intensityMilliamps = 1.0, electrodePairs = listOf(listOf("Fp1", "P3"))))),
        ),
    )

    /** Connected, with the serial characteristic discovered and its first read asked for but not answered. */
    private fun linked(link: FakeLink, scope: TestScope): NeurOneGattManager {
        val manager = NeurOneGattManager(link, scope)
        manager.onDeviceFound("hub")
        manager.onConnected("hub")
        manager.onCharacteristicsDiscovered(setOf(serial, upload))
        return manager
    }

    // @verifies REQ-SW03-31#a
    @Test fun no_protocol_is_written_until_the_encrypted_only_read_has_succeeded_on_the_link() = runTest {
        val link = FakeLink()
        val manager = linked(link, TestScope(testScheduler))
        val uploader = ProtocolUploader(manager, signer = Ed25519ProtocolSigner())
        assertEquals("read $serial", link.calls.first { it == "read $serial" }, "the serial is read at link")

        // The read has not been answered: nothing is sent, the result says why, and the read is asked for again.
        val readsBefore = link.calls.count { it == "read $serial" }
        assertEquals(ProtocolUploader.Result.LinkNotEncrypted, uploader.upload(tdcs(), deviceSerial = ByteArray(32)))
        assertEquals(emptyList(), link.writes, "no write of any kind reached the link")
        assertEquals(readsBefore + 1, link.calls.count { it == "read $serial" })
        assertFalse(manager.writeProtocolChunk(byteArrayOf(1)), "the manager itself refuses a chunk")
        assertEquals(emptyList(), link.writes)

        // The read is refused (an unpaired link): still nothing.
        manager.onCharacteristicReadFailed(serial)
        assertTrue(manager.pairingRequired.value)
        assertEquals(ProtocolUploader.Result.LinkNotEncrypted, uploader.upload(tdcs(), deviceSerial = ByteArray(32)))
        assertEquals(emptyList(), link.writes)

        // The read succeeds: the upload goes, and every write is after that read was asked for.
        manager.onCharacteristicRead(serial, ByteArray(32) { 7 })
        assertTrue(manager.linkEncryptionShown.value)
        assertIs<ProtocolUploader.Result.Success>(uploader.upload(tdcs()))
        assertTrue(link.writes.isNotEmpty() && link.writes.all { it == "write $upload" })
        assertTrue(link.calls.indexOf("read $serial") < link.calls.indexOf("write $upload"))
    }

    // @verifies REQ-SW03-31#b
    @Test fun the_showing_is_per_link_and_a_malformed_serial_or_one_from_no_link_shows_nothing() = runTest {
        val link = FakeLink()
        val manager = NeurOneGattManager(link, TestScope(testScheduler))
        val uploader = ProtocolUploader(manager, signer = Ed25519ProtocolSigner())

        // Not connected: a serial that arrives shows nothing.
        manager.onCharacteristicRead(serial, ByteArray(32))
        assertFalse(manager.linkEncryptionShown.value)

        manager.onDeviceFound("hub")
        manager.onConnected("hub")
        manager.onCharacteristicsDiscovered(setOf(serial, upload))
        // A read that is not the 32-byte serial shows nothing, and a successful read of another characteristic shows nothing.
        manager.onCharacteristicRead(serial, ByteArray(31))
        manager.onCharacteristicRead(UUID.fromString(GattUuidStrings.FIRMWARE_VERSION_ID), ByteArray(4))
        assertFalse(manager.linkEncryptionShown.value)
        assertEquals(ProtocolUploader.Result.LinkNotEncrypted, uploader.upload(tdcs(), deviceSerial = ByteArray(32)))

        manager.onCharacteristicRead(serial, ByteArray(32))
        assertTrue(manager.linkEncryptionShown.value)

        // A later read of the serial is refused: the gate closes again, and the next read that succeeds reopens it.
        manager.onCharacteristicReadFailed(serial)
        assertFalse(manager.linkEncryptionShown.value)
        assertEquals(ProtocolUploader.Result.LinkNotEncrypted, uploader.upload(tdcs(), deviceSerial = ByteArray(32)))
        assertEquals(emptyList(), link.writes)
        manager.onCharacteristicRead(serial, ByteArray(32))
        assertTrue(manager.linkEncryptionShown.value)

        // The link drops and comes back: the new link has shown nothing, and the caller's own serial does not stand in for it.
        manager.onDisconnected("hub")
        assertFalse(manager.linkEncryptionShown.value)
        manager.onDeviceFound("hub")
        manager.onConnected("hub")
        manager.onCharacteristicsDiscovered(setOf(serial, upload))
        assertEquals(ProtocolUploader.Result.LinkNotEncrypted, uploader.upload(tdcs(), deviceSerial = ByteArray(32)))
        assertEquals(emptyList(), link.writes, "nothing was written on either link")

        // A hub that does not publish the serial can never show it: nothing is sent and no read is made up.
        manager.onDisconnected("hub")
        manager.onDeviceFound("hub")
        manager.onConnected("hub")
        manager.onCharacteristicsDiscovered(setOf(upload))
        val reads = link.calls.count { it == "read $serial" }
        assertEquals(ProtocolUploader.Result.LinkNotEncrypted, uploader.upload(tdcs(), deviceSerial = ByteArray(32)))
        assertEquals(reads, link.calls.count { it == "read $serial" })
        assertEquals(emptyList(), link.writes)
    }

    // @verifies REQ-SW03-31#c
    @Test fun a_refused_write_reaches_the_manager_is_counted_for_the_upload_and_is_not_written_again() = runTest {
        val link = FakeLink()
        val manager = NeurOneGattManager(link, TestScope(testScheduler))
        val seen = mutableListOf<UUID>()
        backgroundScope.launch(UnconfinedTestDispatcher(testScheduler)) { manager.writeRefusals.collect { seen += it } }

        // No link: a refusal belongs to nothing.
        manager.onCharacteristicWriteFailed(upload)
        assertEquals(0, manager.uploadWriteRefusals.value)

        manager.onDeviceFound("hub")
        manager.onConnected("hub")
        manager.onCharacteristicsDiscovered(setOf(serial, upload))
        manager.onCharacteristicRead(serial, ByteArray(32))
        assertTrue(manager.writeProtocolChunk(byteArrayOf(1, 2)))
        val writes = link.writes.size

        val readsBefore = link.calls.count { it == "read $serial" }
        manager.onCharacteristicWriteFailed(upload)
        val pauseLimit = UUID.fromString(GattUuidStrings.PAUSE_LIMIT_ID)
        manager.onCharacteristicWriteFailed(pauseLimit)
        assertEquals(1, manager.uploadWriteRefusals.value, "only the upload's refusals are counted against the upload")
        assertEquals(listOf(upload, pauseLimit), seen, "every refusal is an event, with its characteristic")
        assertEquals(writes, link.writes.size, "a refused write is not written again")
        assertEquals(readsBefore + 1, link.calls.count { it == "read $serial" }, "a refused upload write asks for the serial again, once")

        manager.onDisconnected("hub")
        assertEquals(0, manager.uploadWriteRefusals.value, "the count is the link's")
    }

    // @verifies REQ-SW03-31#c
    @Test fun the_uploader_says_the_upload_last_sent_was_refused_and_forgets_it_at_the_next() = runTest {
        val link = FakeLink()
        val manager = linked(link, TestScope(testScheduler))
        manager.onCharacteristicRead(serial, ByteArray(32))
        val uploader = ProtocolUploader(manager, signer = Ed25519ProtocolSigner())

        assertIs<ProtocolUploader.Result.Success>(uploader.upload(tdcs()))
        assertFalse(uploader.writeRefused)
        manager.onCharacteristicWriteFailed(upload)
        assertTrue(uploader.writeRefused, "the screen is told, so no answer from the helmet is not shown as stored")

        // The refused write asked for the serial again; here the hub answers it, and the next upload starts clean.
        manager.onCharacteristicRead(serial, ByteArray(32))
        assertIs<ProtocolUploader.Result.Success>(uploader.upload(tdcs()))
        assertFalse(uploader.writeRefused)

        manager.onCharacteristicWriteFailed(upload)
        manager.onDisconnected("hub")
        assertFalse(uploader.writeRefused, "a refusal belongs to its link")
    }

    private class FakeBridge : HubBridge {
        var onEvent: (HubBridgeEvent) -> Unit = {}
        override fun setReceiver(receiver: (HubBridgeEvent) -> Unit) { onEvent = receiver }
        override fun send(command: HubBridgeCommand): Boolean = true
    }

    // @verifies REQ-SW03-31#c
    @Test fun the_simulated_hubs_refusal_of_an_upload_write_reaches_the_manager() = runTest {
        val bridge = FakeBridge()
        val manager = NeurOneGattManager(SimulatedBleCentral(bridge), TestScope(testScheduler))
        bridge.onEvent(HubBridgeEvent.Opened)
        listOf(0x08, 0x17).forEachIndexed { i, id -> bridge.onEvent(HubBridgeEvent.Characteristic(index = i, count = 2, id = id, props = 1)) }

        bridge.onEvent(HubBridgeEvent.WriteResult(id = 0x08, att = 0))
        assertEquals(0, manager.uploadWriteRefusals.value, "a write the hub took is not a refusal")
        // ATT 0x0F, Insufficient Encryption: what the hub answers on a link that is not encrypted (REQ-FWHUB-81).
        bridge.onEvent(HubBridgeEvent.WriteResult(id = 0x08, att = 0x0F))
        assertEquals(1, manager.uploadWriteRefusals.value)
    }
}
