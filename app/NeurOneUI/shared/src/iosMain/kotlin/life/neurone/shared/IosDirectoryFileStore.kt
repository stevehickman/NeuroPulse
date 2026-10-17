package life.neurone.shared

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.addressOf
import kotlinx.cinterop.convert
import kotlinx.cinterop.usePinned
import life.neurone.core.migration.FileStore
import life.neurone.core.migration.StoredFile
import platform.Foundation.NSFileManager
import platform.Foundation.NSFileSize
import platform.Foundation.NSFileType
import platform.Foundation.NSFileTypeRegular
import platform.Foundation.NSNumber
import platform.posix.SEEK_SET
import platform.posix.fclose
import platform.posix.fflush
import platform.posix.fileno
import platform.posix.fopen
import platform.posix.fseek
import platform.posix.fsync
import platform.posix.fwrite

/**
 * A [FileStore] over one folder tree on this device (SWR-MG-06), the iOS counterpart of the JVM's `DirectoryFileStore` and written
 * to the same contract: [list] reports every regular file under the folder with its path relative to it (forward slashes), no
 * path may leave the folder, [write] overwrites in place and [sync] forces the file to storage. The in-place overwrite is
 * POSIX (`fopen` in update mode, `fwrite`, `fsync`), because the clear must change the bytes of the file that is there, not
 * replace it with another. Wrapped by `KeepingFileStore` and `FolderUnionFileStore` (common code) for the host's folders.
 */
@OptIn(ExperimentalForeignApi::class)
class IosDirectoryFileStore(root: String) : FileStore {
    private val base = root.trimEnd('/')
    private val manager = NSFileManager.defaultManager

    private fun resolve(path: String): String {
        val segments = path.split('/')
        require(path.isNotEmpty() && segments.none { it.isEmpty() || it == "." || it == ".." }) { "unsafe path '$path'" }
        return "$base/$path"
    }

    override suspend fun list(): List<StoredFile> {
        val subpaths = manager.subpathsOfDirectoryAtPath(base, error = null) ?: return emptyList()
        return subpaths.map { it.toString() }.sorted().mapNotNull { relative ->
            val attributes = manager.attributesOfItemAtPath("$base/$relative", error = null) ?: return@mapNotNull null
            if (attributes[NSFileType] != NSFileTypeRegular) return@mapNotNull null
            StoredFile(relative, (attributes[NSFileSize] as? NSNumber)?.longLongValue ?: 0L)
        }
    }

    override suspend fun write(path: String, offset: Long, bytes: ByteArray) {
        val file = fopen(resolve(path), "r+b") ?: error("could not open $path for update")
        try {
            check(fseek(file, offset.convert(), SEEK_SET) == 0) { "could not seek in $path" }
            if (bytes.isNotEmpty()) {
                val written = bytes.usePinned { fwrite(it.addressOf(0), 1.convert(), bytes.size.convert(), file) }
                check(written.toLong() == bytes.size.toLong()) { "short write to $path" }
            }
            check(fflush(file) == 0) { "could not flush $path" }
        } finally {
            fclose(file)
        }
    }

    override suspend fun sync(path: String) {
        val file = fopen(resolve(path), "r+b") ?: error("could not open $path to sync")
        try {
            check(fsync(fileno(file)) == 0) { "could not sync $path" }
        } finally {
            fclose(file)
        }
    }

    override suspend fun delete(path: String) {
        manager.removeItemAtPath(resolve(path), error = null)
    }
}
