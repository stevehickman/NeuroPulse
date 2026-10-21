package life.neurone.app

import android.content.Context
import android.hardware.biometrics.BiometricManager
import android.hardware.biometrics.BiometricPrompt
import android.os.Build
import android.os.CancellationSignal
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyPermanentlyInvalidatedException
import android.security.keystore.KeyProperties
import android.security.keystore.UserNotAuthenticatedException
import kotlinx.coroutines.suspendCancellableCoroutine
import life.neurone.core.common.GatedSecretKeyStore
import life.neurone.core.common.SecretKeyUnavailableException
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec
import kotlin.coroutines.resume

/**
 * The key that protects UHDR once the person's presence is established (`NP-APP-PRESENCE-001` §5, `REQ-PRES-02`, `-04`). Like
 * [AndroidSecretKeyStore] the Keystore holds a non-exportable AES key that wraps the 32-byte data key; unlike it, that key needs the
 * person: it is created with `setUserAuthenticationRequired(true)`, so the Keystore refuses to use it until the person has just
 * passed the system prompt (a biometric, or the device PIN, pattern or password: the prompt is never biometrics-only).
 *
 * **How it works.** [unlock] shows the framework [BiometricPrompt]. On success the data key is unwrapped at once, while the Keystore's
 * short window ([UNLOCK_WINDOW_SECONDS]) is open, and held in memory until [lock]; [load] then answers from memory. So the window only
 * has to outlast one decryption, not the whole time the app is unlocked. While locked [load] throws [SecretKeyUnavailableException],
 * which the stores treat as "could not ask" and never as "no key" (`SWR-MG-15`).
 *
 * A new biometric enrolment does not invalidate the key (`setInvalidatedByBiometricEnrollment(false)`), or adding a finger would
 * destroy the person's record. Removing the screen lock does: Android deletes such keys, and the wrapped bytes then open under
 * nothing, exactly as after [destroy]. That is the case the backup the app insists on first is for (`REQ-PRES-08`).
 *
 * **Not offered yet.** The migration of sealed UHDR to this key is built (`REQ-PRES-07`, in `:core`), and there is no way to turn the gate
 * off (`REQ-PRES-09`). [NeurOneApplication] does not pass this store to the shared app until the device run (`OI-PRES-06`).
 */
