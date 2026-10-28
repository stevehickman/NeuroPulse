package life.neurone.core.cloudauth

import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.NonCancellable
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.ByteArrayOutputStream
import java.io.IOException
import java.net.InetAddress
import java.net.ServerSocket
import java.net.Socket
import java.util.Collections

/**
 * Signs in on a computer by listening for the redirect on the loopback address (RFC 8252 section 7.3): the system browser is sent to the
 * provider's page and the provider sends it back to `http://127.0.0.1:<port>/...`, where this class is listening. The listener is bound to
 * the loopback address only, so nothing else on the network can reach it, and it exists only while a sign-in waits.
 *
 * [open] shows an address in the person's browser; the host supplies it. It is called after the listener is up, so the redirect cannot
 * arrive before there is somebody to take it.
 *
 * **The port is the one in the registered redirect address**, so it has to be free: a port in use fails the sign-in with a reason and
 * nothing opens. (A provider that accepts any port on the loopback address would allow a free one to be picked, but [CloudAuth] sends
 * one redirect address in both the sign-in and the token request, and they must agree.)
 *
 * **What it answers.** Only a `GET` for the redirect's path, from a request whose `Host` is the loopback address and port (a page that
 * resolves its own name to this address is refused: DNS rebinding). Anything else, such as the browser's request for an icon, is
 * answered with an error and the listener carries on. The first matching request is the answer: [authorize] returns its address
 * whole, whatever it carries (a code, or `error=access_denied`), and [CloudAuth] decides. [donePage] is what the browser is shown
 * afterwards; it is the host's, because it is text the person reads (`CLAUDE.md` §17), and empty by default.
 *
 * Cancelling the waiting coroutine closes the listener and every open connection.
 */
class LoopbackAuthorizationLauncher(
    private val donePage: String = "",
    private val open: (String) -> Unit,
) : AuthorizationLauncher {

    override suspend fun authorize(authorizationUrl: String, redirectUri: String): String {
        val redirect = LoopbackRedirect.parse(redirectUri)
        val server = try {
            ServerSocket(redirect.port, BACKLOG, InetAddress.getByName(LOOPBACK))
        } catch (e: IOException) {
            throw CloudAuthException(CloudAuthProblem.PROVIDER, "port ${redirect.port} is not free for the sign-in to return to")
        }
        val answer = CompletableDeferred<String>()
        val connections = Collections.synchronizedSet(mutableSetOf<Socket>())
        val scope = CoroutineScope(Dispatchers.IO + SupervisorJob())
        var acceptLoop: Job? = null
        try {
            acceptLoop = scope.launch {
                while (true) {
                    val socket = try {
                        server.accept()
                    } catch (e: IOException) {
                        return@launch // closed: the sign-in is over
                    }
                    if (server.isClosed) {
                        try { socket.close() } catch (_: IOException) {}
                        return@launch
                    }
                    connections += socket
                    // On the outer scope, not as a child of this loop: waiting for the loop to end must not wait on a connection's read.
                    scope.launch { serve(socket, redirect, answer) }
                }
            }
            try {
                open(authorizationUrl)
            } catch (e: Exception) {
                throw CloudAuthException(CloudAuthProblem.PROVIDER, "the browser could not be opened")
            }
            return answer.await()
        } finally {
            scope.cancel(CancellationException("sign-in over"))
            try { server.close() } catch (_: IOException) {}
            synchronized(connections) { connections.forEach { try { it.close() } catch (_: IOException) {} } }
            // Closing a listener that another thread is blocked in accept() on returns before that thread has woken, and until it does the
            // socket still takes connections. Returning only once the loop has ended means the port is free when this returns (a sign-in
            // started again at once can bind it) and nothing is accepted after a cancel. NonCancellable: this runs when the wait was cancelled.
            withContext(NonCancellable) { acceptLoop?.join() }
        }
    }

    private fun serve(socket: Socket, redirect: LoopbackRedirect, answer: CompletableDeferred<String>) {
        try {
            val head = readHead(socket) ?: return respond(socket, LoopbackRedirect.Reply(431, null), "")
            val reply = redirect.reply(head)
            respond(socket, reply, donePage)
            // The answer is given after the page is written, so closing the listener cannot cut the page off.
            reply.address?.let { answer.complete(it) }
        } catch (_: IOException) {
            // A connection the browser dropped: nothing to answer, and nothing to fail.
        } finally {
            try { socket.close() } catch (_: IOException) {}
        }
    }

    /** The request line and headers, or null if they run past [MAX_HEAD] bytes without ending. */
    private fun readHead(socket: Socket): String? {
        val input = socket.getInputStream()
        val head = ByteArrayOutputStream()
        var lastFour = 0 // the last four bytes read, as one int
        while (head.size() < MAX_HEAD) {
            val b = input.read()
            if (b < 0) return head.toString(Charsets.ISO_8859_1)
            head.write(b)
            lastFour = (lastFour shl 8) or b
            if (lastFour == CRLF_CRLF) return head.toString(Charsets.ISO_8859_1)
        }
        return null
    }

    private fun respond(socket: Socket, reply: LoopbackRedirect.Reply, page: String) {
        val body = if (reply.status == 200) page.toByteArray(Charsets.UTF_8) else ByteArray(0)
        val head = "HTTP/1.1 ${reply.status} ${LoopbackRedirect.reason(reply.status)}\r\n" +
            "Content-Type: text/html; charset=utf-8\r\nContent-Length: ${body.size}\r\n" +
            "Cache-Control: no-store\r\nReferrer-Policy: no-referrer\r\nConnection: close\r\n\r\n"
        val out = socket.getOutputStream()
        out.write(head.toByteArray(Charsets.ISO_8859_1))
        out.write(body)
        out.flush()
    }

    private companion object {
        const val LOOPBACK = "127.0.0.1"
        const val BACKLOG = 8
        const val CRLF_CRLF = 0x0d0a0d0a

        // PROVISIONAL (UC-114): no measurement of the largest request head a browser sends to a loopback address.
        const val MAX_HEAD = 8192
    }
}

