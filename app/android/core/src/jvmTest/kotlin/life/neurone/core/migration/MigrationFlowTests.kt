package life.neurone.core.migration

import kotlinx.coroutines.async
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.test.runTest
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.boolean
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.data.BundleKeyDerivation
import life.neurone.core.data.BundleKeyMethod
import life.neurone.core.data.BundleSecret
import life.neurone.core.data.BundleSecretResult
import life.neurone.core.data.RandomSource
import life.neurone.core.npps.NppsCore
import life.neurone.core.platform.epochSeconds
import kotlinx.coroutines.flow.first
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertIs
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** Distinct bytes on every call, so each salt, nonce and claim token differs. */
private class StepRandom(private var n: Int = 0) : RandomSource {
    override fun nextBytes(count: Int): ByteArray = ByteArray(count) { (n++ * 31 + 7).toByte() }
}

private class MapItemStore(override val item: String, var value: JsonElement? = null) : MigrationItemStore {
    override fun export(): JsonElement? = value
    override fun import(value: JsonElement) { this.value = value }
    override fun clear() { value = null }
}

private class Recorder(private val failOn: String? = null) : ClearActions {
    val ran = mutableListOf<String>()
    override suspend fun run(step: String): Boolean {
        if (step == failOn) return false
        ran += step
        return true
    }
}

/** The app's files in memory, with every call recorded in order and one call that can be made to throw. */
private class MemoryFileStore(val events: MutableList<String>, var failOn: String? = null) : FileStore {
    val files = linkedMapOf<String, ByteArray>()
    private fun check(call: String, path: String) { if (failOn == "$call:$path") error("$call failed") }
    override suspend fun list(): List<StoredFile> = files.map { (p, b) -> StoredFile(p, b.size.toLong()) }
    override suspend fun write(path: String, offset: Long, bytes: ByteArray) {
        check("write", path)
        bytes.copyInto(files.getValue(path), offset.toInt())
        events += "write:$path:$offset:${bytes.size}"
    }
    override suspend fun sync(path: String) { check("sync", path); events += "sync:$path" }
    override suspend fun delete(path: String) { check("delete", path); files.remove(path); events += "delete:$path" }
}

/** A provider with named objects and a create-if-absent the store itself arbitrates. */
private class MemoryObjectStore(var conditional: Boolean = true, var space: Long? = 1L shl 30, var failOn: String? = null) : ObjectStore {
    val objects = linkedMapOf<String, ByteArray>()
    private fun check(call: String) { if (call == failOn) error("$call failed") }
    override suspend fun get(name: String): ByteArray? { check("get"); return objects[name]?.copyOf() }
    override suspend fun put(name: String, bytes: ByteArray) { check("put"); objects[name] = bytes.copyOf() }
    override suspend fun createIfAbsent(name: String, bytes: ByteArray): Boolean {
        check("create")
        if (conditional && name in objects) return false
        objects[name] = bytes.copyOf()
        return true
    }
    override suspend fun delete(name: String) { check("delete"); objects.remove(name) }
    override suspend fun list(prefix: String): List<String> { check("list"); return objects.keys.filter { it.startsWith(prefix) } }
    override suspend fun freeSpace(): Long? { check("space"); return space }
}

/** One end of an in-memory pipe that delivers a few bytes at a time, so a frame always arrives in pieces. */
private class PipeEnd(private val out: Channel<ByteArray>, private val inbox: Channel<ByteArray>, private val piece: Int = 7) : Link {
    var broken = false
    override suspend fun send(bytes: ByteArray) {
        if (broken) error("the link is broken")
        bytes.toList().chunked(piece).forEach { out.send(it.toByteArray()) }
    }
    override suspend fun receive(): ByteArray? {
        if (broken) error("the link is broken")
        return inbox.receiveCatching().getOrNull()
    }
    override suspend fun close() { out.close() }
}

private fun pipe(): Pair<PipeEnd, PipeEnd> {
    val ab = Channel<ByteArray>(Channel.UNLIMITED)
    val ba = Channel<ByteArray>(Channel.UNLIMITED)
    return PipeEnd(ab, ba) to PipeEnd(ba, ab)
}

private fun passphrase(text: String): BundleSecret = (BundleKeyDerivation.passphraseSecret(text) as BundleSecretResult.Ok).secret

private const val SECRET = "a long enough passphrase"
private const val CODE = "12345678"

private fun offer() = PairingOffer(CODE, epochSeconds() + 120, 3)

