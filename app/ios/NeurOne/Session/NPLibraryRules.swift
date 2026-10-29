import Foundation

// The questions the protocol library puts to the shared core (common/npps-core/src/library.rs) and the summaries it
// gives it. Nothing is decided here. It lives beside NppsCore, outside the covered UI tree: every string is a JSON key
// or an op name, not rendered text.

// MARK: - The core's questions

/// One imported entry as the merge sees it: its id, its content hash, and whether it arrived flagged used.
struct NPImportedEntry {
    var id: String
    var hash: String?
    var used: Bool
}

/// How a protocol's layout overlaps the helmet's: sockets in both, only in the protocol's, only in the helmet's.
struct NPLayoutOverlap {
    var shared: Int
    var onlyProtocol: Int
    var onlyHelmet: Int
}

enum NPLibraryRules {

    private static func ask(_ op: String, _ fields: [String: Any] = [:]) -> [String: Any] {
        var request = fields
        request["op"] = op
        // A refusal here is a malformed request, a defect and not a user condition: fail closed as "not allowed".
        return (try? NppsCore.library(request)) ?? ["allowed": false]
    }

    /// The zone namespace as the core takes it: every zone's socket list per hardware version. One layout exists, version 1.
    static func zonesByVersion() -> [String: Any] {
        var out: [String: Any] = [:]
        for (name, zone) in NPZoneRegistry.zones { out[name] = ["1": zone.sockets] }
        return out
    }

    /// Hardware versions a protocol can be sent to (SWR-HV-04); a composite is a version-1 object until composites carry
    /// zones of their own.
    static func supportedVersions(_ entry: NPProtocolEntry) -> [Int] {
        switch entry {
        case .single(let p):
            guard let def = try? JSONSerialization.jsonObject(with: p.nppsCoreJSON()) else { return [] }
            let answer = ask("supportedVersions", ["def": def, "zonesByVersion": zonesByVersion()])
            return (answer["versions"] as? [Any])?.compactMap { ($0 as? NSNumber)?.intValue } ?? []
        case .composite:
            return [1]
        default:
            return []
        }
    }

    /// The summaries the core decides over. `withVersions` costs a core call per protocol, so only the questions that need
    /// it ask.
    static func summaries(_ entries: [NPProtocolEntry], used: Set<String>, withVersions: Bool) -> [[String: Any]] {
        entries.filter { $0.isRunnable }.map { e in
            var s: [String: Any] = [
                "id": e.id.uuidString,
                "used": used.contains(e.familyKey),
                "version": e.versionText,
                "scope": "owner",
            ]
            if let family = e.familyId { s["familyId"] = family }
            if withVersions { s["hardwareVersions"] = supportedVersions(e) }
            return s
        }
    }

    static func saveCheck(_ entries: [[String: Any]], id: String) -> [String: Any] {
        ask("saveCheck", ["entries": entries, "id": id])
    }

    static func deleteCheck(_ entries: [[String: Any]], id: String) -> [String: Any] {
        ask("deleteCheck", ["entries": entries, "id": id])
    }

    static func cloneFamily(_ entries: [[String: Any]], id: String, newFamilyId: String, newIds: [String]) -> [String: Any] {
        ask("cloneFamily", ["entries": entries, "id": id, "newFamilyId": newFamilyId, "newIds": newIds])
    }

    static func addVariantCheck(_ entries: [[String: Any]], familyId: String, hardwareVersion: Int) -> [String: Any] {
        ask("addVariantCheck", ["entries": entries, "familyId": familyId, "hardwareVersion": hardwareVersion])
    }

    static func variantFor(_ entries: [[String: Any]], familyId: String, helmetVersion: Int) -> [String] {
        let answer = ask("variantFor", ["entries": entries, "familyId": familyId, "helmetVersion": helmetVersion])
        return (answer["variants"] as? [String]) ?? []
    }

    static func offered(_ entries: [[String: Any]], archivedFamilies: Set<String>) -> Set<String> {
        let answer = ask("offered", ["entries": entries, "archivedFamilies": Array(archivedFamilies)])
        return Set((answer["offered"] as? [String]) ?? [])
    }

    static func uploadGate(helmetVersion: Int, protocolVersions: [Int], contentHash: String, sourceHash: String?) -> [String: Any] {
        ask("uploadGate", [
            "helmetVersion": helmetVersion,
            "protocolVersions": protocolVersions,
            "contentHash": contentHash,
            "sourceHash": sourceHash.map { $0 as Any } ?? NSNull(),
        ])
    }

    static func uploadResult(sentHash: String, helmetStatus: String?, helmetHash: String?,
                             helmetRefusal: String?, refusedBeforeSend: Bool) -> [String: Any] {
        var request: [String: Any] = ["sentHash": sentHash, "refusedBeforeSend": refusedBeforeSend]
        if let helmetStatus { request["helmetStatus"] = helmetStatus }
        if let helmetHash { request["helmetHash"] = helmetHash }
        if let helmetRefusal { request["helmetRefusal"] = helmetRefusal }
        return ask("uploadResult", request)
    }

