package life.neurone.core.analytics

import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.int
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.data.hexToBytes
import life.neurone.core.npps.NppsCore
import life.neurone.core.platform.secureRandomBytes
import life.neurone.core.platform.toHex

/**
 * The fleet upload as Oblivious HTTP (RFC 9458; `OI-FLEET-01` (1), `REQ-SHDR-07`), so that the server that reads a helmet's
 * SHDR record never sees the connection's network address. The sealing is the Rust core's (`ohttp.rs`, `CLAUDE.md` §22); this
 * only moves bytes and supplies entropy. A platform's uploader sends [Sealed.request] as `message/ohttp-req` to the **relay**
 * (not the gateway) and hands the answer to [open].
 *
 * What this protects, and what it does not: the address is hidden from the gateway only while the relay and the gateway are
 * run by different parties and the relay keeps no log (`NP-INFRA-001` §3.3b). Nothing here can check that.
 */
object FleetOhttp {
    const val REQUEST_MEDIA_TYPE = "message/ohttp-req"
    const val RESPONSE_MEDIA_TYPE = "message/ohttp-res"

    /**
     * A sealed request. [enc] and the response secret are needed once, for the answer. **The secret is not here**: the
     * shared core made it and keeps it, and this holds its handle (`SWR-MG-32` (g)). Opening the answer uses the secret
     * up, right or wrong; a caller that never gets an answer calls [close], which has the core wipe it. [close] after
     * the answer was opened does nothing.
     */
    class Sealed internal constructor(val request: ByteArray, internal val enc: String, internal val secret: JsonObject) {
        fun close() {
            runCatching { NppsCore.migration(buildJsonObject { put("op", "keyRelease"); put("key", secret) }) }
        }

        override fun toString(): String = "Sealed(${request.size} bytes, <the secret is in the core>)"
    }

    /** What the gateway said the fleet service answered. */
    class Answer(val status: Int, val body: ByteArray) {
        val accepted: Boolean get() = status in 200..299
    }

    /**
     * Seal `POST <path>` to [authority] (the fleet service, as the gateway knows it) with the device token header and
     * [payload]. [keyConfigHex] is the gateway's key configuration; one that is not this build's suite is refused.
     */
    fun seal(
        keyConfigHex: String,
        authority: String,
        path: String,
        tokenHeader: String,
        token: String,
        payload: ByteArray,
        entropy: ByteArray = secureRandomBytes(32),
    ): Sealed {
        // The entropy makes the ephemeral key the response secret comes from, so it goes beside the request as bytes; the
        // secret stays in the core (`keep`), and the answer names it.
        val r = NppsCore.migrationSecret(
            buildJsonObject {
                put("op", "ohttpSeal")
                put("keep", true)
                put("keyConfig", keyConfigHex)
                put("method", "POST")
                put("scheme", "https")
                put("authority", authority)
                put("path", path)
                put(
                    "headers",
                    JsonArray(
                        listOf(
                            JsonArray(listOf(JsonPrimitive(tokenHeader), JsonPrimitive(token))),
                            JsonArray(listOf(JsonPrimitive("content-type"), JsonPrimitive("application/octet-stream"))),
                        ),
                    ),
                )
                put("body", payload.toHex())
                put("entropy", NppsCore.secretRef(0))
            },
            listOf(entropy),
        ).answer
        return Sealed(
            r["request"]!!.jsonPrimitive.content.hexToBytes(),
            r["enc"]!!.jsonPrimitive.content,
            r["secret"] as JsonObject,
        )
    }

    /**
     * The gateway's answer to [sealed]; throws [IllegalArgumentException] when it does not authenticate. Either way the
     * response secret is used up: a sealed request has one answer, and a second call for the same one is refused.
     */
    fun open(sealed: Sealed, response: ByteArray): Answer {
        val r = NppsCore.migration(
            buildJsonObject {
                put("op", "ohttpOpen")
                put("enc", sealed.enc)
                put("secret", sealed.secret)
                put("response", response.toHex())
            },
        )
        return Answer(r["status"]!!.jsonPrimitive.int, r["body"]!!.jsonPrimitive.content.hexToBytes())
    }

    /**
     * Whether the fleet service took the record, given what came back from the relay: [relayOk] its HTTP success,
     * [contentType] and [body] its answer. The relay's own status says only that it passed the message on, so an answer that
     * is not `message/ohttp-res`, or does not authenticate, is not an acceptance, and neither is the fleet service's refusal.
     */
    fun accepted(sealed: Sealed, relayOk: Boolean, contentType: String?, body: ByteArray?): Boolean {
        if (!relayOk || contentType != RESPONSE_MEDIA_TYPE || body == null) {
            // No answer to open, so nothing will use the secret up: have the core wipe it.
            sealed.close()
            return false
        }
        return try {
            open(sealed, body).accepted
        } catch (e: IllegalArgumentException) {
            false
        }
    }
}