/** The keys of the four items that are plain keys, from the core: the one list every platform shares. */
private val carriedKeys: Map<String, List<String>> by lazy {
    NppsCore.migration(buildJsonObject { put("op", "itemKeys") })["carried"]!!.jsonObject
        .mapValues { (_, v) -> v.jsonArray.map { it.jsonPrimitive.content } }
}

// The raw `done` object every platform returns, read by key.
private val JsonObject.ok get() = this["ok"]!!.jsonPrimitive.boolean
private val JsonObject.reason get() = this["reason"]!!.jsonPrimitive.content
private fun JsonObject.text(key: String) = this[key]!!.jsonPrimitive.content
private fun JsonObject.flag(key: String) = this[key]!!.jsonPrimitive.boolean
private fun JsonObject.strings(key: String) = this[key]!!.jsonArray.map { it.jsonPrimitive.content }

/** A phone: its key-value store, the items it stores another way, and its randomness. Each call runs one flow through MigrationFlow. */
private class Phone(val kv: InMemoryKeyValueStore = InMemoryKeyValueStore(), private val extra: List<MigrationItemStore> = emptyList(), seed: Int = 0) {
    private val random = StepRandom(seed)

    fun fill() {
        carriedKeys.values.flatten().forEach { kv.putString(it, "value-of-$it") }
    }

    private fun host(route: BackupRoute? = null, actions: ClearActions? = null) = StandardMigrationHost(extra, route, actions)

    private fun params(route: RouteDescriptor, build: kotlinx.serialization.json.JsonObjectBuilder.() -> Unit = {}) = buildJsonObject {
        put("route", route.toJson())
        put("hostItems", JsonArray(host().hostItems.map { JsonPrimitive(it) }))
        build()
    }

    suspend fun backup(kind: String, method: BundleKeyMethod, route: RouteDescriptor, backupRoute: BackupRoute? = null, store: ObjectStore? = null) =
        MigrationFlow.run(
            "backup", params(route) { put("kind", kind); put("method", method.wire) }, host(backupRoute), passphrase(SECRET), kv, store,
            random = random,
        )

    suspend fun restore(
        route: RouteDescriptor, backupRoute: BackupRoute? = null, store: ObjectStore? = null, secret: String = SECRET, link: Link? = null,
        code: String? = if (link != null) CODE else null,
    ) = MigrationFlow.run(
        "restore", params(route) { code?.let { put("pairing", buildJsonObject { put("code", it) }) } }, host(backupRoute), passphrase(secret), kv, store,
        link = link, random = random,
    )

    suspend fun inspect(backupRoute: BackupRoute? = null, store: ObjectStore? = null) =
        MigrationFlow.run("inspect", buildJsonObject { }, host(backupRoute), null, kv, store, random = random)

    suspend fun clear(route: BackupRoute, bundleHash: String, actions: ClearActions, canRemoveOsBond: Boolean, files: FileStore? = null) =
        MigrationFlow.run(
            "clear", buildJsonObject { put("bundleHash", bundleHash); put("canRemoveOsBond", canRemoveOsBond) }, host(route, actions), null, kv,
            files = files, random = random,
        )

    suspend fun probe(store: ObjectStore, bundleBytes: Long) = MigrationFlow.probe(store, bundleBytes, host(), random)
}

/** The migration flow over the real core (JNI) with an in-memory route. The rules are tested in Rust; this holds the order of the calls. */
class MigrationFlowTests {

    // @verifies SWR-MG-01
    // @verifies SWR-MG-02
    // @verifies SWR-MG-08
    @Test
    fun aMigrationMovesEveryCarriedItemAndTheRouteHoldsOnlyCiphertextAndTheBundleHash() = runTest {
        val route = InMemoryBackupRoute()
        val old = Phone(seed = 1).also { it.fill() }
        val done = old.backup("migrate", BundleKeyMethod.PASSPHRASE, route, route)
        assertTrue(done.ok)
        assertFalse(route.getBundle()!!.decodeToString().contains("value-of-np.session.history"), "plaintext must not be on the route")

        val fresh = Phone(seed = 50)
        val r = fresh.restore(route, route)
        assertTrue(r.ok)
        carriedKeys.values.flatten().forEach { assertEquals("value-of-$it", fresh.kv.getString(it), it) }
        assertTrue(r.flag("confirmed"))
        assertEquals(done.text("bundleHash"), route.readConfirmation()!!["bundleHash"]!!.jsonPrimitive.content)
        assertEquals(setOf("bundleHash"), route.readConfirmation()!!.keys, "the confirmation carries the hash and nothing else")
        assertEquals(setOf("token", "bundleHash"), route.readClaim()!!.keys, "the claim carries a token and the hash and nothing else")
    }

