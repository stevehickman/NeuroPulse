package life.neurone.shared

import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.runComposeUiTest
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import life.neurone.core.common.EncryptedKeyValueStore
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.common.InMemorySecretKeyStore
import life.neurone.core.common.SecretKeyStore
import life.neurone.core.common.StorageProtection
import life.neurone.core.data.BundleKeyDerivation
import life.neurone.core.data.BundleKeyMethod
import life.neurone.core.data.SystemRandomSource
import life.neurone.core.migration.ObjectStoreRoute
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.shared.ble.UnavailableBleCentral
import life.neurone.shared.migration.MemoryObjectStore
import life.neurone.shared.migration.ScheduledBackup
import life.neurone.shared.resources.*
import life.neurone.shared.ui.MigrationBackupScreen
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * The automatic backup's code or passphrase is kept only where the run seals it (`SWR-MG-13` (e), `OI-PRIV-61`): an unsealed run's plain
 * store never receives it, and the backup screen says why the automatic backup is not offered.
 */
@OptIn(ExperimentalTestApi::class)
class AutomaticBackupSealingTest {
    private val used = listOf(
        Res.string.migration_backup_kind_uhdr_title,
        Res.string.migration_backup_continue,
        Res.string.migration_backup_auto_option,
        Res.string.migration_backup_auto_unavailable_unsealed,
    )
    private var strings: Map<StringResource, String> = emptyMap()
    private fun text(r: StringResource) = strings.getValue(r)

    private fun services(host: InMemoryKeyValueStore, secrets: SecretKeyStore?) = AppServices(
        platform = object : PlatformServices {
            override val keyValueStore = host
            override val bleCentral = UnavailableBleCentral()
            override val protocolSigner = Ed25519ProtocolSigner()
            override val analyticsBackend = NoOpAnalyticsBackend()
            override val secretKeyStore = secrets
            override val migrationPlace = MigrationPlace("folder", MemoryObjectStore(), ObjectStoreRoute("folder", atomicClaim = true))
            @androidx.compose.runtime.Composable
            override fun rememberBleConnectAction(): () -> Unit = {}
        },
        scope = CoroutineScope(SupervisorJob() + Dispatchers.Unconfined),
    )

    private fun newSecret() = BundleKeyDerivation.newRecoveryCode(SystemRandomSource).secret

    // @verifies SWR-MG-13#e
    @Test
    fun anUnsealedRunNeverPutsTheSecretInTheHostsPlainStore() {
        val host = InMemoryKeyValueStore()
        val app = services(host, null)
        assertEquals(StorageProtection.UNSEALED, app.storageProtection)
        assertFalse(app.scheduledBackup.isAvailable)
        assertFalse(app.scheduledBackup.turnOn(BundleKeyMethod.PASSPHRASE, newSecret()))
        assertNull(host.getString(ScheduledBackup.SECRET_KEY), "nothing lands beside the backups")
    }

    // @verifies SWR-MG-13#e
    @Test
    fun aSealedRunKeepsTheSecretSealedAtRest() {
        val host = InMemoryKeyValueStore()
        val app = services(host, InMemorySecretKeyStore())
        assertEquals(StorageProtection.SEALED, app.storageProtection)
        assertTrue(app.scheduledBackup.isAvailable)
        assertTrue(app.scheduledBackup.turnOn(BundleKeyMethod.RECOVERY_CODE, newSecret()))
        val raw = assertNotNull(host.getString(ScheduledBackup.SECRET_KEY))
        assertTrue(raw.startsWith(EncryptedKeyValueStore.PREFIX), "the host holds it sealed, not as the code")
        assertTrue(app.scheduledBackup.isOn)
    }

    // @verifies SWR-MG-13#e
    @Test
    fun anUnsealedRunStartedOverASecretLeftInTheClearRemovesIt() {
        val host = InMemoryKeyValueStore().also { it.putString(ScheduledBackup.SECRET_KEY, "passphrase:00ff") }
        val app = services(host, null)
        assertNull(host.getString(ScheduledBackup.SECRET_KEY))
        assertFalse(app.scheduledBackup.isOn)
    }

    // @verifies SWR-MG-13#e
    @Test
    fun theBackupScreenSaysWhyTheAutomaticBackupIsNotOfferedOnAnUnsealedRun() = runComposeUiTest {
        val app = services(InMemoryKeyValueStore(), null)
        setContent {
            strings = used.associateWith { stringResource(it) }
            MigrationBackupScreen(app, onDone = {})
        }
        waitForIdle()
        onNodeWithText(text(Res.string.migration_backup_kind_uhdr_title)).performClick()
        waitForIdle()
        onNodeWithText(text(Res.string.migration_backup_continue)).performClick()
        waitForIdle()
        onNodeWithText(text(Res.string.migration_backup_auto_unavailable_unsealed)).assertExists()
        onNodeWithText(text(Res.string.migration_backup_auto_option)).assertDoesNotExist()
    }

    // @verifies SWR-MG-13#e
    @Test
    fun theBackupScreenOffersTheAutomaticBackupOnASealedRun() = runComposeUiTest {
        val app = services(InMemoryKeyValueStore(), InMemorySecretKeyStore())
        setContent {
            strings = used.associateWith { stringResource(it) }
            MigrationBackupScreen(app, onDone = {})
        }
        waitForIdle()
        onNodeWithText(text(Res.string.migration_backup_kind_uhdr_title)).performClick()
        waitForIdle()
        onNodeWithText(text(Res.string.migration_backup_continue)).performClick()
        waitForIdle()
        onNodeWithText(text(Res.string.migration_backup_auto_option)).assertExists()
        onNodeWithText(text(Res.string.migration_backup_auto_unavailable_unsealed)).assertDoesNotExist()
    }
}
