package life.neurone.core.protocol

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive

/**
 * The shipped protocol library, read from the `.npps` files in
 * `protocols/predefined/` — the single source of truth for every runtime.
 *
 * Those files are copied onto the resource path at build time by the
 * `bundlePredefinedProtocols` Gradle task, so there is nothing to keep in sync
 * by hand. This object previously held the same protocols transcribed into
 * Kotlin string literals and had drifted: 8 of 17 were missing the
 * `conditions` / `references` fields added in NP-NPPS-REF-001 Rev 2.
 *
 * **Definition files load first.** `manifest.json` lists `00-zones.npps` and
 * `00-conditions.npps` before the protocols, and they are loaded in that order
 * so a protocol's zone and condition references resolve regardless of file
 * order (NP-NPPS-REF-001 §1.6). The parser gained `zone` and `condition`
 * top-level blocks in Rev 10; before that it raised on them and this loader had
 * to skip both files.
 */
object NPBundledProtocols {

    private const val BASE = "/protocols/predefined"

    private val json = Json { ignoreUnknownKeys = true }

    /**
     * Every file `manifest.json` lists, definitions first: zones, conditions,
     * then protocols and composites.
     */
    val manifestFiles: List<String> by lazy {
        val raw = readResource("$BASE/manifest.json")
            ?: error(
                "protocols/predefined/manifest.json is not on the resource path — the " +
                    "bundlePredefinedProtocols Gradle task did not run."
            )
        val obj = json.parseToJsonElement(raw).jsonObject
        fun arr(key: String): List<String> =
            obj[key]?.jsonArray?.map { it.jsonPrimitive.content } ?: emptyList()
        arr("zones") + arr("conditions") + arr("protocols") + arr("composites")
    }

    /** Protocol and composite file names only — what a protocol library lists. */
    val protocolFiles: List<String> by lazy {
        val defs = definitionFiles.toSet()
        manifestFiles.filterNot { it in defs }
    }

    /** Zone and condition definition file names, in load order. */
    val definitionFiles: List<String> by lazy {
        val raw = readResource("$BASE/manifest.json") ?: return@lazy emptyList()
        val obj = json.parseToJsonElement(raw).jsonObject
        fun arr(key: String): List<String> =
            obj[key]?.jsonArray?.map { it.jsonPrimitive.content } ?: emptyList()
        arr("zones") + arr("conditions")
    }

    /** Raw NPPS text of every bundled file, definitions first. */
    val allContents: List<String> by lazy {
        manifestFiles.map { name ->
            readResource("$BASE/$name")
                ?: error(
                    "protocols/predefined/$name is listed in manifest.json but is not on the " +
                        "resource path."
                )
        }
    }

    /**
     * Everything parsed into one namespace, with zone and condition definitions
     * resolved. Cross-reference errors are returned by
     * [validateNamespaceReferences] rather than thrown, so one bad reference
     * does not cost the caller the whole library.
     */
    val namespace: NPNamespace by lazy {
        val entries = allContents.flatMap { content ->
            runCatching { NPPSParser(NPPSLexer(content).tokenize()).parse() }.getOrDefault(emptyList())
        }
        buildNamespace(entries).namespace
    }

    internal fun readResource(path: String): String? =
        NPBundledProtocols::class.java.getResourceAsStream(path)
            ?.bufferedReader()
            ?.use { it.readText() }
}
