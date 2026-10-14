using System.Text.Json.Nodes;
using NeurOne.Protocol;
using Xunit;

namespace NeurOne.Tests;

// The backup and device-replacement flows through the Windows driver and neurone_npps_ffi.dll. The Rust tests hold the
// ORDER of the flow once for every app (migration_flow.rs); this holds the Windows path: the driver loop and the effect
// shapes the host sees. The same cases run on Kotlin, iOS and the web.
public class MigrationFlowTests
{
    private static readonly string[] HostItems = ["protocols", "sessionHistory", "limitsProfiles", "characterisationEnrolment"];

    private sealed class Route
    {
        public string? Bundle;
        public JsonNode? Claim;
        public JsonNode? Confirmation;
    }

    // A phone and a route in memory: dumb storage that performs whatever effect it is asked for.
    private sealed class Host(Route route, string secret, int seed) : IMigrationHost
    {
        public readonly Dictionary<string, JsonNode> Items = [];
        public readonly List<string> Seen = [];
        private int _n = seed;

        public Task<JsonObject> PerformAsync(JsonObject e)
        {
            var type = (string)e["type"]!;
            Seen.Add(type);
            JsonObject r = type switch
            {
                "random" => new JsonObject { ["bytes"] = Hex(Enumerable.Range(0, (int)e["bytes"]!).Select(_ => Next()).ToArray()) },
                // Not Argon2: a stand-in that depends on the secret and the salt, as the real one does.
                "deriveKey" => new JsonObject { ["key"] = Hex(Enumerable.Range(0, 32).Select(i => Stand(secret + (string)e["kdf"]!["salt"]!, i)).ToArray()) },
                "readItems" => ReadItems(e),
                "applyItem" => Apply(e),
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

        private static byte Stand(string text, int i) => (byte)(text[i % text.Length] * 7 + i);

        private static string Hex(byte[] bytes) => Convert.ToHexString(bytes).ToLowerInvariant();

        private JsonObject ReadItems(JsonObject e)
        {
            var items = new JsonObject();
            foreach (var name in e["names"]!.AsArray().Select(n => (string)n!))
                if (Items.TryGetValue(name, out var v)) items[name] = v.DeepClone();
            return new JsonObject { ["items"] = items };
        }

        private JsonObject Apply(JsonObject e)
        {
            Items[(string)e["name"]!] = e["value"]!.DeepClone();
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
        var items = new JsonArray();
        foreach (var n in HostItems) items.Add(n);
        var p = new JsonObject
        {
            ["route"] = new JsonObject { ["id"] = "memory", ["atomicClaim"] = true, ["direct"] = false, ["authenticatedPairing"] = true },
            ["hostItems"] = items,
        };
        foreach (var (k, v) in extra) p[k] = v;
        return p;
    }

    private static Host Phone(Route route, string secret, int seed, bool filled)
    {
        var h = new Host(route, secret, seed);
        if (filled) foreach (var n in HostItems) h.Items[n] = new JsonObject { [$"np.{n}"] = $"value-of-{n}" };
        return h;
    }

    // @verifies SWR-MG-02
    // @verifies SWR-MG-08
    [Fact]
    public async Task AMigrationMovesAPhoneThroughTheSharedFlowAndEndsWithAConfirmationOfTheBundleHash()
    {
        var route = new Route();
        var old = Phone(route, "a long enough passphrase", 1, filled: true);
        var done = await MigrationFlow.RunAsync("backup", Params(("kind", "migrate"), ("method", "passphrase")), old);
        Assert.True((bool)done["ok"]!);

        var fresh = Phone(route, "a long enough passphrase", 50, filled: false);
        var r = await MigrationFlow.RunAsync("restore", Params(), fresh);
        Assert.True((bool)r["ok"]!);
        Assert.True((bool)r["confirmed"]!);
        Assert.Equal(old.Items.Count, fresh.Items.Count);
        foreach (var (name, value) in old.Items) Assert.True(JsonNode.DeepEquals(value, fresh.Items[name]), name);
        Assert.Equal((string)done["bundleHash"]!, (string)route.Confirmation!["bundleHash"]!);
    }

    // @verifies SWR-MG-07
    [Fact]
    public async Task AWrongSecretOpensNothingAndLeavesNoClaimBehind()
    {
        var route = new Route();
        var old = Phone(route, "a long enough passphrase", 1, filled: true);
        await MigrationFlow.RunAsync("backup", Params(("kind", "migrate"), ("method", "passphrase")), old);
        var wrong = Phone(route, "a different passphrase", 50, filled: false);
        var r = await MigrationFlow.RunAsync("restore", Params(), wrong);
        Assert.False((bool)r["ok"]!);
        Assert.Equal("authentication_failed", (string)r["reason"]!);
        Assert.Null(route.Claim);
        Assert.DoesNotContain("createClaim", wrong.Seen);
    }
}
