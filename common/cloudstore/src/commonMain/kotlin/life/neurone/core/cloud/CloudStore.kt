package life.neurone.core.cloud

import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport

// The thin cloud adapters (SWR-MG-11, GitHub #566; docs/np_sw_cloudstore_001.md §4a, "build thin adapters"). Each adapter is
// an [life.neurone.core.migration.ObjectStore], the six calls the core's backup and restore flows ask of a route, spoken to one
// provider's HTTPS API through the app's own [HttpTransport]. They add no library: the transport is the platform's (OkHttp,
// the JDK client, NSURLSession), and sign-in is the host's, handed in as an [AccessToken].

/**
 * A provider call that did not give a definite answer: no reply at all (no network, a timeout, a pin that did not match), or a
 * status the adapter does not map. The migration flow treats a thrown call as "route unavailable" and the person retries, which
 * is also how a claim whose outcome is unknown is handled (a claim "written but not read back" is not owned, SWR-MG-07).
 */
class CloudStoreException(message: String, val status: Int? = null) : Exception(message)

/**
 * The OAuth 2.0 bearer token of the person's own account at the provider. The host obtains and refreshes it (Google sign-in,
 * Microsoft sign-in); NeurOne holds no credential of its own (SWR-MG-11). Called before every request, so a host may refresh
 * it lazily.
 */
fun interface AccessToken {
    suspend fun bearer(): String
}

/** RFC 3986 percent-encoding of one URL component, over the UTF-8 bytes. Unreserved characters stay as they are. */
internal fun percentEncode(text: String): String {
    val out = StringBuilder()
    for (b in text.encodeToByteArray()) {
        val c = b.toInt() and 0xFF
        val unreserved = (c in 'A'.code..'Z'.code) || (c in 'a'.code..'z'.code) || (c in '0'.code..'9'.code) ||
            c == '-'.code || c == '.'.code || c == '_'.code || c == '~'.code
        if (unreserved) out.append(c.toChar()) else out.append('%').append(HEX[c shr 4]).append(HEX[c and 15])
    }
    return out.toString()
}

/** Percent-encoding of a path whose `/` separators are kept (a blob name with a virtual directory), each segment encoded alone. */
internal fun percentEncodePath(path: String): String = path.split('/').joinToString("/") { percentEncode(it) }

/** The reverse of [percentEncode], over the UTF-8 bytes. A `%` not followed by two hex digits stays as it is. */
internal fun percentDecode(text: String): String {
    val bytes = ArrayList<Byte>(text.length)
    var i = 0
    while (i < text.length) {
        val c = text[i]
        val hi = if (c == '%' && i + 2 < text.length) hexValue(text[i + 1]) else -1
        val lo = if (hi >= 0) hexValue(text[i + 2]) else -1
        if (hi >= 0 && lo >= 0) {
            bytes.add(((hi shl 4) or lo).toByte())
            i += 3
        } else {
            bytes.addAll(c.toString().encodeToByteArray().toList())
            i++
        }
    }
    return bytes.toByteArray().decodeToString()
}

private fun hexValue(c: Char): Int = when (c) {
    in '0'..'9' -> c - '0'
    in 'A'..'F' -> c - 'A' + 10
    in 'a'..'f' -> c - 'a' + 10
    else -> -1
}

private const val HEX = "0123456789ABCDEF"

/** The five predefined XML entities and numeric character references, decoded (a key or blob name in a listing). */
internal fun xmlText(s: String): String =
    Regex("&(#x[0-9A-Fa-f]+|#[0-9]+|amp|lt|gt|quot|apos);").replace(s) { m ->
        when (val e = m.groupValues[1]) {
            "amp" -> "&"
            "lt" -> "<"
            "gt" -> ">"
            "quot" -> "\""
            "apos" -> "'"
            else -> {
                val cp = if (e[1] == 'x') e.substring(2).toInt(16) else e.substring(1).toInt()
                codePointToString(cp)
            }
        }
    }

private fun codePointToString(cp: Int): String =
    if (cp < 0x10000) cp.toChar().toString()
    else {
        val v = cp - 0x10000
        charArrayOf((0xD800 + (v shr 10)).toChar(), (0xDC00 + (v and 0x3FF)).toChar()).concatToString()
    }


/** One request through [transport]; a missing answer is a [CloudStoreException], never a null the caller could mistake for "absent". */
internal suspend fun HttpTransport.call(request: HttpRequest): HttpResponse =
    post(request) ?: throw CloudStoreException("no answer from the provider")
