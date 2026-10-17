package life.neurone.core.migration

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withTimeoutOrNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.common.KeyValueStore
import life.neurone.core.data.BundleSecret
import life.neurone.core.data.RandomSource
import life.neurone.core.data.SystemRandomSource
import life.neurone.core.platform.epochSeconds

// A direct phone-to-phone migration over the local network (SWR-MG-10; issue #569 package 6t).
//
// Everything that decides is in the core (`link.rs`): the pairing code and its policy, the SPAKE2 proof that the other phone
// holds it, the sealed channel, what a peer may ask for. What is here is the host's side of it: a way to reach the other phone
// (the one thing each platform writes, over a local Wi-Fi socket) and the loops around `serve` and `restore`.

/**
 * How two phones reach each other. Each platform writes one over a local Wi-Fi socket; everything above the bytes is shared.
 * The code is never sent through here (it only decides whether the proof succeeds), so a transport may advertise and discover
 * however it likes: a phone that answers is not trusted, it still has to prove it holds the code.
 */
interface LinkTransport {
    /** Make this phone findable and wait for one phone to connect, or null once stopped. Cancel the coroutine to stop. */
    suspend fun accept(): Link?

    /** Find a phone that is waiting and connect to it, or null if none answered. */
    suspend fun connect(): Link?
}

/**
 * An object store held in memory, whose claim is exclusive because every call takes one lock. It holds the bundle of a
 * migration that goes straight to another phone, so there is nothing of it on disk to find afterwards (SWR-MG-06).
 */
class MemoryObjectStore : ObjectStore {
    private val lock = Mutex()
    private val objects = LinkedHashMap<String, ByteArray>()

    override suspend fun get(name: String): ByteArray? = lock.withLock { objects[name]?.copyOf() }
    override suspend fun put(name: String, bytes: ByteArray) = lock.withLock { objects[name] = bytes.copyOf() }
    override suspend fun createIfAbsent(name: String, bytes: ByteArray): Boolean = lock.withLock {
        if (name in objects) false else { objects[name] = bytes.copyOf(); true }
    }
    override suspend fun delete(name: String) = lock.withLock { objects.remove(name); Unit }
    override suspend fun list(prefix: String): List<String> = lock.withLock { objects.keys.filter { it.startsWith(prefix) } }
    override suspend fun freeSpace(): Long? = null
}

/** The route a direct link is: exclusive in this process, direct, and paired by the code. */
val LINK_ROUTE: RouteDescriptor = ObjectStoreRoute("link", atomicClaim = true, direct = true, authenticatedPairing = true)

sealed interface LinkSendState {
    data object Idle : LinkSendState

    /** Showing [display]; the other phone has not paired yet. [offer] says what is left of the code. */
    class Waiting(val offer: PairingOffer, val display: String) : LinkSendState

    /** The other phone has the bundle and took the claim: this phone may now clear (SWR-MG-08). */
    data object Sent : LinkSendState

    /** The code ran out or was used up ([reason] is the core's: `code_expired` or `code_spent`). */
    class Spent(val reason: String) : LinkSendState
}

/**
 * The phone that holds the bundle: shows a code and serves its [store] to the phone that proves it knows it. A pairing that
 * does not end in a claim (a wrong secret, a link that dropped) makes a fresh code, since the person will start again.
 */
class LinkSendSession(
    private val store: ObjectStore,
    private val transport: LinkTransport,
    private val random: RandomSource = SystemRandomSource,
    private val clock: () -> Long = ::epochSeconds,
) {
    private val state = MutableStateFlow<LinkSendState>(LinkSendState.Idle)
    val step: StateFlow<LinkSendState> = state.asStateFlow()

    suspend fun run(): LinkSendState {
        var offer = Pairing.newOffer(random, clock)
        val claim = MigrationFlow.objectNames()["claim"]!!.jsonPrimitive.content
        while (true) {
            state.value = LinkSendState.Waiting(offer, Pairing.display(offer.code))
            val remaining = (offer.expiresAt - clock()) * 1000
            val link = if (remaining > 0) withTimeoutOrNull(remaining) { transport.accept() } else null
            if (link == null) {
                state.value = if (clock() >= offer.expiresAt) LinkSendState.Spent("code_expired") else LinkSendState.Idle
                return state.value
            }
            val done = try {
                MigrationFlow.serve(link, store, offer, random, clock)
            } catch (e: kotlin.coroutines.cancellation.CancellationException) {
                throw e
            } catch (e: Exception) {
                buildJsonObject { put("ok", false); put("reason", "link_failed") }
            } finally {
                runCatching { link.close() }
            }
            if (store.get(claim) != null) {
                state.value = LinkSendState.Sent
                return state.value
            }
            val paired = done["paired"]?.jsonPrimitive?.content == "true"
            offer = if (paired) Pairing.newOffer(random, clock) else offer.after(done) ?: run {
                val why = done["reason"]?.jsonPrimitive?.content
                state.value = LinkSendState.Spent(if (why == "code_expired") why else "code_spent")
                return state.value
            }
        }
    }
}

/**
 * The phone that restores: reads the code the person typed, finds the other phone, pairs and restores over the sealed link.
 * [secretFor] is asked for the recovery code or passphrase only once the bundle's header has been read from the other phone
 * (it is given the header's `kdf`), and returns null if the person declines. Returns the flow's `done` object.
 */
class LinkReceiveSession(
    private val transport: LinkTransport,
    private val keys: KeyValueStore,
    private val items: List<MigrationItemStore> = emptyList(),
    private val random: RandomSource = SystemRandomSource,
    private val clock: () -> Long = ::epochSeconds,
) {
    suspend fun run(typedCode: String, secretFor: suspend (JsonObject) -> BundleSecret?): JsonObject {
        val code = Pairing.parse(typedCode) ?: return failed("wrong_code")
        val link = transport.connect() ?: return failed("link_failed")
        val host = StandardMigrationHost(items)
        val params = buildJsonObject {
            put("route", LINK_ROUTE.toJson())
            put("hostItems", JsonArray(host.hostItems.map { JsonPrimitive(it) }))
            put("pairing", buildJsonObject { put("code", code) })
        }
        return try {
            MigrationFlow.run("restore", params, host, null, keys, link = link, random = random, clock = clock, secretProvider = secretFor)
        } catch (e: SecretDeclined) {
            failed("cancelled")
        } finally {
            runCatching { link.close() }
        }
    }

    private fun failed(reason: String) = buildJsonObject { put("ok", false); put("reason", reason) }
}
