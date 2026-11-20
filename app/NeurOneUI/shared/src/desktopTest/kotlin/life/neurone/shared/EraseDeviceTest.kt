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

    // @verifies SWR-MG-30#d
    @Test fun an_erase_ends_the_cloud_sign_ins_first_and_says_when_a_providers_access_may_remain() = runTest {
        for ((answer, expected) in listOf<Pair<suspend () -> Boolean, Boolean>>(
            suspend { false } to false,
            suspend { true } to true,
            suspend { throw IllegalStateException("no network") } to true,
        )) {
            val kv = InMemoryKeyValueStore().also { it.putString("np.cloud.accounts", "sealed credentials") }
            var asked = 0
            var storedWhenAsked: String? = null
            val model = EraseDeviceModel(kv, null, InMemorySecretKeyStore(), gatt(kv), endCloudAccess = {
                asked++
                storedWhenAsked = kv.getString("np.cloud.accounts")
                answer()
            })
            model.erase()
            val done = model.step.value as EraseStep.Done
            assertEquals(1, asked)
            assertEquals("sealed credentials", storedWhenAsked, "the providers are asked while the credentials can still be read")
            assertEquals(expected, done.cloudAccessMayRemain)
            assertNull(kv.getString("np.cloud.accounts"), "the credentials are erased whatever the providers answered")
        }
        val kv = InMemoryKeyValueStore()
        val plain = EraseDeviceModel(kv, null, InMemorySecretKeyStore(), gatt(kv))
        plain.erase()
        assertFalse((plain.step.value as EraseStep.Done).cloudAccessMayRemain, "a host with no cloud sign-in has nothing to say")
    }

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

    private class Relay(private val status: Int?, private val kv: InMemoryKeyValueStore) : life.neurone.core.net.HttpTransport {
        val bodies = mutableListOf<String>()
        var dataWasStillThere = false
        override suspend fun post(request: life.neurone.core.net.HttpRequest): life.neurone.core.net.HttpResponse? {
            bodies += request.body.decodeToString()
            dataWasStillThere = kv.getString("np.session.history") != null
            return status?.let { life.neurone.core.net.HttpResponse(it, emptyMap(), ByteArray(0)) }
        }
    }

    private class IdBackend(private var id: String?) : life.neurone.core.analytics.AnalyticsBackend {
        override val distinctId: String? get() = id
        override fun configure() {}
        override fun reset() { id = null }
        override fun track(event: String, properties: Map<String, String>) {}
    }

    private val analyticsId = "0b1c2d3e4f5061728394a5b6c7d8e9f0"

    private fun gate(kv: InMemoryKeyValueStore, relay: Relay) = life.neurone.core.analytics.ResearchAnalyticsGate(
        kv, IdBackend(analyticsId),
        life.neurone.core.analytics.AnalyticsErasureQueue(kv, relay) { "https://analytics.example.org" },
    )

    // OI-PRIV-08 (1): the wipe deletes the only copy of the analytics ID, so the vendor is asked to erase it first.
    // @verifies REQ-SW03-19#d
    @Test fun an_erase_asks_the_vendor_to_erase_the_analytics_id_before_it_wipes_the_only_copy() = runTest {
        val kv = InMemoryKeyValueStore().also {
            it.putString("np.session.history", "history of the person")
            it.putBoolean(life.neurone.core.protocol.PersistedKeys.RESEARCH_ANALYTICS_KEY, true)
        }
        val relay = Relay(200, kv)
        val gate = gate(kv, relay)
        val model = EraseDeviceModel(kv, null, null, gatt(kv), beforeWipe = { gate.resetForErase() })
        model.erase()

        assertTrue(relay.bodies.single().contains(analyticsId), "the relay was asked about this install's ID")
        assertTrue(relay.dataWasStillThere, "and it was asked before the wipe, not after")
        assertEquals(0, (model.step.value as EraseStep.Done).analyticsIdsNotErased)
        assertNull(kv.getString("np.session.history"))
        assertNull(kv.getString(life.neurone.core.analytics.AnalyticsErasureQueue.KEY), "nothing of the queue is written back after the wipe")
    }

    // @verifies REQ-SW03-19#d
    @Test fun an_erase_is_not_held_back_by_a_relay_that_does_not_take_the_id_and_says_how_many_were_left() = runTest {
        for (status in listOf(null, 404, 503)) {
            val kv = InMemoryKeyValueStore().also { it.putString("np.session.history", "history of the person") }
            val gate = gate(kv, Relay(status, kv))
            val model = EraseDeviceModel(kv, null, null, gatt(kv), beforeWipe = { gate.resetForErase() })
            model.erase()
            assertEquals(1, (model.step.value as EraseStep.Done).analyticsIdsNotErased, "status $status")
            assertNull(kv.getString("np.session.history"), "the person's erase still happens")
            assertNull(kv.getString(life.neurone.core.analytics.AnalyticsErasureQueue.KEY))
        }
    }

    // @verifies REQ-SW03-19#d
    @Test fun an_erase_still_finishes_when_the_step_before_the_wipe_throws() = runTest {
        val kv = InMemoryKeyValueStore().also { it.putString("np.session.history", "np-enc:1:sealed") }
        val model = EraseDeviceModel(kv, null, null, gatt(kv), beforeWipe = { throw IllegalStateException("the store is locked") })
        model.erase()
        assertEquals(1, (model.step.value as EraseStep.Done).analyticsIdsNotErased, "not known to be erased is reported as not erased")
        assertNull(kv.getString("np.session.history"))
    }

    // A locked run reads its sealed keys as absent, so the gate reports nothing left while the raw store still holds them.
    // @verifies REQ-SW03-19#d
    @Test fun a_locked_run_counts_the_sealed_analytics_values_it_could_not_read_and_any_other_run_counts_none() {
        val locked = life.neurone.core.common.StorageProtection.LOCKED
        val raw = InMemoryKeyValueStore()
        assertEquals(0, life.neurone.shared.migration.unreadableAnalyticsIds(locked, raw), "nothing stored, nothing unsent")
        raw.putString("np.analytics.distinct-id", "np-enc:1:sealed")
        assertEquals(1, life.neurone.shared.migration.unreadableAnalyticsIds(locked, raw))
        raw.putString("np.analytics.pending-erasures", "np-enc:1:sealed")
        assertEquals(2, life.neurone.shared.migration.unreadableAnalyticsIds(locked, raw))
        for (readable in life.neurone.core.common.StorageProtection.entries.filter { it != locked }) {
            assertEquals(0, life.neurone.shared.migration.unreadableAnalyticsIds(readable, raw), "$readable: the gate read them, so its own count stands")
        }
    }

    // OI-ANALYTICS-01 (5) (b): the warning is asked before the person confirms, and never throws.
    // @verifies REQ-SW03-19#f
    @Test fun a_locked_run_can_say_before_the_erase_that_it_holds_analytics_values_it_cannot_read() {
        val kv = InMemoryKeyValueStore()
        assertEquals(0, EraseDeviceModel(kv, null, null, gatt(kv)).analyticsUnreadable, "a host that does not ask has nothing to say")
        assertEquals(2, EraseDeviceModel(kv, null, null, gatt(kv), unreadableBeforeErase = { 2 }).analyticsUnreadable)
        assertEquals(0, EraseDeviceModel(kv, null, null, gatt(kv), unreadableBeforeErase = { 0 }).analyticsUnreadable, "a readable run says nothing")
        assertEquals(0, EraseDeviceModel(kv, null, null, gatt(kv), unreadableBeforeErase = { throw IllegalStateException("store") }).analyticsUnreadable, "a store that cannot answer does not block the screen")
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
