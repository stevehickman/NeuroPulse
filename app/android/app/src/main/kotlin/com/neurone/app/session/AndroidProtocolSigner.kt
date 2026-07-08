package com.neurone.app.session

import android.os.Build
import com.neurone.core.session.ProtocolSigner
import com.neurone.core.session.SignatureResult
import java.security.KeyPair
import java.security.KeyPairGenerator
import java.security.MessageDigest
import java.security.Signature

// App-side Ed25519 implementation of the core ProtocolSigner (parity with iOS
// SessionProtocolSigner, whose Curve25519 key lives in the Keychain). Signs the 32-byte
// SHA-256 digest the SessionProtocolCompiler passes in.
//
// Ed25519 in java.security requires Android 13 (API 33)+. On older devices this throws a
// descriptive error until a BouncyCastle fallback + Keystore-persisted key are added
// (OI-AND-SIGN-01). The current key is ephemeral (regenerated per process) — acceptable
// while the hub firmware / key-pinning contract is still a placeholder.
class AndroidProtocolSigner : ProtocolSigner {

    private val keyPair: KeyPair by lazy { KeyPairGenerator.getInstance("Ed25519").generateKeyPair() }

    override fun sign(digest: ByteArray): SignatureResult {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            throw UnsupportedOperationException(
                "Protocol signing needs Android 13+ (Ed25519). BouncyCastle fallback is OI-AND-SIGN-01.",
            )
        }
        val signature = Signature.getInstance("Ed25519").run {
            initSign(keyPair.private)
            update(digest)
            sign()
        }
        val fingerprint = MessageDigest.getInstance("SHA-256")
            .digest(keyPair.public.encoded)
            .take(8)
            .joinToString("") { "%02x".format(it) }
        return SignatureResult(signature = signature, publicKeyFingerprint = fingerprint)
    }
}
