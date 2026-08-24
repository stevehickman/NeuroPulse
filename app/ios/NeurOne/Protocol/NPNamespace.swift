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
func buildNamespace(_ entries: [NPProtocolEntry]) -> NPNamespaceBuild {
    var zones: [String: NPZoneDefinition] = [:]
    var conditions: [String: NPConditionDefinition] = [:]
    var errors: [String] = []
    // Names seen at least twice: kept out of the namespace, so a third
    // definition cannot re-bind a name already known to collide.
    var collidedZones: Set<String> = []
    var collidedConditions: Set<String> = []

    for entry in entries {
        switch entry {
        case .zone(let z):
            if collidedZones.contains(z.name) { continue }
            if zones[z.name] != nil {
                errors.append(
                    "Duplicate zone name '\(z.name)' — defined in more than one file; zone names "
                    + "must be unique across the protocol directory. The name is left undefined."
                )
                zones.removeValue(forKey: z.name)
                collidedZones.insert(z.name)
                continue
            }
            zones[z.name] = z
        case .condition(let c):
            if collidedConditions.contains(c.name) { continue }
            if conditions[c.name] != nil {
                errors.append(
                    "Duplicate condition name '\(c.name)' — defined in more than one file; "
                    + "condition names must be unique across the protocol directory. "
                    + "The name is left undefined."
                )
                conditions.removeValue(forKey: c.name)
                collidedConditions.insert(c.name)
                continue
            }
            conditions[c.name] = c
        default:
            break
        }
    }
    return NPNamespaceBuild(
        namespace: NPNamespace(entries: entries, zones: zones, conditions: conditions),
        errors: errors
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
