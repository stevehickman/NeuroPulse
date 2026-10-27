package life.neurone.core.cloud

import life.neurone.core.migration.ObjectStore
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport

/**
 * An [ObjectStore] over one Azure Blob Storage container, through the Blob service REST API with a Microsoft Entra OAuth
 * bearer token (SWR-MG-11, `docs/np_sw_cloudstore_001.md` §3.1 and §4b).
 *
 * **Authentication.** The host signs the person in and hands over a token for the resource `https://storage.azure.com/`;
 * the request carries it as `Authorization: Bearer` with a service version of 2017-11-09 or later (Microsoft's OAuth page,
 * read 2026-10-27). Shared Key and shared-access-signature authentication are **not** supported: Shared Key needs an HMAC
 * of the request (cryptography, which `CLAUDE.md` §22 gives to the Rust core) and holds the account's master key.
 *
 * **Create-if-absent** is Put Blob with `If-None-Match: *`: "perform the operation only if the resource doesn't exist, and
 * fail the operation if it does exist" (conditional-headers page, last updated 2025-11-25). The documentation does not say
 * which status an existing blob gives. The conditional-headers page says 412 for a failed condition and the error-code list
 * has `BlobAlreadyExists` as 409, so **both are read as "the blob exists"** (a 409 only when its error code is
 * `BlobAlreadyExists`). Any other status, and no answer, throws [CloudStoreException].
 *
 * Mapping otherwise: a 404 on a read is `null` and on a delete is not an error. A blob goes up in one Put Blob call, which
 * Microsoft documents as at most 5,000 MiB in recent service versions (a larger body is a 413).
 */
class AzureBlobObjectStore(
    private val transport: HttpTransport,
    account: String,
    private val container: String,
    private val token: AccessToken,
    /** How long one call may take. The host decides it (a bundle upload on a mobile link is not a claim write), so no figure is set here. */
    private val timeoutSeconds: Int,
    private val baseUrl: String = "https://$account.blob.core.windows.net",
    /** The service version sent with every call. 2025-11-05 is the version the conditional-headers page names; 2017-11-09 is the floor for OAuth. */
    private val serviceVersion: String = "2025-11-05",
) : ObjectStore {
    private val containerUrl get() = "$baseUrl/${percentEncode(container)}"

    private suspend fun send(method: String, url: String, body: ByteArray = ByteArray(0), extra: Map<String, String> = emptyMap()): HttpResponse =
        transport.call(
            HttpRequest(
                url = url,
                headers = buildMap {
                    put("Authorization", "Bearer ${token.bearer()}")
                    put("x-ms-version", serviceVersion)
                    putAll(extra)
                },
                body = body,
                method = method,
                timeoutSeconds = timeoutSeconds,
            ),
        )

    private fun blobUrl(name: String) = "$containerUrl/${percentEncodePath(name)}"

    private suspend fun upload(name: String, bytes: ByteArray, onlyIfAbsent: Boolean): HttpResponse =
        send(
            "PUT", blobUrl(name), bytes,
            buildMap {
                put("x-ms-blob-type", "BlockBlob")
                put("Content-Type", "application/octet-stream")
                if (onlyIfAbsent) put("If-None-Match", "*")
            },
        )

    override suspend fun get(name: String): ByteArray? {
        val r = send("GET", blobUrl(name))
        return when (r.status) {
            200 -> r.body
            404 -> null
            else -> throw CloudStoreException("get '$name' answered ${r.status}", r.status)
        }
    }

    override suspend fun put(name: String, bytes: ByteArray) {
        val r = upload(name, bytes, onlyIfAbsent = false)
        if (r.status != 201) throw CloudStoreException("put '$name' answered ${r.status}", r.status)
    }

    override suspend fun createIfAbsent(name: String, bytes: ByteArray): Boolean {
        val r = upload(name, bytes, onlyIfAbsent = true)
        return when {
            r.status == 201 -> true
            r.status == 412 -> false
            r.status == 409 && errorCode(r) == "BlobAlreadyExists" -> false
            else -> throw CloudStoreException("create '$name' answered ${r.status}", r.status)
        }
    }

    override suspend fun delete(name: String) {
        val r = send("DELETE", blobUrl(name))
        if (r.status != 202 && r.status != 404) throw CloudStoreException("delete '$name' answered ${r.status}", r.status)
    }

    override suspend fun list(prefix: String): List<String> {
        val names = mutableListOf<String>()
        var marker: String? = null
        do {
            val url = "$containerUrl?restype=container&comp=list&prefix=${percentEncode(prefix)}" +
                (marker?.let { "&marker=${percentEncode(it)}" } ?: "")
            val r = send("GET", url)
            if (r.status != 200) throw CloudStoreException("list '$prefix' answered ${r.status}", r.status)
            val xml = r.body.decodeToString()
            BLOB.findAll(xml).forEach { b ->
                val n = NAME.find(b.groupValues[1]) ?: throw CloudStoreException("list '$prefix' gave a blob with no name")
                val raw = xmlText(n.groupValues[2])
                names += if (n.groupValues[1].contains("Encoded=\"true\"")) percentDecode(raw) else raw
            }
            marker = NEXT_MARKER.find(xml)?.groupValues?.get(1)?.let { xmlText(it) }?.takeIf { it.isNotEmpty() }
        } while (marker != null)
        return names
    }

    /** A storage account has no quota the person's token can read through this API, so the route cannot say. */
    override suspend fun freeSpace(): Long? = null

    /** The error code of a refusal: Blob Storage sends it in the `x-ms-error-code` header and in the body's `<Code>`. */
    private fun errorCode(r: HttpResponse): String? =
        r.header("x-ms-error-code") ?: CODE.find(r.body.decodeToString())?.groupValues?.get(1)

    private companion object {
        val BLOB = Regex("<Blob>([\\s\\S]*?)</Blob>")
        val NAME = Regex("<Name((?: [^>]*)?)>([\\s\\S]*?)</Name>")
        val NEXT_MARKER = Regex("<NextMarker>([\\s\\S]*?)</NextMarker>")
        val CODE = Regex("<Code>([^<]*)</Code>")
    }
}
