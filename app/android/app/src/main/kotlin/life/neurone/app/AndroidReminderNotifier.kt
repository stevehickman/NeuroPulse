package life.neurone.app

import android.Manifest
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import life.neurone.app.ui.MainActivity
import life.neurone.shared.ReminderNotifier

/**
 * The consumable reminders as Android notifications (`REQ-NOTIFY-01`). The shared app words each one and keeps the title and subtitle
 * generic; this shows it. On the lock screen the person sees only a **public version** (the app's name and "action required"), and the
 * consumable and its count appear in the body once the phone is unlocked (visibility private), which is Android's form of the same
 * two-layer rule the Swift app follows (`NP-PRIV-ANALYSIS-003` LOW-1).
 *
 * Permission to notify (Android 13 and later) is asked when the Consumables screen first appears, never at launch (LOW-2): the
 * activity registers the system's dialog and hands it here as [askPermission]. Without the permission a reminder is simply not shown;
 * the Consumables screen still lists it.
 */
class AndroidReminderNotifier(private val context: Context) : ReminderNotifier {
    /** Set by the activity: shows the system's permission dialog. */
    var askPermission: () -> Unit = {}

    private val manager: NotificationManager get() = context.getSystemService(NotificationManager::class.java)

    /** The app's own icon when it has one; a notification without any icon is refused by the system, so there is a plain one to fall back on. */
    private val icon: Int get() = context.applicationInfo.icon.takeIf { it != 0 } ?: android.R.drawable.ic_dialog_info

    private val allowed: Boolean
        get() = Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU ||
            context.checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) == PackageManager.PERMISSION_GRANTED

    override fun requestPermission() {
        if (!allowed) askPermission()
    }

    override fun show(id: String, title: String, subtitle: String, body: String) {
        if (!allowed) return
        manager.createNotificationChannel(
            NotificationChannel(CHANNEL, context.getString(R.string.notify_channel_reminders), NotificationManager.IMPORTANCE_DEFAULT),
        )
        // What the lock screen shows: no consumable and no device type.
        val public = Notification.Builder(context, CHANNEL)
            .setSmallIcon(icon)
            .setContentTitle(title)
            .setContentText(subtitle)
            .build()
        val open = PendingIntent.getActivity(
            context, 0,
            Intent(context, MainActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP or Intent.FLAG_ACTIVITY_CLEAR_TOP),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
        )
        val notification = Notification.Builder(context, CHANNEL)
            .setSmallIcon(icon)
            .setContentTitle(title)
            .setSubText(subtitle)
            .setContentText(body)
            .setVisibility(Notification.VISIBILITY_PRIVATE)
            .setPublicVersion(public)
            .setContentIntent(open)
            .setAutoCancel(true)
            .build()
        // One id per consumable, so a reminder shown again replaces the one before it.
        manager.notify(id.hashCode(), notification)
    }

    private companion object {
        const val CHANNEL = "consumable_reminders"
    }
}
