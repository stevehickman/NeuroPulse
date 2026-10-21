package life.neurone.shared

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.test.advanceUntilIdle
import kotlinx.coroutines.test.runTest
import life.neurone.core.ble.OtaOpcode
import life.neurone.core.models.OtaPhase
import life.neurone.core.models.OtaStatusPacket
import life.neurone.core.ota.FirmwareImage
import life.neurone.core.ota.FirmwareManifest
import life.neurone.core.ota.FirmwareUpdateProviding
import life.neurone.shared.ble.ConnectionState
import life.neurone.shared.ota.OtaError
import life.neurone.shared.ota.OtaException
import life.neurone.shared.ota.OtaLink
import life.neurone.shared.ota.OtaManager
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertIs
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The firmware update's orchestration (DI-PERF-21): the opcode order, the trust gate and what stops a transfer. */
@OptIn(ExperimentalCoroutinesApi::class)
class OtaManagerTest {

    private val trusted = "4e455550deadbeef"

    private class Link(connected: Boolean = true, version: String? = "1.0.0") : OtaLink {
        override val connectionState = MutableStateFlow(if (connected) ConnectionState.CONNECTED else ConnectionState.DISCONNECTED)
        override val otaStatus = MutableStateFlow<OtaStatusPacket?>(null)
        override val hubFirmwareVersion = MutableStateFlow(version)
        val sent = mutableListOf<Pair<OtaOpcode, ByteArray>>()
        /** What the hub does in answer to a command, given how many commands it has had. */
        var hub: (OtaOpcode, Int, Link) -> Unit = { op, _, link ->
            when (op) {
                OtaOpcode.VERIFY -> link.status(OtaPhase.VERIFIED)
                OtaOpcode.COMMIT, OtaOpcode.SAFETY_MCU_COMMIT -> link.status(OtaPhase.COMPLETE, 100)
                else -> Unit
            }
        }

        fun status(phase: OtaPhase, progress: Int = 0, error: Int = 0) {
            otaStatus.value = OtaStatusPacket(phase.rawValue, progress, error)
        }

        override fun sendOtaCommand(opcode: OtaOpcode, payload: ByteArray) {
            sent += opcode to payload
            hub(opcode, sent.size, this)
        }

        val opcodes get() = sent.map { it.first }
    }

    private class Source(
        var manifest: FirmwareManifest? = null,
        var image: ByteArray? = null,
    ) : FirmwareUpdateProviding {
        var downloads = 0
        var fetches = 0
        override suspend fun fetchManifest(): FirmwareManifest {
            fetches++
            return manifest ?: throw life.neurone.core.ota.FirmwareUpdateException("no answer")
        }

        override suspend fun downloadFirmware(version: String, expectedSizeBytes: Int): ByteArray {
            downloads++
            return image ?: throw life.neurone.core.ota.FirmwareUpdateException("no answer")
        }
    }

    // A scope on the test's scheduler that is not "background": advanceUntilIdle runs what it launches. It is never joined.
    private fun kotlinx.coroutines.test.TestScope.scope() =
        kotlinx.coroutines.CoroutineScope(kotlinx.coroutines.test.StandardTestDispatcher(testScheduler) + kotlinx.coroutines.Job())

    private fun image(version: String = "1.1.0", size: Int = 1200, fingerprint: String = trusted, safety: Boolean = false) =
        FirmwareImage(version, "2026-10-01", size, fingerprint, safety, "notes")

    private fun manifest(version: String = "1.1.0", safety: Boolean = false) =
        FirmwareManifest(version, "2026-10-01", 1200, "00", trusted, safety, "notes")

    private fun bytes(n: Int) = ByteArray(n) { (it % 251).toByte() }

    // @verifies DI-PERF-21
    @Test fun an_update_sends_initiate_then_every_chunk_then_verify_then_commit_and_ends_complete() = runTest {
        val link = Link()
        val source = Source(image = bytes(1200))
        val ota = OtaManager(link, source, scope(), setOf(trusted))
        ota.beginUpdate(image())
        assertEquals(
            listOf(OtaOpcode.INITIATE, OtaOpcode.CHUNK, OtaOpcode.CHUNK, OtaOpcode.CHUNK, OtaOpcode.VERIFY, OtaOpcode.COMMIT),
            link.opcodes,
        )
        assertEquals(OtaPhase.COMPLETE, ota.phase.value)
        assertEquals(100, ota.progressPercent.value)
        assertNull(ota.lastError.value)
    }

