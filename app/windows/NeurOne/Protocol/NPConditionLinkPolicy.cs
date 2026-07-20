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
/// Instances come from <see cref="NPBundledConditions"/>, generated from
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
    /// Loopback, link-local, mDNS, RFC1918, and single-label intranet hosts. A
    /// crafted registry must not be able to aim the system browser at a service
    /// bound to the user's own machine or LAN.
    /// </summary>
    public static bool IsLocalOrPrivateHost(string hostname)
    {
        var host = hostname.ToLowerInvariant();

        if (host == "localhost" || host.EndsWith(".localhost", StringComparison.Ordinal)) return true;
        if (host.EndsWith(".local", StringComparison.Ordinal)) return true;

        if (host.Contains(':')) return IsPrivateIPv6(host);

        var octets = ParseIPv4(host);
        if (octets is not null) return IsPrivateIPv4(octets);

        // A single-label name like `wiki` or `intranet` is not a public FQDN; it
        // resolves through the machine's DNS search suffix onto the LAN. Public
        // condition links are always dotted.
        return !host.Contains('.');
    }

    private static bool IsPrivateIPv6(string host)
    {
        if (host is "::1" or "::") return true;
        // fe80::/10 link-local, fc00::/7 unique-local.
        return host.StartsWith("fe80:", StringComparison.Ordinal)
            || host.StartsWith("fc", StringComparison.Ordinal)
            || host.StartsWith("fd", StringComparison.Ordinal);
    }

    private static int[]? ParseIPv4(string host)
    {
        var parts = host.Split('.');
        if (parts.Length != 4) return null;

        var octets = new int[4];
        for (var i = 0; i < 4; i++)
        {
            var part = parts[i];
            if (part.Length is 0 or > 3 || !part.All(IsAsciiDigit)) return null;
            if (!int.TryParse(part, out var n) || n > 255) return null;
            octets[i] = n;
        }
        return octets;
    }

    private static bool IsPrivateIPv4(int[] o)
    {
        int a = o[0], b = o[1];
        if (a is 127 or 0) return true;            // loopback, "this network"
        if (a == 10) return true;                  // RFC1918 10/8
        if (a == 192 && b == 168) return true;     // RFC1918 192.168/16
        if (a == 172 && b is >= 16 and <= 31) return true; // RFC1918 172.16/12
        if (a == 169 && b == 254) return true;     // link-local
        return false;
    }

    /// <summary>
    /// Resolve protocol <c>conditions</c> names against the bundled registry,
    /// preserving order and arity.
    /// </summary>
    public static IReadOnlyList<NPResolvedCondition> Resolve(IEnumerable<string> names) =>
        names.Select(name =>
        {
            if (!NPBundledConditions.ByName.TryGetValue(name, out var definition))
            {
                return new NPResolvedCondition(name, null, NPLinkVerdict.Blocked(NPLinkReason.NotAbsolute));
            }
            return new NPResolvedCondition(name, definition, Evaluate(definition.Link));
        }).ToList();
}
