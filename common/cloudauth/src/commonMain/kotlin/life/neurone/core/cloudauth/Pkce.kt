package life.neurone.core.cloudauth

import life.neurone.core.data.RandomSource

/** RFC 4648 base64url without padding, the form RFC 7636 uses for the verifier and the challenge. */
internal object Base64Url {
    private const val ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"

    fun encode(bytes: ByteArray): String {
        val out = StringBuilder((bytes.size * 4 + 2) / 3)
        var i = 0
        while (i + 2 < bytes.size) {
            val n = ((bytes[i].toInt() and 0xFF) shl 16) or ((bytes[i + 1].toInt() and 0xFF) shl 8) or (bytes[i + 2].toInt() and 0xFF)
            out.append(ALPHABET[n shr 18 and 63]).append(ALPHABET[n shr 12 and 63]).append(ALPHABET[n shr 6 and 63]).append(ALPHABET[n and 63])
            i += 3
        }
        when (bytes.size - i) {
            1 -> {
                val n = (bytes[i].toInt() and 0xFF) shl 16
                out.append(ALPHABET[n shr 18 and 63]).append(ALPHABET[n shr 12 and 63])
            }
            2 -> {
                val n = ((bytes[i].toInt() and 0xFF) shl 16) or ((bytes[i + 1].toInt() and 0xFF) shl 8)
                out.append(ALPHABET[n shr 18 and 63]).append(ALPHABET[n shr 12 and 63]).append(ALPHABET[n shr 6 and 63])
            }
        }
        return out.toString()
    }
}

/**
 * A PKCE pair (RFC 7636): the [verifier] stays in memory until the code is redeemed, the [challenge] goes in the authorization request.
 * The verifier is 32 random bytes as 43 base64url characters (the RFC asks for 43 to 128 unreserved characters); the challenge is
 * `BASE64URL(SHA256(ASCII(verifier)))` with the `S256` method. Both the random bytes and the SHA-256 are the Rust core's (`CLAUDE.md` §22).
 */
internal class Pkce(val verifier: String, val challenge: String) {
    companion object {
        fun create(random: RandomSource, sha256: (ByteArray) -> ByteArray): Pkce {
            val verifier = Base64Url.encode(random.nextBytes(32))
            return Pkce(verifier, challengeFor(verifier, sha256))
        }

        fun challengeFor(verifier: String, sha256: (ByteArray) -> ByteArray): String =
            Base64Url.encode(sha256(verifier.encodeToByteArray()))
    }
}