    // @verifies DI-PERF-21
    @Test fun a_chunk_is_a_little_endian_index_then_496_bytes_of_the_image_and_the_last_is_the_remainder() = runTest {
        val link = Link()
        val data = bytes(1200)
        OtaManager(link, Source(image = data), scope(), setOf(trusted)).beginUpdate(image())
        val chunks = link.sent.filter { it.first == OtaOpcode.CHUNK }.map { it.second }
        assertEquals(listOf(2 + 496, 2 + 496, 2 + 208), chunks.map { it.size })
        assertEquals(listOf(0, 1, 2), chunks.map { (it[0].toInt() and 0xFF) or ((it[1].toInt() and 0xFF) shl 8) })
        assertContentEquals(data, chunks.map { it.copyOfRange(2, it.size) }.reduce { a, b -> a + b })
    }

    // @verifies DI-PERF-21
    @Test fun the_transfer_reports_its_bytes_and_reserves_the_last_fifth_of_the_bar() = runTest {
        val link = Link()
        val ota = OtaManager(link, Source(image = bytes(1200)), scope(), setOf(trusted))
        val seen = mutableListOf<Pair<Long, Int>>()
        link.hub = { op, _, l ->
            if (op == OtaOpcode.CHUNK) seen += (ota.session.value?.sentBytes ?: -1L) to ota.progressPercent.value
            if (op == OtaOpcode.VERIFY) l.status(OtaPhase.VERIFIED)
            if (op == OtaOpcode.COMMIT) l.status(OtaPhase.COMPLETE, 100)
        }
        ota.beginUpdate(image())
        // The count is taken as each chunk goes out, before it is added: 0, 496, 992 bytes; the bar is at most 80% until the transfer ends.
        assertEquals(listOf(0L, 496L, 992L), seen.map { it.first })
        assertTrue(seen.all { it.second <= 80 })
        assertEquals(1200L, ota.session.value?.totalBytes)
    }

    // @verifies DI-PERF-21
    @Test fun an_image_signed_by_a_key_that_is_not_trusted_is_never_downloaded_or_sent() = runTest {
        val link = Link()
        val source = Source(image = bytes(1200))
        val ota = OtaManager(link, source, scope(), setOf(trusted))
        val e = assertFailsWith<OtaException> { ota.beginUpdate(image(fingerprint = "00000000")) }
        assertEquals(OtaError.SignatureInvalid, e.error)
        assertEquals(OtaError.SignatureInvalid, ota.lastError.value)
        assertEquals(0, source.downloads)
        assertTrue(link.sent.isEmpty())
    }

    // @verifies DI-PERF-21
    @Test fun a_fingerprint_is_compared_without_regard_to_case_and_an_empty_trust_set_trusts_nothing() = runTest {
        val upper = image(fingerprint = trusted.uppercase())
        OtaManager(Link(), Source(image = bytes(1200)), scope(), setOf(trusted)).beginUpdate(upper)
        val e = assertFailsWith<OtaException> { OtaManager(Link(), Source(image = bytes(1200)), scope(), emptySet()).beginUpdate(image()) }
        assertEquals(OtaError.SignatureInvalid, e.error)
    }

    // @verifies DI-PERF-21
    @Test fun a_hub_that_is_not_connected_is_reported_before_anything_is_downloaded() = runTest {
        val link = Link(connected = false)
        val source = Source(image = bytes(1200))
        val e = assertFailsWith<OtaException> { OtaManager(link, source, scope(), setOf(trusted)).beginUpdate(image()) }
        assertEquals(OtaError.NotConnected, e.error)
        assertEquals(0, source.downloads)
        assertTrue(link.sent.isEmpty())
    }

    // @verifies DI-PERF-21
    @Test fun a_download_that_fails_sends_nothing_to_the_hub() = runTest {
        val link = Link()
        val e = assertFailsWith<OtaException> { OtaManager(link, Source(image = null), scope(), setOf(trusted)).beginUpdate(image()) }
        assertEquals(OtaError.ImageLoadFailed, e.error)
        assertTrue(link.sent.isEmpty())
    }

