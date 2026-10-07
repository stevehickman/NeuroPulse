package life.neurone.core.platform

@JsFun("(buf, n) => { crypto.getRandomValues(buf); }")
private external fun fillRandom(buf: JsAny, n: Int)

@JsFun("(n) => new Uint8Array(n)")
private external fun newUint8Array(n: Int): JsAny

@JsFun("(a, i) => a[i]")
private external fun uint8At(a: JsAny, i: Int): Int

actual fun secureRandomBytes(count: Int): ByteArray {
    // getRandomValues refuses more than 65536 bytes at a time.
    val out = ByteArray(count)
    var offset = 0
    while (offset < count) {
        val n = minOf(65536, count - offset)
        val buf = newUint8Array(n)
        fillRandom(buf, n)
        for (i in 0 until n) out[offset + i] = uint8At(buf, i).toByte()
        offset += n
    }
    return out
}

/** No Ed25519 verifier on this target yet, so a signed descriptor is refused, never trusted. */
actual fun ed25519Verify(publicKeyRaw: ByteArray, message: ByteArray, signature: ByteArray): Boolean? = null
