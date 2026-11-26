package life.neurone.shared

import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.staticCompositionLocalOf

// Keeping a screen out of screenshots, screen recording and the app switcher (REQ-SW03-36 (b), (c); NP-PRIV-ANALYSIS-006 F-12).
// The screens are written once, here; whether the window can be kept out of capture is the host's. A screen that shows the recovery
// code, a backup, consent choices or the session history calls KeepOffScreenCapture, and the host's window is kept out of capture
// for as long as any such screen is on show.

/**
 * The host's switch for keeping the app's window out of screenshots, screen recording and the app switcher's thumbnail. Android sets
 * `FLAG_SECURE`. A host with no such switch supplies none, and the screens still draw.
 */
fun interface ScreenCaptureBlocker {
    fun setBlocked(blocked: Boolean)
}

/**
 * Counts the screens that ask for the window to be kept out of capture, and turns the host's switch on with the first and off after
 * the last. Counting, not a flag, because the next screen is composed before the last one is disposed: with a flag, the screen that
 * leaves would turn the switch off under the one that arrives.
 */
class ScreenCaptureGuard(private val blocker: ScreenCaptureBlocker?) {
    private var holds = 0

    /** Whether some screen is holding the window out of capture now. */
    val blocked: Boolean get() = holds > 0

    /** Hold the window out of capture until the returned action runs. Running it again does nothing. */
    fun hold(): () -> Unit {
        if (holds++ == 0) blocker?.setBlocked(true)
        var released = false
        return {
            if (!released) {
                released = true
                if (--holds == 0) blocker?.setBlocked(false)
            }
        }
    }
}

/** The guard of the app on show. The default guard has no host switch, for a screen drawn outside the app (a test, a preview). */
val LocalScreenCaptureGuard = staticCompositionLocalOf { ScreenCaptureGuard(null) }

/** Keep the window out of screenshots, screen recording and the app switcher for as long as the calling screen is on show. */
@Composable
fun KeepOffScreenCapture() {
    val guard = LocalScreenCaptureGuard.current
    DisposableEffect(guard) {
        val release = guard.hold()
        onDispose { release() }
    }
}
