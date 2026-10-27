package life.neurone.core.cloud

import kotlinx.coroutines.runBlocking
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import life.neurone.core.migration.ObjectStore
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import java.net.URI
import java.net.URLDecoder
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * A stand-in for CloudKit Web Services, written from Apple's Documentation Archive pages (`docs/np_sw_cloudstore_001.md` §3.1):
 * `assets/upload` answers an upload URL, a POST to it answers `{"singleFile": {...}}`, `records/modify` has `create` (fails with
 * `EXISTS` for a name in use), `forceReplace` and `forceDelete`, `records/lookup` answers an asset's `downloadURL` or `NOT_FOUND`, and
 * `records/query` filters on `recordName BEGINS_WITH` and pages with a `continuationMarker`. **It is not CloudKit.** It serialises racing
 * creates; it answers a create that finds the name in use either on the entry (HTTP 200) or on the request (HTTP 409), because the
 * pages do not say which.
 */
internal class FakeCloudKit(private val pageSize: Int = 2, private val existsOnRequest: Boolean = false) : HttpTransport {
    val records = LinkedHashMap<String, ByteArray>()
    val seen = mutableListOf<HttpRequest>()
    var offline = false
    var failStatus: Int? = null
    var rotateTo: String? = null
    private val uploads = HashMap<String, ByteArray>() // receipt -> bytes
    private var next = 0

    private fun j(code: Int, body: String) = HttpResponse(code, emptyMap(), body.encodeToByteArray())
    private fun q(s: String) = kotlinx.serialization.json.JsonPrimitive(s).toString()
    private fun dec(s: String) = URLDecoder.decode(s.replace("+", "%2B"), "UTF-8")
    private fun withToken(body: String) = rotateTo?.let { body.trimEnd('}') + ",\"ckWebAuthToken\":${q(it)}}" } ?: body

    override suspend fun post(request: HttpRequest): HttpResponse? = synchronized(this) {
        if (offline) return null
        seen += request
        val uri = URI.create(request.url)
        if (uri.host == "up.test") {
            val receipt = "rc${next++}"
            uploads[receipt] = request.body.copyOf()
            return j(200, """{"singleFile":{"wrappingKey":"k","fileChecksum":"c","receipt":"$receipt","referenceChecksum":"r","size":${request.body.size}}}""")
        }
        val query = (uri.rawQuery ?: "").split('&').filter { it.isNotEmpty() }.associate { val kv = it.split('=', limit = 2); dec(kv[0]) to dec(if (kv.size > 1) kv[1] else "") }
        if (query["ckAPIToken"] != "api" || query["ckWebAuthToken"] != "web") {
            return j(421, """{"serverErrorCode":"AUTHENTICATION_REQUIRED","reason":"request needs authorization","redirectURL":"https://appleid.test"}""")
        }
        failStatus?.let { return j(it, """{"serverErrorCode":"INTERNAL_ERROR"}""") }
        val path = uri.rawPath.removePrefix("/database/1/iCloud.test.app/production/private/")
        val body = Json.parseToJsonElement(request.body.decodeToString()).jsonObject
        return when (path) {
            "assets/upload" -> {
                val t = body["tokens"]!!.jsonArray.map { it.jsonObject }
                j(200, withToken("""{"tokens":[${t.joinToString(",") { """{"recordName":${q(it["recordName"]!!.jsonPrimitive.content)},"fieldName":"data","url":"https://up.test/u/${next}"}""" }}]}"""))
            }
            "records/modify" -> {
                val op = body["operations"]!!.jsonArray.single().jsonObject
                val type = op["operationType"]!!.jsonPrimitive.content
                val rec = op["record"]!!.jsonObject
                val name = rec["recordName"]!!.jsonPrimitive.content
                fun ok() = j(200, withToken("""{"records":[{"recordName":${q(name)},"recordChangeTag":"1"}]}"""))
                fun bytesOf(): ByteArray? {
                    if (rec["recordType"]?.jsonPrimitive?.content != "NpObject") return null
                    val receipt = rec["fields"]!!.jsonObject["data"]!!.jsonObject["value"]!!.jsonObject["receipt"]!!.jsonPrimitive.content
                    return uploads[receipt]
                }
                when (type) {
                    "create" -> {
                        val b = bytesOf() ?: return j(400, """{"serverErrorCode":"BAD_REQUEST"}""")
                        if (records.containsKey(name)) {
                            if (existsOnRequest) j(409, """{"serverErrorCode":"EXISTS","reason":"record exists"}""")
                            else j(200, """{"records":[{"recordName":${q(name)},"reason":"record to insert already exists","serverErrorCode":"EXISTS"}]}""")
                        } else { records[name] = b; ok() }
                    }
                    "forceReplace" -> { records[name] = bytesOf() ?: return j(400, """{"serverErrorCode":"BAD_REQUEST"}"""); ok() }
                    "forceDelete" -> if (records.remove(name) != null) ok() else j(200, """{"records":[{"recordName":${q(name)},"serverErrorCode":"NOT_FOUND"}]}""")
                    else -> j(400, """{"serverErrorCode":"BAD_REQUEST"}""")
                }
            }
            "records/lookup" -> {
                val name = body["records"]!!.jsonArray.single().jsonObject["recordName"]!!.jsonPrimitive.content
                val b = records[name] ?: return j(200, """{"records":[{"recordName":${q(name)},"serverErrorCode":"NOT_FOUND","reason":"Record not found"}]}""")
                val receipt = "dl${next++}"
                uploads[receipt] = b
                j(200, """{"records":[{"recordName":${q(name)},"fields":{"data":{"value":{"downloadURL":"https://dl.test/${'$'}{f}?o=$receipt"},"type":"ASSETID"}}}]}""")
            }
            "records/query" -> {
                val filter = body["query"]!!.jsonObject["filterBy"]?.jsonArray?.single()?.jsonObject
                if (filter != null && (filter["comparator"]!!.jsonPrimitive.content != "BEGINS_WITH" || filter["systemFieldName"]!!.jsonPrimitive.content != "recordName")) return j(400, """{"serverErrorCode":"BAD_REQUEST"}""")
                val prefix = filter?.get("fieldValue")?.jsonObject?.get("value")?.jsonPrimitive?.content ?: ""
                val all = records.keys.filter { it.startsWith(prefix) }.sorted()
                val from = body["continuationMarker"]?.jsonPrimitive?.content?.toInt() ?: 0
                val slice = all.drop(from).take(pageSize)
                val more = from + pageSize < all.size
                j(200, """{"records":[${slice.joinToString(",") { """{"recordName":${q(it)},"recordType":"NpObject"}""" }}]${if (more) ",\"continuationMarker\":\"${from + pageSize}\"" else ""}}""")
            }
            else -> j(404, """{"serverErrorCode":"NOT_FOUND"}""")
        }
    }

