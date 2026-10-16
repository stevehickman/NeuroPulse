using System.Text.Json.Nodes;
using System.Threading.Channels;
using NeurOne.Protocol;
using Xunit;

namespace NeurOne.Tests;

// The backup and device-replacement flows through the Windows driver and neurone_npps_ffi.dll. The Rust tests hold the
// ORDER of the flow once for every app (migration_flow.rs); this holds the Windows path: the driver loop, the effects it
// answers itself (the core's Argon2id key, the key-value store) and the effect shapes the host sees. The same cases run on
// Kotlin, iOS and the web.
public class MigrationFlowTests
{
    private sealed class Route
    {
        public string? Bundle;
        public JsonNode? Claim;
        public JsonNode? Confirmation;
    }

    private sealed class KeyStore : IKeyValueStore
    {
        public readonly Dictionary<string, string> Values = [];
        public string? GetString(string key) => Values.GetValueOrDefault(key);
        public void PutString(string key, string value) => Values[key] = value;
        public void Remove(string key) => Values.Remove(key);
    }

    // The app's files in memory, with every call recorded in order and one call that can be made to throw.
    private sealed class MemoryFiles : IFileStore
    {
        public readonly Dictionary<string, byte[]> Files = [];
        public readonly List<string> Events = [];
        public string? FailOn;
        private void Check(string call, string path) { if (FailOn == $"{call}:{path}") throw new InvalidOperationException(call); }
        public Task<IReadOnlyList<StoredFile>> ListAsync() =>
            Task.FromResult<IReadOnlyList<StoredFile>>(Files.OrderBy(f => f.Key).Select(f => new StoredFile(f.Key, f.Value.Length)).ToList());
        public Task WriteAsync(string path, long offset, byte[] bytes)
        {
            Check("write", path);
            bytes.CopyTo(Files[path], (int)offset);
            Events.Add($"write:{path}:{offset}:{bytes.Length}");
            return Task.CompletedTask;
        }
        public Task SyncAsync(string path) { Check("sync", path); Events.Add($"sync:{path}"); return Task.CompletedTask; }
        public Task DeleteAsync(string path) { Check("delete", path); Files.Remove(path); Events.Add($"delete:{path}"); return Task.CompletedTask; }
    }

    // A provider with named objects and a create-if-absent the store itself arbitrates.
    private sealed class MemoryStore : IObjectStore
    {
        public readonly Dictionary<string, string> Objects = [];
        public bool Conditional = true;
        public long? Space = 1L << 30;
        public string? FailOn;
        private void Check(string call) { if (FailOn == call) throw new InvalidOperationException(call); }
        public Task<string?> GetAsync(string name) { Check("get"); return Task.FromResult(Objects.GetValueOrDefault(name)); }
        public Task PutAsync(string name, string data) { Check("put"); Objects[name] = data; return Task.CompletedTask; }
        public Task<bool> CreateIfAbsentAsync(string name, string data)
        {
            Check("create");
            if (Conditional && Objects.ContainsKey(name)) return Task.FromResult(false);
            Objects[name] = data;
            return Task.FromResult(true);
        }
        public Task DeleteAsync(string name) { Check("delete"); Objects.Remove(name); return Task.CompletedTask; }
        public Task<IReadOnlyList<string>> ListAsync(string prefix) { Check("list"); return Task.FromResult<IReadOnlyList<string>>(Objects.Keys.Where(k => k.StartsWith(prefix)).ToList()); }
        public Task<long?> FreeSpaceAsync() { Check("space"); return Task.FromResult(Space); }
    }

