package life.neurone.core.cloud

import kotlinx.datetime.Instant
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.npps.NppsCore

/**
 * [AwsSigV4] by the Rust core (`awsSigV4` in `common/npps-core/src/sigv4.rs`, `CLAUDE.md` §22): this only moves the request
 * description in and the headers out. It lives in `:core` because `common/cloudstore` cannot reach the core's bindings.
 */
class NppsAwsSigV4 : AwsSigV4 {
    override fun sign(request: AwsSigningRequest): Map<String, String> {
        val answer = NppsCore.migration(
            buildJsonObject {
                put("op", "awsSigV4")
                put("method", request.method)
                put("host", request.host)
                put("path", request.path)
                put("query", buildJsonArray { request.query.forEach { (n, v) -> add(buildJsonArray { add(JsonPrimitive(n)); add(JsonPrimitive(v)) }) } })
                put("headers", buildJsonObject { request.headers.forEach { (n, v) -> put(n, v) } })
                put("payloadHash", request.payloadHash)
                put("region", request.region)
                put("service", request.service)
                put("accessKeyId", request.credentials.accessKeyId)
                put("secretAccessKey", request.credentials.secretAccessKey)
                request.credentials.sessionToken?.let { put("sessionToken", it) }
                put("amzDate", request.amzDate)
            },
        )
        return answer["headers"]!!.jsonObject.mapValues { it.value.jsonPrimitive.content }
    }

    companion object {
        /** `YYYYMMDDTHHMMSSZ` in UTC, the form SigV4 signs, for the host's clock. */
        fun amzDate(now: Instant): String {
            val noFraction = now.toString().substringBefore('.').removeSuffix("Z")
            return noFraction.replace("-", "").replace(":", "") + "Z"
        }
    }
}
