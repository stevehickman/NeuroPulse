package life.neurone.shared

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import platform.Foundation.NSNotificationCenter
import platform.Foundation.NSOperationQueue
import platform.UIKit.UIApplication
import platform.UIKit.UIApplicationDidEnterBackgroundNotification
import platform.UIKit.UIApplicationState
import platform.UIKit.UIApplicationWillEnterForegroundNotification

/**
 * Whether the app is in the foreground, for the UHDR presence lock's background timeout (`REQ-PRES-03`). It follows
 * did-enter-background and will-enter-foreground, not will-resign-active: the system's own Face ID and passcode prompt makes the app
 * resign active, and treating that as leaving the app would start the timeout in the middle of unlocking.
 */
class IosAppForeground {
    private val state = MutableStateFlow(UIApplication.sharedApplication.applicationState != UIApplicationState.UIApplicationStateBackground)
    val inForeground: StateFlow<Boolean> = state

    init {
        val center = NSNotificationCenter.defaultCenter
        center.addObserverForName(UIApplicationDidEnterBackgroundNotification, null, NSOperationQueue.mainQueue) { state.value = false }
        center.addObserverForName(UIApplicationWillEnterForegroundNotification, null, NSOperationQueue.mainQueue) { state.value = true }
    }
}
