using System;
using System.Collections.Generic;
using System.Linq;

namespace NeurOne.Protocol;

/// <summary>
/// A standard condition name paired with a link to an external definition
/// (Wikipedia / MeSH / ICD-11). Mirrors the <c>condition</c> block in the NPPS
/// grammar and the iOS/Android <c>NPConditionDefinition</c>.
/// </summary>
/// <remarks>
/// Instances come from <see cref="NPConditionRegistry"/>, read at run time from
/// <c>protocols/predefined/00-conditions.npps</c> by <c>scripts/sync-conditions.ts</c>.
/// </remarks>
/// <param name="Name">Registry key. Protocols reference a condition by this name.</param>
/// <param name="Link">External definition URL. Never opened without passing <see cref="NPConditionLinkPolicy"/>.</param>
/// <param name="Id">Stable UUID from the registry, when the entry declares one.</param>
/// <param name="Code">ICD-11 MMS code where one exists.</param>
public record NPConditionDefinition(
    string Name,
    string Link,
    string? Id = null,
    string? Code = null,
    string? Description = null);

/// <summary>
/// Why a condition link was accepted or refused. <see cref="WireName"/> values
/// match <c>npps/fixtures/condition_links.json</c>, the vector file shared with
/// the TypeScript, Swift, and Kotlin ports.
/// </summary>
public enum NPLinkReason
{
    Ok,
    NotAbsolute,
    SchemeNotHttps,
    EmbeddedCredentials,
    MissingHost,
    LocalOrPrivateHost,
    Malformed,
}

public static class NPLinkReasonExtensions
{
    public static string WireName(this NPLinkReason reason) => reason switch
    {
        NPLinkReason.Ok                  => "ok",
        NPLinkReason.NotAbsolute         => "not_absolute",
        NPLinkReason.SchemeNotHttps      => "scheme_not_https",
        NPLinkReason.EmbeddedCredentials => "embedded_credentials",
        NPLinkReason.MissingHost         => "missing_host",
        NPLinkReason.LocalOrPrivateHost  => "local_or_private_host",
        NPLinkReason.Malformed           => "malformed",
        _ => throw new ArgumentOutOfRangeException(nameof(reason)),
    };

    /// <summary>Human-readable explanation, shown in place of the Open action.</summary>
    public static string BlockedMessage(this NPLinkReason reason) => reason switch
    {
        NPLinkReason.Ok                  => "",
        NPLinkReason.NotAbsolute         => "This condition has no full web address.",
        NPLinkReason.SchemeNotHttps      => "Only secure https links can be opened.",
        NPLinkReason.EmbeddedCredentials => "This link hides its real destination.",
        NPLinkReason.MissingHost         => "This link has no destination site.",
        NPLinkReason.LocalOrPrivateHost  => "This link points to a private address.",
        NPLinkReason.Malformed           => "This link is malformed.",
        _ => throw new ArgumentOutOfRangeException(nameof(reason)),
    };

    public static NPLinkReason? FromWireName(string wire) => wire switch
    {
        "ok"                    => NPLinkReason.Ok,
        "not_absolute"          => NPLinkReason.NotAbsolute,
        "scheme_not_https"      => NPLinkReason.SchemeNotHttps,
        "embedded_credentials"  => NPLinkReason.EmbeddedCredentials,
        "missing_host"          => NPLinkReason.MissingHost,
        "local_or_private_host" => NPLinkReason.LocalOrPrivateHost,
        "malformed"             => NPLinkReason.Malformed,
        _ => null,
    };
}

/// <param name="Host">Destination host, non-null only when allowed. Shown in the confirmation UI.</param>
/// <param name="Url">Whitespace-trimmed URL to hand to the browser, non-null only when allowed.</param>
public record NPLinkVerdict(bool Allowed, NPLinkReason Reason, string? Host = null, string? Url = null)
{
    public static NPLinkVerdict Blocked(NPLinkReason reason) => new(false, reason);
}

/// <summary>
/// A condition name paired with the verdict on its link.
/// </summary>
/// <remarks>
/// Every referenced condition produces one of these, including names absent from
/// the registry and links that fail policy — the UI lists them all, because a
/// condition is valid protocol metadata regardless of whether its link happens
/// to be openable. Only the Open action is withheld.
/// </remarks>
public record NPResolvedCondition(
    string Name,
    NPConditionDefinition? Definition,
    NPLinkVerdict Verdict);

