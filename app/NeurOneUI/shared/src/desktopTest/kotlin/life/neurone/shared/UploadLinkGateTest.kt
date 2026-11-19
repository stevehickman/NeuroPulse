package life.neurone.shared

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestScope
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
 * The upload waits for the hub's encrypted-only `DEVICE_SERIAL` to be read on the link, and a write the link refuses reaches the
 * manager (`REQ-SW03-31`, `OI-PRIV-16`). The links here are fakes: no radio and no hub.
 */
@OptIn(ExperimentalCoroutinesApi::class)
class UploadLinkGateTest {
    /** A link that takes every command and records the writes; the test plays the peer's answers into the manager. */
    private class FakeLink : BleCentral {
        override val adapterState = AdapterState.ON
        val writes = mutableListOf<UUID>()
        val reads = mutableListOf<UUID>()
        override fun setListener(listener: BleCentralListener) {}
        override fun startScan(serviceUuid: UUID) {}
        override fun stopScan() {}
        override fun connect(deviceId: String) {}
        override fun disconnect() {}
        override fun discoverCharacteristics(serviceUuid: UUID, uuids: List<UUID>) {}
        override fun enableNotifications(uuid: UUID) {}
        override fun read(uuid: UUID) { reads += uuid }
        override fun write(uuid: UUID, value: ByteArray) { writes += uuid }
    }

    private class FakeBridge : HubBridge {
        var onEvent: (HubBridgeEvent) -> Unit = {}
        val sent = mutableListOf<HubBridgeCommand>()
        override fun setReceiver(receiver: (HubBridgeEvent) -> Unit) { onEvent = receiver }
        override fun send(command: HubBridgeCommand): Boolean { sent += command; return true }
    }

    private val serial = UUID.fromString(GattUuidStrings.DEVICE_SERIAL_ID)
    private val upload = UUID.fromString(GattUuidStrings.PROTOCOL_UPLOAD_ID)

    private fun tdcs() = NPProtocolDefinition(
        name = "tDCS at 1 mA",
        timingMode = NPTimingMode.Duration(20 * 60),
        modalities = listOf(
            NPProtocolModality(params = NPModalityParams.Tdcs(NPTDCSParams(intensityMilliamps = 1.0, electrodePairs = listOf(listOf("Fp1", "P3"))))),
        ),
    )

    private fun linked(link: FakeLink, scope: TestScope): NeurOneGattManager =
        NeurOneGattManager(link, scope).also {
            it.onDeviceFound("hub")
            it.onConnected("hub")
        }

    // @verifies REQ-SW03-31#b
    @Test fun no_upload_is_written_until_the_encrypted_only_read_has_succeeded_on_the_link() = runTest {
        val link = FakeLink()
        val manager = linked(link, TestScope(testScheduler))
        val uploader = ProtocolUploader(manager, signer = Ed25519ProtocolSigner())

        // Connected, every characteristic found, and the peer has answered no encrypted-only read: a peripheral that only
        // advertises the service is here. Nothing is written, by the uploader or by a caller that goes round it.
        manager.onCharacteristicsDiscovered(setOf(serial, upload))
        assertEquals(ProtocolUploader.Result.LinkNotEncrypted, uploader.upload(tdcs(), deviceSerial = ByteArray(32)))
        assertFalse(manager.writeProtocolChunk(byteArrayOf(1, 2, 3)))
        assertEquals(emptyList(), link.writes, "no write reached the peer")

        manager.onCharacteristicRead(serial, ByteArray(32) { 7 })
        assertIs<ProtocolUploader.Result.Success>(uploader.upload(tdcs()))
        assertTrue(link.writes.isNotEmpty() && link.writes.all { it == upload }, "the upload was written once the read had succeeded")
    }

