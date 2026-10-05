// NPZoneRegistry.cs — the zone definitions in force.
//
// A `zone` block in a .npps file is the only ORIGIN of a zone (NP-NPPS-REF-001 §8). This is a
// lookup over the shipped files, read at run time from beside the assembly (NeurOne.csproj
// copies protocols/predefined/**/*.npps there); it holds no socket list of its own. Windows has
// no full NPPS parser yet, so this reads just the `zone "Name" { ... sockets: [..] ... }`
// blocks, which have a fixed, line-oriented shape in 00-zones.npps (OI-AND-WIRE-01).
//
// Counterpart of NPZoneRegistry.kt (Android) and NPZoneRegistry.swift (iOS).

using System.Globalization;
using System.Text.RegularExpressions;

namespace NeurOne.Protocol;

static class NPZoneRegistry
{
    private static readonly object Gate = new();
    private static IReadOnlyDictionary<string, int[]>? _zones;

    private static readonly Regex ZoneOpen = new(@"^\s*zone\s+""(?<name>[^""]+)""\s*\{\s*$", RegexOptions.CultureInvariant);
    private static readonly Regex SocketsLine = new(@"^\s*sockets\s*:\s*\[(?<ids>[^\]]*)\]\s*$", RegexOptions.CultureInvariant);

    /// Replace the zones in force: tests, and user-authored zones once those files are loaded.
    public static void Use(IReadOnlyDictionary<string, int[]> zones)
    {
        lock (Gate) _zones = zones;
    }

    /// The 1-based socket ids a zone contains, or null when no loaded file defines a zone by that
    /// name (an authoring error in the protocol, not a device state).
    public static int[]? Sockets(string zoneName)
    {
        lock (Gate)
        {
            _zones ??= LoadShipped();
            return _zones.TryGetValue(zoneName, out var ids) ? ids : null;
        }
    }

    /// Parse every `zone` block of a .npps text. A block with no parsable `sockets:` list is
    /// skipped here and is a build-time failure of scripts/sync-socket-map.ts for the shipped files.
    internal static Dictionary<string, int[]> ParseZones(string npps)
    {
        var zones = new Dictionary<string, int[]>(StringComparer.Ordinal);
        string? name = null;
        foreach (var raw in npps.Split('\n'))
        {
            var line = raw.TrimEnd('\r');
            if (name is null)
            {
                var open = ZoneOpen.Match(line);
                if (open.Success) name = open.Groups["name"].Value;
                continue;
            }
            if (line.TrimStart().StartsWith('}')) { name = null; continue; }
            var s = SocketsLine.Match(line);
            if (!s.Success) continue;
            zones[name] = s.Groups["ids"].Value
                .Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries)
                .Select(t => int.Parse(t, NumberStyles.Integer, CultureInfo.InvariantCulture))
                .ToArray();
        }
        return zones;
    }

    private static IReadOnlyDictionary<string, int[]> LoadShipped()
    {
        var dir = Path.Combine(AppContext.BaseDirectory, "protocols", "predefined");
        var all = new Dictionary<string, int[]>(StringComparer.Ordinal);
        if (!Directory.Exists(dir)) return all;
        foreach (var file in Directory.EnumerateFiles(dir, "*.npps", SearchOption.AllDirectories).OrderBy(f => f, StringComparer.Ordinal))
            foreach (var (n, ids) in ParseZones(File.ReadAllText(file)))
                all[n] = ids;
        return all;
    }
}
