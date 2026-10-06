import Foundation

/// The one namespace every loaded `.npps` file shares (NP-NPPS-REF-001 §1.6).
///
/// There is exactly one: a zone or condition defined in any file under the
/// protocol directory tree is referenceable by name from any other file, and
/// definition order does not matter because the whole tree loads before
/// references are resolved.
struct NPNamespace: Equatable {
    var entries: [NPProtocolEntry] = []
    var zones: [String: NPZoneDefinition] = [:]
    var conditions: [String: NPConditionDefinition] = [:]

    /// Protocol and composite entries only — what a library lists.
    var runnableEntries: [NPProtocolEntry] {
        entries.filter { !$0.isDefinition && !$0.isLimits }
    }
}

/// A namespace plus the duplicate-definition errors found while building it.
struct NPNamespaceBuild {
    var namespace: NPNamespace
    var errors: [String] = []
}

/// Fold parsed entries from any number of files into one namespace. A zone or
/// condition name is defined exactly once across the whole tree
/// (NP-NPPS-REF-001 §1.6).
///
/// A name defined by two files is an ERROR, not a last-write-wins warning. The
/// tree is read recursively and nothing guarantees a stable traversal order
/// across platforms, file systems or bundle layouts, so "later" is not a
/// property this function has: last-write-wins bound the name to whichever
/// definition the traversal happened to reach last, which for a zone silently
/// changes which sockets a protocol doses.
///
/// So a collision leaves the name UNBOUND — neither definition wins — and is
/// reported in `errors`. Anything referencing it then fails
/// `validateNamespaceReferences` exactly as if the name had never been defined.
/// Matches the web and Android loaders.
/// Fold parsed entries from any number of files into one namespace. A zone or condition name is defined
/// exactly once across the whole tree (NP-NPPS-REF-001 §1.6).
///
/// A name defined by two files is an ERROR, not a last-write-wins warning: the tree is read recursively and
/// nothing guarantees a stable traversal order across platforms, so "later" is not a property this function
/// has. A collision leaves the name UNBOUND — neither definition wins — and is reported in
/// `NPNamespaceBuild.errors`. Anything referencing it then fails `validateNamespaceReferences` exactly as if
/// the name had never been defined.
///
/// The folding is the shared NPPS core's (common/npps-core, OI-NPPS-CORE-01): this builds the request from
/// the names it reads and maps the surviving names back to the caller's own definitions.
func buildNamespace(_ entries: [NPProtocolEntry]) -> NPNamespaceBuild {
    var zoneDefs: [NPZoneDefinition] = []
    var conditionDefs: [NPConditionDefinition] = []
    for entry in entries {
        switch entry {
        case .zone(let z): zoneDefs.append(z)
        case .condition(let c): conditionDefs.append(c)
        default: break
        }
    }
    let result = namespaceResult(entries: entries, zoneNames: zoneDefs.map { $0.name }, conditionNames: conditionDefs.map { $0.name })
    var zones: [String: NPZoneDefinition] = [:]
    for name in names(in: result["zones"]) {
        zones[name] = zoneDefs.first { $0.name == name }
    }
    var conditions: [String: NPConditionDefinition] = [:]
    for name in names(in: result["conditions"]) {
        conditions[name] = conditionDefs.first { $0.name == name }
    }
    return NPNamespaceBuild(
        namespace: NPNamespace(entries: entries, zones: zones, conditions: conditions),
        errors: result["errors"] as? [String] ?? []
    )
}

/// Cross-reference check: every protocol or composite `conditions` entry must resolve to a condition
/// definition, and every `pbm_transcranial` named zone reference must resolve to a zone definition.
/// Returns the unresolved references; empty means everything resolves.
func validateNamespaceReferences(_ ns: NPNamespace) -> [String] {
    let result = namespaceResult(entries: ns.entries, zoneNames: Array(ns.zones.keys), conditionNames: Array(ns.conditions.keys))
    return result["referenceErrors"] as? [String] ?? []
}

private func names(in value: Any?) -> [String] {
    (value as? [[String: Any]] ?? []).compactMap { $0["name"] as? String }
}

/// Runs the core's namespace over what it reads of a parsed file: each protocol's and composite's name and
/// `conditions`, the zones a `pbm_transcranial` block names, and the names of the zones and conditions defined.
private func namespaceResult(entries: [NPProtocolEntry], zoneNames: [String], conditionNames: [String]) -> [String: Any] {
    var coreEntries: [[String: Any]] = []
    for entry in entries {
        switch entry {
        case .single(let p):
            var modalities: [[String: Any]] = []
            for modality in p.modalities {
                guard case .pbmTranscranial(let params) = modality.params, case .named(let zones) = params.target else { continue }
                modalities.append(["type": "pbm_transcranial", "params": ["zoneRefs": zones]])
            }
            coreEntries.append([
                "kind": "single",
                "protocol": ["name": p.name, "conditions": p.conditions, "modalities": modalities] as [String: Any]
            ])
        case .composite(let c):
            coreEntries.append(["kind": "composite", "composite": ["name": c.name, "conditions": c.conditions] as [String: Any]])
        default:
            break
        }
    }
    let file: [String: Any] = [
        "entries": coreEntries,
        "zones": zoneNames.map { ["name": $0] },
        "conditions": conditionNames.map { ["name": $0] }
    ]
    guard let request = try? JSONSerialization.data(withJSONObject: ["files": [file]]),
          let data = try? NppsCore.namespace(requestJSON: request),
          let object = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
        return ["errors": ["the NPPS core could not build the namespace"]]
    }
    return object
}
