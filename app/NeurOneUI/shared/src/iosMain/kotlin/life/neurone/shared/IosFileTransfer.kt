package life.neurone.shared

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withContext
import life.neurone.shared.migration.FileTransfer
import platform.Foundation.NSData
import platform.Foundation.NSTemporaryDirectory
import platform.Foundation.NSURL
import platform.Foundation.dataWithContentsOfURL
import platform.Foundation.writeToURL
import platform.UIKit.UIApplication
import platform.UIKit.UIDocumentPickerDelegateProtocol
import platform.UIKit.UIDocumentPickerViewController
import platform.UIKit.UIViewController
import platform.UniformTypeIdentifiers.UTTypeData
import platform.darwin.NSObject
import kotlin.coroutines.resume

/**
 * The system's document picker for a file the person carries: their protocols as a `.npps`, or a backup (SWR-MG-02). Saving offers
 * a copy through the export sheet, so the person chooses where it goes (Files, a drive, another app); opening reads a copy of the
 * file they choose. A cancelled picker, or a file that cannot be read, gives false or null and never throws.
 */
@OptIn(ExperimentalForeignApi::class)
class IosFileTransfer : FileTransfer {

    override suspend fun save(name: String, bytes: ByteArray): Boolean = withContext(Dispatchers.Main) {
        // The export sheet takes a file, so the bytes go to a temporary one first.
        val url = NSURL.fileURLWithPath(NSTemporaryDirectory() + name)
        if (!bytes.toNSData().writeToURL(url, atomically = true)) return@withContext false
        val picked = present(UIDocumentPickerViewController(forExportingURLs = listOf(url), asCopy = true))
        picked != null
    }

    override suspend fun open(): ByteArray? = withContext(Dispatchers.Main) {
        val picked = present(UIDocumentPickerViewController(forOpeningContentTypes = listOf(UTTypeData), asCopy = true))
        val url = picked?.firstOrNull() as? NSURL ?: return@withContext null
        NSData.dataWithContentsOfURL(url)?.toByteArray()
    }

    /** Show [picker] and wait for the person: the URLs they picked, or null when they cancelled or nothing could be shown. */
    private suspend fun present(picker: UIDocumentPickerViewController): List<*>? = suspendCancellableCoroutine { cont ->
        val host = topViewController()
        if (host == null) {
            cont.resume(null)
            return@suspendCancellableCoroutine
        }
        // The picker holds its delegate weakly, so the delegate is kept here until the picker answers.
        var keepAlive: PickerDelegate? = null
        val delegate = PickerDelegate { urls ->
            keepAlive = null
            if (cont.isActive) cont.resume(urls)
        }
        keepAlive = delegate
        picker.delegate = delegate
        host.presentViewController(picker, animated = true, completion = null)
        cont.invokeOnCancellation { picker.dismissViewControllerAnimated(true, completion = null) }
    }

    private fun topViewController(): UIViewController? {
        var controller = UIApplication.sharedApplication.keyWindow?.rootViewController
        while (controller?.presentedViewController != null) controller = controller.presentedViewController
        return controller
    }
}

private class PickerDelegate(private val onResult: (List<*>?) -> Unit) : NSObject(), UIDocumentPickerDelegateProtocol {
    override fun documentPicker(controller: UIDocumentPickerViewController, didPickDocumentsAtURLs: List<*>) = onResult(didPickDocumentsAtURLs)
    override fun documentPickerWasCancelled(controller: UIDocumentPickerViewController) = onResult(null)
}
