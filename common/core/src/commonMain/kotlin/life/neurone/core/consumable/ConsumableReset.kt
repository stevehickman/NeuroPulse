package life.neurone.core.consumable

import life.neurone.core.common.KeyValueStore
import life.neurone.core.common.PresenceLock
import life.neurone.core.common.PresenceState
import life.neurone.core.common.SecretKeyUnavailableException
import life.neurone.core.protocol.PersistedKeys

/**
 * The replacement reset for CONSUMABLE_STATUS (OI-ACC-08, GitHub #381). Port of iOS
 * ConsumableReset.swift.
 *
 * The hub owns the counts, and each is "sessions since this part was replaced". Mark replaced
 * zeroes the app's copy at once, but the next CONSUMABLE_STATUS notification overwrites it
 * with the hub's absolute value, so the hub must be told: a 1-byte WRITE to 0x0007 naming the
 * kind (its [ConsumableKind] raw value, 0–3). The hub zeroes that count and notifies the zero.
 *
 * A replacement marked while no hub is connected is queued here, persisted, and written at
 * the next connect. It is removed from the queue when the write is issued. Known limit: a
 * Mode 3 session run between the replacement and that connect is zeroed with the old part's
 * count — an undercount of the new part by the offline sessions.
 */
object ConsumableResetWire {
    /** The WRITE value for [kind], or null for a kind the hub does not count. */
    fun encode(kind: Int): ByteArray? =
        if (kind in 0 until ConsumableKind.entries.size) byteArrayOf(kind.toByte()) else null
}

class ConsumableResetQueue(
    private val store: KeyValueStore,
    /**
     * The presence lock when the host gates UHDR (`NP-APP-PRESENCE-001`), or null when it does not. The queue is sealed, so while
     * locked the kinds already queued cannot be read. A replacement marked then is kept in memory and joins the stored queue at the
     * unlock; writing it would replace the sealed queue, and an earlier replacement the hub was never told about would be lost
     * (`REQ-PRES-01` (d), `OI-PRIV-19`). [onUnlocked] runs after the join, so the caller can write what is now queued.
     */
    private val lock: PresenceLock? = null,
    private val onUnlocked: () -> Unit = {},
) {
    companion object {
    }

    /** Kinds replaced while the queue could not be read, joined to the stored ones at the next unlock. */
    private val held = mutableSetOf<Int>()

    private val readable: Boolean get() = lock == null || lock.isUnlocked

    init {
        lock?.onStateChanged { state ->
            if (state == PresenceState.UNLOCKED) {
                if (held.isNotEmpty()) save((stored + held).distinct().sorted())
                held.clear()
                onUnlocked()
            }
        }
    }

    private val stored: List<Int>
        get() = store.getString(PersistedKeys.CONSUMABLE_RESET_KEY)
            ?.split(",")
            ?.mapNotNull { it.toIntOrNull() }
            ?.filter { ConsumableResetWire.encode(it) != null }
            ?.distinct()
            ?.sorted()
            ?: emptyList()

    /** Kinds replaced and not yet written to the hub, in kind order, no duplicates. While locked, only the ones marked since the lock closed. */
    val pending: List<Int>
        get() = if (readable) (stored + held).distinct().sorted() else held.sorted()

    fun add(kind: Int) {
        if (ConsumableResetWire.encode(kind) == null) return
        if (!readable) {
            held += kind
            return
        }
        save((pending + kind).distinct().sorted())
    }

    /** Hands every pending kind to [write] and forgets it. Locked, that is the kinds held in memory; the sealed ones wait for the unlock. */
    fun drain(write: (ByteArray) -> Unit) {
        val kinds = pending
        if (kinds.isEmpty()) return
        if (readable) save(emptyList()) else held.clear()
        kinds.forEach { k -> ConsumableResetWire.encode(k)?.let(write) }
    }

    private fun save(kinds: List<Int>) {
        if (!readable) return
        try {
            if (kinds.isEmpty()) store.remove(PersistedKeys.CONSUMABLE_RESET_KEY) else store.putString(PersistedKeys.CONSUMABLE_RESET_KEY, kinds.joinToString(","))
        } catch (e: SecretKeyUnavailableException) {
            if (lock == null) throw e
        }
    }
}
