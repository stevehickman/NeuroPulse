package life.neurone.core.analytics

import kotlinx.serialization.json.boolean
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.npps.NppsCore

/**
 * Whether an analytics event may leave the phone (`REQ-SW03-07`; NP-APP-TELEMETRY-001 §3 and §7).
 *
 * The rule is the shared core's, once (`common/npps-core/src/analytics.rs`, reading `common/npps/analytics-properties.json`), so iOS
 * and Android cannot disagree about it. It is an allowlist: the event's name must have the allowed shape and contain no forbidden
 * term, and every property must be one the policy names, with a value of its form. Anything else drops the whole event.
 *
 * **It fails closed.** If the core cannot be asked (not loaded yet, a malformed answer), the event is not permitted: an analytics
 * event that could not be checked is not one to send.
 */
object AnalyticsEventPolicy {
    fun permits(event: String, properties: Map<String, String>): Boolean = try {
        val answer = NppsCore.migration(buildJsonObject {
            put("op", "checkAnalyticsEvent")
            put("event", event)
            put("properties", buildJsonObject { properties.forEach { (k, v) -> put(k, v) } })
        })
        answer["ok"]!!.jsonPrimitive.boolean
    } catch (ignored: Exception) {
        false
    }
}
