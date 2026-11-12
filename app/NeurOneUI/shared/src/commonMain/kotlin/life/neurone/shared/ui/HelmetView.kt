package life.neurone.shared.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.size
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import life.neurone.core.protocol.SocketLattice
import life.neurone.shared.resources.*
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin

// The helmet view, one screen for the simulator and for the app controlling real hardware (NP-SIM-FW-001 §25, OI-SIMFW-37).
// It draws the socket lattice from `SocketLattice` (the generated, single-sourced geometry) as the unrolled-helmet map the zone
// editor already uses, and each tile's state from a `HelmetState`. 2D, tiles only: the Principal accepts 2D for now and wants 3D
// if it can be done (see the 3D notes in §7). Every state has its own outline and marker as well as its own colour
// (`HELMET_STYLE`), and the legend draws the same tile the lattice does.

/** Tags a test can address: the canvas and the one line of text that says no safety MCU is in the loop. */
const val HELMET_VIEW_TAG = "helmet_view"
const val HELMET_NOT_IN_LOOP_TAG = "helmet_not_in_loop"

private fun hexPath(cx: Float, cy: Float, radius: Float): Path = Path().apply {
    for (k in 0 until 6) {
        val a = (PI / 180.0 * (30 + 60 * k)).toFloat()
        val p = Offset(cx + radius * cos(a), cy + radius * sin(a))
        if (k == 0) moveTo(p.x, p.y) else lineTo(p.x, p.y)
    }
    close()
}

/** One tile in one state, at (cx, cy). The lattice and the legend both draw through here. */
private fun DrawScope.drawTile(cx: Float, cy: Float, radius: Float, style: TileStyle) {
    val hex = hexPath(cx, cy, radius)
    val edge = Color(0xFFE2E8F0)
    style.fillArgb?.let { drawPath(hex, Color(it)) }
    val unit = radius / 10f
    when (style.outline) {
        TileOutline.NONE -> {}
        TileOutline.THIN -> drawPath(hex, edge.copy(alpha = 0.35f), style = Stroke(width = unit * 0.6f))
        TileOutline.THICK -> drawPath(hex, Color.White, style = Stroke(width = unit * 1.6f))
        TileOutline.DOTTED -> drawPath(
            hex, edge.copy(alpha = 0.6f),
            style = Stroke(width = unit * 0.8f, pathEffect = PathEffect.dashPathEffect(floatArrayOf(unit * 1.2f, unit * 1.2f))),
        )
    }
    val ink = Color(style.markerArgb)
    val m = radius * 0.42f
    val w = unit * 1.3f
    when (style.marker) {
        TileMarker.NONE -> {}
        TileMarker.DOT -> drawCircle(ink, radius = m * 0.8f, center = Offset(cx, cy))
        TileMarker.RING -> drawCircle(ink, radius = m * 0.8f, center = Offset(cx, cy), style = Stroke(width = w))
        TileMarker.SLASHED_RING -> {
            drawCircle(ink, radius = m * 0.8f, center = Offset(cx, cy), style = Stroke(width = w))
            drawLine(ink, Offset(cx - m * 0.8f, cy + m * 0.8f), Offset(cx + m * 0.8f, cy - m * 0.8f), strokeWidth = w)
        }
        TileMarker.PAUSE -> {
            drawLine(ink, Offset(cx - m * 0.45f, cy - m), Offset(cx - m * 0.45f, cy + m), strokeWidth = w * 1.4f)
            drawLine(ink, Offset(cx + m * 0.45f, cy - m), Offset(cx + m * 0.45f, cy + m), strokeWidth = w * 1.4f)
        }
        TileMarker.SQUARE -> drawRect(ink, topLeft = Offset(cx - m * 0.8f, cy - m * 0.8f), size = Size(m * 1.6f, m * 1.6f))
        TileMarker.TRIANGLE -> drawPath(
            Path().apply {
                moveTo(cx, cy - m); lineTo(cx + m, cy + m * 0.8f); lineTo(cx - m, cy + m * 0.8f); close()
            },
            ink,
        )
        TileMarker.CROSS -> {
            drawLine(ink, Offset(cx - m, cy - m), Offset(cx + m, cy + m), strokeWidth = w)
            drawLine(ink, Offset(cx - m, cy + m), Offset(cx + m, cy - m), strokeWidth = w)
        }
        TileMarker.BANG -> {
            drawLine(ink, Offset(cx, cy - m), Offset(cx, cy + m * 0.35f), strokeWidth = w * 1.4f)
            drawCircle(ink, radius = w * 0.9f, center = Offset(cx, cy + m * 0.85f))
        }
    }
}

