package life.neurone.shared

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.test.runTest
import life.neurone.core.common.EncryptedKeyValueStore
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.common.KeyValueStore
import life.neurone.core.data.BundleKeyDerivation
import life.neurone.core.data.BundleKeyMethod
import life.neurone.core.data.SystemRandomSource
import life.neurone.core.migration.ObjectStoreRoute
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.shared.ble.UnavailableBleCentral
import life.neurone.shared.migration.BackgroundBackupScheduler
import life.neurone.shared.migration.MemoryObjectStore
import life.neurone.shared.migration.ScheduledBackup
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertIs
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The system's wake-up request follows the automatic backup, and a woken app makes the backup only when it can open its data (SWR-MG-03). */
class BackgroundBackupTest {

    private class Recorder : BackgroundBackupScheduler {
        val calls = mutableListOf<String>()
        override fun request(afterSeconds: Long) { calls += "request:$afterSeconds" }
        override fun cancel() { calls += "cancel" }
    }

    private val store = MemoryObjectStore()
    private val recorder = Recorder()

    private fun services(kv: KeyValueStore, placeOn: Boolean = true) = AppServices(
        platform = object : PlatformServices {
            override val keyValueStore = kv
            override val bleCentral = UnavailableBleCentral()
            override val protocolSigner = Ed25519ProtocolSigner()
            override val analyticsBackend = NoOpAnalyticsBackend()
            override val backgroundBackup = recorder
            override val migrationPlace = if (placeOn) MigrationPlace("test", store, ObjectStoreRoute("test", atomicClaim = true)) else null
            @androidx.compose.runtime.Composable
            override fun rememberBleConnectAction(): () -> Unit = {}
        },
        scope = CoroutineScope(SupervisorJob() + Dispatchers.Unconfined),
    )

    private fun secret() = BundleKeyDerivation.newRecoveryCode(SystemRandomSource).secret

    // @verifies SWR-MG-03
    @Test fun turning_the_backup_on_asks_the_system_and_turning_it_off_withdraws_the_request() {
        val app = services(InMemoryKeyValueStore())
        assertEquals(listOf("cancel"), recorder.calls, "off at launch: nothing is asked for")
        app.scheduledBackup.turnOn(BundleKeyMethod.RECOVERY_CODE, secret())
        assertEquals("request:${ScheduledBackup.INTERVAL_SECONDS}", recorder.calls.last())
        app.scheduledBackup.turnOff()
        assertEquals("cancel", recorder.calls.last())
    }

    // @verifies SWR-MG-03
    @Test fun a_launch_with_the_backup_already_on_asks_the_system_again() {
        val kv = InMemoryKeyValueStore()
        services(kv).scheduledBackup.turnOn(BundleKeyMethod.RECOVERY_CODE, secret())
        recorder.calls.clear()
        services(kv)
        assertEquals(listOf("request:${ScheduledBackup.INTERVAL_SECONDS}"), recorder.calls)
    }

    // @verifies SWR-MG-03
    @Test fun with_no_folder_the_system_is_not_asked() {
        val kv = InMemoryKeyValueStore()
        val app = services(kv, placeOn = false)
        app.scheduledBackup.turnOn(BundleKeyMethod.RECOVERY_CODE, secret())
        assertEquals("cancel", recorder.calls.last(), "a backup with nowhere to go is not worth waking the phone for")
    }

    // @verifies SWR-MG-03
    @Test fun a_woken_app_makes_the_backup_and_asks_for_the_next_wakeup() = runTest {
        val app = services(InMemoryKeyValueStore())
        app.scheduledBackup.turnOn(BundleKeyMethod.RECOVERY_CODE, secret())
        recorder.calls.clear()
        val outcome = app.runBackgroundBackup()
        assertIs<ScheduledBackup.Outcome.Made>(outcome)
        assertTrue(store.list("").isNotEmpty())
        assertEquals(listOf("request:${ScheduledBackup.INTERVAL_SECONDS}"), recorder.calls)
    }

    // @verifies SWR-MG-03
    @Test fun a_woken_app_that_cannot_open_its_data_writes_nothing_and_keeps_the_request() = runTest {
        // A sealed value on disk and no keystore to open it: the run is locked.
        val sealedKey = EncryptedKeyValueStore.uhdrKeys().first()
        val kv = InMemoryKeyValueStore().apply { putString(sealedKey, EncryptedKeyValueStore.PREFIX + "ciphertext") }
        val app = services(kv)
        assertEquals(life.neurone.core.common.StorageProtection.LOCKED, app.storageProtection)
        recorder.calls.clear()
        assertNull(app.runBackgroundBackup())
        assertTrue(store.list("").isEmpty())
        assertEquals(listOf("request:${ScheduledBackup.INTERVAL_SECONDS}"), recorder.calls, "the request is kept, not withdrawn")
        assertEquals(EncryptedKeyValueStore.PREFIX + "ciphertext", kv.getString(sealedKey), "the sealed value is untouched")
    }
}
