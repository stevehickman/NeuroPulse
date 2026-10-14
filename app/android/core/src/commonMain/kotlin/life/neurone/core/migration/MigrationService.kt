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
import life.neurone.core.data.Argon2Provider
import life.neurone.core.data.BundleKeyDerivation
import life.neurone.core.data.BundleKeyMethod
import life.neurone.core.data.BundleSecret
import life.neurone.core.data.RandomSource
import life.neurone.core.data.hexToBytes
import life.neurone.core.data.toHex
import life.neurone.core.npps.NppsCore

// The backup and device-replacement flow on this app (SWR-MG-02…10, issue #569 package 6e).
//
// The flow itself is not here. Its ORDER (open the bundle before taking a claim, verify before confirming, confirm before
// the old phone clears, key first when it clears) is a state machine in the shared core (`neurone_npps_core::migration_flow`),
// so iOS, Android, Windows, macOS and the web run the same one and cannot disagree. The core says which effect it needs
// next; this class performs it against this app's route, storage, Argon2 and randomness, and feeds the outcome back.
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

/** SWR-MG-06, SWR-MG-09: what the old phone does at each step of its clear. Returns true when the step finished. */
interface ClearActions {
    suspend fun run(step: String): Boolean
}

/** Which clear steps are finished, kept so a clear that stops part-way resumes at the next launch (SWR-MG-08). */
interface ClearProgress {
    fun done(): Set<String>
    fun markDone(step: String)
}

class InMemoryClearProgress : ClearProgress {
    private val steps = linkedSetOf<String>()
    override fun done(): Set<String> = steps.toSet()
    override fun markDone(step: String) { steps += step }
}

sealed class ClearResult {
    /** [removeOsBond]: the app could not remove the operating system's Bluetooth bond and the screen must say so (SWR-MG-09). */
    class Cleared(val removeOsBond: Boolean) : ClearResult()

    /** Nothing was cleared: `no_confirmation` or `other_bundle`. The old phone keeps working (SWR-MG-08). */
    class Waiting(val reason: String) : ClearResult()
    class Failed(val step: String) : ClearResult()
}

/** What the effects of one flow run against: the route, the person's secret, and the old phone's clear steps. */
private class FlowContext(
    val route: BackupRoute? = null,
    val secret: BundleSecret? = null,
    val actions: ClearActions? = null,
    val progress: ClearProgress? = null,
)