    // @verifies SWR-MG-03
    // @verifies SWR-MG-07
    @Test
    fun theNightlyBackupLeavesOutProtocolsAndLimitsAndIsNotClaimed() = runTest {
        val route = InMemoryBackupRoute(atomicClaim = false)
        val old = Phone(seed = 1).also { it.fill() }
        val done = old.backup("uhdr", BundleKeyMethod.RECOVERY_CODE, route, route)
        assertTrue(done.ok)
        assertTrue("protocols" in done.strings("omitted") && "limitsProfiles" in done.strings("omitted"))
        assertEquals(setOf("sessionHistory", "characterisationEnrolment"), done.strings("written").toSet())

        val fresh = Phone(seed = 50)
        val r = fresh.restore(route, route)
        assertTrue(r.ok)
        assertNull(fresh.kv.getString("np.user-protocols.npps"), "a UHDR-only restore brings no protocols")
        assertEquals("value-of-np.session.history", fresh.kv.getString("np.session.history"))
        assertTrue("protocols" in r.strings("omitted"))
        assertNull(route.readClaim(), "a UHDR-only bundle is not claimed")
        assertFalse(r.flag("confirmed"))
        assertTrue(r.flag("askConsent") && r.flag("tellConsentNotRestored"), "a UHDR-only restore never brings the consent record")
    }

    // @verifies SWR-MG-10
    @Test
    fun aRouteWithoutAnAtomicClaimCannotCarryAMigrationButCanCarryTheBackup() = runTest {
        val route = InMemoryBackupRoute(atomicClaim = false)
        val old = Phone(seed = 1).also { it.fill() }
        val m = old.backup("migrate", BundleKeyMethod.PASSPHRASE, route, route)
        assertFalse(m.ok)
        assertEquals("route_not_offered", m.reason)
        assertNull(route.getBundle(), "nothing was written")
        assertTrue(old.backup("uhdr", BundleKeyMethod.PASSPHRASE, route, route).ok)
    }

    // @verifies SWR-MG-07
    @Test
    fun aWrongSecretOpensNothingAndDoesNotTakeTheClaim() = runTest {
        val route = InMemoryBackupRoute()
        Phone(seed = 1).also { it.fill() }.backup("migrate", BundleKeyMethod.PASSPHRASE, route, route)
        val fresh = Phone(seed = 50)
        val r = fresh.restore(route, route, secret = "a different passphrase")
        assertFalse(r.ok)
        assertEquals("authentication_failed", r.reason)
        assertNull(route.readClaim(), "a typing slip must not leave a claim only the person can release")
        assertNull(fresh.kv.getString("np.session.history"))
    }

    // @verifies SWR-MG-07
    @Test
    fun aSecondPhoneCannotRestoreABundleAlreadyClaimedUntilThePersonReleasesIt() = runTest {
        val route = InMemoryBackupRoute()
        Phone(seed = 1).also { it.fill() }.backup("migrate", BundleKeyMethod.PASSPHRASE, route, route)
        assertTrue(Phone(seed = 50).restore(route, route).ok)

        val second = Phone(seed = 90)
        val r = second.restore(route, route)
        assertFalse(r.ok)
        assertEquals("claimed", r.reason)
        assertNull(second.kv.getString("np.session.history"))

        route.releaseClaim()
        assertNull(route.readClaim())
        assertTrue(second.restore(route, route).ok)
    }

    // @verifies SWR-MG-08
    @Test
    fun aConfirmationThatDoesNotLandIsWrittenAgainAndAnUnconfirmedRestoreSaysSo() = runTest {
        val route = InMemoryBackupRoute()
        Phone(seed = 1).also { it.fill() }.backup("migrate", BundleKeyMethod.PASSPHRASE, route, route)
        route.dropConfirmationWrites = 2
        val r = Phone(seed = 50).restore(route, route)
        assertTrue(r.ok)
        assertTrue(r.flag("confirmed"), "the third write lands")

        val route2 = InMemoryBackupRoute()
        Phone(seed = 1).also { it.fill() }.backup("migrate", BundleKeyMethod.PASSPHRASE, route2, route2)
        route2.dropConfirmationWrites = 3
        val r2 = Phone(seed = 50).restore(route2, route2)
        assertTrue(r2.ok)
        assertFalse(r2.flag("confirmed"))
        assertNull(route2.readConfirmation(), "no confirmation, so the old phone clears nothing")
    }

