package life.neurone.shared.migration

import io.ktor.network.selector.SelectorManager
import io.ktor.network.sockets.Connection
import io.ktor.network.sockets.Datagram
import io.ktor.network.sockets.InetSocketAddress
import io.ktor.network.sockets.aSocket
import io.ktor.network.sockets.connection
import io.ktor.utils.io.ByteReadChannel
import io.ktor.utils.io.ByteWriteChannel
import io.ktor.utils.io.core.readBytes
import io.ktor.utils.io.readAvailable
import io.ktor.utils.io.writeFully
import io.ktor.utils.io.core.buildPacket
import io.ktor.utils.io.core.writeText
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeoutOrNull
import kotlinx.serialization.json.jsonPrimitive
import life.neurone.core.migration.Link
import life.neurone.core.migration.LinkTransport
import life.neurone.core.migration.MigrationFlow

// A phone-to-phone link over the local Wi-Fi network (SWR-MG-10; issue #569 package 6u). Written once for Android, the
// desktop apps and iOS (they share this source set); the browser has no sockets and offers no direct link.
//
// What it does is only to find the other phone and carry bytes. Who may be served, the proof of the code and the sealing of
// every frame are the core's (`link.rs`), so nothing here is trusted: a phone that answers a probe is still asked to prove it
// holds the code, and a stranger that connects is refused by the core.
//
// Discovery: the phone that is waiting accepts on a TCP port of the system's choosing and answers UDP probes on the core's
// discovery port with that port; the phone that is looking broadcasts the probe, takes the first answer and connects to the
// sender. The values come from the core (`linkDiscovery`).

/** Bytes over one TCP connection. */
private class TcpLink(private val connection: Connection, private val selector: SelectorManager) : Link {
    private val input: ByteReadChannel = connection.input
    private val output: ByteWriteChannel = connection.output

    override suspend fun send(bytes: ByteArray) {
        output.writeFully(bytes)
        output.flush()
    }

    override suspend fun receive(): ByteArray? {
        val buffer = ByteArray(READ_CHUNK)
        val n = input.readAvailable(buffer)
        return if (n < 0) null else buffer.copyOf(n)
    }

    override suspend fun close() {
        runCatching { connection.socket.close() }
        // The selector drives this connection's I/O, so it lives exactly as long as the link does.
        runCatching { selector.close() }
    }
}

private const val READ_CHUNK = 64 * 1024

/**
 * [probeTargets] is where the looking phone sends its probe: the network's broadcast address by default. A test passes the
 * loopback address instead, since a sandbox may not route a broadcast. [discoveryPort] is the core's unless a test needs
 * two on one machine to avoid each other.
 */
class LanLinkTransport(
    private val probeTargets: List<String> = listOf("255.255.255.255"),
    private val discoveryPort: Int = MigrationFlow.linkDiscovery().port(),
    private val lookSeconds: Int = 10,
) : LinkTransport {
    private val discovery = MigrationFlow.linkDiscovery()
    private val probe = discovery["probe"]!!.jsonPrimitive.content
    private val reply = discovery["reply"]!!.jsonPrimitive.content

    override suspend fun accept(): Link? {
        val selector = SelectorManager(Dispatchers.Default)
        val server = aSocket(selector).tcp().bind("0.0.0.0", 0)
        val port = (server.localAddress as InetSocketAddress).port
        val udp = aSocket(selector).udp().bind(InetSocketAddress("0.0.0.0", discoveryPort)) { reuseAddress = true }
        var linked = false
        return try {
            coroutineScope {
                val answering = launch {
                    for (datagram in udp.incoming) {
                        if (datagram.packet.readBytes().decodeToString() == probe) {
                            udp.send(Datagram(buildPacket { writeText("$reply $port") }, datagram.address))
                        }
                    }
                }
                try {
                    TcpLink(server.accept().connection(), selector).also { linked = true }
                } finally {
                    answering.cancel()
                }
            }
        } catch (e: CancellationException) {
            throw e
        } finally {
            runCatching { udp.close() }
            runCatching { server.close() }
            if (!linked) runCatching { selector.close() }
        }
    }

    override suspend fun connect(): Link? {
        val selector = SelectorManager(Dispatchers.Default)
        val udp = aSocket(selector).udp().bind(InetSocketAddress("0.0.0.0", 0)) { broadcast = true }
        var linked = false
        try {
            val found = withTimeoutOrNull(lookSeconds * 1000L) {
                coroutineScope {
                    val asking = launch {
                        while (true) {
                            for (target in probeTargets) {
                                runCatching { udp.send(Datagram(buildPacket { writeText(probe) }, InetSocketAddress(target, discoveryPort))) }
                            }
                            delay(PROBE_EVERY_MS)
                        }
                    }
                    try {
                        var heard: Pair<String, Int>? = null
                        while (heard == null) {
                            val d = udp.receive()
                            val text = d.packet.readBytes().decodeToString()
                            val port = text.removePrefix("$reply ").toIntOrNull()
                            if (text.startsWith("$reply ") && port != null) {
                                val address = d.address as InetSocketAddress
                                heard = address.hostname to port
                            }
                        }
                        heard
                    } finally {
                        asking.cancel()
                    }
                }
            }
            val (host, port) = found ?: return null
            val link = TcpLink(aSocket(selector).tcp().connect(InetSocketAddress(host, port)).connection(), selector)
            linked = true
            return link
        } finally {
            runCatching { udp.close() }
            if (!linked) runCatching { selector.close() }
        }
    }

    private companion object {
        const val PROBE_EVERY_MS = 500L
    }
}

private fun kotlinx.serialization.json.JsonObject.port(): Int = this["port"]!!.jsonPrimitive.content.toInt()
