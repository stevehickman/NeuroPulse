package life.neurone.core.net

/** One HTTPS POST: the full URL, its headers and its body. */
class HttpRequest(val url: String, val headers: Map<String, String>, val body: ByteArray)

/** What came back: the status, the headers (names as the platform gave them) and the body. */
class HttpResponse(val status: Int, val headers: Map<String, String>, val body: ByteArray) {
    /** A header by name, ignoring case. */
    fun header(name: String): String? = headers.entries.firstOrNull { it.key.equals(name, ignoreCase = true) }?.value
}

/**
 * The one network call the app's common code makes: an HTTPS POST that is answered or is not. Every host supplies its own
 * (OkHttp, the JDK's client, NSURLSession, `fetch`), because each is an operating-system API; what is sent and what is done
 * with the answer are common code.
 *
 * A transport keeps no cookies, no cache and no credentials, and follows no redirect: a request that is redirected is answered
 * with the 3xx. It returns `null` when there was no answer (no network, a timeout, a certificate that did not verify, a pin that
 * did not match), which callers treat as "try again later", never as a refusal.
 *
 * A transport made with certificate pins refuses a connection whose leaf public key (SPKI SHA-256, as `sha256/<base64>`) is
 * not among them, so a pinned transport is made for one host and used for that host only.
 */
interface HttpTransport {
    suspend fun post(request: HttpRequest): HttpResponse?
}
