package life.neurone.app

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.provider.DocumentsContract
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import life.neurone.core.migration.ObjectStoreRoute
import life.neurone.shared.MigrationPlace
import life.neurone.shared.migration.FolderPlaces
import life.neurone.shared.migration.FolderProblem

/**
 * The folder a backup is kept in on Android: one the person chooses with the system's folder dialog (Storage Access Framework).
 * The app is given that folder and nothing else, and asks the system to keep the grant across restarts, which is what lets an
 * automatic backup write to it without a dialog.
 *
 * **There is no default folder.** The folders the app owns are not ones the person can reach, so a backup kept there could not be
 * carried to a new phone; until a folder is chosen the backup is a file saved and opened through the system's dialogs, as before
 * ([AndroidFileTransfer]), and no backup runs unattended. The folder is named by its tree address (`content://...`), kept as the
 * chosen folder in the key map (`np.backup.folder`).
 *
 * The dialog's launcher is registered with the composition by [AndroidPlatformServices.rememberFileTransfer], which sets [launchTree]
 * and has the dialog's answer reported through [onPicked].
 */
class AndroidFolderPlaces(private val context: Context) : FolderPlaces {
    var launchTree: () -> Unit = {}

    private var picking: CompletableDeferred<Uri?>? = null

    override val defaultFolder: String? = null

    /** The dialog's answer: the folder the person chose, or null if they cancelled. */
    fun onPicked(uri: Uri?) {
        picking?.complete(uri)
        picking = null
    }

    override suspend fun choose(current: String): String? {
        val answer = CompletableDeferred<Uri?>().also { picking = it }
        launchTree()
        val uri = answer.await() ?: return null
        return withContext(Dispatchers.IO) {
            try {
                context.contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
                uri.toString()
            } catch (ex: SecurityException) {
                // A provider that will not keep the grant gives a folder that works until the app restarts; an unattended backup needs more.
                null
            }
        }
    }

    override fun placeAt(folder: String): MigrationPlace {
        val tree = Uri.parse(folder)
        require(tree.scheme == "content") { "'$folder' is not a folder the system gave" }
        // A grant the person revoked in the system's settings, or a drive that is gone, is a folder that cannot be used.
        require(context.contentResolver.persistedUriPermissions.any { it.uri == tree && it.isReadPermission && it.isWritePermission }) { "the folder is no longer open to the app" }
        return MigrationPlace(label(tree), DocumentTreeObjectStore(context.contentResolver, tree), ObjectStoreRoute("android-folder", atomicClaim = false))
    }

    /** A folder the system gave cannot hold the app's own private data, so there is nothing to refuse. */
    override fun refusal(folder: String): FolderProblem? = null

    private fun label(tree: Uri): String {
        val document = DocumentsContract.buildDocumentUriUsingTree(tree, DocumentsContract.getTreeDocumentId(tree))
        val name = context.contentResolver.query(document, arrayOf(DocumentsContract.Document.COLUMN_DISPLAY_NAME), null, null, null)?.use { c ->
            if (c.moveToFirst()) c.getString(0) else null
        }
        return name ?: tree.toString()
    }
}
