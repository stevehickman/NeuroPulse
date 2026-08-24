using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;

namespace NeurOne.Protocol;

/// <summary>
/// The condition registry, read from <c>protocols/predefined/00-conditions.npps</c>
/// at run time.
/// </summary>
/// <remarks>
/// <para>
/// **A `condition` block in a .npps file is the only origin of a condition**, and
/// it is read when the app runs, never baked in when the app is built
/// (NP-NPPS-REF-001 §1.6). This replaces the generated
/// <c>NPBundledConditions.cs</c>, which transcribed the registry into C# source
/// at build time.
/// </para>
/// <para>
/// A build-time cache cannot be correct here: nothing stops the .npps files
/// changing after the build, and when they do the cache is read in place of the
/// edit. The change simply never appears, with no error and nothing to notice —
/// the author sees their edit ignored and has no way to tell why. Reading the
/// file at run time is the only arrangement where editing a .npps file has the
/// effect the author intends.
/// </para>
/// <para>
/// Windows has no NPPS parser, which is why the table was generated in the first
/// place. It does not need one: the whole condition grammar
/// (<c>npps/grammar/npps.peggy</c>) is a quoted name plus quoted string fields,
/// so the targeted reader below covers it. That is the same reader
/// <c>scripts/sync-conditions.ts</c> used to run at build time, moved to where
/// the file actually is.
/// </para>
/// </remarks>
public static class NPConditionRegistry
{
    /// <summary>
    /// Directory holding the shipped .npps library, copied beside the assembly by
    /// the csproj. One fixed location, matching §1.6's directory model.
    /// </summary>
    private const string PredefinedDirectory = "protocols/predefined";

    private const string RegistryFile = "00-conditions.npps";

    // `condition "Name" { ... }` — the whole of the condition grammar.
    private static readonly Regex BlockPattern = new(
        "condition\\s+\"((?:[^\"\\\\]|\\\\.)*)\"\\s*\\{([^}]*)\\}",
        RegexOptions.Compiled);

    private static readonly Regex FieldPattern = new(
        "([A-Za-z_][A-Za-z0-9_]*)\\s*:\\s*\"((?:[^\"\\\\]|\\\\.)*)\"",
        RegexOptions.Compiled);

    private static readonly object Gate = new();
    private static IReadOnlyList<NPConditionDefinition>? _all;
    private static IReadOnlyDictionary<string, NPConditionDefinition>? _byName;

    /// <summary>Every condition the registry file defines.</summary>
    public static IReadOnlyList<NPConditionDefinition> All
    {
        get { EnsureLoaded(); return _all!; }
    }

    /// <summary>Name → definition, for resolving a protocol's conditions entries.</summary>
    public static IReadOnlyDictionary<string, NPConditionDefinition> ByName
    {
        get { EnsureLoaded(); return _byName!; }
    }

    /// <summary>
    /// Duplicate condition names in the registry. A name defined twice binds to
    /// NEITHER definition and is reported here (NP-NPPS-REF-001 §1.6): file read
    /// order is not guaranteed, so letting either win would make which definition
    /// applies a property of the file system.
    /// </summary>
    public static IReadOnlyList<string> Errors { get; private set; } = Array.Empty<string>();

    /// <summary>
    /// Re-read the registry from disk. Call after the .npps library changes on
    /// disk; the next <see cref="All"/> or <see cref="ByName"/> reflects the edit.
    /// </summary>
    public static void Reload()
    {
        lock (Gate)
        {
            _all = null;
            _byName = null;
        }
    }

    private static void EnsureLoaded()
    {
        lock (Gate)
        {
            if (_all is not null) { return; }

            var path = Path.Combine(AppContext.BaseDirectory, PredefinedDirectory, RegistryFile);
            var source = File.Exists(path) ? File.ReadAllText(path) : string.Empty;
            var (conditions, errors) = Parse(source);

            _all = conditions;
            _byName = conditions.ToDictionary(c => c.Name);
            Errors = errors;
        }
    }

    /// <summary>
    /// Read `condition` blocks out of NPPS text. Exposed for tests so the reader
    /// can be exercised without touching the file system.
    /// </summary>
    internal static (IReadOnlyList<NPConditionDefinition> Conditions, IReadOnlyList<string> Errors)
        Parse(string source)
    {
        var byName = new Dictionary<string, NPConditionDefinition>();
        var collided = new HashSet<string>();
        var order = new List<string>();
        var errors = new List<string>();

        foreach (Match block in BlockPattern.Matches(source))
        {
            var name = Unescape(block.Groups[1].Value);
            var fields = new Dictionary<string, string>();
            foreach (Match field in FieldPattern.Matches(block.Groups[2].Value))
            {
                fields[field.Groups[1].Value] = Unescape(field.Groups[2].Value);
            }

            // `link` is the one required field (§9). A block without it defines
            // nothing usable, so it is skipped rather than half-registered.
            if (!fields.TryGetValue("link", out var link)) { continue; }

            if (collided.Contains(name)) { continue; }
            if (byName.ContainsKey(name))
            {
                errors.Add(
                    $"Duplicate condition name '{name}' — defined more than once; condition names " +
                    "must be unique across the protocol directory. The name is left undefined.");
                byName.Remove(name);
                order.Remove(name);
                collided.Add(name);
                continue;
            }

            byName[name] = new NPConditionDefinition(
                Name: name,
                Link: link,
                Id: fields.GetValueOrDefault("id"),
                Code: fields.GetValueOrDefault("code"),
                Description: fields.GetValueOrDefault("description"));
            order.Add(name);
        }

        return (order.Select(n => byName[n]).ToList(), errors);
    }

    private static string Unescape(string value) =>
        value.Replace("\\\"", "\"").Replace("\\\\", "\\");
}
