import Foundation

// The backup and device-replacement flows as iOS drives them (SWR-MG-02…10, issue #569 package 6e, 6f).
//
// The flows are state machines in the shared core (common/npps-core, migration_flow.rs): the ORDER (open the bundle before
// taking a claim, verify before confirming, confirm before the old phone clears) is decided there, once, for iOS, Android,
// Windows, macOS and the web. This file is the whole iOS driver: ask the core, perform the effect it names, hand back the
// outcome, until the core says done. Three effects it answers itself, the same on every platform: `deriveKey` (the core's own
// Argon2id over the secret this driver holds), and `readKeys` / `writeKeys` (the app's key-value store, whose key list the
// core holds in common/npps/migration-keys.json). Everything else is the host's: the route, and any item the app stores
// another way. The table of effects is in migration_flow.rs. The Windows counterpart is
// app/windows/NeurOne/Protocol/MigrationFlow.cs, the web's common/lib/migrationFlow.ts, Kotlin's MigrationService.
//
// A flow state is opaque, is never stored, and never holds a secret or a key.

/// Where this app performs the effects the driver does not answer itself.
protocol MigrationHost {
    /// Perform one effect and return its result object.
    func perform(_ effect: [String: Any]) async throws -> [String: Any]
}

/// The app's key-value store, as the flows need it: get, put and remove, and nothing else.
protocol MigrationKeyStore {
    func get(_ key: String) -> String?
    func put(_ key: String, _ value: String)
    func remove(_ key: String)
}

struct UserDefaultsKeyStore: MigrationKeyStore {
    let defaults: UserDefaults
    func get(_ key: String) -> String? { defaults.string(forKey: key) }
    func put(_ key: String, _ value: String) { defaults.set(value, forKey: key) }
    func remove(_ key: String) { defaults.removeObject(forKey: key) }
}

enum MigrationFlow {
    /// A restore of a large history is a few turns per item; this only stops a flow that never ends.
    static let maxTurns = 100_000

    /// Run `flow` ("backup", "inspect", "restore" or "clear") to its end and return its `done` object. `secret` is what a
    /// backup or restore derives its key from; `keys` is the key-value store the plain-key items live in. When `keys` is
    /// given, the items the core says are plain keys are declared to it as `keyItems`.
    static func run(
        _ flow: String,
        params: [String: Any],
        host: MigrationHost,
        secret: BundleSecret? = nil,
        keys: MigrationKeyStore? = nil
    ) async throws -> [String: Any] {
        var params = params
        if keys != nil, params["keyItems"] == nil {
            let carried = try NppsCore.migration(["op": "itemKeys"])["carried"] as? [String: Any] ?? [:]
            params["keyItems"] = carried.keys.sorted()
        }
        var answer = try NppsCore.migration(["op": "flow", "flow": flow, "params": params])
        for _ in 0..<maxTurns {
            if let done = answer["done"] as? [String: Any] { return done }
            guard let effect = answer["effect"] as? [String: Any], let state = answer["state"] else {
                throw NppsCore.Refusal(message: "the core named no effect")
            }
            let result = try await perform(effect, host: host, secret: secret, keys: keys)
            answer = try NppsCore.migration(["op": "flow", "flow": flow, "state": state, "result": result])
        }
        throw NppsCore.Refusal(message: "The \(flow) flow did not end in \(maxTurns) turns")
    }

    private static func perform(
        _ e: [String: Any],
        host: MigrationHost,
        secret: BundleSecret?,
        keys: MigrationKeyStore?
    ) async throws -> [String: Any] {
        switch e["type"] as? String {
        case "deriveKey":
            guard let secret, let kdf = e["kdf"] as? [String: Any] else {
                throw NppsCore.Refusal(message: "no secret to derive a key from")
            }
            return ["key": try BundleKeyDerivation.deriveKey(secret: secret, kdf: kdf).hex]
        case "readKeys":
            guard let keys else { break }
            var values: [String: Any] = [:]
            for k in e["keys"] as? [String] ?? [] { if let v = keys.get(k) { values[k] = v } }
            return ["values": values]
        case "writeKeys":
            guard let keys else { break }
            for k in e["remove"] as? [String] ?? [] { keys.remove(k) }
            for (k, v) in e["set"] as? [String: Any] ?? [:] { if let text = v as? String { keys.put(k, text) } }
            return ["ok": true]
        default:
            break
        }
        return try await host.perform(e)
    }
}
