import Foundation

// The backup and device-replacement flows as iOS drives them (SWR-MG-02…10, issue #569 package 6e, 6f).
//
// The flows are state machines in the shared core (common/npps-core, migration_flow.rs): the ORDER (open the bundle before
// taking a claim, verify before confirming, confirm before the old phone clears) is decided there, once, for iOS, Android,
// Windows, macOS and the web. This file is the whole iOS driver: ask the core, perform the effect it names, hand back the
// outcome, until the core says done. It answers these effects itself, the same on every platform: `random` (the platform's
// CSPRNG), `deriveKey` (the core's own Argon2id over the secret this driver holds), `readKeys` / `writeKeys` (the app's
// key-value store, whose key list the core holds in common/npps/migration-keys.json), the object, file and link calls.
// Everything else is the host's: the route, and any item the app stores
// another way. The table of effects is in migration_flow.rs. The Windows counterpart is
// app/windows/NeurOne/Protocol/MigrationFlow.cs, the web's common/lib/migrationFlow.ts and Kotlin's MigrationFlow.kt, under the
// same four names: MigrationFlow, MigrationHost, KeyValueStore and ObjectStore.
//
// A flow state is opaque, is never stored, and never holds a secret or a key.

/// Where this app performs the effects the driver does not answer itself.
protocol MigrationHost {
    /// Perform one effect and return its result object.
    func perform(_ effect: [String: Any]) async throws -> [String: Any]
}

/// The app's key-value store, as the flows need it: get, put and remove, and nothing else. The same role and the same names on
/// every platform (Kotlin `KeyValueStore`, C# `IKeyValueStore`, TypeScript `KeyValueStore`).
protocol KeyValueStore {
    func getString(_ key: String) -> String?
    func putString(_ key: String, _ value: String)
    func remove(_ key: String)
}

struct UserDefaultsKeyValueStore: KeyValueStore {
    let defaults: UserDefaults
    func getString(_ key: String) -> String? { defaults.string(forKey: key) }
    func putString(_ key: String, _ value: String) { defaults.set(value, forKey: key) }
    func remove(_ key: String) { defaults.removeObject(forKey: key) }
}

/// The calls a storage provider is asked for (SWR-MG-11), on named objects, with the bytes as lower-case hex. This is the whole
/// of what iOS writes for a cloud account, the local computer or a shared folder; the core holds the rest (object_route.rs).
/// A call that cannot be done (no network, no permission, a refused write) throws, and the flow ends with the route unavailable.
protocol ObjectStore {
    /// The object's bytes, or nil when there is none.
    func get(_ name: String) async throws -> String?
    /// Write the object, replacing any earlier one.
    func put(_ name: String, _ data: String) async throws
    /// Write the object if and only if none exists, arbitrated by the store itself (SWR-MG-07). True when this call wrote it.
    func createIfAbsent(_ name: String, _ data: String) async throws -> Bool
    /// Remove the object. Not an error when there is none.
    func delete(_ name: String) async throws
    func list(_ prefix: String) async throws -> [String]
    /// Free space in bytes, or nil when the provider cannot say.
    func freeSpace() async throws -> Int64?
}

/// A file the app holds, as `FileStore.list` reports it.
struct StoredFile {
    let path: String
    let size: Int64
}

/// The calls the old phone's clear asks of the app's files (SWR-MG-06), and nothing else: the core decides how a file is
/// overwritten (every byte, in chunks, then a sync, then the delete). `list` reports the UHDR-class files still there. A call
/// that cannot be done throws, and the clear stops at that step and resumes at the next launch. The same role and names on
/// every platform (Kotlin `FileStore`, C# `IFileStore`, TypeScript `FileStore`).
protocol FileStore {
    func list() async throws -> [StoredFile]
    /// Overwrite `bytes.count` bytes of `path` starting at `offset`, in place.
    func write(_ path: String, offset: Int64, bytes: Data) async throws
    /// The writes so far have reached storage.
    func sync(_ path: String) async throws
    /// Not an error when there is no such file.
    func delete(_ path: String) async throws
}