    // A route and a phone's randomness in memory: dumb storage that performs whatever effect the driver does not answer.
    // One end of an in-memory pipe that delivers a few bytes at a time (hex), so a frame always arrives in pieces.
    private sealed class PipeEnd(Channel<string> output, Channel<string> input) : ILink
    {
        public bool Broken { get; set; }
        public async Task SendAsync(string data)
        {
            if (Broken) throw new InvalidOperationException("the link is broken");
            for (var i = 0; i < data.Length; i += 14) await output.Writer.WriteAsync(data.Substring(i, Math.Min(14, data.Length - i)));
        }
        public async Task<string?> ReceiveAsync()
        {
            if (Broken) throw new InvalidOperationException("the link is broken");
            return await input.Reader.WaitToReadAsync() ? await input.Reader.ReadAsync() : null;
        }
        public void Close() => output.Writer.Complete();
    }

    private static (PipeEnd Client, PipeEnd Server) Pipe()
    {
        var ab = Channel.CreateUnbounded<string>();
        var ba = Channel.CreateUnbounded<string>();
        return (new PipeEnd(ab, ba), new PipeEnd(ba, ab));
    }

    private sealed class Host(Route route) : IMigrationHost
    {
        public readonly List<string> Seen = [];
        public readonly List<string> Ran = [];

        public Task<JsonObject> PerformAsync(JsonObject e)
        {
            var type = (string)e["type"]!;
            Seen.Add(type);
            JsonObject r = type switch
            {
                "getBundle" => new JsonObject { ["bundle"] = route.Bundle },
                "putBundle" => Put(e),
                "readClaim" => new JsonObject { ["claim"] = route.Claim?.DeepClone() },
                "createClaim" => CreateClaim(e),
                "writeConfirmation" => Confirm(e),
                "readConfirmation" => new JsonObject { ["confirmation"] = route.Confirmation?.DeepClone() },
                "runClearStep" => RunStep(e),
                "listFiles" => new JsonObject { ["files"] = new JsonArray() },
                _ => throw new InvalidOperationException($"unknown effect {type}"),
            };
            return Task.FromResult(r);
        }

        private JsonObject RunStep(JsonObject e)
        {
            Ran.Add((string)e["step"]!);
            return new JsonObject { ["ok"] = true };
        }

        private JsonObject Put(JsonObject e)
        {
            route.Bundle = (string)e["bundle"]!;
            return new JsonObject { ["ok"] = true };
        }

        private JsonObject CreateClaim(JsonObject e)
        {
            var created = route.Claim is null;
            if (created) route.Claim = e["claim"]!.DeepClone();
            return new JsonObject { ["created"] = created };
        }

        private JsonObject Confirm(JsonObject e)
        {
            route.Confirmation = e["confirmation"]!.DeepClone();
            return new JsonObject { ["ok"] = true };
        }
    }

    private static JsonObject Params(params (string Key, JsonNode? Value)[] extra)
    {
        var p = new JsonObject
        {
            ["route"] = new JsonObject { ["id"] = "memory", ["atomicClaim"] = true, ["direct"] = false, ["authenticatedPairing"] = true },
        };
        foreach (var (k, v) in extra) p[k] = v;
        return p;
    }

    private static BundleSecret Secret(string text) => BundleKeyDerivation.PassphraseSecret(text).Secret!;

    private static KeyStore Filled()
    {
        var store = new KeyStore();
        var carried = NppsCore.Migration(new JsonObject { ["op"] = "itemKeys" })["carried"]!.AsObject();
        foreach (var (_, keys) in carried)
            foreach (var k in keys!.AsArray().Select(n => (string)n!)) store.PutString(k, $"value-of-{k}");
        return store;
    }

    // @verifies SWR-MG-02
    // @verifies SWR-MG-08
    [Fact]
    public async Task AMigrationMovesAPhoneThroughTheSharedFlowAndEndsWithAConfirmationOfTheBundleHash()
    {
        var route = new Route();
        var oldStore = Filled();
        using var oldSecret = Secret("a long enough passphrase");
        var done = await MigrationFlow.RunAsync("backup", Params(("kind", "migrate"), ("method", "passphrase")), new Host(route), oldSecret, oldStore);
        Assert.True((bool)done["ok"]!);

        var fresh = new KeyStore();
        using var freshSecret = Secret("a long enough passphrase");
        var r = await MigrationFlow.RunAsync("restore", Params(), new Host(route), freshSecret, fresh);
        Assert.True((bool)r["ok"]!);
        Assert.True((bool)r["confirmed"]!);
        Assert.Equal(oldStore.Values.OrderBy(kv => kv.Key), fresh.Values.OrderBy(kv => kv.Key));
        Assert.Equal((string)done["bundleHash"]!, (string)route.Confirmation!["bundleHash"]!);
    }

