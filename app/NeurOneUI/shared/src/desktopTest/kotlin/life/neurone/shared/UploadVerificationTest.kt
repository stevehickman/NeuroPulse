package life.neurone.shared

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.runTest
import life.neurone.core.common.UUID
import life.neurone.core.models.UploadCheck
import life.neurone.core.protocol.GattUuidStrings
import life.neurone.core.session.SignatureResult
import life.neurone.shared.ble.AdapterState
import life.neurone.shared.ble.BleCentral
import life.neurone.shared.ble.BleCentralListener
import life.neurone.shared.ble.NeurOneGattManager
import life.neurone.shared.session.ProtocolUploader
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

/** After an upload the app asks the helmet what it received and never sends the protocol again (SWR-UP-01, SWR-UP-03). */
@OptIn(ExperimentalCoroutinesApi::class)
class UploadVerificationTest {
    private val checkId = UUID.fromString(GattUuidStrings.UPLOAD_CHECK_ID)

    /** A helmet whose reports are scripted: each READ of UPLOAD_CHECK delivers the next one. */
    private class ScriptedHub(private val reports: List<ByteArray?>) : BleCentral {
        override val adapterState = AdapterState.ON
        lateinit var manager: NeurOneGattManager
        var reads = 0
        var writes = 0
        override fun setListener(listener: BleCentralListener) {}
        override fun startScan(serviceUuid: UUID) {}
        override fun stopScan() {}
        override fun connect(deviceId: String) {}
        override fun disconnect() {}
        override fun discoverCharacteristics(serviceUuid: UUID, uuids: List<UUID>) {}
        override fun enableNotifications(uuid: UUID) {}
        override fun read(uuid: UUID) {
            val report = reports.getOrNull(minOf(reads, reports.size - 1))
            reads++
            if (report != null) manager.onCharacteristicRead(uuid, report)
        }
        override fun write(uuid: UUID, value: ByteArray) { writes++ }
    }

    private fun frame(status: Int, reason: Int, hash: String): ByteArray =
        byteArrayOf(status.toByte(), reason.toByte()) + ByteArray(32) { hash.substring(it * 2, it * 2 + 2).toInt(16).toByte() }

    private fun TestScope.uploaderFor(hub: ScriptedHub): ProtocolUploader {
        val manager = NeurOneGattManager(hub, this)
        hub.manager = manager
        // The characteristic is present on this hub, as it is on one whose firmware publishes it.
        manager.onCharacteristicsDiscovered(setOf(checkId))
        hub.reads = 0
        return ProtocolUploader(manager) { SignatureResult(ByteArray(64), "") }
    }

    private val sent = UploadCheck.hashOf("the exact bytes sent".encodeToByteArray())
    private val other = UploadCheck.hashOf("something else".encodeToByteArray())

    // @verifies SWR-UP-01
    @Test fun a_report_with_the_sent_hash_and_accepted_is_confirmed() = runTest {
        val hub = ScriptedHub(listOf(frame(1, 0, sent)))
        val verdict = uploaderFor(hub).verify(sent)
        assertEquals("accepted", verdict.helmetStatus)
        assertEquals(sent, verdict.helmetHash)
    }

    // @verifies SWR-UP-03
    @Test fun a_pending_report_is_read_again_and_the_protocol_is_never_sent_again() = runTest {
        val hub = ScriptedHub(listOf(frame(3, 0, sent), frame(3, 0, sent), frame(1, 0, sent)))
        val verdict = uploaderFor(hub).verify(sent)
        assertEquals("accepted", verdict.helmetStatus)
        assertEquals(3, hub.reads, "read until the verdict")
        assertEquals(0, hub.writes, "an unknown outcome is resolved by reading, never by a second write")
    }

    // @verifies SWR-UP-01
    @Test fun a_truncated_upload_leaves_the_previous_report_and_its_hash_does_not_match() = runTest {
        val hub = ScriptedHub(listOf(frame(1, 0, other)))
        val verdict = uploaderFor(hub).verify(sent, timeoutMs = 2_000, pollMs = 500)
        assertEquals(other, verdict.helmetHash, "the app passes the helmet's hash on; the library finds it differs from the sent one")
        assertEquals(0, hub.writes)
    }

    // @verifies SWR-UP-03
    @Test fun a_helmet_that_never_answers_is_unknown_not_resent() = runTest {
        val hub = ScriptedHub(listOf(null))
        val verdict = uploaderFor(hub).verify(sent, timeoutMs = 2_000, pollMs = 500)
        assertNull(verdict.helmetStatus)
        assertNull(verdict.helmetHash)
        assertEquals(0, hub.writes)
    }

    // @verifies SWR-UP-02
    @Test fun a_refusal_is_passed_on_with_its_kind() = runTest {
        val hub = ScriptedHub(listOf(frame(2, 20, sent)))
        val verdict = uploaderFor(hub).verify(sent)
        assertEquals("refused", verdict.helmetStatus)
        assertEquals("tier", verdict.helmetRefusal)
    }
}
