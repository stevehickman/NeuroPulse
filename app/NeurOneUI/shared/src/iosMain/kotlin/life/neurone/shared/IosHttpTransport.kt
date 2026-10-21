package life.neurone.shared

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.coroutines.suspendCancellableCoroutine
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import kotlinx.cinterop.readBytes
import platform.CoreFoundation.CFDataGetBytePtr
import platform.CoreFoundation.CFDataGetLength
import platform.CoreFoundation.CFRelease
import platform.Foundation.NSURLAuthenticationChallenge
import platform.Foundation.NSURLAuthenticationMethodServerTrust
import platform.Foundation.NSURLCredential
import platform.Foundation.NSURLSessionAuthChallengeCancelAuthenticationChallenge
import platform.Foundation.NSURLSessionAuthChallengeDisposition
import platform.Foundation.NSURLSessionAuthChallengePerformDefaultHandling
import platform.Foundation.NSURLSessionAuthChallengeUseCredential
import platform.Foundation.credentialForTrust
import platform.Foundation.dataWithContentsOfFile
import platform.Foundation.serverTrust
import platform.Security.SecCertificateCopyKey
import platform.Security.SecKeyCopyExternalRepresentation
import platform.Security.SecTrustEvaluateWithError
import platform.Security.SecTrustGetCertificateAtIndex
import platform.Security.SecTrustRef
import platform.Foundation.HTTPBody
import platform.Foundation.NSData
import platform.Foundation.NSHTTPURLResponse
import platform.Foundation.NSMutableURLRequest
import platform.Foundation.NSURL
import platform.Foundation.NSURLRequest
import platform.Foundation.NSURLRequestReloadIgnoringLocalCacheData
import platform.Foundation.NSURLSession
import platform.Foundation.NSURLSessionConfiguration
import platform.Foundation.NSURLSessionTask
import platform.Foundation.NSURLSessionTaskDelegateProtocol
import platform.Foundation.dataTaskWithRequest
import platform.Foundation.setHTTPBody
import platform.Foundation.setHTTPMethod
import platform.Foundation.setValue
import platform.darwin.NSObject
import kotlin.coroutines.resume

/**
 * iOS's [HttpTransport]: an ephemeral `NSURLSession` (no cookies, no cache, no credential storage) that refuses redirects. A request
 * that is not answered (no network, a timeout, a certificate that did not verify) gives null.
 */
@OptIn(ExperimentalForeignApi::class)
class IosHttpTransport(
    /** When set, a connection to this host is refused unless its leaf public key (SPKI SHA-256, `sha256/<base64>`) is among [pins]. */
    private val pinnedHost: String? = null,
    private val pins: Set<String> = emptySet(),
) : HttpTransport {

    /** A redirect is answered with the 3xx and not followed; a pinned host's certificate is checked against its pins. */
    private class Delegate(private val pinnedHost: String?, private val pins: Set<String>) : NSObject(), NSURLSessionTaskDelegateProtocol {
        override fun URLSession(
            session: NSURLSession,
            task: NSURLSessionTask,
            willPerformHTTPRedirection: NSHTTPURLResponse,
            newRequest: NSURLRequest,
            completionHandler: (NSURLRequest?) -> Unit,
        ) = completionHandler(null)

        override fun URLSession(
            session: NSURLSession,
            didReceiveChallenge: NSURLAuthenticationChallenge,
            completionHandler: (NSURLSessionAuthChallengeDisposition, NSURLCredential?) -> Unit,
        ) {
            val space = didReceiveChallenge.protectionSpace
            val trust = space.serverTrust
            if (pinnedHost == null || space.host != pinnedHost || space.authenticationMethod != NSURLAuthenticationMethodServerTrust || trust == null) {
                completionHandler(NSURLSessionAuthChallengePerformDefaultHandling, null)
                return
            }
            // The operating system's trust evaluation first, then the pin on the leaf's public key.
            val trusted = SecTrustEvaluateWithError(trust, null) && leafSpkiPin(trust) in pins
            if (trusted) {
                completionHandler(NSURLSessionAuthChallengeUseCredential, NSURLCredential.credentialForTrust(trust))
            } else {
                completionHandler(NSURLSessionAuthChallengeCancelAuthenticationChallenge, null)
            }
        }
    }

    private val delegate = Delegate(pinnedHost, pins)

    private val session: NSURLSession = NSURLSession.sessionWithConfiguration(
        NSURLSessionConfiguration.ephemeralSessionConfiguration().apply {
            HTTPShouldSetCookies = false
            timeoutIntervalForRequest = 30.0
            requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData
        },
        delegate = delegate,
        delegateQueue = null,
    )

    override suspend fun post(request: HttpRequest): HttpResponse? = suspendCancellableCoroutine { cont ->
        val url = NSURL.URLWithString(request.url)
        if (url == null) {
            cont.resume(null)
            return@suspendCancellableCoroutine
        }
        val req = NSMutableURLRequest(uRL = url).apply {
            setHTTPMethod(request.method)
            if (request.method != "GET") setHTTPBody(request.body.toNSData())
            setTimeoutInterval(request.timeoutSeconds.toDouble())
            request.headers.forEach { (k, v) -> setValue(v, forHTTPHeaderField = k) }
        }
        val task = session.dataTaskWithRequest(req) { data, response, error ->
            val http = response as? NSHTTPURLResponse
            val answer = if (error != null || http == null) {
                null
            } else {
                @Suppress("UNCHECKED_CAST")
                val headers = (http.allHeaderFields as Map<Any?, Any?>).entries
                    .associate { (it.key as? String).orEmpty() to it.value.toString() }
                HttpResponse(http.statusCode.toInt(), headers, data?.toByteArray() ?: ByteArray(0))
            }
            if (cont.isActive) cont.resume(answer)
        }
        cont.invokeOnCancellation { task.cancel() }
        task.resume()
    }
}

