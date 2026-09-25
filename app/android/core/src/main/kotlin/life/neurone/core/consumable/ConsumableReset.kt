package life.neurone.core.consumable

import life.neurone.core.common.KeyValueStore

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

class ConsumableResetQueue(private val store: KeyValueStore) {
    companion object {
        const val KEY = "np.consumable.pending-resets"
    }

    /** Kinds replaced and not yet written to the hub, in kind order, no duplicates. */
    val pending: List<Int>
        get() = store.getString(KEY)
            ?.split(",")
            ?.mapNotNull { it.toIntOrNull() }
            ?.filter { ConsumableResetWire.encode(it) != null }
            ?.distinct()
            ?.sorted()
            ?: emptyList()

    fun add(kind: Int) {
        if (ConsumableResetWire.encode(kind) == null) return
        save((pending + kind).distinct().sorted())
    }

    /** Hands every pending kind to [write] and forgets it. */
    fun drain(write: (ByteArray) -> Unit) {
        val kinds = pending
        if (kinds.isEmpty()) return
        save(emptyList())
        kinds.forEach { k -> ConsumableResetWire.encode(k)?.let(write) }
    }

    private fun save(kinds: List<Int>) {
        if (kinds.isEmpty()) store.remove(KEY) else store.putString(KEY, kinds.joinToString(","))
    }
}
