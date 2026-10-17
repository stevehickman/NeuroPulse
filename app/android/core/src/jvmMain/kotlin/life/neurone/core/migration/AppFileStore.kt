package life.neurone.core.migration

import java.nio.file.Path

/** One folder of a JVM host's own files, and the paths in it that the clear must leave alone (see [KeepingFileStore]). */
class AppFolder(val name: String, val path: Path, val kept: List<String> = emptyList())

/**
 * SWR-MG-06: the [FileStore] for a host that runs on the JVM (the Android app and the desktop apps), made from its folders. Each
 * folder is a [DirectoryFileStore] that hides its kept paths, and the folders are one store whose paths start with the folder's
 * name. A folder that does not exist holds no files.
 */
fun appFileStore(folders: List<AppFolder>): FileStore = FolderUnionFileStore(
    folders.associate { f ->
        f.name to (DirectoryFileStore(f.path).let { d -> if (f.kept.isEmpty()) d else KeepingFileStore(d, f.kept) })
    },
)
