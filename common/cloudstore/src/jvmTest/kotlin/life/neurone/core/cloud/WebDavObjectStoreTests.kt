package life.neurone.core.cloud

import kotlinx.coroutines.runBlocking
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
 * A stand-in for a WebDAV server, written from RFC 4918, 7231, 7232 and 4331 (`docs/np_sw_cloudstore_001.md` §4c.12): a tree of collections and
 * files under `/dav/`; PUT answers 201 for a new file, 204 for a replaced one and 409 when the parent collection is missing; MKCOL answers 201,
 * 405 on a mapped URL and 409 for a missing parent; PROPFIND requires a Depth header and answers 207 with the resources, each with a
 * `resourcetype`; `If-None-Match: *` makes a PUT answer 412 when the file exists (unless [honoursConditions] is false, which models a server that
 * ignores it). Basic credentials are checked. **It is not a server**; it serialises racing requests and was not checked against any real one.
 */
internal class FakeWebDav(
    private val prefix: String = "D:",
    private val declareNamespaceOnRoot: Boolean = true,
    var honoursConditions: Boolean = true,
) : HttpTransport {
    val files = LinkedHashMap<String, ByteArray>() // path under /dav/ -> bytes
    val collections = mutableSetOf("")             // "" is /dav/
    val seen = mutableListOf<HttpRequest>()
    var offline = false
    var failStatus: Int? = null
    var quota: Long? = 5000L
    var expectedAuth = "Basic dXNlcjpwYXNz"
    var hrefAsFullUri = false

    private fun r(code: Int, body: String = "", headers: Map<String, String> = emptyMap()) = HttpResponse(code, headers, body.encodeToByteArray())
    private fun dec(s: String) = URLDecoder.decode(s.replace("+", "%2B"), "UTF-8")
    private fun parent(path: String) = path.trimEnd('/').substringBeforeLast('/', "")
    private fun enc(s: String) = java.net.URLEncoder.encode(s, "UTF-8").replace("+", "%20")
    private fun href(path: String, isDir: Boolean): String {
        val p = "/dav/" + path.split('/').filter { it.isNotEmpty() }.joinToString("/") { enc(it) } + if (isDir && path.isNotEmpty()) "/" else ""
        return if (hrefAsFullUri) "https://dav.test$p" else p
    }
    private fun el(tag: String, inner: String) = "<$prefix$tag>$inner</$prefix$tag>"
    private fun empty(tag: String) = "<$prefix$tag/>"
    private fun resp(path: String, isDir: Boolean, extra: String = "") =
        el("response", el("href", href(path, isDir).replace("&", "&amp;")) + el("propstat", el("prop", el("resourcetype", if (isDir) empty("collection") else "") + extra) + el("status", "HTTP/1.1 200 OK")))
    private fun multistatus(inner: String): HttpResponse {
        val ns = if (declareNamespaceOnRoot) " xmlns${if (prefix.isEmpty()) "" else ":" + prefix.trimEnd(':')}=\"DAV:\"" else ""
        return r(207, "<?xml version=\"1.0\" encoding=\"utf-8\"?><${prefix}multistatus$ns>$inner</${prefix}multistatus>", mapOf("Content-Type" to "application/xml"))
    }

    override suspend fun post(request: HttpRequest): HttpResponse? = synchronized(this) {
        if (offline) return null
        seen += request
        if (request.headers["Authorization"] != expectedAuth) return r(401)
        failStatus?.let { return r(it) }
        val uri = URI.create(request.url)
        if (!uri.rawPath.startsWith("/dav/")) return r(404)
        val path = dec(uri.rawPath.removePrefix("/dav/"))
        val key = path.trimEnd('/')
        when (request.method) {
            "GET" -> files[key]?.let { r(200).let { _ -> HttpResponse(200, emptyMap(), it.copyOf()) } } ?: r(404)
            "PUT" -> {
                if (parent(key) !in collections) return r(409)
                val existed = files.containsKey(key)
                if (honoursConditions && request.headers["If-None-Match"] == "*" && existed) return r(412)
                files[key] = request.body.copyOf()
                r(if (existed) 204 else 201)
            }
            "MKCOL" -> when {
                key in collections || files.containsKey(key) -> r(405)
                parent(key) !in collections -> r(409)
                else -> { collections += key; r(201) }
            }
            "DELETE" -> if (files.remove(key) != null) r(204) else r(404)
            "PROPFIND" -> {
                val depth = request.headers["Depth"] ?: return r(400)
                val body = request.body.decodeToString()
                if (key !in collections && !files.containsKey(key)) return r(404)
                if (!request.headers["Content-Type"].orEmpty().startsWith("application/xml")) return r(415)
                if ("quota-available-bytes" in body) {
                    val q = quota?.let { el("quota-available-bytes", it.toString()) } ?: ""
                    return multistatus(resp(key, true, q))
                }
                val kids = files.keys.filter { parent(it) == key }.sorted().map { resp(it, false, el("getcontentlength", files[it]!!.size.toString())) } +
                    collections.filter { it.isNotEmpty() && parent(it) == key }.sorted().map { resp(it, true) }
                multistatus(resp(key, true) + if (depth == "1") kids.joinToString("") else "")
            }
            else -> r(405)
        }
    }
}

