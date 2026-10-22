package life.neurone.shared

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.runTest
import life.neurone.core.common.UUID
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
        override fun setListener(listener: BleCentralListener) {}
        override fun startScan(serviceUuid: UUID) {}
        override fun stopScan() {}
        override fun connect(deviceId: String) {}
        override fun disconnect() {}
        override fun discoverCharacteristics(serviceUuid: UUID, uuids: List<UUID>) {}
        override fun enableNotifications(uuid: UUID) {}
        override fun read(uuid: UUID) {}
        override fun write(uuid: UUID, value: ByteArray) { writes += uuid }
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

    // @verifies SWR-SS-03
    @Test fun the_same_protocol_inside_the_limit_is_written() = runTest {
        val central = RecordingCentral()
        val result = connected(central, TestScope(testScheduler)).upload(tdcs(1.0), deviceSerial = ByteArray(32))

        assertIs<ProtocolUploader.Result.Success>(result)
        assertTrue(central.writes.isNotEmpty(), "the chunks were written")
    }
}
