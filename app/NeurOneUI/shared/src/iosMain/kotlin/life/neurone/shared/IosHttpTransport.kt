package life.neurone.shared

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.coroutines.suspendCancellableCoroutine
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import platform.Foundation.HTTPBody
import platform.Foundation.NSHTTPURLResponse
import platform.Foundation.NSMutableURLRequest
import platform.Foundation.NSURL
import platform.Foundation.NSURLRequest
import platform.Foundation.NSURLRequestReloadIgnoringLocalCacheData
import platform.Foundation.NSURLSession
import platform.Foundation.NSURLSessionConfiguration
import platform.Foundation.NSURLSessionTask
import platform.Foundation.NSURLSessionTaskDelegateProtocol
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
class IosHttpTransport : HttpTransport {

    /** A redirect is answered with the 3xx and not followed. */
    private class NoRedirects : NSObject(), NSURLSessionTaskDelegateProtocol {
        override fun URLSession(
            session: NSURLSession,
            task: NSURLSessionTask,
            willPerformHTTPRedirection: NSHTTPURLResponse,
            newRequest: NSURLRequest,
            completionHandler: (NSURLRequest?) -> Unit,
        ) = completionHandler(null)
    }

    private val delegate = NoRedirects()

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
            setHTTPMethod("POST")
            setHTTPBody(request.body.toNSData())
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

/** The analytics host and project token from the host app's Info.plist (`AnalyticsHost`, `PostHogProjectToken`), set by the build. */
fun iosAnalyticsConfig(): life.neurone.core.analytics.AnalyticsConfig {
    val info = platform.Foundation.NSBundle.mainBundle.infoDictionary
    return life.neurone.core.analytics.AnalyticsConfig(
        host = info?.get("AnalyticsHost") as? String,
        projectToken = info?.get("PostHogProjectToken") as? String,
    )
}