    static func mergeImport(usedHashes: [String], incoming: [NPImportedEntry]) -> [String: Bool] {
        let rows: [[String: Any]] = incoming.map { row in
            var r: [String: Any] = ["id": row.id, "used": row.used]
            if let hash = row.hash { r["hash"] = hash }
            return r
        }
        let answer = ask("mergeImport", ["usedHashes": usedHashes, "incoming": rows])
        var out: [String: Bool] = [:]
        for row in (answer["entries"] as? [[String: Any]]) ?? [] {
            if let id = row["id"] as? String, let used = row["used"] as? Bool { out[id] = used }
        }
        return out
    }

    /// The record a used family is stored as: the family id and nothing else (SWR-WO-02).
    static func usedRecord(familyId: String) -> [String: Any] {
        (ask("usedRecord", ["familyId": familyId])["record"] as? [String: Any]) ?? [:]
    }

    static func isValidUsedRecord(_ record: Any) -> Bool {
        (ask("usedRecord", ["check": record])["valid"] as? Bool) ?? false
    }

    /// A protocol's identity (SWR-HV-01) for the one layout there is; nil for a kind that has none or a protocol that
    /// cannot be hashed yet (for example a clinician-selected target the operator has not chosen).
    static func contentHash(_ entry: NPProtocolEntry) -> String? {
        guard case .single(let p) = entry,
              let def = try? JSONSerialization.jsonObject(with: p.nppsCoreJSON()) else { return nil }
        return try? NppsCore.contentHash(request: [
            "def": def,
            "hardwareVersion": 1,
            "zonesByVersion": zonesByVersion(),
            "clinicianSockets": NSNull(),
        ])
    }

    /// How the first of the protocol's layouts overlaps the helmet's, from an `uploadGate` refusal: sockets in both, only
    /// in the protocol's, only in the helmet's. Nil while the helmet's layout is not in the table.
    static func overlap(_ gate: [String: Any]) -> NPLayoutOverlap? {
        guard let diff = ((gate["layoutDiffs"] as? [[String: Any]])?.first)?["diff"] as? [String: Any] else { return nil }
        return NPLayoutOverlap(shared: (diff["both"] as? [Any])?.count ?? 0,
                               onlyProtocol: (diff["onlyA"] as? [Any])?.count ?? 0,
                               onlyHelmet: (diff["onlyB"] as? [Any])?.count ?? 0)
    }
}

// What the helmet's upload report means to the library's upload rule (`SWR-UP-02`). The words here are the rule's codes
// ("accepted", "tier"), not text for a person, so they live beside the other core calls and not in the covered UI tree.
extension UploadCheck {
    /// `NP_HUB_ERR_TIER_F4` (19) and `NP_HUB_ERR_TIER_UNVERIFIED` (20): the helmet refused a protocol for its tier.
    private static let tierRefusalCodes: Set<Int> = [19, 20]

    /// `NP_HUB_ERR_HW_VERSION_MISMATCH` (27) and `NP_HUB_ERR_HW_VERSION_UNVERIFIED` (28): the helmet refused a protocol
    /// compiled for another hardware version, or has not yet heard its own version from the safety MCU (SWR-HV-08).
    /// Neither is a failed upload and neither marks the protocol used, as for a tier refusal. An unmodified clone is not
    /// made by the hub, so no code names it.
    private static let hardwareVersionRefusalCodes: Set<Int> = [27, 28]

    /// The `helmetRefusal` the library's upload rule takes for this refusal, or nil when it is none of the three it names.
    var refusalKind: String? {
        guard status == .refused else { return nil }
        if Self.tierRefusalCodes.contains(reason) { return "tier" }
        if Self.hardwareVersionRefusalCodes.contains(reason) { return "hardware_version" }
        return nil
    }
}

extension UploadVerdict {
    /// The verdict from the last report the helmet gave (nil if it never gave one). A final report (accepted or refused)
    /// is passed on with its hash; the library compares that hash with the one the app sent, so a final report for other
    /// bytes is a failed upload and not a guess here. No report, an empty one or one still pending is no answer, which the
    /// library treats as unknown (`SWR-UP-03`).
    static func of(_ check: UploadCheck?) -> UploadVerdict {
        guard let check, check.status == .accepted || check.status == .refused else {
            return UploadVerdict(helmetStatus: nil, helmetHash: nil, helmetRefusal: nil)
        }
        return UploadVerdict(helmetStatus: check.status == .accepted ? "accepted" : "refused",
                             helmetHash: check.receivedHash, helmetRefusal: check.refusalKind,
                             helmetReason: check.status == .refused ? check.reason : nil)
    }
}
