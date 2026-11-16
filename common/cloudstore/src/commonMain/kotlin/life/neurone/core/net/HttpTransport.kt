package life.neurone.core.net

/**
 * One HTTPS request: the full URL, its headers and its body. [method] is `POST` unless the caller needs another: `GET` (the firmware
 * manifest and image, which carry no body), or the `DELETE` and `GET` of the cloud object stores (`life.neurone.core.cloud`); a
 * `GET` or `HEAD` carries no body. [timeoutSeconds] is how long the whole call may take, longer for an image than for a POST.
 */
class HttpRequest(
    val url: String,
    val headers: Map<String, String>,
    val body: ByteArray,
    val method: String = "POST",
    val timeoutSeconds: Int = 30,
)

/** What came back: the status, the headers (names as the platform gave them) and the body. */
class HttpResponse(val status: Int, val headers: Map<String, String>, val body: ByteArray) {
    /** A header by name, ignoring case. */
    fun header(name: String): String? = headers.entries.firstOrNull { it.key.equals(name, ignoreCase = true) }?.value
}

/**
 * The one network call the app's common code makes: an HTTPS request (a POST, or the GET of [HttpRequest.method]) that is answered or is not. Every host supplies its own
 * (OkHttp, the JDK's client, NSURLSession, `fetch`), because each is an operating-system API; what is sent and what is done
 * with the answer are common code.
 *
 * A transport keeps no cookies, no cache and no credentials, and follows no redirect: a request that is redirected is answered
 * with the 3xx. It returns `null` when there was no answer (no network, a timeout, a certificate that did not verify, a pin that
 * did not match), which callers treat as "try again later", never as a refusal.
 *
 * **A transport never logs, prints or otherwise surfaces a request's URL or headers** (`SWR-MG-28` (b)), on an answer, on a failure
 * or on an address it will not take: an address it cannot use is answered with `null` and never with an exception that could carry it.
 * Some providers put the credential in the query by design (CloudKit's `ckAPIToken` and `ckWebAuthToken`, an Azure shared access
 * signature) and a pre-authenticated upload or download address is a credential in itself, so a URL in a log, a crash report or an
 * error message is a credential there. A host must also leave its HTTP library's own request logging off.
 *
 * A transport made with certificate pins refuses a connection whose leaf public key (SPKI SHA-256, as `sha256/<base64>`) is
 * not among them, so a pinned transport is made for one host and used for that host only.
 */
interface HttpTransport {
    suspend fun post(request: HttpRequest): HttpResponse?
}