    // @verifies DI-PERF-21
    @Test fun a_host_with_no_source_cannot_update() = runTest {
        val e = assertFailsWith<OtaException> { OtaManager(Link(), null, scope(), setOf(trusted)).beginUpdate(image()) }
        assertEquals(OtaError.ImageLoadFailed, e.error)
    }

    // @verifies DI-PERF-21
    @Test fun an_image_never_goes_to_the_wrong_processor() = runTest {
        val link = Link()
        val source = Source(image = bytes(1200))
        val ota = OtaManager(link, source, scope(), setOf(trusted))
        assertEquals(OtaError.WrongImageKind, assertFailsWith<OtaException> { ota.beginUpdate(image(safety = true)) }.error)
        assertEquals(OtaError.WrongImageKind, assertFailsWith<OtaException> { ota.beginSafetyMcuUpdate(image(safety = false)) }.error)
        assertEquals(0, source.downloads)
        assertTrue(link.sent.isEmpty())
    }

    // @verifies DI-PERF-21
    @Test fun the_safety_mcu_update_has_its_own_opcodes_and_no_verify_step() = runTest {
        val link = Link()
        OtaManager(link, Source(image = bytes(1200)), scope(), setOf(trusted)).beginSafetyMcuUpdate(image(safety = true))
        assertEquals(
            listOf(OtaOpcode.SAFETY_MCU_BEGIN, OtaOpcode.SAFETY_MCU_CHUNK, OtaOpcode.SAFETY_MCU_CHUNK, OtaOpcode.SAFETY_MCU_CHUNK, OtaOpcode.SAFETY_MCU_COMMIT),
            link.opcodes,
        )
    }

    // @verifies DI-PERF-21
    @Test fun a_hub_that_refuses_the_image_mid_transfer_is_sent_no_more_of_it() = runTest {
        val link = Link()
        link.hub = { op, n, l -> if (n == 2) l.status(OtaPhase.FAILED, 0, 0x12) }
        val ota = OtaManager(link, Source(image = bytes(1200)), scope(), setOf(trusted))
        val e = assertFailsWith<OtaException> { ota.beginUpdate(image()) }
        assertEquals(OtaError.HubRejected(0x12), e.error)
        assertEquals(listOf(OtaOpcode.INITIATE, OtaOpcode.CHUNK), link.opcodes)
    }

    // @verifies DI-PERF-21
    @Test fun a_link_lost_mid_transfer_stops_it_before_the_commit() = runTest {
        val link = Link()
        link.hub = { op, n, l -> if (n == 2) l.connectionState.value = ConnectionState.DISCONNECTED }
        val e = assertFailsWith<OtaException> {
            OtaManager(link, Source(image = bytes(1200)), scope(), setOf(trusted)).beginUpdate(image())
        }
        assertEquals(OtaError.TransferFailed, e.error)
        assertTrue(OtaOpcode.COMMIT !in link.opcodes && OtaOpcode.VERIFY !in link.opcodes)
    }

    // @verifies DI-PERF-21
    @Test fun a_signature_the_hub_refuses_is_never_committed() = runTest {
        val link = Link()
        link.hub = { op, _, l -> if (op == OtaOpcode.VERIFY) l.status(OtaPhase.FAILED, 0, 0x01) }
        val e = assertFailsWith<OtaException> {
            OtaManager(link, Source(image = bytes(1200)), scope(), setOf(trusted)).beginUpdate(image())
        }
        assertEquals(OtaError.VerificationFailed, e.error)
        assertTrue(OtaOpcode.COMMIT !in link.opcodes)
    }

    // @verifies DI-PERF-21
    @Test fun a_hub_that_never_answers_the_verify_times_out_without_a_commit() = runTest {
        val link = Link()
        link.hub = { _, _, _ -> }
        val e = assertFailsWith<OtaException> {
            OtaManager(link, Source(image = bytes(1200)), scope(), setOf(trusted), verifyTimeoutMs = 1_000).beginUpdate(image())
        }
        assertEquals(OtaError.Timeout, e.error)
        assertTrue(OtaOpcode.COMMIT !in link.opcodes)
    }

    // @verifies DI-PERF-21
    @Test fun a_hub_that_does_not_come_back_after_the_commit_times_out() = runTest {
        val link = Link()
        link.hub = { op, _, l -> if (op == OtaOpcode.VERIFY) l.status(OtaPhase.VERIFIED) }
        val e = assertFailsWith<OtaException> {
            OtaManager(link, Source(image = bytes(1200)), scope(), setOf(trusted), completeTimeoutMs = 1_000).beginUpdate(image())
        }
        assertEquals(OtaError.Timeout, e.error)
        assertEquals(OtaOpcode.COMMIT, link.opcodes.last())
    }

