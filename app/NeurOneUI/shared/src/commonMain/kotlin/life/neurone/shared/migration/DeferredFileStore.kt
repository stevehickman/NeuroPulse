package life.neurone.shared.migration

import life.neurone.core.migration.FileStore
import life.neurone.core.migration.StoredFile

/** A [FileStore] that asks [resolve] for the store each time it is used, so a value that can change (the chosen backup folder) is read when it is needed and not when the app started. */
class DeferredFileStore(private val resolve: () -> FileStore) : FileStore {
    override suspend fun list(): List<StoredFile> = resolve().list()
    override suspend fun write(path: String, offset: Long, bytes: ByteArray) = resolve().write(path, offset, bytes)
    override suspend fun sync(path: String) = resolve().sync(path)
    override suspend fun delete(path: String) = resolve().delete(path)
}
