package life.neurone.shared.ui

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ExtendedFloatingActionButton
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.DialogProperties
import life.neurone.core.models.SessionProgress
import life.neurone.core.models.SessionStatus
import life.neurone.shared.AppServices
import life.neurone.shared.resources.*
import org.jetbrains.compose.resources.stringResource

/**
 * The panic control (SWR-SS-09): one button for whoever administers a session, the wearer included, independent of every
 * modality. It sits in the lower left corner of the screen, in the same place on every screen and in every state (the
 * principal, 2026-11-06), is not mirrored in a right-to-left locale, and is offered whenever a session is running or paused.
 *
 * Pressing it asks the helmet to cut everything at once and hold the session paused; the second step is a modal dialog
 * centred on the screen, with no default and no dismissal by a tap outside or the back key: Stop ends the session, Resume
 * restarts it where it was held (a full start-up, as a resume always is). Resume is offered only once the helmet has said
 * the session is held, so the person is never told it is held when it is not.
 *
 * Place it once, over the whole content. [PanicControlLayer] does that.
 */
@Composable
fun PanicControlLayer(services: AppServices, modifier: Modifier = Modifier) {
    val gatt = services.gattManager
    val session by gatt.session.collectAsState()
    val progress by gatt.progress.collectAsState()
    var asking by remember { mutableStateOf(false) }
    val offered = panicControlOffered(session.status)
    // The session ended under the question (the helmet stopped it, or it completed): there is nothing left to ask.
    LaunchedEffect(offered) { if (!offered) asking = false }
    if (!offered) return

    // The corner is the corner of the screen: it does not follow the reading direction.
    val reading = LocalLayoutDirection.current
    CompositionLocalProvider(LocalLayoutDirection provides LayoutDirection.Ltr) {
        Box(modifier.fillMaxSize()) {
            Box(Modifier.align(Alignment.BottomStart).padding(16.dp)) {
                CompositionLocalProvider(LocalLayoutDirection provides reading) {
                    val label = stringResource(Res.string.panic_button)
                    val description = stringResource(Res.string.panic_button_description)
                    ExtendedFloatingActionButton(
                        onClick = { gatt.requestSessionPanic(); asking = true },
                        containerColor = MaterialTheme.colorScheme.error,
                        contentColor = MaterialTheme.colorScheme.onError,
                        icon = { Icon(Icons.Filled.Warning, contentDescription = null) },
                        text = { Text(label) },
                        modifier = Modifier.semantics { contentDescription = description },
                    )
                }
            }
        }
    }

    if (asking) {
        val held = panicHeld(progress)
        AlertDialog(
            // No default and no dismissal: the person chooses, so they know what is going to happen.
            onDismissRequest = {},
            properties = DialogProperties(dismissOnBackPress = false, dismissOnClickOutside = false),
            title = { Text(stringResource(if (held) Res.string.panic_dialog_title_held else Res.string.panic_dialog_title_waiting)) },
            text = {
                Column {
                    Text(stringResource(if (held) Res.string.panic_dialog_body_held else Res.string.panic_dialog_body_waiting))
                    Text(stringResource(Res.string.panic_dialog_cable_note), style = MaterialTheme.typography.bodySmall,
                        modifier = Modifier.padding(top = 8.dp))
                }
            },
            confirmButton = {
                Button(
                    onClick = { gatt.requestSessionStop(); asking = false },
                    colors = androidx.compose.material3.ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.error),
                ) { Text(stringResource(Res.string.panic_stop)) }
            },
            dismissButton = {
                OutlinedButton(
                    onClick = { gatt.requestSessionResume(); asking = false },
                    enabled = held,
                ) { Text(stringResource(Res.string.panic_resume)) }
            },
        )
    }
}

/** The panic control is offered while a session is running or paused, and only then (there is nothing to cut otherwise). */
fun panicControlOffered(status: SessionStatus): Boolean =
    status == SessionStatus.RUNNING || status == SessionStatus.PAUSED

/** The helmet has said the session is held; until it has, Resume is not offered and the dialog says it is waiting. */
fun panicHeld(progress: SessionProgress?): Boolean = progress?.isPaused == true
