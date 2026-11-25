package life.neurone.shared

import kotlinx.coroutines.test.runTest
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.common.EncryptedKeyValueStore
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.data.BundleKeyDerivation
import life.neurone.core.data.BundleKeyMethod
import life.neurone.core.data.SystemRandomSource
import life.neurone.core.migration.ObjectStoreRoute
import life.neurone.core.npps.NppsCore
import life.neurone.shared.migration.BackupKind
import life.neurone.shared.migration.BackupStep
import life.neurone.shared.migration.MemoryObjectStore
import life.neurone.shared.migration.MigrationBackupModel
import life.neurone.shared.migration.ScheduledBackup
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertIs
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The automatic health-data backup runs when it is on and due, from the code kept on the phone, and never otherwise (SWR-MG-03). */
class ScheduledBackupTest {

    private val route = ObjectStoreRoute("test", atomicClaim = true)

    private fun carriedKeys(): List<String> =
        NppsCore.migration(buildJsonObject { put("op", "itemKeys") })["carried"]!!.jsonObject.values
            .flatMap { v -> v.jsonArray.map { k -> k.jsonPrimitive.content } }

    private fun filled() = InMemoryKeyValueStore().also { kv -> carriedKeys().forEach { kv.putString(it, "value-of-$it") } }

    private class Fixture(val kv: InMemoryKeyValueStore, val store: MemoryObjectStore, var now: Long, val backup: ScheduledBackup)

    private fun fixture(placeOn: Boolean = true, sealed: Boolean = true, kv: InMemoryKeyValueStore = filled()): Fixture {
        val store = MemoryObjectStore()
        lateinit var f: Fixture
        val backup = ScheduledBackup(
            { if (placeOn) MigrationPlace("test", store, route) else null }, kv, nowSeconds = { f.now }, secretKeptSealed = { sealed },
        )
        f = Fixture(kv, store, 1_000_000L, backup)
        return f
    }

    private fun newSecret() = BundleKeyDerivation.newRecoveryCode(SystemRandomSource).secret

    // @verifies SWR-MG-03
    @Test fun it_does_nothing_until_the_person_turns_it_on() = runTest {
        val f = fixture()
        assertFalse(f.backup.isOn)
        assertEquals(ScheduledBackup.Outcome.Off, f.backup.runIfDue())
        assertTrue(f.store.list("").isEmpty())
    }

    // @verifies SWR-MG-03
    @Test fun it_waits_twenty_hours_after_a_backup_then_makes_one_and_waits_again() = runTest {
        val f = fixture()
        f.backup.turnOn(BundleKeyMethod.RECOVERY_CODE, newSecret())
        f.backup.noteMade()  // the backup that turned it on
        f.now += 3600
        assertEquals(ScheduledBackup.Outcome.NotDue, f.backup.runIfDue())
        assertTrue(f.store.list("").isEmpty())

        f.now += ScheduledBackup.INTERVAL_SECONDS
        val made = f.backup.runIfDue()
        assertIs<ScheduledBackup.Outcome.Made>(made)
        assertTrue(f.store.list("").isNotEmpty(), "a bundle was written")
        assertEquals(f.now, f.backup.lastBackupSeconds)

        f.now += 60
        assertEquals(ScheduledBackup.Outcome.NotDue, f.backup.runIfDue())
    }

    // @verifies SWR-MG-03
    @Test fun the_kept_code_opens_what_it_made_and_a_second_run_replaces_the_bundle() = runTest {
        val f = fixture()
        val code = BundleKeyDerivation.newRecoveryCode(SystemRandomSource)
        f.backup.turnOn(BundleKeyMethod.RECOVERY_CODE, code.secret)
        assertIs<ScheduledBackup.Outcome.Made>(f.backup.runIfDue())
        val first = f.store.list("").single()
        f.now += ScheduledBackup.INTERVAL_SECONDS
        assertIs<ScheduledBackup.Outcome.Made>(f.backup.runIfDue())
        assertEquals(listOf(first), f.store.list(""), "the folder still holds one bundle, not a growing pile")
    }

    // @verifies SWR-MG-03
    @Test fun without_a_folder_it_makes_nothing_and_says_so() = runTest {
        val f = fixture(placeOn = false)
        f.backup.turnOn(BundleKeyMethod.RECOVERY_CODE, newSecret())
        assertEquals(ScheduledBackup.Outcome.NoPlace, f.backup.runIfDue())
        assertNull(f.backup.lastBackupSeconds)
    }

    // @verifies SWR-MG-03
    @Test fun turning_it_off_destroys_the_kept_code_and_stops_it() = runTest {
        val f = fixture()
        f.backup.turnOn(BundleKeyMethod.PASSPHRASE, newSecret())
        assertNotNull(f.kv.getString(ScheduledBackup.SECRET_KEY))
        f.backup.turnOff()
        assertNull(f.kv.getString(ScheduledBackup.SECRET_KEY))
        assertFalse(f.backup.isOn)
        assertEquals(ScheduledBackup.Outcome.Off, f.backup.runIfDue())
    }

