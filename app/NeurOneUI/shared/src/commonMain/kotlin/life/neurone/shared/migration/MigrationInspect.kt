package life.neurone.shared.migration

import kotlinx.serialization.json.boolean
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.common.KeyValueStore
import life.neurone.core.data.BundleKeyMethod
import life.neurone.core.data.RandomSource
import life.neurone.core.migration.MigrationFlow
import life.neurone.core.migration.StandardMigrationHost
import life.neurone.core.migration.toJson
import life.neurone.shared.MigrationPlace

// Reading the header of the backup on a place, which needs no secret (SWR-MG-05): what kind it is, how it is locked and its
// hash. The restore screen asks it to know what to ask the person for; the clear screen asks it for the hash it clears against.

/** What the header of the backup on a place says. */
class BundleInfo(val kind: BackupKind?, val method: BundleKeyMethod?, val bundleHash: String)

internal sealed interface Inspected {
    class Found(val info: BundleInfo) : Inspected

    /** The core's reason code: `no_bundle`, `malformed`, `newer_version`, `unknown_kind` or `route_unavailable`. */
    class Failed(val reason: String) : Inspected
}

internal suspend fun inspectBundle(place: MigrationPlace, keys: KeyValueStore, random: RandomSource): Inspected {
    val done = MigrationFlow.run(
        "inspect", buildJsonObject { put("route", place.route.toJson()) }, StandardMigrationHost(), null, null, place.store,
        random = random,
    )
    if (!done["ok"]!!.jsonPrimitive.boolean) return Inspected.Failed(done["reason"]!!.jsonPrimitive.content)
    val kind = BackupKind.entries.firstOrNull { it.wire == done["kind"]!!.jsonPrimitive.content }
    val method = BundleKeyMethod.fromWire(done["method"]!!.jsonPrimitive.content)
    return Inspected.Found(BundleInfo(kind, method, done["bundleHash"]!!.jsonPrimitive.content))
}
