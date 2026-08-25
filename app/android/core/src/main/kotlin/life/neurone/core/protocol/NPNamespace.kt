package life.neurone.core.protocol

/**
 * The one namespace every loaded `.npps` file shares (NP-NPPS-REF-001 §1.6).
 *
 * There is exactly one: a zone or condition defined in any file under the
 * protocol directory tree is referenceable by name from any other file, and
 * definition order does not matter because the whole tree loads before
 * references are resolved.
 */
data class NPNamespace(
    val entries: List<NPProtocolEntry> = emptyList(),
    val zones: Map<String, NPZoneDefinition> = emptyMap(),
    val conditions: Map<String, NPConditionDefinition> = emptyMap(),
) {
    /** Protocol and composite entries only — what a library lists. */
    val runnableEntries: List<NPProtocolEntry>
        get() = entries.filter { it is NPProtocolEntry.Single || it is NPProtocolEntry.Composite }
}

/** A namespace plus the duplicate-definition errors found while building it. */
data class NPNamespaceBuild(
    val namespace: NPNamespace,
    val errors: List<String> = emptyList(),
)

/**
 * Fold parsed entries from any number of files into one namespace. A zone or
 * condition name is defined exactly once across the whole tree
 * (NP-NPPS-REF-001 §1.6).
 *
 * A name defined by two files is an ERROR, not a last-write-wins warning. The
 * tree is read recursively and nothing guarantees a stable traversal order
 * across platforms, file systems or bundle layouts, so "later" is not a property
 * this function has: last-write-wins bound the name to whichever definition the
 * traversal happened to reach last, which for a zone silently changes which
 * sockets a protocol doses.
 *
 * So a collision leaves the name UNBOUND — neither definition wins — and is
 * reported in [NPNamespaceBuild.errors]. Anything referencing it then fails
 * [validateNamespaceReferences] exactly as if the name had never been defined.
 * Matches the web and iOS loaders.
 */
fun buildNamespace(entries: List<NPProtocolEntry>): NPNamespaceBuild {
    val zones = LinkedHashMap<String, NPZoneDefinition>()
    val conditions = LinkedHashMap<String, NPConditionDefinition>()
    val errors = ArrayList<String>()
    // Names seen at least twice: kept out of the namespace, so a third
    // definition cannot re-bind a name already known to collide.
    val collidedZones = HashSet<String>()
    val collidedConditions = HashSet<String>()

    for (e in entries) {
        when (e) {
            is NPProtocolEntry.Zone -> {
                val name = e.zone.name
                if (name in collidedZones) continue
                if (zones.containsKey(name)) {
                    errors.add(
                        "Duplicate zone name '$name' — defined in more than one file; zone names " +
                            "must be unique across the protocol directory. " +
                            "The name is left undefined."
                    )
                    zones.remove(name)
                    collidedZones.add(name)
                    continue
                }
                zones[name] = e.zone
            }
            is NPProtocolEntry.Condition -> {
                val name = e.condition.name
                if (name in collidedConditions) continue
                if (conditions.containsKey(name)) {
                    errors.add(
                        "Duplicate condition name '$name' — defined in more than one file; " +
                            "condition names must be unique across the protocol directory. " +
                            "The name is left undefined."
                    )
                    conditions.remove(name)
                    collidedConditions.add(name)
                    continue
                }
                conditions[name] = e.condition
            }
            else -> Unit
        }
    }
    return NPNamespaceBuild(NPNamespace(entries, zones, conditions), errors)
}

/**
 * Cross-reference check: every protocol or composite `conditions` entry must
 * resolve to a condition definition, and every `pbm_transcranial` named zone
 * reference must resolve to a zone definition. Returns the unresolved
 * references; empty means everything resolves.
 */
fun validateNamespaceReferences(ns: NPNamespace): List<String> {
    val errors = ArrayList<String>()

    fun checkConditions(owner: String, names: List<String>) {
        for (c in names) {
            if (!ns.conditions.containsKey(c)) {
                errors.add("Protocol '$owner' references undefined condition '$c'")
            }
        }
    }

    for (entry in ns.entries) {
        when (entry) {
            is NPProtocolEntry.Single -> {
                checkConditions(entry.protocol.name, entry.protocol.conditions)
                for (m in entry.protocol.modalities) {
                    val p = (m.params as? NPModalityParams.PbmTranscranial)?.params ?: continue
                    val target = p.target as? NPPBMTarget.Named ?: continue
                    for (z in target.zoneNames) {
                        if (!ns.zones.containsKey(z)) {
                            errors.add(
                                "Protocol '${entry.protocol.name}' references undefined zone '$z'"
                            )
                        }
                    }
                }
            }
            is NPProtocolEntry.Composite -> checkConditions(entry.composite.name, entry.composite.conditions)
            else -> Unit
        }
    }
    return errors
}
