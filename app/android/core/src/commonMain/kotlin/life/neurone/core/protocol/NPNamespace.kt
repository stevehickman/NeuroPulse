package life.neurone.core.protocol

import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import kotlinx.serialization.json.putJsonArray
import kotlinx.serialization.json.putJsonObject
import life.neurone.core.npps.NppsCore

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
 * A name defined by two files is an ERROR, not a last-write-wins warning: the tree is read recursively
 * and nothing guarantees a stable traversal order across platforms, so "later" is not a property this
 * function has. A collision leaves the name UNBOUND — neither definition wins — and is reported in
 * [NPNamespaceBuild.errors]. Anything referencing it then fails [validateNamespaceReferences] exactly as
 * if the name had never been defined.
 *
 * The folding is the shared NPPS core's (common/npps-core, OI-NPPS-CORE-01): this builds the request from
 * the names it reads and maps the surviving names back to the caller's own definitions.
 */
fun buildNamespace(entries: List<NPProtocolEntry>): NPNamespaceBuild {
    val zoneDefs = entries.filterIsInstance<NPProtocolEntry.Zone>().map { it.zone }
    val conditionDefs = entries.filterIsInstance<NPProtocolEntry.Condition>().map { it.condition }
    val result = NppsCore.namespace(listOf(namespaceRequestFile(entries, zoneDefs.map { it.name }, conditionDefs.map { it.name })))
    val zones = LinkedHashMap<String, NPZoneDefinition>()
    for (z in result["zones"]!!.jsonArray) {
        val name = z.jsonObject["name"]!!.jsonPrimitive.content
        zones[name] = zoneDefs.first { it.name == name }
    }
    val conditions = LinkedHashMap<String, NPConditionDefinition>()
    for (c in result["conditions"]!!.jsonArray) {
        val name = c.jsonObject["name"]!!.jsonPrimitive.content
        conditions[name] = conditionDefs.first { it.name == name }
    }
    val errors = result["errors"]!!.jsonArray.map { it.jsonPrimitive.content }
    return NPNamespaceBuild(NPNamespace(entries, zones, conditions), errors)
}

/**
 * Cross-reference check: every protocol or composite `conditions` entry must
 * resolve to a condition definition, and every `pbm_transcranial` named zone
 * reference must resolve to a zone definition. Returns the unresolved
 * references; empty means everything resolves.
 */
fun validateNamespaceReferences(ns: NPNamespace): List<String> {
    val file = namespaceRequestFile(ns.entries, ns.zones.keys.toList(), ns.conditions.keys.toList())
    return NppsCore.namespace(listOf(file))["referenceErrors"]!!.jsonArray.map { it.jsonPrimitive.content }
}

/**
 * What the core's namespace reads of a parsed file: each protocol's and composite's name and `conditions`,
 * the zones a `pbm_transcranial` block names, and the names of the zones and conditions defined.
 */
private fun namespaceRequestFile(
    entries: List<NPProtocolEntry>,
    zoneNames: List<String>,
    conditionNames: List<String>,
): JsonObject = buildJsonObject {
    putJsonArray("entries") {
        for (e in entries) when (e) {
            is NPProtocolEntry.Single -> add(buildJsonObject {
                put("kind", "single")
                putJsonObject("protocol") {
                    put("name", e.protocol.name)
                    putJsonArray("conditions") { e.protocol.conditions.forEach { add(JsonPrimitive(it)) } }
                    putJsonArray("modalities") {
                        for (m in e.protocol.modalities) {
                            val target = ((m.params as? NPModalityParams.PbmTranscranial)?.params?.target) as? NPPBMTarget.Named
                                ?: continue
                            add(buildJsonObject {
                                put("type", "pbm_transcranial")
                                putJsonObject("params") {
                                    putJsonArray("zoneRefs") { target.zoneNames.forEach { add(JsonPrimitive(it)) } }
                                }
                            })
                        }
                    }
                }
            })
            is NPProtocolEntry.Composite -> add(buildJsonObject {
                put("kind", "composite")
                putJsonObject("composite") {
                    put("name", e.composite.name)
                    putJsonArray("conditions") { e.composite.conditions.forEach { add(JsonPrimitive(it)) } }
                }
            })
            else -> Unit
        }
    }
    putJsonArray("zones") { zoneNames.forEach { add(buildJsonObject { put("name", it) }) } }
    putJsonArray("conditions") { conditionNames.forEach { add(buildJsonObject { put("name", it) }) } }
}
