package life.neurone.core.migration

import kotlinx.coroutines.test.runTest
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.npps.NppsCore
import life.neurone.core.data.BundleKeyMethod
import life.neurone.core.data.BundleKeyDerivation
import life.neurone.core.data.BundleSecret
import life.neurone.core.data.BundleSecretResult
import life.neurone.core.data.RandomSource
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

private fun passphrase(text: String): BundleSecret = (BundleKeyDerivation.passphraseSecret(text) as BundleSecretResult.Ok).secret

private const val SECRET = "a long enough passphrase"

/** The keys of the four items that are plain keys, from the core: the one list every platform shares. */
private val carriedKeys: Map<String, List<String>> by lazy {
    NppsCore.migration(buildJsonObject { put("op", "itemKeys") })["carried"]!!.jsonObject
        .mapValues { (_, v) -> v.jsonArray.map { it.jsonPrimitive.content } }
}

private class Phone(val kv: InMemoryKeyValueStore = InMemoryKeyValueStore(), extra: List<MigrationItemStore> = emptyList(), seed: Int = 0) {
    val service = MigrationService(kv, extra, StepRandom(seed))
    fun fill() {
        carriedKeys.values.flatten().forEach { kv.putString(it, "value-of-$it") }
    }
}

/** The migration flow over the real core (JNI) with an in-memory route. The rules are tested in Rust; this holds the order of the calls. */
class MigrationServiceTests {

    // @verifies SWR-MG-01
    // @verifies SWR-MG-02
    // @verifies SWR-MG-08
    @Test
    fun aMigrationMovesEveryCarriedItemAndTheRouteHoldsOnlyCiphertextAndTheBundleHash() = runTest {
        val route = InMemoryBackupRoute()
        val old = Phone(seed = 1).also { it.fill() }
        val done = old.service.backup("migrate", BundleKeyMethod.PASSPHRASE, passphrase(SECRET), route)
        assertIs<BackupResult.Done>(done)
        assertFalse(route.getBundle()!!.decodeToString().contains("value-of-np.session.history"), "plaintext must not be on the route")

        val fresh = Phone(seed = 50)
        val r = fresh.service.restore(route, passphrase(SECRET))
        assertIs<RestoreResult.Restored>(r)
        carriedKeys.values.flatten().forEach { assertEquals("value-of-$it", fresh.kv.getString(it), it) }
        assertTrue(r.confirmed)
        assertEquals(done.bundleHash, route.readConfirmation()!!["bundleHash"]!!.jsonPrimitive.content)
        assertEquals(setOf("bundleHash"), route.readConfirmation()!!.keys, "the confirmation carries the hash and nothing else")
        assertEquals(setOf("token", "bundleHash"), route.readClaim()!!.keys, "the claim carries a token and the hash and nothing else")
    }

    // @verifies SWR-MG-03
    // @verifies SWR-MG-07
    @Test
    fun theNightlyBackupLeavesOutProtocolsAndLimitsAndIsNotClaimed() = runTest {
        val route = InMemoryBackupRoute(atomicClaim = false)
        val old = Phone(seed = 1).also { it.fill() }
        val done = old.service.backup("uhdr", BundleKeyMethod.RECOVERY_CODE, passphrase(SECRET), route)
        assertIs<BackupResult.Done>(done)
        assertTrue("protocols" in done.omitted && "limitsProfiles" in done.omitted)
        assertEquals(setOf("sessionHistory", "characterisationEnrolment"), done.written.toSet())

        val fresh = Phone(seed = 50)
        val r = fresh.service.restore(route, passphrase(SECRET))
        assertIs<RestoreResult.Restored>(r)
        assertNull(fresh.kv.getString("np.user-protocols.npps"), "a UHDR-only restore brings no protocols")
        assertEquals("value-of-np.session.history", fresh.kv.getString("np.session.history"))
        assertTrue("protocols" in r.omitted)
        assertNull(route.readClaim(), "a UHDR-only bundle is not claimed")
        assertFalse(r.confirmed)
        assertTrue(r.askConsent && r.tellConsentNotRestored, "a UHDR-only restore never brings the consent record")
    }

