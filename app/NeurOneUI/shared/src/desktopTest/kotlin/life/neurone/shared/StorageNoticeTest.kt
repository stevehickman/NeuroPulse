package life.neurone.shared

import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.runComposeUiTest
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.common.InMemorySecretKeyStore
import life.neurone.core.common.KeyValueStore
import life.neurone.core.common.SecretKeyStore
import life.neurone.core.common.StorageProtection
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.shared.ble.UnavailableBleCentral
import life.neurone.shared.resources.*
import life.neurone.shared.ui.NeurOneApp
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import kotlin.test.Test
import kotlin.test.assertEquals

/** The person is told when the protected data is not sealed on this run, and a run that cannot open it does not overwrite it (SWR-MG-13). */
@OptIn(ExperimentalTestApi::class)
class StorageNoticeTest {
    private val used = listOf(Res.string.storage_locked_notice, Res.string.storage_unsealed_notice)
    private var strings: Map<StringResource, String> = emptyMap()
    private fun text(r: StringResource) = strings.getValue(r)

    private fun services(kv: KeyValueStore, secrets: SecretKeyStore?, reports: Boolean) = AppServices(
        platform = object : PlatformServices {
            override val keyValueStore = kv
            override val bleCentral = UnavailableBleCentral()
            override val protocolSigner = Ed25519ProtocolSigner()
            override val analyticsBackend = NoOpAnalyticsBackend()
            override val secretKeyStore = secrets
            override val reportsUnsealedStorage = reports
            @androidx.compose.runtime.Composable
            override fun rememberBleConnectAction(): () -> Unit = {}
        },
        scope = CoroutineScope(SupervisorJob() + Dispatchers.Unconfined),
    )

    /** A host store that an earlier, working run has sealed data into. */
    private fun sealedHost(): Pair<InMemoryKeyValueStore, InMemorySecretKeyStore> {
        val host = InMemoryKeyValueStore()
        val secrets = InMemorySecretKeyStore()
        services(host, secrets, false).keyValueStore.putString("np.session.history", "history of the person")
        return host to secrets
    }

    // @verifies SWR-MG-13#c
    @Test
    fun aLockedRunTellsThePersonAndLeavesTheSealedDataAlone() = runComposeUiTest {
        val (host, _) = sealedHost()
        val before = host.getString("np.session.history")
        val app = services(host, null, reports = false)
        assertEquals(StorageProtection.LOCKED, app.storageProtection)

        setContent {
            strings = used.associateWith { stringResource(it) }
            NeurOneApp(app)
        }
        waitForIdle()
        onNodeWithText(text(Res.string.storage_locked_notice)).assertExists()
        assertEquals(before, host.getString("np.session.history"), "the sealed history is as it was")
    }

    // @verifies SWR-MG-13#c
    @Test
    fun aHostThatKeepsDataOnDiskAndHasNoKeystoreIsToldItIsUnsealed() = runComposeUiTest {
        val app = services(InMemoryKeyValueStore(), null, reports = true)
        assertEquals(StorageProtection.UNSEALED, app.storageProtection)

        setContent {
            strings = used.associateWith { stringResource(it) }
            NeurOneApp(app)
        }
        waitForIdle()
        onNodeWithText(text(Res.string.storage_unsealed_notice)).assertExists()
    }

    // @verifies SWR-MG-13#c
    @Test
    fun aHostThatKeepsNothingAtRestShowsNoNotice() = runComposeUiTest {
        val app = services(InMemoryKeyValueStore(), null, reports = false)
        setContent {
            strings = used.associateWith { stringResource(it) }
            NeurOneApp(app)
        }
        waitForIdle()
        onNodeWithText(text(Res.string.storage_locked_notice)).assertDoesNotExist()
        onNodeWithText(text(Res.string.storage_unsealed_notice)).assertDoesNotExist()
    }

    // @verifies SWR-MG-13#c
    @Test
    fun aSealedRunShowsNoNotice() = runComposeUiTest {
        val app = services(InMemoryKeyValueStore(), InMemorySecretKeyStore(), reports = true)
        assertEquals(StorageProtection.SEALED, app.storageProtection)
        setContent {
            strings = used.associateWith { stringResource(it) }
            NeurOneApp(app)
        }
        waitForIdle()
        onNodeWithText(text(Res.string.storage_locked_notice)).assertDoesNotExist()
        onNodeWithText(text(Res.string.storage_unsealed_notice)).assertDoesNotExist()
    }
}
