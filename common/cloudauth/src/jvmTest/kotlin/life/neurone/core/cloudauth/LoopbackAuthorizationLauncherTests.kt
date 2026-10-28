package life.neurone.core.cloudauth

import kotlinx.coroutines.CoroutineStart
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.cancelAndJoin
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withTimeout
import java.net.ConnectException
import java.net.InetAddress
import java.net.ServerSocket
import java.net.Socket
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * A port that was free a moment ago, below the range the system hands out as the source port of its own connections (32768 and up on Linux,
 * 49152 and up on macOS and Windows). An ephemeral port can be taken by one of the test's own client sockets between the check and the bind,
 * and a connection to a closed ephemeral port can connect to itself (a TCP self-connect), which reads as "still listening".
 */
private fun freePort(): Int {
    val random = java.util.Random()
    repeat(200) {
        val port = 20_000 + random.nextInt(10_000)
        try {
            ServerSocket(port, 1, InetAddress.getByName("127.0.0.1")).close()
            return port
        } catch (e: java.io.IOException) {
            // in use: try another
        }
    }
    error("no free port in 20000..29999")
}

/** True if nothing is listening on the port: a connection to it is refused. Used to show a listener was closed, without binding the port again (a bind can lose a race to another process). */
private fun isClosed(port: Int): Boolean = try { Socket("127.0.0.1", port).close(); false } catch (e: ConnectException) { true }

/** One raw HTTP exchange with the listener; the whole reply text, or "" if the connection was closed unanswered. */
private fun exchange(port: Int, request: String): String = Socket("127.0.0.1", port).use { s ->
    s.getOutputStream().write(request.toByteArray(Charsets.ISO_8859_1))
    s.getOutputStream().flush()
    s.getInputStream().readBytes().toString(Charsets.ISO_8859_1)
}

private fun get(port: Int, target: String, host: String = "127.0.0.1:$port") = exchange(port, "GET $target HTTP/1.1\r\nHost: $host\r\nUser-Agent: test\r\n\r\n")

class LoopbackRedirectTests {
    // @verifies SWR-MG-19#h
    @Test
    fun onlyAnHttpLoopbackAddressWithAPortIsARedirectTheListenerCanReceive() {
        val ok = LoopbackRedirect.parse("http://127.0.0.1:9004")
        assertEquals(9004, ok.port)
        assertEquals(9004, LoopbackRedirect.parse("http://localhost:9004/cb").port)
        for (bad in listOf("https://127.0.0.1:9004", "http://127.0.0.1", "http://evil.example:9004", "http://127.0.0.2:9004", "http://127.0.0.1:0", "http://127.0.0.1:70000", "msal://auth")) {
            assertEquals(CloudAuthProblem.INVALID_INPUT, assertFailsWith<CloudAuthException>(bad) { LoopbackRedirect.parse(bad) }.problem, bad)
        }
    }

    // @verifies SWR-MG-19#h
    @Test
    fun aRequestIsTakenOnlyForTheRedirectPathWithAGetFromTheLoopbackHost() {
        val r = LoopbackRedirect.parse("http://127.0.0.1:9004/cb")
        fun head(line: String, host: String? = "127.0.0.1:9004") = line + "\r\n" + (host?.let { "Host: $it\r\n" } ?: "") + "\r\n"
        assertEquals("http://127.0.0.1:9004/cb?code=c&state=s", r.reply(head("GET /cb?code=c&state=s HTTP/1.1")).address)
        assertEquals("http://127.0.0.1:9004/cb", r.reply(head("GET /cb HTTP/1.1", "LOCALHOST:9004")).address, "the name localhost is the same place")
        assertEquals(404, r.reply(head("GET /favicon.ico HTTP/1.1")).status)
        assertEquals(404, r.reply(head("GET /cb/extra?code=c HTTP/1.1")).status)
        assertEquals(405, r.reply(head("POST /cb HTTP/1.1")).status)
        assertEquals(403, r.reply(head("GET /cb?code=c HTTP/1.1", "evil.example:9004")).status, "a rebound name")
        assertEquals(403, r.reply(head("GET /cb?code=c HTTP/1.1", "127.0.0.1:9999")).status, "another port")
        assertEquals(403, r.reply(head("GET /cb?code=c HTTP/1.1", null)).status, "no Host")
        assertEquals(400, r.reply(head("nonsense")).status)
        assertEquals(400, r.reply(head("GET http://evil.example/cb HTTP/1.1")).status, "an absolute-form target")
        assertNull(r.reply(head("GET /cb HTTP/1.1", "evil.example:9004")).address)
        // A redirect with no path takes "/".
        assertEquals("http://127.0.0.1:9004/?code=c", LoopbackRedirect.parse("http://127.0.0.1:9004").reply(head("GET /?code=c HTTP/1.1")).address)
    }
}