    // @verifies SWR-MG-03
    @Test fun a_kept_code_that_cannot_be_read_is_a_recorded_failure_not_a_crash() = runTest {
        val f = fixture()
        f.kv.putString(ScheduledBackup.SECRET_KEY, "recoveryCode:not-hex")
        val out = f.backup.runIfDue()
        assertEquals(ScheduledBackup.Outcome.Failed("secret_unreadable"), out)
        assertEquals("secret_unreadable", f.backup.lastFailure)
        assertTrue(f.store.list("").isEmpty())
    }

    // @verifies SWR-MG-03
    @Test fun the_backup_screen_model_turns_it_on_only_when_asked_and_only_for_a_health_data_backup() = runTest {
        // Asked, health-data backup: on, with the 20 hours started.
        run {
            val f = fixture()
            val m = MigrationBackupModel(MigrationPlace("test", f.store, route), f.kv, scheduled = f.backup)
            m.keepAutomatic = true
            m.chooseKind(BackupKind.UHDR); m.continueToMethod(); m.useRecoveryCode(); m.confirmCodeSaved()
            assertIs<BackupStep.Done>(m.step.value)
            assertTrue(f.backup.isOn)
            assertEquals(f.now, f.backup.lastBackupSeconds)
        }
        // Not asked: a backup is made and nothing is kept.
        run {
            val f = fixture()
            val m = MigrationBackupModel(MigrationPlace("test", f.store, route), f.kv, scheduled = f.backup)
            m.chooseKind(BackupKind.UHDR); m.continueToMethod(); m.useRecoveryCode(); m.confirmCodeSaved()
            assertIs<BackupStep.Done>(m.step.value)
            assertFalse(f.backup.isOn)
        }
        // Asked, but a migration: never kept.
        run {
            val f = fixture()
            val m = MigrationBackupModel(MigrationPlace("test", f.store, route), f.kv, scheduled = f.backup)
            m.keepAutomatic = true
            m.chooseKind(BackupKind.MIGRATE); m.continueToMethod(); m.useRecoveryCode(); m.confirmCodeSaved()
            assertIs<BackupStep.Done>(m.step.value)
            assertFalse(f.backup.isOn)
        }
    }

    // @verifies SWR-MG-13#e
    @Test fun a_run_that_does_not_keep_the_secret_sealed_refuses_to_turn_it_on_and_writes_nothing() = runTest {
        val f = fixture(sealed = false)
        assertFalse(f.backup.isAvailable)
        assertFalse(f.backup.turnOn(BundleKeyMethod.PASSPHRASE, newSecret()), "the answer says it was not turned on")
        assertNull(f.kv.getString(ScheduledBackup.SECRET_KEY), "no code or passphrase lands in the plain store")
        assertFalse(f.backup.isOn)
        assertEquals(ScheduledBackup.Outcome.Off, f.backup.runIfDue())
        assertTrue(f.store.list("").isEmpty())
    }

    // @verifies SWR-MG-13#e
    @Test fun a_secret_an_earlier_run_left_in_the_clear_is_removed_and_a_sealed_one_is_left_for_a_run_that_can_open_it() = runTest {
        val plain = filled().also { it.putString(ScheduledBackup.SECRET_KEY, "recoveryCode:00ff") }
        val f = fixture(sealed = false, kv = plain)
        assertNull(plain.getString(ScheduledBackup.SECRET_KEY), "the key no longer sits beside the backups it opens")
        assertFalse(f.backup.isOn)

        val sealedValue = EncryptedKeyValueStore.PREFIX + "opaque"
        val kept = filled().also { it.putString(ScheduledBackup.SECRET_KEY, sealedValue) }
        fixture(sealed = false, kv = kept)
        assertEquals(sealedValue, kept.getString(ScheduledBackup.SECRET_KEY), "a value sealed by an earlier run is not this run's to erase")

        val sealedRun = filled().also { it.putString(ScheduledBackup.SECRET_KEY, "recoveryCode:00ff") }
        assertTrue(fixture(sealed = true, kv = sealedRun).backup.isOn, "a sealed run keeps what it holds")
    }

    // @verifies SWR-MG-13#e
    @Test fun the_backup_screen_model_still_makes_the_backup_on_an_unsealed_run_and_keeps_nothing() = runTest {
        val f = fixture(sealed = false)
        val m = MigrationBackupModel(MigrationPlace("test", f.store, route), f.kv, scheduled = f.backup)
        assertFalse(m.automaticAvailable, "the screen is told not to offer it")
        m.keepAutomatic = true
        m.chooseKind(BackupKind.UHDR); m.continueToMethod(); m.useRecoveryCode(); m.confirmCodeSaved()
        assertIs<BackupStep.Done>(m.step.value)
        assertTrue(f.store.list("").isNotEmpty(), "the backup is made under the code the person holds")
        assertFalse(f.backup.isOn)
        assertNull(f.kv.getString(ScheduledBackup.SECRET_KEY))
        assertEquals(f.now, f.backup.lastBackupSeconds, "the backup is still recorded")
    }
}
