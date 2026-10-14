// MigrationFlow.cs — the backup and device-replacement flows as Windows drives them (SWR-MG-02…10, issue #569 package 6e).
//
// The flows are state machines in the shared core (common/npps-core, migration_flow.rs): the ORDER (open the bundle before
// taking a claim, verify before confirming, confirm before the old phone clears) is decided there, once, for iOS, Android,
// Windows, macOS and the web. This file is the whole Windows driver: ask the core, perform the effect it names through the
// host, hand back the outcome, until the core says done. The host is where this app touches its own storage, its route,
// its Argon2 and its randomness; the table of effects is in migration_flow.rs. The iOS counterpart is
// app/ios/NeurOne/Session/MigrationFlow.swift, the web's common/lib/migrationFlow.ts, Kotlin's MigrationService.
//
// A flow state is opaque, is never stored, and never holds a secret or a key: the host keeps the secret, and a key is
// returned from deriveKey, used by the core for one answer and dropped.

using System.Text.Json.Nodes;

namespace NeurOne.Protocol;

/// Where this app performs the effects the core asks for.
interface IMigrationHost
{
    /// Perform one effect and return its result object.
    Task<JsonObject> PerformAsync(JsonObject effect);
}

static class MigrationFlow
{
    // A restore of a large history is a few turns per item; this only stops a flow that never ends.
    private const int MaxTurns = 100_000;

    /// Run `flow` ("backup", "inspect", "restore" or "clear") to its end and return its `done` object.
    public static async Task<JsonObject> RunAsync(string flow, JsonObject parameters, IMigrationHost host)
    {
        var answer = NppsCore.Migration(new JsonObject { ["op"] = "flow", ["flow"] = flow, ["params"] = parameters.DeepClone() }).AsObject();
        for (var turn = 0; turn < MaxTurns; turn++)
        {
            if (answer["done"] is JsonObject done) return done;
            var effect = answer["effect"]?.AsObject() ?? throw new NppsRefusal("the core named no effect");
            var state = answer["state"]?.DeepClone();
            var result = await host.PerformAsync(effect);
            answer = NppsCore.Migration(new JsonObject { ["op"] = "flow", ["flow"] = flow, ["state"] = state, ["result"] = result }).AsObject();
        }
        throw new NppsRefusal($"The {flow} flow did not end in {MaxTurns} turns");
    }
}