    // @verifies SWR-MG-10
    @Test
    fun aRouteWithoutAnAtomicClaimCannotCarryAMigrationButCanCarryTheBackup() = runTest {
        val route = InMemoryBackupRoute(atomicClaim = false)
        val old = Phone(seed = 1).also { it.fill() }
        val m = old.service.backup("migrate", BundleKeyMethod.PASSPHRASE, passphrase(SECRET), route)
        assertIs<BackupResult.Refused>(m)
        assertEquals("route_not_offered", m.reason)
        assertNull(route.getBundle(), "nothing was written")
        assertIs<BackupResult.Done>(old.service.backup("uhdr", BundleKeyMethod.PASSPHRASE, passphrase(SECRET), route))
    }

    // @verifies SWR-MG-07
    @Test
    fun aWrongSecretOpensNothingAndDoesNotTakeTheClaim() = runTest {
        val route = InMemoryBackupRoute()
        Phone(seed = 1).also { it.fill() }.service.backup("migrate", BundleKeyMethod.PASSPHRASE, passphrase(SECRET), route)
        val fresh = Phone(seed = 50)
        val r = fresh.service.restore(route, passphrase("a different passphrase"))
        assertIs<RestoreResult.Refused>(r)
        assertEquals("authentication_failed", r.reason)
        assertNull(route.readClaim(), "a typing slip must not leave a claim only the person can release")
        assertNull(fresh.kv.getString("np.session.history"))
    }

    // @verifies SWR-MG-07
    @Test
    fun aSecondPhoneCannotRestoreABundleAlreadyClaimedUntilThePersonReleasesIt() = runTest {
        val route = InMemoryBackupRoute()
        Phone(seed = 1).also { it.fill() }.service.backup("migrate", BundleKeyMethod.PASSPHRASE, passphrase(SECRET), route)
        assertIs<RestoreResult.Restored>(Phone(seed = 50).service.restore(route, passphrase(SECRET)))

        val second = Phone(seed = 90)
        val r = second.service.restore(route, passphrase(SECRET))
        assertIs<RestoreResult.Refused>(r)
        assertEquals("claimed", r.reason)
        assertNull(second.kv.getString("np.session.history"))

        second.service.releaseClaim(route)
        assertNull(route.readClaim())
        assertIs<RestoreResult.Restored>(second.service.restore(route, passphrase(SECRET)))
    }

    // @verifies SWR-MG-08
    @Test
    fun aConfirmationThatDoesNotLandIsWrittenAgainAndAnUnconfirmedRestoreSaysSo() = runTest {
        val route = InMemoryBackupRoute()
        Phone(seed = 1).also { it.fill() }.service.backup("migrate", BundleKeyMethod.PASSPHRASE, passphrase(SECRET), route)
        route.dropConfirmationWrites = 2
        val r = Phone(seed = 50).service.restore(route, passphrase(SECRET))
        assertIs<RestoreResult.Restored>(r)
        assertTrue(r.confirmed, "the third write lands")

        val route2 = InMemoryBackupRoute()
        Phone(seed = 1).also { it.fill() }.service.backup("migrate", BundleKeyMethod.PASSPHRASE, passphrase(SECRET), route2)
        route2.dropConfirmationWrites = 3
        val r2 = Phone(seed = 50).service.restore(route2, passphrase(SECRET))
        assertIs<RestoreResult.Restored>(r2)
        assertFalse(r2.confirmed)
        assertNull(route2.readConfirmation(), "no confirmation, so the old phone clears nothing")
    }

    // @verifies SWR-MG-04
    @Test
    fun aRestoreNeverResetsAWithdrawalAndAnItemWithNowhereToGoIsReportedNotApplied() = runTest {
        val route = InMemoryBackupRoute()
        val oldWithdrawal = MapItemStore("withdrawalState", buildJsonObject {
            put("blanketConsentWithdrawnAt", "2026-03-01T00:00:00Z")
            put("studyWithdrawals", JsonArray(listOf(JsonPrimitive("study-a"))))
            put("categoryWithdrawals", JsonArray(emptyList()))
        })
        val oldGrants = MapItemStore("consentGrants", buildJsonObject { put("grants", "g") })
        val old = Phone(extra = listOf(oldWithdrawal, oldGrants), seed = 1).also { it.fill() }
        old.service.backup("migrate", BundleKeyMethod.PASSPHRASE, passphrase(SECRET), route)

        // The new phone already holds a LATER withdrawal and another study's: the union stands, and the later time.
        val newWithdrawal = MapItemStore("withdrawalState", buildJsonObject {
            put("blanketConsentWithdrawnAt", "2026-06-01T00:00:00Z")
            put("studyWithdrawals", JsonArray(listOf(JsonPrimitive("study-b"))))
            put("categoryWithdrawals", JsonArray(emptyList()))
        })
        val fresh = Phone(extra = listOf(newWithdrawal), seed = 50)
        val r = fresh.service.restore(route, passphrase(SECRET))
        assertIs<RestoreResult.Restored>(r)
        val merged = newWithdrawal.value!!.jsonObject
        assertEquals("2026-06-01T00:00:00Z", merged["blanketConsentWithdrawnAt"]!!.jsonPrimitive.content)
        assertEquals(setOf("study-a", "study-b"), merged["studyWithdrawals"]!!.jsonArray.map { it.jsonPrimitive.content }.toSet())
        // This phone has no store for consentGrants: the bundle held them, and the report says they were not applied.
        assertEquals(listOf("consentGrants"), r.notApplied)
        assertTrue(r.askConsent && r.tellConsentNotRestored)
    }