    // @verifies SWR-MG-04
    @Test
    fun aRestoreNeverResetsAWithdrawalAndAnItemWithNowhereToGoIsReportedNotApplied() = runTest {
        val route = InMemoryBackupRoute()
        val oldWithdrawal = MapItemStore("withdrawalState", buildJsonObject {
            put("blanketConsentWithdrawnAt", "2026-03-01T00:00:00Z")
            put("studyWithdrawals", buildJsonObject { put("study-a", "2026-02-01T00:00:00Z") })
            put("categoryWithdrawals", buildJsonObject { })
        })
        val old = Phone(extra = listOf(oldWithdrawal), seed = 1).also { it.fill() }
        old.backup("migrate", BundleKeyMethod.PASSPHRASE, route, route)

        // The new phone already holds a LATER withdrawal and another study's: the union stands, and the later time.
        val newWithdrawal = MapItemStore("withdrawalState", buildJsonObject {
            put("blanketConsentWithdrawnAt", "2026-06-01T00:00:00Z")
            put("studyWithdrawals", buildJsonObject { put("study-b", "2026-05-01T00:00:00Z") })
            put("categoryWithdrawals", buildJsonObject { })
        })
        val fresh = Phone(extra = listOf(newWithdrawal), seed = 50)
        val r = fresh.restore(route, route)
        assertTrue(r.ok)
        val merged = newWithdrawal.value!!.jsonObject
        assertEquals("2026-06-01T00:00:00Z", merged["blanketConsentWithdrawnAt"]!!.jsonPrimitive.content)
        assertEquals(setOf("study-a", "study-b"), merged["studyWithdrawals"]!!.jsonObject.keys)
        // Grants are never carried: the person is asked again, and told their earlier choices were not restored.
        assertTrue(r.flag("askConsent") && r.flag("tellConsentNotRestored"))
    }

    // @verifies SWR-MG-08
    // @verifies SWR-MG-06
    // @verifies SWR-MG-09
    @Test
    fun theOldPhoneClearsOnlyAfterTheMatchingConfirmationAndKeyFirstAndPairingIncluded() = runTest {
        val route = InMemoryBackupRoute()
        val old = Phone(seed = 1).also { it.fill() }
        val done = old.backup("migrate", BundleKeyMethod.PASSPHRASE, route, route)
        val actions = Recorder()

        val before = old.clear(route, done.text("bundleHash"), actions, canRemoveOsBond = false)
        assertFalse(before.ok)
        assertEquals("no_confirmation", before.reason)
        assertTrue(actions.ran.isEmpty(), "nothing is cleared without a confirmation")

        Phone(seed = 50).restore(route, route)
        val other = old.clear(route, "not-this-bundle", actions, canRemoveOsBond = false)
        assertEquals("other_bundle", other.reason)

        val after = old.clear(route, done.text("bundleHash"), actions, canRemoveOsBond = false)
        assertTrue(after.ok)
        assertTrue(after.flag("removeOsBond"), "the app could not remove the OS bond, so the person is told to")
        assertEquals("destroyEncryptionKey", actions.ran.first(), "the key goes first, so what flash still holds is unreadable")
        assertEquals(listOf("destroyEncryptionKey", "forgetPairingRecord"), actions.ran, "the app does only its own steps")
        assertEquals("true", old.kv.getString("np.migration.cleared"), "the proof that the clear ran stays")
        assertNull(old.kv.getString("np.migration.clear-progress"), "the progress key goes when the plan is complete")
        assertNull(old.kv.getString("ble.last-hub-id"), "the core forgets the last paired helmet id itself")
    }

    // @verifies SWR-MG-08
    @Test
    fun aClearThatStopsPartWayResumesFromTheStepItReachedWithoutAskingAgain() = runTest {
        val route = InMemoryBackupRoute()
        val old = Phone(seed = 1).also { it.fill() }
        val done = old.backup("migrate", BundleKeyMethod.PASSPHRASE, route, route)
        Phone(seed = 50).restore(route, route)

        val first = Recorder(failOn = "forgetPairingRecord")
        val r1 = old.clear(route, done.text("bundleHash"), first, canRemoveOsBond = true)
        assertFalse(r1.ok)
        assertEquals("forgetPairingRecord", r1.text("step"))
        assertEquals(listOf("destroyEncryptionKey"), first.ran)

        // Next launch: the route is unreachable (no confirmation to read) but the clear is already under way.
        val gone = InMemoryBackupRoute()
        val second = Recorder()
        val r2 = old.clear(gone, done.text("bundleHash"), second, canRemoveOsBond = true)
        assertTrue(r2.ok)
        assertFalse(r2.flag("removeOsBond"))
        assertEquals("forgetPairingRecord", second.ran.first(), "steps already done are not run again")
        assertFalse("destroyEncryptionKey" in second.ran)
    }