    /** The download: the fake keeps it behind the same transport; a real one is a pre-authenticated URL. */
    fun download(request: HttpRequest): HttpResponse {
        val receipt = request.url.substringAfter("o=")
        return uploads[receipt]?.let { HttpResponse(200, emptyMap(), it.copyOf()) } ?: HttpResponse(404, emptyMap(), ByteArray(0))
    }
}

/** Routes the download host to the fake's [FakeCloudKit.download], everything else to its API. */
private class Routed(val fake: FakeCloudKit) : HttpTransport {
    override suspend fun post(request: HttpRequest): HttpResponse? =
        if (URI.create(request.url.replace("\${f}", "data")).host == "dl.test") { fake.seen += request; fake.download(request) } else fake.post(request)
}

private fun ck(fake: FakeCloudKit, onToken: suspend (String) -> Unit = {}) =
    CloudKitObjectStore(Routed(fake), "iCloud.test.app", CloudKitCredentials { "api" to "web" }, timeoutSeconds = 30, onWebAuthToken = onToken)

class CloudKitObjectStoreContractTests : ObjectStoreContract() {
    override fun newStore(): ObjectStore = ck(FakeCloudKit())
}

class CloudKitRequestLevelExistsContractTests : ObjectStoreContract() {
    override fun newStore(): ObjectStore = ck(FakeCloudKit(existsOnRequest = true))
}

class CloudKitObjectStoreTests {
    private fun bodies(fake: FakeCloudKit, sub: String) =
        fake.seen.filter { it.url.contains("/$sub?") }.map { Json.parseToJsonElement(it.body.decodeToString()).jsonObject }

