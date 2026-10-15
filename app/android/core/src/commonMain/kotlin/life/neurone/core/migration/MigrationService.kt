package life.neurone.core.migration

import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonObjectBuilder
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.add
import kotlinx.serialization.json.boolean
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.data.BundleKeyMethod
import life.neurone.core.data.BundleSecret
import life.neurone.core.common.KeyValueStore
import life.neurone.core.data.RandomSource
import life.neurone.core.data.hexToBytes
import life.neurone.core.data.toHex
import life.neurone.core.npps.NppsCore

// The backup and device-replacement flow on this app (SWR-MG-02…10, issue #569 package 6e).
//
// The flow itself is not here. Its ORDER (open the bundle before taking a claim, verify before confirming, confirm before
// the old phone clears, key first when it clears) is a state machine in the shared core (`neurone_npps_core::migration_flow`),
// so iOS, Android, Windows, macOS and the web run the same one and cannot disagree. The core says which effect it needs
// next; this class performs it against this app's route, storage and randomness (Argon2id is the core's too) and feeds the
// outcome back.
// It produces no text for a person: a result is a code, and the screen words it from the locale files (CLAUDE.md §17).
//
// The secret stays in this class and the key is made, used for one answer and destroyed: neither is ever in a flow state.
// The route sees only the sealed bundle, the claim marker (a random token and the bundle hash) and the confirmation (the
// bundle hash): nothing about the user or the device.

sealed class BackupResult {
    class Done(val bundleHash: String, val kind: String, val written: List<String>, val omitted: List<String>) : BackupResult()
    class Refused(val reason: String) : BackupResult()
}

sealed class InspectResult {
    class Found(val kind: String, val method: BundleKeyMethod, val formatVersion: Int, val bundleHash: String) : InspectResult()
    class Refused(val reason: String) : InspectResult()
}

sealed class RestoreResult {
    class Restored(
        val kind: String,
        val bundleHash: String,
        /** Items written to this phone. */
        val restored: List<String>,
        /** Items the kind carries and the bundle lacked. */
        val missing: List<String>,
        /** Items the kind never carries. */
        val omitted: List<String>,
        /** Items the bundle held that this build has nothing to write them to, so they were not applied (consent, today). */
        val notApplied: List<String>,
        /** SWR-MG-04: the app asks about consent, and tells the person the consent choices were not restored. */
        val askConsent: Boolean,
        val tellConsentNotRestored: Boolean,
        /** SWR-MG-08: the confirmation landed on the route. False for a UHDR-only bundle, which needs none. */
        val confirmed: Boolean,
    ) : RestoreResult()

    /** [reason] is a code: `no_bundle`, `malformed`, `unknown_kind`, `newer_version`, `unsupported_kdf`, `authentication_failed`,
     * `route_not_offered`, `claimed`, `claim_not_owned`, `verify_failed`. */
    class Refused(val reason: String) : RestoreResult()
}

/**
 * SWR-MG-06, SWR-MG-09: the old phone's clear steps that are the platform's own. The core runs the rest itself through the key
 * store (the last paired helmet id, every key the lists name, the cleared marker, and which steps are finished), so an app
 * writes only these three: `destroyEncryptionKey`, `overwriteAndDeleteFiles` and `forgetPairingRecord`. Returns true when the
 * step finished.
 */
interface ClearActions {
    suspend fun run(step: String): Boolean
}

/** [result] is `sufficient`, `backup_only` or `insufficient`; [failedOperation] names the first operation that failed (SWR-MG-12). */
class ProbeResult(val result: String, val failedOperation: String?)

sealed class ClearResult {
    /** [removeOsBond]: the app could not remove the operating system's Bluetooth bond and the screen must say so (SWR-MG-09). */
    class Cleared(val removeOsBond: Boolean) : ClearResult()

    /** Nothing was cleared: `no_confirmation` or `other_bundle`. The old phone keeps working (SWR-MG-08). */
    class Waiting(val reason: String) : ClearResult()
    class Failed(val step: String) : ClearResult()
}

/**
 * The effects the driver ([MigrationFlow]) does not answer itself, for this app: randomness, the items stored another way,
 * a route the app writes itself ([BackupRoute]) and the platform's clear steps ([ClearActions]).
 */
