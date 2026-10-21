package life.neurone.app

import life.neurone.core.migration.DirectoryObjectStore
import life.neurone.core.migration.ObjectStore
import life.neurone.shared.session.UhdrFolder
import java.io.File

/**
 * Where Android keeps the EDF+ recordings fetched from the hub: a `UHDR` folder in the app's private storage, which no other app can
 * read, which the manifest keeps out of cloud backup and device transfer (`allowBackup="false"`), and which the clear overwrites with
 * the rest of the app's files. Nothing more is asked of a file that lands in it.
 */
class AndroidUhdrFolder(private val directory: File) : UhdrFolder {
    override val store: ObjectStore by lazy { DirectoryObjectStore(directory.toPath()) }

    override fun protect(name: String) {}
}
