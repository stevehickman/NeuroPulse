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
        case .notAbsolute:         return String(localized: "UI_CONDITION_BLOCKED_NOT_ABSOLUTE")
        case .schemeNotHTTPS:      return String(localized: "UI_CONDITION_BLOCKED_NOT_HTTPS")
        case .embeddedCredentials: return String(localized: "UI_CONDITION_BLOCKED_CREDENTIALS")
        case .missingHost:         return String(localized: "UI_CONDITION_BLOCKED_MISSING_HOST")
        case .localOrPrivateHost:  return String(localized: "UI_CONDITION_BLOCKED_PRIVATE_HOST")
        case .malformed:           return String(localized: "UI_CONDITION_BLOCKED_MALFORMED")
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

    /// Loopback, link-local, mDNS, RFC1918, CGNAT, multicast, and single-label
    /// intranet hosts. A crafted registry must not be able to aim the system
    /// browser at a service bound to the user's own machine or LAN.
    ///
    /// A numeric host is accepted only in canonical dotted-quad form: browsers
    /// read `127.1`, `0x7f.0.0.1`, `0177.0.0.1` and `2130706433` as 127.0.0.1,
    /// so a host whose last label is a number and which is not a canonical quad
    /// is refused. IPv6 literals are parsed to eight groups so an embedded IPv4
    /// address is classified as IPv4; an unparseable literal or a zone
    /// identifier is refused (NP-PRIV-ANALYSIS-007 L14, OI-PRIV-66).
    static func isLocalOrPrivateHost(_ hostname: String) -> Bool {
        let host = hostname.lowercased()

        if host.contains(":") {
            guard let groups = parseIPv6(host) else { return true }
            return !isPublicIPv6(groups)
        }

        // One trailing dot is the DNS root and names the same host.
        let name = host.hasSuffix(".") ? String(host.dropLast()) : host
        if name.isEmpty { return true }
        let labels = name.split(separator: ".", omittingEmptySubsequences: false).map(String.init)
        if labels.contains(where: { $0.isEmpty }) { return true }

        if name == "localhost" || name.hasSuffix(".localhost") { return true }
        if name.hasSuffix(".local") { return true }

        if let last = labels.last, isNumericLabel(last) {
            guard let octets = parseCanonicalIPv4(labels) else { return true }
            return isPrivateIPv4(octets)
        }

        // A single-label name like `wiki` or `intranet` is not a public FQDN; it
        // resolves through the host's DNS search suffix onto the LAN. Public
        // condition links are always dotted.
        return labels.count == 1
    }

    private static func isDecimalDigits(_ s: String) -> Bool {
        s.utf8.allSatisfy { $0 >= 0x30 && $0 <= 0x39 }
    }

    private static func isHexDigits(_ s: String) -> Bool {
        s.utf8.allSatisfy { ($0 >= 0x30 && $0 <= 0x39) || ($0 >= 0x61 && $0 <= 0x66) }
    }

    /// All decimal digits, or `0x` and hex digits: a label a browser reads as a number.
    private static func isNumericLabel(_ label: String) -> Bool {
        if label.hasPrefix("0x") { return isHexDigits(String(label.dropFirst(2))) }
        return isDecimalDigits(label)
    }

    /// Exactly four decimal octets, 0–255, with no leading zero.
    private static func parseCanonicalIPv4(_ labels: [String]) -> [Int]? {
        guard labels.count == 4 else { return nil }
        var octets: [Int] = []
        for part in labels {
            guard !part.isEmpty, part.utf8.count <= 3, isDecimalDigits(part) else { return nil }
            if part.utf8.count > 1 && part.hasPrefix("0") { return nil }
            guard let n = Int(part), n <= 255 else { return nil }
            octets.append(n)
        }
        return octets
    }

    private static func isPrivateIPv4(_ o: [Int]) -> Bool {
        let a = o[0], b = o[1]
        if a == 127 || a == 0 { return true }             // loopback, "this network"
        if a == 10 { return true }                        // RFC1918 10/8
        if a == 100 && (64...127).contains(b) { return true } // RFC6598 shared address space (CGNAT)
        if a == 192 && b == 168 { return true }           // RFC1918 192.168/16
        if a == 172 && (16...31).contains(b) { return true } // RFC1918 172.16/12
        if a == 169 && b == 254 { return true }           // link-local
        if a >= 224 { return true }                       // multicast, reserved, broadcast
        return false
    }

    /// RFC 4291 text form to eight 16-bit groups, or nil. At most one `::`; a
    /// dotted IPv4 tail only as the last element; no zone identifier.
    private static func parseIPv6(_ host: String) -> [Int]? {
        if host.contains("%") { return nil }
        let head: String
        let tail: String
        let hasGap: Bool
        if let gap = host.range(of: "::") {
            if host[gap.upperBound...].contains("::") { return nil }
            // `:::` holds a second `::` starting one character into the first.
            if host[host.index(after: gap.lowerBound)...].hasPrefix("::") { return nil }
            head = String(host[..<gap.lowerBound])
            tail = String(host[gap.upperBound...])
            hasGap = true
        } else {
            head = host
            tail = ""
            hasGap = false
        }
        let headParts = head.isEmpty ? [] : head.split(separator: ":", omittingEmptySubsequences: false).map(String.init)
        let tailParts = tail.isEmpty ? [] : tail.split(separator: ":", omittingEmptySubsequences: false).map(String.init)

        guard let headGroups = ipv6Groups(headParts, dottedLastAllowed: !hasGap),
              let tailGroups = ipv6Groups(tailParts, dottedLastAllowed: hasGap) else { return nil }
        let total = headGroups.count + tailGroups.count
        if !hasGap && total != 8 { return nil }
        if hasGap && total > 7 { return nil }

        var groups = [Int](repeating: 0, count: 8)
        for (i, g) in headGroups.enumerated() { groups[i] = g }
        for (i, g) in tailGroups.enumerated() { groups[8 - tailGroups.count + i] = g }
        return groups
    }

    private static func ipv6Groups(_ parts: [String], dottedLastAllowed: Bool) -> [Int]? {
        var out: [Int] = []
        for (i, part) in parts.enumerated() {
            if part.contains(".") {
                guard dottedLastAllowed, i == parts.count - 1 else { return nil }
                let quad = part.split(separator: ".", omittingEmptySubsequences: false).map(String.init)
                guard let o = parseCanonicalIPv4(quad) else { return nil }
                out.append((o[0] << 8) | o[1])
                out.append((o[2] << 8) | o[3])
            } else {
                guard !part.isEmpty, part.utf8.count <= 4, isHexDigits(part),
                      let n = Int(part, radix: 16) else { return nil }
                out.append(n)
            }
        }
        return out
    }

    private static func embeddedIPv4(_ hi: Int, _ lo: Int) -> [Int] {
        [hi >> 8, hi & 0xff, lo >> 8, lo & 0xff]
    }

    private static func isPublicIPv6(_ g: [Int]) -> Bool {
        func zeroTo(_ n: Int) -> Bool { g[0..<n].allSatisfy { $0 == 0 } }
        // ::, ::1, and the deprecated IPv4-compatible ::a.b.c.d.
        if zeroTo(6) { return false }
        // ::ffff:a.b.c.d (IPv4-mapped) and ::ffff:0:a.b.c.d (IPv4-translated).
        if zeroTo(5) && g[5] == 0xffff { return !isPrivateIPv4(embeddedIPv4(g[6], g[7])) }
        if zeroTo(4) && g[4] == 0xffff && g[5] == 0 { return !isPrivateIPv4(embeddedIPv4(g[6], g[7])) }
        // 64:ff9b::/96 NAT64 well-known prefix; 64:ff9b:1::/48 is local-use.
        if g[0] == 0x64 && g[1] == 0xff9b {
            if g[2] == 1 { return false }
            if g[2] == 0 && g[3] == 0 && g[4] == 0 && g[5] == 0 {
                return !isPrivateIPv4(embeddedIPv4(g[6], g[7]))
            }
        }
        // 2002::/16 6to4 carries an IPv4 address in groups 1–2.
        if g[0] == 0x2002 { return !isPrivateIPv4(embeddedIPv4(g[1], g[2])) }
        if (g[0] & 0xff80) == 0xfe80 { return false }  // fe80::/10 link-local, fec0::/10 site-local
        if (g[0] & 0xfe00) == 0xfc00 { return false }  // fc00::/7 unique-local
        if (g[0] & 0xff00) == 0xff00 { return false }  // ff00::/8 multicast
        return true
    }
}
