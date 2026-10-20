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
 * A line written before this (keyed by the raw identifier) is still read, and is rewritten under the hash when that helmet next
 * connects. Until then the raw identifier of a helmet that has not reconnected stays in the value.
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
     * The token kept for this helmet, or `null` if there is none. A line under the raw identifier is read too, and is rewritten
     * under the hash at once, so a helmet's address leaves the value the first time it connects after this change.
     */
    fun load(hub: String): String? {
        val t = parse()
        t.salt?.let { t.entries[keyFor(it, hub)] }?.let { return it }
        val legacy = t.entries[hub] ?: return null
        save(hub, legacy)
        return legacy
    }

    fun save(hub: String, token: String) {
        val t = parse()
        val salt = t.salt ?: randomBytes(32).toHex()
        t.entries.remove(hub) // a line under the raw identifier is replaced, not kept beside the hashed one
        t.entries[keyFor(salt, hub)] = token
        val lines = listOf("$SALT_KEY\t$salt") + t.entries.map { "${it.key}\t${it.value}" }
        write(lines.joinToString("\n"))
    }

    private fun hexBytes(h: String): ByteArray = ByteArray(h.length / 2) { h.substring(2 * it, 2 * it + 2).toInt(16).toByte() }

    private companion object {
        /** Not a hash (it has no 64-hex form), so it cannot collide with a helmet's line. */
        const val SALT_KEY = "#salt"
        val HEX_32 = Regex("^[0-9a-f]{64}$")
    }
}
