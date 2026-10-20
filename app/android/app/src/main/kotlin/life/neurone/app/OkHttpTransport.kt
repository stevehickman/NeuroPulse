package life.neurone.app

import kotlinx.coroutines.suspendCancellableCoroutine
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import okhttp3.Call
import okhttp3.Callback
import okhttp3.CertificatePinner
import okhttp3.CookieJar
import okhttp3.MediaType.Companion.toMediaTypeOrNull
import okhttp3.OkHttpClient
import okhttp3.RequestBody.Companion.toRequestBody
import okhttp3.Response
import java.io.IOException
import java.util.concurrent.TimeUnit
import kotlin.coroutines.resume

/**
 * Android's [HttpTransport]: OkHttp, with no cookies, no cache and no redirects. A request that is not answered (no network, a
 * timeout, a certificate that did not verify) gives null.
 */
class OkHttpTransport(
    private val client: OkHttpClient = defaultClient(),
) : HttpTransport {

    override suspend fun post(request: HttpRequest): HttpResponse? = suspendCancellableCoroutine { cont ->
        val contentType = request.headers.entries.firstOrNull { it.key.equals("Content-Type", ignoreCase = true) }?.value
        val builder = okhttp3.Request.Builder()
            .url(request.url)
            .post(request.body.toRequestBody(contentType?.toMediaTypeOrNull()))
        request.headers.forEach { (k, v) -> if (!k.equals("Content-Type", ignoreCase = true)) builder.header(k, v) }
        val call = client.newCall(builder.build())
        cont.invokeOnCancellation { call.cancel() }
        call.enqueue(object : Callback {
            override fun onFailure(call: Call, e: IOException) {
                if (cont.isActive) cont.resume(null)
            }

            override fun onResponse(call: Call, response: Response) {
                val answer = response.use {
                    HttpResponse(it.code, it.headers.toMultimap().mapValues { e -> e.value.first() }, it.body?.bytes() ?: ByteArray(0))
                }
                if (cont.isActive) cont.resume(answer)
            }
        })
    }

    companion object {
        fun defaultClient(): OkHttpClient = baseClient().build()

        /** A client that refuses any connection to [host] whose leaf key (SPKI SHA-256, `sha256/<base64>`) is not among [pins]. */
        fun pinnedClient(host: String, pins: List<String>): OkHttpClient =
            baseClient().certificatePinner(CertificatePinner.Builder().apply { pins.forEach { add(host, it) } }.build()).build()

        private fun baseClient(): OkHttpClient.Builder = OkHttpClient.Builder()
            .cookieJar(CookieJar.NO_COOKIES)
            .cache(null)
            .followRedirects(false)
            .followSslRedirects(false)
            .callTimeout(30, TimeUnit.SECONDS)
    }
}
