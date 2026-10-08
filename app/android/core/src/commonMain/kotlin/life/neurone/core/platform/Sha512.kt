package life.neurone.core.platform

/**
 * SHA-512 (FIPS 180-4) in common code, because Ed25519 hashes with it and no browser API is synchronous.
 *
 * The 80 round constants and 8 initial values are the first 64 bits of the fractional parts of the cube and
 * square roots of the first primes. They are computed here, exactly, with a small unsigned integer type, rather
 * than typed: a doubles-based computation has 53 bits and a typed table has 640 digits that can be wrong. The
 * test pins the digest against the published vectors and, on the JVM, against the JDK.
 */
internal fun sha512Impl(input: ByteArray): ByteArray {
    val h = Sha512Constants.initial.copyOf()
    val k = Sha512Constants.rounds
    val bitLength = input.size.toLong() * 8
    val total = ((input.size + 16) / 128 + 1) * 128
    val padded = input.copyOf(total)
    padded[input.size] = 0x80.toByte()
    // 128-bit big-endian length; the high 64 bits are zero for any array that fits in memory.
    for (i in 0 until 8) padded[total - 1 - i] = (bitLength ushr (8 * i)).toByte()

    val w = LongArray(80)
    for (block in 0 until total / 128) {
        for (t in 0 until 16) {
            var v = 0L
            for (b in 0 until 8) v = (v shl 8) or (padded[block * 128 + t * 8 + b].toLong() and 0xFF)
            w[t] = v
        }
        for (t in 16 until 80) {
            val s0 = w[t - 15].rotateRight(1) xor w[t - 15].rotateRight(8) xor (w[t - 15] ushr 7)
            val s1 = w[t - 2].rotateRight(19) xor w[t - 2].rotateRight(61) xor (w[t - 2] ushr 6)
            w[t] = w[t - 16] + s0 + w[t - 7] + s1
        }
        var a = h[0]; var b = h[1]; var c = h[2]; var d = h[3]
        var e = h[4]; var f = h[5]; var g = h[6]; var hh = h[7]
        for (t in 0 until 80) {
            val s1 = e.rotateRight(14) xor e.rotateRight(18) xor e.rotateRight(41)
            val ch = (e and f) xor (e.inv() and g)
            val t1 = hh + s1 + ch + k[t] + w[t]
            val s0 = a.rotateRight(28) xor a.rotateRight(34) xor a.rotateRight(39)
            val maj = (a and b) xor (a and c) xor (b and c)
            val t2 = s0 + maj
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh
    }
    return ByteArray(64) { i -> (h[i / 8] ushr (56 - 8 * (i % 8))).toByte() }
}

private fun Long.rotateRight(n: Int): Long = (this ushr n) or (this shl (64 - n))

private object Sha512Constants {
    private val primes: List<Int> = generateSequence(2) { it + 1 }
        .filter { n -> (2 until n).none { n % it == 0 } }.take(80).toList()

    /** First 64 fractional bits of √p for the first 8 primes. */
    val initial: LongArray = LongArray(8) { fractionalRootBits(primes[it], 2) }

    /** First 64 fractional bits of ∛p for the first 80 primes. */
    val rounds: LongArray = LongArray(80) { fractionalRootBits(primes[it], 3) }

    /**
     * ⌊root(p · 2^(64·degree)) ⌋ mod 2^64: the integer root of p shifted left by 64·degree bits is the root of p scaled
     * by 2^64, so its low 64 bits are the fraction. Found bit by bit, high to low, comparing powers exactly.
     */
    private fun fractionalRootBits(p: Int, degree: Int): Long {
        val target = Natural.of(p.toLong()).shiftLeft(64 * degree)
        // The root is below 2^70: √p·2^64 < 2^68 and ∛p·2^64 < 2^70 for every prime up to 409.
        var acc = Natural.ZERO
        for (bit in 69 downTo 0) {
            val trial = acc.plusBit(bit)
            val power = if (degree == 2) trial.times(trial) else trial.times(trial).times(trial)
            if (power.compareTo(target) <= 0) acc = trial
        }
        return acc.low64()
    }
}

/** Unsigned arbitrary-size integers, just enough for [Sha512Constants]: 16-bit limbs, little-endian. */
private class Natural private constructor(private val limbs: IntArray) {

    fun shiftLeft(bits: Int): Natural {
        val whole = bits / 16
        val rest = bits % 16
        val out = IntArray(limbs.size + whole + 1)
        for (i in limbs.indices) {
            val v = limbs[i] shl rest
            out[i + whole] = out[i + whole] or (v and 0xFFFF)
            out[i + whole + 1] = out[i + whole + 1] or (v ushr 16)
        }
        return Natural(out)
    }

    fun plusBit(bit: Int): Natural {
        val out = limbs.copyOf(maxOf(limbs.size, bit / 16 + 1))
        out[bit / 16] = out[bit / 16] or (1 shl (bit % 16))
        return Natural(out)
    }

    fun times(other: Natural): Natural {
        val out = IntArray(limbs.size + other.limbs.size + 1)
        for (i in limbs.indices) {
            var carry = 0L
            for (j in other.limbs.indices) {
                val cur = out[i + j].toLong() + limbs[i].toLong() * other.limbs[j].toLong() + carry
                out[i + j] = (cur and 0xFFFF).toInt()
                carry = cur ushr 16
            }
            var k = i + other.limbs.size
            while (carry != 0L) {
                val cur = out[k].toLong() + carry
                out[k] = (cur and 0xFFFF).toInt()
                carry = cur ushr 16
                k++
            }
        }
        return Natural(out)
    }

    fun compareTo(other: Natural): Int {
        val n = maxOf(limbs.size, other.limbs.size)
        for (i in n - 1 downTo 0) {
            val a = limbs.getOrElse(i) { 0 }
            val b = other.limbs.getOrElse(i) { 0 }
            if (a != b) return a.compareTo(b)
        }
        return 0
    }

    fun low64(): Long {
        var v = 0L
        for (i in 3 downTo 0) v = (v shl 16) or limbs.getOrElse(i) { 0 }.toLong()
        return v
    }

    companion object {
        val ZERO = Natural(IntArray(1))
        fun of(v: Long): Natural = Natural(IntArray(4) { ((v ushr (16 * it)) and 0xFFFF).toInt() })
    }
}
