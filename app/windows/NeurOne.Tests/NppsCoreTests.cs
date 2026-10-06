using System.Runtime.CompilerServices;
using System.Text.Json.Nodes;
using NeurOne.Protocol;
using Xunit;

namespace NeurOne.Tests;

// The Windows binding of the shared NPPS core (OI-NPPS-CORE-01), exercised through neurone_npps_ffi.dll
// (CI builds it: windows-ci.yml). The goldens are the web parser's own output (bun scripts/gen-npps-parse-golden.ts);
// the Rust crate and its C ABI are already diffed against them, so what this adds is the P/Invoke layer: the
// buffer ownership, UTF-8 (the messages contain "—" and "²") and the typed models built from the JSON.
public class NppsCoreTests
{
    private static readonly string Root = RepoRoot();

    private static string RepoRoot([CallerFilePath] string file = "")
        => new DirectoryInfo(Path.GetDirectoryName(file)!).Parent!.Parent!.Parent!.FullName;

    private static JsonObject Golden() => JsonNode.Parse(File.ReadAllText(
        Path.Combine(Root, "app", "NeurOneShared", "TestData", "npps-parse-golden.json")))!.AsObject();

    private static readonly string[] ParseKeys = ["entries", "zones", "conditions", "wavelengthRules", "limits"];

    /// The web parser keeps only the first `limits` block; the core reports every one.
    private static JsonObject Reduce(JsonObject parsed)
    {
        var reduced = new JsonObject();
        foreach (var key in ParseKeys.Where(k => k != "limits")) reduced[key] = parsed[key]!.DeepClone();
        var limits = parsed["limits"]!.AsArray();
        reduced["limits"] = limits.Count > 0 ? limits[0]!.DeepClone() : null;
        return reduced;
    }

    private static void CheckParse(string name, string source, JsonNode expected, List<string> failures)
    {
        var want = expected["error"]?.GetValue<string>();
        try
        {
            var got = Reduce(JsonNode.Parse(NppsCore.Parse(source))!.AsObject());
            if (want is not null) { failures.Add($"{name}: accepted what the web parser refuses: {want}"); return; }
            foreach (var key in ParseKeys)
                if (!JsonNode.DeepEquals(got[key], expected[key])) failures.Add($"{name}: {key} differ");
        }
        catch (NppsRefusal e)
        {
            if (want is null) failures.Add($"{name}: refused what the web parser accepts: {e.Message}");
            else if (e.Message != want) failures.Add($"{name}: message differs\n  core: {e.Message}\n  web:  {want}");
        }
    }

    [Fact]
    public void TheWholeShippedLibraryParsesAsTheWebParserDoes()
    {
        var files = Golden()["files"]!.AsObject();
        Assert.True(files.Count > 60, "the golden covers the shipped library");
        var failures = new List<string>();
        foreach (var (rel, expected) in files)
            CheckParse(rel, File.ReadAllText(Path.Combine(Root, rel)), expected!, failures);
        Assert.True(failures.Count == 0, $"{failures.Count} divergence(s):\n" + string.Join("\n", failures));
    }

    [Fact]
    public void TheRefusalAndAliasCorpusParsesAsTheWebParserDoes()
    {
        var failures = new List<string>();
        foreach (var (name, c) in Golden()["cases"]!.AsObject())
            CheckParse(name, c!["source"]!.GetValue<string>(), c, failures);
        Assert.True(failures.Count == 0, $"{failures.Count} divergence(s):\n" + string.Join("\n", failures));
    }

    [Fact]
    public void NamespacesFoldAndValidateAsTheWebFunctionsDo()
    {
        var cases = Golden()["namespaces"]!.AsObject();
        Assert.True(cases.ContainsKey("library") && cases.Count > 5);
        var failures = new List<string>();
        foreach (var (name, c) in cases)
        {
            var sources = c!["paths"] is JsonArray paths
                ? paths.Select(p => File.ReadAllText(Path.Combine(Root, p!.GetValue<string>())))
                : c["sources"]!.AsArray().Select(s => s!.GetValue<string>());
            var files = sources.Select(s => JsonNode.Parse(NppsCore.Parse(s))!).ToList();
            var got = JsonNode.Parse(NppsCore.Namespace(files))!.AsObject();
            foreach (var key in new[] { "entries", "zones", "conditions", "errors", "referenceErrors" })
                if (!JsonNode.DeepEquals(got[key], c[key])) failures.Add($"{name}: {key} differ");
        }
        Assert.True(failures.Count == 0, $"{failures.Count} divergence(s):\n" + string.Join("\n", failures));
    }

