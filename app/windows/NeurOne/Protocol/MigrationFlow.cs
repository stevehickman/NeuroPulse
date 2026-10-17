// MigrationFlow.cs — the backup and device-replacement flows as Windows drives them (SWR-MG-02…10, issue #569 packages 6e, 6f).
//
// The flows are state machines in the shared core (common/npps-core, migration_flow.rs): the ORDER (open the bundle before
// taking a claim, verify before confirming, confirm before the old phone clears) is decided there, once, for iOS, Android,
// Windows, macOS and the web. This file is the whole Windows driver: ask the core, perform the effect it names, hand back
// the outcome, until the core says done. It answers these effects itself, the same on every platform: random (the platform's
// CSPRNG), deriveKey (the core's own Argon2id over the secret this driver holds), readKeys / writeKeys (the app's key-value
// store, whose key list the core holds in common/npps/migration-keys.json), the object, file and link calls.
// Everything else is the host's: the route, and any item the app stores
// another way. The table of effects is in migration_flow.rs. The iOS counterpart is app/ios/NeurOne/Session/MigrationFlow.swift,
// the web's common/lib/migrationFlow.ts, Kotlin's MigrationFlow.kt, under the same four names:
// MigrationFlow, MigrationHost, KeyValueStore and ObjectStore (the interfaces here carry an I prefix).
//
// A flow state is opaque, is never stored, and never holds a secret or a key.

using System.Security.Cryptography;
using System.Text.Json.Nodes;

namespace NeurOne.Protocol;

/// Where this app performs the effects the driver does not answer itself.
interface IMigrationHost
{
    /// Perform one effect and return its result object.
    Task<JsonObject> PerformAsync(JsonObject effect);
}

/// The app's key-value store, as the flows need it: get, put and remove, and nothing else. The same role and the same names on
/// every platform (Kotlin KeyValueStore, Swift KeyValueStore, TypeScript KeyValueStore).
interface IKeyValueStore
{
    string? GetString(string key);
    void PutString(string key, string value);
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

/// A file the app holds, as IFileStore.ListAsync reports it.
record StoredFile(string Path, long Size);

/// The calls the old phone's clear asks of the app's files (SWR-MG-06), and nothing else: the core decides how a file is
/// overwritten (every byte, in chunks, then a sync, then the delete). ListAsync reports the UHDR-class files still there. A call
/// that cannot be done throws, and the clear stops at that step and resumes at the next launch. The same role and names on
/// every platform (Kotlin FileStore, Swift FileStore, TypeScript FileStore).
interface IFileStore
{
    Task<IReadOnlyList<StoredFile>> ListAsync();
    /// Overwrite bytes.Length bytes of path starting at offset, in place.
    Task WriteAsync(string path, long offset, byte[] bytes);
    /// The writes so far have reached storage.
    Task SyncAsync(string path);
    /// Not an error when there is no such file.
    Task DeleteAsync(string path);
}

/// A pipe to the other phone (SWR-MG-10): bytes in, bytes out, as lower-case hex like the object store's. Bluetooth, a local
/// socket or a cable is each one implementation; everything above the bytes (framing, which calls a peer may make, the claim)
/// is in the core (link.rs). The same role and names on every platform (Kotlin Link, Swift Link, TypeScript Link).
interface ILink
{
    /// Send the bytes; throws if the link broke.
    Task SendAsync(string data);
    /// The next bytes, or null once the other end has closed; throws if the link broke.
    Task<string?> ReceiveAsync();
}

static class MigrationFlow
{
    // A restore of a large history is a few turns per item; this only stops a flow that never ends.
    private const int MaxTurns = 100_000;

    /// Run `flow` ("backup", "inspect", "restore" or "clear") to its end and return its `done` object. `secret` is what a
    /// backup or restore derives its key from; `keys` is the key-value store the plain-key items live in. When `keys` is
    /// given, the items the core says are plain keys are declared to it as `keyItems`.
    public static async Task<JsonObject> RunAsync(
        string flow, JsonObject parameters, IMigrationHost host, BundleSecret? secret = null, IKeyValueStore? keys = null,
        IObjectStore? store = null, IFileStore? files = null, ILink? link = null,
        Func<int, byte[]>? random = null)
    {
        var p = (JsonObject)parameters.DeepClone();
        if ((store is not null || link is not null) && p["objectStore"] is null) p["objectStore"] = true;
        if (link is not null && flow != "serve" && p["link"] is null) p["link"] = true;
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
            var result = await PerformAsync(effect, host, secret, keys, store, files, link, random ?? RandomNumberGenerator.GetBytes);
            answer = NppsCore.Migration(new JsonObject { ["op"] = "flow", ["flow"] = flow, ["state"] = state, ["result"] = result }).AsObject();
        }
        throw new NppsRefusal($"The {flow} flow did not end in {MaxTurns} turns");
    }