class LoopbackAuthorizationLauncherTests {
    // @verifies SWR-MG-19#h
    @Test
    fun theBrowsersRedirectIsTakenWholeAndTheBrowserIsShownTheHostsPage() = runBlocking {
        val port = freePort()
        val redirect = "http://127.0.0.1:$port"
        var seenReply = ""
        val launcher = LoopbackAuthorizationLauncher("<p>done</p>") { shown ->
            assertEquals("https://idp.example/auth?x=1", shown)
            Thread { seenReply = get(port, "/?code=c%2F1&state=s") }.start() // the browser comes back, from another thread
        }
        val back = withTimeout(10_000) { launcher.authorize("https://idp.example/auth?x=1", redirect) }
        assertEquals("$redirect/?code=c%2F1&state=s", back)
        // The page is written before the listener closes; give the browser thread a moment to read it.
        for (i in 0 until 100) { if (seenReply.isNotEmpty()) break; Thread.sleep(20) }
        assertTrue(seenReply.startsWith("HTTP/1.1 200 OK"), seenReply)
        assertTrue(seenReply.endsWith("<p>done</p>"))
        assertTrue("Referrer-Policy: no-referrer" in seenReply && "Cache-Control: no-store" in seenReply)
    }

    // @verifies SWR-MG-19#h
    @Test
    fun otherRequestsAreAnsweredWithAnErrorAndTheSignInKeepsWaiting() = runBlocking {
        val port = freePort()
        val redirect = "http://127.0.0.1:$port/cb"
        val statuses = mutableListOf<String>()
        val launcher = LoopbackAuthorizationLauncher { 
            Thread {
                statuses += get(port, "/favicon.ico").lineSequence().first()
                statuses += get(port, "/cb?code=stolen", host = "evil.example:$port").lineSequence().first()
                statuses += exchange(port, "POST /cb HTTP/1.1\r\nHost: 127.0.0.1:$port\r\nContent-Length: 0\r\n\r\n").lineSequence().first()
                statuses += get(port, "/cb?code=real&state=s").lineSequence().first()
            }.start()
        }
        val back = withTimeout(10_000) { launcher.authorize("https://idp.example/auth", redirect) }
        assertEquals("$redirect?code=real&state=s", back, "none of the earlier requests ended the sign-in")
        for (i in 0 until 100) { if (statuses.size == 4) break; Thread.sleep(20) }
        assertEquals(listOf("HTTP/1.1 404 Not Found", "HTTP/1.1 403 Forbidden", "HTTP/1.1 405 Method Not Allowed", "HTTP/1.1 200 OK"), statuses)
    }

    // @verifies SWR-MG-19#h
    @Test
    fun anErrorAnswerIsHandedBackForCloudAuthToRefuseAndAnOversizedRequestIsNot() = runBlocking {
        val port = freePort()
        val redirect = "http://localhost:$port"
        var oversized = ""
        val launcher = LoopbackAuthorizationLauncher {
            Thread {
                oversized = exchange(port, "GET /?" + "a".repeat(9000) + " HTTP/1.1\r\nHost: localhost:$port\r\n\r\n").lineSequence().first()
                get(port, "/?error=access_denied&state=s", host = "localhost:$port")
            }.start()
        }
        val back = withTimeout(10_000) { launcher.authorize("https://idp.example/auth", redirect) }
        assertEquals("$redirect/?error=access_denied&state=s", back)
        for (i in 0 until 100) { if (oversized.isNotEmpty()) break; Thread.sleep(20) }
        assertEquals("HTTP/1.1 431 Request Header Fields Too Large", oversized)
    }

    // @verifies SWR-MG-19#h
    @Test
    fun aPortInUseFailsBeforeAnythingOpensAndABrowserThatWillNotOpenFailsAndFreesThePort() = runBlocking {
        val opened = mutableListOf<String>()
        // The busy port is one this test holds open, so it is busy by construction and not by a guess that a port just released stays free.
        val held = ServerSocket(0, 1, InetAddress.getByName("127.0.0.1"))
        val busy = held.localPort
        held.use {
            val e = assertFailsWith<CloudAuthException> { LoopbackAuthorizationLauncher { opened += it }.authorize("https://x", "http://127.0.0.1:$busy") }
            assertEquals(CloudAuthProblem.PROVIDER, e.problem)
            assertTrue(opened.isEmpty(), "the page is not opened when nothing could take the answer")
        }
        val port = freePort()
        val broken = LoopbackAuthorizationLauncher { throw IllegalStateException("no browser") }
        assertEquals(CloudAuthProblem.PROVIDER, assertFailsWith<CloudAuthException> { broken.authorize("https://x", "http://127.0.0.1:$port") }.problem)
        assertTrue(isClosed(port), "the listener was closed")
    }

    // @verifies SWR-MG-19#h
    @Test
    fun cancellingTheWaitClosesTheListener() = runBlocking {
        val port = freePort()
        var up = false
        val wait = async(Dispatchers.IO, start = CoroutineStart.DEFAULT) {
            LoopbackAuthorizationLauncher { up = true }.authorize("https://x", "http://127.0.0.1:$port")
        }
        for (i in 0 until 200) { if (up) break; Thread.sleep(10) }
        assertTrue(up)
        wait.cancelAndJoin()
        assertTrue(isClosed(port), "the listener was closed")
    }
}