private class ServiceHost(
    private val random: RandomSource,
    private val byItem: Map<String, MigrationItemStore>,
    private val route: RouteDescriptor?,
    private val actions: ClearActions?,
) : MigrationHost {
    private fun strings(a: JsonElement?): List<String> = a?.jsonArray?.map { it.jsonPrimitive.content } ?: emptyList()
    private fun typed(): BackupRoute = route as? BackupRoute ?: error("this route is an object store; the core drives it")

    override suspend fun perform(effect: JsonObject): JsonObject = when (val type = effect["type"]!!.jsonPrimitive.content) {
        "random" -> {
            val n = effect["bytes"]!!.jsonPrimitive.content.toInt()
            val bytes = random.nextBytes(n)
            require(bytes.size == n) { "The random source gave ${bytes.size} bytes, not $n" }
            buildJsonObject { put("bytes", bytes.toHex()) }
        }
        "readItems" -> buildJsonObject {
            put("items", buildJsonObject {
                strings(effect["names"]).forEach { n -> byItem[n]?.export()?.let { put(n, it) } }
            })
        }
        "applyItem" -> {
            byItem[effect["name"]!!.jsonPrimitive.content]!!.import(effect["value"]!!)
            buildJsonObject { put("ok", true) }
        }
        "getBundle" -> buildJsonObject { put("bundle", typed().getBundle()?.let { JsonPrimitive(it.toHex()) } ?: JsonNull) }
        "putBundle" -> {
            typed().putBundle(effect["bundle"]!!.jsonPrimitive.content.hexToBytes())
            buildJsonObject { put("ok", true) }
        }
        "readClaim" -> buildJsonObject { put("claim", typed().readClaim() ?: JsonNull) }
        "createClaim" -> buildJsonObject { put("created", typed().createClaimIfAbsent(effect["claim"]!!.jsonObject)) }
        "writeConfirmation" -> {
            typed().writeConfirmation(effect["confirmation"]!!.jsonObject)
            buildJsonObject { put("ok", true) }
        }
        "readConfirmation" -> buildJsonObject { put("confirmation", typed().readConfirmation() ?: JsonNull) }
        "runClearStep" -> buildJsonObject { put("ok", actions!!.run(effect["step"]!!.jsonPrimitive.content)) }
        else -> error("unknown effect '$type'")
    }
}

/**
 * The typed outcomes on top of [MigrationFlow], for Kotlin callers that want a sealed type instead of the `done` object the
 * other platforms return. It holds no flow logic: each call builds the flow's params and hands them to [MigrationFlow.run].
 *
 * @param kv the app's key-value store: the items that are plain keys (protocols, session history, limits profiles,
 *   characterisation enrolment) are read and written through it, by the key list the core holds.
 * @param adapters items this app stores another way, read and written whole (consent, once its adapter is wired).
 */
