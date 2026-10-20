package life.neurone.core.analytics

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
 * erasure relay has accepted it or refused it for good. A withdrawal made offline is therefore not lost: the request is sent when
 * the phone next can.
 *
 * Contract with the relay (`docs/np_analytics_001.md` §7.6): one `POST <host>/np/erase` with `{"distinct_id": …}` and nothing else.
 * 2xx: accepted (erasing an ID the vendor does not hold is accepted too), and the ID is dropped. 4xx other than 408 and 429: refused
 * for good, and the ID is dropped (retrying cannot change the answer). 408, 429, 5xx or no answer: the ID stays, and the rest wait
 * for the next call.
 *
 * The list is per install and sealed with the rest of the app's state; an ID in it is a pseudonym the vendor holds data under, so it
 * is never written to a log.
 */
class AnalyticsErasureQueue(
    private val store: KeyValueStore,
    private val transport: HttpTransport?,
    private val baseUrl: () -> String?,
) {
    private var draining = false

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
     * Ask the relay about every queued ID. Does nothing, and makes no network call, when the queue is empty, the build has no
     * analytics host or the host has no transport.
     */
    suspend fun drain() {
        if (draining) return
        val transport = transport ?: return
        draining = true
        try {
            for (id in pending) {
                val url = baseUrl()?.let { "$it/$PATH" } ?: return
                val body = buildJsonObject { put("distinct_id", id) }.toString().encodeToByteArray()
                val answer = transport.post(HttpRequest(url, mapOf("Content-Type" to "application/json"), body))
                when (outcome(answer?.status)) {
                    Outcome.DONE, Outcome.REFUSED -> remove(id)
                    Outcome.RETRY_LATER -> return
                }
            }
        } finally {
            draining = false
        }
    }

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
