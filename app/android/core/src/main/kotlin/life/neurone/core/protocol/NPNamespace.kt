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

/** A namespace plus the collisions found while building it. */
data class NPNamespaceBuild(
    val namespace: NPNamespace,
    val warnings: List<String> = emptyList(),
)

/**
 * Fold parsed entries from any number of files into one namespace. A later file
 * redefining a zone or condition name replaces the earlier definition
 * (last-write-wins) and the collision is reported as a warning rather than an
 * error, matching the web loader.
 */
fun buildNamespace(entries: List<NPProtocolEntry>): NPNamespaceBuild {
    val zones = LinkedHashMap<String, NPZoneDefinition>()
    val conditions = LinkedHashMap<String, NPConditionDefinition>()
    val warnings = ArrayList<String>()

    for (e in entries) {
        when (e) {
            is NPProtocolEntry.Zone -> {
                if (zones.containsKey(e.zone.name)) {
                    warnings.add("Duplicate zone name '${e.zone.name}' — later definition wins")
                }
                zones[e.zone.name] = e.zone
            }
            is NPProtocolEntry.Condition -> {
                if (conditions.containsKey(e.condition.name)) {
                    warnings.add(
                        "Duplicate condition name '${e.condition.name}' — later definition wins"
                    )
                }
                conditions[e.condition.name] = e.condition
            }
            else -> Unit
        }
    }
    return NPNamespaceBuild(NPNamespace(entries, zones, conditions), warnings)
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