    // @verifies SWR-MG-08
    // @verifies SWR-MG-09
    [Fact]
    public async Task TheOldPhoneClearsThroughTheSharedFlowAndTheHostDoesOnlyItsOwnSteps()
    {
        var route = new Route();
        var oldStore = Filled();
        oldStore.PutString("ble.last-hub-id", "hub-1");
        using var oldSecret = Secret("a long enough passphrase");
        var done = await MigrationFlow.RunAsync("backup", Params(("kind", "migrate"), ("method", "passphrase")), new Host(route), oldSecret, oldStore);
        var clearParams = new JsonObject { ["bundleHash"] = (string)done["bundleHash"]!, ["canRemoveOsBond"] = false };

        var before = await MigrationFlow.RunAsync("clear", clearParams.DeepClone().AsObject(), new Host(route), null, oldStore);
        Assert.Equal("no_confirmation", (string)before["reason"]!);

        using var freshSecret = Secret("a long enough passphrase");
        await MigrationFlow.RunAsync("restore", Params(), new Host(route), freshSecret, new KeyStore());
        var host = new Host(route);
        var after = await MigrationFlow.RunAsync("clear", clearParams, host, null, oldStore);
        Assert.True((bool)after["ok"]!);
        Assert.True((bool)after["removeOsBond"]!);
        Assert.Equal(["destroyEncryptionKey", "forgetPairingRecord"], host.Ran);
        Assert.Equal(["np.migration.cleared"], oldStore.Values.Keys.ToArray());
    }

    // @verifies SWR-MG-10
    // @verifies SWR-MG-11
    // @verifies SWR-MG-07
    [Fact]
    public async Task AMigrationRunsOverAnObjectStoreTheAppOnlyHasToProvideAndASecondRestoreIsRefused()
    {
        var store = new MemoryStore();
        var oldStore = Filled();
        using var oldSecret = Secret("a long enough passphrase");
        var done = await MigrationFlow.RunAsync("backup", Params(("kind", "migrate"), ("method", "passphrase")), new Host(new Route()), oldSecret, oldStore, store);
        Assert.True((bool)done["ok"]!);
        Assert.Equal([(string)MigrationFlow.ObjectNames()["bundle"]!], store.Objects.Keys.ToArray());

        var fresh = new KeyStore();
        var host = new Host(new Route());
        using var freshSecret = Secret("a long enough passphrase");
        var r = await MigrationFlow.RunAsync("restore", Params(), host, freshSecret, fresh, store);
        Assert.True((bool)r["ok"]!);
        Assert.True((bool)r["confirmed"]!);
        Assert.Equal(oldStore.Values.OrderBy(kv => kv.Key), fresh.Values.OrderBy(kv => kv.Key));
        Assert.DoesNotContain(host.Seen, t => t is "getBundle" or "putBundle" or "readClaim" or "createClaim");

        using var thirdSecret = Secret("a long enough passphrase");
        var second = await MigrationFlow.RunAsync("restore", Params(), new Host(new Route()), thirdSecret, new KeyStore(), store);
        Assert.Equal("claimed", (string)second["reason"]!);
    }

