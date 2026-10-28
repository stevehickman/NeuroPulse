package life.neurone.core.cloud

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.longOrNull
import life.neurone.core.migration.ObjectStore
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport

/**
 * An [ObjectStore] over the **app folder** of the person's Dropbox, through the Dropbox HTTP API v2 (SWR-MG-11, `docs/np_sw_cloudstore_001.md`
 * §3.3 and §4b). An app registered with the "App folder" access type sees only `Apps/<app name>`, and its paths are relative to it (the root is `""`).
 *
 * **Written from memory of the API, not from its pages.** `www.dropbox.com` could not be reached from the session that wrote this, so none of
 * the request shapes, the error tags or the limits below were read there (`UC-116`). Two things were seen in search excerpts only: that
 * `files/upload` with `autorename` false fails on a name conflict where it would otherwise rename, and that the authorization-code flow with
 * PKCE needs no app secret. Everything else is the author's recollection, and the stand-in in the tests is written from the same recollection,
 * so **a test passing here shows the adapter agrees with itself**, not with Dropbox.
 *
 * **Create-if-absent** is `files/upload` with `mode: add` and `autorename: false`: the name is taken, the call fails with HTTP 409 and an
 * `error_summary` that begins `path/conflict`, which is read as "the object exists". Any other 409, and every other status, throws. Whether two
 * uploads racing for one name are arbitrated atomically is not something this class knows; the contract test against a real account
 * (`SWR-MG-11#contract`) is what would show it, and no route built on this class promises an exclusive create.
 *
 * **Upload** is one call, up to 150 MB (the limit as recalled); a larger object is refused here rather than half-sent, because the session-based
 * upload for larger files is not built. **Download** is `files/download`, whose arguments are in the `Dropbox-API-Arg` header; a missing
 * file is a 409 with `path/not_found`, read as null. The header is JSON and Dropbox wants non-ASCII characters in it written `\uXXXX`.
 * **Delete** is `files/delete_v2`; a missing file is not an error. A deleted file stays recoverable by the person for a while, as ciphertext.
 * **List** is `files/list_folder` followed by `files/list_folder/continue` while `has_more`; folders are skipped.
 * **Free space** is `users/get_space_usage` (allocated minus used), null when the reply gives no allocation.
 *
 * **Names.** Dropbox is case-insensitive and refuses some characters in a name; the characters OneDrive refuses (and `%`) are written `%XX`
 * here as well ([OneDriveObjectStore.fileName]), which is stricter than Dropbox needs, not looser. The reserved-word list is not handled.
 */