    // @verifies SWR-MG-06
    // @verifies SWR-MG-08
    @Test
    fun theFilesAreOverwrittenInFullSyncedAndDeletedAfterTheKeyAndAFailedCallResumesAtTheNextLaunch() = runTest {
        val route = InMemoryBackupRoute()
        val old = Phone(seed = 1).also { it.fill() }
        val done = old.backup("migrate", BundleKeyMethod.PASSPHRASE, route, route)
        Phone(seed = 50).restore(route, route)

        val events = mutableListOf<String>()
        val files = MemoryFileStore(events)
        files.files["a-big.db"] = ByteArray(40_000) { 0x11 }
        files.files["b-empty.db"] = ByteArray(0)
        files.files["c-small.db"] = ByteArray(100) { 0x22 }
        files.failOn = "delete:c-small.db"
        val first = object : ClearActions {
            override suspend fun run(step: String): Boolean { events += "run:$step"; return true }
        }
        val r1 = old.clear(route, done.text("bundleHash"), first, canRemoveOsBond = true, files = files)
        assertFalse(r1.ok)
        assertEquals("overwriteAndDeleteFiles", r1.text("step"))
        assertEquals(listOf("c-small.db"), files.files.keys.toList(), "what was not reached is left")
        assertEquals("run:destroyEncryptionKey", events.first(), "the key goes before any file is touched")

        val writes = events.filter { it.startsWith("write:a-big.db:") }.map { it.split(":") }
        assertEquals(40_000, writes.sumOf { it[3].toInt() }, "every byte of the big file was written")
        assertTrue(writes.all { it[3].toInt() <= 16 * 1024 }, "in chunks")
        assertTrue(events.indexOf("sync:a-big.db") < events.indexOf("delete:a-big.db"), "synced before it is deleted")
        assertTrue(events.none { it.contains("b-empty.db") && !it.startsWith("delete:") }, "an empty file is only deleted")

        files.failOn = null
        val second = Recorder()
        val r2 = old.clear(InMemoryBackupRoute(), done.text("bundleHash"), second, canRemoveOsBond = true, files = files)
        assertTrue(r2.ok)
        assertTrue(files.files.isEmpty())
        assertEquals(listOf("forgetPairingRecord"), second.ran, "the key is not destroyed twice")
    }

    // @verifies SWR-MG-10
    // @verifies SWR-MG-11
    // @verifies SWR-MG-07
    @Test
    fun aMigrationRunsOverAnObjectStoreTheAppOnlyHasToProvide() = runTest {
        val store = MemoryObjectStore()
        val route = ObjectStoreRoute("cloud", atomicClaim = true)
        val old = Phone(seed = 1).also { it.fill() }
        val done = old.backup("migrate", BundleKeyMethod.PASSPHRASE, route, store = store)
        assertTrue(done.ok)
        assertEquals(1, store.objects.size, "the bundle is the only object after a backup")

        val fresh = Phone(seed = 50)
        val r = fresh.restore(route, store = store)
        assertTrue(r.ok)
        assertTrue(r.flag("confirmed"))
        assertEquals(old.kv.getString(carriedKeys.values.first().first()), fresh.kv.getString(carriedKeys.values.first().first()))

        val second = Phone(seed = 90).restore(route, store = store)
        assertEquals("claimed", second.reason)
        assertTrue(Phone().inspect(store = store).ok)

        // The person's explicit release deletes the claim object the core names (SWR-MG-07).
        store.delete(MigrationFlow.objectNames().text("claim"))
        assertEquals(2, store.objects.size, "only the claim is gone: the bundle and the confirmation stay")
        assertTrue(done.text("bundleHash").isNotEmpty())
    }