class AndroidGatedSecretKeyStore(private val context: Context) : GatedSecretKeyStore {
    private val prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)

    @Volatile private var dataKey: ByteArray? = null
    @Volatile private var unlocked = false

    override val isUnlocked: Boolean get() = unlocked

    override val isEnabled: Boolean get() = prefs.getBoolean(ENABLED, false)

    /**
     * Turn the gate on: the person passes the prompt once, which proves they can and opens the Keystore window, and the flag is kept. No
     * data moves here. The next launch opens storage gated, and the first unlock seals each value again under this key
     * (`REQ-PRES-07`); the key itself is made by the first write ([store]), inside that unlock's window.
     */
    override suspend fun enable(reason: String): Boolean {
        if (!unlock(reason)) return false
        return prefs.edit().putBoolean(ENABLED, true).commit()
    }

    override suspend fun unlock(reason: String): Boolean {
        if (unlocked) return true
        if (!authenticate(reason)) return false
        return try {
            dataKey = unwrap()
            unlocked = true
            true
        } catch (ex: UserNotAuthenticatedException) {
            // The window closed before the key was used: the person is asked again, nothing is held.
            false
        } catch (ex: java.security.GeneralSecurityException) {
            false
        }
    }

    override fun lock() {
        dataKey?.fill(0)
        dataKey = null
        unlocked = false
    }

    override fun load(): ByteArray? {
        if (!unlocked) throw SecretKeyUnavailableException("the person's presence has not been established")
        return dataKey?.copyOf()
    }

    override fun store(key: ByteArray) {
        if (!unlocked) throw SecretKeyUnavailableException("the person's presence has not been established")
        try {
            val cipher = Cipher.getInstance(TRANSFORM)
            cipher.init(Cipher.ENCRYPT_MODE, wrappingKey(create = true))
            val wrapped = cipher.iv + cipher.doFinal(key)
            check(prefs.edit().putString(WRAPPED, wrapped.joinToString("") { "%02x".format(it) }).commit()) { "could not keep the wrapped key" }
            dataKey?.fill(0)
            dataKey = key.copyOf()
        } catch (ex: UserNotAuthenticatedException) {
            throw SecretKeyUnavailableException("the unlock window closed before the key was kept", ex)
        } catch (ex: java.security.GeneralSecurityException) {
            throw SecretKeyUnavailableException("the Android Keystore could not wrap the data key", ex)
        }
    }

    override fun destroy() {
        // Destroying needs no presence. The Keystore entry first, so the wrapped bytes open under nothing whatever becomes of the file.
        val ks = keystore()
        if (ks.containsAlias(ALIAS)) ks.deleteEntry(ALIAS)
        prefs.edit().remove(WRAPPED).commit()
        lock()
    }

    private fun keystore(): KeyStore = KeyStore.getInstance(PROVIDER).apply { load(null) }

    private fun wrappingKey(create: Boolean): SecretKey? {
        (keystore().getKey(ALIAS, null) as? SecretKey)?.let { return it }
        if (!create) return null
        val spec = KeyGenParameterSpec.Builder(ALIAS, KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT)
            .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
            .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
            .setKeySize(256)
            .setUserAuthenticationRequired(true)
            .setInvalidatedByBiometricEnrollment(false)
            .apply {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                    setUserAuthenticationParameters(
                        UNLOCK_WINDOW_SECONDS,
                        KeyProperties.AUTH_BIOMETRIC_STRONG or KeyProperties.AUTH_DEVICE_CREDENTIAL,
                    )
                } else {
                    @Suppress("DEPRECATION")
                    setUserAuthenticationValidityDurationSeconds(UNLOCK_WINDOW_SECONDS)
                }
            }
            .build()
        return KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, PROVIDER).apply { init(spec) }.generateKey()
    }

    /** The data key unwrapped, or null if nothing was ever kept or the key is gone (removing the screen lock deletes it). */
    private fun unwrap(): ByteArray? {
        val wrapped = prefs.getString(WRAPPED, null) ?: return null
        return try {
            val key = wrappingKey(create = false) ?: return null
            val bytes = wrapped.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
            val cipher = Cipher.getInstance(TRANSFORM)
            cipher.init(Cipher.DECRYPT_MODE, key, GCMParameterSpec(TAG_BITS, bytes.copyOfRange(0, IV_BYTES)))
            cipher.doFinal(bytes, IV_BYTES, bytes.size - IV_BYTES)
        } catch (ex: KeyPermanentlyInvalidatedException) {
            null
        } catch (ex: javax.crypto.AEADBadTagException) {
            null
        }
    }

    /** Ask the system to establish presence: a strong biometric or the device credential. True only if the person passed. */
    private suspend fun authenticate(reason: String): Boolean = suspendCancellableCoroutine { cont ->
        val signal = CancellationSignal()
        cont.invokeOnCancellation { signal.cancel() }
        val prompt = BiometricPrompt.Builder(context)
            .setTitle(reason)
            .apply {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                    setAllowedAuthenticators(BiometricManager.Authenticators.BIOMETRIC_STRONG or BiometricManager.Authenticators.DEVICE_CREDENTIAL)
                } else {
                    @Suppress("DEPRECATION")
                    setDeviceCredentialAllowed(true)
                }
            }
            .build()
        prompt.authenticate(signal, context.mainExecutor, object : BiometricPrompt.AuthenticationCallback() {
            override fun onAuthenticationSucceeded(result: BiometricPrompt.AuthenticationResult) {
                if (cont.isActive) cont.resume(true)
            }

            override fun onAuthenticationError(errorCode: Int, errString: CharSequence) {
                if (cont.isActive) cont.resume(false)
            }
        })
    }

    private companion object {
        const val PROVIDER = "AndroidKeyStore"
        const val ALIAS = "np-uhdr-key-wrap"
        const val PREFS = "np-uhdr-key"
        const val WRAPPED = "wrapped"
        const val ENABLED = "enabled"
        const val TRANSFORM = "AES/GCM/NoPadding"
        const val IV_BYTES = 12
        const val TAG_BITS = 128

        /** How long the Keystore lets the key be used after the prompt. The data key is unwrapped within it (`UC-107`). */
        const val UNLOCK_WINDOW_SECONDS = 15
    }
}