    // @verifies SWR-MG-10
    // @verifies SWR-MG-11
    // @verifies SWR-MG-07
    [Fact]
    public async Task AMigrationRunsOverADirectLinkInSmallPiecesAndASecondRestoreIsRefused()
    {
        var store = new MemoryStore();
        var oldStore = Filled();
        using var oldSecret = Secret("a long enough passphrase");
        var done = await MigrationFlow.RunAsync("backup", Params(("kind", "migrate"), ("method", "passphrase")), new Host(new Route()), oldSecret, oldStore, store);
        Assert.True((bool)done["ok"]!);

        async Task<JsonObject> RestoreOver(KeyStore keys)
        {
            var (client, server) = Pipe();
            var serving = MigrationFlow.ServeAsync(server, store, true);
            using var secret = Secret("a long enough passphrase");
            var r = await MigrationFlow.RunAsync("restore", Params(), new Host(new Route()), secret, keys, link: client);
            client.Close();
            Assert.Equal("closed", (string)(await serving)["reason"]!);
            return r;
        }
        var fresh = new KeyStore();
        var r = await RestoreOver(fresh);
        Assert.True((bool)r["ok"]!);
        Assert.True((bool)r["confirmed"]!);
        Assert.Equal(oldStore.Values.OrderBy(kv => kv.Key), fresh.Values.OrderBy(kv => kv.Key));
        var names = MigrationFlow.ObjectNames();
        Assert.Contains((string)names["claim"]!, store.Objects.Keys);
        Assert.Contains((string)names["confirmation"]!, store.Objects.Keys);
        Assert.Equal("claimed", (string)(await RestoreOver(new KeyStore()))["reason"]!);
    }

    // @verifies SWR-MG-10
    [Fact]
    public async Task AServingPhoneWhoseLinkIsNotAuthenticatedServesNothing()
    {
        var (_, server) = Pipe();
        var r = await MigrationFlow.ServeAsync(server, new MemoryStore(), false);
        Assert.False((bool)r["ok"]!);
        Assert.Equal("not_authenticated", (string)r["reason"]!);
    }

    // @verifies SWR-MG-11
    [Fact]
    public async Task ALinkThatBreaksDuringARestoreEndsItWithTheRouteUnavailable()
    {
        var (client, _) = Pipe();
        client.Broken = true;
        using var secret = Secret("a long enough passphrase");
        var r = await MigrationFlow.RunAsync("restore", Params(), new Host(new Route()), secret, new KeyStore(), link: client);
        Assert.Equal("route_unavailable", (string)r["reason"]!);
    }

    // @verifies SWR-MG-11
    [Fact]
    public async Task AnObjectCallThatThrowsEndsTheFlowWithTheRouteUnavailable()
    {
        var store = new MemoryStore { FailOn = "put" };
        using var oldSecret = Secret("a long enough passphrase");
        var r = await MigrationFlow.RunAsync("backup", Params(("kind", "migrate"), ("method", "passphrase")), new Host(new Route()), oldSecret, Filled(), store);
        Assert.Equal("route_unavailable", (string)r["reason"]!);
    }

    // @verifies SWR-MG-12
    [Fact]
    public async Task TheProbeSaysWhatAnAccountCanDoAndLeavesNothingBehind()
    {
        var good = new MemoryStore();
        good.Objects["users-file"] = "01";
        var r = await MigrationFlow.ProbeAsync(good, 1000, new Host(new Route()));
        Assert.Equal("sufficient", (string)r["result"]!);
        Assert.Equal(["users-file"], good.Objects.Keys.ToArray());

        var noConditional = new MemoryStore { Conditional = false };
        r = await MigrationFlow.ProbeAsync(noConditional, 1000, new Host(new Route()));
        Assert.Equal("backup_only", (string)r["result"]!);
        Assert.Equal("createIfAbsentSecondRefused", (string)r["failedOperation"]!);
        Assert.Empty(noConditional.Objects);

        r = await MigrationFlow.ProbeAsync(new MemoryStore { Space = 10 }, 1000, new Host(new Route()));
        Assert.Equal("space", (string)r["failedOperation"]!);
    }

