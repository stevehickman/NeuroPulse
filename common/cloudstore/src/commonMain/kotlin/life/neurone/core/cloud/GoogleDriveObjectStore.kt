package life.neurone.core.cloud

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import life.neurone.core.migration.ObjectStore
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport

/**
 * An [ObjectStore] over the **application data folder** of the person's Google Drive (`spaces=appDataFolder`, scope `drive.appdata`),
 * through the Drive API v3 (SWR-MG-11, `docs/np_sw_cloudstore_001.md` §3.1 and §4b). The folder is hidden from the Drive interface and
 * "only accessible by your app" (app data folder guide, read 2026-10-28). The host supplies the token.
 *
 * **It cannot make the claim.** Drive addresses a file by an ID it generates, and "name ... isn't necessarily unique within a folder"
 * (files reference). `files.generateIds` returns server-made IDs, so two phones cannot both derive the one ID that would make a second
 * create fail, and the pages read show no precondition or ETag on create. [createIfAbsent] therefore **always throws** [CloudStoreException]
 * and never guesses: a "list, create, list again" imitation can answer `true` to two simultaneous callers, which is exactly what
 * `SWR-MG-07` forbids ("a route that cannot give an atomic claim is not offered for the migrate kind"). The account check
 * (`SWR-MG-12`) meets the throw at its create-if-absent step, so Drive stays listed for the UHDR-only backup (`SWR-MG-10`), which uses
 * no claim. Whether Drive accepts a client-chosen ID that did not come from `generateIds` is not stated on the pages read, and this class does
 * not depend on it.
 *
 * **Name to file.** The name is the file's `name`, found by a query in the app folder. Because names are not unique, a duplicate can exist
 * (two writers, or a write that lands before an earlier one is listed: whether a listing is current after a write is **not stated** on the
 * pages read). Reads take the **newest** by `modifiedTime`, a write updates the newest, and a delete removes every file of that name.
 *
 * **Upload** is always the resumable protocol, one code path from a claim-sized object to a bundle: a POST (create, with `parents: ["appDataFolder"]`)
 * or a PATCH (update) starts the session and answers its URI in `Location`; the bytes then go in ranges that are a multiple of 256 KiB
 * (262,144), 8 MiB by default, answered 308 until the last, which answers 200 or 201 (Drive's upload guide). The range requests carry no
 * `Authorization` header, as in the guide's own example. An empty object throws. **List** reads every file in the app folder and filters on
 * the client, because `name contains` matches whole-word prefixes, not a name prefix. **Delete** is permanent: an app-folder file cannot be trashed.
 * **Free space** is `storageQuota.limit - usage` (usage across all Google services), null when `limit` is absent (unlimited storage).
 */