private fun login(auth: String = "Basic dXNlcjpwYXNz") = WebDavLoginProvider { WebDavLogin("https://dav.test/dav/", auth) }
private fun wd(fake: FakeWebDav) = WebDavObjectStore(fake, login(), timeoutSeconds = 30)

class WebDavObjectStoreContractTests : ObjectStoreContract() {
    override fun newStore(): ObjectStore = wd(FakeWebDav())
}

class WebDavObjectStoreTests {
    // @verifies SWR-MG-11#calls
    @Test
    fun aFirstWriteMakesTheFolderOnceAndLaterWritesDoNot() = runBlocking {
        val fake = FakeWebDav()
        val s = wd(fake)
        s.put("np-a", byteArrayOf(1))
        s.put("np-b", byteArrayOf(2))
        assertEquals(listOf("PUT", "MKCOL", "PUT", "PUT"), fake.seen.map { it.method })
        assertEquals("https://dav.test/dav/neurone-backup/", fake.seen[1].url)
        assertContentEquals(byteArrayOf(2), s.get("np-b"))
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aMissingCollectionTheStoreDoesNotMakeIsAnErrorNotAnEndlessRetry() = runBlocking {
        val fake = FakeWebDav()
        val s = WebDavObjectStore(fake, { WebDavLogin("https://dav.test/dav/missing/", "Basic dXNlcjpwYXNz") }, 30)
        assertFailsWith<CloudStoreException> { s.put("x", byteArrayOf(1)) }
        assertEquals(listOf("PUT", "MKCOL"), fake.seen.map { it.method }, "one retry at most, and the MKCOL's 409 ends it")
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aCreateIsAPutWithIfNoneMatchAndAnExistingFileIsA412() = runBlocking {
        val fake = FakeWebDav()
        val s = wd(fake)
        assertEquals(true, s.createIfAbsent("np-claim", byteArrayOf(1)))
        assertEquals(false, s.createIfAbsent("np-claim", byteArrayOf(2)))
        assertContentEquals(byteArrayOf(1), s.get("np-claim"), "the refused write left the first writer's bytes")
        assertTrue(fake.seen.filter { it.method == "PUT" }.all { it.headers["If-None-Match"] == "*" })
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aServerThatReplacesDespiteIfNoneMatchIsReportedNotTrusted() = runBlocking {
        val fake = FakeWebDav(honoursConditions = false)
        val s = wd(fake)
        assertEquals(true, s.createIfAbsent("np-claim", byteArrayOf(1)))
        val e = assertFailsWith<CloudStoreException> { s.createIfAbsent("np-claim", byteArrayOf(2)) }
        assertEquals(204, e.status)
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun listUsesDepthOneSkipsCollectionsAndReadsAnyNamespacePrefix() = runBlocking {
        for ((prefix, onRoot) in listOf("D:" to true, "d:" to true, "" to true, "ns0:" to true)) {
            val fake = FakeWebDav(prefix = prefix, declareNamespaceOnRoot = onRoot)
            val s = wd(fake)
            listOf("np-1", "np-2", "other").forEach { s.put(it, byteArrayOf(1)) }
            fake.collections += "neurone-backup/sub"
            assertEquals(listOf("np-1", "np-2"), s.list("np-").sorted(), "prefix '$prefix'")
            val propfind = fake.seen.last { it.method == "PROPFIND" }
            assertEquals("1", propfind.headers["Depth"])
            assertTrue(propfind.headers["Content-Type"]!!.startsWith("application/xml"))
        }
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun aFolderNotMadeYetListsAsEmptyAndHrefsMayBeFullUris() = runBlocking {
        val fake = FakeWebDav()
        val s = wd(fake)
        assertEquals(emptyList(), s.list(""))
        fake.hrefAsFullUri = true
        s.put("np-1", byteArrayOf(1))
        assertEquals(listOf("np-1"), s.list(""))
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun namesTheStoreEncodesRoundTripThroughTheUrlAndTheListing() = runBlocking {
        val fake = FakeWebDav()
        val s = wd(fake)
        s.put("dir/a:b?%&é", byteArrayOf(7))
        assertEquals(setOf("neurone-backup/dir%2Fa%3Ab%3F%25&é"), fake.files.keys, "stored under the %XX form")
        assertContentEquals(byteArrayOf(7), s.get("dir/a:b?%&é"))
        assertEquals(listOf("dir/a:b?%&é"), s.list("dir/"))
        s.delete("dir/a:b?%&é")
        s.delete("dir/a:b?%&é") // a missing file is not an error
        assertNull(s.get("dir/a:b?%&é"))
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun freeSpaceIsTheQuotaAvailableBytesOfTheCollectionOrNull() = runBlocking {
        val fake = FakeWebDav()
        assertEquals(5000L, wd(fake).freeSpace())
        assertEquals("0", fake.seen.last().headers["Depth"])
        fake.quota = null
        assertNull(wd(fake).freeSpace())
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun theLoginIsReadForEveryCallAndAWrongOneIsA401() = runBlocking {
        val fake = FakeWebDav()
        var auth = "Basic dXNlcjpwYXNz"
        val s = WebDavObjectStore(fake, { WebDavLogin("https://dav.test/dav/", auth) }, 30)
        s.put("x", byteArrayOf(1))
        auth = "Basic d3Jvbmc="
        assertEquals(401, assertFailsWith<CloudStoreException> { s.get("x") }.status)
        auth = "Basic dXNlcjpwYXNz"
        assertContentEquals(byteArrayOf(1), s.get("x"))
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun noAnswerAndErrorsThrowInsteadOfReadingAsAbsentOrRefused() = runBlocking {
        val fake = FakeWebDav()
        val s = wd(fake)
        s.put("x", byteArrayOf(1))
        fake.failStatus = 500
        for (call in listOf<suspend () -> Unit>({ s.get("x") }, { s.put("x", byteArrayOf(1)) }, { s.createIfAbsent("x", byteArrayOf(1)) }, { s.delete("x") }, { s.list("") }, { s.freeSpace() })) {
            assertEquals(500, assertFailsWith<CloudStoreException> { call() }.status)
        }
        fake.failStatus = null
        fake.offline = true
        assertFailsWith<CloudStoreException> { s.get("x") }
        Unit
    }

    // @verifies SWR-MG-11#calls
    @Test
    fun theFolderNameIsAPlainName() {
        assertFailsWith<IllegalArgumentException> { WebDavObjectStore(FakeWebDav(), login(), 30, folder = "a/b") }
        assertFailsWith<IllegalArgumentException> { WebDavObjectStore(FakeWebDav(), login(), 30, folder = "") }
    }
}
