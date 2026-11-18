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
     * The contract line of [HttpTransport]: the request URL is never logged or surfaced. A URL whose query holds a marker is sent three ways (answered,
     * to a port nothing listens on, and malformed), and the marker must appear in nothing the transport wrote to the console or to `java.util.logging`
     * and in nothing it threw. **What this cannot see:** a log written through another logging library, or to a file.
     */
    fun theRequestUrlIsNeverLoggedOrSurfaced() {
        val marker = "np-secret-" + java.util.UUID.randomUUID()
        val records = java.util.concurrent.CopyOnWriteArrayList<String>()
        val handler = object : java.util.logging.Handler() {
            override fun publish(record: java.util.logging.LogRecord) {
                // The loopback server below is in this process and logs each request it receives at FINE: that is the far end, not the transport.
                if (record.loggerName?.startsWith("com.sun.net.httpserver") == true) return
                records += (record.message ?: "") + " " + (record.parameters?.joinToString(" ") ?: "") + " " + (record.thrown?.toString() ?: "")
            }

            override fun flush() {}

            override fun close() {}
        }
        val root = java.util.logging.Logger.getLogger("")
        val oldLevel = root.level
        val oldOut = System.out
        val oldErr = System.err
        val console = java.io.ByteArrayOutputStream()
        val thrown = mutableListOf<Throwable>()
        root.addHandler(handler)
        root.level = java.util.logging.Level.ALL
        handler.level = java.util.logging.Level.ALL
        System.setOut(java.io.PrintStream(console, true))
        System.setErr(java.io.PrintStream(console, true))
        try {
            val live = Loopback(200, emptyMap())
            val dead = Loopback(200, emptyMap()).also { it.stop() }
            try {
                val urls = listOf(
                    live.url("/o/x?sig=$marker&ckWebAuthToken=$marker"),
                    dead.url("/o/x?sig=$marker"),
                    "https://exa mple.invalid:notaport/o/x?sig=$marker",
                    "nonsense://$marker",
                )
                for (u in urls) {
                    try {
                        runBlocking { transport().post(HttpRequest(u, mapOf("Authorization" to "Bearer $marker"), ByteArray(0), method = "GET", timeoutSeconds = 5)) }
                    } catch (e: Throwable) {
                        thrown += e
                    }
                }
            } finally {
                live.stop()
            }
        } finally {
            System.setOut(oldOut)
            System.setErr(oldErr)
            root.removeHandler(handler)
            root.level = oldLevel
        }
        assertTrue(thrown.isEmpty(), "a request that cannot be sent is null, not an exception: " + thrown.map { it::class.simpleName })
        assertTrue(marker !in console.toString(), "the transport wrote the request's address or a header to the console")
        assertTrue(records.none { marker in it }, "the transport logged the request's address or a header: " + records.filter { marker in it }.map { it.replace(marker, "<marker>").take(160) })
    }

    fun noServerGivesNullNotAnException() {
        val s = Loopback(200, emptyMap())
        val url = s.url("/gone")
        s.stop()
        assertNull(runBlocking { transport().post(HttpRequest(url, emptyMap(), ByteArray(0), method = "DELETE", timeoutSeconds = 5)) })
    }
}
