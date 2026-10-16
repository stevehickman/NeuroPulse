package life.neurone.shared.migration

import kotlinx.serialization.json.jsonPrimitive
import life.neurone.core.migration.MigrationFlow
import life.neurone.core.migration.ObjectStore
import life.neurone.core.migration.ObjectStoreRoute
import life.neurone.shared.MigrationPlace

// A backup that is a file the person carries (SWR-MG-02, SWR-MG-10; issue #569 package 6q). Every platform can save a file and
// open one, and a file written by one platform opens on another, so this is a place for a host that has no folder of its own:
// a phone, a browser. The backup is made in memory, and the person is handed the one file the core names; restoring starts by
// choosing that file. All of it is common: a host supplies only the two dialogs, as a [FileTransfer].

/** The two things only the operating system can do for a file the person carries: save one, and choose one. */
interface FileTransfer {
    /** Offer [bytes] to the person as a file called [name] (a download, a save dialog). True when it was saved. */
    suspend fun save(name: String, bytes: ByteArray): Boolean

    /** Ask the person to choose a file. Null when they cancel or the file cannot be read. */
    suspend fun open(): ByteArray?
}

/** An [ObjectStore] in memory: what a backup in progress, or one just opened from a file, lives in. */
class MemoryObjectStore : ObjectStore {
    private val objects = linkedMapOf<String, ByteArray>()

    override suspend fun get(name: String): ByteArray? = objects[name]?.copyOf()
    override suspend fun put(name: String, bytes: ByteArray) { objects[name] = bytes.copyOf() }
    override suspend fun createIfAbsent(name: String, bytes: ByteArray): Boolean {
        if (name in objects) return false
        objects[name] = bytes.copyOf()
        return true
    }
    override suspend fun delete(name: String) { objects.remove(name) }
    override suspend fun list(prefix: String): List<String> = objects.keys.filter { it.startsWith(prefix) }
    override suspend fun freeSpace(): Long? = null
}

/**
 * A place that is a file. Its route promises no exclusive create: one file carried by hand cannot say that only one phone
 * opened it, and the core withholds a place with no atomic claim from a migration (SWR-MG-10) while leaving it for the
 * health-data-only backup. So this place carries that backup to any platform, and a migration needs a place that can promise.
 */
class PortableFilePlace(private val transfer: FileTransfer) {
    private val bundleName: String = MigrationFlow.objectNames()["bundle"]!!.jsonPrimitive.content
    private val store = MemoryObjectStore()

    /** The place a backup or restore is given. [MigrationPlace.label] is the file's name. */
    val place: MigrationPlace = MigrationPlace(bundleName, store, ObjectStoreRoute("portable-file", atomicClaim = false))

    /** Hand the person the backup that was made. False when there is none, or they did not save it. */
    suspend fun saveBackup(): Boolean {
        val bytes = store.get(bundleName) ?: return false
        return transfer.save(bundleName, bytes)
    }

    /** Let the person choose a backup file and put it on the place. False when they chose none. */
    suspend fun openBackup(): Boolean {
        val bytes = transfer.open() ?: return false
        store.put(bundleName, bytes)
        return true
    }
}
