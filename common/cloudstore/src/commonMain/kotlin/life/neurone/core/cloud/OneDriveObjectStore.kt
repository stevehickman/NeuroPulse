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
 * An [ObjectStore] over the **app folder** of the person's OneDrive (`/me/drive/special/approot`), through Microsoft Graph
 * (SWR-MG-11, `docs/np_sw_cloudstore_001.md` §3.2 and §4b). It works for a personal Microsoft account, which has no Azure
 * subscription and no bucket: the token needs the `Files.ReadWrite.AppFolder` scope (OneDrive app-folder page, read
 * 2026-10-27), and the folder is created on the first call, named after the app.
 *
 * **Create-if-absent is an upload session with `conflictBehavior: fail`**, because the documented conflict check is there
 * (createUploadSession page, read 2026-10-27): `@microsoft.graph.conflictBehavior` is `fail` (the default), `replace` or `rename`,
 * and "if a conflict occurs after the file is uploaded (for example, an item with the same name was created during the upload
 * session), an error is returned when the last byte range is uploaded", as **409 Conflict with code `nameAlreadyExists`**.
 * That 409 at creation or at completion is read as "the object exists"; a 409 with any other code, and every other status,
 * throws. The simple upload (`PUT .../content`) is not used for the claim because its page documents no conflict parameter. The
 * documentation does not say that two sessions racing for one name are arbitrated atomically; the contract test against a real
 * account (`SWR-MG-11#contract`) is what shows it.
 *
 * **Upload.** The bytes go in ranges that are a multiple of 320 KiB (327,680 bytes) and under 60 MiB, 10 MiB by default (the
 * size Microsoft recommends), so a bundle of any size is one session. The ranges go to the pre-authenticated `uploadUrl`
 * **without** an `Authorization` header (the page says including one "might result in an HTTP 401"). An object cannot be empty.
 *
 * **Download.** `GET .../content` answers **302** with a pre-authenticated URL, and [HttpTransport] follows no redirect, so
 * this class makes the second request itself, again without a token.
 *
 * **Names.** OneDrive refuses `" * : < > ? / \ |` in a name and reserves some (`.lock`, `CON`, `PRN`, `AUX`, `NUL`, `COM0-9`,
 * `LPT0-9`, `_vti_`, `desktop.ini`, a leading `~$`) (Microsoft's restrictions article, seen through search excerpts only). The
 * core's names (`np-...`) are plain, but any other name is stored with those characters, a `%`, and a leading or trailing space or
 * period written as `%XX`, and read back the same way, so the folder stays flat. The reserved words are **not** handled.
 *
 * **Delete** moves the file to the recycle bin (Microsoft's delete page). It stays there, as ciphertext, until the person empties it.
 * **Reads of a missing item are 404.** The folder also holds whatever the person puts in it; folders are skipped when listing.
 */
class OneDriveObjectStore(
    private val transport: HttpTransport,
    private val token: AccessToken,
    /** How long one call may take. The host decides it (a bundle upload on a mobile link is not a claim write), so no figure is set here. */
    private val timeoutSeconds: Int,
    private val baseUrl: String = "https://graph.microsoft.com/v1.0",
    /** The size of one range of an upload: a multiple of 320 KiB and under 60 MiB. Microsoft's recommended 10 MiB by default. */
    private val chunkBytes: Int = 10_485_760,
) : ObjectStore {
    init {
        require(chunkBytes > 0 && chunkBytes % RANGE_UNIT == 0 && chunkBytes < 60 * 1024 * 1024) {
            "an upload range is a multiple of 320 KiB and under 60 MiB"
        }
    }

    private enum class Outcome { CREATED, EXISTS }

    private suspend fun send(
        method: String,
        url: String,
        body: ByteArray = ByteArray(0),
        headers: Map<String, String> = emptyMap(),
        authorized: Boolean = true,
    ): HttpResponse =
        transport.call(
            HttpRequest(
                url = url,
                headers = if (authorized) headers + ("Authorization" to "Bearer ${token.bearer()}") else headers,
                body = body,
                method = method,
                timeoutSeconds = timeoutSeconds,
            ),
        )

    private fun itemUrl(name: String) = "$baseUrl/me/drive/special/approot:/${percentEncode(fileName(name))}"

    private fun errorCode(r: HttpResponse): String? = try {
        Json.parseToJsonElement(r.body.decodeToString()).jsonObject["error"]?.jsonObject?.get("code")?.jsonPrimitive?.contentOrNull
    } catch (e: Exception) {
        null
    }

    private suspend fun upload(name: String, bytes: ByteArray, behavior: String): Outcome {
        if (bytes.isEmpty()) throw CloudStoreException("an empty object cannot be uploaded to OneDrive")
        val body = """{"item":{"@microsoft.graph.conflictBehavior":"$behavior","name":${JsonPrimitive(fileName(name))}}}"""
        val start = send("POST", "${itemUrl(name)}:/createUploadSession", body.encodeToByteArray(), mapOf("Content-Type" to "application/json"))
        if (start.status == 409 && errorCode(start) == "nameAlreadyExists") return Outcome.EXISTS
        if (start.status != 200) throw CloudStoreException("upload '$name' could not start: ${start.status}", start.status)
        val uploadUrl = try {
            Json.parseToJsonElement(start.body.decodeToString()).jsonObject["uploadUrl"]!!.jsonPrimitive.content
        } catch (e: Exception) {
            throw CloudStoreException("upload '$name' got a reply with no uploadUrl")
        }
        var offset = 0
        while (offset < bytes.size) {
            val end = minOf(offset + chunkBytes, bytes.size)
            val last = end == bytes.size
            val r = send(
                "PUT", uploadUrl, bytes.copyOfRange(offset, end),
                mapOf("Content-Range" to "bytes $offset-${end - 1}/${bytes.size}", "Content-Type" to "application/octet-stream"),
                authorized = false,
            )
            when {
                r.status == 409 && errorCode(r) == "nameAlreadyExists" -> return Outcome.EXISTS
                last && (r.status == 200 || r.status == 201) -> return Outcome.CREATED
                !last && r.status == 202 -> offset = end
                else -> throw CloudStoreException("upload '$name' range $offset-${end - 1} answered ${r.status}", r.status)
            }
        }
        throw CloudStoreException("upload '$name' ended without a final answer")
    }

    override suspend fun get(name: String): ByteArray? {
        val r = send("GET", "${itemUrl(name)}:/content")
        return when (r.status) {
            200 -> r.body
            301, 302, 303, 307, 308 -> {
                val to = r.header("Location") ?: throw CloudStoreException("get '$name' was redirected with no Location", r.status)
                val d = send("GET", to, authorized = false)
                if (d.status != 200) throw CloudStoreException("get '$name' download answered ${d.status}", d.status)
                d.body
            }
            404 -> null
            else -> throw CloudStoreException("get '$name' answered ${r.status}", r.status)
        }
    }

    override suspend fun put(name: String, bytes: ByteArray) {
        upload(name, bytes, "replace")
    }

    override suspend fun createIfAbsent(name: String, bytes: ByteArray): Boolean = upload(name, bytes, "fail") == Outcome.CREATED

    override suspend fun delete(name: String) {
        val r = send("DELETE", itemUrl(name))
        if (r.status != 204 && r.status != 404) throw CloudStoreException("delete '$name' answered ${r.status}", r.status)
    }

    override suspend fun list(prefix: String): List<String> {
        val names = mutableListOf<String>()
        var url: String? = "$baseUrl/me/drive/special/approot/children?\$select=name,file&\$top=200"
        while (url != null) {
            val r = send("GET", url)
            if (r.status != 200) throw CloudStoreException("list '$prefix' answered ${r.status}", r.status)
            val json = try {
                Json.parseToJsonElement(r.body.decodeToString()).jsonObject
            } catch (e: Exception) {
                throw CloudStoreException("list '$prefix' gave a reply that is not JSON")
            }
            for (item in json["value"]?.jsonArray ?: emptyList()) {
                val o = item as JsonObject
                if (o["file"] == null) continue // a folder the person made
                val decoded = objectName(o["name"]!!.jsonPrimitive.content)
                if (decoded.startsWith(prefix)) names += decoded
            }
            url = json["@odata.nextLink"]?.jsonPrimitive?.contentOrNull
        }
        return names
    }

    /** The drive's `quota.remaining` (Microsoft's quota resource); null when the reply has none. */
    override suspend fun freeSpace(): Long? {
        val r = send("GET", "$baseUrl/me/drive")
        if (r.status != 200) throw CloudStoreException("quota answered ${r.status}", r.status)
        return try {
            Json.parseToJsonElement(r.body.decodeToString()).jsonObject["quota"]?.jsonObject?.get("remaining")?.jsonPrimitive?.longOrNull
        } catch (e: Exception) {
            null
        }
    }

    internal companion object {
        const val RANGE_UNIT = 327_680

        /** The file name an object is stored under: the characters OneDrive refuses, `%`, control characters, and a leading or trailing space or period as `%XX`. */
        fun fileName(name: String): String {
            val sb = StringBuilder()
            name.forEachIndexed { i, c ->
                val edge = (c == ' ' && (i == 0 || i == name.lastIndex)) || (c == '.' && i == name.lastIndex)
                if (c in "\"*:<>?/\\|%" || c.code < 0x20 || edge) {
                    for (b in c.toString().encodeToByteArray()) sb.append('%').append("0123456789ABCDEF"[(b.toInt() and 0xFF) shr 4]).append("0123456789ABCDEF"[b.toInt() and 15])
                } else {
                    sb.append(c)
                }
            }
            return sb.toString()
        }

        fun objectName(fileName: String): String = percentDecode(fileName)
    }
}
