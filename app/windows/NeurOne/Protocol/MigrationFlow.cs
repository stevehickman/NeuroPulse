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

/// The calls a storage provider is asked for (SWR-MG-11), on named objects, with the bytes as lower-case hex. This is the whole
/// of what Windows writes for a cloud account, the local computer or a shared folder; the core holds the rest (object_route.rs).
/// A call that cannot be done (no network, no permission, a refused write) throws, and the flow ends with the route unavailable.
interface IObjectStore
{
    /// The object's bytes, or null when there is none.
    Task<string?> GetAsync(string name);
    /// Write the object, replacing any earlier one.
    Task PutAsync(string name, string data);
    /// Write the object if and only if none exists, arbitrated by the store itself (SWR-MG-07). True when this call wrote it.
    Task<bool> CreateIfAbsentAsync(string name, string data);
    /// Remove the object. Not an error when there is none.
    Task DeleteAsync(string name);
    Task<IReadOnlyList<string>> ListAsync(string prefix);
    /// Free space in bytes, or null when the provider cannot say.
    Task<long?> FreeSpaceAsync();
}

static class MigrationFlow
{
    // A restore of a large history is a few turns per item; this only stops a flow that never ends.
    private const int MaxTurns = 100_000;

    /// Run `flow` ("backup", "inspect", "restore" or "clear") to its end and return its `done` object. `secret` is what a
    /// backup or restore derives its key from; `keys` is the key-value store the plain-key items live in. When `keys` is
    /// given, the items the core says are plain keys are declared to it as `keyItems`.
    public static async Task<JsonObject> RunAsync(
        string flow, JsonObject parameters, IMigrationHost host, BundleSecret? secret = null, IMigrationKeyStore? keys = null,
        IObjectStore? store = null)
    {
        var p = (JsonObject)parameters.DeepClone();
        if (store is not null && p["objectStore"] is null) p["objectStore"] = true;
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
            var result = await PerformAsync(effect, host, secret, keys, store);
            answer = NppsCore.Migration(new JsonObject { ["op"] = "flow", ["flow"] = flow, ["state"] = state, ["result"] = result }).AsObject();
        }
        throw new NppsRefusal($"The {flow} flow did not end in {MaxTurns} turns");
    }

    private static Task<JsonObject> PerformAsync(JsonObject e, IMigrationHost host, BundleSecret? secret, IMigrationKeyStore? keys, IObjectStore? store)
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
            case "objectGet" or "objectPut" or "objectCreate" or "objectDelete" or "objectList" or "objectSpace" when store is not null:
                return PerformObjectAsync(e, store);
            default:
                return host.PerformAsync(e);
        }
    }

    // One object call. A call that throws is reported to the core as failed; the core decides what that means (SWR-MG-11).
    private static async Task<JsonObject> PerformObjectAsync(JsonObject e, IObjectStore store)
    {
        var name = (string?)e["name"] ?? "";
        try
        {
            switch ((string?)e["type"])
            {
                case "objectGet":
                    return new JsonObject { ["data"] = await store.GetAsync(name) };
                case "objectPut":
                    await store.PutAsync(name, (string)e["data"]!);
                    return new JsonObject { ["ok"] = true };
                case "objectCreate":
                    return new JsonObject { ["created"] = await store.CreateIfAbsentAsync(name, (string)e["data"]!) };
                case "objectDelete":
                    await store.DeleteAsync(name);
                    return new JsonObject { ["ok"] = true };
                case "objectList":
                    var names = new JsonArray();
                    foreach (var n in await store.ListAsync((string)e["prefix"]!)) names.Add(n);
                    return new JsonObject { ["names"] = names };
                default:
                    return new JsonObject { ["bytes"] = await store.FreeSpaceAsync() };
            }
        }
        catch (Exception ex) when (ex is not OperationCanceledException)
        {
            return new JsonObject { ["failed"] = true };
        }
    }

    /// The object names the core uses, for a host that releases a claim on the person's explicit act (SWR-MG-07).
    public static JsonObject ObjectNames() => NppsCore.Migration(new JsonObject { ["op"] = "objectNames" }).AsObject();

    /// SWR-MG-12: check an account on throwaway objects that hold no user data. `host` answers `random`, as for any flow.
    public static Task<JsonObject> ProbeAsync(IObjectStore store, long bundleBytes, IMigrationHost host) =>
        RunAsync("probe", new JsonObject { ["bundleBytes"] = bundleBytes }, host, null, null, store);
}
