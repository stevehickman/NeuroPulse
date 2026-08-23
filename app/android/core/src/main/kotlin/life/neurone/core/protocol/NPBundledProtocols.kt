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
 * **Definition files are excluded.** `manifest.json` also lists `00-zones.npps`
 * and `00-conditions.npps`, but this parser has no `zone` or `condition`
 * top-level block — [NPProtocolEntry] is Single/Composite/Limits only, and
 * anything else raises "Unexpected keyword". That is a standing gap against
 * NP-NPPS-REF-001 Rev 2, not something this loader should paper over, so it
 * takes only the files the parser can actually read. The condition registry
 * reaches Android through [NPBundledConditions], generated from the same
 * `00-conditions.npps` by `scripts/sync-conditions.ts`.
 */
object NPBundledProtocols {

    private const val BASE = "/protocols/predefined"

    private val json = Json { ignoreUnknownKeys = true }

    /** Protocol and composite file names from `manifest.json`, in manifest order. */
    val manifestFiles: List<String> by lazy {
        val raw = readResource("$BASE/manifest.json")
            ?: error(
                "protocols/predefined/manifest.json is not on the resource path — the " +
                    "bundlePredefinedProtocols Gradle task did not run."
            )
        val obj = json.parseToJsonElement(raw).jsonObject
        fun arr(key: String): List<String> =
            obj[key]?.jsonArray?.map { it.jsonPrimitive.content } ?: emptyList()
        arr("protocols") + arr("composites")
    }

    /** Raw NPPS text of every bundled protocol and composite file. */
    val allContents: List<String> by lazy {
        manifestFiles.map { name ->
            readResource("$BASE/$name")
                ?: error(
                    "protocols/predefined/$name is listed in manifest.json but is not on the " +
                        "resource path."
                )
        }
    }

    internal fun readResource(path: String): String? =
        NPBundledProtocols::class.java.getResourceAsStream(path)
            ?.bufferedReader()
            ?.use { it.readText() }
}
