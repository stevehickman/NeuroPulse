package life.neurone.core.analytics

/**
 * The host the app sends analytics and erasure requests to, from the build (`REQ-SW03-20`). Port of the iOS `AnalyticsHost`.
 *
 * It is a build setting and not a literal in the source, so a release build names its production domain and a test host never
 * ships by accident. Only a bare host name (and an optional port) is accepted: the app always speaks HTTPS, and a build file may
 * read `//` as a comment, so a value with a scheme could not be set there. A host that is missing, unexpanded (`$(…)`, `${…}`)
 * or not a plain host name gives null, and a caller then sends nothing: no analytics event leaves, and an erasure request stays
 * queued.
 */
object AnalyticsHost {
    private const val ALLOWED = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-"

    /** `https://<value>` if [value] is letters, digits, dots and hyphens with a dot (or `localhost`) and an optional `:port`; else null. */
    fun baseUrl(value: String?): String? {
        if (value.isNullOrEmpty()) return null
        val parts = value.split(':')
        if (parts.size > 2) return null
        val host = parts[0]
        if (host.isEmpty() || !host.all { it in ALLOWED }) return null
        if (host.startsWith('.') || host.startsWith('-') || host.endsWith('.') || host.contains("..")) return null
        if (host != "localhost" && !host.contains('.')) return null
        if (parts.size == 2) {
            val port = parts[1]
            if (port.isEmpty() || port.length > 5 || !port.all { it in '0'..'9' }) return null
            if (port.toInt() !in 1..65535) return null
        }
        return "https://$value"
    }
}

/**
 * What a host's build supplies for analytics: the host (see [AnalyticsHost]) and the vendor's project token (a public ingestion
 * token, `phc_…`, which is not a secret but is not kept in source either). Either missing means analytics is off on that build.
 */
class AnalyticsConfig(val host: String?, val projectToken: String?) {
    val baseUrl: String? get() = AnalyticsHost.baseUrl(host)
    val usable: Boolean get() = baseUrl != null && !projectToken.isNullOrBlank() && !projectToken.startsWith("$(") && !projectToken.startsWith("\${")
}
