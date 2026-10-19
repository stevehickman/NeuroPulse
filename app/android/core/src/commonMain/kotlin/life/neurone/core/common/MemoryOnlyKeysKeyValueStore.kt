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
         * The keys the browser keeps in memory only: **every key the core lists as sealed at rest** (`itemKeys`, `sealedAtRest`),
         * which is every key the apps store. Other hosts seal those keys under a key in the platform's keystore (SWR-MG-06,
         * SWR-MG-16); the browser has no keystore, and `localStorage` is readable in the clear by every script on the page and every
         * extension, so it holds none of them. That is the session history, the consent state, the saved protocols and limits, the
         * research enrolment, the onboarding records (age, biometric-privacy consent) and the per-install keys. Using the core's one
         * list means a key added to it is covered here without a second edit.
         *
         * **The page forgets all of it on a reload**: the person answers the onboarding screens again, and keeps their protocols and
         * limits through the backup file the browser can make and read (REQ-SW03-18 (b)). A consent withdrawal is not remembered
         * across a reload either (a person who said stop reads as never asked there, `CLAUDE.md` §6.0). That cannot let data flow:
         * the browser has no analytics vendor, no clinician channel and a study-descriptor verifier that refuses everything
         * (`OI-CONSENT-07`). A browser flow that could start one has to settle this first.
         */
        fun browserKeys(): Set<String> =
            NppsCore.migration(buildJsonObject { put("op", "itemKeys") })["sealedAtRest"]!!.jsonArray.map { it.jsonPrimitive.content }.toSet()
    }
}
