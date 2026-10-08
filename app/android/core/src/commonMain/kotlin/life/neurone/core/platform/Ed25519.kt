package life.neurone.core.platform

/**
 * Ed25519 signing (RFC 8032) in common code, for the targets that have no synchronous platform signer: iOS and
 * the browser (WebCrypto is asynchronous, and [life.neurone.core.session.ProtocolSigner] is not). The JVM targets
 * keep the JDK's provider; this exists so a protocol can be signed everywhere, not to replace it.
 *
 * A port of TweetNaCl's `crypto_sign` (public domain, Bernstein et al.): field elements are 16 limbs of 16 bits
 * in a [LongArray]. It is not constant-time and is not meant for a server; it signs a descriptor on the wearer's
 * own device with a key that lives for one process. The two derived constants (the curve's d and the base point)
 * are computed rather than typed, and the tests pin it to RFC 8032 and, on the JVM, to the JDK byte for byte,
 * which is possible because Ed25519 signatures are deterministic.
 */
object Ed25519 {

    /** The 32-byte public key for a 32-byte secret seed. */
    fun publicKey(seed: ByteArray): ByteArray {
        require(seed.size == 32) { "An Ed25519 seed is 32 bytes" }
        val d = Digests.sha512(seed)
        d[0] = (d[0].toInt() and 248).toByte()
        d[31] = ((d[31].toInt() and 127) or 64).toByte()
        return pack(scalarBase(d.copyOf(32)))
    }

    /** The 64-byte signature of [message] under the secret [seed]. */
    fun sign(seed: ByteArray, message: ByteArray): ByteArray {
        require(seed.size == 32) { "An Ed25519 seed is 32 bytes" }
        val d = Digests.sha512(seed)
        d[0] = (d[0].toInt() and 248).toByte()
        d[31] = ((d[31].toInt() and 127) or 64).toByte()
        val publicKey = publicKey(seed)

        // r = H(prefix || M) mod L ;  R = r·B
        val r = reduce(Digests.sha512(d.copyOfRange(32, 64) + message))
        val rPoint = pack(scalarBase(r.copyOf(32)))
        // h = H(R || A || M) mod L ;  S = r + h·a mod L
        val h = reduce(Digests.sha512(rPoint + publicKey + message))
        val x = LongArray(64)
        for (i in 0 until 32) x[i] = (r[i].toLong() and 0xFF)
        for (i in 0 until 32) for (j in 0 until 32) x[i + j] += (h[i].toLong() and 0xFF) * (d[j].toLong() and 0xFF)
        val s = ByteArray(32)
        modL(s, x)
        return rPoint + s
    }

    /** SubjectPublicKeyInfo (RFC 8410) for a raw key: the form the JDK and the key fingerprints use. */
    fun spki(publicKey: ByteArray): ByteArray = SPKI_PREFIX + publicKey

    private val SPKI_PREFIX = byteArrayOf(0x30, 0x2a, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65, 0x70, 0x03, 0x21, 0x00)

    // ── Field arithmetic modulo 2^255 − 19 ───────────────────────────────

    private fun gf(vararg v: Long): LongArray = LongArray(16).also { for (i in v.indices) it[i] = v[i] }

    private val GF0 = gf()
    private val GF1 = gf(1)

    private fun set(r: LongArray, a: LongArray) { for (i in 0 until 16) r[i] = a[i] }

    private fun carry(o: LongArray) {
        for (i in 0 until 16) {
            o[i] += 1L shl 16
            val c = o[i] shr 16
            o[if (i < 15) i + 1 else 0] += c - 1 + (if (i == 15) 37 * (c - 1) else 0)
            o[i] -= c shl 16
        }
    }

    private fun select(p: LongArray, q: LongArray, b: Int) {
        val c = (b - 1).toLong().inv()
        for (i in 0 until 16) {
            val t = c and (p[i] xor q[i])
            p[i] = p[i] xor t
            q[i] = q[i] xor t
        }
    }

