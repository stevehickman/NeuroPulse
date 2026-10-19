package life.neurone.core.common

import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.boolean
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.data.RandomSource
import life.neurone.core.data.SystemRandomSource
import life.neurone.core.data.toHex
import life.neurone.core.npps.NppsCore

// Encryption at rest for the UHDR-class state the app holds (SWR-MG-06, issue #569 package 6r). The sealing is the shared
// core's (`at_rest.rs`: AES-256-GCM, the value's name as associated data), so every platform writes the same bytes. What is
// each platform's is where the 32-byte data key lives: a [SecretKeyStore] over the operating system's keystore. Destroying the
// key there is what makes the data unreadable, which is why the clear's first step is to destroy it.

/**
 * Where the data key lives: the platform's keystore (the Android Keystore, the iOS Keychain, the desktop's keychain). A
 * store that cannot keep a secret the way the platform's keystore does must not implement this: the app then holds its data
 * unencrypted and says the device cannot clear itself, rather than claim a protection it does not give.
 */
interface SecretKeyStore {
    /**
     * The data key, or null when the keystore answered and there is none (never made, or destroyed). **Null means "no key" and
     * nothing else**: when the keystore cannot be asked or its answer cannot be read (a timeout, a locked keychain, a tool that
     * fails), throw [SecretKeyUnavailableException]. A caller that took "could not ask" for "no key" would make a new key over the
     * real one, and everything sealed under the real one would be lost (OI-DESKTOP-01, item 3).
     */
    fun load(): ByteArray?

    /** Keep [key], replacing any earlier one. */
    fun store(key: ByteArray)

    /** Destroy the key in the keystore, not merely forget it: what it protected is unreadable from here on. */
    fun destroy()
}

/** The keystore could not be asked for the data key, or what it answered cannot be read. The key may well exist: it is not "no key". */
class SecretKeyUnavailableException(message: String, cause: Throwable? = null) : RuntimeException(message, cause)

/** A key held in memory, for tests. */
class InMemorySecretKeyStore : SecretKeyStore {
    private var key: ByteArray? = null
    override fun load(): ByteArray? = key?.copyOf()
    override fun store(key: ByteArray) { this.key = key.copyOf() }
    override fun destroy() { key?.fill(0); key = null }
}

/**
 * A [KeyValueStore] that seals the values of its [protectedKeys] under the data key and leaves every other key as it is. The
 * data key is made the first time a protected value is written and kept in [secrets]; a value sealed under a key that no
 * longer exists reads as absent, and so does one while the keystore cannot be asked (it opens again when it can). A write while the
 * keystore cannot be asked throws [SecretKeyUnavailableException] and replaces nothing. A plain value found under a protected key (from before this store existed) is read as it is
 * and sealed by [migrate].
 */