/// A pipe to the other phone (SWR-MG-10): bytes in, bytes out, as lower-case hex like the object store's. Bluetooth, a local
/// socket or a cable is each one implementation; everything above the bytes (framing, which calls a peer may make, the claim)
/// is in the core (link.rs). The same role and names on every platform (Kotlin `Link`, C# `ILink`, TypeScript `Link`).
/// SwiftUI has a view of the same name, so a view that links to a URL writes `SwiftUI.Link`.
protocol Link {
    /// Send the bytes; throws if the link broke.
    func send(_ data: String) async throws
    /// The next bytes, or nil once the other end has closed; throws if the link broke.
    func receive() async throws -> String?
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
        keys: KeyValueStore? = nil,
        store: ObjectStore? = nil,
        files: FileStore? = nil,
        link: Link? = nil,
        random: BundleRandomSource = SystemBundleRandomSource()
    ) async throws -> [String: Any] {
        var params = params
        if store != nil || link != nil, params["objectStore"] == nil { params["objectStore"] = true }
        if link != nil, flow != "serve", params["link"] == nil { params["link"] = true }
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
            let result = try await perform(
                effect, host: host, secret: secret, keys: keys, store: store, files: files, link: link, random: random)
            answer = try NppsCore.migration(["op": "flow", "flow": flow, "state": state, "result": result])
        }
        throw NppsCore.Refusal(message: "The \(flow) flow did not end in \(maxTurns) turns")
    }

    private static func perform(
        _ e: [String: Any],
        host: MigrationHost,
        secret: BundleSecret?,
        keys: KeyValueStore?,
        store: ObjectStore?,
        files: FileStore?,
        link: Link?,
        random: BundleRandomSource
    ) async throws -> [String: Any] {
        switch e["type"] as? String {
        case "random":
            let n = (e["bytes"] as? NSNumber)?.intValue ?? 0
            let bytes = try random.nextBytes(n)
            guard bytes.count == n else { throw BundleKeyError.noRandomness }
            return ["bytes": bytes.map { String(format: "%02x", $0) }.joined()]
        case "deriveKey":
            guard let secret, let kdf = e["kdf"] as? [String: Any] else {
                throw NppsCore.Refusal(message: "no secret to derive a key from")
            }
            return ["key": try BundleKeyDerivation.deriveKey(secret: secret, kdf: kdf).hex]
        case "readKeys":
            guard let keys else { break }
            var values: [String: Any] = [:]
            for k in e["keys"] as? [String] ?? [] { if let v = keys.getString(k) { values[k] = v } }
            return ["values": values]
        case "writeKeys":
            guard let keys else { break }
            for k in e["remove"] as? [String] ?? [] { keys.remove(k) }
            for (k, v) in e["set"] as? [String: Any] ?? [:] { if let text = v as? String { keys.putString(k, text) } }
            return ["ok": true]
        case "objectGet", "objectPut", "objectCreate", "objectDelete", "objectList", "objectSpace":
            guard let store else { break }
            return await performObject(e, store)
        case "linkSend", "linkRecv":
            guard let link else { break }
            return await performLink(e, link)
        case "listFiles", "writeFile", "syncFile", "deleteFile":
            guard let files else { break }
            return await performFile(e, files)
        default:
            break
        }
        return try await host.perform(e)
    }

    /// One link call. A link that breaks is reported to the core as failed; the core decides what that means (SWR-MG-11).
    private static func performLink(_ e: [String: Any], _ link: Link) async -> [String: Any] {
        do {
            if e["type"] as? String == "linkSend" {
                try await link.send(e["frame"] as? String ?? "")
                return ["ok": true]
            }
            let chunk = try await link.receive()
            return ["chunk": chunk as Any? ?? NSNull()]
        } catch {
            return ["failed": true]
        }
    }

    private struct NoHost: MigrationHost {
        func perform(_ effect: [String: Any]) async throws -> [String: Any] {
            throw NppsCore.Refusal(message: "a serving phone is asked for no host effect")
        }
    }

    /// SWR-MG-10: serve this phone's migration objects from `store` to the phone on the other end of `link`, until it closes.
    /// `authenticated` is the app's assertion that the link was paired (the pairing is not designed yet); the core refuses to
    /// serve without it. Returns `{ok, reason}`.
    static func serve(link: Link, store: ObjectStore, authenticated: Bool) async throws -> [String: Any] {
        try await run("serve", params: ["authenticated": authenticated], host: NoHost(), store: store, link: link)
    }

    /// One file call of the clear. A call that throws is reported to the core as failed, and the clear stops at that step (SWR-MG-06).
    private static func performFile(_ e: [String: Any], _ files: FileStore) async -> [String: Any] {
        let path = e["path"] as? String ?? ""
        do {
            switch e["type"] as? String {
            case "listFiles":
                let list = try await files.list()
                return ["files": list.map { ["path": $0.path, "size": $0.size] as [String: Any] }]
            case "writeFile":
                let offset = (e["offset"] as? NSNumber)?.int64Value ?? 0
                try await files.write(path, offset: offset, bytes: hexData(e["data"] as? String ?? ""))
                return ["ok": true]
            case "syncFile":
                try await files.sync(path)
                return ["ok": true]
            default:
                try await files.delete(path)
                return ["ok": true]
            }
        } catch {
            return ["failed": true]
        }
    }

    private static func hexData(_ hex: String) -> Data {
        var bytes = [UInt8]()
        var index = hex.startIndex
        while index < hex.endIndex {
            let next = hex.index(index, offsetBy: 2)
            bytes.append(UInt8(hex[index..<next], radix: 16) ?? 0)
            index = next
        }
        return Data(bytes)
    }

    /// One object call. A call that throws is reported to the core as failed; the core decides what that means (SWR-MG-11).
    private static func performObject(_ e: [String: Any], _ store: ObjectStore) async -> [String: Any] {
        let name = e["name"] as? String ?? ""
        do {
            switch e["type"] as? String {
            case "objectGet":
                let data = try await store.get(name)
                return ["data": data as Any? ?? NSNull()]
            case "objectPut":
                try await store.put(name, e["data"] as? String ?? "")
                return ["ok": true]
            case "objectCreate":
                let created = try await store.createIfAbsent(name, e["data"] as? String ?? "")
                return ["created": created]
            case "objectDelete":
                try await store.delete(name)
                return ["ok": true]
            case "objectList":
                let names = try await store.list(e["prefix"] as? String ?? "")
                return ["names": names]
            default:
                let bytes = try await store.freeSpace()
                return ["bytes": bytes as Any? ?? NSNull()]
            }
        } catch {
            return ["failed": true]
        }
    }

    /// The locale key that words each reason, rejected secret, storage-check result and item (SWR-MG-03), from the core, so no
    /// platform spells one its own way. Groups: `reasons`, `secret`, `probe`, `items`; each maps a code to a key.
    static func messageKeys() throws -> [String: Any] { try NppsCore.migration(["op": "messageKeys"]) }

    /// The object names the core uses, for a host that releases a claim on the person's explicit act (SWR-MG-07).
    static func objectNames() throws -> [String: Any] { try NppsCore.migration(["op": "objectNames"]) }

    /// SWR-MG-12: check an account on throwaway objects that hold no user data.
    static func probe(
        store: ObjectStore, bundleBytes: Int, host: MigrationHost, random: BundleRandomSource = SystemBundleRandomSource()
    ) async throws -> [String: Any] {
        try await run("probe", params: ["bundleBytes": bundleBytes], host: host, store: store, random: random)
    }
}
