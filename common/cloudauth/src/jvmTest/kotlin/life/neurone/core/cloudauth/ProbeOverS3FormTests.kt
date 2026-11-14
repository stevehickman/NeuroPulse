package life.neurone.core.cloudauth

import kotlinx.coroutines.runBlocking
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import life.neurone.core.cloud.AwsCredentials
import life.neurone.core.cloud.AwsCredentialsProvider
import life.neurone.core.cloud.NppsAwsSigV4
import life.neurone.core.cloud.S3ObjectStore
import life.neurone.core.migration.MigrationFlow
import life.neurone.core.migration.MigrationHost
import life.neurone.core.migration.ObjectStore
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import java.net.URI
import java.net.URLDecoder
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * A stand-in for an S3-compatible store reached at Oracle's compatibility endpoint (`docs/np_sw_cloudstore_001.md` §7.5). It is **not Oracle**: Oracle's
 * pages do not say whether `If-None-Match: *` on a PutObject is honoured, so [honorsCreateCondition] is the unknown, and both values are run. It answers
 * 412 to a conditional create of an existing key when it honours the header and overwrites with 200 when it does not.
 */
private class FakeCompatStore(private val honorsCreateCondition: Boolean) : HttpTransport {
    val objects = LinkedHashMap<String, ByteArray>()
    val seen = mutableListOf<HttpRequest>()

    override suspend fun post(request: HttpRequest): HttpResponse = synchronized(this) {
        seen += request
        fun r(code: Int, body: ByteArray = ByteArray(0)) = HttpResponse(code, emptyMap(), body)
        val uri = URI.create(request.url)
        val query = (uri.rawQuery ?: "").split('&').filter { it.isNotEmpty() }.associate {
            val kv = it.split('=', limit = 2)
            URLDecoder.decode(kv[0], "UTF-8") to URLDecoder.decode(kv.getOrElse(1) { "" }, "UTF-8")
        }
        val key = uri.rawPath.removePrefix("/$BUCKET").removePrefix("/").let { URLDecoder.decode(it, "UTF-8") }
        when (request.method) {
            "GET" -> if (key.isEmpty()) {
                val prefix = query["prefix"] ?: ""
                val body = "<ListBucketResult><IsTruncated>false</IsTruncated>" +
                    objects.keys.filter { it.startsWith(prefix) }.sorted().joinToString("") { "<Contents><Key>$it</Key></Contents>" } + "</ListBucketResult>"
                r(200, body.encodeToByteArray())
            } else objects[key]?.let { r(200, it.copyOf()) } ?: r(404)
            "PUT" -> {
                if (honorsCreateCondition && request.headers["if-none-match"] == "*" && objects.containsKey(key)) return r(412)
                objects[key] = request.body.copyOf()
                r(200)
            }
            "DELETE" -> { objects.remove(key); r(204) }
            else -> r(405)
        }
    }

    companion object { const val BUCKET = "bkt" }
}

/** The store with a number for free space. A bucket has none to read, so this stands in for "the room is known" and isolates the other steps of the check. */
private class WithRoom(private val inner: ObjectStore, private val bytes: Long) : ObjectStore by inner {
    override suspend fun freeSpace(): Long? = bytes
}

/** The probe asks the store for everything, so the host is never asked to perform an effect. */
private val NO_EFFECTS = object : MigrationHost {
    override suspend fun perform(effect: JsonObject): JsonObject = error("the probe asked the host for ${effect["type"]}; an object store answers every effect it makes")
}

private const val ORACLE_HOST = "examplens.compat.objectstorage.us-ashburn-1.oci.customer-oci.com"

private fun s3At(transport: HttpTransport): S3ObjectStore =
    S3ObjectStore(
        transport, FakeCompatStore.BUCKET, "us-ashburn-1", AwsCredentialsProvider { AwsCredentials("AKIAEXAMPLE", "secret") }, NppsAwsSigV4(),
        { NppsAwsSigV4.amzDate(kotlinx.datetime.Clock.System.now()) }, timeoutSeconds = 30, endpointHost = ORACLE_HOST,
    )

private fun JsonObject.text(k: String) = this[k]?.jsonPrimitive?.content

/**
 * The account check (`SWR-MG-12`) run through the real S3 adapter, signed by the Rust core, at an Oracle-style endpoint. The core's own tests run it against an
 * in-memory provider; these show what it says about the form Oracle was routed through on 2026-11-11.
 */
class ProbeOverS3FormTests {
    // @verifies SWR-MG-12
    @Test
    fun anEndpointThatIgnoresIfNoneMatchIsCaughtAsBackupOnlyAndTheProbeLeavesNothingBehind() = runBlocking {
        val fake = FakeCompatStore(honorsCreateCondition = false)
        val done = MigrationFlow.probe(WithRoom(s3At(fake), 1L shl 30), 1_000, NO_EFFECTS)
        assertEquals("backup_only", done.text("result"))
        assertEquals("createIfAbsentSecondRefused", done.text("failedOperation"), "the second create overwrote and said created")
        assertTrue(fake.objects.isEmpty(), "both throwaway objects are gone: ${fake.objects.keys}")
        // The request was the path-style Oracle form, with the condition sent and signed.
        val put = fake.seen.first { it.method == "PUT" && it.headers["if-none-match"] == "*" }
        assertTrue(put.url.startsWith("https://$ORACLE_HOST/${FakeCompatStore.BUCKET}/np-probe-"), put.url)
        assertTrue("if-none-match" in put.headers.entries.first { it.key.equals("authorization", true) }.value)
    }

    // @verifies SWR-MG-12
    @Test
    fun anEndpointThatRefusesTheSecondCreateIsSufficientAndTheProbeLeavesNothingBehind() = runBlocking {
        val fake = FakeCompatStore(honorsCreateCondition = true)
        val done = MigrationFlow.probe(WithRoom(s3At(fake), 1L shl 30), 1_000, NO_EFFECTS)
        assertEquals("sufficient", done.text("result"))
        assertTrue(fake.objects.isEmpty())
        assertEquals(true, done["ops"]!!.jsonObject["createIfAbsentSecondRefused"]!!.jsonPrimitive.content.toBoolean())
    }

    // @verifies SWR-MG-12
    @Test
    fun aBucketStoreThatCannotReadItsFreeSpaceIsNotFailedForItButTheResultSaysTheRoomWasNotConfirmed() = runBlocking {
        // freeSpace() is null for a bucket (no quota the credentials can read). The principal ruled on 2026-11-13 (OI-UI-KMP-11 M12 space, option a)
        // that unknown room does not fail the check; the claim steps are still judged, and the result carries spaceConfirmed = false.
        val honoring = FakeCompatStore(honorsCreateCondition = true)
        val ok = MigrationFlow.probe(s3At(honoring), 1_000, NO_EFFECTS)
        assertEquals("sufficient", ok.text("result"))
        assertEquals("false", ok.text("spaceConfirmed"))
        assertTrue(honoring.objects.isEmpty())
        val ignoring = FakeCompatStore(honorsCreateCondition = false)
        val caught = MigrationFlow.probe(s3At(ignoring), 1_000, NO_EFFECTS)
        assertEquals("backup_only", caught.text("result"))
        assertEquals("createIfAbsentSecondRefused", caught.text("failedOperation"))
        assertTrue(ignoring.objects.isEmpty())
        // A store that does report its room still confirms it.
        assertEquals("true", MigrationFlow.probe(WithRoom(s3At(FakeCompatStore(true)), 1L shl 30), 1_000, NO_EFFECTS).text("spaceConfirmed"))
    }
}
