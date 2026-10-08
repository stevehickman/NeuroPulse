package life.neurone.core.platform

/**
 * SHA-256 and MD5 in common code, so a digest means the same on every target without a platform
 * actual per hash. MD5 exists only for RFC 4122 name-based (version 3) UUIDs, which derive stable
 * ids for bundled zones and conditions; it is never used for integrity or secrecy.
 */
object Digests {
    fun sha256(input: ByteArray): ByteArray {
        val h = H256.copyOf()
        val padded = pad(input, bigEndianLength = true)
        val w = IntArray(64)
        for (block in 0 until padded.size / 64) {
            for (t in 0 until 16) {
                val o = block * 64 + t * 4
                w[t] = (padded[o].toInt() and 0xFF shl 24) or (padded[o + 1].toInt() and 0xFF shl 16) or
                    (padded[o + 2].toInt() and 0xFF shl 8) or (padded[o + 3].toInt() and 0xFF)
            }
            for (t in 16 until 64) {
                val s0 = w[t - 15].rotateRight(7) xor w[t - 15].rotateRight(18) xor (w[t - 15] ushr 3)
                val s1 = w[t - 2].rotateRight(17) xor w[t - 2].rotateRight(19) xor (w[t - 2] ushr 10)
                w[t] = w[t - 16] + s0 + w[t - 7] + s1
            }
            var a = h[0]; var b = h[1]; var c = h[2]; var d = h[3]
            var e = h[4]; var f = h[5]; var g = h[6]; var hh = h[7]
            for (t in 0 until 64) {
                val s1 = e.rotateRight(6) xor e.rotateRight(11) xor e.rotateRight(25)
                val ch = (e and f) xor (e.inv() and g)
                val t1 = hh + s1 + ch + K256[t] + w[t]
                val s0 = a.rotateRight(2) xor a.rotateRight(13) xor a.rotateRight(22)
                val maj = (a and b) xor (a and c) xor (b and c)
                val t2 = s0 + maj
                hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2
            }
            h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh
        }
        return ByteArray(32) { i -> (h[i / 4] ushr (24 - 8 * (i % 4))).toByte() }
    }

    fun sha512(input: ByteArray): ByteArray = sha512Impl(input)

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

    // Fractional parts of the square roots (initial hash) and cube roots (round constants) of the
    // first primes, FIPS 180-4 §4.2.2 and §5.3.3. Computed, not typed, so no digit can be wrong.
    private val FIRST_PRIMES: List<Int> = generateSequence(2) { it + 1 }
        .filter { n -> (2 until n).none { n % it == 0 } }.take(64).toList()

    private fun fractionBits(x: Double): Int = ((x - kotlin.math.floor(x)) * 4294967296.0).toLong().toInt()

    private val H256: IntArray = IntArray(8) { fractionBits(kotlin.math.sqrt(FIRST_PRIMES[it].toDouble())) }

    private val K256: IntArray = IntArray(64) { fractionBits(kotlin.math.cbrt(FIRST_PRIMES[it].toDouble())) }
}

/** Lower-case hex, two digits per byte. */
fun ByteArray.toHex(): String = joinToString("") { it.toUByte().toString(16).padStart(2, '0') }