class EncryptedKeyValueStore(
    private val inner: KeyValueStore,
    private val secrets: SecretKeyStore,
    private val protectedKeys: Set<String>,
    private val random: RandomSource = SystemRandomSource,
) : KeyValueStore {
    private var cached: ByteArray? = null

    /** The data key, or null if the keystore says there is none. Throws [SecretKeyUnavailableException] if it could not be asked. */
    private fun key(): ByteArray? = cached ?: secrets.load()?.also { cached = it }

    /**
     * A key is made only when the keystore answered that there is none. If it could not be asked, [key] throws and nothing is made
     * or stored, so the real key, which may be there, is not replaced (OI-DESKTOP-01, item 3). The write fails instead.
     */
    private fun ensureKey(): ByteArray {
        key()?.let { return it }
        val fresh = random.nextBytes(KEY_BYTES)
        require(fresh.size == KEY_BYTES) { "The random source gave ${fresh.size} bytes, not $KEY_BYTES" }
        secrets.store(fresh)
        cached = fresh
        return fresh
    }

    private fun ask(op: String, build: kotlinx.serialization.json.JsonObjectBuilder.() -> Unit): JsonObject =
        NppsCore.migration(buildJsonObject { put("op", op); build() })

    private fun seal(name: String, value: String): String {
        val nonce = random.nextBytes(NONCE_BYTES)
        require(nonce.size == NONCE_BYTES) { "The random source gave ${nonce.size} bytes, not $NONCE_BYTES" }
        val sealed = ask("sealValue") { put("key", ensureKey().toHex()); put("nonce", nonce.toHex()); put("name", name); put("value", value) }
        return PREFIX + sealed["sealed"]!!.jsonPrimitive.content
    }

    /** The value as it was written, or null if there is none or it cannot be opened (the key is gone, or it was changed). */
    private fun read(name: String): String? {
        val stored = try { inner.getString(name) } catch (ignored: ClassCastException) { null } ?: return null
        if (!stored.startsWith(PREFIX)) return stored
        val k = (try { key() } catch (ignored: SecretKeyUnavailableException) { null }) ?: return null
        val r = ask("openValue") { put("key", k.toHex()); put("name", name); put("sealed", stored.removePrefix(PREFIX)) }
        return if (r["ok"]!!.jsonPrimitive.boolean) r["value"]!!.jsonPrimitive.content else null
    }

    override fun getString(key: String): String? = if (key in protectedKeys) read(key) else inner.getString(key)

    override fun putString(key: String, value: String) {
        if (key in protectedKeys) inner.putString(key, seal(key, value)) else inner.putString(key, value)
    }

    override fun getInt(key: String, default: Int): Int =
        if (key in protectedKeys) read(key)?.toIntOrNull() ?: legacy { inner.getInt(key, default) } ?: default else inner.getInt(key, default)

    override fun putInt(key: String, value: Int) {
        if (key in protectedKeys) putString(key, value.toString()) else inner.putInt(key, value)
    }

    override fun getBoolean(key: String, default: Boolean): Boolean =
        if (key in protectedKeys) {
            read(key)?.let(::flagOf) ?: legacy { inner.getBoolean(key, default) } ?: default
        } else {
            inner.getBoolean(key, default)
        }

    override fun putBoolean(key: String, value: Boolean) {
        if (key in protectedKeys) putString(key, value.toString()) else inner.putBoolean(key, value)
    }

    override fun remove(key: String) = inner.remove(key)

    /** A flag as text: `true` and `false`, and `1` and `0`, which some host stores give for a flag they kept as a number. */
    private fun flagOf(text: String): Boolean? = when (text) { "1" -> true; "0" -> false; else -> text.toBooleanStrictOrNull() }

    /** A value written before this store existed, as an Int or a Boolean; null if there is none or it is not of that type. */
    private fun <T> legacy(read: () -> T): T? = try { read() } catch (ignored: ClassCastException) { null }

    /**
     * Seal every protected value still held in the clear, whatever type the host stored it as. Called once at start-up; a value
     * already sealed is left alone. A flag or a number from before this store existed is a Boolean or an Int in the host's own
     * store, not a string, so it is found by asking for it with two different defaults: only a value that is there answers both
     * the same. It is sealed as its text, and read back as the type it is asked for.
     */
    fun migrate() {
        for (name in protectedKeys) {
            val stored = try { inner.getString(name) } catch (ignored: ClassCastException) { null }
            if (stored != null) {
                if (!stored.startsWith(PREFIX)) putString(name, stored)
                continue
            }
            val flag = legacy { inner.getBoolean(name, false).takeIf { it == inner.getBoolean(name, true) } }
            val number = if (flag == null) legacy { inner.getInt(name, 0).takeIf { it == inner.getInt(name, 1) } } else null
            when {
                flag != null -> putString(name, flag.toString())
                number != null -> putString(name, number.toString())
            }
        }
    }

    /**
     * Destroy the data key (SWR-MG-06). What was sealed under it cannot be opened again, here or anywhere. A value written
     * afterwards is sealed under a new key, so the app can go on being used as a new install.
     */
    fun destroyKey() {
        secrets.destroy()
        cached?.fill(0)
        cached = null
    }

    companion object {
        const val KEY_BYTES = 32
        const val NONCE_BYTES = 12

        /** Marks a sealed value, so a plain one (from before) is told from it. */
        const val PREFIX = "np-enc:1:"

        /**
         * The opaque device token the SHDR upload sends (`np.shdr.device-token`, a per-install key of the core's list). It is
         * not user health data, but it is a secret that identifies the install to the fleet, so it is sealed under the same
         * data key rather than left in the clear or kept by a second library.
         */
        const val SHDR_DEVICE_TOKEN_KEY = "np.shdr.device-token"

        /**
         * The keys whose values are sealed at rest (SWR-MG-06), from the core's one list (`itemKeys`, `sealedAtRest`): every key of
         * every item a migration carries, the consent keys, and the keys not yet sorted for migration, which are UHDR until shown
         * not to be (`CLAUDE.md` §5.1). Per-install keys name a device or an install, not the person, and stay as they are.
         */
        fun uhdrKeys(): Set<String> =
            NppsCore.migration(buildJsonObject { put("op", "itemKeys") })["sealedAtRest"]!!.jsonArray.map { it.jsonPrimitive.content }.toSet()
    }
}
