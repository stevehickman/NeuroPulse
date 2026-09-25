import Foundation

// The replacement reset for CONSUMABLE_STATUS (OI-ACC-08, GitHub #381).
// Android port: app/android/core/.../consumable/ConsumableReset.kt.
//
// The hub owns the counts, and each is "sessions since this part was replaced". Mark replaced
// zeroes the app's copy at once, but the next CONSUMABLE_STATUS notification overwrites it with
// the hub's absolute value, so the hub must be told: a 1-byte WRITE to 0x0007 naming the kind
// (its ConsumableKind raw value, 0–3). The hub zeroes that count and notifies the zero.
//
// A replacement marked while no hub is connected is queued here, persisted, and written at the
// next connect. It leaves the queue when the write is issued. Known limit: a Mode 3 session run
// between the replacement and that connect is zeroed with the old part's count — an undercount
// of the new part by the offline sessions.

enum ConsumableResetWire {
    /// The WRITE value for `kind`, or nil for a kind the hub does not count.
    static func encode(_ kind: Int) -> Data? {
        guard (0..<ConsumableKind.allCases.count).contains(kind) else { return nil }
        return Data([UInt8(kind)])
    }
}

struct ConsumableResetQueue {
    static let key = "np.consumable.pending-resets"

    let defaults: UserDefaults

    /// Kinds replaced and not yet written to the hub, in kind order, no duplicates.
    var pending: [Int] {
        let raw = defaults.array(forKey: Self.key) as? [Int] ?? []
        return Array(Set(raw.filter { ConsumableResetWire.encode($0) != nil })).sorted()
    }

    func add(_ kind: Int) {
        guard ConsumableResetWire.encode(kind) != nil else { return }
        save(Array(Set(pending + [kind])).sorted())
    }

    /// Hands every pending kind to `write` and forgets it.
    func drain(_ write: (Data) -> Void) {
        let kinds = pending
        guard !kinds.isEmpty else { return }
        save([])
        kinds.compactMap(ConsumableResetWire.encode).forEach(write)
    }

    private func save(_ kinds: [Int]) {
        if kinds.isEmpty {
            defaults.removeObject(forKey: Self.key)
        } else {
            defaults.set(kinds, forKey: Self.key)
        }
    }
}