    // @verifies SWR-MG-08
    // @verifies SWR-MG-06
    // @verifies SWR-MG-09
    @Test
    fun theOldPhoneClearsOnlyAfterTheMatchingConfirmationAndKeyFirstAndPairingIncluded() = runTest {
        val route = InMemoryBackupRoute()
        val old = Phone(seed = 1).also { it.fill() }
        val done = old.service.backup("migrate", BundleKeyMethod.PASSPHRASE, passphrase(SECRET), route) as BackupResult.Done
        val actions = Recorder()

        val before = old.service.clearOldPhone(route, done.bundleHash, actions, canRemoveOsBond = false)
        assertIs<ClearResult.Waiting>(before)
        assertEquals("no_confirmation", before.reason)
        assertTrue(actions.ran.isEmpty(), "nothing is cleared without a confirmation")

        Phone(seed = 50).service.restore(route, passphrase(SECRET))
        val other = old.service.clearOldPhone(route, "not-this-bundle", actions, canRemoveOsBond = false)
        assertEquals("other_bundle", (other as ClearResult.Waiting).reason)

        val after = old.service.clearOldPhone(route, done.bundleHash, actions, canRemoveOsBond = false)
        assertIs<ClearResult.Cleared>(after)
        assertTrue(after.removeOsBond, "the app could not remove the OS bond, so the person is told to")
        assertEquals("destroyEncryptionKey", actions.ran.first(), "the key goes first, so what flash still holds is unreadable")
        assertEquals(listOf("destroyEncryptionKey", "overwriteAndDeleteFiles", "forgetPairingRecord"), actions.ran, "the app does only its own steps")
        assertEquals("true", old.kv.getString("np.migration.cleared"), "the proof that the clear ran stays")
        assertNull(old.kv.getString("np.migration.clear-progress"), "the progress key goes when the plan is complete")
        assertNull(old.kv.getString("ble.last-hub-id"), "the core forgets the last paired helmet id itself")
    }

    // @verifies SWR-MG-08
    @Test
    fun aClearThatStopsPartWayResumesFromTheStepItReachedWithoutAskingAgain() = runTest {
        val route = InMemoryBackupRoute()
        val old = Phone(seed = 1).also { it.fill() }
        val done = old.service.backup("migrate", BundleKeyMethod.PASSPHRASE, passphrase(SECRET), route) as BackupResult.Done
        Phone(seed = 50).service.restore(route, passphrase(SECRET))

        val first = Recorder(failOn = "forgetPairingRecord")
        val r1 = old.service.clearOldPhone(route, done.bundleHash, first, canRemoveOsBond = true)
        assertEquals("forgetPairingRecord", (r1 as ClearResult.Failed).step)
        assertEquals(listOf("destroyEncryptionKey", "overwriteAndDeleteFiles"), first.ran)

        // Next launch: the route is unreachable (no confirmation to read) but the clear is already under way.
        val gone = InMemoryBackupRoute()
        val second = Recorder()
        val r2 = old.service.clearOldPhone(gone, done.bundleHash, second, canRemoveOsBond = true)
        assertIs<ClearResult.Cleared>(r2)
        assertFalse(r2.removeOsBond)
        assertEquals("forgetPairingRecord", second.ran.first(), "steps already done are not run again")
        assertFalse("destroyEncryptionKey" in second.ran)
    }