class DropboxObjectStore(
    private val transport: HttpTransport,
    private val token: AccessToken,
    /** How long one call may take. The host decides it (a bundle upload on a mobile link is not a claim write), so no figure is set here. */
    private val timeoutSeconds: Int,
    private val apiUrl: String = "https://api.dropboxapi.com/2",
    private val contentUrl: String = "https://content.dropboxapi.com/2",
) : ObjectStore {
    private enum class Outcome { CREATED, EXISTS }

    private suspend fun send(method: String, url: String, headers: Map<String, String>, body: ByteArray = ByteArray(0)): HttpResponse =
        transport.call(
            HttpRequest(
                url = url,
                headers = headers + ("Authorization" to "Bearer ${token.bearer()}"),
                body = body,
                method = method,
                timeoutSeconds = timeoutSeconds,
            ),
        )

    private suspend fun rpc(path: String, json: String): HttpResponse =
        send("POST", "$apiUrl/$path", mapOf("Content-Type" to "application/json"), json.encodeToByteArray())

    private fun path(name: String) = "/" + OneDriveObjectStore.fileName(name)

    /** The `Dropbox-API-Arg` header value: JSON with every non-ASCII character written as an escape, which the header must be. */
    private fun arg(vararg fields: Pair<String, Any>): String = buildString {
        append('{')
        fields.forEachIndexed { i, (k, v) ->
            if (i > 0) append(',')
            append(asciiJson(k)).append(':')
            append(if (v is String) asciiJson(v) else v.toString())
        }
        append('}')
    }

    private fun asciiJson(s: String): String = buildString {
        append('"')
        for (c in s) {
            when {
                c == '"' -> append("\\\"")
                c == '\\' -> append("\\\\")
                c.code < 0x20 || c.code > 0x7E -> append("\\u").append(c.code.toString(16).padStart(4, '0'))
                else -> append(c)
            }
        }
        append('"')
    }

    /** The reply's `error_summary`, which names the error as a path of tags (`path/conflict/file/...`), or null. */
    private fun errorSummary(r: HttpResponse): String? = try {
        Json.parseToJsonElement(r.body.decodeToString()).jsonObject["error_summary"]?.jsonPrimitive?.contentOrNull
    } catch (e: Exception) {
        null
    }

    private suspend fun upload(name: String, bytes: ByteArray, mode: String): Outcome {
        if (bytes.size > MAX_UPLOAD_BYTES) throw CloudStoreException("'$name' is larger than one Dropbox upload call takes")
        val r = send(
            "POST", "$contentUrl/files/upload",
            mapOf(
                "Content-Type" to "application/octet-stream",
                "Dropbox-API-Arg" to arg("path" to path(name), "mode" to mode, "autorename" to false, "mute" to true),
            ),
            bytes,
        )
        return when {
            r.status == 200 -> Outcome.CREATED
            r.status == 409 && errorSummary(r)?.startsWith("path/conflict") == true -> Outcome.EXISTS
            else -> throw CloudStoreException("upload '$name' answered ${r.status}", r.status)
        }
    }

    override suspend fun get(name: String): ByteArray? {
        val r = send("POST", "$contentUrl/files/download", mapOf("Dropbox-API-Arg" to arg("path" to path(name))))
        return when {
            r.status == 200 -> r.body
            r.status == 409 && errorSummary(r)?.startsWith("path/not_found") == true -> null
            else -> throw CloudStoreException("get '$name' answered ${r.status}", r.status)
        }
    }

    override suspend fun put(name: String, bytes: ByteArray) {
        upload(name, bytes, "overwrite")
    }

    override suspend fun createIfAbsent(name: String, bytes: ByteArray): Boolean = upload(name, bytes, "add") == Outcome.CREATED

    override suspend fun delete(name: String) {
        val r = rpc("files/delete_v2", """{"path":${asciiJson(path(name))}}""")
        if (r.status == 200) return
        if (r.status == 409 && errorSummary(r)?.startsWith("path_lookup/not_found") == true) return
        throw CloudStoreException("delete '$name' answered ${r.status}", r.status)
    }

    override suspend fun list(prefix: String): List<String> {
        val names = mutableListOf<String>()
        var reply = rpc("files/list_folder", """{"path":"","recursive":false,"limit":$PAGE}""")
        while (true) {
            if (reply.status != 200) throw CloudStoreException("list '$prefix' answered ${reply.status}", reply.status)
            val json = try {
                Json.parseToJsonElement(reply.body.decodeToString()).jsonObject
            } catch (e: Exception) {
                throw CloudStoreException("list '$prefix' gave a reply that is not JSON")
            }
            for (entry in json["entries"]?.jsonArray ?: emptyList()) {
                val o = entry as JsonObject
                if (o[".tag"]?.jsonPrimitive?.contentOrNull != "file") continue // a folder the person made
                val decoded = OneDriveObjectStore.objectName(o["name"]!!.jsonPrimitive.content)
                if (decoded.startsWith(prefix)) names += decoded
            }
            if (json["has_more"]?.jsonPrimitive?.contentOrNull != "true") return names
            val cursor = json["cursor"]?.jsonPrimitive?.contentOrNull ?: throw CloudStoreException("list '$prefix' has more pages and no cursor")
            reply = rpc("files/list_folder/continue", """{"cursor":${JsonPrimitive(cursor)}}""")
        }
    }

    /** Allocated minus used from `users/get_space_usage`; null when the reply has no allocation figure. */
    override suspend fun freeSpace(): Long? {
        val r = send("POST", "$apiUrl/users/get_space_usage", emptyMap())
        if (r.status != 200) throw CloudStoreException("space usage answered ${r.status}", r.status)
        return try {
            val o = Json.parseToJsonElement(r.body.decodeToString()).jsonObject
            val used = o["used"]?.jsonPrimitive?.longOrNull ?: return null
            val allocated = o["allocation"]?.jsonObject?.get("allocated")?.jsonPrimitive?.longOrNull ?: return null
            (allocated - used).coerceAtLeast(0)
        } catch (e: Exception) {
            null
        }
    }

    internal companion object {
        /** One upload call's limit as recalled (150 MB); not read from Dropbox's pages. */
        const val MAX_UPLOAD_BYTES = 150_000_000
        const val PAGE = 500
    }
}
