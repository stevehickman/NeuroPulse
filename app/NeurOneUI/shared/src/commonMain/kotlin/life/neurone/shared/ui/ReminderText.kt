package life.neurone.shared.ui

import life.neurone.core.consumable.ConsumableReminder
import life.neurone.shared.ReminderNotifier
import life.neurone.shared.resources.Res
import life.neurone.shared.resources.consumable_notify_exceeded
import life.neurone.shared.resources.consumable_notify_low
import life.neurone.shared.resources.notify_action_required
import life.neurone.shared.resources.ui_app_title
import org.jetbrains.compose.resources.ExperimentalResourceApi
import org.jetbrains.compose.resources.ResourceEnvironment
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.getString

/**
 * Show a consumable reminder through the host's notifier, worded here once for every host. The title is the app's name and the subtitle
 * says only that something needs doing, so the lock screen names neither the consumable nor the device; the consumable and its count are
 * in the body (NP-PRIV-ANALYSIS-003 LOW-1). The id is per consumable, so a reminder shown again replaces itself. [environment] is for a
 * caller that has one (a test with no display); the hosts leave it out and the system's is used.
 */
@OptIn(ExperimentalResourceApi::class)
suspend fun ReminderNotifier.showReminder(reminder: ConsumableReminder) = showReminderIn(reminder, null)

@ExperimentalResourceApi
internal suspend fun ReminderNotifier.showReminderIn(reminder: ConsumableReminder, environment: ResourceEnvironment?) {
    suspend fun text(resource: StringResource, vararg args: Any): String =
        if (environment == null) getString(resource, *args) else getString(environment, resource, *args)
    val state = reminder.state
    val name = text(state.kind.displayNameRes())
    val body = if (state.isExceeded) text(Res.string.consumable_notify_exceeded, name)
    else text(Res.string.consumable_notify_low, name, state.sessionsRemaining)
    show(
        id = state.kind.name,
        title = text(Res.string.ui_app_title),
        subtitle = text(Res.string.notify_action_required),
        body = body,
    )
}