    // @verifies SWR-MG-06
    // @verifies SWR-MG-08
    [Fact]
    public async Task TheFilesAreOverwrittenInFullSyncedAndDeletedAfterTheKeyAndAFailedCallResumesAtTheNextLaunch()
    {
        var route = new Route();
        var oldStore = Filled();
        using var oldSecret = Secret("a long enough passphrase");
        var done = await MigrationFlow.RunAsync("backup", Params(("kind", "migrate"), ("method", "passphrase")), new Host(route), oldSecret, oldStore);
        using var freshSecret = Secret("a long enough passphrase");
        await MigrationFlow.RunAsync("restore", Params(), new Host(route), freshSecret, new KeyStore());
        JsonObject ClearParams() => new() { ["bundleHash"] = (string)done["bundleHash"]!, ["canRemoveOsBond"] = true };

        var files = new MemoryFiles { FailOn = "delete:c-small.db" };
        files.Files["a-big.db"] = Enumerable.Repeat((byte)0x11, 40_000).ToArray();
        files.Files["b-empty.db"] = [];
        files.Files["c-small.db"] = Enumerable.Repeat((byte)0x22, 100).ToArray();
        var host = new Host(route);
        var r1 = await MigrationFlow.RunAsync("clear", ClearParams(), host, null, oldStore, null, files);
        Assert.False((bool)r1["ok"]!);
        Assert.Equal("overwriteAndDeleteFiles", (string)r1["step"]!);
        Assert.Equal(["c-small.db"], files.Files.Keys.ToArray());
        Assert.Equal(["destroyEncryptionKey"], host.Ran);

        var writes = files.Events.Where(e => e.StartsWith("write:a-big.db:")).Select(e => e.Split(':').Skip(2).Select(int.Parse).ToArray()).ToList();
        Assert.Equal(40_000, writes.Sum(w => w[1]));
        Assert.All(writes, w => Assert.True(w[1] <= 16 * 1024));
        Assert.True(files.Events.IndexOf("sync:a-big.db") < files.Events.IndexOf("delete:a-big.db"));
        Assert.Equal(["delete:b-empty.db"], files.Events.Where(e => e.Contains("b-empty.db")).ToArray());

        files.FailOn = null;
        var second = new Host(new Route());
        var r2 = await MigrationFlow.RunAsync("clear", ClearParams(), second, null, oldStore, null, files);
        Assert.True((bool)r2["ok"]!);
        Assert.Empty(files.Files);
        Assert.Equal(["forgetPairingRecord"], second.Ran);
    }

    // @verifies SWR-MG-01
    [Fact]
    public async Task KeysThatBelongToTheInstallNeverMove()
    {
        var route = new Route();
        var oldStore = Filled();
        string[] installKeys = ["np.research.device-token", "np.warranty.consent-granted", "ble.last-hub-id", "np.consumable.snooze-counts"];
        foreach (var k in installKeys) oldStore.PutString(k, "belongs-to-this-phone");
        using var oldSecret = Secret("a long enough passphrase");
        await MigrationFlow.RunAsync("backup", Params(("kind", "migrate"), ("method", "passphrase")), new Host(route), oldSecret, oldStore);
        var fresh = new KeyStore();
        using var freshSecret = Secret("a long enough passphrase");
        await MigrationFlow.RunAsync("restore", Params(), new Host(route), freshSecret, fresh);
        foreach (var k in installKeys) Assert.Null(fresh.GetString(k));
        Assert.Equal("value-of-np.session.history", fresh.GetString("np.session.history"));
    }

    // @verifies SWR-MG-07
    [Fact]
    public async Task AWrongSecretOpensNothingAndLeavesNoClaimBehind()
    {
        var route = new Route();
        using var oldSecret = Secret("a long enough passphrase");
        await MigrationFlow.RunAsync("backup", Params(("kind", "migrate"), ("method", "passphrase")), new Host(route), oldSecret, Filled());
        var wrong = new Host(route);
        using var wrongSecret = Secret("a different passphrase");
        var r = await MigrationFlow.RunAsync("restore", Params(), wrong, wrongSecret, new KeyStore());
        Assert.False((bool)r["ok"]!);
        Assert.Equal("authentication_failed", (string)r["reason"]!);
        Assert.Null(route.Claim);
        Assert.DoesNotContain("createClaim", wrong.Seen);
    }
}
