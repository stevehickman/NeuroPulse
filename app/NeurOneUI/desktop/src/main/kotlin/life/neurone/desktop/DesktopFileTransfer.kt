package life.neurone.desktop

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import life.neurone.shared.migration.FileTransfer
import java.awt.FileDialog
import java.awt.Frame
import java.io.File

/**
 * The operating system's own save and open dialogs on macOS, Windows and Linux (AWT's native [FileDialog]), for a file the person
 * carries: a backup, or their protocols as a `.npps` file. The dialog blocks, so it runs off the UI thread; a cancelled dialog or an
 * unreadable file gives false or null, never an exception.
 */
class DesktopFileTransfer : FileTransfer {

    override suspend fun save(name: String, bytes: ByteArray): Boolean = withContext(Dispatchers.IO) {
        val dialog = FileDialog(null as Frame?, name, FileDialog.SAVE).apply { file = name }
        dialog.isVisible = true
        val chosen = chosen(dialog) ?: return@withContext false
        runCatching { chosen.writeBytes(bytes) }.isSuccess
    }

    override suspend fun open(): ByteArray? = withContext(Dispatchers.IO) {
        val dialog = FileDialog(null as Frame?, "", FileDialog.LOAD)
        dialog.isVisible = true
        chosen(dialog)?.let { runCatching { it.readBytes() }.getOrNull() }
    }

    private fun chosen(dialog: FileDialog): File? {
        val file = dialog.file ?: return null
        val dir = dialog.directory ?: return null
        return File(dir, file)
    }
}
