package life.neurone.desktop

import life.neurone.core.migration.DirectoryObjectStore
import life.neurone.core.migration.ObjectStore
import life.neurone.shared.session.UhdrFolder
import java.io.File

/**
 * Where the desktop apps keep the EDF+ recordings fetched from the hub: a `UHDR` folder inside the app's data folder, which the clear
 * overwrites with the rest. A file that lands in it is made owner-only and left out of backup programs where the system can say so
 * (`DataFileProtection`), as the data file is.
 */
class DesktopUhdrFolder(
    private val directory: File,
    private val protection: DataFileProtection = DataFileProtection.forThisSystem(),
) : UhdrFolder {
    override val store: ObjectStore by lazy { DirectoryObjectStore(directory.toPath()) }

    init {
        runCatching { directory.mkdirs(); protection.prepare(directory) }
    }

    override fun protect(name: String) {
        runCatching { protection.prepare(File(directory, name)) }
    }
}
