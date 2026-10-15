using System.Text.Json.Nodes;
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

    private sealed class KeyStore : IMigrationKeyStore
    {
        public readonly Dictionary<string, string> Values = [];
        public string? Get(string key) => Values.GetValueOrDefault(key);
        public void Put(string key, string value) => Values[key] = value;
        public void Remove(string key) => Values.Remove(key);
    }

    // A provider with named objects and a create-if-absent the store itself arbitrates.
    private sealed class MemoryStore : IMigrationObjectStore
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
    private sealed class Host(Route route, int seed) : IMigrationHost
    {
        public readonly List<string> Seen = [];
        public readonly List<string> Ran = [];
        private int _n = seed;

        public Task<JsonObject> PerformAsync(JsonObject e)
        {
            var type = (string)e["type"]!;
            Seen.Add(type);
            JsonObject r = type switch
            {
                "random" => new JsonObject { ["bytes"] = Convert.ToHexString(Enumerable.Range(0, (int)e["bytes"]!).Select(_ => Next()).ToArray()).ToLowerInvariant() },
                "getBundle" => new JsonObject { ["bundle"] = route.Bundle },
                "putBundle" => Put(e),
                "readClaim" => new JsonObject { ["claim"] = route.Claim?.DeepClone() },
                "createClaim" => CreateClaim(e),
                "writeConfirmation" => Confirm(e),
                "readConfirmation" => new JsonObject { ["confirmation"] = route.Confirmation?.DeepClone() },
                "runClearStep" => RunStep(e),
                _ => throw new InvalidOperationException($"unknown effect {type}"),
            };
            return Task.FromResult(r);
        }

        private JsonObject RunStep(JsonObject e)
        {
            Ran.Add((string)e["step"]!);
            return new JsonObject { ["ok"] = true };
        }

        private byte Next() => (byte)(_n = (_n * 31 + 7) & 255);

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
            foreach (var k in keys!.AsArray().Select(n => (string)n!)) store.Put(k, $"value-of-{k}");
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
        var done = await MigrationFlow.RunAsync("backup", Params(("kind", "migrate"), ("method", "passphrase")), new Host(route, 1), oldSecret, oldStore);
        Assert.True((bool)done["ok"]!);

        var fresh = new KeyStore();
        using var freshSecret = Secret("a long enough passphrase");
        var r = await MigrationFlow.RunAsync("restore", Params(), new Host(route, 50), freshSecret, fresh);
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
        oldStore.Put("ble.last-hub-id", "hub-1");
        using var oldSecret = Secret("a long enough passphrase");
        var done = await MigrationFlow.RunAsync("backup", Params(("kind", "migrate"), ("method", "passphrase")), new Host(route, 1), oldSecret, oldStore);
        var clearParams = new JsonObject { ["bundleHash"] = (string)done["bundleHash"]!, ["canRemoveOsBond"] = false };

        var before = await MigrationFlow.RunAsync("clear", clearParams.DeepClone().AsObject(), new Host(route, 1), null, oldStore);
        Assert.Equal("no_confirmation", (string)before["reason"]!);

        using var freshSecret = Secret("a long enough passphrase");
        await MigrationFlow.RunAsync("restore", Params(), new Host(route, 50), freshSecret, new KeyStore());
        var host = new Host(route, 1);
        var after = await MigrationFlow.RunAsync("clear", clearParams, host, null, oldStore);
        Assert.True((bool)after["ok"]!);
        Assert.True((bool)after["removeOsBond"]!);
        Assert.Equal(["destroyEncryptionKey", "overwriteAndDeleteFiles", "forgetPairingRecord"], host.Ran);
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
        var done = await MigrationFlow.RunAsync("backup", Params(("kind", "migrate"), ("method", "passphrase")), new Host(new Route(), 1), oldSecret, oldStore, store);
        Assert.True((bool)done["ok"]!);
        Assert.Equal([(string)MigrationFlow.ObjectNames()["bundle"]!], store.Objects.Keys.ToArray());

        var fresh = new KeyStore();
        var host = new Host(new Route(), 50);
        using var freshSecret = Secret("a long enough passphrase");
        var r = await MigrationFlow.RunAsync("restore", Params(), host, freshSecret, fresh, store);
        Assert.True((bool)r["ok"]!);
        Assert.True((bool)r["confirmed"]!);
        Assert.Equal(oldStore.Values.OrderBy(kv => kv.Key), fresh.Values.OrderBy(kv => kv.Key));
        Assert.DoesNotContain(host.Seen, t => t is "getBundle" or "putBundle" or "readClaim" or "createClaim");

        using var thirdSecret = Secret("a long enough passphrase");
        var second = await MigrationFlow.RunAsync("restore", Params(), new Host(new Route(), 90), thirdSecret, new KeyStore(), store);
        Assert.Equal("claimed", (string)second["reason"]!);
    }

    // @verifies SWR-MG-11
    [Fact]
    public async Task AnObjectCallThatThrowsEndsTheFlowWithTheRouteUnavailable()
    {
        var store = new MemoryStore { FailOn = "put" };
        using var oldSecret = Secret("a long enough passphrase");
        var r = await MigrationFlow.RunAsync("backup", Params(("kind", "migrate"), ("method", "passphrase")), new Host(new Route(), 1), oldSecret, Filled(), store);
        Assert.Equal("route_unavailable", (string)r["reason"]!);
    }

    // @verifies SWR-MG-12
    [Fact]
    public async Task TheProbeSaysWhatAnAccountCanDoAndLeavesNothingBehind()
    {
        var good = new MemoryStore();
        good.Objects["users-file"] = "01";
        var r = await MigrationFlow.ProbeAsync(good, 1000, new Host(new Route(), 7));
        Assert.Equal("sufficient", (string)r["result"]!);
        Assert.Equal(["users-file"], good.Objects.Keys.ToArray());

        var noConditional = new MemoryStore { Conditional = false };
        r = await MigrationFlow.ProbeAsync(noConditional, 1000, new Host(new Route(), 7));
        Assert.Equal("backup_only", (string)r["result"]!);
        Assert.Equal("createIfAbsentSecondRefused", (string)r["failedOperation"]!);
        Assert.Empty(noConditional.Objects);

        r = await MigrationFlow.ProbeAsync(new MemoryStore { Space = 10 }, 1000, new Host(new Route(), 7));
        Assert.Equal("space", (string)r["failedOperation"]!);
    }

    // @verifies SWR-MG-01
    [Fact]
    public async Task KeysThatBelongToTheInstallNeverMove()
    {
        var route = new Route();
        var oldStore = Filled();
        string[] installKeys = ["np.research.device-token", "np.warranty.consent-granted", "ble.last-hub-id", "np.consumable.snooze-counts"];
        foreach (var k in installKeys) oldStore.Put(k, "belongs-to-this-phone");
        using var oldSecret = Secret("a long enough passphrase");
        await MigrationFlow.RunAsync("backup", Params(("kind", "migrate"), ("method", "passphrase")), new Host(route, 1), oldSecret, oldStore);
        var fresh = new KeyStore();
        using var freshSecret = Secret("a long enough passphrase");
        await MigrationFlow.RunAsync("restore", Params(), new Host(route, 50), freshSecret, fresh);
        foreach (var k in installKeys) Assert.Null(fresh.Get(k));
        Assert.Equal("value-of-np.session.history", fresh.Get("np.session.history"));
    }

    // @verifies SWR-MG-07
    [Fact]
    public async Task AWrongSecretOpensNothingAndLeavesNoClaimBehind()
    {
        var route = new Route();
        using var oldSecret = Secret("a long enough passphrase");
        await MigrationFlow.RunAsync("backup", Params(("kind", "migrate"), ("method", "passphrase")), new Host(route, 1), oldSecret, Filled());
        var wrong = new Host(route, 50);
        using var wrongSecret = Secret("a different passphrase");
        var r = await MigrationFlow.RunAsync("restore", Params(), wrong, wrongSecret, new KeyStore());
        Assert.False((bool)r["ok"]!);
        Assert.Equal("authentication_failed", (string)r["reason"]!);
        Assert.Null(route.Claim);
        Assert.DoesNotContain("createClaim", wrong.Seen);
    }
}
