import Foundation

// Stage 2 of vetting a protocol (OI-HWFIT-01): can the compiled descriptor run on the modules fitted now?
// The rule is the shared core's (`fitCheck`, common/npps-core/src/fit.rs); this marshals the hub's tile types in and the
// shortfalls out and decides nothing. The answer is advice: the hub's placement check stays the authority.

/// An element type a socket needs and does not have, and the part numbers of the module designs that carry it.
struct MissingElement: Equatable {
    let element: String
    let candidateParts: [String]
}

/// Sockets the protocol drives that fall short in the same way, so a person reads one line and not one per socket.
struct ShortfallGroup: Equatable {
    let sockets: [Int]
    let missing: [MissingElement]
}

enum ModuleFit {
    /// - Parameters:
    ///   - descriptor: a compiled descriptor, signed or not
    ///   - tileTypes: socket id to the tile type the hub reported there (`ZoneModuleType.rawValue`), for the sockets that hold an
    ///     identified module without a fault. A socket not listed is empty.
    /// - Returns: the sockets that fall short, grouped by what they lack (the core groups them); empty when the protocol fits.
    static func check(descriptor: Data, tileTypes: [Int: Int]) throws -> [ShortfallGroup] {
        let answer = try NppsCore.library([
            "op": "fitCheck",
            "descriptorHex": hexString(descriptor),
            "inventory": Dictionary(uniqueKeysWithValues: tileTypes.map { (String($0.key), $0.value) }),
        ])
        let groups = answer["groups"] as? [[String: Any]] ?? []
        return groups.map { g in
            ShortfallGroup(
                sockets: (g["sockets"] as? [Int]) ?? [],
                missing: (g["missing"] as? [[String: Any]] ?? []).map {
                    MissingElement(element: $0["element"] as? String ?? "", candidateParts: $0["candidateParts"] as? [String] ?? [])
                }
            )
        }
    }

    /// The words for one group, from the locale files: `Sockets: 1, 2. Needs: 1064nm LED. Modules that have it: ZM-PBM-TRI.`
    static func line(_ group: ShortfallGroup) -> String {
        let needs = group.missing.map { String(localized: String.LocalizationValue("ELEMENT_\($0.element.uppercased())")) }
        let parts = Array(Set(group.missing.flatMap(\.candidateParts))).sorted()
        return String(format: String(localized: "UPLOAD_NOT_FITTED_LINE"),
                      group.sockets.map(String.init).joined(separator: ", "),
                      needs.joined(separator: ", "), parts.joined(separator: ", "))
    }
}