@Composable
fun HelmetView(state: HelmetState, modifier: Modifier = Modifier) {
    val cells = SocketLattice.cells
    val minX = cells.minOf { it.x }.toFloat()
    val maxX = cells.maxOf { it.x }.toFloat()
    val maxY = cells.maxOf { it.y }.toFloat()
    val pad = 0.9f
    val spanX = (maxX - minX) + 2 * pad
    val spanY = maxY + 2 * pad

    Column(modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        if (!state.safetyInLoop) {
            Text(
                stringResource(Res.string.helmet_not_in_loop),
                style = MaterialTheme.typography.labelMedium,
                modifier = Modifier.testTag(HELMET_NOT_IN_LOOP_TAG),
            )
        }
        Text(stringResource(Res.string.web_map_front), style = MaterialTheme.typography.labelSmall, modifier = Modifier.fillMaxWidth())
        Canvas(Modifier.fillMaxWidth().aspectRatio(spanX / spanY).testTag(HELMET_VIEW_TAG)) {
            val unit = size.width / spanX
            val radius = 0.54f * unit   // a little under the tile's circumradius (1/sqrt(3) of the pitch), so tiles keep a gap
            for (c in cells) {
                drawTile((c.x.toFloat() - minX + pad) * unit, (c.y.toFloat() + pad) * unit, radius, HELMET_STYLE.getValue(state.drawState(c.id)))
            }
        }
        Legend()
        Reasons(helmetReasonRows(state))
    }
}

/**
 * An exhaustive mapping from every reason to its string: a reason with no text does not compile, so the list cannot fall behind
 * the reasons. The keys these resolve are the ones each value names in `HelmetReason.kt` (a test holds the two together).
 */
private fun reasonLabel(r: SocketReason): StringResource = when (r) {
    is HubStatusReason -> when (r) {
        HubStatusReason.POWER_BUDGET -> Res.string.helmet_reason_hub_power_budget
        HubStatusReason.DRIVER_INTERLOCK -> Res.string.helmet_reason_hub_driver_interlock
        HubStatusReason.PHOTIC_ZONE -> Res.string.helmet_reason_hub_photic_zone
        HubStatusReason.PPX_HALT -> Res.string.helmet_reason_hub_ppx_halt
        HubStatusReason.PPX_BLIND -> Res.string.helmet_reason_hub_ppx_blind
        HubStatusReason.GOGGLE_LIFT_UNARMED -> Res.string.helmet_reason_hub_goggle_lift_unarmed
        HubStatusReason.GOGGLES_NOT_SEATED -> Res.string.helmet_reason_hub_goggles_not_seated
        HubStatusReason.EYE_NOT_OPEN -> Res.string.helmet_reason_hub_eye_not_open
        HubStatusReason.MPE_CHECK -> Res.string.helmet_reason_hub_mpe_check
        HubStatusReason.SLEEVE_NOT_AUTH -> Res.string.helmet_reason_hub_sleeve_not_auth
        HubStatusReason.CONTACT_NOT_CONFIRMED -> Res.string.helmet_reason_hub_contact_not_confirmed
        HubStatusReason.INVALID_ARGUMENT -> Res.string.helmet_reason_hub_invalid_argument
        HubStatusReason.NOT_PRESENT -> Res.string.helmet_reason_hub_not_present
        HubStatusReason.MODULE_FAULT -> Res.string.helmet_reason_hub_module_fault
        HubStatusReason.MODULE_INIT -> Res.string.helmet_reason_hub_module_init
        HubStatusReason.TIMEOUT -> Res.string.helmet_reason_hub_timeout
        HubStatusReason.SAFETY_FAULT -> Res.string.helmet_reason_hub_safety_fault
        HubStatusReason.SESSION_ACTIVE -> Res.string.helmet_reason_hub_session_active
        HubStatusReason.UNREPORTED -> Res.string.helmet_reason_hub_unreported
    }
    is WithheldReason -> when (r) {
        WithheldReason.AWAITING_SESSION_SIGNATURE -> Res.string.helmet_reason_withheld_signature
        WithheldReason.CONTACT_CHECK_FAILED -> Res.string.helmet_reason_withheld_contact
        WithheldReason.NOT_REQUESTED -> Res.string.helmet_reason_withheld_not_requested
        WithheldReason.TIER_GATE -> Res.string.helmet_reason_withheld_tier
        WithheldReason.UNDECLARED_WAVEFORM -> Res.string.helmet_reason_withheld_undeclared
        WithheldReason.MISSING_GEOMETRY -> Res.string.helmet_reason_withheld_geometry
        WithheldReason.UNREPORTED -> Res.string.helmet_reason_withheld_unreported
    }
    is CutoffReason -> when (r) {
        CutoffReason.INDEPENDENT_WATCHDOG -> Res.string.helmet_reason_cutoff_independent_watchdog
        CutoffReason.SESSION_SIGNATURE -> Res.string.helmet_reason_cutoff_session_signature
        CutoffReason.MCU_FAULT -> Res.string.helmet_reason_cutoff_mcu_fault
        CutoffReason.THERMAL -> Res.string.helmet_reason_cutoff_thermal
        CutoffReason.CHARGE -> Res.string.helmet_reason_cutoff_charge
        CutoffReason.CARDIAC -> Res.string.helmet_reason_cutoff_cardiac
        CutoffReason.HEARTBEAT_LOST -> Res.string.helmet_reason_cutoff_heartbeat
        CutoffReason.CUTOFF_ONLY -> Res.string.helmet_reason_cutoff_only
        CutoffReason.UNREPORTED -> Res.string.helmet_reason_cutoff_unreported
    }
}

