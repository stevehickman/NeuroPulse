package life.neurone.core.migration

import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.add
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.common.KeyValueStore
import life.neurone.core.data.BundleKeyDerivation
import life.neurone.core.data.BundleSecret
import life.neurone.core.data.hexToBytes
import life.neurone.core.data.toHex
import life.neurone.core.npps.NppsCore

// The backup and device-replacement flows as Kotlin drives them (SWR-MG-02…12, issue #569 packages 6e to 6h).
//
// The flows are state machines in the shared core (common/npps-core, migration_flow.rs): the ORDER (open the bundle before
// taking a claim, verify before confirming, confirm before the old phone clears) is decided there, once, for iOS, Android,
// Windows, macOS and the web. This file is the whole Kotlin driver, and it is the same one under the same names on every
// platform: `MigrationFlow` runs a flow, `MigrationHost` performs the effects the driver does not answer itself, and
// `KeyValueStore`, `ObjectStore` and `FileStore` are the three storage roles it does (Swift and the web have the same four names, C# has them
// with an `I` prefix on the interfaces). The driver answers three kinds of effect itself: `deriveKey` (the core's own Argon2id
// over the secret it holds), `readKeys` / `writeKeys` (the key-value store) and the six `object*` calls (the object store).
// Everything else is the host's. The table of effects is in migration_flow.rs.
//
// A flow state is opaque, is never stored, and never holds a secret or a key.

/** Where this app performs the effects the driver does not answer itself. */
interface MigrationHost {
    /** Perform one effect and return its result object (see the table in `migration_flow.rs`). */
    suspend fun perform(effect: JsonObject): JsonObject
}

object MigrationFlow {
    // A restore of a large history is a few turns per item; this only stops a flow that never ends.
    private const val MAX_TURNS = 100_000

    /**
     * Run [flow] ("backup", "inspect", "restore", "clear" or "probe") to its end and return its `done` object. [secret] is what
     * a backup or restore derives its key from; [keys] is the key-value store the plain-key items live in (the items the core
     * says are plain keys are then declared to it as `keyItems`); [store] is a route that is an object store, which the core
     * then drives itself (`object_route.rs`).
     */
    suspend fun run(
        flow: String,
        params: JsonObject,
        host: MigrationHost,
        secret: BundleSecret? = null,
        keys: KeyValueStore? = null,
        store: ObjectStore? = null,
        files: FileStore? = null,
    ): JsonObject {
        val p = buildJsonObject {
            params.forEach { (k, v) -> put(k, v) }
            if (store != null && "objectStore" !in params) put("objectStore", true)
            if (keys != null && "keyItems" !in params) {
                put("keyItems", buildJsonArray { ask("itemKeys")["carried"]!!.jsonObject.keys.forEach { add(it) } })
            }
        }
        var answer = ask("flow") { put("flow", flow); put("params", p) }
        repeat(MAX_TURNS) {
            answer["done"]?.let { return it.jsonObject }
            val turn = answer
            val result = perform(turn["effect"]!!.jsonObject, host, secret, keys, store, files)
            answer = ask("flow") { put("flow", flow); put("state", turn["state"]!!); put("result", result) }
        }
        error("The $flow flow did not end in $MAX_TURNS turns")
    }

    /** The object names the core uses, for a host that releases a claim on the person's explicit act (SWR-MG-07). */
    fun objectNames(): JsonObject = ask("objectNames")

    /**
     * SWR-MG-12: check an account on throwaway objects that hold no user data. [host] answers `random`, as for any flow. The
     * result is `{result, failedOperation?, ops}`.
     */
    suspend fun probe(store: ObjectStore, bundleBytes: Long, host: MigrationHost): JsonObject =
        run("probe", buildJsonObject { put("bundleBytes", bundleBytes) }, host, store = store)

    private fun ask(op: String, build: kotlinx.serialization.json.JsonObjectBuilder.() -> Unit = {}): JsonObject =
        NppsCore.migration(buildJsonObject { put("op", op); build() })

