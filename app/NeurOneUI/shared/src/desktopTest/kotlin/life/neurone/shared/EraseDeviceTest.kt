package life.neurone.shared

import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.SemanticsProperties
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.SemanticsMatcher
import androidx.compose.ui.test.assertIsEnabled
import androidx.compose.ui.test.assertIsNotEnabled
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.runComposeUiTest
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.test.runTest
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.common.InMemorySecretKeyStore
import life.neurone.core.common.SecretKeyStore
import life.neurone.core.migration.DirectoryFileStore
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.shared.ble.NeurOneGattManager
import life.neurone.shared.ble.UnavailableBleCentral
import life.neurone.shared.migration.EraseDeviceModel
import life.neurone.shared.migration.EraseStep
import life.neurone.shared.resources.*
import life.neurone.shared.ui.SettingsScreen
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import java.nio.file.Files
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The person's "erase this device" (desktop audit of the privacy review, item 5, SWR-MG-17): the model, then the Settings section. */
@OptIn(ExperimentalTestApi::class)
class EraseDeviceTest {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Unconfined)

    /** A keystore that holds a key and cannot remove it. */
    private class StuckKeystore(private val real: SecretKeyStore) : SecretKeyStore {
        override fun load(): ByteArray? = real.load()
        override fun store(key: ByteArray) = real.store(key)
        override fun destroy() = throw IllegalStateException("the credential tool timed out")
    }

    private fun gatt(kv: InMemoryKeyValueStore) = NeurOneGattManager(UnavailableBleCentral(), scope, kv)

    // @verifies SWR-MG-17#b
    @Test fun an_erase_destroys_the_key_overwrites_and_deletes_the_files_and_forgets_every_stored_key() = runTest {
        val dir = Files.createTempDirectory("np-erase").toFile()
        dir.resolve("np-app.properties").writeBytes(ByteArray(2_000) { 7 })
        val kv = InMemoryKeyValueStore().also {
            it.putString("np.session.history", "history of the person")
            it.putString("ble.last-hub-id", "hub-1")
        }
        val secrets = InMemorySecretKeyStore().also { it.store(ByteArray(32) { 1 }) }

        val model = EraseDeviceModel(kv, DirectoryFileStore(dir.toPath()), secrets, gatt(kv))
        model.erase()

        val done = model.step.value as EraseStep.Done
        assertFalse(done.keyKept)
        assertNull(secrets.load(), "the key is destroyed")
        assertFalse(dir.resolve("np-app.properties").exists(), "the file is overwritten and deleted")
        assertNull(kv.getString("np.session.history"))
        assertNull(kv.getString("ble.last-hub-id"))
    }

    // @verifies SWR-MG-17#b
    @Test fun an_erase_still_finishes_when_the_keystore_cannot_remove_the_key_and_says_so() = runTest {
        val dir = Files.createTempDirectory("np-erase").toFile()
        dir.resolve("np-app.properties").writeBytes(ByteArray(100) { 7 })
        val kv = InMemoryKeyValueStore().also { it.putString("np.session.history", "np-enc:1:sealed") }
        val real = InMemorySecretKeyStore().also { it.store(ByteArray(32) { 1 }) }

        val model = EraseDeviceModel(kv, DirectoryFileStore(dir.toPath()), StuckKeystore(real), gatt(kv))
        model.erase()

        assertTrue((model.step.value as EraseStep.Done).keyKept, "the person is told the key stayed")
        assertFalse(dir.resolve("np-app.properties").exists(), "the data the key opened is gone")
        assertNull(kv.getString("np.session.history"), "and so are the stored keys")
    }

    // @verifies SWR-MG-17#b
    @Test fun an_erase_works_on_a_host_with_no_keystore() = runTest {
        val kv = InMemoryKeyValueStore().also { it.putString("np.session.history", "history of the person") }
        val model = EraseDeviceModel(kv, null, null, gatt(kv))
        model.erase()
        assertTrue(model.step.value is EraseStep.Done)
        assertNull(kv.getString("np.session.history"))
    }

    private val used = listOf(
        Res.string.settings_erase_device_title, Res.string.settings_erase_device_confirm,
        Res.string.settings_erase_device_button, Res.string.settings_erase_device_close,
    )
    private var strings: Map<StringResource, String> = emptyMap()
    private fun text(r: StringResource) = strings.getValue(r)

    private fun services(endRun: (() -> Unit)?, kv: InMemoryKeyValueStore) = AppServices(
        platform = object : PlatformServices {
            override val keyValueStore = kv
            override val bleCentral = UnavailableBleCentral()
            override val protocolSigner = Ed25519ProtocolSigner()
            override val analyticsBackend = NoOpAnalyticsBackend()
            override val endRunAfterErase = endRun
            @androidx.compose.runtime.Composable
            override fun rememberBleConnectAction(): () -> Unit = {}
        },
        scope = scope,
    )

    // @verifies SWR-MG-17#c
    @Test fun the_button_stays_disabled_until_the_acknowledgement_then_erases_and_offers_to_close_the_app() = runComposeUiTest {
        val kv = InMemoryKeyValueStore().also { it.putString("np.session.history", "history of the person") }
        var ended = 0
        val app = services({ ended++ }, kv)
        setContent {
            strings = used.associateWith { stringResource(it) }
            SettingsScreen(app)
        }
        onNodeWithText(text(Res.string.settings_erase_device_title)).assertExists()
        onNodeWithText(text(Res.string.settings_erase_device_button)).performScrollTo().assertIsNotEnabled()

        onNode(SemanticsMatcher.expectValue(SemanticsProperties.Role, Role.Checkbox)).performScrollTo().performClick()
        onNodeWithText(text(Res.string.settings_erase_device_button)).performScrollTo().assertIsEnabled()
        onNodeWithText(text(Res.string.settings_erase_device_button)).performClick()

        waitUntil { app.eraseDevice!!.step.value is EraseStep.Done }
        assertNull(kv.getString("np.session.history"), "what was stored is forgotten")
        assertEquals(0, ended, "the app is not ended until the person closes it")
        onNodeWithText(text(Res.string.settings_erase_device_close)).performScrollTo().performClick()
        assertEquals(1, ended)
    }

    // @verifies SWR-MG-17#c
    @Test fun a_host_that_cannot_end_the_run_offers_no_erase() = runComposeUiTest {
        val app = services(null, InMemoryKeyValueStore())
        setContent {
            strings = used.associateWith { stringResource(it) }
            SettingsScreen(app)
        }
        onNodeWithText(text(Res.string.settings_erase_device_title)).assertDoesNotExist()
    }
}
