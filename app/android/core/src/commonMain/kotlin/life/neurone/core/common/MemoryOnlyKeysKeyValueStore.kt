package life.neurone.core.common

import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.npps.NppsCore
import life.neurone.core.protocol.PersistedKeys

// A host with no keystore to seal a key in, and no storage the platform keeps out of reach of other code, can still choose
// not to keep a person's health-derived records at all. The browser is that host: its only durable store is `localStorage`,
// which any script on the page, any extension and any injected code can read in the clear, and which no backup rule governs.
// Data that is not written cannot be read out of it (privacy review, Finding 1).

/**
 * A [KeyValueStore] that keeps the values of its [memoryOnlyKeys] in memory for the life of the process and writes them nowhere,
 * and passes every other key through to [inner] unchanged.
 *
 * **Anything an earlier build left under a memory-only key in [inner] is removed when this store is made**, so a value written
 * before the key became memory-only does not stay readable in the host's store. That is done in the constructor, so a host
 * cannot forget it.
 *
 * A memory-only key reads as absent after a restart, which is the point: the data is not on the device. Anything built on such a
 * key must treat absent as an empty start (the session history does: `SessionHistoryStore` starts clean when it finds nothing).
 */
class MemoryOnlyKeysKeyValueStore(
    private val inner: KeyValueStore,
    private val memoryOnlyKeys: Set<String>,
    /**
     * Whether to remove what [inner] holds under the memory-only keys. True for the browser, where an earlier build's plaintext
     * must not stay readable. **False for [ProtectedStorage]'s locked state**, where what [inner] holds under those keys is the
     * person's sealed data that this run cannot open and must leave exactly as it is.
     */
    removeStaleFromInner: Boolean = true,
) : KeyValueStore {
    private val memory = mutableMapOf<String, String>()

    init {
        if (removeStaleFromInner) memoryOnlyKeys.forEach { inner.remove(it) }
    }

    private fun held(key: String) = key in memoryOnlyKeys

    override fun getString(key: String): String? = if (held(key)) memory[key] else inner.getString(key)

    override fun putString(key: String, value: String) {
        if (held(key)) memory[key] = value else inner.putString(key, value)
    }

    override fun getInt(key: String, default: Int): Int =
        if (held(key)) memory[key]?.toIntOrNull() ?: default else inner.getInt(key, default)

    override fun putInt(key: String, value: Int) {
        if (held(key)) memory[key] = value.toString() else inner.putInt(key, value)
    }

    override fun getBoolean(key: String, default: Boolean): Boolean =
        if (held(key)) memory[key]?.toBooleanStrictOrNull() ?: default else inner.getBoolean(key, default)

    override fun putBoolean(key: String, value: Boolean) {
        if (held(key)) memory[key] = value.toString() else inner.putBoolean(key, value)
    }

    override fun remove(key: String) {
        if (held(key)) memory.remove(key) else inner.remove(key)
    }

    companion object {
        /**
         * The keys the browser keeps in memory only: the session history, and every consent key, from the core's one list
         * (`itemKeys`, `consent`). The consent keys hold the research categories the person ticked, the clinicians they granted,
         * their study answers and the contact they gave (a mail address or a number), so they are the person's data, and the
         * browser has no keystore to seal them in. What the browser still keeps (the onboarding records, the saved protocols
         * and limits, the per-install keys) is listed in `common/npps/migration-keys.json` and is a separate decision.
         *
         * **A consent withdrawal is therefore not remembered across a reload in the browser** (a person who said stop reads as
         * never asked there, `CLAUDE.md` §6.0). That cannot let data flow: the browser has no analytics vendor, no clinician
         * channel, and a study-descriptor verifier that refuses everything (`OI-CONSENT-07`). A browser flow that could
         * start one has to settle this first.
         */
        fun browserKeys(): Set<String> =
            setOf(PersistedKeys.SESSION_HISTORY_KEY) +
                NppsCore.migration(buildJsonObject { put("op", "itemKeys") })["consent"]!!.jsonArray.map { it.jsonPrimitive.content }
    }
}