    private static Task<JsonObject> PerformAsync(JsonObject e, IMigrationHost host, BundleSecret? secret, IKeyValueStore? keys, IObjectStore? store, IFileStore? files, ILink? link,
        Func<int, byte[]> random)
    {
        switch ((string?)e["type"])
        {
            // The pairing code's expiry reads the platform clock (SWR-MG-10).
            case "now":
                return Task.FromResult(new JsonObject { ["seconds"] = DateTimeOffset.UtcNow.ToUnixTimeSeconds() });
            case "random":
                var n = (int)e["bytes"]!;
                var bytes = random(n);
                if (bytes.Length != n) throw new NppsRefusal($"The random source gave {bytes.Length} bytes, not {n}");
                return Task.FromResult(new JsonObject { ["bytes"] = Convert.ToHexString(bytes).ToLowerInvariant() });
            case "deriveKey":
                if (secret is null) throw new NppsRefusal("no secret to derive a key from");
                using (var key = BundleKeyDerivation.DeriveKey(secret, e["kdf"]!.AsObject()))
                    return Task.FromResult(new JsonObject { ["key"] = key.Hex });
            case "readKeys" when keys is not null:
                var values = new JsonObject();
                foreach (var k in e["keys"]!.AsArray().Select(n => (string)n!))
                    if (keys.GetString(k) is { } v) values[k] = v;
                return Task.FromResult(new JsonObject { ["values"] = values });
            case "writeKeys" when keys is not null:
                foreach (var k in e["remove"]!.AsArray().Select(n => (string)n!)) keys.Remove(k);
                foreach (var (k, v) in e["set"]!.AsObject()) keys.PutString(k, (string)v!);
                return Task.FromResult(new JsonObject { ["ok"] = true });
            case "objectGet" or "objectPut" or "objectCreate" or "objectDelete" or "objectList" or "objectSpace" when store is not null:
                return PerformObjectAsync(e, store);
            case "linkSend" or "linkRecv" when link is not null:
                return PerformLinkAsync(e, link);
            case "listFiles" or "writeFile" or "syncFile" or "deleteFile" when files is not null:
                return PerformFileAsync(e, files);
            default:
                return host.PerformAsync(e);
        }
    }

    // One link call. A link that breaks is reported to the core as failed; the core decides what that means (SWR-MG-11).
    private static async Task<JsonObject> PerformLinkAsync(JsonObject e, ILink link)
    {
        try
        {
            if ((string?)e["type"] == "linkSend")
            {
                await link.SendAsync((string)e["frame"]!);
                return new JsonObject { ["ok"] = true };
            }
            return new JsonObject { ["chunk"] = await link.ReceiveAsync() };
        }
        catch (Exception ex) when (ex is not OperationCanceledException)
        {
            return new JsonObject { ["failed"] = true };
        }
    }

    private sealed class NoHost : IMigrationHost
    {
        public Task<JsonObject> PerformAsync(JsonObject effect) =>
            throw new NppsRefusal("a serving phone is asked for no host effect");
    }

    /// SWR-MG-10: serve this phone's migration objects from `store` to the phone on the other end of `link`, until it closes,
    /// once that phone has proved it knows the `offer`'s code. Returns {ok, reason, paired, triesLeft}: the host keeps the
    /// offer across connections and passes the tries left back in, and ends it when `paired` or when none are left.
    public static Task<JsonObject> ServeAsync(ILink link, IObjectStore store, PairingOffer offer, Func<int, byte[]>? random = null) =>
        RunAsync("serve", new JsonObject { ["pairing"] = offer.ToJson() }, new NoHost(), null, null, store, null, link, random);

    /// The pairing code's digits, life and tries: digits, lifetimeSeconds, tries, randomBytes (SWR-MG-10).
    public static JsonObject PairingPolicy() => NppsCore.Migration(new JsonObject { ["op"] = "pairingPolicy" }).AsObject();

