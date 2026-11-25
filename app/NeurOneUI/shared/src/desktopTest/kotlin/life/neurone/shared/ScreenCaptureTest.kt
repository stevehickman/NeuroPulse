package life.neurone.shared

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.SemanticsProperties
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.SemanticsMatcher
import androidx.compose.ui.test.hasAnyAncestor
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.onLast
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollTo
import androidx.compose.ui.test.runComposeUiTest
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.migration.ObjectStoreRoute
import life.neurone.core.protocol.PersistedKeys
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.core.session.SessionHistoryStore
import life.neurone.shared.ble.UnavailableBleCentral
import life.neurone.shared.migration.MemoryObjectStore
import life.neurone.shared.resources.*
import life.neurone.shared.ui.ConsentDashboardScreen
import life.neurone.shared.ui.ConsentOnboardingScreen
import life.neurone.shared.ui.HistoryScreen
import life.neurone.shared.ui.MigrationBackupScreen
import life.neurone.shared.ui.MigrationLinkReceiveScreen
import life.neurone.shared.ui.MigrationLinkSendScreen
import life.neurone.shared.ui.MigrationRestoreScreen
import life.neurone.shared.ui.NeurOneApp
import life.neurone.shared.ui.OnboardingKeys
import life.neurone.shared.ui.RECOVERY_CODE_TAG
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/**
 * The recovery code and the screens that hold health data are kept out of copies and captures (`REQ-SW03-36`; NP-PRIV-ANALYSIS-006
 * F-12): the code cannot be selected, and each such screen holds the window out of capture for as long as it is on show.
 */
@OptIn(ExperimentalTestApi::class)
class ScreenCaptureTest {
    private class RecordingBlocker : ScreenCaptureBlocker {
        val calls = mutableListOf<Boolean>()
        override fun setBlocked(blocked: Boolean) { calls += blocked }
    }

    private fun services(
        kv: InMemoryKeyValueStore = InMemoryKeyValueStore(),
        blocker: ScreenCaptureBlocker? = null,
        place: MigrationPlace? = null,
    ) = AppServices(
        platform = object : PlatformServices {
            override val keyValueStore = kv
            override val bleCentral = UnavailableBleCentral()
            override val protocolSigner = Ed25519ProtocolSigner()
            override val analyticsBackend = NoOpAnalyticsBackend()
            override val migrationPlace = place
            override val screenCaptureBlocker = blocker
            @Composable
            override fun rememberBleConnectAction(): () -> Unit = {}
        },
        scope = CoroutineScope(SupervisorJob() + Dispatchers.Unconfined),
    )

    /** Whether some ancestor of a node makes its text selectable: a `SelectionContainer` adds a focusable node above the text. */
    private val insideSelection = hasAnyAncestor(SemanticsMatcher.keyIsDefined(SemanticsProperties.Focused))

    // @verifies REQ-SW03-36#b
    @Test
    fun theWindowIsHeldOutOfCaptureFromTheFirstHoldUntilTheLastRelease() {
        val blocker = RecordingBlocker()
        val guard = ScreenCaptureGuard(blocker)
        val first = guard.hold()
        val second = guard.hold()
        assertEquals(listOf(true), blocker.calls, "the switch is turned on once, by the first hold")
        first()
        first()
        assertTrue(guard.blocked, "a release run twice is one release, and the second hold still stands")
        assertEquals(listOf(true), blocker.calls)
        second()
        assertFalse(guard.blocked)
        assertEquals(listOf(true, false), blocker.calls, "the switch is turned off after the last release")
    }

