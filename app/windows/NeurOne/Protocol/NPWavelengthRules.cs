// NPWavelengthRules.cs — PBM wavelength mapping rules.
// C# port of common/lib/wavelengthRules.ts (the reference), NP-NPPS-REF-001
// Rev 17 §4.1a and §7a.
//
// A protocol states the wavelength its source used ("810nm"). These rules say
// which emitter channel, if any, may deliver it. A wavelength no rule accepts
// maps to NOTHING: it is never moved to the nearest channel and never driven on
// every channel. Windows reads only the shipped defaults; editing the rules is
// done in the web app for now.

using System.Globalization;
using System.Text.RegularExpressions;

namespace NeurOne.Protocol;

enum PbmChannelElement { Led660, Led808, Led1064 }

sealed record WavelengthChannelRule(PbmChannelElement Element, double NominalNm, double MinNm, double MaxNm);

sealed record WavelengthRules(string Name, string Level, IReadOnlyList<WavelengthChannelRule> Channels);

static class WavelengthRulesEngine
{
    /// Mirrors protocols/predefined/00-wavelength-rules.npps: each channel's band in
    /// CLAUDE.md §3 widened 10 nm each side. An UNVALIDATED DEFAULT, not a safety parameter.
    public static readonly WavelengthRules Default = new(
        "NeurOne default wavelength mapping",
        "global",
        new[]
        {
            new WavelengthChannelRule(PbmChannelElement.Led660, 660, 650, 680),
            new WavelengthChannelRule(PbmChannelElement.Led808, 808, 798, 840),
            new WavelengthChannelRule(PbmChannelElement.Led1064, 1064, 1054, 1074),
        });

    /// The two combined channel names the language used to accept (NP-NPPS-REF-001 Rev 18).
    /// RETIRED: each wavelength is its own block. "1064nm" was never one of them in substance;
    /// it is a single wavelength the default rules map to the 1064 nm channel.
    public static readonly IReadOnlyDictionary<string, string[]> Retired =
        new Dictionary<string, string[]>
        {
            ["660_808nm"] = new[] { "660nm", "808nm" },
            ["660_808_1064nm"] = new[] { "660nm", "808nm", "1064nm" },
        };

    /// The refusal text for a retired name, naming the blocks that replace it.
    public static string RetiredMessage(string value)
    {
        var blocks = string.Join(" and ", (Retired.TryGetValue(value, out var r) ? r : Array.Empty<string>()).Select(w => $"\"{w}\""));
        return $"wavelength \"{value}\" is retired: it welded independent emitters into one block. " +
               $"Write one block per wavelength ({blocks}), each with its own irradiance.";
    }

    private static readonly Regex SingleNm = new(@"^([0-9]+(?:\.[0-9]+)?)nm\z", RegexOptions.CultureInvariant);

    /// The channel that delivers `nm`, or null. Nearest nominal wins; a tie goes to the
    /// earlier channel (660, 808, 1064), so rule order never matters.
    public static PbmChannelElement? Map(double nm, WavelengthRules rules)
    {
        WavelengthChannelRule? best = null;
        foreach (var element in new[] { PbmChannelElement.Led660, PbmChannelElement.Led808, PbmChannelElement.Led1064 })
        {
            var rule = rules.Channels.FirstOrDefault(c => c.Element == element);
            if (rule is null || nm < rule.MinNm || nm > rule.MaxNm) continue;
            if (best is null || Math.Abs(nm - rule.NominalNm) < Math.Abs(nm - best.NominalNm)) best = rule;
        }
        return best?.Element;
    }

    /// The channels a `wavelength` value drives, or null with a reason ("invalid", "retired" or "unmapped").
    public static PbmChannelElement[]? ResolveChannels(string value, WavelengthRules rules, out string? refusal)
    {
        refusal = null;
        if (Retired.ContainsKey(value)) { refusal = "retired"; return null; }
        var m = SingleNm.Match(value);
        if (!m.Success || !double.TryParse(m.Groups[1].Value, NumberStyles.Float, CultureInfo.InvariantCulture, out var nm) || nm <= 0)
        {
            refusal = "invalid";
            return null;
        }
        var element = Map(nm, rules);
        if (element is null) { refusal = "unmapped"; return null; }
        return new[] { element.Value };
    }
}
