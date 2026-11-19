package life.neurone.core.analytics

import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put
import life.neurone.core.common.KeyValueStore
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpTransport

/**
 * Erasing a withdrawn person's pseudonymous analytics ID from the vendor (`NP-APP-TELEMETRY-001` §5, `REQ-SW03-19`, `OI-ANALYTICS-01`).
 * Port of the iOS `AnalyticsErasureQueue`, once for every host.
 *
 * The policy says the ID must be deleted from the vendor within 30 days of a research-consent withdrawal. Resetting the vendor
 * client makes a NEW anonymous ID, so the old one is gone from the phone the moment it runs: unless it was kept, nothing can ever
 * ask for its deletion. So the gate captures it first ([ResearchAnalyticsGate.reset]) and this queue holds it, durably, until the
 * erasure relay has accepted it. A withdrawal made offline is therefore not lost: the request is sent when the phone next can.
 *
 * Contract with the relay (`docs/np_analytics_001.md` §7.6): one `POST <host>/np/erase` with `{"distinct_id": …}` and nothing else.
 * 2xx: accepted (erasing an ID the vendor does not hold is accepted too, so there is no separate "already gone" answer), and the
 * ID is dropped. **Nothing else drops it** (`OI-PRIV-08` (2)): the queue holds the only copy, and a 401, 403 or 404 is what a
 * misdeployed relay or a proxy in front of it answers, not a verdict on the ID. A 4xx other than 408 and 429 keeps the ID and the
 * drain goes on to the next one (the answer may be about this ID alone); 408, 429, 5xx, any other status or no answer keeps it
 * and the rest wait for the next call (the relay is likely unavailable to all of them). Either way an ID is asked about at most
 * once in a drain, and a drain runs at launch, at a withdrawal and before an erase, so a relay that refuses is not asked in a loop.
 *
 * The list is per install and sealed with the rest of the app's state; an ID in it is a pseudonym the vendor holds data under, so it
 * is never written to a log.
 */
class AnalyticsErasureQueue(
    private val store: KeyValueStore,
    private val transport: HttpTransport?,
    private val baseUrl: () -> String?,
) {
    /** One drain at a time; a second caller waits and then asks about whatever is still queued, so an erase can wait for the answer. */
    private val draining = Mutex()

    /** The IDs still waiting, oldest first. */
    val pending: List<String>
        get() = store.getString(KEY).orEmpty().split('\n').filter { it.isNotEmpty() }

    /** Keep [id] until the relay has answered for it. Persisted before it returns. An ID already queued is not queued twice. */
    fun enqueue(id: String) {
        if (id.isEmpty() || !VALID.matches(id)) return
        val ids = pending
        if (id in ids) return
        save(ids + id)
    }

    /**
     * Ask the relay about every queued ID, once each. Does nothing, and makes no network call, when the queue is empty, the build
     * has no analytics host or the host has no transport. Returns when every queued ID has been asked about or the relay stopped
     * answering; [pending] then says what is left.
     */
    suspend fun drain() {
        val transport = transport ?: return
        draining.withLock {
            for (id in pending) {
                val url = baseUrl()?.let { "$it/$PATH" } ?: return
                val body = buildJsonObject { put("distinct_id", id) }.toString().encodeToByteArray()
                val answer = transport.post(HttpRequest(url, mapOf("Content-Type" to "application/json"), body))
                when (outcome(answer?.status)) {
                    Outcome.DONE -> remove(id)
                    Outcome.REFUSED -> continue
                    Outcome.RETRY_LATER -> return
                }
            }
        }
    }

    /** [REFUSED] is a 4xx that is not about load: the ID is kept and asked about again at the next drain, never dropped. */
    internal enum class Outcome { DONE, REFUSED, RETRY_LATER }

    private fun outcome(status: Int?): Outcome = when {
        status == null -> Outcome.RETRY_LATER
        status in 200..299 -> Outcome.DONE
        status == 408 || status == 429 -> Outcome.RETRY_LATER
        status in 400..499 -> Outcome.REFUSED
        else -> Outcome.RETRY_LATER
    }

    private fun remove(id: String) = save(pending.filter { it != id })

    private fun save(ids: List<String>) {
        if (ids.isEmpty()) store.remove(KEY) else store.putString(KEY, ids.joinToString("\n"))
    }

    companion object {
        /** The relay's path on the analytics host. */
        const val PATH = "np/erase"
        const val KEY = "np.analytics.pending-erasures"

        /** What the vendor's client makes: a UUID or a token of letters, digits and hyphens. Anything else is not queued (it could not be an ID, and a newline would corrupt the list). */
        private val VALID = Regex("^[0-9A-Za-z-]{16,64}$")
    }
}
