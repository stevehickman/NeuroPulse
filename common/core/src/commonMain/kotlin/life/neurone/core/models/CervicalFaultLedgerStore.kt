package life.neurone.core.models

import life.neurone.core.common.KeyValueStore
import life.neurone.core.common.PresenceLock
import life.neurone.core.common.PresenceState
import life.neurone.core.common.SecretKeyUnavailableException
import life.neurone.core.protocol.PersistedKeys
import kotlinx.serialization.builtins.MapSerializer
import kotlinx.serialization.builtins.serializer
import kotlinx.serialization.json.Json

/**
 * Where each person's cardiac-fault acknowledgement point is kept (`OI-PRIV-19`, `CLAUDE.md` §5.1: that a person had a cardiac cutoff is
 * about the person, so it is UHDR). **One sealed value under [PersistedKeys.CERVICAL_FAULT_LEDGER_KEY]**: a JSON object of the person's
 * tag to the last session acknowledged. The ledger used to be one key per person (the key plus `.` plus the tag), and a key with a suffix
 * matches nothing on the protected list, so every one was stored in the clear and outside the presence gate, and the suffix itself said
 * which people used the device. A key of that old form is read once, moved into the sealed value and removed.
 *
 * Under the presence gate (`REQ-PRES-01` (d)): while locked nothing is read, so a person's ledger reads as having acknowledged nothing,
 * which is the safe answer (every offline fault shows and a cervical restart stays blocked, `CervicalFaultLedger.blocksCervicalRestart`).
 * An acknowledgement made then is kept in memory and joins the sealed value at the unlock; writing it would replace every other
 * person's point with defaults. [onUnlocked] runs after the join, so the caller can show what is now unacknowledged.
 */
class CervicalFaultLedgerStore(
    private val store: KeyValueStore,
    private val lock: PresenceLock? = null,
    private val onUnlocked: () -> Unit = {},
) {
    private val json = Json { ignoreUnknownKeys = true }
    private val serializer = MapSerializer(String.serializer(), Long.serializer())

    /** Acknowledgements made while the sealed value could not be read: name to session, null for a clear. */
    private val held = linkedMapOf<String, Long?>()

    private val readable: Boolean get() = lock == null || lock.isUnlocked

    init {
        lock?.onStateChanged { state ->
            if (state == PresenceState.UNLOCKED) {
                if (held.isNotEmpty()) {
                    val map = readMap().toMutableMap()
                    held.forEach { (name, n) -> if (n == null) map.remove(name) else map[name] = n }
                    writeMap(map)
                    held.clear()
                }
                onUnlocked()
            }
        }
    }

    /** [tag]'s ledger: the person's own point, or nothing acknowledged when it cannot be read. */
    fun ledgerFor(tag: Long?): CervicalFaultLedger {
        val name = nameOf(tag)
        if (!readable) return CervicalFaultLedger(held[name])
        return CervicalFaultLedger(readMap(migratingLegacy = name)[name])
    }

    /** Keep [ledger] as [tag]'s point. */
    fun set(tag: Long?, ledger: CervicalFaultLedger) {
        val name = nameOf(tag)
        val n = ledger.lastAcknowledgedSession
        if (!readable) {
            held[name] = n
            return
        }
        val map = readMap(migratingLegacy = name).toMutableMap()
        if (n == null) map.remove(name) else map[name] = n
        writeMap(map)
    }

    private fun nameOf(tag: Long?): String = tag?.toString() ?: UNNAMED

    /** The stored map, with [migratingLegacy]'s old per-person key folded in and removed. A value that does not read is an empty map: nothing acknowledged. */
    private fun readMap(migratingLegacy: String? = null): Map<String, Long> {
        val map = store.getString(PersistedKeys.CERVICAL_FAULT_LEDGER_KEY)
            ?.let { runCatching { json.decodeFromString(serializer, it) }.getOrNull() }
            ?.toMutableMap()
            ?: mutableMapOf()
        if (migratingLegacy != null) {
            val legacyKey = PersistedKeys.CERVICAL_FAULT_LEDGER_KEY + "." + migratingLegacy
            val legacy = store.getString(legacyKey)
            if (legacy != null) {
                legacy.toLongOrNull()?.let { map.putIfAbsent(migratingLegacy, it) }
                if (legacy.toLongOrNull() != null && map[migratingLegacy] != null) {
                    writeMap(map)
                    store.remove(legacyKey)
                }
            }
        }
        return map
    }

    private fun writeMap(map: Map<String, Long>) {
        if (!readable) return
        try {
            if (map.isEmpty()) store.remove(PersistedKeys.CERVICAL_FAULT_LEDGER_KEY)
            else store.putString(PersistedKeys.CERVICAL_FAULT_LEDGER_KEY, json.encodeToString(serializer, map))
        } catch (e: SecretKeyUnavailableException) {
            if (lock == null) throw e
        }
    }

    companion object {
        /** The name of the ledger when no person is named. */
        const val UNNAMED = "unnamed"
    }
}