    // @verifies SWR-MG-10
    // @verifies SWR-MG-11
    // @verifies SWR-MG-07
    @Test
    fun aMigrationRunsOverADirectLinkInSmallPiecesAndASecondRestoreIsRefused() = runTest {
        val store = MemoryObjectStore()
        val route = ObjectStoreRoute("link", atomicClaim = true)
        val old = Phone(seed = 1).also { it.fill() }
        assertTrue(old.backup("migrate", BundleKeyMethod.PASSPHRASE, route, store = store).ok)

        suspend fun restoreOver(phone: Phone): JsonObject = coroutineScope {
            val (client, server) = pipe()
            val serving = async { MigrationFlow.serve(server, store, offer()) }
            val r = phone.restore(route, link = client)
            client.close()
            val s = serving.await()
            assertEquals(JsonPrimitive("closed"), s["reason"])
            assertEquals(JsonPrimitive(true), s["paired"])
            r
        }

        val fresh = Phone(seed = 50)
        val r = restoreOver(fresh)
        assertTrue(r.ok)
        assertTrue(r.flag("confirmed"))
        assertEquals(old.kv.getString(carriedKeys.values.first().first()), fresh.kv.getString(carriedKeys.values.first().first()))
        val names = MigrationFlow.objectNames()
        assertTrue(names.text("claim") in store.objects && names.text("confirmation") in store.objects, "the serving phone took the claim")
        assertEquals("claimed", restoreOver(Phone(seed = 90)).reason)
    }

    // @verifies SWR-MG-10
    @Test
    fun aServingPhoneWithNoValidOfferServesNothing() = runTest {
        val (_, server) = pipe()
        val r = MigrationFlow.serve(server, MemoryObjectStore(), PairingOffer("1234", epochSeconds() + 120, 3))
        assertFalse(r.ok)
        assertEquals("not_authenticated", r.reason)
    }

    // @verifies SWR-MG-10
    @Test
    fun aWrongCodeIsRefusedOnTheRestoringPhoneAndCostsTheOfferATry() = runTest {
        val store = MemoryObjectStore()
        val route = ObjectStoreRoute("link", atomicClaim = true)
        Phone(seed = 1).also { it.fill() }.backup("migrate", BundleKeyMethod.PASSPHRASE, route, store = store)
        val (client, server) = pipe()
        val first = offer()
        val serving = kotlinx.coroutines.coroutineScope {
            val s = async { MigrationFlow.serve(server, store, first) }
            val fresh = Phone(seed = 50)
            val r = fresh.restore(route, link = client, code = "87654321")
            client.close()
            assertEquals("wrong_code", r.reason)
            assertTrue(fresh.kv.getString(carriedKeys.values.first().first()) == null, "nothing was restored")
            s.await()
        }
        assertEquals(JsonPrimitive(false), serving["paired"])
        assertEquals(2, first.after(serving)!!.triesLeft)
        assertNull(first.after(JsonObject(mapOf("paired" to JsonPrimitive(true)))), "a paired offer is done")
        assertNull(first.copy(triesLeft = 1).after(JsonObject(mapOf("triesLeft" to JsonPrimitive(0)))), "no tries left")
        assertEquals(Pairing.newOffer(random = StepRandom(5)).triesLeft, 3)
    }

    // @verifies SWR-MG-10
    @Test
    fun theCodeIsMadeAndReadByTheCoreAndShownInTwoGroups() {
        val offer = Pairing.newOffer(random = StepRandom(5))
        assertEquals(8, offer.code.length)
        assertTrue(offer.code.all { it.isDigit() })
        assertEquals(offer.code, Pairing.parse(Pairing.display(offer.code)))
        assertEquals("1234 5678", Pairing.display("12345678"))
        assertNull(Pairing.parse("1234 567"))
        assertNull(Pairing.parse("abcd efgh"))
    }

    // @verifies SWR-MG-11
    @Test
    fun aLinkThatBreaksDuringARestoreEndsItWithTheRouteUnavailable() = runTest {
        val (client, _) = pipe()
        client.broken = true
        val r = Phone(seed = 50).restore(ObjectStoreRoute("link", atomicClaim = true), link = client)
        assertFalse(r.ok)
        assertEquals("link_failed", r.reason)
    }

    // @verifies SWR-MG-03
    @Test
    fun theCoreNamesTheLocaleKeyOfEveryReasonAndItem() {
        val keys = MigrationFlow.messageKeys()
        assertEquals("MIGRATION_REASON_CLAIMED", keys["reasons"]!!.jsonObject.text("claimed"))
        assertEquals("MIGRATION_SECRET_BAD_CHECKSUM", keys["secret"]!!.jsonObject.text("bad_checksum"))
        assertEquals("MIGRATION_PROBE_BACKUP_ONLY", keys["probe"]!!.jsonObject.text("backup_only"))
        assertEquals("MIGRATION_ITEM_SESSION_HISTORY", keys["items"]!!.jsonObject.text("sessionHistory"))
    }

