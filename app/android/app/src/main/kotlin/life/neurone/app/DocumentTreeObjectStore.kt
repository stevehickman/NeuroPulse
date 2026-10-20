package life.neurone.app

import android.content.ContentResolver
import android.net.Uri
import android.provider.DocumentsContract
import android.provider.OpenableColumns
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import life.neurone.core.migration.ObjectStore
import java.util.UUID

/**
 * An [ObjectStore] over a folder the person chose through the system's folder dialog (Storage Access Framework, a document tree):
 * each object is a file named by the object, directly in that folder. The app can reach that one folder and nothing else, and
 * keeps the grant across restarts ([AndroidFolderPlaces]).
 *
 * **It is not atomic.** A provider cannot create a file only if none exists, so [createIfAbsent] looks and then creates, and the
 * route over this store says `atomicClaim = false`: the core then withholds it from a migration and uses it only for the health-data
 * backup (SWR-MG-10). [put] writes a complete temporary file before it replaces the old object, so a stop part-way leaves the old
 * bundle or the new one, never half of one (the gap between removing the old and naming the new leaves the new, under its temporary
 * name, which [list] hides).
 */
class DocumentTreeObjectStore(private val resolver: ContentResolver, private val tree: Uri) : ObjectStore {
    private val rootId: String = DocumentsContract.getTreeDocumentId(tree)
    private val root: Uri = DocumentsContract.buildDocumentUriUsingTree(tree, rootId)
    private val children: Uri = DocumentsContract.buildChildDocumentsUriUsingTree(tree, rootId)

    private class Child(val id: String, val name: String, val isFolder: Boolean)

    private fun listChildren(): List<Child> {
        val columns = arrayOf(DocumentsContract.Document.COLUMN_DOCUMENT_ID, DocumentsContract.Document.COLUMN_DISPLAY_NAME, DocumentsContract.Document.COLUMN_MIME_TYPE)
        val found = ArrayList<Child>()
        resolver.query(children, columns, null, null, null)?.use { c ->
            while (c.moveToNext()) {
                found += Child(c.getString(0), c.getString(1) ?: continue, c.getString(2) == DocumentsContract.Document.MIME_TYPE_DIR)
            }
        } ?: throw java.io.IOException("the folder cannot be read")
        return found
    }

    private fun find(name: String): Uri? =
        listChildren().firstOrNull { !it.isFolder && it.name == name }?.let { DocumentsContract.buildDocumentUriUsingTree(tree, it.id) }

    /** Create a file called [name]. A provider may add an extension to a name it does not know the type of, so the name is checked and set. */
    private fun create(name: String, bytes: ByteArray): Uri {
        val created = DocumentsContract.createDocument(resolver, root, MIME, name) ?: throw java.io.IOException("the folder takes no file")
        try {
            resolver.openOutputStream(created, "wt")?.use { it.write(bytes) } ?: throw java.io.IOException("the file cannot be written")
            return named(created, name)
        } catch (ex: java.io.IOException) {
            runCatching { DocumentsContract.deleteDocument(resolver, created) }
            throw ex
        }
    }

    private fun named(document: Uri, name: String): Uri {
        val actual = resolver.query(document, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { c -> if (c.moveToFirst()) c.getString(0) else null }
        if (actual == name) return document
        return DocumentsContract.renameDocument(resolver, document, name) ?: throw java.io.IOException("the file cannot be named '$name'")
    }

    override suspend fun get(name: String): ByteArray? = withContext(Dispatchers.IO) {
        require(isSafeName(name)) { "unsafe object name '$name'" }
        find(name)?.let { uri -> resolver.openInputStream(uri)?.use { it.readBytes() } }
    }

    override suspend fun put(name: String, bytes: ByteArray) = withContext(Dispatchers.IO) {
        require(isSafeName(name)) { "unsafe object name '$name'" }
        val temp = create(TEMP_PREFIX + UUID.randomUUID().toString().take(8), bytes)
        try {
            find(name)?.let { DocumentsContract.deleteDocument(resolver, it) }
            named(temp, name)
            Unit
        } catch (ex: java.io.IOException) {
            runCatching { DocumentsContract.deleteDocument(resolver, temp) }
            throw ex
        }
    }

    override suspend fun createIfAbsent(name: String, bytes: ByteArray): Boolean = withContext(Dispatchers.IO) {
        require(isSafeName(name)) { "unsafe object name '$name'" }
        if (find(name) != null) false else { create(name, bytes); true }
    }

    override suspend fun delete(name: String) = withContext(Dispatchers.IO) {
        require(isSafeName(name)) { "unsafe object name '$name'" }
        find(name)?.let { DocumentsContract.deleteDocument(resolver, it) }
        Unit
    }

    override suspend fun list(prefix: String): List<String> = withContext(Dispatchers.IO) {
        listChildren().filter { !it.isFolder && it.name.startsWith(prefix) && !it.name.startsWith(TEMP_PREFIX) }.map { it.name }.sorted()
    }

    /** A provider cannot say how much room a folder has. */
    override suspend fun freeSpace(): Long? = null

    private companion object {
        const val MIME = "application/octet-stream"
        const val TEMP_PREFIX = ".tmp-"

        /** The same rule as the folder store on the desktop and iOS, so a name written by one is readable by the others. */
        fun isSafeName(name: String): Boolean =
            name.isNotEmpty() && name.length <= 255 && !name.startsWith(".") && name.all { (it in 'a'..'z') || (it in 'A'..'Z') || (it in '0'..'9') || it in "._-" }
    }
}
