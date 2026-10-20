package life.neurone.desktop

import kotlin.test.Test
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class TrayCloseTest {
    // @verifies SWR-MG-03
    @Test fun closing_the_window_keeps_the_app_only_while_the_backup_is_on_and_there_is_a_tray() {
        assertTrue(keepsRunningInTray(trayAvailable = true, backupOn = true))
        assertFalse(keepsRunningInTray(trayAvailable = true, backupOn = false))
        assertFalse(keepsRunningInTray(trayAvailable = false, backupOn = true), "no tray: nothing could bring the window back")
    }
}