/**
 * `sha256/<base64>` of the leaf certificate's SubjectPublicKeyInfo, or null if its key type is not one of the four a fleet certificate
 * may use (RSA 2048 or 4096, ECDSA P-256 or P-384). The system gives the raw key; the fixed DER header that precedes it in an SPKI
 * is added here, chosen by the key's size (the same headers the Swift app's pinning used). The hash is the Rust core's.
 */
@OptIn(ExperimentalForeignApi::class, kotlin.io.encoding.ExperimentalEncodingApi::class)
private fun leafSpkiPin(trust: SecTrustRef): String? {
    val certificate = SecTrustGetCertificateAtIndex(trust, 0) ?: return null
    val key = SecCertificateCopyKey(certificate) ?: return null
    try {
        val data = SecKeyCopyExternalRepresentation(key, null) ?: return null
        try {
            val raw = CFDataGetBytePtr(data)!!.readBytes(CFDataGetLength(data).toInt())
            val header = spkiHeader(raw) ?: return null
            val digest = life.neurone.core.platform.Digests.sha256(header + raw)
            return "sha256/" + kotlin.io.encoding.Base64.encode(digest)
        } finally {
            CFRelease(data)
        }
    } finally {
        CFRelease(key)
    }
}

private fun bytes(vararg v: Int): ByteArray = ByteArray(v.size) { v[it].toByte() }

private fun spkiHeader(raw: ByteArray): ByteArray? = when {
    raw.size == 65 && raw[0] == 4.toByte() -> bytes(0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00)
    raw.size == 97 && raw[0] == 4.toByte() -> bytes(0x30, 0x76, 0x30, 0x10, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x22, 0x03, 0x62, 0x00)
    raw.size == 270 && raw[0] == 0x30.toByte() -> bytes(0x30, 0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0f, 0x00)
    raw.size == 526 && raw[0] == 0x30.toByte() -> bytes(0x30, 0x82, 0x02, 0x22, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x02, 0x0f, 0x00)
    else -> null
}

/** The SHDR record the hub leaves for the app: a file in the app's Documents folder (the same convention the Swift app reads). */
@OptIn(ExperimentalForeignApi::class)
class IosShdrStaging(private val path: String) : life.neurone.core.analytics.ShdrStaging {
    override fun read(): ByteArray? = NSData.dataWithContentsOfFile(path)?.toByteArray()

    override fun clear() {
        platform.Foundation.NSFileManager.defaultManager.removeItemAtPath(path, error = null)
    }
}

/** The analytics host and project token from the host app's Info.plist (`AnalyticsHost`, `PostHogProjectToken`), set by the build. */
fun iosAnalyticsConfig(): life.neurone.core.analytics.AnalyticsConfig {
    val info = platform.Foundation.NSBundle.mainBundle.infoDictionary
    return life.neurone.core.analytics.AnalyticsConfig(
        host = info?.get("AnalyticsHost") as? String,
        projectToken = info?.get("PostHogProjectToken") as? String,
    )
}
