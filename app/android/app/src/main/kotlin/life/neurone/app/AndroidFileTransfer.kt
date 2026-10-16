package life.neurone.app

import android.content.Context
import android.net.Uri
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import life.neurone.shared.migration.FileTransfer

/**
 * The system's save and open dialogs for a backup file (SWR-MG-02), through the Storage Access Framework: the person chooses
 * where the file goes (their device, a cloud drive, a USB stick) and the app gets access to that one file and nothing else. The
 * two launchers are registered with the composition by [AndroidPlatformServices.rememberFileTransfer], which sets [launchSave]
 * and [launchOpen]; each dialog answers through [onPicked].
 */
class AndroidFileTransfer(private val context: Context) : FileTransfer {
    var launchSave: (String) -> Unit = {}
    var launchOpen: () -> Unit = {}

    private var picking: CompletableDeferred<Uri?>? = null

    /** The dialog's answer: the file the person chose, or null if they cancelled. */
    fun onPicked(uri: Uri?) {
        picking?.complete(uri)
        picking = null
    }

    override suspend fun save(name: String, bytes: ByteArray): Boolean {
        val answer = CompletableDeferred<Uri?>().also { picking = it }
        launchSave(name)
        val uri = answer.await() ?: return false
        return withContext(Dispatchers.IO) {
            try {
                // "wt" truncates: a file chosen over an older backup must not keep that backup's tail.
                context.contentResolver.openOutputStream(uri, "wt")?.use { it.write(bytes) } != null
            } catch (ex: java.io.IOException) {
                false
            }
        }
    }

    override suspend fun open(): ByteArray? {
        val answer = CompletableDeferred<Uri?>().also { picking = it }
        launchOpen()
        val uri = answer.await() ?: return null
        return withContext(Dispatchers.IO) {
            try {
                context.contentResolver.openInputStream(uri)?.use { it.readBytes() }
            } catch (ex: java.io.IOException) {
                null
            }
        }
    }
}