    private fun packField(o: ByteArray, n: LongArray) {
        val m = LongArray(16)
        val t = n.copyOf()
        carry(t); carry(t); carry(t)
        for (j in 0 until 2) {
            m[0] = t[0] - 0xffed
            for (i in 1 until 15) {
                m[i] = t[i] - 0xffff - ((m[i - 1] shr 16) and 1)
                m[i - 1] = m[i - 1] and 0xffff
            }
            m[15] = t[15] - 0x7fff - ((m[14] shr 16) and 1)
            val b = ((m[15] shr 16) and 1).toInt()
            m[14] = m[14] and 0xffff
            select(t, m, 1 - b)
        }
        for (i in 0 until 16) {
            o[2 * i] = (t[i] and 0xff).toByte()
            o[2 * i + 1] = (t[i] shr 8).toByte()
        }
    }

    private fun parity(a: LongArray): Int {
        val d = ByteArray(32)
        packField(d, a)
        return d[0].toInt() and 1
    }

    private fun add(o: LongArray, a: LongArray, b: LongArray) { for (i in 0 until 16) o[i] = a[i] + b[i] }
    private fun sub(o: LongArray, a: LongArray, b: LongArray) { for (i in 0 until 16) o[i] = a[i] - b[i] }

    private fun mul(o: LongArray, a: LongArray, b: LongArray) {
        val t = LongArray(31)
        for (i in 0 until 16) for (j in 0 until 16) t[i + j] += a[i] * b[j]
        for (i in 0 until 15) t[i] += 38 * t[i + 16]
        for (i in 0 until 16) o[i] = t[i]
        carry(o); carry(o)
    }

    private fun square(o: LongArray, a: LongArray) = mul(o, a, a)

    private fun invert(o: LongArray, i: LongArray) {
        val c = i.copyOf()
        for (a in 253 downTo 0) {
            square(c, c)
            if (a != 2 && a != 4) mul(c, c, i)
        }
        set(o, c)
    }

    /** i^(2^252 − 3), the exponent behind square roots modulo p. */
    private fun pow2523(o: LongArray, i: LongArray) {
        val c = i.copyOf()
        for (a in 250 downTo 0) {
            square(c, c)
            if (a != 1) mul(c, c, i)
        }
        set(o, c)
    }

    private fun fieldOf(v: Long): LongArray = gf(v)

    /** d = −121665 / 121666, the Edwards curve constant. */
    private val D: LongArray by lazy {
        val inv = LongArray(16)
        invert(inv, fieldOf(121666))
        val n = LongArray(16)
        sub(n, GF0, fieldOf(121665))
        LongArray(16).also { mul(it, n, inv) }
    }

    private val D2: LongArray by lazy { LongArray(16).also { add(it, D, D) } }

    /** The base point's y = 4/5, and its x, the even root of (y² − 1) / (d·y² + 1). */
    private val BASE_Y: LongArray by lazy {
        val inv = LongArray(16)
        invert(inv, fieldOf(5))
        LongArray(16).also { mul(it, fieldOf(4), inv) }
    }

    private val BASE_X: LongArray by lazy {
        val y2 = LongArray(16).also { square(it, BASE_Y) }
        val num = LongArray(16).also { sub(it, y2, GF1) }
        val den = LongArray(16).also { mul(it, D, y2); add(it, it, GF1) }
        val denInv = LongArray(16).also { invert(it, den) }
        val x2 = LongArray(16).also { mul(it, num, denInv) }
        // sqrt(x2) = x2^((p+3)/8), times sqrt(−1) if that squares to −x2 instead.
        val cand = LongArray(16).also { pow2523(it, x2); mul(it, it, x2) }
        val check = LongArray(16).also { square(it, cand) }
        val diff = LongArray(16).also { sub(it, check, x2) }
        val bytes = ByteArray(32).also { packField(it, diff) }
        val x = if (bytes.all { it == 0.toByte() }) cand else LongArray(16).also { mul(it, cand, SQRT_M1) }
        if (parity(x) == 1) LongArray(16).also { sub(it, GF0, x) } else x
    }

