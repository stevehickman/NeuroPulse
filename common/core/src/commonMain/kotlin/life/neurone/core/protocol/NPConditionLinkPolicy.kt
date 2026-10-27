package life.neurone.core.protocol

/**
 * A standard condition name paired with a link to an external definition
 * (Wikipedia / MeSH / ICD-11). Mirrors the `condition` block in the NPPS
 * grammar and the iOS `NPConditionDefinition`.
 *
 * Instances come from [NPBundledConditions], generated from
 * `protocols/predefined/00-conditions.npps` by `scripts/sync-conditions.ts`.
 */
data class NPConditionDefinition(
    /** Registry key. Protocols reference a condition by this name. */
    val name: String,
    /** Stable UUID from the registry, when the entry declares one. */
    val id: String? = null,
    /** External definition URL. Never opened without passing [NPConditionLinkPolicy]. */
    val link: String,
    /** ICD-11 MMS code where one exists. */
    val code: String? = null,
    val description: String? = null,
)

/**
 * Why a condition link was accepted or refused. [wireName] values match
 * `npps/fixtures/condition_links.json`, the vector file shared with the
 * TypeScript, Swift, and C# ports.
 */
enum class NPLinkReason(val wireName: String) {
    OK("ok"),
    NOT_ABSOLUTE("not_absolute"),
    SCHEME_NOT_HTTPS("scheme_not_https"),
    EMBEDDED_CREDENTIALS("embedded_credentials"),
    MISSING_HOST("missing_host"),
    LOCAL_OR_PRIVATE_HOST("local_or_private_host"),
    MALFORMED("malformed");

    /** Human-readable explanation, shown in place of the Open action. */
    val blockedMessage: String
        get() = when (this) {
            OK -> ""
            NOT_ABSOLUTE -> "This condition has no full web address."
            SCHEME_NOT_HTTPS -> "Only secure https links can be opened."
            EMBEDDED_CREDENTIALS -> "This link hides its real destination."
            MISSING_HOST -> "This link has no destination site."
            LOCAL_OR_PRIVATE_HOST -> "This link points to a private address."
            MALFORMED -> "This link is malformed."
        }

    companion object {
        fun fromWireName(wire: String): NPLinkReason? = entries.firstOrNull { it.wireName == wire }
    }
}

data class NPLinkVerdict(
    val allowed: Boolean,
    val reason: NPLinkReason,
    /** Destination host, present only when allowed. Shown in the confirmation UI. */
    val host: String? = null,
    /** Whitespace-trimmed URL to hand to the browser, present only when allowed. */
    val url: String? = null,
) {
    companion object {
        fun blocked(reason: NPLinkReason) = NPLinkVerdict(allowed = false, reason = reason)
    }
}

/**
 * A condition name paired with the verdict on its link.
 *
 * Every referenced condition produces one of these, including names absent from
 * the registry and links that fail policy — the UI lists them all, because a
 * condition is valid protocol metadata regardless of whether its link happens to
 * be openable. Only the Open action is withheld.
 */
data class NPResolvedCondition(
    val name: String,
    val definition: NPConditionDefinition?,
    val verdict: NPLinkVerdict,
)

/**
 * Condition external-link policy — NP-COND-LINK-001 §3.
 *
 * A condition `link` originates in a `.npps` file, and `.npps` files are
 * user-authorable. Handing an arbitrary string to an ACTION_VIEW intent is an
 * injection surface, so every link passes through [evaluate] before the UI
 * offers it.
 *
 * Scheme and authority are extracted by hand rather than by handing the string
 * to `java.net.URI` first. Platform URL parsers normalise aggressively and
 * disagree with each other — WHATWG turns `https:///wiki/Stroke` into host
 * `wiki`, while java.net reports an empty host — so a policy built on the parsed
 * result would be a different policy on each platform. The scan below is the
 * same five steps as the TypeScript, Swift, and C# ports, and all four are
 * tested against `npps/fixtures/condition_links.json`.
 */
object NPConditionLinkPolicy {