class GoogleDriveObjectStore(
    private val transport: HttpTransport,
    private val token: AccessToken,
    /** How long one call may take. The host decides it, so no figure is set here. */
    private val timeoutSeconds: Int,
    private val baseUrl: String = "https://www.googleapis.com",
    /** The size of one range of an upload: a multiple of 256 KiB. 8 MiB by default. */
    private val chunkBytes: Int = 8 * 1024 * 1024,
) : ObjectStore {
    init {
        require(chunkBytes > 0 && chunkBytes % RANGE_UNIT == 0) { "an upload range is a multiple of 256 KiB" }
    }

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

    private fun json(r: HttpResponse, what: String): JsonObject = try {
        Json.parseToJsonElement(r.body.decodeToString()).jsonObject
    } catch (e: Exception) {
        throw CloudStoreException("$what gave a reply that is not JSON", r.status)
    }

    private fun quote(s: String) = "'" + s.replace("\\", "\\\\").replace("'", "\\'") + "'"

    /** The IDs of the files in the app folder with this name, newest first. */
    private suspend fun find(name: String): List<String> {
        val ids = mutableListOf<String>()
        var page: String? = null
        do {
            val q = "name = ${quote(name)} and 'appDataFolder' in parents"
            val url = "$baseUrl/drive/v3/files?spaces=appDataFolder&q=${percentEncode(q)}&orderBy=modifiedTime%20desc&pageSize=1000" +
                "&fields=${percentEncode("nextPageToken,files(id)")}" + (page?.let { "&pageToken=${percentEncode(it)}" } ?: "")
            val r = send("GET", url)
            if (r.status != 200) throw CloudStoreException("find '$name' answered ${r.status}", r.status)
            val j = json(r, "find '$name'")
            for (f in j["files"]?.jsonArray ?: emptyList()) ids += (f as JsonObject)["id"]!!.jsonPrimitive.content
            page = j["nextPageToken"]?.jsonPrimitive?.contentOrNull
        } while (page != null)
        return ids
    }

    override suspend fun get(name: String): ByteArray? {
        val id = find(name).firstOrNull() ?: return null
        val r = send("GET", "$baseUrl/drive/v3/files/${percentEncode(id)}?alt=media")
        return when (r.status) {
            200 -> r.body
            404 -> null // deleted between the find and the read
            else -> throw CloudStoreException("get '$name' answered ${r.status}", r.status)
        }
    }

    override suspend fun put(name: String, bytes: ByteArray) {
        if (bytes.isEmpty()) throw CloudStoreException("an empty object cannot be uploaded to Google Drive")
        val existing = find(name).firstOrNull()
        val start = if (existing == null) {
            val meta = """{"name":${kotlinx.serialization.json.JsonPrimitive(name)},"parents":["appDataFolder"]}"""
            send(
                "POST", "$baseUrl/upload/drive/v3/files?uploadType=resumable", meta.encodeToByteArray(),
                mapOf("Content-Type" to "application/json; charset=UTF-8", "X-Upload-Content-Length" to bytes.size.toString()),
            )
        } else {
            send(
                "PATCH", "$baseUrl/upload/drive/v3/files/${percentEncode(existing)}?uploadType=resumable", ByteArray(0),
                mapOf("X-Upload-Content-Length" to bytes.size.toString()),
            )
        }
        if (start.status != 200) throw CloudStoreException("upload '$name' could not start: ${start.status}", start.status)
        val session = start.header("Location") ?: throw CloudStoreException("upload '$name' got no session URI", start.status)
        var offset = 0
        while (offset < bytes.size) {
            val end = minOf(offset + chunkBytes, bytes.size)
            val last = end == bytes.size
            val r = send(
                "PUT", session, bytes.copyOfRange(offset, end),
                mapOf("Content-Range" to "bytes $offset-${end - 1}/${bytes.size}"),
                authorized = false,
            )
            when {
                last && (r.status == 200 || r.status == 201) -> return
                !last && r.status == 308 -> offset = end
                else -> throw CloudStoreException("upload '$name' range $offset-${end - 1} answered ${r.status}", r.status)
            }
        }
        throw CloudStoreException("upload '$name' ended without a final answer")
    }

    override suspend fun createIfAbsent(name: String, bytes: ByteArray): Boolean =
        throw CloudStoreException("Google Drive has no atomic create-if-absent, so it cannot hold a claim (SWR-MG-07); it is for the UHDR-only backup")

    override suspend fun delete(name: String) {
        for (id in find(name)) {
            val r = send("DELETE", "$baseUrl/drive/v3/files/${percentEncode(id)}")
            if (r.status != 204 && r.status != 404) throw CloudStoreException("delete '$name' answered ${r.status}", r.status)
        }
    }

    override suspend fun list(prefix: String): List<String> {
        val names = linkedSetOf<String>()
        var page: String? = null
        do {
            val url = "$baseUrl/drive/v3/files?spaces=appDataFolder&q=${percentEncode("'appDataFolder' in parents")}&pageSize=1000" +
                "&fields=${percentEncode("nextPageToken,files(name)")}" + (page?.let { "&pageToken=${percentEncode(it)}" } ?: "")
            val r = send("GET", url)
            if (r.status != 200) throw CloudStoreException("list '$prefix' answered ${r.status}", r.status)
            val j = json(r, "list '$prefix'")
            for (f in j["files"]?.jsonArray ?: emptyList()) {
                val n = (f as JsonObject)["name"]!!.jsonPrimitive.content
                if (n.startsWith(prefix)) names += n
            }
            page = j["nextPageToken"]?.jsonPrimitive?.contentOrNull
        } while (page != null)
        return names.toList()
    }

    /** `storageQuota.limit - usage` (the usage is across all Google services); null when there is no limit. */
    override suspend fun freeSpace(): Long? {
        val r = send("GET", "$baseUrl/drive/v3/about?fields=${percentEncode("storageQuota(limit,usage)")}")
        if (r.status != 200) throw CloudStoreException("quota answered ${r.status}", r.status)
        val q = json(r, "quota")["storageQuota"]?.jsonObject ?: return null
        val limit = q["limit"]?.jsonPrimitive?.contentOrNull?.toLongOrNull() ?: return null
        val usage = q["usage"]?.jsonPrimitive?.contentOrNull?.toLongOrNull() ?: return null
        return (limit - usage).coerceAtLeast(0)
    }

    internal companion object {
        const val RANGE_UNIT = 262_144
    }
}