    // @verifies SWR-MG-11
    @Test
    fun anObjectCallThatThrowsEndsTheFlowWithTheRouteUnavailable() = runTest {
        val store = MemoryObjectStore(failOn = "put")
        val r = Phone(seed = 1).also { it.fill() }.backup("migrate", BundleKeyMethod.PASSPHRASE, ObjectStoreRoute("cloud", atomicClaim = true), store = store)
        assertFalse(r.ok)
        assertEquals("route_unavailable", r.reason)
    }

    // @verifies SWR-MG-12
    @Test
    fun theProbeSaysWhatAnAccountCanDoAndLeavesNothingBehind() = runTest {
        val phone = Phone()
        val good = MemoryObjectStore().also { it.objects["users-file"] = byteArrayOf(1) }
        assertEquals("sufficient", phone.probe(good, 1_000).text("result"))
        assertEquals(listOf("users-file"), good.objects.keys.toList())

        val noConditional = phone.probe(MemoryObjectStore(conditional = false), 1_000)
        assertEquals("backup_only", noConditional.text("result"))
        assertEquals("createIfAbsentSecondRefused", noConditional.text("failedOperation"))

        val tooSmall = phone.probe(MemoryObjectStore(space = 10), 1_000)
        assertEquals("insufficient", tooSmall.text("result"))
        assertEquals("space", tooSmall.text("failedOperation"))
    }

    // @verifies SWR-MG-05
    @Test
    fun theKindAndHowTheKeyWasMadeAreReadFromTheRouteBeforeAnySecretIsAsked() = runTest {
        val route = InMemoryBackupRoute()
        val none = Phone().inspect(route)
        assertFalse(none.ok)
        assertEquals("no_bundle", none.reason)
        Phone(seed = 1).also { it.fill() }.backup("uhdr", BundleKeyMethod.RECOVERY_CODE, route, route)
        val found = Phone().inspect(route)
        assertTrue(found.ok)
        assertEquals("uhdr", found.text("kind"))
        assertEquals(BundleKeyMethod.RECOVERY_CODE.wire, found.text("method"))
        assertEquals("1", found.text("formatVersion"))
    }

    // @verifies SWR-MG-01
    @Test
    fun keysThatBelongToTheInstallNeverMoveAndNoOtherItemsKeysAreWritten() = runTest {
        val route = InMemoryBackupRoute()
        val old = Phone(seed = 1).also { it.fill() }
        listOf("np.research.device-token", "np.warranty.consent-granted", "ble.last-hub-id", "np.consumable.snooze-counts")
            .forEach { old.kv.putString(it, "belongs-to-this-phone") }
        old.backup("migrate", BundleKeyMethod.PASSPHRASE, route, route)
        val fresh = Phone(seed = 50)
        assertTrue(fresh.restore(route, route).ok)
        listOf("np.research.device-token", "np.warranty.consent-granted", "ble.last-hub-id", "np.consumable.snooze-counts")
            .forEach { assertNull(fresh.kv.getString(it), "$it must not move") }
        assertEquals("value-of-np.session.history", fresh.kv.getString("np.session.history"))
    }

    // ── The sessions around a direct link (SWR-MG-10) ───────────────────────────────────────────────────────────

    /** A transport whose phones find each other at once: `connect` hands the waiting `accept` the other end of a pipe. */
    private class Loopback : LinkTransport {
        private val waiting = Channel<PipeEnd>(Channel.UNLIMITED)
        override suspend fun accept(): Link? = waiting.receive()
        override suspend fun connect(): Link? {
            val (client, server) = pipe()
            waiting.send(server)
            return client
        }
    }

    private suspend fun filledLinkStore(): MemoryObjectStore {
        val store = MemoryObjectStore()
        val old = Phone(seed = 1).also { it.fill() }
        assertTrue(old.backup("migrate", BundleKeyMethod.PASSPHRASE, LINK_ROUTE, store = store).ok)
        return store
    }

    private suspend fun LinkSendSession.waitingCode(): LinkSendState.Waiting =
        step.first { it is LinkSendState.Waiting && it.offer.code.isNotEmpty() } as LinkSendState.Waiting