    // @verifies REQ-SW03-36#b
    @Test
    fun eachScreenThatShowsTheCodeABackupConsentOrTheHistoryHoldsTheWindowOutOfCaptureWhileItIsOnShow() = runComposeUiTest {
        val app = services(place = MigrationPlace("test place", MemoryObjectStore(), ObjectStoreRoute("local-folder", atomicClaim = true)))
        val screens: List<Pair<String, @Composable () -> Unit>> = listOf(
            "backup" to { MigrationBackupScreen(app, onDone = {}) },
            "restore" to { MigrationRestoreScreen(app, onDone = {}) },
            "move: send" to { MigrationLinkSendScreen(app, onDone = {}) },
            "move: receive" to { MigrationLinkReceiveScreen(app, onDone = {}) },
            "research consent" to { ConsentOnboardingScreen(app.consentStore, onComplete = {}) },
            "privacy dashboard" to { ConsentDashboardScreen(app) },
            "history" to { HistoryScreen(SessionHistoryStore(InMemoryKeyValueStore())) },
        )
        val guard = ScreenCaptureGuard(null)
        var shown by mutableStateOf<Int?>(null)
        setContent {
            CompositionLocalProvider(LocalScreenCaptureGuard provides guard) {
                shown?.let { screens[it].second() } ?: Text("nothing held")
            }
        }
        screens.forEachIndexed { i, (name, _) ->
            shown = i
            waitForIdle()
            assertTrue(guard.blocked, "$name holds the window out of capture while on show")
            shown = null
            waitForIdle()
            assertFalse(guard.blocked, "$name lets go when it leaves")
        }
    }

    // @verifies REQ-SW03-36#b
    @Test
    fun theAppHandsTheHostsSwitchToItsScreens() = runComposeUiTest {
        // Past the age gate and the biometric release, the app opens on the research-consent screen, which holds the window.
        val kv = InMemoryKeyValueStore().apply {
            putBoolean(PersistedKeys.AGE_CONFIRMED_KEY, true)
            putBoolean(OnboardingKeys.BIPA_SHOWN, true)
        }
        val blocker = RecordingBlocker()
        val app = services(kv, blocker)
        setContent { NeurOneApp(app) }
        waitForIdle()
        assertEquals(listOf(true), blocker.calls, "the host's switch is on while the consent screen shows")
    }

    // @verifies REQ-SW03-36#a
    @Test
    fun theRecoveryCodeCannotBeSelectedSoItCannotBeCopied() = runComposeUiTest {
        val used = listOf(
            Res.string.migration_backup_kind_uhdr_title, Res.string.migration_backup_continue, Res.string.migration_backup_method_code,
        )
        var strings: Map<StringResource, String> = emptyMap()
        fun text(r: StringResource) = strings.getValue(r)
        val app = services(place = MigrationPlace("test place", MemoryObjectStore(), ObjectStoreRoute("local-folder", atomicClaim = true)))
        setContent {
            strings = used.associateWith { stringResource(it) }
            Column {
                MigrationBackupScreen(app, onDone = {})
                // The control: the same check finds a text that is selectable.
                SelectionContainer { Text("control", Modifier.testTag("control")) }
            }
        }
        // The health-data card is the last kind offered, and each card has its own Continue; then the preview's Continue.
        onNodeWithText(text(Res.string.migration_backup_kind_uhdr_title)).assertExists()
        onAllNodesWithText(text(Res.string.migration_backup_continue)).onLast().performScrollTo().performClick()
        waitForIdle()
        onAllNodesWithText(text(Res.string.migration_backup_continue)).onLast().performScrollTo().performClick()
        waitForIdle()
        onNodeWithText(text(Res.string.migration_backup_method_code)).performScrollTo().performClick()
        waitForIdle()

        assertTrue(insideSelection.matches(onNodeWithTag("control", useUnmergedTree = true).fetchSemanticsNode()))
        val code = onNodeWithTag(RECOVERY_CODE_TAG, useUnmergedTree = true).fetchSemanticsNode()
        assertTrue(hasTestTag(RECOVERY_CODE_TAG).matches(code))
        assertFalse(insideSelection.matches(code), "the code is not inside a selection, so it cannot be copied")
    }
}
