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
        // The secret and the session token are strings when the credential store hands them over. From here they are
        // bytes beside the request, never text in it (SWR-MG-32); the copies made here are overwritten when the call ends.
        val secrets = listOfNotNull(request.credentials.secretAccessKey.encodeToByteArray(), request.credentials.sessionToken?.encodeToByteArray())
        val answer = try { NppsCore.migrationSecret(
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
                put("secretAccessKey", NppsCore.secretRef(0))
                if (secrets.size > 1) put("sessionToken", NppsCore.secretRef(1))
                put("amzDate", request.amzDate)
            },
            secrets,
        ).answer } finally { secrets.forEach { it.fill(0) } }
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
