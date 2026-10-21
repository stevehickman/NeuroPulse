package life.neurone.core.ota

import kotlinx.coroutines.test.runTest
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The firmware source (DI-PERF-21): what it asks for, what it accepts, and that it asks nothing about the person or the device. */
class FirmwareUpdateServiceTests {

    private class Fake(var status: Int? = 200, var body: ByteArray = ByteArray(0)) : HttpTransport {
        val requests = mutableListOf<HttpRequest>()
        override suspend fun post(request: HttpRequest): HttpResponse? {
            requests += request
            return status?.let { HttpResponse(it, emptyMap(), body) }
        }
    }

    private val manifestJson = """
        {"version":"1.5.0","releaseDate":"2026-06-09","imageSizeBytes":4194304,"sha256Hex":"ab",
         "ed25519KeyFingerprint":"4e455550deadbeef","isSafetyMCUFirmware":false,"releaseNotes":"Stability improvements.","extra":1}
    """.trimIndent()

    // @verifies DI-PERF-21
    @Test fun the_manifest_decodes_and_unknown_fields_are_ignored() = runTest {
        val m = FirmwareUpdateService(Fake(body = manifestJson.encodeToByteArray())).fetchManifest()
        assertEquals("1.5.0", m.version)
        assertEquals(4194304, m.imageSizeBytes)
        assertEquals(false, m.isSafetyMCUFirmware)
        assertEquals("Stability improvements.", m.releaseNotes)
        assertEquals("2026-06-09", m.toFirmwareImage()?.buildDate)
    }

    // @verifies DI-PERF-21
    @Test fun a_manifest_with_a_bad_version_or_date_yields_no_image() {
        val ok = FirmwareManifest("1.5.0", "2026-06-09", 1, "", "k", false, "")
        assertNotNull(ok.toFirmwareImage())
        assertNull(ok.copy(version = "1.5").toFirmwareImage())
        assertNull(ok.copy(releaseDate = "June 9").toFirmwareImage())
    }

    // @verifies DI-PERF-21
    @Test fun the_manifest_request_is_a_get_that_names_no_device_user_or_version() = runTest {
        val t = Fake(body = manifestJson.encodeToByteArray())
        FirmwareUpdateService(t).fetchManifest()
        val r = t.requests.single()
        assertEquals("GET", r.method)
        assertEquals(FirmwareConfig.MANIFEST_URL, r.url)
        assertEquals(mapOf("User-Agent" to FirmwareConfig.USER_AGENT), r.headers)
        assertEquals(0, r.body.size)
    }

    // @verifies DI-PERF-21
    @Test fun a_manifest_that_is_refused_malformed_or_unanswered_is_an_error() = runTest {
        assertFailsWith<FirmwareUpdateException> { FirmwareUpdateService(Fake(status = 404)).fetchManifest() }
        assertFailsWith<FirmwareUpdateException> { FirmwareUpdateService(Fake(status = null)).fetchManifest() }
        assertFailsWith<FirmwareUpdateException> { FirmwareUpdateService(Fake(body = "{}".encodeToByteArray())).fetchManifest() }
    }

    // @verifies DI-PERF-21
    @Test fun the_image_url_is_the_static_path_for_a_validated_version_and_has_a_long_timeout() = runTest {
        val t = Fake(body = ByteArray(10))
        FirmwareUpdateService(t).downloadFirmware("1.5.0", 10)
        val r = t.requests.single()
        assertEquals("https://firmware.neurone.ai/firmware/main-1.5.0.npfw", r.url)
        assertEquals(FirmwareConfig.IMAGE_TIMEOUT_SECONDS, r.timeoutSeconds)
        assertTrue(r.timeoutSeconds > FirmwareConfig.MANIFEST_TIMEOUT_SECONDS)
    }

    // @verifies DI-PERF-21
    @Test fun a_version_that_is_not_a_version_never_reaches_a_url() = runTest {
        val t = Fake()
        assertFailsWith<FirmwareUpdateException> { FirmwareUpdateService(t).downloadFirmware("../../evil", 1) }
        assertTrue(t.requests.isEmpty())
    }

    // @verifies DI-PERF-21
    @Test fun an_image_of_the_wrong_size_or_status_is_refused() = runTest {
        assertFailsWith<FirmwareUpdateException> { FirmwareUpdateService(Fake(body = ByteArray(9))).downloadFirmware("1.5.0", 10) }
        assertFailsWith<FirmwareUpdateException> { FirmwareUpdateService(Fake(status = 500, body = ByteArray(10))).downloadFirmware("1.5.0", 10) }
        assertFailsWith<FirmwareUpdateException> { FirmwareUpdateService(Fake(status = null)).downloadFirmware("1.5.0", 10) }
    }
}
