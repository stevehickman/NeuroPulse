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

    // A route and a phone's randomness in memory: dumb storage that performs whatever effect the driver does not answer.
    private sealed class Host(Route route, int seed) : IMigrationHost
    {
        public readonly List<string> Seen = [];
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
                _ => throw new InvalidOperationException($"unknown effect {type}"),
            };
            return Task.FromResult(r);
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

    // @verifies SWR-MG-01
    [Fact]
    public async Task KeysThatBelongToTheInstallNeverMove()
    {
        var route = new Route();
        var oldStore = Filled();
        string[] installKeys = ["np.research.device-token", "np.warranty.consent-granted", "ble.lastHubId", "np.consumable.snooze-counts"];
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
