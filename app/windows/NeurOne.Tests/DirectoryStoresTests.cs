using System.Text.Json.Nodes;
using NeurOne.Protocol;
using Xunit;

namespace NeurOne.Tests;

// The folder-backed stores on a real disk: the route for a local computer or a USB drive, and the app's files for the old phone's
// clear (SWR-MG-06, SWR-MG-07, SWR-MG-10, SWR-MG-12). The same cases run on Kotlin and Swift.
public class DirectoryStoresTests : IDisposable
{
    private readonly List<string> _dirs = [];

    private string Temp()
    {
        var dir = Path.Combine(Path.GetTempPath(), "np-dir-store-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(dir);
        _dirs.Add(dir);
        return dir;
    }

    public void Dispose()
    {
        foreach (var d in _dirs) if (Directory.Exists(d)) Directory.Delete(d, recursive: true);
    }

    private sealed class RandomOnly(int seed) : IMigrationHost
    {
        private int _n = seed;
        public Task<JsonObject> PerformAsync(JsonObject e)
        {
            if ((string?)e["type"] != "random") throw new InvalidOperationException($"unknown effect {(string?)e["type"]}");
            var bytes = Enumerable.Range(0, (int)e["bytes"]!).Select(_ => (byte)(_n = (_n * 31 + 7) & 255)).ToArray();
            return Task.FromResult(new JsonObject { ["bytes"] = Convert.ToHexString(bytes).ToLowerInvariant() });
        }
    }

    private sealed class ClearHost : IMigrationHost
    {
        public Task<JsonObject> PerformAsync(JsonObject e) =>
            (string?)e["type"] == "runClearStep"
                ? Task.FromResult(new JsonObject { ["ok"] = true })
                : throw new InvalidOperationException($"unknown effect {(string?)e["type"]}");
    }

    private sealed class Keys : IKeyValueStore
    {
        public readonly Dictionary<string, string> Values = [];
        public string? GetString(string key) => Values.GetValueOrDefault(key);
        public void PutString(string key, string value) => Values[key] = value;
        public void Remove(string key) => Values.Remove(key);
    }

    private static BundleSecret Secret() => BundleKeyDerivation.PassphraseSecret("a long enough passphrase").Secret!;

    // @verifies SWR-MG-07
    [Fact]
    public async Task OfManySimultaneousCreatesExactlyOneWinsAndItsContentIsWhole()
    {
        var store = new DirectoryObjectStore(Temp());
        var results = await Task.WhenAll(Enumerable.Range(1, 24).Select(i =>
            Task.Run(() => store.CreateIfAbsentAsync("claim.json", string.Concat(Enumerable.Repeat(i.ToString("x2"), 4096))))));
        Assert.Equal(1, results.Count(r => r));
        var content = (await store.GetAsync("claim.json"))!;
        Assert.Equal(8192, content.Length);
        Assert.Single(Enumerable.Range(0, 4096).Select(i => content.Substring(i * 2, 2)).Distinct());
        Assert.Equal(["claim.json"], await store.ListAsync(""));
    }

    // @verifies SWR-MG-11
    [Fact]
    public async Task AnObjectIsWrittenReplacedReadListedAndDeletedAndANameCannotLeaveTheFolder()
    {
        var store = new DirectoryObjectStore(Temp());
        Assert.Null(await store.GetAsync("a.bin"));
        await store.PutAsync("a.bin", "010203");
        await store.PutAsync("a.bin", "09");
        Assert.Equal("09", await store.GetAsync("a.bin"));
        await store.PutAsync("b.bin", "");
        Assert.Equal(["a.bin", "b.bin"], await store.ListAsync(""));
        Assert.Equal(["b.bin"], await store.ListAsync("b"));
        await store.DeleteAsync("a.bin");
        await store.DeleteAsync("a.bin");
        Assert.Null(await store.GetAsync("a.bin"));
        Assert.True((await store.FreeSpaceAsync() ?? 0) > 0);
        foreach (var bad in new[] { "../x", "a/b", ".hidden", "", "a\\b", "é" })
            await Assert.ThrowsAsync<ArgumentException>(() => store.GetAsync(bad));
    }

    // @verifies SWR-MG-12
    [Fact]
    public async Task AFolderOnDiskPassesTheAccountCheckAndTheProbeLeavesItEmpty()
    {
        var dir = Temp();
        var result = await MigrationFlow.ProbeAsync(new DirectoryObjectStore(dir), 1_000_000, new RandomOnly(7));
        Assert.Equal("sufficient", (string)result["result"]!);
        Assert.Empty(Directory.EnumerateFileSystemEntries(dir));
    }

    // @verifies SWR-MG-06
    [Fact]
    public async Task TheFileStoreOverwritesInPlaceAndWillNotLeaveItsFolder()
    {
        var dir = Temp();
        var files = new DirectoryFileStore(dir);
        await File.WriteAllBytesAsync(Path.Combine(dir, "x.db"), Enumerable.Repeat((byte)1, 10).ToArray());
        Assert.Equal([("x.db", 10L)], (await files.ListAsync()).Select(f => (f.Path, f.Size)).ToArray());
        await files.WriteAsync("x.db", 4, [7, 7]);
        await files.SyncAsync("x.db");
        Assert.Equal(new byte[] { 1, 1, 1, 1, 7, 7, 1, 1, 1, 1 }, await File.ReadAllBytesAsync(Path.Combine(dir, "x.db")));
        await files.DeleteAsync("x.db");
        await files.DeleteAsync("x.db");
        Assert.False(File.Exists(Path.Combine(dir, "x.db")));
        foreach (var bad in new[] { "../x.db", "/etc/passwd", "" })
            await Assert.ThrowsAsync<ArgumentException>(() => files.DeleteAsync(bad));
    }

    // @verifies SWR-MG-10
    // @verifies SWR-MG-07
    // @verifies SWR-MG-06
    [Fact]
    public async Task AMigrationOverAFolderOnDiskMovesThePhoneAndTheClearOverwritesAndDeletesRealFiles()
    {
        var folder = new DirectoryObjectStore(Temp());
        JsonObject Route() => new() { ["id"] = "folder", ["atomicClaim"] = true, ["direct"] = false, ["authenticatedPairing"] = true };
        var oldKeys = new Keys();
        oldKeys.PutString("np.session.history", "history of the old phone");
        using var s1 = Secret();
        var backup = await MigrationFlow.RunAsync("backup",
            new JsonObject { ["kind"] = "migrate", ["method"] = "passphrase", ["route"] = Route(), ["hostItems"] = new JsonArray() },
            new RandomOnly(1), s1, oldKeys, folder);
        Assert.True((bool)backup["ok"]!);

        var newKeys = new Keys();
        using var s2 = Secret();
        var restore = await MigrationFlow.RunAsync("restore", new JsonObject { ["route"] = Route(), ["hostItems"] = new JsonArray() }, new RandomOnly(50), s2, newKeys, folder);
        Assert.True((bool)restore["ok"]!);
        Assert.Equal("history of the old phone", newKeys.GetString("np.session.history"));
        using var s3 = Secret();
        var second = await MigrationFlow.RunAsync("restore", new JsonObject { ["route"] = Route(), ["hostItems"] = new JsonArray() }, new RandomOnly(90), s3, new Keys(), folder);
        Assert.Equal("claimed", (string)second["reason"]!);

        var data = Temp();
        Directory.CreateDirectory(Path.Combine(data, "uhdr"));
        await File.WriteAllBytesAsync(Path.Combine(data, "uhdr", "sessions.db"), Enumerable.Repeat((byte)0x5A, 40_000).ToArray());
        await File.WriteAllBytesAsync(Path.Combine(data, "settings.db"), []);
        var clear = await MigrationFlow.RunAsync("clear",
            new JsonObject { ["bundleHash"] = (string)backup["bundleHash"]!, ["canRemoveOsBond"] = true },
            new ClearHost(), null, oldKeys, folder, new DirectoryFileStore(data));
        Assert.True((bool)clear["ok"]!, clear.ToJsonString());
        Assert.Empty(Directory.EnumerateFiles(data, "*", SearchOption.AllDirectories));
    }
}