    /// A pairing code from `random` bytes (as many as the policy says).
    public static string PairingCode(byte[] random) =>
        (string)NppsCore.Migration(new JsonObject { ["op"] = "pairingCode", ["bytes"] = Convert.ToHexString(random).ToLowerInvariant() })["code"]!;

    /// The digits of a code a person typed, in any grouping, or null if it is not eight of them.
    public static string? ParsePairingCode(string text)
    {
        var r = NppsCore.Migration(new JsonObject { ["op"] = "pairingParse", ["text"] = text }).AsObject();
        return (bool)r["ok"]! ? (string)r["code"]! : null;
    }

    // One file call of the clear. A call that throws is reported to the core as failed, and the clear stops at that step (SWR-MG-06).
    private static async Task<JsonObject> PerformFileAsync(JsonObject e, IFileStore files)
    {
        var path = (string?)e["path"] ?? "";
        try
        {
            switch ((string?)e["type"])
            {
                case "listFiles":
                    var list = new JsonArray();
                    foreach (var f in await files.ListAsync()) list.Add(new JsonObject { ["path"] = f.Path, ["size"] = f.Size });
                    return new JsonObject { ["files"] = list };
                case "writeFile":
                    await files.WriteAsync(path, (long)e["offset"]!, Convert.FromHexString((string)e["data"]!));
                    return new JsonObject { ["ok"] = true };
                case "syncFile":
                    await files.SyncAsync(path);
                    return new JsonObject { ["ok"] = true };
                default:
                    await files.DeleteAsync(path);
                    return new JsonObject { ["ok"] = true };
            }
        }
        catch (Exception ex) when (ex is not OperationCanceledException)
        {
            return new JsonObject { ["failed"] = true };
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

    /// The locale key that words each reason, rejected secret, storage-check result and item (SWR-MG-03), from the core, so no
    /// platform spells one its own way. Groups: reasons, secret, probe, items; each maps a code to a key.
    public static JsonObject MessageKeys() => NppsCore.Migration(new JsonObject { ["op"] = "messageKeys" }).AsObject();

    /// The object names the core uses, for a host that releases a claim on the person's explicit act (SWR-MG-07).
    public static JsonObject ObjectNames() => NppsCore.Migration(new JsonObject { ["op"] = "objectNames" }).AsObject();

    /// SWR-MG-12: check an account on throwaway objects that hold no user data.
    public static Task<JsonObject> ProbeAsync(IObjectStore store, long bundleBytes, IMigrationHost host, Func<int, byte[]>? random = null) =>
        RunAsync("probe", new JsonObject { ["bundleBytes"] = bundleBytes }, host, null, null, store, random: random);
}

/// What the phone that holds the bundle shows, and keeps until it is spent (SWR-MG-10). `ExpiresAt` is Unix seconds;
/// `TriesLeft` is what is left after the sessions already tried (each one costs a try, wrong or not).
public sealed record PairingOffer(string Code, long ExpiresAt, int TriesLeft)
{
    internal JsonObject ToJson() => new() { ["code"] = Code, ["expiresAt"] = ExpiresAt, ["triesLeft"] = TriesLeft };

    /// The offer after a serve ended: the same code and expiry with the tries the core says are left, or null once it is spent.
    public PairingOffer? After(JsonObject result)
    {
        if (result["paired"]?.GetValue<bool>() == true) return null;
        var left = result["triesLeft"]?.GetValue<int>() ?? TriesLeft;
        if (left <= 0 || (string?)result["reason"] == "code_expired") return null;
        return this with { TriesLeft = left };
    }

    /// A new offer: the core's code from the platform's randomness, valid for the core's lifetime and tries.
    public static PairingOffer Make(Func<int, byte[]>? random = null, DateTimeOffset? now = null)
    {
        var policy = MigrationFlow.PairingPolicy();
        var bytes = (random ?? RandomNumberGenerator.GetBytes)((int)policy["randomBytes"]!);
        return new PairingOffer(
            MigrationFlow.PairingCode(bytes),
            (now ?? DateTimeOffset.UtcNow).ToUnixTimeSeconds() + (long)policy["lifetimeSeconds"]!,
            (int)policy["tries"]!);
    }

    /// The code as shown: the digits in two groups, so it is easy to read out and to type.
    public string Display => Code[..(Code.Length / 2)] + " " + Code[(Code.Length / 2)..];
}
