package life.neurone.core.net

import com.sun.net.httpserver.HttpServer
import kotlinx.coroutines.runBlocking
import java.net.InetSocketAddress
import java.util.concurrent.CopyOnWriteArrayList
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * What every [HttpTransport] must do with the method it is handed, run against a loopback server (SWR-MG-11: the cloud object
 * stores need DELETE and PUT-style calls, and not only the POST and GET the firmware and fleet calls use).
 *
 * The checks are plain functions, not tests, so each platform's transport (OkHttp on Android, the JDK client on desktop, and any
 * later one) calls them from a test of its own in whatever test framework its module uses. Shared here once because the same
 * assertions must hold for all of them.
 */
abstract class HttpTransportChecks {
    protected abstract fun transport(): HttpTransport

    private class Seen(val method: String, val path: String, val headers: Map<String, String>, val body: ByteArray)

    private class Loopback(private val replyStatus: Int, private val replyHeaders: Map<String, String>) {
        val seen = CopyOnWriteArrayList<Seen>()
        private val server = HttpServer.create(InetSocketAddress("127.0.0.1", 0), 0).also { s ->
            s.createContext("/") { ex ->
                val body = ex.requestBody.readBytes()
                seen += Seen(ex.requestMethod, ex.requestURI.rawPath, ex.requestHeaders.entries.associate { it.key.lowercase() to it.value.first() }, body)
                replyHeaders.forEach { (k, v) -> ex.responseHeaders.add(k, v) }
                if (ex.requestMethod == "HEAD") {
                    ex.sendResponseHeaders(replyStatus, -1)
                } else {
                    val out = "reply".encodeToByteArray()
                    ex.sendResponseHeaders(replyStatus, out.size.toLong())
                    ex.responseBody.use { it.write(out) }
                }
                ex.close()
            }
            s.start()
        }
        val port get() = server.address.port
        fun url(path: String) = "http://127.0.0.1:$port$path"
        fun stop() = server.stop(0)
    }

    private fun <T> withServer(status: Int = 200, headers: Map<String, String> = emptyMap(), block: suspend (Loopback) -> T): T {
        val s = Loopback(status, headers)
        try {
            return runBlocking { block(s) }
        } finally {
            s.stop()
        }
    }

    fun deleteIsSentAsDeleteWithNoBody(): Unit = withServer { s ->
        val r = transport().post(HttpRequest(s.url("/o/claim"), emptyMap(), ByteArray(0), method = "DELETE"))!!
        assertEquals(200, r.status)
        val seen = s.seen.single()
        assertEquals("DELETE", seen.method)
        assertEquals("/o/claim", seen.path)
        assertEquals(0, seen.body.size)
    }

    fun putCarriesItsBodyAndContentType(): Unit = withServer { s ->
        val body = ByteArray(3000) { (it * 7).toByte() }
        transport().post(HttpRequest(s.url("/o/bundle"), mapOf("Content-Type" to "application/octet-stream", "Authorization" to "Bearer t"), body, method = "PUT"))!!
        val seen = s.seen.single()
        assertEquals("PUT", seen.method)
        assertContentEquals(body, seen.body)
        assertEquals("application/octet-stream", seen.headers["content-type"]?.substringBefore(';'))
        assertEquals("Bearer t", seen.headers["authorization"])
    }

    fun theDefaultMethodIsStillPostWithItsBody(): Unit = withServer { s ->
        transport().post(HttpRequest(s.url("/up"), emptyMap(), byteArrayOf(1, 2, 3)))!!
        val seen = s.seen.single()
        assertEquals("POST", seen.method)
        assertContentEquals(byteArrayOf(1, 2, 3), seen.body)
    }

    fun getAndHeadCarryNoBody(): Unit = withServer { s ->
        val g = transport().post(HttpRequest(s.url("/o/x?alt=media"), emptyMap(), ByteArray(0), method = "GET"))!!
        assertEquals("reply", g.body.decodeToString())
        val h = transport().post(HttpRequest(s.url("/o/x"), emptyMap(), ByteArray(0), method = "HEAD"))!!
        assertEquals(200, h.status)
        assertEquals(listOf("GET", "HEAD"), s.seen.map { it.method })
        assertTrue(s.seen.all { it.body.isEmpty() })
        assertEquals("/o/x?alt=media".substringBefore('?'), s.seen[0].path)
    }

