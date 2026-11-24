package life.neurone.core.analytics

import life.neurone.core.platform.Digests
import life.neurone.core.platform.secureRandomBytes
import life.neurone.core.platform.toHex

/**
 * The interim SHDR device tokens, one per helmet, kept as one text value (`REQ-SHDR-05`) and keyed by a salted hash of the helmet's
 * identifier, never by the identifier (`REQ-SHDR-05` (d); `OI-FLEET-01`).
 *
 * On Android the central's identifier for a peripheral is its Bluetooth hardware address: stable for the helmet's life, shared by
 * every phone that sees it, and the one value that names the helmet's radio. The table is sealed at rest, but a sealed value is
 * opened by every part of the app that holds the data key, and by a restore on another phone, so an address that need not be in
 * it is left out. The key of a line is `SHA-256(salt ‖ identifier)` in hex, with a 32-byte salt made once per install and kept as
 * the first line of the same value (a key of its own would be a key the store is not told to seal). Without the salt the hash
 * does not give the address back: an address is 48 bits, with a known vendor prefix, so an unsalted hash would.
 *
 * A line written before this (keyed by the raw identifier) is still read, and **every** such line is rewritten under the hash
 * the first time the table is used after this change: [migrateLegacy] at launch, and any [load] or [save]. No raw identifier
 * waits for its helmet to reconnect (`OI-PRIV-08` (4)). A line's key is a raw identifier exactly when it is not 64 hex digits,
 * which no Bluetooth address or peripheral UUID is.
 *
 * The caller supplies [read] and [write] for the one value, so this has no store of its own and runs on any platform.
 */
class HubTokenTable(
    private val read: () -> String?,
    private val write: (String) -> Unit,
    private val randomBytes: (Int) -> ByteArray = ::secureRandomBytes,
    /** SHA-256, the Rust core's by default (`CLAUDE.md` §22); a host test with no native library supplies its own. */
    private val sha256: (ByteArray) -> ByteArray = Digests::sha256,
) {
    private class Parsed(val salt: String?, val entries: LinkedHashMap<String, String>)

    private fun parse(): Parsed {
        var salt: String? = null
        val entries = LinkedHashMap<String, String>()
        for (line in read().orEmpty().lines()) {
            val parts = line.split('\t')
            if (parts.size != 2 || parts[0].isEmpty() || parts[1].isEmpty()) continue
            if (parts[0] == SALT_KEY) {
                if (parts[1].matches(HEX_32)) salt = parts[1]
            } else {
                entries[parts[0]] = parts[1]
            }
        }
        return Parsed(salt, entries)
    }

    private fun keyFor(salt: String, hub: String): String = sha256(hexBytes(salt) + hub.encodeToByteArray()).toHex()

    /**
     * The table with every line keyed by the hash. A line under a raw identifier is moved to its hash, unless that helmet already
     * has a hashed line, which is the later one and wins. Writes only if something moved.
     */
    private fun migrated(): Parsed {
        val t = parse()
        val legacy = t.entries.keys.filterNot { it.matches(HEX_32) }
        if (legacy.isEmpty()) return t
        val salt = t.salt ?: randomBytes(32).toHex()
        val entries = LinkedHashMap<String, String>()
        for ((key, token) in t.entries) if (key.matches(HEX_32)) entries[key] = token
        for (hub in legacy) entries.getOrPut(keyFor(salt, hub)) { t.entries.getValue(hub) }
        return Parsed(salt, entries).also(::store)
    }

    private fun store(t: Parsed) {
        val lines = listOf("$SALT_KEY\t${t.salt}") + t.entries.map { "${it.key}\t${it.value}" }
        write(lines.joinToString("\n"))
    }

    /**
     * Rewrite every line still keyed by a raw identifier under its hash, now, without waiting for that helmet. Called at launch;
     * it does nothing, and computes no hash, when there is no such line. A value that cannot be read yet (a locked run reads it
     * as absent) is left alone and is migrated by the first [load] or [save] instead.
     */
    fun migrateLegacy() {
        migrated()
    }

    /** The token kept for this helmet, or `null` if there is none. */
    fun load(hub: String): String? {
        val t = migrated()
        return t.salt?.let { t.entries[keyFor(it, hub)] }
    }

    fun save(hub: String, token: String) {
        val t = migrated()
        val salt = t.salt ?: randomBytes(32).toHex()
        t.entries[keyFor(salt, hub)] = token
        store(Parsed(salt, t.entries))
    }

    /**
     * The key of this helmet's line, `SHA-256(salt ‖ identifier)` in hex, or `null` when the table has no salt yet (nothing was
     * ever kept). It names the helmet without naming its identifier, so the caller may use it where the table's own value cannot
     * go: the name of the helmet's staged SHDR file (`REQ-SHDR-08` (f)). It makes no salt: a key that a later salt would change
     * would name nothing.
     */
    fun keyOf(hub: String): String? = migrated().salt?.let { keyFor(it, hub) }

    /** Remove this helmet's line, so the helmet reads as never kept. Every other helmet's line, and the salt, stay. */
    fun remove(hub: String) {
        val t = migrated()
        val salt = t.salt ?: return
        if (t.entries.remove(keyFor(salt, hub)) != null) store(t)
    }

    private fun hexBytes(h: String): ByteArray = ByteArray(h.length / 2) { h.substring(2 * it, 2 * it + 2).toInt(16).toByte() }

    private companion object {
        /** Not a hash (it has no 64-hex form), so it cannot collide with a helmet's line. */
        const val SALT_KEY = "#salt"
        val HEX_32 = Regex("^[0-9a-f]{64}$")
    }
}