    fun evaluate(raw: String): NPLinkVerdict {
        val trimmed = raw.trim()

        // 1. Control characters and spaces. Checked before any parsing: URL
        //    parsers silently strip tabs and newlines and percent-encode spaces.
        if (hasControlOrSpace(trimmed)) return NPLinkVerdict.blocked(NPLinkReason.MALFORMED)

        // 2. Scheme must be present, and 3. must be exactly https. The scheme
        //    carries the whole attack surface: javascript: and data: execute,
        //    file: reads local disk, custom schemes deep-link into other apps.
        val scheme = leadingScheme(trimmed)
            ?: return NPLinkVerdict.blocked(NPLinkReason.NOT_ABSOLUTE)
        if (scheme.lowercase() != "https") {
            return NPLinkVerdict.blocked(NPLinkReason.SCHEME_NOT_HTTPS)
        }

        // 4. https must be followed by an authority. `https:wiki/Stroke` is a
        //    well-formed URL with no host to show or to reach.
        val afterScheme = trimmed.substring(scheme.length + 1)
        if (!afterScheme.startsWith("//")) {
            return NPLinkVerdict.blocked(NPLinkReason.NOT_ABSOLUTE)
        }

        // 5. Authority runs to the first path, query, or fragment delimiter.
        val authority = afterScheme.substring(2).takeWhile { it != '/' && it != '?' && it != '#' }
        if (authority.isEmpty()) return NPLinkVerdict.blocked(NPLinkReason.MISSING_HOST)

        // 6. `https://en.wikipedia.org@evil.example.com/...` reads as Wikipedia
        //    to a human but resolves to evil.example.com. Reject before showing
        //    any host at all.
        if (authority.contains('@')) {
            return NPLinkVerdict.blocked(NPLinkReason.EMBEDDED_CREDENTIALS)
        }

        val host = hostFromAuthority(authority)
        if (host.isEmpty()) return NPLinkVerdict.blocked(NPLinkReason.MISSING_HOST)
        if (isLocalOrPrivateHost(host)) {
            return NPLinkVerdict.blocked(NPLinkReason.LOCAL_OR_PRIVATE_HOST)
        }

        return NPLinkVerdict(
            allowed = true,
            reason = NPLinkReason.OK,
            host = host.lowercase(),
            url = trimmed,
        )
    }

    /** Any ASCII control character or space, anywhere in the string. */
    private fun hasControlOrSpace(s: String): Boolean =
        s.any { it.code <= 0x20 || it.code == 0x7F }

    /** RFC 3986 scheme: ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ) followed by ":". */
    private fun leadingScheme(s: String): String? {
        val sb = StringBuilder()
        for (ch in s) {
            if (ch == ':') return if (sb.isEmpty()) null else sb.toString()
            if (sb.isEmpty()) {
                if (!isAsciiLetter(ch)) return null
            } else {
                if (!isAsciiLetter(ch) && !isAsciiDigit(ch) && ch != '+' && ch != '-' && ch != '.') {
                    return null
                }
            }
            sb.append(ch)
        }
        return null
    }

    private fun isAsciiLetter(c: Char) = c in 'a'..'z' || c in 'A'..'Z'
    private fun isAsciiDigit(c: Char) = c in '0'..'9'

    /** Strip the port, and unwrap an IPv6 literal's brackets. */
    private fun hostFromAuthority(authority: String): String {
        if (authority.startsWith("[")) {
            val close = authority.indexOf(']')
            return if (close == -1) "" else authority.substring(1, close)
        }
        val colon = authority.indexOf(':')
        return if (colon == -1) authority else authority.substring(0, colon)
    }

    /**
     * Loopback, link-local, mDNS, RFC1918, and single-label intranet hosts. A
     * crafted registry must not be able to aim the system browser at a service
     * bound to the user's own device or LAN.
     */
    fun isLocalOrPrivateHost(hostname: String): Boolean {
        val host = hostname.lowercase()

        if (host == "localhost" || host.endsWith(".localhost")) return true
        if (host.endsWith(".local")) return true

        if (host.contains(':')) return isPrivateIPv6(host)

        val octets = parseIPv4(host)
        if (octets != null) return isPrivateIPv4(octets)

        // A single-label name like `wiki` or `intranet` is not a public FQDN; it
        // resolves through the device's DNS search suffix onto the LAN. Public
        // condition links are always dotted.
        return !host.contains('.')
    }

    private fun isPrivateIPv6(host: String): Boolean {
        if (host == "::1" || host == "::") return true
        // fe80::/10 link-local, fc00::/7 unique-local.
        return host.startsWith("fe80:") || host.startsWith("fc") || host.startsWith("fd")
    }

    private fun parseIPv4(host: String): IntArray? {
        val parts = host.split('.')
        if (parts.size != 4) return null

        val octets = IntArray(4)
        for ((i, part) in parts.withIndex()) {
            if (part.isEmpty() || part.length > 3 || !part.all { isAsciiDigit(it) }) return null
            val n = part.toIntOrNull() ?: return null
            if (n > 255) return null
            octets[i] = n
        }
        return octets
    }

    private fun isPrivateIPv4(o: IntArray): Boolean {
        val a = o[0]
        val b = o[1]
        if (a == 127 || a == 0) return true            // loopback, "this network"
        if (a == 10) return true                       // RFC1918 10/8
        if (a == 192 && b == 168) return true          // RFC1918 192.168/16
        if (a == 172 && b in 16..31) return true       // RFC1918 172.16/12
        if (a == 169 && b == 254) return true          // link-local
        return false
    }
}

/** Resolve protocol `conditions` names against the bundled registry. */
fun NPBundledConditions.resolve(names: List<String>): List<NPResolvedCondition> =
    names.map { name ->
        val definition = byName[name]
        NPResolvedCondition(
            name = name,
            definition = definition,
            verdict = if (definition == null) {
                NPLinkVerdict.blocked(NPLinkReason.NOT_ABSOLUTE)
            } else {
                NPConditionLinkPolicy.evaluate(definition.link)
            },
        )
    }