/** The loopback redirect address and what to make of a request to it; no sockets, so it is tested without any. */
internal class LoopbackRedirect private constructor(private val authority: String, val port: Int, private val path: String) {
    class Reply(val status: Int, val address: String?)

    /** The decision for one request head (request line and headers). */
    fun reply(head: String): Reply {
        val lines = head.split("\r\n")
        val parts = lines[0].split(' ')
        if (parts.size != 3 || !parts[2].startsWith("HTTP/1.")) return Reply(400, null)
        if (parts[0] != "GET") return Reply(405, null)
        val host = lines.drop(1).firstOrNull { it.startsWith("host:", ignoreCase = true) }?.substringAfter(':')?.trim()
        if (host == null || (host != "127.0.0.1:$port" && !host.equals("localhost:$port", ignoreCase = true))) return Reply(403, null)
        val target = parts[1]
        if (!target.startsWith("/")) return Reply(400, null)
        val requestPath = target.substringBefore('?').substringBefore('#')
        if (requestPath != path) return Reply(404, null)
        return Reply(200, "http://$authority$target")
    }

    companion object {
        /** Only `http` on `127.0.0.1` or `localhost` with an explicit port: any other redirect is not one a listener here can receive. */
        fun parse(redirectUri: String): LoopbackRedirect {
            val bad = { why: String -> CloudAuthException(CloudAuthProblem.INVALID_INPUT, "not a loopback redirect: $why") }
            if (!redirectUri.startsWith("http://")) throw bad("it is not http")
            val rest = redirectUri.removePrefix("http://")
            val authority = rest.substringBefore('/').substringBefore('?').substringBefore('#')
            val host = authority.substringBeforeLast(':', "")
            val port = authority.substringAfterLast(':', "").toIntOrNull() ?: throw bad("it has no port")
            if (host != "127.0.0.1" && !host.equals("localhost", ignoreCase = true)) throw bad("the host is not the loopback address")
            if (port !in 1..65535) throw bad("the port is out of range")
            val path = rest.removePrefix(authority).substringBefore('?').substringBefore('#').ifEmpty { "/" }
            return LoopbackRedirect(authority, port, path)
        }

        fun reason(status: Int): String = when (status) {
            200 -> "OK"
            400 -> "Bad Request"
            403 -> "Forbidden"
            404 -> "Not Found"
            405 -> "Method Not Allowed"
            else -> "Request Header Fields Too Large"
        }
    }
}
