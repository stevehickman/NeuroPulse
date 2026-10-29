package life.neurone.core.cloud

import life.neurone.core.migration.ObjectStore
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport

/**
 * An [ObjectStore] over one Amazon S3 bucket (or an S3-compatible store), signed with AWS Signature Version 4
 * (SWR-MG-11, `docs/np_sw_cloudstore_001.md` §3.1 and §4b).
 *
 * **Signing** is the Rust core's, reached through [signer]; this class describes each request and adds the headers that come
 * back. The payload is sent as `UNSIGNED-PAYLOAD`, which Amazon documents for S3 ("include the literal string
 * UNSIGNED-PAYLOAD ... and set the same value as the x-amz-content-sha256 header", IAM User Guide, read 2026-10-27), so a
 * bundle is not hashed twice; the bundle is already sealed and its SHA-256 is checked by the migration flow, and the
 * connection is TLS. `if-none-match` and `content-type` are **signed**, so nothing on the path can strip the condition.
 *
 * **Create-if-absent** is PutObject with `If-None-Match: *`: a write to an empty key gives 200, to an existing key **412**,
 * and the first of several concurrent conditional writes wins (S3 conditional-writes page, read 2026-10-27). A **409**
 * (`ConditionalRequestConflict`, a concurrent delete finished first) is neither "created" nor "exists", so it **throws**;
 * S3 says a PutObject may be retried, and the migration flow's retry is the person's. **An S3-compatible store that ignores
 * `If-None-Match` answers 200 and overwrites**, which this class cannot see: the capability check (`SWR-MG-12`) is what
 * catches it, and only AWS S3 is documented here to honour the header.
 *
 * Reads: 404 is `null`. Note that S3 answers a missing key with **403** unless the credentials may list the bucket; that is
 * read as a refusal and throws. A delete is 204 (S3 gives 204 for a missing key too).
 *
 * Addressing is virtual-hosted (`bucket.s3.region.amazonaws.com`) unless [pathStyle] is set, which uses
 * `s3.region.amazonaws.com/bucket` (needed for a bucket name with dots, whose TLS name does not match) or [endpointHost] for
 * an S3-compatible store.
 */
