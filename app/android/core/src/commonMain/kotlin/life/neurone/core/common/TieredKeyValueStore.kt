package life.neurone.core.common

// The sealed state in two tiers (NP-APP-PRESENCE-001 §3). The **device tier** is sealed under the data key the device releases on its own,
// so it stays readable while the app is locked; the **UHDR tier** is sealed under the gated key, so it is readable only while the person's
// presence stands (`REQ-PRES-01`, `-05`). Every key not in either tier is passed to the host store as it is.
//
// While locked a UHDR key reads as absent, never as ciphertext. A write made while locked is held in memory, nothing on disk changes, and
// it is applied when the person unlocks, in the order it was made; a write is never silently dropped (a withdrawal of consent made while
// locked must survive, `CLAUDE.md` §6.0). A removal acts on the host store at once, since it discloses nothing.

class TieredKeyValueStore(
    private val inner: KeyValueStore,
    private val device: EncryptedKeyValueStore,
    private val uhdr: EncryptedKeyValueStore,
    private val deviceKeys: Set<String>,
    private val uhdrKeys: Set<String>,
    private val lock: PresenceLock,
) : KeyValueStore {
    /** Writes held while locked, in order. A later write to the same key replaces the earlier one in place. */
    private val pending = LinkedHashMap<String, String>()

    init {
        require(deviceKeys.none { it in uhdrKeys }) { "a key is in one tier only" }
        lock.onStateChanged { state ->
            if (state == PresenceState.LOCKED) uhdr.forgetKey() else applyPending()
        }
    }

    private fun isUhdr(key: String) = key in uhdrKeys
    private fun open() = lock.isUnlocked

    /** Apply what was held, now that the key can be read. A write that fails stays held, and the ones after it with it. */
    private fun applyPending() {
        uhdr.migrate()
        val it = pending.entries.iterator()
        while (it.hasNext()) {
            val (k, v) = it.next()
            try { uhdr.putString(k, v) } catch (e: SecretKeyUnavailableException) { return }
            it.remove()
        }
    }

    override fun getString(key: String): String? = when {
        key in deviceKeys -> device.getString(key)
        isUhdr(key) -> if (open()) uhdr.getString(key) else pending[key]
        else -> inner.getString(key)
    }

    override fun putString(key: String, value: String) {
        when {
            key in deviceKeys -> device.putString(key, value)
            isUhdr(key) -> if (open()) { applyPending(); uhdr.putString(key, value) } else { pending.remove(key); pending[key] = value }
            else -> inner.putString(key, value)
        }
    }

    override fun getInt(key: String, default: Int): Int = when {
        key in deviceKeys -> device.getInt(key, default)
        isUhdr(key) -> if (open()) uhdr.getInt(key, default) else pending[key]?.toIntOrNull() ?: default
        else -> inner.getInt(key, default)
    }

    override fun putInt(key: String, value: Int) {
        if (key in deviceKeys || isUhdr(key)) putString(key, value.toString()) else inner.putInt(key, value)
    }

    override fun getBoolean(key: String, default: Boolean): Boolean = when {
        key in deviceKeys -> device.getBoolean(key, default)
        isUhdr(key) -> if (open()) uhdr.getBoolean(key, default) else pending[key]?.let { it.toBooleanStrictOrNull() ?: when (it) { "1" -> true; "0" -> false; else -> null } } ?: default
        else -> inner.getBoolean(key, default)
    }

    override fun putBoolean(key: String, value: Boolean) {
        if (key in deviceKeys || isUhdr(key)) putString(key, value.toString()) else inner.putBoolean(key, value)
    }

    override fun remove(key: String) {
        pending.remove(key)
        inner.remove(key)
    }
}
