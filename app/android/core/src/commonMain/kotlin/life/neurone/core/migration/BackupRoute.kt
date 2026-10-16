package life.neurone.core.migration

import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put

// Where a bundle lives while it moves between phones, or waits as a backup (SWR-MG-02, SWR-MG-07, SWR-MG-08, SWR-MG-10).
//
// A route is a dumb store: it holds one encrypted bundle, one claim marker and one confirmation, and it knows nothing of
// what is inside. Every rule about them (who owns a claim, when the old phone may clear, whether a route may carry a
// migration at all) is the shared core's, asked through MigrationFlow. A route never sees plaintext and never
// reaches a NeurOne server: the cloud route is the user's own account (SWR-MG-10), the others are a local computer and a
// direct link.

interface RouteDescriptor {
    /** Stable name of the route, for the core's route check and for the screen. */
    val id: String

    /**
     * True only when [createClaimIfAbsent] is an exclusive create-if-absent that the store itself arbitrates, so that of
     * two simultaneous claims exactly one succeeds (SWR-MG-07). A route that cannot promise this says false, and the core
     * then withholds it from the migrate kind (SWR-MG-10) while leaving it usable for the UHDR-only backup.
     */
    val atomicClaim: Boolean

    /** A phone-to-phone link, which the core offers only with an authenticated pairing (SWR-MG-10). */
    val direct: Boolean get() = false
    val authenticatedPairing: Boolean get() = true
}

/**
 * A route whose five calls the app writes itself. Prefer [ObjectStoreRoute]: the core then holds which object is the bundle,
 * how a claim is encoded and how a missing object reads, and the app writes the six object calls once for every route.
 */
interface BackupRoute : RouteDescriptor {
    /** Write the bundle, replacing any earlier one. */
    suspend fun putBundle(bytes: ByteArray)

    /** The bundle on this route, or null when there is none. */
    suspend fun getBundle(): ByteArray?

    /** Create the claim marker if and only if none exists. True when this call created it. */
    suspend fun createClaimIfAbsent(claim: JsonObject): Boolean

    suspend fun readClaim(): JsonObject?

    /** Remove the claim marker. Called only on the person's explicit act, never on a timer (SWR-MG-07). */
    suspend fun releaseClaim()

    suspend fun writeConfirmation(confirmation: JsonObject)

    suspend fun readConfirmation(): JsonObject?
}

/**
 * A route held in memory, for tests and for a host that has no storage yet. Its claim is atomic only for callers on one
 * thread; a real route gets that from the store it writes to, which is the whole point of SWR-MG-07, so this one is not
 * offered to a person.
 */
class InMemoryBackupRoute(
    override val id: String = "memory",
    override val atomicClaim: Boolean = true,
    override val direct: Boolean = false,
    override val authenticatedPairing: Boolean = true,
) : BackupRoute {
    private var bundle: ByteArray? = null
    private var claim: JsonObject? = null
    private var confirmation: JsonObject? = null

    /** A test hook: drop the next confirmation write, as a flaky store would (SWR-MG-08 retries a write that did not land). */
    var dropConfirmationWrites = 0

    override suspend fun putBundle(bytes: ByteArray) { bundle = bytes.copyOf() }
    override suspend fun getBundle(): ByteArray? = bundle?.copyOf()

    override suspend fun createClaimIfAbsent(claim: JsonObject): Boolean {
        if (this.claim != null) return false
        this.claim = claim
        return true
    }

    override suspend fun readClaim(): JsonObject? = claim
    override suspend fun releaseClaim() { claim = null }

    override suspend fun writeConfirmation(confirmation: JsonObject) {
        if (dropConfirmationWrites > 0) { dropConfirmationWrites--; return }
        this.confirmation = confirmation
    }

    override suspend fun readConfirmation(): JsonObject? = confirmation
}

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

/** A file the app holds, as [FileStore.list] reports it. */
class StoredFile(val path: String, val size: Long)

/**
 * The calls the old phone's clear asks of the app's files (SWR-MG-06), and nothing else: the core decides how a file is
 * overwritten (every byte, in chunks, then a sync, then the delete). [list] reports the UHDR-class files still there. A call
 * that cannot be done throws, and the clear stops at that step and resumes at the next launch.
 */
interface FileStore {
    suspend fun list(): List<StoredFile>

    /** Overwrite [bytes].size bytes of [path] starting at [offset], in place. */
    suspend fun write(path: String, offset: Long, bytes: ByteArray)

    /** The writes so far have reached storage. */
    suspend fun sync(path: String)

    /** Not an error when there is no such file. */
    suspend fun delete(path: String)
}

/**
 * A route that is an object store: the descriptor the flow's `route` param carries. The store itself is handed to
 * [MigrationFlow.run] as `store`, and the core then drives the route; the typed calls of [BackupRoute] never reach it.
 */
class ObjectStoreRoute(
    override val id: String,
    override val atomicClaim: Boolean,
    override val direct: Boolean = false,
    override val authenticatedPairing: Boolean = true,
) : RouteDescriptor

/** The descriptor a flow's `route` param carries: the core offers or withholds the route from it (SWR-MG-10). */
fun RouteDescriptor.toJson(): JsonObject = buildJsonObject {
    put("id", id)
    put("atomicClaim", atomicClaim)
    put("direct", direct)
    put("authenticatedPairing", authenticatedPairing)
}
