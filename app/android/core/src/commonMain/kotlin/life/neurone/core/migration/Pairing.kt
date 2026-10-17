package life.neurone.core.migration

import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.data.RandomSource
import life.neurone.core.data.SystemRandomSource
import life.neurone.core.platform.epochSeconds

// The code that pairs two phones for a direct-link migration (SWR-MG-10). The code, its life, its tries and how it is read are
// the core's (`link.rs`); this is only the host's side of them: make an offer, keep it between connections, read what was typed.

/**
 * What the phone that holds the bundle shows, and keeps until it is spent. [expiresAt] is Unix seconds; [triesLeft] is what
 * is left after the sessions already tried (each one costs a try, wrong or not).
 */
data class PairingOffer(val code: String, val expiresAt: Long, val triesLeft: Int) {
    internal fun toJson(): JsonObject = buildJsonObject {
        put("code", code)
        put("expiresAt", expiresAt)
        put("triesLeft", triesLeft)
    }

    /** The offer after a `serve` ended: the same code and expiry with the tries the core says are left, or null once it is spent. */
    fun after(result: JsonObject): PairingOffer? {
        if (result["paired"]?.jsonPrimitive?.content == "true") return null
        val left = result["triesLeft"]?.jsonPrimitive?.content?.toIntOrNull() ?: triesLeft
        return if (left <= 0 || result["reason"]?.jsonPrimitive?.content == "code_expired") null else copy(triesLeft = left)
    }
}

object Pairing {
    /** A new offer: the core's code from the platform's randomness, valid for the core's lifetime and tries. */
    fun newOffer(random: RandomSource = SystemRandomSource, clock: () -> Long = ::epochSeconds): PairingOffer {
        val policy = MigrationFlow.pairingPolicy()
        val code = MigrationFlow.pairingCode(random.nextBytes(policy["randomBytes"]!!.jsonPrimitive.content.toInt()))
        return PairingOffer(
            code = code,
            expiresAt = clock() + policy["lifetimeSeconds"]!!.jsonPrimitive.content.toLong(),
            triesLeft = policy["tries"]!!.jsonPrimitive.content.toInt(),
        )
    }

    /** The code as shown: the digits in two groups, so it is easy to read out and to type. */
    fun display(code: String): String = code.chunked(code.length / 2).joinToString(" ")

    /** The digits of what the person typed, or null if it is not a code. */
    fun parse(text: String): String? = MigrationFlow.parsePairingCode(text)
}
