package life.neurone.shared.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import life.neurone.core.protocol.NPProtocolLibrary
import life.neurone.core.protocol.NPSocketSet
import life.neurone.core.protocol.NPZoneDefinition
import life.neurone.core.protocol.NPZoneGrouping
import life.neurone.core.protocol.NPZoneRegistry
import life.neurone.core.protocol.SocketLattice
import life.neurone.shared.resources.*
import org.jetbrains.compose.resources.stringResource
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin
import kotlin.math.sqrt

// Port of the web ZonePane / ZoneEditor / SocketPicker (HelmetConfig, NP-CFG-UI-001): the zones in force, and a way to
// define one as a name plus a socket set, typed or tapped on the unrolled-helmet map. Unlike the web editor, which only
// copies the NPPS, a zone saved here is kept with the user's own scripts, so it is real at once (NP-NPPS-REF-001 §8).
// What is fitted in each socket is not shown: no hub characteristic reports the module inventory yet.

/** Qualitative palette, assigned to zones in name order; it cycles, and the legend disambiguates by name. */
private val ZONE_PALETTE = listOf(
    0xFF3B82F6, 0xFFF59E0B, 0xFF10B981, 0xFF8B5CF6, 0xFFEF4444, 0xFF06B6D4,
    0xFFEC4899, 0xFF84CC16, 0xFFF97316, 0xFF6366F1, 0xFF14B8A6, 0xFFA855F7,
).map { Color(it) }
private val UNZONED = Color(0xFF94A3B8)

@Composable
fun ZoneEditorScreen(
    library: NPProtocolLibrary,
    onDone: () -> Unit,
    modifier: Modifier = Modifier,
) {
    // The library holds the zones; this counter re-reads them after a save or a delete.
    var revision by remember { mutableStateOf(0) }
    var defining by remember { mutableStateOf(false) }
    var blocked by remember { mutableStateOf<List<String>?>(null) }

    Column(modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp)) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
            TextButton(onClick = onDone) { Text(stringResource(Res.string.consent_done_button)) }
            Text(stringResource(Res.string.web_pane_zones), style = MaterialTheme.typography.titleMedium)
            Spacer(Modifier.height(0.dp).padding(horizontal = 32.dp))
        }
        Text(stringResource(Res.string.zones_note), style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(12.dp))

        androidx.compose.runtime.key(revision) {
            val zones = NPZoneRegistry.zoneNames.mapNotNull { NPZoneRegistry.zones[it] }
            val mine = library.userZones.map { it.name }.toSet()
            zones.forEach { zone ->
                Row(Modifier.fillMaxWidth().padding(vertical = 2.dp), verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        Text(zone.name, style = MaterialTheme.typography.bodyMedium)
                        Text(
                            pluralString(Res.string.zones_socket_count_one, Res.string.zones_socket_count_other, zone.sockets.size) +
                                if (zone.name in mine) " · " + stringResource(Res.string.zones_user_defined_tag) else "",
                            style = MaterialTheme.typography.bodySmall,
                        )
                    }
                    if (zone.name in mine) {
                        TextButton(onClick = {
                            when (val r = library.deleteUserZone(zone.name)) {
                                is NPProtocolLibrary.ZoneDeleteResult.InUse -> blocked = r.protocols
                                else -> { blocked = null; revision++ }
                            }
                        }) { Text(stringResource(Res.string.ui_delete), color = MaterialTheme.colorScheme.error) }
                    }
                }
            }
        }
        blocked?.let {
            Text(
                stringResource(Res.string.zones_used_by, it.joinToString(", ")),
                color = MaterialTheme.colorScheme.error, style = MaterialTheme.typography.bodySmall,
            )
        }

        Spacer(Modifier.height(12.dp))
        OutlinedButton(onClick = { defining = !defining }) {
            Text(stringResource(if (defining) Res.string.common_cancel else Res.string.web_define_new_zone))
        }
        if (defining) {
            Spacer(Modifier.height(12.dp))
            NewZoneForm(library, onSaved = { defining = false; blocked = null; revision++ })
        }
    }
}

