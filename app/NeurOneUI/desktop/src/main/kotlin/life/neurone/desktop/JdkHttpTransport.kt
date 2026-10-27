package life.neurone.desktop

import kotlinx.coroutines.future.await
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import java.net.URI
import java.net.http.HttpClient
import java.net.http.HttpRequest as JdkRequest
import java.net.http.HttpResponse as JdkResponse
import java.time.Duration

/**
 * The desktop [HttpTransport]: the JDK's own client, with no cookie handler, no cache and no redirects. A request that is not
 * answered (no network, a timeout, a certificate that did not verify) gives null.
 */
class JdkHttpTransport(
    private val client: HttpClient = HttpClient.newBuilder()
        .followRedirects(HttpClient.Redirect.NEVER)
        .connectTimeout(Duration.ofSeconds(15))
        .build(),
) : HttpTransport {

    override suspend fun post(request: HttpRequest): HttpResponse? = try {
        val builder = JdkRequest.newBuilder(URI.create(request.url))
            .timeout(Duration.ofSeconds(request.timeoutSeconds.toLong()))
        // Any method the caller names (the cloud object stores need DELETE and others); a GET or HEAD carries no body.
        if (request.method == "GET" || request.method == "HEAD") builder.method(request.method, JdkRequest.BodyPublishers.noBody())
        else builder.method(request.method, JdkRequest.BodyPublishers.ofByteArray(request.body))
        request.headers.forEach { (k, v) -> builder.header(k, v) }
        val answer = client.sendAsync(builder.build(), JdkResponse.BodyHandlers.ofByteArray()).await()
        HttpResponse(answer.statusCode(), answer.headers().map().mapValues { it.value.firstOrNull().orEmpty() }, answer.body())
    } catch (e: java.io.IOException) {
        null
    } catch (e: IllegalArgumentException) {
        null // a URL the JDK will not take is not an answer either
    } catch (e: java.util.concurrent.CompletionException) {
        null
    }
}

/**
 * A client that accepts a server only if its certificate chain verifies as usual AND its leaf public key (SPKI SHA-256, as
 * `sha256/<base64>`) is among [pins] (`REQ-SHDR-04`). A transport made with it is for the one host the pins belong to; any other
 * host fails the pin check, which is the intended direction. The hash is the Rust core's (`CLAUDE.md` §22).
 */
fun pinnedHttpClient(pins: List<String>): HttpClient {
    val factory = javax.net.ssl.TrustManagerFactory.getInstance(javax.net.ssl.TrustManagerFactory.getDefaultAlgorithm())
    factory.init(null as java.security.KeyStore?)
    val system = factory.trustManagers.filterIsInstance<javax.net.ssl.X509TrustManager>().first()
    val pinning = object : javax.net.ssl.X509TrustManager {
        override fun checkClientTrusted(chain: Array<java.security.cert.X509Certificate>, authType: String) = system.checkClientTrusted(chain, authType)

        override fun checkServerTrusted(chain: Array<java.security.cert.X509Certificate>, authType: String) {
            system.checkServerTrusted(chain, authType)
            val leaf = chain.firstOrNull() ?: throw java.security.cert.CertificateException("no certificate")
            val pin = "sha256/" + java.util.Base64.getEncoder().encodeToString(life.neurone.core.platform.Digests.sha256(leaf.publicKey.encoded))
            if (pin !in pins) throw java.security.cert.CertificateException("the certificate's public key is not pinned")
        }

        override fun getAcceptedIssuers(): Array<java.security.cert.X509Certificate> = system.acceptedIssuers
    }
    val context = javax.net.ssl.SSLContext.getInstance("TLS").apply { init(null, arrayOf(pinning), null) }
    return HttpClient.newBuilder()
        .sslContext(context)
        .followRedirects(HttpClient.Redirect.NEVER)
        .connectTimeout(Duration.ofSeconds(15))
        .build()
}

/** The build's analytics settings on a desktop run: environment variables, or system properties for a packaged app. */
fun desktopAnalyticsConfig(): life.neurone.core.analytics.AnalyticsConfig = life.neurone.core.analytics.AnalyticsConfig(
    host = System.getProperty("neurone.analytics.host") ?: System.getenv("ANALYTICS_HOST"),
    projectToken = System.getProperty("neurone.analytics.token") ?: System.getenv("POSTHOG_PROJECT_TOKEN"),
)