    // @verifies DI-PERF-21
    @Test fun an_abort_before_the_commit_tells_the_hub_and_resets() = runTest {
        val link = Link()
        link.hub = { _, _, _ -> }
        val ota = OtaManager(link, Source(image = bytes(1200)), scope(), setOf(trusted), verifyTimeoutMs = 60_000)
        ota.start(image())
        advanceUntilIdle()
        assertEquals(OtaPhase.VERIFYING, ota.phase.value)
        ota.abort()
        assertEquals(OtaOpcode.ABORT, link.opcodes.last())
        assertEquals(OtaPhase.IDLE, ota.phase.value)
        assertNull(ota.session.value)
        assertEquals(0, ota.progressPercent.value)
    }

    // @verifies DI-PERF-21
    @Test fun an_abort_while_the_hub_is_writing_its_bank_is_ignored() = runTest {
        val link = Link()
        link.hub = { op, _, l -> if (op == OtaOpcode.VERIFY) l.status(OtaPhase.VERIFIED) }
        val ota = OtaManager(link, Source(image = bytes(1200)), scope(), setOf(trusted))
        ota.start(image())
        advanceUntilIdle()
        assertEquals(OtaPhase.APPLYING, ota.phase.value)
        ota.abort()
        assertTrue(OtaOpcode.ABORT !in link.opcodes)
        assertEquals(OtaPhase.APPLYING, ota.phase.value)
    }

    // @verifies DI-PERF-21
    @Test fun a_newer_manifest_version_is_offered_and_the_same_or_older_is_not() = runTest {
        val link = Link(version = "1.0.0")
        val source = Source(manifest = manifest("1.1.0"))
        val ota = OtaManager(link, source, scope(), setOf(trusted))
        ota.refreshManifest()
        assertEquals("1.1.0", ota.availableUpdate.value?.version)
        source.manifest = manifest("1.0.0")
        ota.refreshManifest()
        assertNull(ota.availableUpdate.value)
        assertEquals(false, ota.updatesUnavailable.value)
    }

    // @verifies DI-PERF-21
    @Test fun an_unknown_hub_version_still_shows_what_is_available() = runTest {
        val ota = OtaManager(Link(version = null), Source(manifest = manifest("0.0.1")), scope(), setOf(trusted))
        ota.refreshManifest()
        assertNotNull(ota.availableUpdate.value)
    }

    // @verifies DI-PERF-21
    @Test fun a_manifest_that_cannot_be_fetched_is_unavailable_and_not_up_to_date() = runTest {
        val ota = OtaManager(Link(), Source(manifest = null), scope(), setOf(trusted))
        ota.refreshManifest()
        assertNull(ota.availableUpdate.value)
        assertTrue(ota.updatesUnavailable.value)
        val none = OtaManager(Link(), null, scope(), setOf(trusted))
        none.refreshManifest()
        assertTrue(none.updatesUnavailable.value)
    }

    // @verifies DI-PERF-21
    @Test fun connecting_to_the_hub_looks_for_an_update() = runTest {
        val link = Link(connected = false)
        val source = Source(manifest = manifest("1.1.0"))
        val ota = OtaManager(link, source, scope(), setOf(trusted))
        advanceUntilIdle()
        assertEquals(0, source.fetches)
        link.connectionState.value = ConnectionState.CONNECTED
        advanceUntilIdle()
        assertEquals(1, source.fetches)
        assertEquals("1.1.0", ota.availableUpdate.value?.version)
    }

    // @verifies DI-PERF-21
    @Test fun the_hubs_error_code_is_kept_when_it_reports_one() = runTest {
        val link = Link()
        val ota = OtaManager(link, Source(), scope(), setOf(trusted))
        link.status(OtaPhase.FAILED, 10, 0x34)
        advanceUntilIdle()
        assertIs<OtaError.HubRejected>(ota.lastError.value)
        assertEquals(0x34, (ota.lastError.value as OtaError.HubRejected).code)
        assertEquals(OtaPhase.FAILED, ota.phase.value)
    }
}
