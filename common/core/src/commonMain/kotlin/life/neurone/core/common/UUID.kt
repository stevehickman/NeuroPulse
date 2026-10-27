package life.neurone.core.common

import life.neurone.core.platform.Digests
import life.neurone.core.platform.secureRandomBytes
import life.neurone.core.platform.toHex

/**
 * RFC 4122 UUID, written once for every target (the JVM class is not in Kotlin common). Mirrors
 * the part of `life.neurone.core.common.UUID` the app uses, and prints and orders the same way, so ids persisted
 * by an older build still parse and compare.
 */
class UUID(val mostSignificantBits: Long, val leastSignificantBits: Long) : Comparable<UUID> {

    override fun toString(): String {
        val hex = toBytes().toHex()
        return "${hex.substring(0, 8)}-${hex.substring(8, 12)}-${hex.substring(12, 16)}-" +
            "${hex.substring(16, 20)}-${hex.substring(20)}"
    }

    fun toBytes(): ByteArray = ByteArray(16) { i ->
        val word = if (i < 8) mostSignificantBits else leastSignificantBits
        (word ushr (56 - 8 * (i % 8))).toByte()
    }

    override fun equals(other: Any?): Boolean =
        other is UUID && other.mostSignificantBits == mostSignificantBits &&
            other.leastSignificantBits == leastSignificantBits

    override fun hashCode(): Int {
        val x = mostSignificantBits xor leastSignificantBits
        return (x shr 32).toInt() xor x.toInt()
    }

    override fun compareTo(other: UUID): Int =
        if (mostSignificantBits != other.mostSignificantBits) {
            mostSignificantBits.compareTo(other.mostSignificantBits)
        } else {
            leastSignificantBits.compareTo(other.leastSignificantBits)
        }

    companion object {
        fun randomUUID(): UUID = fromBytes(secureRandomBytes(16), version = 4)

        /** Version 3 (MD5, name-based): the same bytes always give the same id. */
        fun nameUUIDFromBytes(name: ByteArray): UUID = fromBytes(Digests.md5(name), version = 3)

        /** Strict `8-4-4-4-12` hexadecimal; anything else throws [IllegalArgumentException]. */
        fun fromString(text: String): UUID {
            val parts = text.split('-')
            require(parts.size == 5 && parts.map { it.length } == listOf(8, 4, 4, 4, 12)) {
                "Invalid UUID string: $text"
            }
            val hex = parts.joinToString("")
            require(hex.all { it in '0'..'9' || it in 'a'..'f' || it in 'A'..'F' }) { "Invalid UUID string: $text" }
            val bytes = ByteArray(16) { hex.substring(it * 2, it * 2 + 2).toInt(16).toByte() }
            return of(bytes)
        }

        private fun fromBytes(source: ByteArray, version: Int): UUID {
            val b = source.copyOf(16)
            b[6] = ((b[6].toInt() and 0x0F) or (version shl 4)).toByte()
            b[8] = ((b[8].toInt() and 0x3F) or 0x80).toByte()
            return of(b)
        }

        private fun of(b: ByteArray): UUID {
            var msb = 0L
            var lsb = 0L
            for (i in 0 until 8) msb = (msb shl 8) or (b[i].toLong() and 0xFF)
            for (i in 8 until 16) lsb = (lsb shl 8) or (b[i].toLong() and 0xFF)
            return UUID(msb, lsb)
        }
    }
}
