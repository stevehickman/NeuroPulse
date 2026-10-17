package life.neurone.core.migration

/**
 * SWR-MG-06: the app's own files for the old phone's clear, assembled from a host's folders. A host says which folders hold its
 * files and which paths in them must survive (the backup place is among the app's files on a computer, and the clear must not
 * delete the very backup it is clearing against); the core overwrites and deletes whatever [FileStore.list] reports. Both
 * wrappers are common code, so the rule is written once for every host that has files: the JVM's folders
 * (`appFileStore`), iOS's, and a host added later.
 */

/**
 * Several folders as one [FileStore]. A path is `<folder name>/<path in that folder>`, so two folders may hold a file of the same
 * name, and a call reaches only the folder its path names. [folders] are keyed by a name with no slash.
 */
class FolderUnionFileStore(private val folders: Map<String, FileStore>) : FileStore {
    init {
        require(folders.keys.none { it.isEmpty() || '/' in it }) { "a folder name is one non-empty path segment" }
    }

    private fun part(path: String): Pair<FileStore, String> {
        val name = path.substringBefore('/')
        val rest = path.substringAfter('/', "")
        val store = folders[name]
        require(store != null && rest.isNotEmpty()) { "no such folder in path '$path'" }
        return store to rest
    }

    override suspend fun list(): List<StoredFile> =
        folders.entries.sortedBy { it.key }.flatMap { (name, store) -> store.list().map { StoredFile("$name/${it.path}", it.size) } }

    override suspend fun write(path: String, offset: Long, bytes: ByteArray) = part(path).let { (s, p) -> s.write(p, offset, bytes) }

    override suspend fun sync(path: String) = part(path).let { (s, p) -> s.sync(p) }

    override suspend fun delete(path: String) = part(path).let { (s, p) -> s.delete(p) }
}

/**
 * A [FileStore] that does not see the paths in [kept]: a kept path is a file with that name or anything under a folder of that
 * name. It is left out of [list], so the clear never asks for it, and a call that names one is refused rather than done, so a
 * mistake elsewhere cannot reach it either.
 */
class KeepingFileStore(private val inner: FileStore, kept: List<String>) : FileStore {
    private val kept: List<String> = kept.map { it.trim('/') }.also { k -> require(k.none { it.isEmpty() }) { "a kept path is not empty" } }

    private fun isKept(path: String): Boolean {
        val p = path.trim('/')
        return kept.any { p == it || p.startsWith("$it/") }
    }

    private fun refuseKept(path: String) = require(!isKept(path)) { "'$path' is kept and is not cleared" }

    override suspend fun list(): List<StoredFile> = inner.list().filterNot { isKept(it.path) }

    override suspend fun write(path: String, offset: Long, bytes: ByteArray) { refuseKept(path); inner.write(path, offset, bytes) }

    override suspend fun sync(path: String) { refuseKept(path); inner.sync(path) }

    override suspend fun delete(path: String) { refuseKept(path); inner.delete(path) }
}
