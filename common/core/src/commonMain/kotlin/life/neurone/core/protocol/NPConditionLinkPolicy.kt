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
     * Loopback, link-local, mDNS, RFC1918, CGNAT, multicast, and single-label
     * intranet hosts. A crafted registry must not be able to aim the system
     * browser at a service bound to the user's own device or LAN.
     *
     * A numeric host is accepted only in canonical dotted-quad form. Browsers
     * also read `127.1`, `0x7f.0.0.1`, `0177.0.0.1` and `2130706433` as
     * 127.0.0.1 (the WHATWG IPv4 parser), so any host whose last label is a
     * number and which is not a canonical dotted quad is refused rather than
     * re-parsed: it cannot be shown to be public. IPv6 literals are parsed to
     * their eight groups, so the IPv4 address inside a mapped, compatible,
     * NAT64 or 6to4 literal is classified as IPv4. A literal that does not
     * parse, or carries a zone identifier, is refused (NP-PRIV-ANALYSIS-007
     * L14, `OI-PRIV-66`).
     */
    fun isLocalOrPrivateHost(hostname: String): Boolean {
        val host = hostname.lowercase()

        if (host.contains(':')) {
            val groups = parseIPv6(host) ?: return true
            return !isPublicIPv6(groups)
        }

        // One trailing dot is the DNS root and names the same host.
        val name = if (host.endsWith('.')) host.dropLast(1) else host
        if (name.isEmpty()) return true
        val labels = name.split('.')
        if (labels.any { it.isEmpty() }) return true

        if (name == "localhost" || name.endsWith(".localhost")) return true
        if (name.endsWith(".local")) return true

        if (isNumericLabel(labels.last())) {
            val octets = parseCanonicalIPv4(labels) ?: return true
            return isPrivateIPv4(octets)
        }

        // A single-label name like `wiki` or `intranet` is not a public FQDN; it
        // resolves through the device's DNS search suffix onto the LAN. Public
        // condition links are always dotted.
        return labels.size == 1
    }

    /** All decimal digits, or `0x` and hex digits: a label a browser reads as a number. */
    private fun isNumericLabel(label: String): Boolean {
        if (label.startsWith("0x")) return label.drop(2).all { isHexDigit(it) }
        return label.all { isAsciiDigit(it) }
    }

    private fun isHexDigit(c: Char) = isAsciiDigit(c) || c in 'a'..'f'

    /** Exactly four decimal octets, 0–255, with no leading zero. */
    private fun parseCanonicalIPv4(labels: List<String>): IntArray? {
        if (labels.size != 4) return null
        val octets = IntArray(4)
        for ((i, part) in labels.withIndex()) {
            if (part.isEmpty() || part.length > 3 || !part.all { isAsciiDigit(it) }) return null
            if (part.length > 1 && part[0] == '0') return null
            val n = part.toInt()
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
        if (a == 100 && b in 64..127) return true      // RFC6598 shared address space (CGNAT)
        if (a == 192 && b == 168) return true          // RFC1918 192.168/16
        if (a == 172 && b in 16..31) return true       // RFC1918 172.16/12
        if (a == 169 && b == 254) return true          // link-local
        if (a >= 224) return true                      // multicast, reserved, broadcast
        return false
    }

    /**
     * RFC 4291 text form to eight 16-bit groups, or null. At most one `::`; a
     * dotted IPv4 tail only as the last element; no zone identifier.
     */
    private fun parseIPv6(host: String): IntArray? {
        if (host.contains('%')) return null
        val gap = host.indexOf("::")
        if (gap != -1 && host.indexOf("::", gap + 1) != -1) return null

        val head = if (gap == -1) host else host.substring(0, gap)
        val tail = if (gap == -1) "" else host.substring(gap + 2)
        val headParts = if (head.isEmpty()) emptyList() else head.split(':')
        val tailParts = if (tail.isEmpty()) emptyList() else tail.split(':')
        val lastIsHead = gap == -1

        val headGroups = ipv6Groups(headParts, dottedLastAllowed = lastIsHead) ?: return null
        val tailGroups = ipv6Groups(tailParts, dottedLastAllowed = !lastIsHead) ?: return null
        val total = headGroups.size + tailGroups.size
        if (gap == -1 && total != 8) return null
        if (gap != -1 && total > 7) return null

        val groups = IntArray(8)
        headGroups.forEachIndexed { i, g -> groups[i] = g }
        tailGroups.forEachIndexed { i, g -> groups[8 - tailGroups.size + i] = g }
        return groups
    }

    private fun ipv6Groups(parts: List<String>, dottedLastAllowed: Boolean): List<Int>? {
        val out = mutableListOf<Int>()
        for ((i, part) in parts.withIndex()) {
            if (part.contains('.')) {
                if (!dottedLastAllowed || i != parts.size - 1) return null
                val o = parseCanonicalIPv4(part.split('.')) ?: return null
                out.add((o[0] shl 8) or o[1])
                out.add((o[2] shl 8) or o[3])
            } else {
                if (part.isEmpty() || part.length > 4 || !part.all { isHexDigit(it) }) return null
                out.add(part.toInt(16))
            }
        }
        return out
    }

    private fun embeddedIPv4(hi: Int, lo: Int) =
        intArrayOf(hi shr 8, hi and 0xff, lo shr 8, lo and 0xff)

    private fun isPublicIPv6(g: IntArray): Boolean {
        val zeroTo = { n: Int -> (0 until n).all { g[it] == 0 } }
        // ::, ::1, and the deprecated IPv4-compatible ::a.b.c.d.
        if (zeroTo(6)) return false
        // ::ffff:a.b.c.d (IPv4-mapped) and ::ffff:0:a.b.c.d (IPv4-translated).
        if (zeroTo(5) && g[5] == 0xffff) return !isPrivateIPv4(embeddedIPv4(g[6], g[7]))
        if (zeroTo(4) && g[4] == 0xffff && g[5] == 0) return !isPrivateIPv4(embeddedIPv4(g[6], g[7]))
        // 64:ff9b::/96 NAT64 well-known prefix; 64:ff9b:1::/48 is local-use.
        if (g[0] == 0x64 && g[1] == 0xff9b) {
            if (g[2] == 1) return false
            if (g[2] == 0 && g[3] == 0 && g[4] == 0 && g[5] == 0) {
                return !isPrivateIPv4(embeddedIPv4(g[6], g[7]))
            }
        }
        // 2002::/16 6to4 carries an IPv4 address in groups 1–2.
        if (g[0] == 0x2002) return !isPrivateIPv4(embeddedIPv4(g[1], g[2]))
        if ((g[0] and 0xff80) == 0xfe80) return false  // fe80::/10 link-local, fec0::/10 site-local
        if ((g[0] and 0xfe00) == 0xfc00) return false  // fc00::/7 unique-local
        if ((g[0] and 0xff00) == 0xff00) return false  // ff00::/8 multicast
        return true
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
