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

private const val HEX = "0123456789ABCDEF"

/** One request through [transport]; a missing answer is a [CloudStoreException], never a null the caller could mistake for "absent". */
internal suspend fun HttpTransport.call(request: HttpRequest): HttpResponse =
    post(request) ?: throw CloudStoreException("no answer from the provider")