    private fun strings(a: JsonElement?): List<String> = a?.jsonArray?.map { it.jsonPrimitive.content } ?: emptyList()

    private suspend fun perform(
        e: JsonObject,
        host: MigrationHost,
        secret: BundleSecret?,
        keys: KeyValueStore?,
        store: ObjectStore?,
        files: FileStore?,
    ): JsonObject = when (val type = e["type"]!!.jsonPrimitive.content) {
        "deriveKey" -> {
            val key = BundleKeyDerivation.deriveKey(secret ?: error("no secret to derive a key from"), e["kdf"]!!.jsonObject)
            try { buildJsonObject { put("key", key.hex()) } } finally { key.destroy() }
        }
        "readKeys" -> if (keys == null) host.perform(e) else buildJsonObject {
            put("values", buildJsonObject { strings(e["keys"]).forEach { k -> keys.getString(k)?.let { put(k, JsonPrimitive(it)) } } })
        }
        "writeKeys" -> if (keys == null) host.perform(e) else {
            strings(e["remove"]).forEach { keys.remove(it) }
            e["set"]!!.jsonObject.forEach { (k, v) -> keys.putString(k, v.jsonPrimitive.content) }
            buildJsonObject { put("ok", true) }
        }
        "objectGet", "objectPut", "objectCreate", "objectDelete", "objectList", "objectSpace" ->
            if (store == null) host.perform(e) else performObject(type, e, store)
        "listFiles", "writeFile", "syncFile", "deleteFile" -> if (files == null) host.perform(e) else performFile(type, e, files)
        else -> host.perform(e)
    }

    /** One file call of the clear. A call that throws is reported to the core as failed, and the clear stops at that step (SWR-MG-06). */
    private suspend fun performFile(type: String, e: JsonObject, files: FileStore): JsonObject {
        val path = e["path"]?.jsonPrimitive?.content ?: ""
        return try {
            when (type) {
                "listFiles" -> buildJsonObject {
                    put("files", buildJsonArray {
                        files.list().forEach { f -> add(buildJsonObject { put("path", f.path); put("size", f.size) }) }
                    })
                }
                "writeFile" -> {
                    files.write(path, e["offset"]!!.jsonPrimitive.content.toLong(), e["data"]!!.jsonPrimitive.content.hexToBytes())
                    buildJsonObject { put("ok", true) }
                }
                "syncFile" -> { files.sync(path); buildJsonObject { put("ok", true) } }
                else -> { files.delete(path); buildJsonObject { put("ok", true) } }
            }
        } catch (ex: Exception) {
            if (ex is kotlin.coroutines.cancellation.CancellationException) throw ex
            buildJsonObject { put("failed", true) }
        }
    }

    /** One object call. A call that throws is reported to the core as failed; the core decides what that means (SWR-MG-11). */
    private suspend fun performObject(type: String, e: JsonObject, store: ObjectStore): JsonObject {
        val name = e["name"]?.jsonPrimitive?.content ?: ""
        return try {
            when (type) {
                "objectGet" -> buildJsonObject { put("data", store.get(name)?.let { JsonPrimitive(it.toHex()) } ?: JsonNull) }
                "objectPut" -> { store.put(name, e["data"]!!.jsonPrimitive.content.hexToBytes()); buildJsonObject { put("ok", true) } }
                "objectCreate" -> buildJsonObject { put("created", store.createIfAbsent(name, e["data"]!!.jsonPrimitive.content.hexToBytes())) }
                "objectDelete" -> { store.delete(name); buildJsonObject { put("ok", true) } }
                "objectList" -> buildJsonObject { put("names", buildJsonArray { store.list(e["prefix"]!!.jsonPrimitive.content).forEach { add(it) } }) }
                else -> buildJsonObject { put("bytes", store.freeSpace()?.let { JsonPrimitive(it) } ?: JsonNull) }
            }
        } catch (ex: Exception) {
            if (ex is kotlin.coroutines.cancellation.CancellationException) throw ex
            buildJsonObject { put("failed", true) }
        }
    }
}