/// <summary>
/// Condition external-link policy — NP-COND-LINK-001 §3.
/// </summary>
/// <remarks>
/// <para>
/// A condition link originates in a <c>.npps</c> file, and <c>.npps</c> files
/// are user-authorable. Handing an arbitrary string to <c>Process.Start</c> with
/// <c>UseShellExecute</c> is an injection surface — the shell will happily launch
/// a registered custom-scheme handler — so every link passes through
/// <see cref="Evaluate"/> before the UI offers it.
/// </para>
/// <para>
/// Scheme and authority are extracted by hand rather than by constructing a
/// <see cref="Uri"/> first. Platform URL parsers normalise aggressively and
/// disagree with each other — WHATWG turns <c>https:///wiki/Stroke</c> into host
/// <c>wiki</c>, while Foundation and java.net report an empty host — so a policy
/// built on the parsed result would be a different policy on each platform. The
/// scan below is the same five steps as the TypeScript, Swift, and Kotlin ports,
/// and all four are tested against <c>npps/fixtures/condition_links.json</c>.
/// </para>
/// </remarks>
public static class NPConditionLinkPolicy
{
    public static NPLinkVerdict Evaluate(string raw)
    {
        var trimmed = raw.Trim();

        // 1. Control characters and spaces. Checked before any parsing: URL
        //    parsers silently strip tabs and newlines and percent-encode spaces.
        if (HasControlOrSpace(trimmed)) return NPLinkVerdict.Blocked(NPLinkReason.Malformed);

        // 2. Scheme must be present, and 3. must be exactly https. The scheme
        //    carries the whole attack surface: javascript: and data: execute,
        //    file: reads local disk, custom schemes launch other installed apps.
        var scheme = LeadingScheme(trimmed);
        if (scheme is null) return NPLinkVerdict.Blocked(NPLinkReason.NotAbsolute);
        if (!scheme.Equals("https", StringComparison.OrdinalIgnoreCase))
        {
            return NPLinkVerdict.Blocked(NPLinkReason.SchemeNotHttps);
        }

        // 4. https must be followed by an authority. `https:wiki/Stroke` is a
        //    well-formed URL with no host to show or to reach.
        var afterScheme = trimmed[(scheme.Length + 1)..];
        if (!afterScheme.StartsWith("//", StringComparison.Ordinal))
        {
            return NPLinkVerdict.Blocked(NPLinkReason.NotAbsolute);
        }

        // 5. Authority runs to the first path, query, or fragment delimiter.
        var authority = new string(
            afterScheme[2..].TakeWhile(c => c != '/' && c != '?' && c != '#').ToArray());
        if (authority.Length == 0) return NPLinkVerdict.Blocked(NPLinkReason.MissingHost);

        // 6. `https://en.wikipedia.org@evil.example.com/...` reads as Wikipedia
        //    to a human but resolves to evil.example.com. Reject before showing
        //    any host at all.
        if (authority.Contains('@')) return NPLinkVerdict.Blocked(NPLinkReason.EmbeddedCredentials);

        var host = HostFromAuthority(authority);
        if (host.Length == 0) return NPLinkVerdict.Blocked(NPLinkReason.MissingHost);
        if (IsLocalOrPrivateHost(host)) return NPLinkVerdict.Blocked(NPLinkReason.LocalOrPrivateHost);

        return new NPLinkVerdict(true, NPLinkReason.Ok, host.ToLowerInvariant(), trimmed);
    }

    /// <summary>Any ASCII control character or space, anywhere in the string.</summary>
    private static bool HasControlOrSpace(string s) => s.Any(c => c <= 0x20 || c == 0x7F);

    /// <summary>RFC 3986 scheme: ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ) followed by ":".</summary>
    private static string? LeadingScheme(string s)
    {
        for (var i = 0; i < s.Length; i++)
        {
            var ch = s[i];
            if (ch == ':') return i == 0 ? null : s[..i];

            var valid = i == 0
                ? IsAsciiLetter(ch)
                : IsAsciiLetter(ch) || IsAsciiDigit(ch) || ch is '+' or '-' or '.';
            if (!valid) return null;
        }
        return null;
    }

    private static bool IsAsciiLetter(char c) => c is >= 'a' and <= 'z' or >= 'A' and <= 'Z';
    private static bool IsAsciiDigit(char c) => c is >= '0' and <= '9';

