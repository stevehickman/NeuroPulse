package life.neurone.shared.ui

import androidx.compose.runtime.Composable
import life.neurone.shared.resources.Res
import life.neurone.shared.resources.tray_open
import life.neurone.shared.resources.tray_quit
import life.neurone.shared.resources.tray_tooltip
import org.jetbrains.compose.resources.stringResource

/** The words of the desktop tray icon. They are here, not in the desktop host, because the string resources belong to `:shared`. */
data class TrayText(val open: String, val quit: String, val tooltip: String)

@Composable
fun trayText(): TrayText = TrayText(
    open = stringResource(Res.string.tray_open),
    quit = stringResource(Res.string.tray_quit),
    tooltip = stringResource(Res.string.tray_tooltip),
)