    // @verifies SWR-MG-10
    // @verifies SWR-MG-11
    // @verifies SWR-MG-07
    @Test
    fun aMigrationRunsOverAnObjectStoreTheAppOnlyHasToProvide() = runTest {
        val store = MemoryObjectStore()
        val route = ObjectStoreRoute(store, "cloud", atomicClaim = true)
        val old = Phone(seed = 1).also { it.fill() }
        val done = old.service.backup("migrate", BundleKeyMethod.PASSPHRASE, passphrase(SECRET), route) as BackupResult.Done
        assertEquals(1, store.objects.size, "the bundle is the only object after a backup")

        val fresh = Phone(seed = 50)
        val r = fresh.service.restore(route, passphrase(SECRET))
        assertIs<RestoreResult.Restored>(r)
        assertTrue(r.confirmed)
        assertEquals(old.kv.getString(carriedKeys.values.first().first()), fresh.kv.getString(carriedKeys.values.first().first()))

        val second = Phone(seed = 90).service.restore(route, passphrase(SECRET))
        assertEquals("claimed", (second as RestoreResult.Refused).reason)
        assertIs<InspectResult.Found>(Phone().service.inspect(route))

        // The person's explicit release deletes the claim object the core names (SWR-MG-07).
        old.service.releaseClaim(route)
        assertEquals(2, store.objects.size, "only the claim is gone: the bundle and the confirmation stay")
        assertTrue(done.bundleHash.isNotEmpty())
    }

    // @verifies SWR-MG-11
    @Test
    fun anObjectCallThatThrowsEndsTheFlowWithTheRouteUnavailable() = runTest {
        val store = MemoryObjectStore(failOn = "put")
        val r = Phone(seed = 1).also { it.fill() }.service
            .backup("migrate", BundleKeyMethod.PASSPHRASE, passphrase(SECRET), ObjectStoreRoute(store, "cloud", atomicClaim = true))
        assertEquals("route_unavailable", (r as BackupResult.Refused).reason)
    }

    // @verifies SWR-MG-12
    @Test
    fun theProbeSaysWhatAnAccountCanDoAndLeavesNothingBehind() = runTest {
        val service = Phone().service
        val good = MemoryObjectStore().also { it.objects["users-file"] = byteArrayOf(1) }
        assertEquals("sufficient", service.probe(good, 1_000).result)
        assertEquals(listOf("users-file"), good.objects.keys.toList())

        val noConditional = service.probe(MemoryObjectStore(conditional = false), 1_000)
        assertEquals("backup_only", noConditional.result)
        assertEquals("createIfAbsentSecondRefused", noConditional.failedOperation)

        val tooSmall = service.probe(MemoryObjectStore(space = 10), 1_000)
        assertEquals("insufficient", tooSmall.result)
        assertEquals("space", tooSmall.failedOperation)
    }

    // @verifies SWR-MG-05
    @Test
    fun theKindAndHowTheKeyWasMadeAreReadFromTheRouteBeforeAnySecretIsAsked() = runTest {
        val route = InMemoryBackupRoute()
        assertEquals("no_bundle", (Phone().service.inspect(route) as InspectResult.Refused).reason)
        Phone(seed = 1).also { it.fill() }.service.backup("uhdr", BundleKeyMethod.RECOVERY_CODE, passphrase(SECRET), route)
        val found = Phone().service.inspect(route)
        assertIs<InspectResult.Found>(found)
        assertEquals("uhdr", found.kind)
        assertEquals(BundleKeyMethod.RECOVERY_CODE, found.method)
        assertEquals(1, found.formatVersion)
    }

    // @verifies SWR-MG-01
    @Test
    fun keysThatBelongToTheInstallNeverMoveAndNoOtherItemsKeysAreWritten() = runTest {
        val route = InMemoryBackupRoute()
        val old = Phone(seed = 1).also { it.fill() }
        listOf("np.research.device-token", "np.warranty.consent-granted", "ble.last-hub-id", "np.consumable.snooze-counts")
            .forEach { old.kv.putString(it, "belongs-to-this-phone") }
        old.service.backup("migrate", BundleKeyMethod.PASSPHRASE, passphrase(SECRET), route)
        val fresh = Phone(seed = 50)
        assertIs<RestoreResult.Restored>(fresh.service.restore(route, passphrase(SECRET)))
        listOf("np.research.device-token", "np.warranty.consent-granted", "ble.last-hub-id", "np.consumable.snooze-counts")
            .forEach { assertNull(fresh.kv.getString(it), "$it must not move") }
        assertEquals("value-of-np.session.history", fresh.kv.getString("np.session.history"))
    }
}
