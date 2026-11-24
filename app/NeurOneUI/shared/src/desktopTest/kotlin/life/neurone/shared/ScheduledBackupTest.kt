package life.neurone.shared

import kotlinx.coroutines.test.runTest
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.common.EncryptedKeyValueStore
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.common.InMemorySecretKeyStore
import life.neurone.core.data.BundleSecretResult
import life.neurone.core.data.BundleKeyDerivation
import life.neurone.core.data.BundleKeyMethod
import life.neurone.core.data.SystemRandomSource
import life.neurone.core.migration.ObjectStoreRoute
import life.neurone.core.npps.NppsCore
import life.neurone.shared.migration.BackupKind
import life.neurone.shared.migration.BackupStep
import life.neurone.shared.migration.MemoryObjectStore
import life.neurone.shared.migration.MigrationBackupModel
import life.neurone.shared.migration.MigrationRestoreModel
import life.neurone.shared.migration.RestoreStep
import life.neurone.shared.migration.ScheduledBackup
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertIs
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The automatic health-data backup runs when it is on and due, from the key kept on the phone in place of the code (SWR-MG-03, SWR-MG-32 (i)), and never otherwise. */
class ScheduledBackupTest {

    private val route = ObjectStoreRoute("test", atomicClaim = true)

    private fun carriedKeys(): List<String> =
        NppsCore.migration(buildJsonObject { put("op", "itemKeys") })["carried"]!!.jsonObject.values
            .flatMap { v -> v.jsonArray.map { k -> k.jsonPrimitive.content } }

    private fun filled() = InMemoryKeyValueStore().also { kv -> carriedKeys().forEach { kv.putString(it, "value-of-$it") } }

    private class Fixture(val kv: InMemoryKeyValueStore, val store: MemoryObjectStore, var now: Long, val backup: ScheduledBackup)

    /** The data key the kept backup key is sealed under: the sealed tier's, as `AppServices` passes it (`OI-PRIV-65` (2)). */
    private val dataKey = InMemorySecretKeyStore()

    private fun fixture(placeOn: Boolean = true, sealed: Boolean = true): Fixture {
        val kv = filled()
        val store = MemoryObjectStore()
        lateinit var f: Fixture
        val sealer = if (sealed) EncryptedKeyValueStore(kv, dataKey, emptySet()) else null
        val backup = ScheduledBackup({ if (placeOn) MigrationPlace("test", store, route) else null }, kv, nowSeconds = { f.now }, sealer = sealer)
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
        f.kv.putString(ScheduledBackup.SECRET_KEY, "{\"v\":2,\"method\":\"recoveryCode\",\"kdf\":{},\"key\":\"00\"}")
        val out = f.backup.runIfDue()
        assertEquals(ScheduledBackup.Outcome.Failed("secret_unreadable"), out)
        assertEquals("secret_unreadable", f.backup.lastFailure)
        assertTrue(f.store.list("").isEmpty())
    }

    // @verifies SWR-MG-32#i
    @Test fun the_earlier_form_that_kept_the_secret_itself_is_destroyed_unread() = runTest {
        val f = fixture()
        val passphraseHex = "a long enough passphrase".encodeToByteArray().joinToString("") { (it.toInt() and 0xff).toString(16).padStart(2, '0') }
        f.kv.putString(ScheduledBackup.SECRET_KEY, "passphrase:$passphraseHex")
        assertEquals(ScheduledBackup.Outcome.Failed("secret_unreadable"), f.backup.runIfDue())
        assertNull(f.kv.getString(ScheduledBackup.SECRET_KEY), "the stored passphrase is gone")
        assertFalse(f.backup.isOn, "the person turns it on again with their next backup")
        assertTrue(f.store.list("").isEmpty(), "nothing was made from it")
    }

    // @verifies SWR-MG-32#i
    @Test fun a_passphrase_is_never_kept_and_what_is_kept_makes_bundles_the_passphrase_alone_restores() = runTest {
        val passphrase = "a long enough passphrase"
        val f = fixture()
        val secret = (BundleKeyDerivation.passphraseSecret(passphrase) as BundleSecretResult.Ok).secret
        assertTrue(f.backup.turnOn(BundleKeyMethod.PASSPHRASE, secret))
        secret.destroy()
        val kept = f.kv.getString(ScheduledBackup.SECRET_KEY)!!
        val hex = passphrase.encodeToByteArray().joinToString("") { (it.toInt() and 0xff).toString(16).padStart(2, '0') }
        assertFalse(kept.contains(passphrase) || kept.contains(hex), "neither the passphrase nor its hex is kept")

        // Two runs: each bundle is sealed under the one kept salt, and each opens with the passphrase on a phone that holds nothing.
        repeat(2) {
            assertIs<ScheduledBackup.Outcome.Made>(f.backup.runIfDue())
            val restore = MigrationRestoreModel(MigrationPlace("test", f.store, route), InMemoryKeyValueStore())
            restore.look()
            restore.submitSecret(passphrase)
            assertIs<RestoreStep.Done>(restore.step.value)
            f.now += ScheduledBackup.INTERVAL_SECONDS
        }
    }

    // @verifies SWR-MG-32#i
    @Test fun a_run_that_cannot_seal_keeps_nothing_and_a_destroyed_data_key_stops_the_backup() = runTest {
        val unsealed = fixture(sealed = false)
        assertFalse(unsealed.backup.turnOn(BundleKeyMethod.RECOVERY_CODE, newSecret()))
        assertNull(unsealed.kv.getString(ScheduledBackup.SECRET_KEY), "nothing is stored where it cannot be sealed")
        assertFalse(unsealed.backup.isOn)

        val f = fixture()
        assertTrue(f.backup.turnOn(BundleKeyMethod.RECOVERY_CODE, newSecret()))
        // A later run whose keystore no longer has that data key (it was destroyed and a new one made): the kept key does not open.
        val later = ScheduledBackup({ MigrationPlace("test", f.store, route) }, f.kv, nowSeconds = { f.now },
            sealer = EncryptedKeyValueStore(f.kv, InMemorySecretKeyStore(), emptySet()))
        assertEquals(ScheduledBackup.Outcome.Failed("secret_unreadable"), later.runIfDue())
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
        // Asked, on a run whose storage is not sealed: the backup is made, and nothing is kept, so it stays off.
        run {
            val f = fixture(sealed = false)
            val m = MigrationBackupModel(MigrationPlace("test", f.store, route), f.kv, scheduled = f.backup)
            m.keepAutomatic = true
            m.chooseKind(BackupKind.UHDR); m.continueToMethod(); m.useRecoveryCode(); m.confirmCodeSaved()
            assertIs<BackupStep.Done>(m.step.value)
            assertFalse(f.backup.isOn)
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
}
