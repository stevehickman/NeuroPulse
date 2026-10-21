package life.neurone.shared

import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.runComposeUiTest
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.consumable.ConsumableKind
import life.neurone.core.consumable.ConsumableReminder
import life.neurone.core.consumable.ConsumableState
import life.neurone.core.models.HRVData
import life.neurone.core.models.SessionState
import life.neurone.core.models.SessionStatus
import life.neurone.core.protocol.PersistedKeys
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.shared.ble.UnavailableBleCentral
import life.neurone.shared.resources.Res
import life.neurone.shared.resources.tab_consumables
import life.neurone.shared.ui.HealthSignalsEffect
import life.neurone.shared.ui.NeurOneApp
import life.neurone.shared.ui.OnboardingKeys
import life.neurone.shared.ui.hrvBiofeedbackActive
import life.neurone.shared.ui.showReminderIn
import org.jetbrains.compose.resources.rememberResourceEnvironment
import org.jetbrains.compose.resources.stringResource
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** What the iOS host's seams promise, run on the JVM: the reminders' words and timing, and when HealthKit is read. */
@OptIn(ExperimentalTestApi::class, org.jetbrains.compose.resources.ExperimentalResourceApi::class)
class IosHostSeamsTest {

    private class Notifier : ReminderNotifier {
        var permissionAsked = 0
        val shown = mutableListOf<List<String>>()
        override fun requestPermission() { permissionAsked++ }
        override fun show(id: String, title: String, subtitle: String, body: String) { shown += listOf(id, title, subtitle, body) }
    }

    private class Health : HealthSignals {
        val calls = mutableListOf<String>()
        private val hrv = MutableStateFlow<Double?>(null)
        override val hrvSdnnMilliseconds: StateFlow<Double?> = hrv
        override val heartRateBpm: StateFlow<Double?> = MutableStateFlow(null)
        override suspend fun start() { calls += "start" }
        override fun stopAndClear() { calls += "stop" }
    }

    // @verifies REQ-NOTIFY-01#a
    // @verifies REQ-NOTIFY-01#c
    @Test fun a_reminder_keeps_the_consumable_out_of_the_title_and_subtitle_and_has_one_id_per_consumable() = runComposeUiTest {
        val notifier = Notifier()
        val low = ConsumableReminder(ConsumableState(ConsumableKind.INTRANASAL_SLEEVES, sessionCount = ConsumableKind.INTRANASAL_SLEEVES.sessionLimit - 2))
        val exceeded = ConsumableReminder(ConsumableState(ConsumableKind.INTRANASAL_SLEEVES, sessionCount = ConsumableKind.INTRANASAL_SLEEVES.sessionLimit))
        setContent {
            val environment = rememberResourceEnvironment()
            LaunchedEffect(Unit) { notifier.showReminderIn(low, environment); notifier.showReminderIn(exceeded, environment) }
        }
        waitForIdle()

        assertEquals(2, notifier.shown.size)
        val (id, title, subtitle, body) = notifier.shown[0]
        assertEquals("NeurOne", title)
        assertFalse(subtitle.contains("Intranasal", ignoreCase = true) || title.contains("Intranasal", ignoreCase = true), "the lock screen names no consumable")
        assertTrue(body.contains("Intranasal Sleeves") && body.contains("2"), "the consumable and its count are in the body: $body")
        assertTrue(notifier.shown[1][3].contains("Replace now"), "an exceeded consumable says to replace it: ${notifier.shown[1][3]}")
        assertEquals(id, notifier.shown[1][0], "a second reminder for the same consumable replaces the first")
    }

    // @verifies REQ-NOTIFY-01#b
    @Test fun permission_to_notify_is_asked_when_the_consumables_screen_appears_and_not_before() = runComposeUiTest {
        val notifier = Notifier()
        val kv = InMemoryKeyValueStore().apply {
            putBoolean(PersistedKeys.AGE_CONFIRMED_KEY, true)
            putBoolean(OnboardingKeys.BIPA_ACCEPTED, true)
            putBoolean(OnboardingKeys.CONSENT_SHOWN, true)
            putBoolean(PersistedKeys.FIRST_SETUP_KEY, true)
        }
        val app = AppServices(
            platform = object : PlatformServices {
                override val keyValueStore = kv
                override val bleCentral = UnavailableBleCentral()
                override val protocolSigner = Ed25519ProtocolSigner()
                override val analyticsBackend = NoOpAnalyticsBackend()
                override val reminderNotifier = notifier
                @Composable
                override fun rememberBleConnectAction(): () -> Unit = {}
            },
            scope = CoroutineScope(SupervisorJob() + Dispatchers.Unconfined),
        )
        var consumables = ""
        setContent {
            consumables = stringResource(Res.string.tab_consumables)
            NeurOneApp(app)
        }
        waitForIdle()
        assertEquals(0, notifier.permissionAsked, "nothing is asked at launch")
        onNodeWithText(consumables).performClick()
        waitForIdle()
        assertEquals(1, notifier.permissionAsked)
    }

    // @verifies REQ-HEALTH-01#a
    @Test fun healthkit_is_started_only_while_hrv_biofeedback_runs_and_cleared_when_it_stops_or_the_screen_goes() = runComposeUiTest {
        val health = Health()
        var active by mutableStateOf(false)
        var shown by mutableStateOf(true)
        setContent { if (shown) HealthSignalsEffect(health, active) }
        waitForIdle()
        assertFalse("start" in health.calls, "not asked at launch or before a session")

        active = true
        waitForIdle()
        assertEquals("start", health.calls.last())

        active = false
        waitForIdle()
        assertEquals("stop", health.calls.last(), "cleared when the session ends")

        active = true
        waitForIdle()
        shown = false
        waitForIdle()
        assertEquals("stop", health.calls.last(), "cleared when the screen goes")
    }

    // @verifies REQ-HEALTH-01#a
    @Test fun hrv_biofeedback_is_a_running_session_with_hrv_data_and_nothing_less() {
        val hrv = HRVData(coherenceScore = 5f, rmssdMilliseconds = 40)
        val running = SessionState.EMPTY.copy(status = SessionStatus.RUNNING, hrv = hrv)
        assertTrue(hrvBiofeedbackActive(running))
        assertFalse(hrvBiofeedbackActive(running.copy(hrv = null)), "a running session with no HRV data is not biofeedback")
        assertFalse(hrvBiofeedbackActive(running.copy(status = SessionStatus.PAUSED)))
        assertFalse(hrvBiofeedbackActive(SessionState.EMPTY.copy(hrv = hrv)), "an idle session reads nothing")
    }
}