class MigrationService(
    private val kv: KeyValueStore,
    private val adapters: List<MigrationItemStore>,
    private val random: RandomSource,
) {
    private val byItem = adapters.associateBy { it.item }

    private fun ask(op: String, build: JsonObjectBuilder.() -> Unit = {}): JsonObject =
        NppsCore.migration(buildJsonObject { put("op", op); build() })

    private fun strings(a: JsonElement?): List<String> = a?.jsonArray?.map { it.jsonPrimitive.content } ?: emptyList()

    private fun routeJson(route: RouteDescriptor): JsonObject = buildJsonObject {
        put("id", route.id)
        put("atomicClaim", route.atomicClaim)
        put("direct", route.direct)
        put("authenticatedPairing", route.authenticatedPairing)
    }

    private fun hostItems(): JsonArray = buildJsonArray { byItem.keys.forEach { add(it) } }

    private suspend fun drive(
        flow: String,
        params: JsonObject,
        route: RouteDescriptor? = null,
        secret: BundleSecret? = null,
        actions: ClearActions? = null,
    ): JsonObject = MigrationFlow.run(
        flow, params, ServiceHost(random, byItem, route, actions), secret, kv, (route as? ObjectStoreRoute)?.store,
    )

    /** The params that say which route a flow runs over: its descriptor. */
    private fun JsonObjectBuilder.putRoute(route: RouteDescriptor) {
        put("route", routeJson(route))
    }

    /** What a kind will carry and leave out, for the screen to say before the backup (SWR-MG-03). */
    fun inventory(kind: String): JsonObject = ask("inventory") { put("kind", kind) }

    /**
     * Seal this phone's state of [kind] under the user's [secret] and put it on [route]. The UHDR-only kind is the nightly
     * backup; the migrate kind is the act of replacing a phone (SWR-MG-03). A route that cannot give an atomic claim is
     * refused for migrate and allowed for UHDR-only (SWR-MG-10).
     */
    suspend fun backup(kind: String, method: BundleKeyMethod, secret: BundleSecret, route: RouteDescriptor): BackupResult {
        val d = drive(
            "backup",
            buildJsonObject {
                put("kind", kind); put("method", method.wire); putRoute(route)
                put("hostItems", hostItems())
            },
            route = route, secret = secret,
        )
        if (!d.ok()) return BackupResult.Refused(d.reason())
        return BackupResult.Done(d["bundleHash"]!!.jsonPrimitive.content, kind, strings(d["written"]), strings(d["omitted"]))
    }

    /** What is on the route, read with no key: the kind, the format version and how its key was made (SWR-MG-05). */
    suspend fun inspect(route: RouteDescriptor): InspectResult {
        val d = drive("inspect", buildJsonObject { }, route = route)
        if (!d.ok()) return InspectResult.Refused(d.reason())
        return InspectResult.Found(
            kind = d["kind"]!!.jsonPrimitive.content,
            method = BundleKeyMethod.fromWire(d["method"]!!.jsonPrimitive.content) ?: return InspectResult.Refused("unsupported_kdf"),
            formatVersion = d["formatVersion"]!!.jsonPrimitive.content.toInt(),
            bundleHash = d["bundleHash"]!!.jsonPrimitive.content,
        )
    }

    /**
     * Restore the bundle on [route] onto this phone. The order, and why, is the core's (`migration_flow.rs`): the bundle is
     * opened before any claim is taken, every applied item is read back and compared before the confirmation is written,
     * and the confirmation is the last act (SWR-MG-07, SWR-MG-08).
     */
    suspend fun restore(route: RouteDescriptor, secret: BundleSecret): RestoreResult {
        val d = drive(
            "restore",
            buildJsonObject { putRoute(route); put("hostItems", hostItems()) },
            route = route, secret = secret,
        )
        if (!d.ok()) return RestoreResult.Refused(d.reason())
        return RestoreResult.Restored(
            kind = d["kind"]!!.jsonPrimitive.content,
            bundleHash = d["bundleHash"]!!.jsonPrimitive.content,
            restored = strings(d["restored"]),
            missing = strings(d["missing"]),
            omitted = strings(d["omitted"]),
            notApplied = strings(d["notApplied"]),
            askConsent = d["askConsent"]!!.jsonPrimitive.boolean,
            tellConsentNotRestored = d["tellConsentNotRestored"]!!.jsonPrimitive.boolean,
            confirmed = d["confirmed"]!!.jsonPrimitive.boolean,
        )
    }

    /** The person's explicit release of a claim that stands without a confirmation (SWR-MG-07). Not a flow. */
    suspend fun releaseClaim(route: RouteDescriptor) {
        when (route) {
            is BackupRoute -> route.releaseClaim()
            is ObjectStoreRoute -> route.store.delete(MigrationFlow.objectNames()["claim"]!!.jsonPrimitive.content)
            else -> error("unknown route")
        }
    }

    /**
     * SWR-MG-12: check an account on throwaway objects that hold no user data, and say whether it is sufficient for
     * migration and backup, for the UHDR-only backup alone, or neither, naming the first operation that failed. The core
     * runs every operation and deletes what it made, even when one fails part-way. It does not prove a claim safe under two
     * simultaneous writers (that is the provider's contract test, SWR-MG-11).
     */
    suspend fun probe(store: ObjectStore, bundleBytes: Long): ProbeResult {
        val d = MigrationFlow.probe(store, bundleBytes, ServiceHost(random, byItem, null, null))
        return ProbeResult(
            result = d["result"]!!.jsonPrimitive.content,
            failedOperation = d["failedOperation"]?.jsonPrimitive?.content,
        )
    }

    /**
     * The old phone clears after, and only after, it reads a confirmation that matches [bundleHash] (SWR-MG-08), then runs
     * the core's clear plan one step at a time, key first (SWR-MG-06), pairing included (SWR-MG-09). A clear already in
     * progress (the core keeps which steps are finished under a key of its own) resumes without asking again.
     */
    suspend fun clearOldPhone(
        route: RouteDescriptor,
        bundleHash: String,
        actions: ClearActions,
        canRemoveOsBond: Boolean,
    ): ClearResult {
        val d = drive(
            "clear",
            buildJsonObject { put("bundleHash", bundleHash); put("canRemoveOsBond", canRemoveOsBond) },
            route = route, actions = actions,
        )
        if (d.ok()) return ClearResult.Cleared(d["removeOsBond"]!!.jsonPrimitive.boolean)
        return if (d.reason() == "step_failed") ClearResult.Failed(d["step"]!!.jsonPrimitive.content) else ClearResult.Waiting(d.reason())
    }
}
