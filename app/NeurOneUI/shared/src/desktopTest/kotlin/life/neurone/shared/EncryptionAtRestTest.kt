package life.neurone.shared

import androidx.compose.runtime.Composable
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.test.runTest
import life.neurone.core.common.EncryptedKeyValueStore
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.common.InMemorySecretKeyStore
import life.neurone.core.common.KeyValueStore
import life.neurone.core.common.SecretKeyStore
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.shared.ble.UnavailableBleCentral
import life.neurone.shared.migration.BackupKind
import life.neurone.shared.migration.BackupStep
import life.neurone.shared.migration.ClearStep
import life.neurone.shared.migration.MigrationBackupModel
import life.neurone.shared.migration.MigrationClearModel
import life.neurone.shared.migration.MigrationRestoreModel
import life.neurone.shared.migration.MemoryObjectStore
import life.neurone.shared.migration.RestoreStep
import life.neurone.core.migration.ObjectStoreRoute
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The app's data sealed at rest under a key the host keeps, and the clear that destroys the key (SWR-MG-06, SWR-MG-09). */
class EncryptionAtRestTest {
    private fun services(kv: KeyValueStore, secrets: SecretKeyStore?, place: MigrationPlace? = null) = AppServices(
        platform = object : PlatformServices {
            override val keyValueStore = kv
            override val bleCentral = UnavailableBleCentral()
            override val protocolSigner = Ed25519ProtocolSigner()
            override val analyticsBackend = NoOpAnalyticsBackend()
            override val secretKeyStore = secrets
            override val migrationPlace = place
            @Composable
            override fun rememberBleConnectAction(): () -> Unit = {}
        },
        scope = CoroutineScope(SupervisorJob() + Dispatchers.Unconfined),
    )

    // @verifies SWR-MG-06
    @Test fun a_host_with_a_keystore_has_its_uhdr_class_data_sealed_and_a_clear_it_can_do() {
        val raw = InMemoryKeyValueStore()
        val secrets = InMemorySecretKeyStore()
        val app = services(raw, secrets)
        app.keyValueStore.putString("np.session.history", "history of the person")
        assertTrue(raw.getString("np.session.history")!!.startsWith(EncryptedKeyValueStore.PREFIX), "the data is sealed in the host's store")
        assertEquals("history of the person", app.keyValueStore.getString("np.session.history"))
        assertNotNull(app.clearActions)
        // A key that is not user health data is left as it is.
        app.keyValueStore.putString("ble.last-hub-id", "hub-1")
        assertEquals("hub-1", raw.getString("ble.last-hub-id"))
    }

    // @verifies SWR-MG-06
    @Test fun a_host_with_no_keystore_keeps_its_data_as_before_and_cannot_clear_itself() {
        val raw = InMemoryKeyValueStore()
        val app = services(raw, null)
        app.keyValueStore.putString("np.session.history", "history of the person")
        assertEquals("history of the person", raw.getString("np.session.history"))
        assertNull(app.clearActions)
    }

    // @verifies SWR-MG-06
    @Test fun data_already_on_the_phone_is_sealed_when_the_app_starts() {
        val raw = InMemoryKeyValueStore().also { it.putString("np.session.history", "written before") }
        val app = services(raw, InMemorySecretKeyStore())
        assertTrue(raw.getString("np.session.history")!!.startsWith(EncryptedKeyValueStore.PREFIX))
        assertEquals("written before", app.keyValueStore.getString("np.session.history"))
    }

    // @verifies SWR-MG-06
    // @verifies SWR-MG-08
    // @verifies SWR-MG-09
    @Test fun the_clear_destroys_the_key_first_and_the_data_cannot_be_opened_with_any_key_the_app_holds() = runTest {
        val raw = InMemoryKeyValueStore()
        val secrets = InMemorySecretKeyStore()
        val old = services(raw, secrets, place = null)
        val store = MemoryObjectStore()
        val place = MigrationPlace("here", store, ObjectStoreRoute("test", atomicClaim = true))

        old.keyValueStore.putString("np.session.history", "history of the person")
        old.keyValueStore.putString("ble.last-hub-id", "hub-1")
        val backup = MigrationBackupModel(place, old.keyValueStore)
        backup.chooseKind(BackupKind.MIGRATE)
        backup.continueToMethod()
        backup.usePassphrase()
        backup.submitPassphrase("a long enough passphrase")
        assertTrue(backup.step.value is BackupStep.Done, backup.step.value.toString())

        val fresh = InMemoryKeyValueStore()
        val restore = MigrationRestoreModel(place, fresh)
        restore.look()
        restore.submitSecret("a long enough passphrase")
        assertTrue((restore.step.value as RestoreStep.Done).report.confirmed)

        assertNotNull(secrets.load())
        val clear = MigrationClearModel(place, old.keyValueStore, old.clearActions)
        clear.look()
        clear.clear()
        assertTrue(clear.step.value is ClearStep.Done, clear.step.value.toString())

        assertNull(secrets.load(), "the key is destroyed in the keystore")
        assertNull(raw.getString("np.session.history"), "and the data is gone from the host's store")
        assertNull(raw.getString("ble.last-hub-id"))
        assertFalse(old.keyValueStore.getString("np.session.history") != null)
    }
}