    fun aRefusalIsAnAnswerWithItsStatusAndHeaders(): Unit = withServer(status = 412, headers = mapOf("X-Reason" to "precondition")) { s ->
        val r = transport().post(HttpRequest(s.url("/up"), emptyMap(), byteArrayOf(1), method = "POST"))
        assertEquals(412, r!!.status)
        assertEquals("precondition", r.header("x-reason"))
    }

    /**
     * `SWR-MG-28` (b): the address and headers of a request reach the server and nothing else. Some providers put the credential in the query
     * (CloudKit, an Azure shared access signature), so each request here carries a marked value in its query and in a header, and the mark must
     * not come out on standard output, on standard error, in a `java.util.logging` record, or in an exception, whether the request is answered,
     * refused, unanswered, or has an address the transport will not take (which is `null`, never a throw).
     *
     * What this cannot see: a log the platform keeps outside the process (Android's logcat is not in a JVM unit test), and logging a host turns on
     * in its HTTP library's own configuration.
     */
    fun theAddressAndHeadersOfARequestAreNeverPrintedLoggedOrThrown() {
        val mark = "np-secret-mark-7f3a"
        val out = java.io.ByteArrayOutputStream()
        val records = CopyOnWriteArrayList<String>()
        val handler = object : java.util.logging.Handler() {
            override fun publish(record: java.util.logging.LogRecord) {
                records += listOfNotNull(record.message, record.parameters?.joinToString(), record.thrown?.toString()).joinToString(" ")
            }
            override fun flush() {}
            override fun close() {}
        }.apply { level = java.util.logging.Level.ALL }
        val root = java.util.logging.Logger.getLogger("")
        val oldOut = System.out
        val oldErr = System.err
        val answered = Loopback(200, emptyMap())
        val refusing = Loopback(403, emptyMap())
        val gone = Loopback(200, emptyMap()).let { s -> s.url("/gone").also { s.stop() } }
        val results = LinkedHashMap<String, Any?>()
        root.addHandler(handler)
        System.setOut(java.io.PrintStream(out, true))
        System.setErr(java.io.PrintStream(out, true))
        try {
            for ((what, address) in listOf(
                "answered" to answered.url("/database/1/c/production/private/records/query"),
                "refused" to refusing.url("/container/blob"),
                "no server" to gone,
                "a scheme that is not http" to "ftp://127.0.0.1/x",
                "a host with a space" to "http://exa mple.test/x",
                "no scheme" to "127.0.0.1/x",
                "not an address" to "not an address",
            )) {
                results[what] = try {
                    runBlocking {
                        transport().post(
                            HttpRequest("$address?ckAPIToken=$mark&sig=$mark", mapOf("Authorization" to "Bearer $mark"), ByteArray(0), method = "GET", timeoutSeconds = 5),
                        )?.status
                    }
                } catch (e: Throwable) {
                    e
                }
            }
        } finally {
            System.setOut(oldOut)
            System.setErr(oldErr)
            root.removeHandler(handler)
            answered.stop()
            refusing.stop()
        }
        results.forEach { (what, r) -> assertTrue(r !is Throwable, "$what threw ${(r as? Throwable)?.javaClass?.name}; an address a transport cannot use is null") }
        assertEquals(200, results["answered"])
        assertEquals(403, results["refused"])
        for (what in listOf("no server", "a scheme that is not http", "a host with a space", "no scheme", "not an address")) assertNull(results[what], what)
        assertTrue(answered.seen.single().headers["authorization"] == "Bearer $mark", "the request did carry the mark to the server")
        assertTrue(mark !in out.toString(), "the mark was printed")
        assertTrue(records.none { mark in it }, "the mark was logged")
    }

    fun noServerGivesNullNotAnException() {
        val s = Loopback(200, emptyMap())
        val url = s.url("/gone")
        s.stop()
        assertNull(runBlocking { transport().post(HttpRequest(url, emptyMap(), ByteArray(0), method = "DELETE", timeoutSeconds = 5)) })
    }
}
