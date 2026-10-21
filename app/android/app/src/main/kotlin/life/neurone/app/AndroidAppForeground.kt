package life.neurone.app

import android.app.Activity
import android.app.Application
import android.os.Bundle
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow

/**
 * Whether any of the app's activities is on screen, for the UHDR presence lock's background timeout (`REQ-PRES-03`). Counting started
 * activities needs no lifecycle library: the app is in the foreground from the first activity's start to the last one's stop, and a
 * rotation (stop then start of a new instance) is not a trip to the background because the new one starts first.
 */
class AndroidAppForeground : Application.ActivityLifecycleCallbacks {
    private var started = 0
    private val state = MutableStateFlow(false)
    val inForeground: StateFlow<Boolean> = state

    fun register(app: Application) = app.registerActivityLifecycleCallbacks(this)

    override fun onActivityStarted(activity: Activity) {
        started++
        state.value = true
    }

    override fun onActivityStopped(activity: Activity) {
        started = (started - 1).coerceAtLeast(0)
        if (started == 0 && !activity.isChangingConfigurations) state.value = false
    }

    override fun onActivityCreated(activity: Activity, savedInstanceState: Bundle?) {}
    override fun onActivityResumed(activity: Activity) {}
    override fun onActivityPaused(activity: Activity) {}
    override fun onActivitySaveInstanceState(activity: Activity, outState: Bundle) {}
    override fun onActivityDestroyed(activity: Activity) {}
}
