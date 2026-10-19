package life.neurone.desktop

import java.io.File
import java.nio.file.FileSystems
import java.nio.file.Files
import java.nio.file.attribute.PosixFilePermission
import java.nio.file.attribute.PosixFilePermissions
import java.util.concurrent.TimeUnit

// Who can read the app's data file, and whether a backup program copies it (desktop audit of the privacy review, item 6, SWR-MG-18).
// Everything the file holds is sealed at rest under a key in the system's credential store (SWR-MG-06, SWR-MG-16), so this is the
// second layer, not the first: another account on the computer, or a backup that lands somewhere else, gets ciphertext, and gets
// less of that.

/**
 * Keeps a file or folder away from other accounts and from backup programs where the system has a way to say so.
 *
 * - **macOS and Linux:** only the owner can reach it (`rwx------` for the folder, `rw-------` for a file), and a file is **made**
 *   that way: created empty with those permissions and only then written, so there is no moment in which another account could
 *   read it.
 * - **macOS:** a file is also excluded from Time Machine (`tmutil addexclusion`, which marks the file itself, so it follows the
 *   file when the store replaces it). The folder is not: the person's own backups are kept in a folder beside the file and are
 *   better for being copied.
 * - **Windows:** nothing is done here. The data folder is under `%LOCALAPPDATA%` (SWR-MG-14), which only its owner (and the
 *   administrators) can read by default and which File History and OneDrive's folder backup do not copy.
 *
 * Best effort by design: a system that cannot say it (a file system with no permissions, a missing `tmutil`) leaves the data
 * sealed and readable as before, and a failure here never stops the store from saving a decision the person made.
 */
interface DataFileProtection {
    /** Makes [file] (or the folder [file]) owner-only, creating a file that is not there yet; excludes a file from backups where the system can. */
    fun prepare(file: File)

    companion object {
        val NONE: DataFileProtection = object : DataFileProtection {
            override fun prepare(file: File) {}
        }

        /** The protection for the system the app is running on. */
        fun forThisSystem(os: String = System.getProperty("os.name").lowercase()): DataFileProtection =
            OwnerOnlyProtection(excludeFromBackup = if ("mac" in os) TimeMachineExclusion::exclude else null)
    }
}

internal class OwnerOnlyProtection(
    private val excludeFromBackup: ((File) -> Unit)? = null,
    private val posix: Boolean = FileSystems.getDefault().supportedFileAttributeViews().contains("posix"),
) : DataFileProtection {
    override fun prepare(file: File) {
        try {
            if (posix) {
                val path = file.toPath()
                if (file.isDirectory) {
                    Files.setPosixFilePermissions(path, FOLDER)
                } else if (!file.exists()) {
                    Files.createFile(path, PosixFilePermissions.asFileAttribute(FILE))
                } else {
                    Files.setPosixFilePermissions(path, FILE)
                }
            }
        } catch (ignored: Exception) {
            // A file system with no permissions, or one the account cannot change: the data is sealed all the same.
        }
        if (file.isFile) {
            try {
                excludeFromBackup?.invoke(file)
            } catch (ignored: Exception) {
                // No backup program to tell, or no tool to tell it with.
            }
        }
    }

    private companion object {
        val FOLDER: Set<PosixFilePermission> = PosixFilePermissions.fromString("rwx------")
        val FILE: Set<PosixFilePermission> = PosixFilePermissions.fromString("rw-------")
    }
}

/** Time Machine's own tool, which marks the file with the exclusion attribute that stays with it. */
internal object TimeMachineExclusion {
    private const val TIMEOUT_SECONDS = 10L

    /** Set once the tool is found to be missing, so a computer without it is not asked on every save. */
    @Volatile private var missing = false

    fun exclude(file: File) {
        if (missing) return
        try {
            val process = ProcessBuilder("tmutil", "addexclusion", file.path).redirectErrorStream(true).start()
            process.outputStream.close()
            process.inputStream.readBytes()
            if (!process.waitFor(TIMEOUT_SECONDS, TimeUnit.SECONDS)) process.destroyForcibly()
        } catch (ex: java.io.IOException) {
            missing = true
        }
    }
}
