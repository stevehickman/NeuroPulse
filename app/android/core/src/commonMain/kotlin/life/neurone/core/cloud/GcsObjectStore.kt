package life.neurone.core.cloud

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import life.neurone.core.migration.ObjectStore
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpTransport

/**
 * An [ObjectStore] over one Google Cloud Storage bucket, through the JSON API (SWR-MG-11, `docs/np_sw_cloudstore_001.md` §3.1).
 *
 * The create-if-absent is `ifGenerationMatch=0`: the request "only proceeds if no object with the specified name exists ... If
 * there is a live version with the specified name, the request fails with a status code of 412 Precondition Failed", and the
 * object is readable as soon as the write is acknowledged (strong global consistency). Both are the provider's documentation,
 * read on 2026-10-27; neither has been run against a real bucket.
 *
 * Mapping: 412 on a create is `false` (someone else holds the claim); a 404 on a read is `null` and on a delete is not an
 * error; any other status, and no answer, throws [CloudStoreException].
 */
class GcsObjectStore(
    private val transport: HttpTransport,
    private val bucket: String,
    private val token: AccessToken,
    /** How long one call may take. The host decides it (a bundle upload on a mobile link is not a claim write), so no figure is set here. */
    private val timeoutSeconds: Int,
    private val baseUrl: String = "https://storage.googleapis.com",
) : ObjectStore {
    private val bucketPath get() = percentEncode(bucket)

    private suspend fun send(method: String, url: String, body: ByteArray = ByteArray(0), contentType: String? = null) =
        transport.call(
            HttpRequest(
                url = url,
                headers = buildMap {
                    put("Authorization", "Bearer ${token.bearer()}")
                    if (contentType != null) put("Content-Type", contentType)
                },
                body = body,
                method = method,
                timeoutSeconds = timeoutSeconds,
            ),
        )

    private fun objectUrl(name: String) = "$baseUrl/storage/v1/b/$bucketPath/o/${percentEncode(name)}"

    private fun uploadUrl(name: String, extra: String = "") =
        "$baseUrl/upload/storage/v1/b/$bucketPath/o?uploadType=media&name=${percentEncode(name)}$extra"

    override suspend fun get(name: String): ByteArray? {
        val r = send("GET", "${objectUrl(name)}?alt=media")
        return when (r.status) {
            200 -> r.body
            404 -> null
            else -> throw CloudStoreException("get '$name' answered ${r.status}", r.status)
        }
    }

    override suspend fun put(name: String, bytes: ByteArray) {
        val r = send("POST", uploadUrl(name), bytes, OCTET_STREAM)
        if (r.status != 200) throw CloudStoreException("put '$name' answered ${r.status}", r.status)
    }

    override suspend fun createIfAbsent(name: String, bytes: ByteArray): Boolean {
        val r = send("POST", uploadUrl(name, "&ifGenerationMatch=0"), bytes, OCTET_STREAM)
        return when (r.status) {
            200 -> true
            412 -> false
            else -> throw CloudStoreException("create '$name' answered ${r.status}", r.status)
        }
    }

    override suspend fun delete(name: String) {
        val r = send("DELETE", objectUrl(name))
        if (r.status != 204 && r.status != 200 && r.status != 404) throw CloudStoreException("delete '$name' answered ${r.status}", r.status)
    }

    override suspend fun list(prefix: String): List<String> {
        val names = mutableListOf<String>()
        var page: String? = null
        do {
            val url = "$baseUrl/storage/v1/b/$bucketPath/o?prefix=${percentEncode(prefix)}&fields=items(name),nextPageToken" +
                (page?.let { "&pageToken=${percentEncode(it)}" } ?: "")
            val r = send("GET", url)
            if (r.status != 200) throw CloudStoreException("list '$prefix' answered ${r.status}", r.status)
            val json = try {
                Json.parseToJsonElement(r.body.decodeToString()).jsonObject
            } catch (e: IllegalArgumentException) {
                throw CloudStoreException("list '$prefix' gave a reply that is not JSON")
            }
            names += (json["items"]?.jsonArray ?: emptyList()).map { (it as JsonObject)["name"]!!.jsonPrimitive.content }
            page = json["nextPageToken"]?.jsonPrimitive?.contentOrNull
        } while (page != null)
        return names
    }

    /** A Cloud Storage bucket has no quota the person's account can read, so the route cannot say (the check then skips the space step). */
    override suspend fun freeSpace(): Long? = null

    private companion object {
        const val OCTET_STREAM = "application/octet-stream"
    }
}
