package life.neurone.shared

import kotlinx.cinterop.ExperimentalForeignApi
import life.neurone.core.migration.ObjectStore
import life.neurone.shared.session.UhdrFolder
import platform.Foundation.NSFileManager
import platform.Foundation.NSFileProtectionCompleteUntilFirstUserAuthentication
import platform.Foundation.NSFileProtectionKey
import platform.Foundation.NSURL
import platform.Foundation.NSURLIsExcludedFromBackupKey

/**
 * Where iOS keeps the EDF+ recordings fetched from the hub: `Documents/UHDR`, which the clear overwrites with the rest of Documents. A file
 * that lands in it, and the folder, are given the protection class `completeUntilFirstUserAuthentication` and left out of iCloud and
 * iTunes backups (`REQ-UHDRFP-01`, as the Swift app's `UHDRFileProtection`). Not `complete`: the automatic backup reads these files in
 * the background while the phone is locked. A class that could not be set is not an error: the download still succeeds.
 */
@OptIn(ExperimentalForeignApi::class)
class IosUhdrFolder(private val root: String, private val files: NSFileManager = NSFileManager.defaultManager) : UhdrFolder {

    override val store: ObjectStore by lazy { IosFolderObjectStore(root) }

    init {
        files.createDirectoryAtPath(root, withIntermediateDirectories = true, attributes = null, error = null)
        apply(root)
        (files.contentsOfDirectoryAtPath(root, error = null) ?: emptyList<Any?>()).forEach { name -> apply("$root/$name") }
    }

    override fun protect(name: String) { apply("$root/$name") }

    /** True when the class was set; a refusal is returned, never thrown, so the rest of the folder still gets its class (`REQ-UHDRFP-01` c). */
    private fun apply(path: String): Boolean {
        val set = files.setAttributes(mapOf<Any?, Any?>(NSFileProtectionKey to PROTECTION_CLASS), ofItemAtPath = path, error = null)
        NSURL.fileURLWithPath(path).setResourceValue(true, forKey = NSURLIsExcludedFromBackupKey, error = null)
        return set
    }

    internal companion object {
        /** Not `complete`: the automatic backup reads these files in the background while the phone is locked (`REQ-UHDRFP-01` b). */
        val PROTECTION_CLASS: String? = NSFileProtectionCompleteUntilFirstUserAuthentication
    }
}
