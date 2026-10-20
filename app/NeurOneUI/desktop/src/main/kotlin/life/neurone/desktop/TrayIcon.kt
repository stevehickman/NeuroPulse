package life.neurone.desktop

import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.painter.Painter

/** A plain filled disc: the tray needs some icon and the app ships no image, so one is drawn rather than a binary added. */
object TrayIcon : Painter() {
    override val intrinsicSize = androidx.compose.ui.geometry.Size(32f, 32f)

    override fun DrawScope.onDraw() {
        drawCircle(Color(0xFF2E7D6B), radius = size.minDimension / 2f, center = Offset(size.width / 2f, size.height / 2f))
    }
}
