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
    /** The keys [store] keeps off the disk in the clear on this run: sealed ([StorageProtection.SEALED]) or held in memory ([StorageProtection.LOCKED]). Null is every key (the all-in-memory run); empty when [StorageProtection.UNSEALED]. */
    private val keptFromTheClear: Set<String>?,
    /** The presence lock of a gated run, or null on a run with no gate. A store that holds sealed values asks it before it reads or writes them (`OI-PRIV-19`). */
    val presenceLock: PresenceLock? = null,
) {
    /** Whether the person's sealed values can be read now: true when there is no gate, or the gate is open. */
    val isReadable: Boolean get() = presenceLock?.isUnlocked != false

    /**
     * Whether a value written under [key] through [store] on this run can never land on disk in the clear: the key is sealed at rest
     * ([StorageProtection.SEALED]) or held in memory for the run ([StorageProtection.LOCKED]). False for every key of an
     * [StorageProtection.UNSEALED] run, and for a key that is not on the protected list. A store of secrets asks this before it is
     * built (`SWR-MG-19` (l)), so the requirement is checked where the state is known and not left to the caller's comment.
     */
    fun keepsFromTheClear(key: String): Boolean =
        state != StorageProtection.UNSEALED && (keptFromTheClear == null || key in keptFromTheClear)

    companion object {
        /**
         * @param host the host's own durable store.
         * @param secrets the host's keystore, or null if it has none.
         * @param protectedKeys the keys to seal. A function, because the list comes from the shared core, which a target without
         *   its binding cannot call; then nothing can be sealed or told apart, and the host's store is used as it is.
         */
        fun open(host: KeyValueStore, secrets: SecretKeyStore?, protectedKeys: () -> Set<String>): ProtectedStorage {
            return openInternal(host, secrets, protectedKeys, gateOffered = false)
        }

        /**
         * The same, on a host that offers the presence gate (it supplies a [GatedSecretKeyStore]) and opens ungated on this run. **It
         * fails closed** (`REQ-PRES-10` (b), `OI-PRIV-62` H2): if a protected value is sealed and does not open under [secrets], it was
         * sealed under a key this run does not hold (the gated key, whose flag or record was lost), and opening it ungated would read it
         * as absent and seal the next write over it under a key the device releases alone, the downgrade `REQ-PRES-09` forbids. The run
         * is [StorageProtection.LOCKED] instead: the protected keys are held in memory and nothing on disk changes. Values left by a clear
         * that destroyed the key and stopped part-way (`SWR-MG-08`) are not counted: the clear's own progress says the key is gone.
         *
         * Only a host that offers the gate fails closed this way. On one that does not, a value that does not open under its key was
         * sealed under a key the keystore lost, and reading it as absent is what that host has always done (`SWR-MG-06`).
         */
        fun openOnGateHost(host: KeyValueStore, secrets: SecretKeyStore?, protectedKeys: () -> Set<String>): ProtectedStorage {
            return openInternal(host, secrets, protectedKeys, gateOffered = true)
        }

        /**
         * Whether [host] holds a protected value that is sealed and does not open under [secrets]: evidence that the gate is on whatever
         * its flag says (`REQ-PRES-10` (b)), because the only key this install seals under besides [secrets] is the gated key. A keystore
         * that cannot be asked answers true (it cannot be shown otherwise). Values a part-way clear left under a destroyed key do not
         * count. Reads only; the probe's key is wiped.
         */
        fun holdsValueSealedElsewhere(host: KeyValueStore, secrets: SecretKeyStore, protectedKeys: Set<String>): Boolean {
            if (clearDestroyedTheKey(host)) return false
            val sealed = protectedKeys.filter { isSealed(host, it) }
            if (sealed.isEmpty()) return false
            val probe = EncryptedKeyValueStore(host, secrets, sealed.toSet())
            return try { sealed.any { probe.getString(it) == null } } finally { probe.forgetKey() }
        }

        /** The gate's own records (`REQ-PRES-10` (a)): never sealed, whatever list a caller passes. Empty only when the core cannot be asked. */
        private fun hostPlain(): Set<String> = runCatching { EncryptedKeyValueStore.hostPlainKeys() }.getOrDefault(emptySet())

        /** Whether a clear has destroyed the data key and not yet finished (`SWR-MG-08`): the sealed values it left are under no key. */
        private fun clearDestroyedTheKey(host: KeyValueStore): Boolean {
            val progressKey = runCatching { EncryptedKeyValueStore.clearProgressKey() }.getOrNull() ?: return false
            val progress = try { host.getString(progressKey) } catch (ignored: ClassCastException) { null } ?: return false
            return progress.split(',').any { it.trim() == "destroyEncryptionKey" }
        }

        private fun isSealed(host: KeyValueStore, key: String): Boolean =
            (try { host.getString(key) } catch (ignored: ClassCastException) { null })?.startsWith(EncryptedKeyValueStore.PREFIX) == true

        /**
         * The same, with the person's presence required for the UHDR tier (`NP-APP-PRESENCE-001`): the device tier (`deviceTierKeys`) is
         * sealed under [secrets] and stays readable locked; every other protected key is sealed under [gated] and reads as absent until
         * [lock] is unlocked. Values sealed under the earlier, ungated key ([secrets]) still read, and the first unlock seals each of
         * them again under the gated key (`REQ-PRES-07`, [EncryptedKeyValueStore.resealFromLegacy]).
         */
        fun openGated(
            host: KeyValueStore,
            secrets: SecretKeyStore,
            gated: GatedSecretKeyStore,
            lock: PresenceLock,
            protectedKeys: () -> Set<String>,
            deviceTierKeys: () -> Set<String>,
        ): ProtectedStorage {
            // A gated install that cannot get the list of protected keys cannot tell a sealed value from a plain one, so it can
            // neither read the host's store (a sealed value would read as text) nor write to it (a UHDR value would land in the
            // clear over the sealed one). The whole run is held in memory and the host's store is left exactly as it is. The
            // ungated [open] keeps the host's store in this case; a gated install must not (`REQ-PRES-09` (b)).
            val all = (runCatching(protectedKeys).getOrNull() ?: return ProtectedStorage(InMemoryKeyValueStore(), StorageProtection.LOCKED, null, null, lock)) - hostPlain()
            val deviceKeys = runCatching(deviceTierKeys).getOrNull()?.intersect(all) ?: emptySet()
            val uhdrKeys = all - deviceKeys
            // A gated install that cannot open its tiers fails closed: the protected keys are held in memory for this run and nothing
            // is written in the clear. It never falls back to the ungated or unsealed store, which would lower the protection UHDR has
            // (`CLAUDE.md` §5.1, `REQ-PRES-09`).
            // [gated] is the device tier's `legacy`: a key the principal moved from the UHDR tier to this one (the analytics ID and its
            // erasure queue, 2026-11-21) may still be sealed under the gated key on an install that was gated before, and the first unlock
            // seals it again under the device key (`TieredKeyValueStore.applyPending`). Until then it reads as absent, never as ciphertext.
            val device = runCatching { EncryptedKeyValueStore(host, secrets, deviceKeys, legacy = gated).also { it.migrate() } }.getOrNull()
                ?: return ProtectedStorage(MemoryOnlyKeysKeyValueStore(host, all, removeStaleFromInner = false), StorageProtection.LOCKED, null, all, lock)
            val uhdr = EncryptedKeyValueStore(host, gated, uhdrKeys, siblings = listOf(device), legacy = secrets)
            return ProtectedStorage(TieredKeyValueStore(host, device, uhdr, deviceKeys, uhdrKeys, lock), StorageProtection.SEALED, uhdr, all, lock)
        }

        private fun openInternal(host: KeyValueStore, secrets: SecretKeyStore?, protectedKeys: () -> Set<String>, gateOffered: Boolean): ProtectedStorage {
            val keys = (runCatching(protectedKeys).getOrNull()
                ?: return ProtectedStorage(host, StorageProtection.UNSEALED, null, emptySet())) - hostPlain()
            if (secrets != null) {
                val sealing = runCatching {
                    // Building the store does not touch the keystore when there is nothing plain to seal, so ask it once here: a
                    // keystore that cannot be asked would otherwise fail on the first read or write. A throw is "could not ask"
                    // and the run is LOCKED (or UNSEALED if nothing is sealed); a null answer is "no key", not a failure: after a
                    // clear destroyed the key the store must still be built, so that a clear interrupted part-way resumes at the
                    // next launch (SWR-MG-08).
                    secrets.load()?.fill(0)
                    // Asked before anything is written: a value sealed under the gated key is left exactly as it is (`REQ-PRES-10` (b)).
                    if (gateOffered && holdsValueSealedElsewhere(host, secrets, keys)) return lockedRun(host, keys)
                    EncryptedKeyValueStore(host, secrets, keys).also { it.migrate() }
                }.getOrNull()
                if (sealing != null) return ProtectedStorage(sealing, StorageProtection.SEALED, sealing, keys)
            }
            return if (holdsSealedValue(host, keys)) {
                lockedRun(host, keys)
            } else {
                ProtectedStorage(host, StorageProtection.UNSEALED, null, emptySet())
            }
        }

        private fun lockedRun(host: KeyValueStore, keys: Set<String>) =
            ProtectedStorage(MemoryOnlyKeysKeyValueStore(host, keys, removeStaleFromInner = false), StorageProtection.LOCKED, null, keys)

        /** Whether any protected key in [host] holds a sealed value. A key stored as another type is not a sealed value. */
        fun holdsSealedValue(host: KeyValueStore, keys: Set<String>): Boolean = keys.any { key ->
            val stored = try { host.getString(key) } catch (ignored: ClassCastException) { null }
            stored?.startsWith(EncryptedKeyValueStore.PREFIX) == true
        }
    }
}
