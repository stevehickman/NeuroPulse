package life.neurone.desktop

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import life.neurone.core.migration.DirectoryObjectStore
import life.neurone.core.migration.ObjectStoreRoute
import life.neurone.shared.MigrationPlace
import life.neurone.shared.migration.FolderPlaces
import life.neurone.shared.migration.FolderProblem
import java.awt.GraphicsEnvironment
import java.io.File
import java.nio.file.Path
import javax.swing.JFileChooser
import javax.swing.SwingUtilities

/**
 * Where the desktop apps keep a backup, and the person's choice of it (issue #569 package 6zz). The choice, the checks and the
 * saved setting are common (`BackupFolderModel`); this supplies the three things only a desktop has: the folder dialog, the
 * object store over a directory (the one Android shares, `DirectoryObjectStore`), and the layout of the app's own data folder.
 *
 * The default is `backups` inside the data folder. A folder the person chooses may be anywhere a program can write: a USB drive, a
 * shared folder, another disk. It must already exist, because a drive that is not plugged in is a path that does not exist, and
 * making it would put the backup on the computer's own disk under the drive's name.
 */
class DesktopFolderPlaces(
    private val dataDirectory: File,
    private val chooser: (File) -> File? = ::swingFolderChooser,
) : FolderPlaces {

    override val defaultFolder: String = dataDirectory.resolve(BACKUP_FOLDER).path

    override suspend fun choose(current: String): String? = withContext(Dispatchers.IO) { chooser(File(current))?.path }

    override fun placeAt(folder: String): MigrationPlace {
        val directory = File(folder)
        require(folder == defaultFolder || directory.isDirectory) { "'$folder' is not a folder that exists" }
        return MigrationPlace(directory.path, DirectoryObjectStore(directory.toPath()), ObjectStoreRoute("local-folder", atomicClaim = true))
    }

    /** The data folder itself, or a folder above it, would be cleared with the data the backup is made to survive. */
    override fun refusal(folder: String): FolderProblem? =
        if (real(dataDirectory).startsWith(real(File(folder)))) FolderProblem.HoldsAppData else null

    private fun real(file: File): Path = try { file.toPath().toRealPath() } catch (ignored: java.io.IOException) { file.absoluteFile.toPath().normalize() }
}

/** The system's folder dialog (Swing's, on every desktop). Null when the person cancels, or when there is no display to show it on. */
fun swingFolderChooser(start: File): File? {
    if (GraphicsEnvironment.isHeadless()) return null
    var picked: File? = null
    SwingUtilities.invokeAndWait {
        val dialog = JFileChooser(start.takeIf { it.isDirectory } ?: File(System.getProperty("user.home")))
        dialog.fileSelectionMode = JFileChooser.DIRECTORIES_ONLY
        dialog.isAcceptAllFileFilterUsed = false
        if (dialog.showOpenDialog(null) == JFileChooser.APPROVE_OPTION) picked = dialog.selectedFile
    }
    return picked
}

/**
 * The desktop app's own files for the clear (SWR-MG-06): the whole data folder, except the chosen backup folder when it lies inside
 * it, since the clear is run against that backup. A folder outside the data folder needs no exception.
 */
fun desktopOwnFiles(dataDirectory: File, backupFolder: String): life.neurone.core.migration.FileStore {
    val data = dataDirectory.absoluteFile.toPath().normalize()
    val backup = File(backupFolder).absoluteFile.toPath().normalize()
    val kept = if (backup.startsWith(data) && backup != data) {
        listOf(data.relativize(backup).joinToString("/") { it.toString() })
    } else {
        emptyList()
    }
    return life.neurone.core.migration.appFileStore(listOf(life.neurone.core.migration.AppFolder("data", data, kept = kept)))
}