    /// <summary>Strip the port, and unwrap an IPv6 literal's brackets.</summary>
    private static string HostFromAuthority(string authority)
    {
        if (authority.StartsWith('['))
        {
            var close = authority.IndexOf(']');
            return close == -1 ? "" : authority[1..close];
        }
        var colon = authority.IndexOf(':');
        return colon == -1 ? authority : authority[..colon];
    }

    /// <summary>
    /// Loopback, link-local, mDNS, RFC1918, CGNAT, multicast, and single-label
    /// intranet hosts. A crafted registry must not be able to aim the system
    /// browser at a service bound to the user's own machine or LAN.
    /// </summary>
    /// <remarks>
    /// A numeric host is accepted only in canonical dotted-quad form: browsers
    /// read <c>127.1</c>, <c>0x7f.0.0.1</c>, <c>0177.0.0.1</c> and
    /// <c>2130706433</c> as 127.0.0.1, so a host whose last label is a number and
    /// which is not a canonical quad is refused. IPv6 literals are parsed to eight
    /// groups so an embedded IPv4 address is classified as IPv4; an unparseable
    /// literal or a zone identifier is refused (NP-PRIV-ANALYSIS-007 L14,
    /// OI-PRIV-66).
    /// </remarks>
    public static bool IsLocalOrPrivateHost(string hostname)
    {
        var host = hostname.ToLowerInvariant();

        if (host.Contains(':'))
        {
            var groups = ParseIPv6(host);
            return groups is null || !IsPublicIPv6(groups);
        }

        // One trailing dot is the DNS root and names the same host.
        var name = host.EndsWith('.') ? host[..^1] : host;
        if (name.Length == 0) return true;
        var labels = name.Split('.');
        if (labels.Any(l => l.Length == 0)) return true;

        if (name == "localhost" || name.EndsWith(".localhost", StringComparison.Ordinal)) return true;
        if (name.EndsWith(".local", StringComparison.Ordinal)) return true;

        if (IsNumericLabel(labels[^1]))
        {
            var octets = ParseCanonicalIPv4(labels);
            return octets is null || IsPrivateIPv4(octets);
        }

        // A single-label name like `wiki` or `intranet` is not a public FQDN; it
        // resolves through the machine's DNS search suffix onto the LAN. Public
        // condition links are always dotted.
        return labels.Length == 1;
    }

    private static bool IsHexDigit(char c) => IsAsciiDigit(c) || c is >= 'a' and <= 'f';

    /// <summary>All decimal digits, or <c>0x</c> and hex digits: a label a browser reads as a number.</summary>
    private static bool IsNumericLabel(string label) =>
        label.StartsWith("0x", StringComparison.Ordinal)
            ? label[2..].All(IsHexDigit)
            : label.All(IsAsciiDigit);

    /// <summary>Exactly four decimal octets, 0–255, with no leading zero.</summary>
    private static int[]? ParseCanonicalIPv4(string[] labels)
    {
        if (labels.Length != 4) return null;

        var octets = new int[4];
        for (var i = 0; i < 4; i++)
        {
            var part = labels[i];
            if (part.Length is 0 or > 3 || !part.All(IsAsciiDigit)) return null;
            if (part.Length > 1 && part[0] == '0') return null;
            var n = int.Parse(part, System.Globalization.CultureInfo.InvariantCulture);
            if (n > 255) return null;
            octets[i] = n;
        }
        return octets;
    }

    private static bool IsPrivateIPv4(int[] o)
    {
        int a = o[0], b = o[1];
        if (a is 127 or 0) return true;            // loopback, "this network"
        if (a == 10) return true;                  // RFC1918 10/8
        if (a == 100 && b is >= 64 and <= 127) return true; // RFC6598 shared address space (CGNAT)
        if (a == 192 && b == 168) return true;     // RFC1918 192.168/16
        if (a == 172 && b is >= 16 and <= 31) return true; // RFC1918 172.16/12
        if (a == 169 && b == 254) return true;     // link-local
        if (a >= 224) return true;                 // multicast, reserved, broadcast
        return false;
    }

