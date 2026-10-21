package life.neurone.shared

import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.runComposeUiTest
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.runBlocking
import life.neurone.core.common.GatedSecretKeyStore
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.common.InMemorySecretKeyStore
import life.neurone.core.common.PresenceState
import life.neurone.core.common.SecretKeyUnavailableException
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.shared.ble.UnavailableBleCentral
import life.neurone.shared.migration.ScheduledBackup
import life.neurone.shared.resources.*
import life.neurone.shared.ui.NeurOneApp
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** The unlock screen, the lifecycle feed and the backup-first rule for the UHDR presence gate (`NP-APP-PRESENCE-001`, increment 2). */
@OptIn(ExperimentalTestApi::class)
class PresenceGateTest {
    private class Gated(var allow: Boolean = true, override var isEnabled: Boolean = true, var canChange: Boolean = true) : GatedSecretKeyStore {
        private val real = InMemorySecretKeyStore()
        override var isUnlocked = false
            private set
        var prompts = 0
        override suspend fun unlock(reason: String): Boolean { prompts++; if (allow) isUnlocked = true; return allow }
        override fun lock() { isUnlocked = false }
        override suspend fun enable(): Boolean { if (canChange) isEnabled = true; return canChange }
        override suspend fun disable(): Boolean { if (canChange) isEnabled = false; return canChange }
        override fun load(): ByteArray? { if (!isUnlocked) throw SecretKeyUnavailableException("locked"); return real.load() }
        override fun store(key: ByteArray) { if (!isUnlocked) throw SecretKeyUnavailableException("locked"); real.store(key) }
        override fun destroy() = real.destroy()
    }

    private val foreground = MutableStateFlow(true)
    private var clock = 0L
    private val used = listOf(
        Res.string.presence_unlock_title, Res.string.presence_unlock_failed, Res.string.presence_unlock_button, Res.string.setup_title_welcome,
    )
    private var strings: Map<StringResource, String> = emptyMap()
    private fun text(r: StringResource) = strings.getValue(r)

    private fun services(gated: Gated?, host: InMemoryKeyValueStore = InMemoryKeyValueStore()) = AppServices(
        platform = object : PlatformServices {
            override val keyValueStore = host
            override val bleCentral = UnavailableBleCentral()
            override val protocolSigner = Ed25519ProtocolSigner()
            override val analyticsBackend = NoOpAnalyticsBackend()
            override val secretKeyStore = InMemorySecretKeyStore()
            override val gatedKeyStore = gated
            override val appInForeground: StateFlow<Boolean> = foreground
            @androidx.compose.runtime.Composable
            override fun rememberBleConnectAction(): () -> Unit = {}
        },
        scope = CoroutineScope(SupervisorJob() + Dispatchers.Unconfined),
        nowMs = { clock },
    )

    // @verifies REQ-PRES-03
    @Test
    fun aColdStartAsksBeforeAnythingIsShown() = runComposeUiTest {
        val gated = Gated()
        val app = services(gated)
        assertEquals(PresenceState.LOCKED, app.presenceLock!!.state.value)
        setContent {
            strings = used.associateWith { stringResource(it) }
            NeurOneApp(app)
        }
        waitForIdle()
        assertEquals(1, gated.prompts, "asked once, at show")
        assertEquals(PresenceState.UNLOCKED, app.presenceLock!!.state.value)
        onNodeWithText(text(Res.string.setup_title_welcome)).assertExists()
    }

    // @verifies REQ-PRES-01#a
    @Test
    fun aRefusedPromptShowsNothingBehindItAndTheButtonAsksAgain() = runComposeUiTest {
        val gated = Gated(allow = false)
        val app = services(gated)
        setContent {
            strings = used.associateWith { stringResource(it) }
            NeurOneApp(app)
        }
        waitForIdle()
        onNodeWithText(text(Res.string.presence_unlock_failed)).assertExists()
        onNodeWithText(text(Res.string.setup_title_welcome)).assertDoesNotExist()

        gated.allow = true
        onNodeWithText(text(Res.string.presence_unlock_button)).performClick()
        waitForIdle()
        assertEquals(2, gated.prompts)
        onNodeWithText(text(Res.string.setup_title_welcome)).assertExists()
    }

    // @verifies REQ-PRES-03
    @Test
    fun theBackgroundTimeoutLocksAgainAndAShortAbsenceDoesNot() = runComposeUiTest {
        val gated = Gated()
        val app = services(gated)
        setContent {
            strings = used.associateWith { stringResource(it) }
            NeurOneApp(app)
        }
        waitForIdle()
        assertTrue(app.presenceLock!!.isUnlocked)

        foreground.value = false
        waitForIdle()
        clock += app.presenceLock!!.backgroundTimeoutMs - 1
        foreground.value = true
        waitForIdle()
        assertTrue(app.presenceLock!!.isUnlocked, "back inside the timeout")

        foreground.value = false
        waitForIdle()
        clock += app.presenceLock!!.backgroundTimeoutMs
        foreground.value = true
        waitForIdle()
        assertEquals(2, gated.prompts, "asked again after the timeout")
    }

    // @verifies REQ-PRES-08
    @Test
    fun theGateIsNotTurnedOnUntilABackupExists() {
        val host = InMemoryKeyValueStore()
        val gated = Gated(isEnabled = false)
        val app = services(gated, host)
        assertFalse(app.presenceEnabled)
        assertEquals(AppServices.PresenceChange.BACKUP_REQUIRED, runBlocking { app.setPresenceGate(true) })
        assertFalse(gated.isEnabled, "nothing was changed")

        app.keyValueStore.putString(ScheduledBackup.LAST_KEY, "1")
        assertEquals(AppServices.PresenceChange.CHANGED, runBlocking { app.setPresenceGate(true) })
        assertTrue(app.presenceEnabled)
    }

    // @verifies REQ-PRES-08
    @Test
    fun turningTheGateOffNeedsNoBackupAndAHostThatCannotLeavesItAsItWas() {
        val gated = Gated(isEnabled = true, canChange = false)
        val app = services(gated)
        assertEquals(AppServices.PresenceChange.REFUSED, runBlocking { app.setPresenceGate(false) })
        assertTrue(gated.isEnabled)
        gated.canChange = true
        assertEquals(AppServices.PresenceChange.CHANGED, runBlocking { app.setPresenceGate(false) })
    }

    // @verifies REQ-PRES-01#a
    @Test
    fun withoutAHostKeyThereIsNoGateAndNothingChanges() = runComposeUiTest {
        val app = services(null)
        assertEquals(null, app.presenceLock)
        assertFalse(app.presenceAvailable)
        setContent {
            strings = used.associateWith { stringResource(it) }
            NeurOneApp(app)
        }
        waitForIdle()
        onNodeWithText(text(Res.string.setup_title_welcome)).assertExists()
    }
}