class S3ObjectStore(
    private val transport: HttpTransport,
    private val bucket: String,
    private val region: String,
    private val credentials: AwsCredentialsProvider,
    private val signer: AwsSigV4,
    /** The time as `YYYYMMDDTHHMMSSZ` in UTC. The host has the clock; the signing core has none. */
    private val amzDate: () -> String,
    /** How long one call may take. The host decides it (a bundle upload on a mobile link is not a claim write), so no figure is set here. */
    private val timeoutSeconds: Int,
    private val pathStyle: Boolean = false,
    endpointHost: String? = null,
    /**
     * The header and value that make a PutObject "only if the key is absent". Amazon S3 takes `If-None-Match: *`. **Google Cloud Storage does not**: its
     * reference lists `If-None-Match` for GET and HEAD only, and takes `x-goog-if-generation-match: 0` for "only if the object does not exist" (answering
     * 412 otherwise), so a store at `storage.googleapis.com` is given [GOOGLE_CREATE_CONDITION] and would otherwise overwrite and answer "created".
     */
    private val createCondition: Pair<String, String> = S3_CREATE_CONDITION,
) : ObjectStore {
    private val host: String = endpointHost ?: if (pathStyle) "s3.$region.amazonaws.com" else "$bucket.s3.$region.amazonaws.com"
    private val usePathStyle = pathStyle || endpointHost != null

    private fun path(key: String?): String {
        val k = key?.let { percentEncodePath(it) }
        return if (usePathStyle) "/${percentEncode(bucket)}" + (k?.let { "/$it" } ?: "/") else "/" + (k ?: "")
    }

    private suspend fun send(
        method: String,
        key: String?,
        query: List<Pair<String, String>> = emptyList(),
        body: ByteArray = ByteArray(0),
        signedExtra: Map<String, String> = emptyMap(),
    ): HttpResponse {
        val p = path(key)
        val signed = signer.sign(
            AwsSigningRequest(
                method = method, host = host, path = p, query = query, headers = signedExtra,
                payloadHash = UNSIGNED, region = region, service = "s3", credentials = credentials.credentials(), amzDate = amzDate(),
            ),
        )
        val qs = if (query.isEmpty()) "" else "?" + query.joinToString("&") { (n, v) -> "${percentEncode(n)}=${percentEncode(v)}" }
        return transport.call(
            HttpRequest(url = "https://$host$p$qs", headers = signedExtra + signed, body = body, method = method, timeoutSeconds = timeoutSeconds),
        )
    }

    override suspend fun get(name: String): ByteArray? {
        val r = send("GET", name)
        return when (r.status) {
            200 -> r.body
            404 -> null
            else -> throw CloudStoreException("get '$name' answered ${r.status}", r.status)
        }
    }

    override suspend fun put(name: String, bytes: ByteArray) {
        val r = send("PUT", name, body = bytes, signedExtra = mapOf("content-type" to OCTET_STREAM))
        if (r.status != 200) throw CloudStoreException("put '$name' answered ${r.status}", r.status)
    }

    override suspend fun createIfAbsent(name: String, bytes: ByteArray): Boolean {
        val r = send("PUT", name, body = bytes, signedExtra = mapOf("content-type" to OCTET_STREAM, createCondition.first to createCondition.second))
        return when (r.status) {
            200 -> true
            412 -> false
            else -> throw CloudStoreException("create '$name' answered ${r.status}", r.status)
        }
    }

    override suspend fun delete(name: String) {
        val r = send("DELETE", name)
        if (r.status != 204 && r.status != 200 && r.status != 404) throw CloudStoreException("delete '$name' answered ${r.status}", r.status)
    }

    override suspend fun list(prefix: String): List<String> {
        val names = mutableListOf<String>()
        var token: String? = null
        do {
            val query = buildList {
                add("list-type" to "2")
                add("prefix" to prefix)
                token?.let { add("continuation-token" to it) }
            }
            val r = send("GET", null, query)
            if (r.status != 200) throw CloudStoreException("list '$prefix' answered ${r.status}", r.status)
            val xml = r.body.decodeToString()
            CONTENTS.findAll(xml).forEach { c ->
                val k = KEY.find(c.groupValues[1]) ?: throw CloudStoreException("list '$prefix' gave an entry with no key")
                names += xmlText(k.groupValues[1])
            }
            val truncated = TRUNCATED.find(xml)?.groupValues?.get(1) == "true"
            token = if (truncated) NEXT.find(xml)?.groupValues?.get(1)?.let { xmlText(it) } else null
            if (truncated && token == null) throw CloudStoreException("list '$prefix' was truncated with no continuation token")
        } while (token != null)
        return names
    }

    /** A bucket has no quota the credentials can read through this API, so the route cannot say. */
    override suspend fun freeSpace(): Long? = null

    companion object {
        /** Amazon S3 (S3 conditional writes): create only if absent. */
        val S3_CREATE_CONDITION: Pair<String, String> = "if-none-match" to "*"

        /** Google Cloud Storage XML API (request headers reference, read 2026-10-29): create only if no live object of that name exists. */
        val GOOGLE_CREATE_CONDITION: Pair<String, String> = "x-goog-if-generation-match" to "0"

        /** The host Google documents for reaching Cloud Storage with an S3 client and HMAC keys. */
        const val GOOGLE_HOST = "storage.googleapis.com"

        private const val UNSIGNED = "UNSIGNED-PAYLOAD"
        private const val OCTET_STREAM = "application/octet-stream"
        private val CONTENTS = Regex("<Contents>([\\s\\S]*?)</Contents>")
        private val KEY = Regex("<Key>([\\s\\S]*?)</Key>")
        private val TRUNCATED = Regex("<IsTruncated>(true|false)</IsTruncated>")
        private val NEXT = Regex("<NextContinuationToken>([\\s\\S]*?)</NextContinuationToken>")
    }
}
