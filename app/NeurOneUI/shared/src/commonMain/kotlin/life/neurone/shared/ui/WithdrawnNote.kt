package life.neurone.shared.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import life.neurone.shared.resources.Res
import life.neurone.shared.resources.dashboard_withdrawn_on_format
import org.jetbrains.compose.resources.stringResource

/**
 * The day a consent was withdrawn, for a consent that is off now, or null (`SWR-MG-04`).
 *
 * [withdrawnAt] is the store's UTC instant (`yyyy-MM-ddTHH:mm:ssZ`) and only the store sets it
 * (`REQ-CONSENT-04`); a screen reads it and never writes it. A consent that is on shows nothing,
 * because a later re-grant leaves the old withdrawal in the record but it is not the current state.
 * "Never asked" has no time and shows nothing, so it stays distinguishable from "said stop".
 */
fun withdrawnDay(granted: Boolean, withdrawnAt: String?): String? =
    if (granted || withdrawnAt.isNullOrBlank()) null else withdrawnAt.substringBefore('T')

/** One line under a consent control that was withdrawn; draws nothing when [day] is null. */
@Composable
fun WithdrawnNote(day: String?) {
    if (day == null) return
    Text(
        stringResource(Res.string.dashboard_withdrawn_on_format, day),
        style = MaterialTheme.typography.labelSmall,
    )
}