    /** sqrt(−1) = 2^((p−1)/4) = 2^(2^253 − 5). */
    private val SQRT_M1: LongArray by lazy {
        val two = fieldOf(2)
        val t = LongArray(16)
        pow2523(t, two)          // 2^(2^252 − 3)
        square(t, t)             // 2^(2^253 − 6)
        mul(t, t, two)           // 2^(2^253 − 5)
        t
    }

    // ── Group arithmetic (extended twisted Edwards coordinates) ──────────

    private fun newPoint(): Array<LongArray> = Array(4) { LongArray(16) }

    private fun addPoints(p: Array<LongArray>, q: Array<LongArray>) {
        val a = LongArray(16); val b = LongArray(16); val c = LongArray(16); val d = LongArray(16)
        val t = LongArray(16); val e = LongArray(16); val f = LongArray(16); val g = LongArray(16); val h = LongArray(16)
        sub(a, p[1], p[0]); sub(t, q[1], q[0]); mul(a, a, t)
        add(b, p[0], p[1]); add(t, q[0], q[1]); mul(b, b, t)
        mul(c, p[3], q[3]); mul(c, c, D2)
        mul(d, p[2], q[2]); add(d, d, d)
        sub(e, b, a); sub(f, d, c); add(g, d, c); add(h, b, a)
        mul(p[0], e, f); mul(p[1], h, g); mul(p[2], g, f); mul(p[3], e, h)
    }

    private fun conditionalSwap(p: Array<LongArray>, q: Array<LongArray>, b: Int) {
        for (i in 0 until 4) select(p[i], q[i], b)
    }

    private fun pack(p: Array<LongArray>): ByteArray {
        val zi = LongArray(16); val tx = LongArray(16); val ty = LongArray(16)
        invert(zi, p[2])
        mul(tx, p[0], zi)
        mul(ty, p[1], zi)
        val r = ByteArray(32)
        packField(r, ty)
        r[31] = (r[31].toInt() xor (parity(tx) shl 7)).toByte()
        return r
    }

    private fun scalarMult(q: Array<LongArray>, s: ByteArray): Array<LongArray> {
        val p = newPoint()
        set(p[0], GF0); set(p[1], GF1); set(p[2], GF1); set(p[3], GF0)
        for (i in 255 downTo 0) {
            val b = (s[i / 8].toInt() shr (i and 7)) and 1
            conditionalSwap(p, q, b)
            addPoints(q, p)
            addPoints(p, p)
            conditionalSwap(p, q, b)
        }
        return p
    }

    private fun scalarBase(s: ByteArray): Array<LongArray> {
        val q = newPoint()
        set(q[0], BASE_X); set(q[1], BASE_Y); set(q[2], GF1); mul(q[3], BASE_X, BASE_Y)
        return scalarMult(q, s)
    }

    // ── Arithmetic modulo the group order L ──────────────────────────────

    private val L = longArrayOf(
        0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58, 0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x10,
    )

    private fun modL(r: ByteArray, x: LongArray) {
        var carry: Long
        for (i in 63 downTo 32) {
            carry = 0
            var j = i - 32
            while (j < i - 12) {
                x[j] += carry - 16 * x[i] * L[j - (i - 32)]
                carry = (x[j] + 128) shr 8
                x[j] -= carry shl 8
                j++
            }
            x[j] += carry
            x[i] = 0
        }
        carry = 0
        for (j in 0 until 32) {
            x[j] += carry - (x[31] shr 4) * L[j]
            carry = x[j] shr 8
            x[j] = x[j] and 255
        }
        for (j in 0 until 32) x[j] -= carry * L[j]
        for (i in 0 until 32) {
            x[i + 1] += x[i] shr 8
            r[i] = (x[i] and 255).toByte()
        }
    }

    /** A 64-byte hash read as a little-endian number, reduced modulo L, returned as 64 bytes (the top 32 are zero). */
    private fun reduce(hash: ByteArray): ByteArray {
        val x = LongArray(64) { hash[it].toLong() and 0xFF }
        val out = ByteArray(64)
        val low = ByteArray(32)
        modL(low, x)
        low.copyInto(out)
        return out
    }
}
