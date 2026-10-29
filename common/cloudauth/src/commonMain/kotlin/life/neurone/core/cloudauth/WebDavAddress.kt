package life.neurone.core.cloudauth

import kotlin.io.encoding.Base64
import kotlin.io.encoding.ExperimentalEncodingApi
import life.neurone.core.cloud.percentDecode

/**
 * The address of a person's WebDAV folder, checked before it is kept or used (SWR-MG-24). The password goes with every call, so the address is
 * `https` only. The person owns the server, so a single-label host (`nas`) and an IPv4 address are accepted; what is refused is anything that
 * could make a request go somewhere other than the address read: a user name in the address (`user@host`), a query or fragment, an IPv6 literal,
 * a backslash or space, a `.` or `..` segment (also written percent-encoded), an encoded `/`, and a malformed `%`.
 */
object WebDavAddress {
    private val host = Regex("[A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?(\\.[A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?)*")
    private val segment = Regex("([A-Za-z0-9._~@+=,-]|%[0-9A-Fa-f]{2})+")

    /** The address normalised (lower-case scheme and host, a trailing `/`), or null when it is not acceptable. */
    fun parse(input: String): String? {
        val text = input.trim()
        if (!text.startsWith("https://", ignoreCase = true)) return null
        val rest = text.substring("https://".length)
        val authority = rest.substringBefore('/')
        val path = if ('/' in rest) rest.substring(authority.length) else "/"
        val hostPart = authority.substringBeforeLast(':', authority)
        val port = if (':' in authority) authority.substringAfterLast(':') else null
        if (!host.matches(hostPart)) return null
        if (port != null && (port.isEmpty() || port.length > 5 || !port.all { it in '0'..'9' } || port.toInt() !in 1..65535)) return null
        val segments = path.trim('/').split('/').filter { it.isNotEmpty() }
        for (s in segments) {
            if (!segment.matches(s)) return null
            val decoded = percentDecode(s)
            if (decoded == "." || decoded == ".." || '/' in decoded || '\\' in decoded) return null
        }
        return "https://" + authority.lowercase() + "/" + segments.joinToString("") { "$it/" }
    }

    /** The `Authorization` header value for Basic credentials (RFC 7617): the base64 of `user:password` in UTF-8. */
    @OptIn(ExperimentalEncodingApi::class)
    fun basic(user: String, password: String): String = "Basic " + Base64.encode("$user:$password".encodeToByteArray())
}