    [Fact]
    public void AShippedProtocolParsesToTheTypedModelsAndAZoneToItsSockets()
    {
        var text = File.ReadAllText(Path.Combine(Root, "protocols", "predefined", "01-gamma-focus.npps"));
        var protocol = Assert.Single(NPPSParser.Parse(text));
        Assert.True(protocol.IsPredefined);
        Assert.NotEmpty(protocol.Modalities);

        var zones = NPPSParser.ParseFile(File.ReadAllText(Path.Combine(Root, "protocols", "predefined", "00-zones.npps"))).Zones;
        Assert.Contains("Frontal", zones.Keys);
        Assert.NotEmpty(zones["Frontal"]);
    }

    [Fact]
    public void ARefusalCarriesTheCoresMessageAndLine()
    {
        var e = Assert.Throws<NPPSParseException>(() => NPPSParser.Parse("zone {\n}\n"));
        Assert.Equal("Line 1: Expected zone name string, got LBRACE", e.Message);
        Assert.Equal(1, e.Line);
    }

    [Fact]
    public void ASessionUuidThatIsNot16BytesIsRefusedBeforeTheCore()
    {
        var options = new NppsCompileOptions { NowUnix = 0, SessionUuid = new byte[15] };
        var e = Assert.Throws<NppsRefusal>(() => NppsCore.Compile("{}"u8, options));
        Assert.Equal("sessionUUID must be 16 bytes", e.Message);
    }

    // ── serialize and validate (the P/Invoke layer: UTF-8 in and out, refusals as NppsRefusal) ─────────────────

    private static JsonObject SerializeGolden() => JsonNode.Parse(File.ReadAllText(
        Path.Combine(Root, "app", "NeurOneShared", "TestData", "npps-serialize-golden.json")))!.AsObject();

    [Fact]
    public void SerializeWritesTheShippedLibraryAsTheWebSerializerDid()
    {
        var failures = new List<string>();
        var n = 0;
        foreach (var (rel, items) in SerializeGolden()["files"]!.AsObject())
            foreach (var item in items!.AsArray())
            {
                n++;
                var got = NppsCore.Serialize([item!["item"]!]);
                if (got != item["text"]!.GetValue<string>()) failures.Add($"{rel}: differs");
            }
        Assert.True(n > 100);
        Assert.True(failures.Count == 0, $"{failures.Count} divergence(s):\n" + string.Join("\n", failures));
    }

    [Fact]
    public void SerializeRefusesAZoneItCannotWriteWithTheCoresMessage()
    {
        var zone = JsonNode.Parse("""{"kind":"zone","zone":{"name":"Z","sockets":[0,999]}}""")!;
        var e = Assert.Throws<NppsRefusal>(() => NppsCore.Serialize([zone]));
        // The message holds an em dash: it must survive the trip through the DLL unchanged.
        Assert.Equal("cannot serialize zone \"Z\": 0, 999 are not sockets on this helmet \u2014 ids are whole numbers 1\u201380", e.Message);
    }

    [Fact]
    public void ValidateReturnsLocaleKeysAndArguments()
    {
        var request = JsonNode.Parse("""
            {"entry":{"kind":"single","protocol":{"name":"P","timingMode":{"type":"duration","seconds":1200},
             "modalities":[{"type":"bes_tacs","enabled":true,"interval":{},
             "params":{"intensityMilliamps":2,"frequencyHz":10,"waveform":"square"}}]}},"limits":{}}
            """)!;
        var result = JsonNode.Parse(NppsCore.Validate(request))!.AsObject();
        Assert.False(result["isValid"]!.GetValue<bool>());
        var message = result["issues"]![0]!["message"]!;
        Assert.Equal("VALIDATE_MSG_BES_TACS_INTENSITYMILLIAMPS", message["key"]!.GetValue<string>());
        Assert.Equal(["2", "1"], message["args"]!.AsArray().Select(a => a!.GetValue<string>()).ToArray());
    }

    [Fact]
    public void ResolveLimitsResolvesEveryTierCombinationAsTheWebFunctionDid()
    {
        var cases = JsonNode.Parse(File.ReadAllText(
            Path.Combine(Root, "app", "NeurOneShared", "TestData", "npps-resolve-golden.json")))!["cases"]!.AsArray();
        Assert.True(cases.Count >= 300);
        var failures = new List<string>();
        foreach (var c in cases)
        {
            var got = JsonNode.Parse(NppsCore.ResolveLimits(c!["global"], c["helmet"], c["individual"]))!["limits"]!.AsObject();
            got.Remove("level");
            if (!JsonNode.DeepEquals(got, c["expected"])) failures.Add($"{c["name"]}: differs");
        }
        Assert.True(failures.Count == 0, $"{failures.Count} divergence(s):\n" + string.Join("\n", failures.Take(5)));
    }
}
