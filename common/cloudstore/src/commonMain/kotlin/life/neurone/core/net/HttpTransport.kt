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
 * **A transport never logs or surfaces the request URL** (SWR-MG-30 (e)), in whole or in part: not to a log, the console, a crash or analytics
 * report, a cache, or the text of anything it throws. Two providers take their credentials in the query string, by their own design (CloudKit's
 * `ckAPIToken` and `ckWebAuthToken`, an Azure shared access signature), and an upload or download address a provider hands out is often a
 * credential in itself. A request that cannot be sent, a malformed address included, is `null` like any other request with no answer, never an
 * exception that quotes it. Headers and bodies are held to the same rule; `HttpTransportChecks.theRequestUrlIsNeverLoggedOrSurfaced` is the
 * check each platform's transport runs.
 *
 * A transport made with certificate pins refuses a connection whose leaf public key (SPKI SHA-256, as `sha256/<base64>`) is
 * not among them, so a pinned transport is made for one host and used for that host only.
 */
interface HttpTransport {
    suspend fun post(request: HttpRequest): HttpResponse?
}
