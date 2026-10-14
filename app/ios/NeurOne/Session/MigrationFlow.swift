import Foundation

// The backup and device-replacement flows as iOS drives them (SWR-MG-02…10, issue #569 package 6e).
//
// The flows are state machines in the shared core (common/npps-core, migration_flow.rs): the ORDER (open the bundle before
// taking a claim, verify before confirming, confirm before the old phone clears) is decided there, once, for iOS, Android,
// Windows, macOS and the web. This file is the whole iOS driver: ask the core, perform the effect it names through the
// host, hand back the outcome, until the core says done. The host is where this app touches its own storage, its route,
// its Argon2 and its randomness; the table of effects is in migration_flow.rs. The Windows counterpart is
// app/windows/NeurOne/Protocol/MigrationFlow.cs, the web's common/lib/migrationFlow.ts, Kotlin's MigrationService.
//
// A flow state is opaque, is never stored, and never holds a secret or a key: the host keeps the secret, and a key is
// returned from deriveKey, used by the core for one answer and dropped.

/// Where this app performs the effects the core asks for.
protocol MigrationHost {
    /// Perform one effect and return its result object.
    func perform(_ effect: [String: Any]) async throws -> [String: Any]
}

enum MigrationFlow {
    /// A restore of a large history is a few turns per item; this only stops a flow that never ends.
    static let maxTurns = 100_000

    /// Run `flow` ("backup", "inspect", "restore" or "clear") to its end and return its `done` object.
    static func run(_ flow: String, params: [String: Any], host: MigrationHost) async throws -> [String: Any] {
        var answer = try NppsCore.migration(["op": "flow", "flow": flow, "params": params])
        for _ in 0..<maxTurns {
            if let done = answer["done"] as? [String: Any] { return done }
            guard let effect = answer["effect"] as? [String: Any], let state = answer["state"] else {
                throw NppsCore.Refusal(message: "the core named no effect")
            }
            let result = try await host.perform(effect)
            answer = try NppsCore.migration(["op": "flow", "flow": flow, "state": state, "result": result])
        }
        throw NppsCore.Refusal(message: "The \(flow) flow did not end in \(maxTurns) turns")
    }
}