/** Tag a test can address: the reasons list, present only when some socket has a reason to give. */
const val HELMET_REASONS_TAG = "helmet_reasons"

/** The reason beside each state: the state's own tile, its legend text, the reason in words and how many sockets share it. */
@Composable
private fun Reasons(rows: List<HelmetReasonRow>) {
    if (rows.isEmpty()) return
    Column(Modifier.testTag(HELMET_REASONS_TAG), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Text(stringResource(Res.string.helmet_reasons_title), style = MaterialTheme.typography.labelMedium)
        for (row in rows) {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
                Canvas(Modifier.size(22.dp)) { drawTile(size.width / 2f, size.height / 2f, size.width * 0.46f, HELMET_STYLE.getValue(row.state)) }
                Column {
                    Text(stringResource(legendLabel(row.state)), style = MaterialTheme.typography.labelSmall)
                    Text(stringResource(reasonLabel(row.reason)), style = MaterialTheme.typography.bodySmall)
                    Text(stringResource(Res.string.helmet_reason_sockets, row.count.toString()), style = MaterialTheme.typography.labelSmall)
                }
            }
        }
    }
}

/** An exhaustive `when`: a state with no legend text does not compile, so the legend cannot fall behind the states. */
private fun legendLabel(s: SocketDrawState): StringResource = when (s) {
    SocketDrawState.ABSENT -> Res.string.helmet_state_absent
    SocketDrawState.IDLE -> Res.string.helmet_state_idle
    SocketDrawState.REFUSED -> Res.string.helmet_state_refused
    SocketDrawState.WRITE_FAILED -> Res.string.helmet_state_write_failed
    SocketDrawState.COMMANDED -> Res.string.helmet_state_commanded
    SocketDrawState.DRIVEN -> Res.string.helmet_state_driven
    SocketDrawState.DRIVEN_UNGATED -> Res.string.helmet_state_driven_ungated
    SocketDrawState.WITHHELD -> Res.string.helmet_state_withheld
    SocketDrawState.CUT_OFF -> Res.string.helmet_state_cut_off
    SocketDrawState.FAULT -> Res.string.helmet_state_fault
}

@Composable
private fun Legend() {
    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        for (s in SocketDrawState.entries) {
            val label = legendLabel(s)
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
                Canvas(Modifier.size(22.dp)) { drawTile(size.width / 2f, size.height / 2f, size.width * 0.46f, HELMET_STYLE.getValue(s)) }
                Text(stringResource(label), style = MaterialTheme.typography.labelSmall)
            }
        }
    }
}
