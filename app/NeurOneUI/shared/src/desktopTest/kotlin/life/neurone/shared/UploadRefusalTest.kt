package life.neurone.shared

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.runTest
import life.neurone.core.common.UUID
import life.neurone.core.protocol.NPCervicalVnsParams
import life.neurone.core.protocol.NPModalityParams
import life.neurone.core.protocol.NPProtocolDefinition
import life.neurone.core.protocol.NPProtocolModality
import life.neurone.core.protocol.NPTDCSParams
import life.neurone.core.protocol.NPTimingMode
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.shared.ble.AdapterState
import life.neurone.shared.ble.BleCentral
import life.neurone.shared.ble.BleCentralListener
import life.neurone.shared.ble.NeurOneGattManager
import life.neurone.shared.session.ProtocolUploader
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertIs
import kotlin.test.assertTrue

/**
 * A protocol over a hardware limit is refused before anything reaches the helmet (`SWR-SS-03`). Ported from the SwiftUI app's
 * `SessionProtocolUploaderTests`; the validator's own cases are in `NPProtocolValidatorTests`, so this checks the other half, that a
 * refusal sends no chunk.
 */
@OptIn(ExperimentalCoroutinesApi::class)
class UploadRefusalTest {
    private class RecordingCentral : BleCentral {
        override val adapterState = AdapterState.ON
        val writes = mutableListOf<UUID>()
        val chunks = mutableListOf<ByteArray>()
        override fun setListener(listener: BleCentralListener) {}
        override fun startScan(serviceUuid: UUID) {}
        override fun stopScan() {}
        override fun connect(deviceId: String) {}
        override fun disconnect() {}
        override fun discoverCharacteristics(serviceUuid: UUID, uuids: List<UUID>) {}
        override fun enableNotifications(uuid: UUID) {}
        override fun read(uuid: UUID) {}
        override fun write(uuid: UUID, value: ByteArray) { writes += uuid; chunks += value }
    }

    private fun tdcs(milliamps: Double) = NPProtocolDefinition(
        name = "tDCS at $milliamps mA",
        timingMode = NPTimingMode.Duration(20 * 60),
        modalities = listOf(
            NPProtocolModality(params = NPModalityParams.Tdcs(NPTDCSParams(intensityMilliamps = milliamps, electrodePairs = listOf(listOf("Fp1", "P3"))))),
        ),
    )

    private fun connected(central: RecordingCentral, scope: TestScope): ProtocolUploader {
        val manager = NeurOneGattManager(central, scope)
        manager.onDeviceFound("hub")
        manager.onConnected("hub")
        return ProtocolUploader(manager, signer = Ed25519ProtocolSigner())
    }

    // @verifies SWR-SS-03
    @Test fun a_protocol_over_a_hardware_limit_is_refused_and_no_chunk_is_written() = runTest {
        val central = RecordingCentral()
        val result = connected(central, TestScope(testScheduler)).upload(tdcs(2.5), deviceSerial = ByteArray(32))

        // tDCS above its 2 mA ceiling: refused outright, with no way to acknowledge it, and nothing was sent.
        assertIs<ProtocolUploader.Result.Failure>(result)
        assertEquals(emptyList(), central.writes, "no write reached the helmet")
    }

    // @verifies SWR-SS-03 DI-SAFE-06
    @Test fun the_same_protocol_inside_the_limit_is_written() = runTest {
        val central = RecordingCentral()
        val result = connected(central, TestScope(testScheduler)).upload(tdcs(1.0), deviceSerial = ByteArray(32))

        assertIs<ProtocolUploader.Result.Success>(result)
        assertTrue(central.writes.isNotEmpty(), "the chunks were written")
    }

    private fun cervical() = NPProtocolDefinition(
        name = "Cervical",
        timingMode = NPTimingMode.Duration(120),
        modalities = listOf(NPProtocolModality(params = NPModalityParams.CervicalVns(NPCervicalVnsParams()))),
    )

    /** An unread cardiac cutoff from an offline session: [version, state, 1 record, 0] + [counter 7, kind 1 (heart-rate change), no sides]. */
    private val unreadCutoff = byteArrayOf(0x01, 0x00, 0x01, 0x00, 0x07, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00)

    /** Another person on this device has an outstanding cutoff: no records, the outstanding flag set and not this user's. */
    private val anotherPersonsCutoff = byteArrayOf(0x01, 0x00, 0x00, 0x02)

    private fun connectedManager(central: RecordingCentral, scope: TestScope): Pair<NeurOneGattManager, ProtocolUploader> {
        val manager = NeurOneGattManager(central, scope)
        manager.onDeviceFound("hub")
        manager.onConnected("hub")
        return manager to ProtocolUploader(manager, signer = Ed25519ProtocolSigner())
    }

