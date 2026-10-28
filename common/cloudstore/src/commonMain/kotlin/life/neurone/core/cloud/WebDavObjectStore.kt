package life.neurone.core.cloud

import life.neurone.core.migration.ObjectStore
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport

/** The person's WebDAV location and the `Authorization` header value to send with every call. [baseUrl] is an `https` collection URL ending in `/`. */
class WebDavLogin(val baseUrl: String, val authorization: String)

/** Where [WebDavObjectStore] gets its login for each call, so a login changed in the account is used by a store built before. */
fun interface WebDavLoginProvider {
    suspend fun get(): WebDavLogin
}

/**
 * An [ObjectStore] over a **WebDAV** collection (RFC 4918) such as Nextcloud, ownCloud or a NAS: the objects are files in one folder, [folder], under
 * the collection the person gave (SWR-MG-11, SWR-MG-24, `docs/np_sw_cloudstore_001.md` §4c.12). Read against RFC 4918, RFC 7231, RFC 7232 and RFC 4331
 * on 2026-10-28; **no server has been contacted** and the stand-in in the tests is written from the same reading.
 *
 * **Calls.** `GET` reads (404 is null). `PUT` writes: "A PUT that would result in the creation of a resource without an appropriately scoped
 * parent collection MUST fail with a 409 (Conflict)" (RFC 4918 §9.7.1), so a 409 makes the store `MKCOL` the folder once and write again. `MKCOL`
 * answers 201, or 405 when the URL is already mapped (§9.3.1), which is read as "the folder is there"; a 409 there means the person's collection
 * is missing, and the store does not create intermediate collections. `DELETE` of a missing file (404) is not an error. A `PROPFIND` carries
 * the required `Depth` header (§9.1: "A client MUST submit a Depth header") and is answered 207 Multi-Status; the folder is listed with `Depth: 1`
 * and a folder not yet made lists as empty. Free space is `DAV:quota-available-bytes` (RFC 4331 §3) of the collection, null when the server gives none.
 *
 * **Create-if-absent** is `PUT` with `If-None-Match: *`. RFC 7232 §3.2 says that makes the request conditional on there being no current
 * representation and §5 that the origin server MUST NOT perform the method when the condition is false, answering 412, and RFC 7231 §4.3.4 that a PUT
 * which creates answers 201 and one that replaces answers 200 or 204. So 201 is "created" and 412 is "exists". **A 200 or 204 means the server
 * replaced a file despite the condition**, which can have destroyed another writer's claim, so it throws rather than answering either way. Whether a
 * given server evaluates the condition atomically against concurrent writers is not something the standards say; no route built on this class
 * promises an exclusive create.
 *
 * **Names.** The characters OneDrive refuses (and `%`) are written `%XX` ([OneDriveObjectStore.fileName]), and the file name is percent-encoded
 * again in the URL, so what a server sees is always a plain name.
 *
 * **Transport.** `PROPFIND` and `MKCOL` are not methods most HTTP clients name; each host's transport passes the method it is given.
 */
