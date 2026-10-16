package life.neurone.shared

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.addressOf
import kotlinx.cinterop.convert
import kotlinx.cinterop.usePinned
import life.neurone.core.migration.ObjectStore
import platform.Foundation.NSData
import platform.Foundation.NSFileManager
import platform.Foundation.NSUUID
import platform.Foundation.create
import platform.Foundation.dataWithContentsOfFile
import platform.Foundation.length
import platform.Foundation.bytes
import platform.posix.EEXIST
import platform.posix.errno
import platform.posix.link
import platform.posix.memcpy
import platform.posix.rename

/**
 * An [ObjectStore] over one folder on this device (SWR-MG-10; issue #569 package 6q), the iOS counterpart of the JVM's
 * `DirectoryObjectStore` with the same file names and raw bytes, so a folder one platform wrote another can read. A claim is
 * created with a hard link from a fully written temporary file, so the file system arbitrates it: exactly one of two
 * simultaneous creates succeeds, and a reader never sees a half-written claim (SWR-MG-07). The folder is in the app's Documents
 * directory, which the Files app shows when the app declares file sharing, so the person can move a backup out of it.
 */
@OptIn(ExperimentalForeignApi::class)
class IosFolderObjectStore(private val root: String) : ObjectStore {
    private val files = NSFileManager.defaultManager

    init {
        files.createDirectoryAtPath(root, withIntermediateDirectories = true, attributes = null, error = null)
    }

    private fun path(name: String): String {
        require(isSafeName(name)) { "unsafe object name '$name'" }
        return "$root/$name"
    }

    private fun writeTemp(bytes: ByteArray): String {
        val temp = "$root/$TEMP_PREFIX${NSUUID().UUIDString}"
        check(files.createFileAtPath(temp, contents = bytes.toNSData(), attributes = null)) { "could not write $temp" }
        return temp
    }

    override suspend fun get(name: String): ByteArray? = NSData.dataWithContentsOfFile(path(name))?.toByteArray()

    override suspend fun put(name: String, bytes: ByteArray) {
        val target = path(name)
        val temp = writeTemp(bytes)
        try {
            check(rename(temp, target) == 0) { "could not replace $target" }
        } finally {
            files.removeItemAtPath(temp, error = null)
        }
    }

    override suspend fun createIfAbsent(name: String, bytes: ByteArray): Boolean {
        val target = path(name)
        val temp = writeTemp(bytes)
        try {
            if (link(temp, target) == 0) return true
            check(errno == EEXIST) { "could not create $target (errno $errno)" }
            return false
        } finally {
            files.removeItemAtPath(temp, error = null)
        }
    }

    override suspend fun delete(name: String) {
        files.removeItemAtPath(path(name), error = null)
    }

    override suspend fun list(prefix: String): List<String> =
        (files.contentsOfDirectoryAtPath(root, error = null) ?: emptyList<Any?>()).map { it.toString() }
            .filter { it.startsWith(prefix) && !it.startsWith(TEMP_PREFIX) }.sorted()

    /** Not asked of the file system: the probe then reports the space as unknown, which it treats as a provider that cannot say. */
    override suspend fun freeSpace(): Long? = null

    companion object {
        const val TEMP_PREFIX = ".tmp-"

        /** One path segment of ASCII letters, digits, dot, dash and underscore, not starting with a dot (as the JVM's). */
        fun isSafeName(name: String): Boolean = name.isNotEmpty() && name.length <= 255 && !name.startsWith(".") &&
            name.all { (it in 'a'..'z') || (it in 'A'..'Z') || (it in '0'..'9') || it in "._-" }
    }
}

@OptIn(ExperimentalForeignApi::class)
private fun NSData.toByteArray(): ByteArray {
    val n = length.toInt()
    val out = ByteArray(n)
    if (n > 0) out.usePinned { memcpy(it.addressOf(0), bytes, length) }
    return out
}

@OptIn(ExperimentalForeignApi::class)
private fun ByteArray.toNSData(): NSData =
    if (isEmpty()) NSData() else usePinned { NSData.create(bytes = it.addressOf(0), length = size.convert()) }