    // @verifies SWR-MG-11#calls
    @Test
    fun aWriteIsAnAssetUploadThenAModifyThatCarriesTheReceipt() = runBlocking {
        val fake = FakeCloudKit()
        ck(fake).createIfAbsent("np-migration-claim.json", byteArrayOf(7, 8))
        assertEquals(
            listOf("assets/upload", "up.test", "records/modify"),
            fake.seen.map { r -> URI.create(r.url).let { if (it.host == "up.test") "up.test" else it.rawPath.substringAfter("/private/") } },
        )
        val first = fake.seen.first().url
        assertTrue(first.startsWith("https://api.apple-cloudkit.com/database/1/iCloud.test.app/production/private/assets/upload?ckAPIToken=api&ckWebAuthToken=web"))
        val op = bodies(fake, "records/modify").single()["operations"]!!.jsonArray.single().jsonObject
        assertEquals("create", op["operationType"]!!.jsonPrimitive.content)
        assertEquals("np-migration-claim.json", op["record"]!!.jsonObject["recordName"]!!.jsonPrimitive.content)
        assertEquals("NpObject", op["record"]!!.jsonObject["recordType"]!!.jsonPrimitive.content)
        assertContentEquals(byteArrayOf(7, 8), fake.records["np-migration-claim.json"])
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aPutIsAForceReplaceAndADeleteAForceDelete() = runBlocking {
        val fake = FakeCloudKit()
        val s = ck(fake)
        s.put("x", byteArrayOf(1)); s.put("x", byteArrayOf(2)); s.delete("x"); s.delete("x")
        val ops = bodies(fake, "records/modify").map { it["operations"]!!.jsonArray.single().jsonObject["operationType"]!!.jsonPrimitive.content }
        assertEquals(listOf("forceReplace", "forceReplace", "forceDelete", "forceDelete"), ops)
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun theDownloadIsFetchedWithoutCredentialsAndThePlaceholderIsFilled() = runBlocking {
        val fake = FakeCloudKit()
        val s = ck(fake)
        s.put("x", byteArrayOf(5, 6))
        assertContentEquals(byteArrayOf(5, 6), s.get("x"))
        val dl = fake.seen.last()
        assertEquals("GET", dl.method)
        assertTrue("ckAPIToken" !in dl.url && "\${f}" !in dl.url && dl.headers.keys.none { it.equals("authorization", true) })
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun namesAreEncodedIntoRecordNamesAndALeadingUnderscoreIsEscaped() = runBlocking {
        val fake = FakeCloudKit(pageSize = 100)
        val s = ck(fake)
        s.put("dir/a b+c", byteArrayOf(1)); s.put("_hidden", byteArrayOf(1))
        assertEquals(setOf("dir%2Fa%20b%2Bc", "%5Fhidden"), fake.records.keys)
        assertEquals(listOf("dir/a b+c"), s.list("dir/"))
        assertEquals(listOf("_hidden"), s.list("_"))
        assertFailsWith<CloudStoreException> { s.put("x".repeat(256), byteArrayOf(1)) }
        Unit
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun listFollowsTheContinuationMarker() = runBlocking {
        val fake = FakeCloudKit(pageSize = 1)
        val s = ck(fake)
        listOf("np-1", "np-2", "np-3", "other").forEach { s.put(it, byteArrayOf(1)) }
        assertEquals(listOf("np-1", "np-2", "np-3"), s.list("np-"))
        assertEquals(4, s.list("").size)
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun anObjectTooBigForOneAssetOrEmptyIsRefusedBeforeAnythingIsSent() = runBlocking {
        val fake = FakeCloudKit()
        val s = ck(fake)
        assertFailsWith<CloudStoreException> { s.put("big", ByteArray(CloudKitObjectStore.MAX_BYTES + 1)) }
        assertFailsWith<CloudStoreException> { s.put("empty", ByteArray(0)) }
        assertTrue(fake.seen.isEmpty())
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aTokenInTheReplyIsHandedToTheHostAndSignInIsSurfacedAsA421() = runBlocking {
        val fake = FakeCloudKit(); fake.rotateTo = "next+token="
        val got = mutableListOf<String>()
        ck(fake) { got += it }.put("x", byteArrayOf(1))
        assertTrue(got.isNotEmpty() && got.all { it == "next+token=" })
        val signedOut = CloudKitObjectStore(Routed(FakeCloudKit()), "iCloud.test.app", CloudKitCredentials { "api" to null }, 30)
        assertEquals(421, assertFailsWith<CloudStoreException> { signedOut.get("x") }.status)
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun noAnswerAndErrorsThrowInsteadOfReadingAsAbsentOrRefused() = runBlocking {
        val fake = FakeCloudKit()
        val s = ck(fake)
        fake.offline = true
        assertFailsWith<CloudStoreException> { s.get("x") }
        assertFailsWith<CloudStoreException> { s.createIfAbsent("x", byteArrayOf(1)) }
        fake.offline = false
        fake.failStatus = 503
        assertEquals(503, assertFailsWith<CloudStoreException> { s.get("x") }.status)
        assertFailsWith<CloudStoreException> { s.list("") }
        assertFailsWith<CloudStoreException> { s.delete("x") }
        assertNull(runCatching { fake.records["x"] }.getOrNull())
    }
}
