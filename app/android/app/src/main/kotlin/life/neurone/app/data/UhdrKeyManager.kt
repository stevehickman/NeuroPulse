package life.neurone.app.data

import android.content.Context
import androidx.biometric.BiometricManager.Authenticators.BIOMETRIC_STRONG
import androidx.biometric.BiometricManager.Authenticators.DEVICE_CREDENTIAL
import androidx.biometric.BiometricPrompt
import androidx.fragment.app.FragmentActivity
import androidx.security.crypto.EncryptedSharedPreferences
import androidx.security.crypto.MasterKey
import life.neurone.core.data.Argon2Provider
import life.neurone.core.data.UhdrKey
import life.neurone.core.data.UhdrKeyDerivation
import java.security.SecureRandom
import org.signal.argon2.Argon2
import org.signal.argon2.MemoryCost
import org.signal.argon2.Type

/**
 * UHDR key manager — Android parallel of iOS UHDRKeyManager.swift.
 *
 * Derives the memory-only UHDR AES-256-XTS key (K1+K2) from:
 *  - a 32-byte CSPRNG credential seed, stored in Keystore-encrypted prefs and
 *    released only after successful BiometricPrompt authentication
 *    (BIOMETRIC_STRONG | DEVICE_CREDENTIAL — the credential fallback supports
 *    Parkinson's/post-stroke/no-biometric users, parity with iOS
 *    .deviceOwnerAuthentication), and
 *  - a separate 32-byte CSPRNG salt (device-bound, never synced).
 *
 * The DERIVED KEY IS NEVER PERSISTED and never transmitted — NeurOne does
 * not hold the decryption key (CLAUDE.md §5.1). Callers must destroy() the
 * key when the session ends.
 *
 * SOUP: org.signal:argon2 (Argon2id, RFC 9106) — NP-SW-001 SOUP table entry
 * required before beta (parallel of the iOS vendored PHC argon2).
 */
class UhdrKeyManager(context: Context) {

    private companion object {
        const val PREFS_FILE = "np-uhdr-secure"
        const val SEED_PREF = "np.uhdr.credential-seed"
        const val SALT_PREF = "np.uhdr.salt"
    }

    private val prefs = run {
        val masterKey = MasterKey.Builder(context)
            .setKeyScheme(MasterKey.KeyScheme.AES256_GCM)
            .setUserAuthenticationRequired(false) // gate is BiometricPrompt below
            .build()
        EncryptedSharedPreferences.create(
            context,
            PREFS_FILE,
            masterKey,
            EncryptedSharedPreferences.PrefKeyEncryptionScheme.AES256_SIV,
            EncryptedSharedPreferences.PrefValueEncryptionScheme.AES256_GCM,
        )
    }

    private val argon2Provider = object : Argon2Provider {
        override fun argon2idHash(
            password: ByteArray,
            salt: ByteArray,
            memoryKiB: Int,
            iterations: Int,
            parallelism: Int,
            outputLength: Int,
        ): ByteArray = Argon2.Builder(org.signal.argon2.Version.V13)
            .type(Type.Argon2id)
            .memoryCost(MemoryCost.KiB(memoryKiB))
            .iterations(iterations)
            .parallelism(parallelism)
            .hashLength(outputLength)
            .build()
            .hash(password, salt)
            .hash
    }

    /**
     * Authenticate the device owner, then derive the UHDR key. The key is
     * handed to [onKey] and must be destroy()ed by the caller after use.
     */
    fun deriveKeyAfterAuthentication(
        activity: FragmentActivity,
        onKey: (UhdrKey) -> Unit,
        onError: (String) -> Unit,
    ) {
        val prompt = BiometricPrompt(
            activity,
            object : BiometricPrompt.AuthenticationCallback() {
                override fun onAuthenticationSucceeded(result: BiometricPrompt.AuthenticationResult) {
                    val seed = loadOrCreate(SEED_PREF)
                    val salt = loadOrCreate(SALT_PREF)
                    try {
                        onKey(UhdrKeyDerivation.deriveKey(argon2Provider, seed, salt))
                    } finally {
                        seed.fill(0)
                    }
                }

                override fun onAuthenticationError(errorCode: Int, errString: CharSequence) {
                    onError(errString.toString())
                }
            },
        )
        prompt.authenticate(
            BiometricPrompt.PromptInfo.Builder()
                .setTitle("Unlock your health data")
                .setSubtitle("Your recordings are encrypted with a key only you hold")
                .setAllowedAuthenticators(BIOMETRIC_STRONG or DEVICE_CREDENTIAL)
                .build(),
        )
    }

    private fun loadOrCreate(prefKey: String): ByteArray {
        prefs.getString(prefKey, null)?.let { hex ->
            return hex.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
        }
        val bytes = ByteArray(UhdrKeyDerivation.SALT_LENGTH).also { SecureRandom().nextBytes(it) }
        prefs.edit().putString(prefKey, bytes.joinToString("") { "%02x".format(it) }).apply()
        return bytes
    }
}
