package life.neurone.core.migration

/**
 * The calls a storage provider is asked for (SWR-MG-11), on named objects. This is the whole of what an app writes for a
 * cloud account, the local computer or a shared folder; the core holds the rest (`object_route.rs`). A call that cannot be
 * done (no network, no permission, a refused write) throws, and the service reports the route unavailable.
 */
interface ObjectStore {
    /** The object's bytes, or null when there is none. */
    suspend fun get(name: String): ByteArray?

    /** Write the object, replacing any earlier one. */
    suspend fun put(name: String, bytes: ByteArray)

    /** Write the object if and only if none exists, arbitrated by the store itself (SWR-MG-07). True when this call wrote it. */
    suspend fun createIfAbsent(name: String, bytes: ByteArray): Boolean

    /** Remove the object. Not an error when there is none. */
    suspend fun delete(name: String)

    suspend fun list(prefix: String): List<String>

    /** Free space in bytes, or null when the provider cannot say. */
    suspend fun freeSpace(): Long?
}
