import Foundation

/// Why a condition link was accepted or refused. Raw values match
/// `npps/fixtures/condition_links.json`, the vector file shared with the
/// TypeScript, Kotlin, and C# ports.
enum NPLinkReason: String, Codable, Equatable {
    case ok
    case notAbsolute          = "not_absolute"
    case schemeNotHTTPS       = "scheme_not_https"
    case embeddedCredentials  = "embedded_credentials"
    case missingHost          = "missing_host"
    case localOrPrivateHost   = "local_or_private_host"
    case malformed

    /// Human-readable explanation, shown in place of the Open action.
    var blockedMessage: String {
        switch self {
        case .ok:                  return ""
        case .notAbsolute:         return "This condition has no full web address."
        case .schemeNotHTTPS:      return "Only secure https links can be opened."
        case .embeddedCredentials: return "This link hides its real destination."
        case .missingHost:         return "This link has no destination site."
        case .localOrPrivateHost:  return "This link points to a private address."
        case .malformed:           return "This link is malformed."
        }
    }
}

struct NPLinkVerdict: Equatable {
    let allowed: Bool
    let reason: NPLinkReason
    /// Destination host, present only when allowed. Shown in the confirmation UI.
    let host: String?
    /// Whitespace-trimmed URL to hand to the browser, present only when allowed.
    let url: String?

    static func blocked(_ reason: NPLinkReason) -> NPLinkVerdict {
        NPLinkVerdict(allowed: false, reason: reason, host: nil, url: nil)
    }
}

/// Condition external-link policy — NP-COND-LINK-001 §3.
///
/// A condition `link` originates in a `.npps` file, and `.npps` files are
/// user-authorable. Handing an arbitrary string to `openURL` is an injection
/// surface, so every link passes through `evaluate` before the UI offers it.
///
/// Scheme and authority are extracted by hand rather than by constructing a
/// `URL` first. Platform URL parsers normalise aggressively and disagree with
/// each other — WHATWG turns `https:///wiki/Stroke` into host `wiki`, while
/// Foundation reports an empty host — so a policy built on the parsed result
/// would be a different policy on each platform. The scan below is the same
/// five steps as the TypeScript, Kotlin, and C# ports, and all four are tested
/// against `npps/fixtures/condition_links.json`.
enum NPConditionLinkPolicy {

    static func evaluate(_ raw: String) -> NPLinkVerdict {
        let trimmed = raw.trimmingCharacters(in: .whitespacesAndNewlines)

        // 1. Control characters and spaces. Checked before any parsing: URL
        //    parsers silently strip tabs and newlines and percent-encode spaces.
        if hasControlOrSpace(trimmed) { return .blocked(.malformed) }

        // 2. Scheme must be present, and 3. must be exactly https. The scheme
        //    carries the whole attack surface: javascript: and data: execute,
        //    file: reads local disk, custom schemes deep-link into other apps.
        guard let scheme = leadingScheme(trimmed) else { return .blocked(.notAbsolute) }
        guard scheme.lowercased() == "https" else { return .blocked(.schemeNotHTTPS) }

        // 4. https must be followed by an authority. `https:wiki/Stroke` is a
        //    well-formed URL with no host to show or to reach.
        let afterScheme = trimmed.dropFirst(scheme.count + 1)
        guard afterScheme.hasPrefix("//") else { return .blocked(.notAbsolute) }

        // 5. Authority runs to the first path, query, or fragment delimiter.
        let authority = afterScheme.dropFirst(2).prefix { $0 != "/" && $0 != "?" && $0 != "#" }
        if authority.isEmpty { return .blocked(.missingHost) }

        // 6. `https://en.wikipedia.org@evil.example.com/...` reads as Wikipedia
        //    to a human but resolves to evil.example.com. Reject before showing
        //    any host at all.
        if authority.contains("@") { return .blocked(.embeddedCredentials) }

        let host = hostFromAuthority(String(authority))
        if host.isEmpty { return .blocked(.missingHost) }
        if isLocalOrPrivateHost(host) { return .blocked(.localOrPrivateHost) }

        return NPLinkVerdict(
            allowed: true,
            reason: .ok,
            host: host.lowercased(),
            url: trimmed
        )
    }

    /// Any ASCII control character or space, anywhere in the string.
    private static func hasControlOrSpace(_ s: String) -> Bool {
        s.unicodeScalars.contains { $0.value <= 0x20 || $0.value == 0x7F }
    }

    /// RFC 3986 scheme: ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ) followed by ":".
    private static func leadingScheme(_ s: String) -> String? {
        var scheme = ""
        for ch in s {
            if ch == ":" { return scheme.isEmpty ? nil : scheme }
            if scheme.isEmpty {
                guard ch.isLetter, ch.isASCII else { return nil }
            } else {
                guard ch.isASCII, ch.isLetter || ch.isNumber || ch == "+" || ch == "-" || ch == "." else {
                    return nil
                }
            }
            scheme.append(ch)
        }
        return nil
    }

    /// Strip the port, and unwrap an IPv6 literal's brackets.
    private static func hostFromAuthority(_ authority: String) -> String {
        if authority.hasPrefix("[") {
            guard let close = authority.firstIndex(of: "]") else { return "" }
            return String(authority[authority.index(after: authority.startIndex)..<close])
        }
        if let colon = authority.firstIndex(of: ":") {
            return String(authority[authority.startIndex..<colon])
        }
        return authority
    }

    /// Loopback, link-local, mDNS, RFC1918, and single-label intranet hosts. A
    /// crafted registry must not be able to aim the system browser at a service
    /// bound to the user's own machine or LAN.
    static func isLocalOrPrivateHost(_ hostname: String) -> Bool {
        let host = hostname.lowercased()

        if host == "localhost" || host.hasSuffix(".localhost") { return true }
        if host.hasSuffix(".local") { return true }

        if host.contains(":") { return isPrivateIPv6(host) }

        if let octets = parseIPv4(host) { return isPrivateIPv4(octets) }

        // A single-label name like `wiki` or `intranet` is not a public FQDN; it
        // resolves through the host's DNS search suffix onto the LAN. Public
        // condition links are always dotted.
        return !host.contains(".")
    }

    private static func isPrivateIPv6(_ host: String) -> Bool {
        if host == "::1" || host == "::" { return true }
        // fe80::/10 link-local, fc00::/7 unique-local.
        return host.hasPrefix("fe80:") || host.hasPrefix("fc") || host.hasPrefix("fd")
    }

    private static func parseIPv4(_ host: String) -> [Int]? {
        let parts = host.split(separator: ".", omittingEmptySubsequences: false)
        guard parts.count == 4 else { return nil }

        var octets: [Int] = []
        for part in parts {
            guard !part.isEmpty, part.count <= 3, part.allSatisfy({ $0.isASCII && $0.isNumber }),
                  let n = Int(part), n <= 255 else { return nil }
            octets.append(n)
        }
        return octets
    }

    private static func isPrivateIPv4(_ o: [Int]) -> Bool {
        let a = o[0], b = o[1]
        if a == 127 || a == 0 { return true }             // loopback, "this network"
        if a == 10 { return true }                        // RFC1918 10/8
        if a == 192 && b == 168 { return true }           // RFC1918 192.168/16
        if a == 172 && (16...31).contains(b) { return true } // RFC1918 172.16/12
        if a == 169 && b == 254 { return true }           // link-local
        return false
    }
}
