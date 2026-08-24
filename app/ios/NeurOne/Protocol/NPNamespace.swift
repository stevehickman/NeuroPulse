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

/// A namespace plus the collisions found while building it.
struct NPNamespaceBuild {
    var namespace: NPNamespace
    var warnings: [String] = []
}

/// Fold parsed entries from any number of files into one namespace. A later
/// file redefining a zone or condition name replaces the earlier definition
/// (last-write-wins) and the collision is reported as a warning rather than an
/// error, matching the web loader.
func buildNamespace(_ entries: [NPProtocolEntry]) -> NPNamespaceBuild {
    var zones: [String: NPZoneDefinition] = [:]
    var conditions: [String: NPConditionDefinition] = [:]
    var warnings: [String] = []

    for entry in entries {
        switch entry {
        case .zone(let z):
            if zones[z.name] != nil {
                warnings.append("Duplicate zone name '\(z.name)' — later definition wins")
            }
            zones[z.name] = z
        case .condition(let c):
            if conditions[c.name] != nil {
                warnings.append("Duplicate condition name '\(c.name)' — later definition wins")
            }
            conditions[c.name] = c
        default:
            break
        }
    }
    return NPNamespaceBuild(
        namespace: NPNamespace(entries: entries, zones: zones, conditions: conditions),
        warnings: warnings
    )
}

/// Cross-reference check: every protocol or composite `conditions` entry must
/// resolve to a condition definition, and every `pbm_transcranial` named zone
/// reference must resolve to a zone definition. Returns the unresolved
/// references; empty means everything resolves.
func validateNamespaceReferences(_ ns: NPNamespace) -> [String] {
    var errors: [String] = []

    func checkConditions(owner: String, _ names: [String]) {
        for name in names where ns.conditions[name] == nil {
            errors.append("Protocol '\(owner)' references undefined condition '\(name)'")
        }
    }

    for entry in ns.entries {
        switch entry {
        case .single(let p):
            checkConditions(owner: p.name, p.conditions)
            for modality in p.modalities {
                guard case .pbmTranscranial(let params) = modality.params,
                      case .named(let names) = params.target else { continue }
                for name in names where ns.zones[name] == nil {
                    errors.append("Protocol '\(p.name)' references undefined zone '\(name)'")
                }
            }
        case .composite(let c):
            checkConditions(owner: c.name, c.conditions)
        default:
            break
        }
    }
    return errors
}
