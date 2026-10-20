package life.neurone.core.analytics

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put
import life.neurone.core.common.KeyValueStore
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpTransport
import life.neurone.core.platform.secureRandomBytes
import life.neurone.core.platform.toHex

/**
 * The production [AnalyticsBackend]: PostHog's capture API from common code, so every host sends the same events the same way and
 * none of them carries a vendor SDK (`CLAUDE.md` §22 leaves the network to the host, not the policy). Consumed only by
 * [ResearchAnalyticsGate]; nothing else sends an analytics event.
 *
 * What it sends: the event name, the property map the gate already checked against the allowlist (`AnalyticsEventPolicy`), the
 * installation's pseudonymous `distinct_id`, the project token, and two switches that tell the server not to build a person
 * (`$process_person_profile` false) and not to look the address up (`$geoip_disable` true). Nothing else: no client timestamp, no
 * device model, no operating-system version, no locale, no session or screen. The iOS SDK this replaces added its own device
 * properties; this adds none.
 *
 * What it keeps: the `distinct_id` (a random value the vendor holds events under; see [AnalyticsErasureQueue] for how it is erased)
 * and nothing else. Events are not queued: one that cannot be sent now is dropped, which for engagement counts is the
 * privacy-preserving direction and means no log of events sits on the phone.
 *
 * It sends nothing until [configure] is called (by the gate, after the consent flow completes) and nothing after [reset]. A build
 * without a usable host and token never sends.
 */
class PostHogAnalyticsBackend(
    private val store: KeyValueStore,
    private val transport: HttpTransport,
    private val config: AnalyticsConfig,
    private val scope: CoroutineScope,
    private val randomBytes: (Int) -> ByteArray = ::secureRandomBytes,
) : AnalyticsBackend {

    private var active = false

    override val distinctId: String?
        get() = if (active) currentId() else null

    override fun configure() {
        if (!config.usable) return
        active = true
    }

    override fun track(event: String, properties: Map<String, String>) {
        if (!active) return
        val baseUrl = config.baseUrl ?: return
        val token = config.projectToken ?: return
        val body = buildJsonObject {
            put("api_key", token)
            put("event", event)
            put("distinct_id", currentId())
            put(
                "properties",
                buildJsonObject {
                    properties.forEach { (k, v) -> put(k, JsonPrimitive(v)) }
                    put("\$process_person_profile", false)
                    put("\$geoip_disable", true)
                },
            )
        }.toString().encodeToByteArray()
        scope.launch {
            transport.post(HttpRequest("$baseUrl/capture/", mapOf("Content-Type" to "application/json"), body))
        }
    }

    /**
     * Stop collecting and make a new ID, so a later session cannot be joined to the earlier ones even if the person consents
     * again. The gate has read [distinctId] and queued it for erasure before this runs.
     */
    override fun reset() {
        active = false
        store.putString(ID_KEY, newId())
    }

    private fun currentId(): String = store.getString(ID_KEY)?.takeIf { it.isNotEmpty() } ?: newId().also { store.putString(ID_KEY, it) }

    private fun newId(): String = randomBytes(16).toHex()

    companion object {
        const val ID_KEY = "np.analytics.distinct-id"
    }
}
