package life.neurone.shared.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import life.neurone.core.protocol.SocketLattice
import life.neurone.shared.resources.*
import org.jetbrains.compose.resources.stringResource
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin

// The helmet view, one screen for the simulator and for the app controlling real hardware (NP-SIM-FW-001 §25, OI-SIMFW-37).
// It draws the socket lattice from `SocketLattice` (the generated, single-sourced geometry) as the unrolled-helmet map the zone
// editor already uses, and each tile's state from a `HelmetState`. First slice: 2D, tiles only; see the open items in §7.

private val STATE_COLOR = mapOf(
    SocketDrawState.ABSENT to Color(0xFF334155),
    SocketDrawState.IDLE to Color(0xFF94A3B8),
    SocketDrawState.COMMANDED to Color(0xFFF59E0B),
    SocketDrawState.DRIVEN to Color(0xFFEF4444),
    SocketDrawState.WITHHELD to Color(0xFF7C5E10),
    SocketDrawState.FAULT to Color(0xFFD946EF),
)

/** Tags a test can address: the canvas and the one line of text that says no safety MCU is in the loop. */
const val HELMET_VIEW_TAG = "helmet_view"
const val HELMET_NOT_IN_LOOP_TAG = "helmet_not_in_loop"

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
                val cx = (c.x.toFloat() - minX + pad) * unit
                val cy = (c.y.toFloat() + pad) * unit
                val hex = Path().apply {
                    for (k in 0 until 6) {
                        val a = (PI / 180.0 * (30 + 60 * k)).toFloat()
                        val p = Offset(cx + radius * cos(a), cy + radius * sin(a))
                        if (k == 0) moveTo(p.x, p.y) else lineTo(p.x, p.y)
                    }
                    close()
                }
                val ds = state.drawState(c.id)
                drawPath(hex, STATE_COLOR.getValue(ds))
                // A tile the hub commanded without the MCU's line enabled is outlined, so it cannot read as delivering.
                if (ds == SocketDrawState.COMMANDED || ds == SocketDrawState.WITHHELD) drawPath(hex, Color.White, style = Stroke(width = 2f))
            }
        }
        Legend()
    }
}

@Composable
private fun Legend() {
    val rows = listOf(
        SocketDrawState.ABSENT to Res.string.helmet_state_absent,
        SocketDrawState.IDLE to Res.string.helmet_state_idle,
        SocketDrawState.COMMANDED to Res.string.helmet_state_commanded,
        SocketDrawState.DRIVEN to Res.string.helmet_state_driven,
        SocketDrawState.WITHHELD to Res.string.helmet_state_withheld,
        SocketDrawState.FAULT to Res.string.helmet_state_fault,
    )
    Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
        for ((s, label) in rows) {
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                Canvas(Modifier.padding(top = 3.dp).aspectRatio(1f).fillMaxWidth(0.03f)) { drawRect(STATE_COLOR.getValue(s)) }
                Text(stringResource(label), style = MaterialTheme.typography.labelSmall)
            }
        }
    }
}
