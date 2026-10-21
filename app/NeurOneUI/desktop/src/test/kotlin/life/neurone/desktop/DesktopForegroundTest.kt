package life.neurone.desktop

import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.shared.ble.UnavailableBleCentral
import kotlin.test.Test
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** The desktop host tells the presence lock when its window leaves and returns to the front. */
class DesktopForegroundTest {
    // @verifies REQ-PRES-03
    @Test
    fun theWindowsFocusIsTheAppsForeground() {
        val platform = DesktopPlatformServices(keyValueStore = InMemoryKeyValueStore(), bleCentral = UnavailableBleCentral())
        assertTrue(platform.appInForeground.value, "a new run starts in front")
        platform.setWindowFocused(false)
        assertFalse(platform.appInForeground.value)
        platform.setWindowFocused(true)
        assertTrue(platform.appInForeground.value)
    }
}
