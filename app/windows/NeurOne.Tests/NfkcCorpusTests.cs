using System.Runtime.CompilerServices;
using System.Text;
using System.Text.Json.Nodes;
using NeurOne.Protocol;
using Xunit;

namespace NeurOne.Tests;

// The passphrase normalisation through neurone_npps_ffi.dll: the corpus of known NFKC answers
// (app/NeurOneShared/TestData/nfkc-corpus.json) that the Rust core, Kotlin, Swift and the web run too. .NET strings are UTF-16
// and the C ABI takes UTF-8 bytes, so the cases outside the BMP (surrogate pairs) are the ones this proves (SWR-MG-02,
// NP-SOUP-RUST-001).
public class NfkcCorpusTests
{
    private static readonly string Root = RepoRoot();

    private static string RepoRoot([CallerFilePath] string file = "")
        => new DirectoryInfo(Path.GetDirectoryName(file)!).Parent!.Parent!.Parent!.FullName;

    private static JsonObject Corpus() => JsonNode.Parse(File.ReadAllText(
        Path.Combine(Root, "app", "NeurOneShared", "TestData", "nfkc-corpus.json")))!.AsObject();

    private static JsonObject Secret(string passphrase) =>
        NppsCore.Migration(new JsonObject { ["op"] = "passphraseSecret", ["passphrase"] = passphrase }).AsObject();

    private static string HexOf(string text) => Convert.ToHexString(Encoding.UTF8.GetBytes(text)).ToLowerInvariant();

    // @verifies SWR-MG-02
    [Fact]
    public void EveryKnownNfkcAnswerHoldsThroughTheBinding()
    {
        var corpus = Corpus();
        var pad = (string)corpus["padding"]!;
        var wrong = new List<string>();
        foreach (var c in corpus["cases"]!.AsArray())
        {
            var got = (string?)Secret(pad + (string)c!["input"]!)["secret"];
            var want = HexOf(pad + (string)c["expected"]!);
            if (got != want) wrong.Add($"{(string)c["name"]!}: {got} != {want}");
        }
        foreach (var c in corpus["exact"]!.AsArray())
            if ((string?)Secret((string)c!["input"]!)["secret"] != HexOf((string)c["expected"]!)) wrong.Add($"{(string)c["name"]!}: the floor is counted wrongly");
        foreach (var c in corpus["tooShort"]!.AsArray())
            if ((string?)Secret((string)c!["input"]!)["reason"] != "too_short") wrong.Add($"{(string)c["name"]!}: accepted under the floor");
        Assert.True(wrong.Count == 0, $"{wrong.Count} answer(s) differ:\n" + string.Join("\n", wrong));
        Assert.Equal(12, (int)corpus["minChars"]!);
    }
}