@Composable
private fun NewZoneForm(library: NPProtocolLibrary, onSaved: () -> Unit) {
    var name by remember { mutableStateOf("") }
    var typed by remember { mutableStateOf("") }
    var selected by remember { mutableStateOf<Set<Int>>(emptySet()) }

    val parsed = NPSocketSet.parse(typed)
    val sockets = selected.sorted()
    val trimmed = name.trim()
    val taken = trimmed.isNotEmpty() && (NPZoneRegistry.zones.containsKey(trimmed))
    val canSave = trimmed.isNotEmpty() && sockets.isNotEmpty() && !taken

    OutlinedTextField(
        value = name, onValueChange = { name = it }, singleLine = true,
        label = { Text(stringResource(Res.string.web_zone_name_label)) },
        placeholder = { Text(stringResource(Res.string.web_zone_name_placeholder)) },
        modifier = Modifier.fillMaxWidth(),
    )
    if (taken) Text(stringResource(Res.string.web_zone_name_taken, trimmed), color = MaterialTheme.colorScheme.error, style = MaterialTheme.typography.bodySmall)

    Spacer(Modifier.height(8.dp))
    OutlinedTextField(
        value = typed,
        onValueChange = { typed = it; selected = NPSocketSet.parse(it).sockets.toSet() },
        label = { Text(stringResource(Res.string.web_socket_addresses_label)) },
        placeholder = { Text(stringResource(Res.string.web_socket_addresses_placeholder)) },
        modifier = Modifier.fillMaxWidth(),
    )
    if (parsed.invalid.isNotEmpty()) {
        Text(
            stringResource(Res.string.web_invalid_sockets, SocketLattice.rangeLabel, parsed.invalid.joinToString(", ")),
            color = MaterialTheme.colorScheme.error, style = MaterialTheme.typography.bodySmall,
        )
    }

    Spacer(Modifier.height(8.dp))
    SocketMap(
        selected = selected,
        zones = NPZoneRegistry.zoneNames.mapNotNull { NPZoneRegistry.zones[it] },
        onToggle = { id ->
            // One set shared by the typed list and the map, so the two views stay in step.
            selected = if (id in selected) selected - id else selected + id
            typed = selected.sorted().joinToString(", ")
        },
    )
    Text(
        pluralString(Res.string.web_sockets_selected_one, Res.string.web_sockets_selected_other, sockets.size),
        style = MaterialTheme.typography.bodySmall,
    )

    Spacer(Modifier.height(8.dp))
    Button(
        enabled = canSave,
        onClick = { if (library.addUserZone(trimmed, sockets) == NPProtocolLibrary.ZoneSaveResult.Saved) onSaved() },
    ) { Text(stringResource(Res.string.zones_save_zone)) }
}

// Unit-lattice to canvas: pointy-top hex packing, horizontal pitch 1 unit and vertical pitch sqrt(3)/2, as the generated lattice
// carries it. Positions are provisional (shell CAD), so selection never depends on them.
private const val HEX_CIRCUMRADIUS = 0.5774f

@Composable
private fun SocketMap(selected: Set<Int>, zones: List<NPZoneDefinition>, onToggle: (Int) -> Unit) {
    val cells = SocketLattice.cells
    val minX = cells.minOf { it.x }.toFloat()
    val maxX = cells.maxOf { it.x }.toFloat()
    val maxY = cells.maxOf { it.y }.toFloat()
    val pad = 0.9f
    val spanX = (maxX - minX) + 2 * pad
    val spanY = maxY + 2 * pad
    val grouping = remember(zones) { NPZoneGrouping(zones) }

    Text(stringResource(Res.string.web_map_front), style = MaterialTheme.typography.labelSmall, modifier = Modifier.fillMaxWidth())
    Canvas(
        Modifier.fillMaxWidth().aspectRatio(spanX / spanY).testTag("socket_map")
            .pointerInput(selected, cells) {
                detectTapGestures { tap ->
                    val unit = size.width / spanX
                    // The nearest tile centre within one tile width.
                    val hit = cells.minByOrNull { c ->
                        val cx = (c.x.toFloat() - minX + pad) * unit
                        val cy = (c.y.toFloat() + pad) * unit
                        (tap.x - cx) * (tap.x - cx) + (tap.y - cy) * (tap.y - cy)
                    }
                    if (hit != null) {
                        val cx = (hit.x.toFloat() - minX + pad) * unit
                        val cy = (hit.y.toFloat() + pad) * unit
                        val d = sqrt((tap.x - cx) * (tap.x - cx) + (tap.y - cy) * (tap.y - cy))
                        if (d <= 0.55f * unit) onToggle(hit.id)
                    }
                }
            },
    ) {
        val unit = size.width / spanX
        cells.forEach { c ->
            val cx = (c.x.toFloat() - minX + pad) * unit
            val cy = (c.y.toFloat() + pad) * unit
            val zone = grouping.primaryOf[c.id]
            val colour = zone?.let { ZONE_PALETTE[grouping.colorIndexOf.getValue(it) % ZONE_PALETTE.size] } ?: UNZONED
            val isSelected = c.id in selected
            val path = Path().apply {
                for (i in 0 until 6) {
                    val a = (PI / 180.0) * (60 * i + 30)
                    val px = cx + HEX_CIRCUMRADIUS * unit * 0.92f * cos(a).toFloat()
                    val py = cy + HEX_CIRCUMRADIUS * unit * 0.92f * sin(a).toFloat()
                    if (i == 0) moveTo(px, py) else lineTo(px, py)
                }
                close()
            }
            drawPath(path, colour.copy(alpha = if (isSelected) 0.85f else 0.2f))
            drawPath(path, colour, style = Stroke(width = if (isSelected) 4f else 2f))
        }
        // Dashed midline at x = 0.
        val mx = (0f - minX + pad) * unit
        drawLine(
            Color.Gray, Offset(mx, 0f), Offset(mx, size.height), strokeWidth = 1.5f,
            pathEffect = androidx.compose.ui.graphics.PathEffect.dashPathEffect(floatArrayOf(10f, 10f)),
        )
    }
    Text(stringResource(Res.string.web_map_back), style = MaterialTheme.typography.labelSmall, modifier = Modifier.fillMaxWidth())
    Text(stringResource(Res.string.web_socket_legend_note), style = MaterialTheme.typography.bodySmall)
}
