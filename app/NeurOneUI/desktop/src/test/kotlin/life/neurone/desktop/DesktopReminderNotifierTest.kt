package life.neurone.desktop

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class DesktopReminderNotifierTest {
    // @verifies REQ-NOTIFY-01#a
    @Test fun the_balloon_carries_the_title_and_subtitle_and_never_the_consumable() {
        val shown = mutableListOf<Pair<String, String>>()
        val notifier = DesktopReminderNotifier()
        notifier.attach { title, message -> shown += title to message }
        notifier.show("INTRANASAL_SLEEVES", "NeurOne", "Action required", "Intranasal Sleeves - Replace now")
        assertEquals(listOf("NeurOne" to "Action required"), shown)
        assertFalse(shown.any { (t, m) -> "Intranasal" in t || "Intranasal" in m })
    }

    // @verifies REQ-NOTIFY-01#a
    @Test fun a_reminder_with_no_tray_attached_is_dropped_and_the_screen_still_lists_it() {
        val notifier = DesktopReminderNotifier()
        notifier.show("X", "NeurOne", "Action required", "body")  // nothing attached: must not throw
        val shown = mutableListOf<String>()
        notifier.attach { t, _ -> shown += t }
        notifier.attach(null)
        notifier.show("X", "NeurOne", "Action required", "body")
        assertTrue(shown.isEmpty())
    }
}
