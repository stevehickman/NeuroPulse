package life.neurone.core.common

// How the app's UHDR-class state is held on this run, decided once, in common code, for every host (privacy review, desktop audit
// item 1). Before this, a launch on which the host's keystore could not be used fell back to the host's plain store. That is right
// for a host that never sealed anything, and wrong for one that had: the sealed values then read as raw ciphertext (consent
// reads as never asked, the history as empty), and every new write replaced a sealed value with plaintext. The earlier state was
// destroyed and the new state was unprotected, with nothing to tell the person.

/** How the protected keys are held on this run. */
enum class StorageProtection {
    /** Sealed at rest under the data key in the host's keystore (SWR-MG-06). */
    SEALED,

    /**
     * The host has no usable keystore and holds no sealed value: the protected keys are kept as the host stores them, as they
     * always were on this host. The person is told when the host is one that keeps state on disk.
     */
    UNSEALED,

    /**
     * Sealed values are on disk and this run cannot open them (no usable keystore, or it failed). The protected keys are held in
     * memory for this run and **nothing is written to them on disk**, so the sealed values stay exactly as they were and open
     * again on a run where the keystore works. What is done in this run under those keys is lost when it ends, and the person
     * is told so.
     */
    LOCKED,
}

/** The store the app reads and writes through, with how it is protected on this run. */
class ProtectedStorage private constructor(
    val store: KeyValueStore,
    val state: StorageProtection,
    /** The sealing store, or null unless [state] is [StorageProtection.SEALED]: what the clear's key destruction acts on. */
    val sealing: EncryptedKeyValueStore?,
) {
    companion object {
        /**
         * @param host the host's own durable store.
         * @param secrets the host's keystore, or null if it has none.
         * @param protectedKeys the keys to seal. A function, because the list comes from the shared core, which a target without
         *   its binding cannot call; then nothing can be sealed or told apart, and the host's store is used as it is.
         */
        fun open(host: KeyValueStore, secrets: SecretKeyStore?, protectedKeys: () -> Set<String>): ProtectedStorage {
            val keys = runCatching(protectedKeys).getOrNull()
                ?: return ProtectedStorage(host, StorageProtection.UNSEALED, null)
            if (secrets != null) {
                val sealing = runCatching { EncryptedKeyValueStore(host, secrets, keys).also { it.migrate() } }.getOrNull()
                if (sealing != null) return ProtectedStorage(sealing, StorageProtection.SEALED, sealing)
            }
            return if (holdsSealedValue(host, keys)) {
                ProtectedStorage(MemoryOnlyKeysKeyValueStore(host, keys, removeStaleFromInner = false), StorageProtection.LOCKED, null)
            } else {
                ProtectedStorage(host, StorageProtection.UNSEALED, null)
            }
        }

        /** Whether any protected key in [host] holds a sealed value. A key stored as another type is not a sealed value. */
        fun holdsSealedValue(host: KeyValueStore, keys: Set<String>): Boolean = keys.any { key ->
            val stored = try { host.getString(key) } catch (ignored: ClassCastException) { null }
            stored?.startsWith(EncryptedKeyValueStore.PREFIX) == true
        }
    }
}