class WebDavObjectStore(
    private val transport: HttpTransport,
    private val login: WebDavLoginProvider,
    /** How long one call may take. The host decides it (a bundle upload on a mobile link is not a claim write), so no figure is set here. */
    private val timeoutSeconds: Int,
    private val folder: String = DEFAULT_FOLDER,
) : ObjectStore {
    init {
        require(folder.isNotEmpty() && folder.all { it.isLetterOrDigit() || it == '-' || it == '_' }) { "the folder is a plain name" }
    }

    private enum class Outcome { CREATED, EXISTS }

    private suspend fun send(method: String, url: (WebDavLogin) -> String, headers: Map<String, String> = emptyMap(), body: ByteArray = ByteArray(0)): HttpResponse {
        val l = login.get()
        return transport.call(
            HttpRequest(
                url = url(l),
                headers = headers + ("Authorization" to l.authorization),
                body = body,
                method = method,
                timeoutSeconds = timeoutSeconds,
            ),
        )
    }

    private fun base(l: WebDavLogin) = l.baseUrl.trimEnd('/') + "/"
    private fun folderUrl(l: WebDavLogin) = base(l) + folder + "/"
    private fun objectUrl(l: WebDavLogin, name: String) = folderUrl(l) + percentEncode(OneDriveObjectStore.fileName(name))

    private fun fail(what: String, r: HttpResponse): Nothing = throw CloudStoreException("$what answered ${r.status}", r.status)

    /** Make the folder. 201 made it, 405 says it is there already (RFC 4918 §9.3.1); anything else, including a missing parent (409), throws. */
    private suspend fun makeFolder() {
        val r = send("MKCOL", { folderUrl(it) })
        if (r.status != 201 && r.status != 405) fail("make folder", r)
    }

    private suspend fun upload(name: String, bytes: ByteArray, onlyIfAbsent: Boolean): Outcome {
        val headers = buildMap {
            put("Content-Type", "application/octet-stream")
            if (onlyIfAbsent) put("If-None-Match", "*")
        }
        var r = send("PUT", { objectUrl(it, name) }, headers, bytes)
        if (r.status == 409) { // the folder is missing: make it once and write again
            makeFolder()
            r = send("PUT", { objectUrl(it, name) }, headers, bytes)
        }
        return when {
            onlyIfAbsent && r.status == 201 -> Outcome.CREATED
            onlyIfAbsent && r.status == 412 -> Outcome.EXISTS
            onlyIfAbsent && (r.status == 200 || r.status == 204) ->
                throw CloudStoreException("the server replaced '$name' although the write was conditional on it not existing", r.status)
            !onlyIfAbsent && (r.status == 200 || r.status == 201 || r.status == 204) -> Outcome.CREATED
            else -> fail("upload '$name'", r)
        }
    }

    override suspend fun get(name: String): ByteArray? {
        val r = send("GET", { objectUrl(it, name) })
        return when (r.status) {
            200 -> r.body
            404 -> null
            else -> fail("get '$name'", r)
        }
    }

    override suspend fun put(name: String, bytes: ByteArray) {
        upload(name, bytes, onlyIfAbsent = false)
    }

    override suspend fun createIfAbsent(name: String, bytes: ByteArray): Boolean = upload(name, bytes, onlyIfAbsent = true) == Outcome.CREATED

    override suspend fun delete(name: String) {
        val r = send("DELETE", { objectUrl(it, name) })
        if (r.status !in setOf(200, 202, 204, 404)) fail("delete '$name'", r)
    }

    override suspend fun list(prefix: String): List<String> {
        val r = send("PROPFIND", { folderUrl(it) }, propfindHeaders("1"), LIST_BODY.encodeToByteArray())
        if (r.status == 404) return emptyList() // the folder has not been made yet
        if (r.status != 207) fail("list '$prefix'", r)
        val names = mutableListOf<String>()
        for (response in DavXml.responses(r.body.decodeToString())) {
            if (DavXml.isCollection(response)) continue // the folder itself, or one the person made
            val href = DavXml.href(response) ?: continue
            val decoded = OneDriveObjectStore.objectName(DavXml.lastSegment(href))
            if (decoded.startsWith(prefix)) names += decoded
        }
        return names
    }

    /** `DAV:quota-available-bytes` of the person's collection (RFC 4331 §3); null when the server does not give it. */
    override suspend fun freeSpace(): Long? {
        val r = send("PROPFIND", { base(it) }, propfindHeaders("0"), QUOTA_BODY.encodeToByteArray())
        if (r.status == 404) return null
        if (r.status != 207) fail("quota", r)
        return DavXml.responses(r.body.decodeToString()).firstNotNullOfOrNull { DavXml.quotaAvailable(it) }
    }

    private fun propfindHeaders(depth: String) = mapOf("Depth" to depth, "Content-Type" to "application/xml; charset=utf-8")

    internal companion object {
        const val DEFAULT_FOLDER = "neurone-backup"

        private const val LIST_BODY = """<?xml version="1.0" encoding="utf-8"?><d:propfind xmlns:d="DAV:"><d:prop><d:resourcetype/></d:prop></d:propfind>"""
        private const val QUOTA_BODY = """<?xml version="1.0" encoding="utf-8"?><d:propfind xmlns:d="DAV:"><d:prop><d:quota-available-bytes/></d:prop></d:propfind>"""
    }
}

/**
 * Just enough XML to read a 207 Multi-Status (RFC 4918 §13): the `response` elements, each one's `href`, whether its `resourcetype` holds
 * `collection`, and `quota-available-bytes`. Servers use any prefix for the `DAV:` namespace, or none, so the matching ignores the prefix. This
 * reads the text of the reply and never builds a document: no entity is expanded but the five predefined ones and numeric references, and a
 * DOCTYPE or external entity is never looked at.
 */
internal object DavXml {
    private const val P = "(?:[A-Za-z_][\\w.-]*:)?"
    private val response = Regex("<${P}response(?:\\s[^>]*)?>(.*?)</${P}response\\s*>", RegexOption.DOT_MATCHES_ALL)
    private val href = Regex("<${P}href(?:\\s[^>]*)?>\\s*(.*?)\\s*</${P}href\\s*>", RegexOption.DOT_MATCHES_ALL)
    private val collection = Regex("<${P}collection\\b")
    private val quota = Regex("<${P}quota-available-bytes(?:\\s[^>]*)?>\\s*(\\d+)\\s*</${P}quota-available-bytes\\s*>")

    fun responses(xml: String): List<String> = response.findAll(xml).map { it.groupValues[1] }.toList()

    fun href(response: String): String? = href.find(response)?.groupValues?.get(1)?.let { unescape(it) }

    fun isCollection(response: String): Boolean = collection.containsMatchIn(response)

    fun quotaAvailable(response: String): Long? = quota.find(response)?.groupValues?.get(1)?.toLongOrNull()

    /** The last path segment of an `href` (an absolute path or a full URI, RFC 4918 §8.3 and §14.7), still percent-encoded once, then decoded once. */
    fun lastSegment(href: String): String {
        val path = if ("://" in href) "/" + href.substringAfter("://").substringAfter('/', "") else href
        return percentDecode(path.trimEnd('/').substringAfterLast('/'))
    }

    private fun unescape(s: String): String {
        if ('&' !in s) return s
        return Regex("&(#x[0-9A-Fa-f]+|#[0-9]+|amp|lt|gt|quot|apos);").replace(s) { m ->
            when (val e = m.groupValues[1]) {
                "amp" -> "&"; "lt" -> "<"; "gt" -> ">"; "quot" -> "\""; "apos" -> "'"
                else -> {
                    val cp = if (e.startsWith("#x")) e.drop(2).toIntOrNull(16) else e.drop(1).toIntOrNull()
                    if (cp == null || cp !in 1..0x10FFFF) m.value else cp.toChar().takeIf { cp < 0x10000 }?.toString() ?: m.value
                }
            }
        }
    }
}
