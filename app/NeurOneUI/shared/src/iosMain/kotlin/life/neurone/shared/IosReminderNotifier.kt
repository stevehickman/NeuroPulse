package life.neurone.shared

import platform.UserNotifications.UNAuthorizationOptionAlert
import platform.UserNotifications.UNAuthorizationOptionBadge
import platform.UserNotifications.UNAuthorizationOptionSound
import platform.UserNotifications.UNMutableNotificationContent
import platform.UserNotifications.UNNotificationRequest
import platform.UserNotifications.UNNotificationSound
import platform.UserNotifications.UNTimeIntervalNotificationTrigger
import platform.UserNotifications.UNUserNotificationCenter

/**
 * The consumable reminders as iOS local notifications, as the Swift app shows them. The shared app words them and keeps the lock
 * screen generic (the title is the app's name, the subtitle only says action is needed, the detail is in the body); this only hands
 * them to the system. A notification is shown a second after it is added, and one with the same id replaces the one before it.
 */
class IosReminderNotifier : ReminderNotifier {
    override fun requestPermission() {
        UNUserNotificationCenter.currentNotificationCenter().requestAuthorizationWithOptions(
            UNAuthorizationOptionAlert or UNAuthorizationOptionBadge or UNAuthorizationOptionSound,
        ) { _, _ -> }
    }

    override fun show(id: String, title: String, subtitle: String, body: String) {
        val content = UNMutableNotificationContent()
        content.setTitle(title)
        content.setSubtitle(subtitle)
        content.setBody(body)
        content.setSound(UNNotificationSound.defaultSound)
        val trigger = UNTimeIntervalNotificationTrigger.triggerWithTimeInterval(1.0, repeats = false)
        val request = UNNotificationRequest.requestWithIdentifier(id, content = content, trigger = trigger)
        UNUserNotificationCenter.currentNotificationCenter().addNotificationRequest(request, withCompletionHandler = null)
    }
}
