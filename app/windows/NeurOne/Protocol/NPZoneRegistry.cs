// NPZoneRegistry.cs — the zone definitions in force.
//
// A `zone` block in a .npps file is the only ORIGIN of a zone (NP-NPPS-REF-001 §8). This is a
// lookup over the shipped files, read at run time from beside the assembly (NeurOne.csproj
// copies protocols/predefined/**/*.npps there); it holds no socket list of its own, and no parser of its
// own: zones come from the shared NPPS core through NPPSParser (OI-NPPS-CORE-01).
//
// Counterpart of NPZoneRegistry.kt (Android) and NPZoneRegistry.swift (iOS).

namespace NeurOne.Protocol;

static class NPZoneRegistry
{
    private static readonly object Gate = new();
    private static IReadOnlyDictionary<string, int[]>? _zones;

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

    /// Parse every `zone` block of a .npps text with the shared NPPS core (NPPSParser). A text the core
    /// refuses throws NPPSParseException: the shipped files are valid, and a zone that fails to parse must
    /// not read as a zone that does not exist.
    internal static Dictionary<string, int[]> ParseZones(string npps)
        => new(NPPSParser.ParseFile(npps).Zones, StringComparer.Ordinal);

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