    // @verifies REQ-SW03-03 DI-SAFE-09 REQ-CVNS-09#f
    @Test fun a_cervical_protocol_is_refused_while_a_cardiac_cutoff_is_unread_and_nothing_reaches_the_hub() = runTest {
        val central = RecordingCentral()
        val (manager, uploader) = connectedManager(central, TestScope(testScheduler))
        manager.applyCervicalFaultStatus(unreadCutoff)

        assertEquals(ProtocolUploader.Result.CervicalRestartBlocked, uploader.upload(cervical(), deviceSerial = ByteArray(32)))
        assertEquals(emptyList(), central.writes, "nothing may reach the hub")
    }

    // @verifies REQ-SW03-03
    @Test fun a_protocol_without_cervical_stimulation_is_not_affected_by_an_unread_cutoff() = runTest {
        val central = RecordingCentral()
        val (manager, uploader) = connectedManager(central, TestScope(testScheduler))
        manager.applyCervicalFaultStatus(unreadCutoff)

        assertIs<ProtocolUploader.Result.Success>(uploader.upload(tdcs(1.0), deviceSerial = ByteArray(32)))
        assertTrue(central.writes.isNotEmpty())
    }

    // @verifies REQ-SW03-03 DI-SAFE-09 REQ-CVNS-09#f
    @Test fun another_persons_cutoff_needs_one_confirmation_per_upload() = runTest {
        val central = RecordingCentral()
        val (manager, uploader) = connectedManager(central, TestScope(testScheduler))
        manager.applyCervicalFaultStatus(anotherPersonsCutoff)

        assertEquals(ProtocolUploader.Result.DifferentPersonConfirmationRequired, uploader.upload(cervical(), deviceSerial = ByteArray(32)))
        assertEquals(emptyList(), central.writes)

        uploader.confirmDifferentPerson()
        val confirmed = uploader.upload(cervical(), deviceSerial = ByteArray(32))
        assertTrue(confirmed !is ProtocolUploader.Result.DifferentPersonConfirmationRequired && confirmed !is ProtocolUploader.Result.CervicalRestartBlocked, "confirmed, so the gate lets it through")

        // The confirmation is one-shot: the next upload asks again.
        assertEquals(ProtocolUploader.Result.DifferentPersonConfirmationRequired, uploader.upload(cervical(), deviceSerial = ByteArray(32)))
    }

    /** The descriptor's flags byte (offset 6), read back out of the first chunk written to the hub (after the 4-byte length prefix). */
    private fun flagsWritten(central: RecordingCentral): Int {
        val first = central.chunks.first()
        return first[life.neurone.core.protocol.BleTransfer.PROTOCOL_LEN_PREFIX + 6].toInt() and 0xFF
    }

    // @verifies REQ-SW03-03 DI-SAFE-09 REQ-CVNS-09#f
    @Test fun mode_3_programming_goes_through_the_same_cervical_gate_and_writes_nothing_while_a_cutoff_is_unread() = runTest {
        val central = RecordingCentral()
        val (manager, uploader) = connectedManager(central, TestScope(testScheduler))
        manager.applyCervicalFaultStatus(unreadCutoff)
        assertEquals(ProtocolUploader.Result.CervicalRestartBlocked, uploader.upload(cervical(), deviceSerial = ByteArray(32), autonomous = true))
        assertEquals(emptyList(), central.writes, "Mode 3 must not get round an unread cardiac cutoff")

        // Another person's cutoff: the same one-shot confirmation question in Mode 3.
        val other = RecordingCentral()
        val (manager2, uploader2) = connectedManager(other, TestScope(testScheduler))
        manager2.applyCervicalFaultStatus(anotherPersonsCutoff)
        assertEquals(ProtocolUploader.Result.DifferentPersonConfirmationRequired, uploader2.upload(cervical(), deviceSerial = ByteArray(32), autonomous = true))
        assertEquals(emptyList(), other.writes)
        uploader2.confirmDifferentPerson()
        val confirmed = uploader2.upload(cervical(), deviceSerial = ByteArray(32), autonomous = true)
        assertTrue(confirmed !is ProtocolUploader.Result.DifferentPersonConfirmationRequired && confirmed !is ProtocolUploader.Result.CervicalRestartBlocked)
    }

    // @verifies REQ-SW03-03
    @Test fun a_mode_3_upload_sets_the_autonomous_flag_in_what_reaches_the_hub_and_a_mode_2_upload_does_not() = runTest {
        val mode2 = RecordingCentral()
        assertIs<ProtocolUploader.Result.Success>(connected(mode2, TestScope(testScheduler)).upload(tdcs(1.0), deviceSerial = ByteArray(32)))
        assertEquals(0, flagsWritten(mode2) and 0b10, "Mode 2 leaves the autonomous bit clear")

        val mode3 = RecordingCentral()
        assertIs<ProtocolUploader.Result.Success>(connected(mode3, TestScope(testScheduler)).upload(tdcs(1.0), deviceSerial = ByteArray(32), autonomous = true))
        assertEquals(0b10, flagsWritten(mode3) and 0b10, "Mode 3 sets the autonomous bit")
    }
}