    // @verifies SWR-MG-10
    // @verifies SWR-MG-07
    @Test
    fun aMigrationRunsBetweenTwoSessionsThatPairByTheCodeShownAndTyped() = runTest {
        val transport = Loopback()
        val store = filledLinkStore()
        val sending = LinkSendSession(store, transport, StepRandom(11))
        val sent = async { sending.run() }
        val shown = sending.waitingCode().display
        val fresh = InMemoryKeyValueStore()
        val done = LinkReceiveSession(transport, fresh, random = StepRandom(60)).run(shown) { passphrase(SECRET) }
        assertTrue(done.ok, "$done")
        assertTrue(done.flag("confirmed"))
        assertIs<LinkSendState.Sent>(sent.await(), "the sending phone may now clear")
        val key = carriedKeys.values.first().first()
        assertEquals(Phone(seed = 1).also { it.fill() }.kv.getString(key), fresh.getString(key))
    }

    // @verifies SWR-MG-10
    @Test
    fun aWrongCodeCostsATryAndALastWrongOneSpendsTheCode() = runTest {
        val transport = Loopback()
        val sending = LinkSendSession(filledLinkStore(), transport, StepRandom(11))
        val sent = async { sending.run() }
        val real = sending.waitingCode().offer.code
        val wrong = if (real == "00000000") "11111111" else "00000000"
        for (left in listOf(2, 1)) {
            val fresh = InMemoryKeyValueStore()
            val r = LinkReceiveSession(transport, fresh, random = StepRandom(60)).run(wrong) { passphrase(SECRET) }
            assertEquals("wrong_code", r.reason)
            assertTrue(fresh.getString(carriedKeys.values.first().first()) == null, "nothing was restored")
            val w = sending.step.first { it is LinkSendState.Waiting && it.offer.triesLeft == left } as LinkSendState.Waiting
            assertEquals(real, w.offer.code, "a wrong guess does not change the code")
        }
        assertEquals("wrong_code", LinkReceiveSession(transport, InMemoryKeyValueStore(), random = StepRandom(60)).run(wrong) { null }.reason)
        val end = sent.await()
        assertIs<LinkSendState.Spent>(end)
        assertEquals("code_spent", end.reason)
    }

    // @verifies SWR-MG-10
    @Test
    fun aWrongSecretOrADeclinedOneLeavesNoClaimAndMakesAFreshCode() = runTest {
        val transport = Loopback()
        val store = filledLinkStore()
        val sending = LinkSendSession(store, transport, StepRandom(11))
        val sent = async { sending.run() }
        val first = sending.waitingCode().offer.code
        val bad = LinkReceiveSession(transport, InMemoryKeyValueStore(), random = StepRandom(60)).run(first) { passphrase("not the passphrase at all") }
        assertEquals("authentication_failed", bad.reason)
        val second = sending.step.first { it is LinkSendState.Waiting && it.offer.code != first && it.offer.triesLeft == 3 } as LinkSendState.Waiting
        val declined = LinkReceiveSession(transport, InMemoryKeyValueStore(), random = StepRandom(61)).run(second.offer.code) { null }
        assertEquals("cancelled", declined.reason)
        val third = sending.step.first { it is LinkSendState.Waiting && it.offer.code != second.offer.code } as LinkSendState.Waiting
        val ok = LinkReceiveSession(transport, InMemoryKeyValueStore(), random = StepRandom(62)).run(third.offer.code) { passphrase(SECRET) }
        assertTrue(ok.ok, "$ok")
        assertIs<LinkSendState.Sent>(sent.await())
    }

    // @verifies SWR-MG-10
    @Test
    fun aCodeThatIsNotEightDigitsOrFindsNoPhoneEndsAtOnce() = runTest {
        val none = object : LinkTransport {
            override suspend fun accept(): Link? = null
            override suspend fun connect(): Link? = null
        }
        val session = LinkReceiveSession(none, InMemoryKeyValueStore())
        assertEquals("wrong_code", session.run("1234 567") { null }.reason)
        assertEquals("link_failed", session.run("1234 5678") { null }.reason)
    }

    // @verifies SWR-MG-10
    @Test
    fun aCodeNobodyUsesExpires() = runTest {
        val sending = LinkSendSession(MemoryObjectStore(), Loopback(), StepRandom(11), clock = { 1_000_000 + testScheduler.currentTime / 1000 })
        val end = sending.run()
        assertIs<LinkSendState.Spent>(end)
        assertEquals("code_expired", end.reason)
    }
}
