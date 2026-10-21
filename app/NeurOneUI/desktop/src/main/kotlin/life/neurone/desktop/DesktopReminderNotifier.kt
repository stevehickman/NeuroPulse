package life.neurone.desktop

import life.neurone.shared.ReminderNotifier

/**
 * The consumable reminders on the desktop (`REQ-NOTIFY-01`): a balloon from the tray icon while the app runs in the tray. A desktop has no
 * lock-screen split between a public and a private version, and anyone who can see the screen sees the balloon, so it carries **only the
 * title and the subtitle**, the app's name and "action required", and never the consumable or its count. The detail is in the app: the tray
 * icon opens the window, and the Consumables screen lists every reminder. (The Swift app and the phones put the detail in a body shown
 * after unlock; there is no such moment here.)
 *
 * The tray exists only inside the running application, so the host attaches it ([attach]) once the tray is composed. A reminder that
 * arrives before that, or on a system with no tray, is not shown; the Consumables screen still lists it. Permission to notify is the
 * operating system's, asked of the person if at all when the first balloon is shown; the app does not ask.
 */
class DesktopReminderNotifier : ReminderNotifier {
    private var sink: ((title: String, message: String) -> Unit)? = null

    /** Where a balloon goes: the tray's. Null detaches it. */
    fun attach(show: ((title: String, message: String) -> Unit)?) { sink = show }

    override fun requestPermission() {}

    override fun show(id: String, title: String, subtitle: String, body: String) {
        sink?.invoke(title, subtitle)
    }
}