    // @verifies REQ-SW03-31#b
    @Test fun the_proof_is_per_link_and_a_refused_read_takes_it_back() = runTest {
        val link = FakeLink()
        val manager = linked(link, TestScope(testScheduler))
        val uploader = ProtocolUploader(manager, signer = Ed25519ProtocolSigner())
        manager.onCharacteristicRead(serial, ByteArray(32))
        assertTrue(manager.encryptedReadShown.value)

        // The link drops and another comes up: what the last link showed says nothing about this one.
        manager.onDisconnected("hub")
        manager.onDeviceFound("other")
        manager.onConnected("other")
        assertFalse(manager.encryptedReadShown.value)
        assertEquals(ProtocolUploader.Result.LinkNotEncrypted, uploader.upload(tdcs(), deviceSerial = ByteArray(32)))

        // A read the same link later refuses closes the gate again.
        manager.onCharacteristicRead(serial, ByteArray(32))
        manager.onCharacteristicReadFailed(serial)
        assertEquals(ProtocolUploader.Result.LinkNotEncrypted, uploader.upload(tdcs(), deviceSerial = ByteArray(32)))
        assertEquals(emptyList(), link.writes)
    }

    // @verifies REQ-SW03-31#b
    @Test fun a_read_reported_as_a_change_opens_the_gate_and_a_value_with_no_link_does_not() = runTest {
        // CoreBluetooth delivers a read's value through the notification callback, so the iOS central reports it as a change.
        val link = FakeLink()
        val manager = NeurOneGattManager(link, TestScope(testScheduler))
        manager.onCharacteristicChanged(serial, ByteArray(32))
        assertFalse(manager.encryptedReadShown.value, "no link is up")

        manager.onDeviceFound("hub")
        manager.onConnected("hub")
        manager.onCharacteristicChanged(serial, ByteArray(32))
        assertTrue(manager.encryptedReadShown.value)
    }

    // @verifies REQ-SW03-31#a
    @Test fun a_refused_upload_write_reaches_the_manager_and_marks_the_upload_refused() = runTest {
        val link = FakeLink()
        val manager = linked(link, TestScope(testScheduler))
        val uploader = ProtocolUploader(manager, signer = Ed25519ProtocolSigner())
        manager.onCharacteristicRead(serial, ByteArray(32))
        assertIs<ProtocolUploader.Result.Success>(uploader.upload(tdcs()))
        assertFalse(uploader.writeRefused)

        val readsBefore = link.reads.size
        val writesBefore = link.writes.size
        manager.onCharacteristicWriteFailed(upload)
        assertTrue(manager.uploadWriteRefused.value)
        assertTrue(uploader.writeRefused, "the uploader does not report this upload stored")
        assertEquals(listOf(serial), link.reads.drop(readsBefore), "the serial is read again, which raises pairing if the link is not paired")
        assertEquals(writesBefore, link.writes.size, "a refused write is not sent again")

        // A refusal of another write is not a refused upload, and the next upload starts clean.
        assertIs<ProtocolUploader.Result.Success>(uploader.upload(tdcs()))
        assertFalse(uploader.writeRefused)
        manager.onCharacteristicWriteFailed(UUID.fromString(GattUuidStrings.PAUSE_LIMIT_ID))
        assertFalse(manager.uploadWriteRefused.value)

        // With no link there is nothing to mark.
        manager.onDisconnected("hub")
        manager.onCharacteristicWriteFailed(upload)
        assertFalse(manager.uploadWriteRefused.value)
    }

    // @verifies REQ-SW03-31#a
    @Test fun the_simulated_hubs_att_refusal_of_a_write_is_the_same_event() = runTest {
        val bridge = FakeBridge()
        val central = SimulatedBleCentral(bridge)
        val manager = NeurOneGattManager(central, TestScope(testScheduler))
        bridge.onEvent(HubBridgeEvent.Opened)
        listOf(0x08, 0x17).forEachIndexed { i, id -> bridge.onEvent(HubBridgeEvent.Characteristic(i, 2, id, props = 1)) }
        bridge.onEvent(HubBridgeEvent.ReadResult(0x17, att = 0, hex = "00".repeat(32)))
        assertTrue(manager.writeProtocolChunk(byteArrayOf(1)))

        bridge.onEvent(HubBridgeEvent.WriteResult(0x08, att = 0))
        assertFalse(manager.uploadWriteRefused.value, "a write the hub took is not a refusal")
        bridge.onEvent(HubBridgeEvent.WriteResult(0x08, att = 0x0F)) // Insufficient Encryption
        assertTrue(manager.uploadWriteRefused.value)
    }
}
