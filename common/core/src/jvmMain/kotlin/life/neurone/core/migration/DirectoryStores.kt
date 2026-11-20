package life.neurone.core.migration

import java.nio.ByteBuffer
import java.nio.channels.FileChannel
import java.nio.file.FileAlreadyExistsException
import java.nio.file.Files
import java.nio.file.LinkOption
import java.nio.file.NoSuchFileException
import java.nio.file.Path
import java.nio.file.StandardCopyOption
import java.nio.file.StandardOpenOption
import java.nio.file.attribute.BasicFileAttributeView
import java.nio.file.attribute.FileTime

// A folder on disk as the app's two storage roles (SWR-MG-06, SWR-MG-10, issue #569 package 6k): [DirectoryObjectStore] is the
// route for a local computer, a USB drive or a shared folder, and [DirectoryFileStore] is the app's own files for the old
// phone's clear. Both are plain java.nio, so Android and the desktop apps share them. The Swift (DirectoryStores.swift) and C#
// (DirectoryStores.cs) ones use the same file names and raw bytes, so a folder one platform wrote another can read.

/**
 * An [ObjectStore] over one folder: each object is a file named by the object. A claim is created with a hard link from a
 * fully written temporary file, so the operating system arbitrates it (exactly one of two simultaneous creates succeeds) and a
 * reader never sees a half-written claim (SWR-MG-07). A route over a folder on a network share is only as atomic as that share's
 * hard links; the app declares `atomicClaim` for the folder it knows.
 */
class DirectoryObjectStore(private val root: Path) : ObjectStore {
    init {
        Files.createDirectories(root)
    }

    private fun file(name: String): Path {
        require(isSafeName(name)) { "unsafe object name '$name'" }
        return root.resolve(name)
    }

    override suspend fun get(name: String): ByteArray? = try {
        Files.readAllBytes(file(name))
    } catch (ignored: NoSuchFileException) {
        null
    }

    override suspend fun put(name: String, bytes: ByteArray) {
        val target = file(name)
        val temp = Files.createTempFile(root, TEMP_PREFIX, "")
        try {
            Files.write(temp, bytes)
            withoutTimes(temp)
            Files.move(temp, target, StandardCopyOption.REPLACE_EXISTING, StandardCopyOption.ATOMIC_MOVE)
        } finally {
            Files.deleteIfExists(temp)
        }
    }

    override suspend fun createIfAbsent(name: String, bytes: ByteArray): Boolean {
        val target = file(name)
        val temp = Files.createTempFile(root, TEMP_PREFIX, "")
        try {
            Files.write(temp, bytes)
            withoutTimes(temp)
            return try {
                Files.createLink(target, temp)
                true
            } catch (ignored: FileAlreadyExistsException) {
                false
            }
        } finally {
            Files.deleteIfExists(temp)
        }
    }

    override suspend fun delete(name: String) {
        Files.deleteIfExists(file(name))
    }

    override suspend fun list(prefix: String): List<String> = Files.list(root).use { s ->
        s.iterator().asSequence().filter { Files.isRegularFile(it) }.map { it.fileName.toString() }
            .filter { it.startsWith(prefix) && !it.startsWith(TEMP_PREFIX) }.sorted().toList()
    }

    override suspend fun freeSpace(): Long? = Files.getFileStore(root).usableSpace

    companion object {
        const val TEMP_PREFIX = ".tmp-"

        /**
         * Set the file's creation, modification and access times to the epoch, so the folder does not show when each backup ran
         * (`REQ-UHDRBK-01` d; session times are UHDR, CLAUDE.md §5.1). Set before the file is moved or linked into place, so it never
         * has its real time under its final name. A file system that cannot set them leaves the file as written.
         */
        private fun withoutTimes(file: Path) {
            val epoch = FileTime.fromMillis(0)
            try {
                Files.getFileAttributeView(file, BasicFileAttributeView::class.java).setTimes(epoch, epoch, epoch)
            } catch (ignored: java.io.IOException) {
            } catch (ignored: UnsupportedOperationException) {
            }
        }

        /** A name is one path segment of ASCII letters, digits, dot, dash and underscore, and does not start with a dot. */
        fun isSafeName(name: String): Boolean =
            name.isNotEmpty() && name.length <= 255 && !name.startsWith(".") && name.all { (it in 'a'..'z') || (it in 'A'..'Z') || (it in '0'..'9') || it in "._-" }
    }
}

/**
 * A [FileStore] over one folder tree: [list] reports every regular file under it with its path relative to the folder (forward
 * slashes), and no path may leave the folder. [write] overwrites in place and [sync] forces the file to storage.
 *
 * **No call follows a symbolic link** (`OI-PRIV-10` (3)). The clear overwrites every file [list] reports, so a link planted in the
 * folder would otherwise have it overwrite a file somewhere else. A link is not a regular file and is not listed; a path is refused
 * when its folder, once links are resolved, is outside this one, or when the file itself is a link; and the file is opened with
 * `NOFOLLOW_LINKS`, so a link put there after the check is refused by the operating system.
 */
class DirectoryFileStore(root: Path) : FileStore {
    private val base: Path = root.toAbsolutePath().normalize()

    private fun resolve(path: String): Path {
        val p = base.resolve(path).normalize()
        require(p.startsWith(base) && p != base) { "path leaves the folder: '$path'" }
        // The check above is on the text of the path. This one is on where it really is: a folder on the way that is a link
        // (or a Windows junction) to somewhere else has a real path outside the real folder.
        val parent = p.parent
        if (Files.exists(parent)) {
            require(parent.toRealPath().startsWith(base.toRealPath())) { "path leaves the folder through a link: '$path'" }
        }
        require(!Files.isSymbolicLink(p)) { "path is a link: '$path'" }
        return p
    }

    override suspend fun list(): List<StoredFile> {
        if (!Files.isDirectory(base)) return emptyList()
        // `Files.walk` does not enter a linked folder, and NOFOLLOW_LINKS keeps a link to a file from counting as that file.
        return Files.walk(base).use { s ->
            s.iterator().asSequence().filter { Files.isRegularFile(it, LinkOption.NOFOLLOW_LINKS) }
                .map { StoredFile(base.relativize(it).joinToString("/") { seg -> seg.toString() }, Files.size(it)) }
                .sortedBy { it.path }.toList()
        }
    }

    override suspend fun write(path: String, offset: Long, bytes: ByteArray) {
        FileChannel.open(resolve(path), setOf(StandardOpenOption.WRITE, StandardOpenOption.CREATE, LinkOption.NOFOLLOW_LINKS)).use { f ->
            val buffer = ByteBuffer.wrap(bytes)
            var at = offset
            while (buffer.hasRemaining()) at += f.write(buffer, at)
        }
    }

    override suspend fun sync(path: String) {
        FileChannel.open(resolve(path), setOf(StandardOpenOption.WRITE, LinkOption.NOFOLLOW_LINKS)).use { it.force(true) }
    }

    override suspend fun delete(path: String) {
        // Deleting a link removes the link and never what it points at, so the last name need not be checked twice.
        Files.deleteIfExists(resolve(path))
    }
}