    /// <summary>
    /// RFC 4291 text form to eight 16-bit groups, or null. At most one <c>::</c>;
    /// a dotted IPv4 tail only as the last element; no zone identifier.
    /// </summary>
    private static int[]? ParseIPv6(string host)
    {
        if (host.Contains('%')) return null;
        var gap = host.IndexOf("::", StringComparison.Ordinal);
        if (gap != -1 && host.IndexOf("::", gap + 1, StringComparison.Ordinal) != -1) return null;

        var head = gap == -1 ? host : host[..gap];
        var tail = gap == -1 ? "" : host[(gap + 2)..];
        var headParts = head.Length == 0 ? Array.Empty<string>() : head.Split(':');
        var tailParts = tail.Length == 0 ? Array.Empty<string>() : tail.Split(':');

        var headGroups = IPv6Groups(headParts, dottedLastAllowed: gap == -1);
        var tailGroups = IPv6Groups(tailParts, dottedLastAllowed: gap != -1);
        if (headGroups is null || tailGroups is null) return null;
        var total = headGroups.Count + tailGroups.Count;
        if (gap == -1 && total != 8) return null;
        if (gap != -1 && total > 7) return null;

        var groups = new int[8];
        for (var i = 0; i < headGroups.Count; i++) groups[i] = headGroups[i];
        for (var i = 0; i < tailGroups.Count; i++) groups[8 - tailGroups.Count + i] = tailGroups[i];
        return groups;
    }

    private static List<int>? IPv6Groups(string[] parts, bool dottedLastAllowed)
    {
        var output = new List<int>();
        for (var i = 0; i < parts.Length; i++)
        {
            var part = parts[i];
            if (part.Contains('.'))
            {
                if (!dottedLastAllowed || i != parts.Length - 1) return null;
                var o = ParseCanonicalIPv4(part.Split('.'));
                if (o is null) return null;
                output.Add((o[0] << 8) | o[1]);
                output.Add((o[2] << 8) | o[3]);
            }
            else
            {
                if (part.Length is 0 or > 4 || !part.All(IsHexDigit)) return null;
                output.Add(Convert.ToInt32(part, 16));
            }
        }
        return output;
    }

    private static int[] EmbeddedIPv4(int hi, int lo) => new[] { hi >> 8, hi & 0xff, lo >> 8, lo & 0xff };

    private static bool IsPublicIPv6(int[] g)
    {
        bool ZeroTo(int n) => g.Take(n).All(x => x == 0);
        // ::, ::1, and the deprecated IPv4-compatible ::a.b.c.d.
        if (ZeroTo(6)) return false;
        // ::ffff:a.b.c.d (IPv4-mapped) and ::ffff:0:a.b.c.d (IPv4-translated).
        if (ZeroTo(5) && g[5] == 0xffff) return !IsPrivateIPv4(EmbeddedIPv4(g[6], g[7]));
        if (ZeroTo(4) && g[4] == 0xffff && g[5] == 0) return !IsPrivateIPv4(EmbeddedIPv4(g[6], g[7]));
        // 64:ff9b::/96 NAT64 well-known prefix; 64:ff9b:1::/48 is local-use.
        if (g[0] == 0x64 && g[1] == 0xff9b)
        {
            if (g[2] == 1) return false;
            if (g[2] == 0 && g[3] == 0 && g[4] == 0 && g[5] == 0)
                return !IsPrivateIPv4(EmbeddedIPv4(g[6], g[7]));
        }
        // 2002::/16 6to4 carries an IPv4 address in groups 1–2.
        if (g[0] == 0x2002) return !IsPrivateIPv4(EmbeddedIPv4(g[1], g[2]));
        if ((g[0] & 0xff80) == 0xfe80) return false;  // fe80::/10 link-local, fec0::/10 site-local
        if ((g[0] & 0xfe00) == 0xfc00) return false;  // fc00::/7 unique-local
        if ((g[0] & 0xff00) == 0xff00) return false;  // ff00::/8 multicast
        return true;
    }

    /// <summary>
    /// Resolve protocol <c>conditions</c> names against the bundled registry,
    /// preserving order and arity.
    /// </summary>
    public static IReadOnlyList<NPResolvedCondition> Resolve(IEnumerable<string> names) =>
        names.Select(name =>
        {
            if (!NPConditionRegistry.ByName.TryGetValue(name, out var definition))
            {
                return new NPResolvedCondition(name, null, NPLinkVerdict.Blocked(NPLinkReason.NotAbsolute));
            }
            return new NPResolvedCondition(name, definition, Evaluate(definition.Link));
        }).ToList();
}
