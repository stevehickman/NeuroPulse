package life.neurone.core.platform

import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.data.hexToBytes
import life.neurone.core.npps.NppsCore

/**
 * Digests for common code. SHA-256 and SHA-512 are computed by the Rust core, the one implementation every
 * platform shares. MD5 stays here, and exists only for RFC 4122 name-based (version 3) UUIDs, which derive stable
 * ids for bundled zones and conditions; it is never used for integrity or secrecy, so it is the one
 * non-security exception to `CLAUDE.md` §22.
 */
object Digests {
    /** SHA-256 and SHA-512 are the Rust core's (`CLAUDE.md` §22); this only moves the bytes. */
    fun sha256(input: ByteArray): ByteArray = digest("sha256", input)

    fun sha512(input: ByteArray): ByteArray = digest("sha512", input)

    private fun digest(op: String, input: ByteArray): ByteArray {
        val r = NppsCore.migration(buildJsonObject { put("op", op); put("bytes", input.toHex()) })
        return r["digest"]!!.jsonPrimitive.content.hexToBytes()
    }

    fun md5(input: ByteArray): ByteArray {
        var a0 = 0x67452301; var b0 = -0x10325477; var c0 = -0x67452302; var d0 = 0x10325476
        val padded = pad(input, bigEndianLength = false)
        val m = IntArray(16)
        for (block in 0 until padded.size / 64) {
            for (t in 0 until 16) {
                val o = block * 64 + t * 4
                m[t] = (padded[o].toInt() and 0xFF) or (padded[o + 1].toInt() and 0xFF shl 8) or
                    (padded[o + 2].toInt() and 0xFF shl 16) or (padded[o + 3].toInt() and 0xFF shl 24)
            }
            var a = a0; var b = b0; var c = c0; var d = d0
            for (i in 0 until 64) {
                var f: Int
                val g: Int
                when (i / 16) {
                    0 -> { f = (b and c) or (b.inv() and d); g = i }
                    1 -> { f = (d and b) or (d.inv() and c); g = (5 * i + 1) % 16 }
                    2 -> { f = b xor c xor d; g = (3 * i + 5) % 16 }
                    else -> { f = c xor (b or d.inv()); g = (7 * i) % 16 }
                }
                f = f + a + MD5_K[i] + m[g]
                a = d; d = c; c = b
                b += f.rotateLeft(MD5_S[i])
            }
            a0 += a; b0 += b; c0 += c; d0 += d
        }
        val out = ByteArray(16)
        intArrayOf(a0, b0, c0, d0).forEachIndexed { w, v ->
            for (j in 0 until 4) out[w * 4 + j] = (v ushr (8 * j)).toByte()
        }
        return out
    }

    private fun pad(input: ByteArray, bigEndianLength: Boolean): ByteArray {
        val bitLength = input.size.toLong() * 8
        val total = ((input.size + 8) / 64 + 1) * 64
        val out = input.copyOf(total)
        out[input.size] = 0x80.toByte()
        for (i in 0 until 8) {
            val byte = (bitLength ushr (8 * i)).toByte()
            if (bigEndianLength) out[total - 1 - i] = byte else out[total - 8 + i] = byte
        }
        return out
    }

    private val MD5_S = intArrayOf(
        7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
        5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
        4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
        6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
    )

    private val MD5_K = IntArray(64) { i ->
        // floor(2^32 × |sin(i + 1)|), the RFC 1321 table.
        (kotlin.math.abs(kotlin.math.sin((i + 1).toDouble())) * 4294967296.0).toLong().toInt()
    }
}

/** Lower-case hex, two digits per byte. */
fun ByteArray.toHex(): String = joinToString("") { it.toUByte().toString(16).padStart(2, '0') }
