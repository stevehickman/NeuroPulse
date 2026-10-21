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
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.consumable.ConsumableKind
import life.neurone.core.consumable.ConsumableReminder
import life.neurone.core.consumable.ConsumableState
import life.neurone.core.protocol.PersistedKeys
import life.neurone.core.session.Ed25519ProtocolSigner
import life.neurone.shared.ble.UnavailableBleCentral
import life.neurone.shared.resources.Res
import life.neurone.shared.resources.tab_consumables
import life.neurone.shared.ui.NeurOneApp
import life.neurone.shared.ui.OnboardingKeys
import life.neurone.shared.ui.showReminderIn
import org.jetbrains.compose.resources.rememberResourceEnvironment
import org.jetbrains.compose.resources.stringResource
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** What the iOS host's seams promise, run on the JVM: the reminders' words and timing. */
@OptIn(ExperimentalTestApi::class, org.jetbrains.compose.resources.ExperimentalResourceApi::class)
class IosHostSeamsTest {

    private class Notifier : ReminderNotifier {
        var permissionAsked = 0
        val shown = mutableListOf<List<String>>()
        override fun requestPermission() { permissionAsked++ }
        override fun show(id: String, title: String, subtitle: String, body: String) { shown += listOf(id, title, subtitle, body) }
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
}
