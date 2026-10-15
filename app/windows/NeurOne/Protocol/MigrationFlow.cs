// MigrationFlow.cs — the backup and device-replacement flows as Windows drives them (SWR-MG-02…10, issue #569 packages 6e, 6f).
//
// The flows are state machines in the shared core (common/npps-core, migration_flow.rs): the ORDER (open the bundle before
// taking a claim, verify before confirming, confirm before the old phone clears) is decided there, once, for iOS, Android,
// Windows, macOS and the web. This file is the whole Windows driver: ask the core, perform the effect it names, hand back
// the outcome, until the core says done. Three effects it answers itself, the same on every platform: deriveKey (the core's
// own Argon2id over the secret this driver holds), and readKeys / writeKeys (the app's key-value store, whose key list the
// core holds in common/npps/migration-keys.json). Everything else is the host's: the route, and any item the app stores
// another way. The table of effects is in migration_flow.rs. The iOS counterpart is app/ios/NeurOne/Session/MigrationFlow.swift,
// the web's common/lib/migrationFlow.ts, Kotlin's MigrationService.
//
// A flow state is opaque, is never stored, and never holds a secret or a key.

using System.Text.Json.Nodes;

namespace NeurOne.Protocol;

/// Where this app performs the effects the driver does not answer itself.
interface IMigrationHost
{
    /// Perform one effect and return its result object.
    Task<JsonObject> PerformAsync(JsonObject effect);
}

/// The app's key-value store, as the flows need it: get, put and remove, and nothing else.
interface IMigrationKeyStore
{
    string? Get(string key);
    void Put(string key, string value);
    void Remove(string key);
}

static class MigrationFlow
{
    // A restore of a large history is a few turns per item; this only stops a flow that never ends.
    private const int MaxTurns = 100_000;

    /// Run `flow` ("backup", "inspect", "restore" or "clear") to its end and return its `done` object. `secret` is what a
    /// backup or restore derives its key from; `keys` is the key-value store the plain-key items live in. When `keys` is
    /// given, the items the core says are plain keys are declared to it as `keyItems`.
    public static async Task<JsonObject> RunAsync(
        string flow, JsonObject parameters, IMigrationHost host, BundleSecret? secret = null, IMigrationKeyStore? keys = null)
    {
        var p = (JsonObject)parameters.DeepClone();
        if (keys is not null && p["keyItems"] is null)
        {
            var carried = NppsCore.Migration(new JsonObject { ["op"] = "itemKeys" })["carried"]!.AsObject();
            var items = new JsonArray();
            foreach (var (name, _) in carried) items.Add(name);
            p["keyItems"] = items;
        }
        var answer = NppsCore.Migration(new JsonObject { ["op"] = "flow", ["flow"] = flow, ["params"] = p }).AsObject();
        for (var turn = 0; turn < MaxTurns; turn++)
        {
            if (answer["done"] is JsonObject done) return done;
            var effect = answer["effect"]?.AsObject() ?? throw new NppsRefusal("the core named no effect");
            var state = answer["state"]?.DeepClone();
            var result = await PerformAsync(effect, host, secret, keys);
            answer = NppsCore.Migration(new JsonObject { ["op"] = "flow", ["flow"] = flow, ["state"] = state, ["result"] = result }).AsObject();
        }
        throw new NppsRefusal($"The {flow} flow did not end in {MaxTurns} turns");
    }

    private static Task<JsonObject> PerformAsync(JsonObject e, IMigrationHost host, BundleSecret? secret, IMigrationKeyStore? keys)
    {
        switch ((string?)e["type"])
        {
            case "deriveKey":
                if (secret is null) throw new NppsRefusal("no secret to derive a key from");
                using (var key = BundleKeyDerivation.DeriveKey(secret, e["kdf"]!.AsObject()))
                    return Task.FromResult(new JsonObject { ["key"] = key.Hex });
            case "readKeys" when keys is not null:
                var values = new JsonObject();
                foreach (var k in e["keys"]!.AsArray().Select(n => (string)n!))
                    if (keys.Get(k) is { } v) values[k] = v;
                return Task.FromResult(new JsonObject { ["values"] = values });
            case "writeKeys" when keys is not null:
                foreach (var k in e["remove"]!.AsArray().Select(n => (string)n!)) keys.Remove(k);
                foreach (var (k, v) in e["set"]!.AsObject()) keys.Put(k, (string)v!);
                return Task.FromResult(new JsonObject { ["ok"] = true });
            default:
                return host.PerformAsync(e);
        }
    }
}
