package life.neurone.app

import android.app.Activity
import android.app.Application
import android.os.Bundle
import android.view.WindowManager
import life.neurone.shared.ScreenCaptureBlocker

/**
 * Keeps the app's windows out of screenshots, screen recording and the recents thumbnail while the shared app asks (`REQ-SW03-36` (c)):
 * `FLAG_SECURE` on every live activity's window, and on each one created while it is asked, so a rotation (a new activity) keeps it.
 * The shared screens ask through [life.neurone.shared.KeepOffScreenCapture]; the flag is set and cleared on the main thread, where the
 * composition's effects run.
 */
class AndroidScreenCaptureBlocker : Application.ActivityLifecycleCallbacks, ScreenCaptureBlocker {
    private val live = mutableSetOf<Activity>()
    private var blocked = false

    fun register(app: Application) = app.registerActivityLifecycleCallbacks(this)

    override fun setBlocked(blocked: Boolean) {
        this.blocked = blocked
        live.forEach { apply(it) }
    }

    private fun apply(activity: Activity) {
        if (blocked) {
            activity.window.addFlags(WindowManager.LayoutParams.FLAG_SECURE)
        } else {
            activity.window.clearFlags(WindowManager.LayoutParams.FLAG_SECURE)
        }
    }

    override fun onActivityCreated(activity: Activity, savedInstanceState: Bundle?) {
        live += activity
        apply(activity)
    }

    override fun onActivityDestroyed(activity: Activity) {
        live -= activity
    }

    override fun onActivityStarted(activity: Activity) {}
    override fun onActivityResumed(activity: Activity) {}
    override fun onActivityPaused(activity: Activity) {}
    override fun onActivityStopped(activity: Activity) {}
    override fun onActivitySaveInstanceState(activity: Activity, outState: Bundle) {}
}
