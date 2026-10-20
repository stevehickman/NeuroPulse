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
            .timeout(Duration.ofSeconds(30))
            .POST(JdkRequest.BodyPublishers.ofByteArray(request.body))
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

/** The build's analytics settings on a desktop run: environment variables, or system properties for a packaged app. */
fun desktopAnalyticsConfig(): life.neurone.core.analytics.AnalyticsConfig = life.neurone.core.analytics.AnalyticsConfig(
    host = System.getProperty("neurone.analytics.host") ?: System.getenv("ANALYTICS_HOST"),
    projectToken = System.getProperty("neurone.analytics.token") ?: System.getenv("POSTHOG_PROJECT_TOKEN"),
)