class MigrationService(
    private val items: List<MigrationItemStore>,
    private val argon2: Argon2Provider,
    private val random: RandomSource,
) {
    private val byItem = items.associateBy { it.item }

    private fun ask(op: String, build: JsonObjectBuilder.() -> Unit = {}): JsonObject =
        NppsCore.migration(buildJsonObject { put("op", op); build() })

    private fun strings(a: JsonElement?): List<String> = a?.jsonArray?.map { it.jsonPrimitive.content } ?: emptyList()

    private fun routeJson(route: BackupRoute): JsonObject = buildJsonObject {
        put("id", route.id)
        put("atomicClaim", route.atomicClaim)
        put("direct", route.direct)
        put("authenticatedPairing", route.authenticatedPairing)
    }

    private fun hostItems(): JsonArray = buildJsonArray { byItem.keys.forEach { add(it) } }

    /** Run one flow to its end: ask the core, perform the effect it names, hand back the outcome, until it says done. */
    private suspend fun drive(flow: String, params: JsonObject, ctx: FlowContext): JsonObject {
        var answer = ask("flow") { put("flow", flow); put("params", params) }
        repeat(MAX_TURNS) {
            answer["done"]?.let { return it.jsonObject }
            val turn = answer
            val result = perform(turn["effect"]!!.jsonObject, ctx)
            answer = ask("flow") { put("flow", flow); put("state", turn["state"]!!); put("result", result) }
        }
        error("The $flow flow did not end in $MAX_TURNS turns")
    }

    /** The effects (the table in `migration_flow.rs`). Each is one storage, route, Argon2 or randomness call and no rule. */
    private suspend fun perform(e: JsonObject, ctx: FlowContext): JsonObject = when (val type = e["type"]!!.jsonPrimitive.content) {
        "random" -> {
            val n = e["bytes"]!!.jsonPrimitive.content.toInt()
            val bytes = random.nextBytes(n)
            require(bytes.size == n) { "The random source gave ${bytes.size} bytes, not $n" }
            buildJsonObject { put("bytes", bytes.toHex()) }
        }
        "deriveKey" -> {
            val key = BundleKeyDerivation.deriveKey(argon2, ctx.secret ?: error("no secret"), e["kdf"]!!.jsonObject)
            try { buildJsonObject { put("key", key.hex()) } } finally { key.destroy() }
        }
        "readItems" -> buildJsonObject {
            put("items", buildJsonObject {
                strings(e["names"]).forEach { n -> byItem[n]?.export()?.let { put(n, it) } }
            })
        }
        "applyItem" -> {
            byItem[e["name"]!!.jsonPrimitive.content]!!.import(e["value"]!!)
            buildJsonObject { put("ok", true) }
        }
        "getBundle" -> buildJsonObject { put("bundle", ctx.route!!.getBundle()?.let { JsonPrimitive(it.toHex()) } ?: JsonNull) }
        "putBundle" -> {
            ctx.route!!.putBundle(e["bundle"]!!.jsonPrimitive.content.hexToBytes())
            buildJsonObject { put("ok", true) }
        }
        "readClaim" -> buildJsonObject { put("claim", ctx.route!!.readClaim() ?: JsonNull) }
        "createClaim" -> buildJsonObject { put("created", ctx.route!!.createClaimIfAbsent(e["claim"]!!.jsonObject)) }
        "writeConfirmation" -> {
            ctx.route!!.writeConfirmation(e["confirmation"]!!.jsonObject)
            buildJsonObject { put("ok", true) }
        }
        "readConfirmation" -> buildJsonObject { put("confirmation", ctx.route!!.readConfirmation() ?: JsonNull) }
        "loadClearProgress" -> buildJsonObject { put("done", buildJsonArray { ctx.progress!!.done().forEach { add(it) } }) }
        "runClearStep" -> buildJsonObject { put("ok", ctx.actions!!.run(e["step"]!!.jsonPrimitive.content)) }
        "markClearStep" -> {
            ctx.progress!!.markDone(e["step"]!!.jsonPrimitive.content)
            buildJsonObject { put("ok", true) }
        }
        else -> error("unknown effect '$type'")
    }

    private fun JsonObject.ok(): Boolean = this["ok"]!!.jsonPrimitive.boolean
    private fun JsonObject.reason(): String = this["reason"]!!.jsonPrimitive.content

    /** What a kind will carry and leave out, for the screen to say before the backup (SWR-MG-03). */
    fun inventory(kind: String): JsonObject = ask("inventory") { put("kind", kind) }

    /**
     * Seal this phone's state of [kind] under the user's [secret] and put it on [route]. The UHDR-only kind is the nightly
     * backup; the migrate kind is the act of replacing a phone (SWR-MG-03). A route that cannot give an atomic claim is
     * refused for migrate and allowed for UHDR-only (SWR-MG-10).
     */
    suspend fun backup(kind: String, method: BundleKeyMethod, secret: BundleSecret, route: BackupRoute): BackupResult {
        val d = drive(
            "backup",
            buildJsonObject {
                put("kind", kind); put("method", method.wire); put("route", routeJson(route)); put("hostItems", hostItems())
            },
            FlowContext(route = route, secret = secret),
        )
        if (!d.ok()) return BackupResult.Refused(d.reason())
        return BackupResult.Done(d["bundleHash"]!!.jsonPrimitive.content, kind, strings(d["written"]), strings(d["omitted"]))
    }

    /** What is on the route, read with no key: the kind, the format version and how its key was made (SWR-MG-05). */
    suspend fun inspect(route: BackupRoute): InspectResult {
        val d = drive("inspect", buildJsonObject { }, FlowContext(route = route))
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
    suspend fun restore(route: BackupRoute, secret: BundleSecret): RestoreResult {
        val d = drive(
            "restore",
            buildJsonObject { put("route", routeJson(route)); put("hostItems", hostItems()) },
            FlowContext(route = route, secret = secret),
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
    suspend fun releaseClaim(route: BackupRoute) = route.releaseClaim()

    /**
     * The old phone clears after, and only after, it reads a confirmation that matches [bundleHash] (SWR-MG-08), then runs
     * the core's clear plan one step at a time, key first (SWR-MG-06), pairing included (SWR-MG-09). A clear already in
     * progress (a step finished in [progress]) resumes without asking again.
     */
    suspend fun clearOldPhone(
        route: BackupRoute,
        bundleHash: String,
        actions: ClearActions,
        progress: ClearProgress,
        canRemoveOsBond: Boolean,
    ): ClearResult {
        val d = drive(
            "clear",
            buildJsonObject { put("bundleHash", bundleHash); put("canRemoveOsBond", canRemoveOsBond) },
            FlowContext(route = route, actions = actions, progress = progress),
        )
        if (d.ok()) return ClearResult.Cleared(d["removeOsBond"]!!.jsonPrimitive.boolean)
        return if (d.reason() == "step_failed") ClearResult.Failed(d["step"]!!.jsonPrimitive.content) else ClearResult.Waiting(d.reason())
    }

    private companion object {
        const val MAX_TURNS = 100_000
    }
}
