package life.neurone.app

import android.content.Context
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import life.neurone.core.common.SecretKeyStore
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

/**
 * The data key that protects the app's UHDR-class state (SWR-MG-06), kept with the Android Keystore. The data key itself is
 * 32 random bytes the shared code makes; the Keystore holds a non-exportable AES key that wraps it, and only the wrapped
 * bytes are written to app storage. [destroy] deletes the wrapping key from the Keystore, so the wrapped bytes that remain
 * on the flash open under nothing, which is what makes the data unreadable (and not merely the file deleted).
 */
class AndroidSecretKeyStore(context: Context) : SecretKeyStore {
    private val prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)

    private fun keystore(): KeyStore = KeyStore.getInstance(PROVIDER).apply { load(null) }

    private fun wrappingKey(create: Boolean): SecretKey? {
        (keystore().getKey(ALIAS, null) as? SecretKey)?.let { return it }
        if (!create) return null
        val generator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, PROVIDER)
        generator.init(
            KeyGenParameterSpec.Builder(ALIAS, KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT)
                .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                .setKeySize(256)
                .build(),
        )
        return generator.generateKey()
    }

    override fun load(): ByteArray? {
        val wrapped = prefs.getString(WRAPPED, null) ?: return null
        val key = wrappingKey(create = false) ?: return null
        return try {
            val bytes = wrapped.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
            val cipher = Cipher.getInstance(TRANSFORM)
            cipher.init(Cipher.DECRYPT_MODE, key, GCMParameterSpec(TAG_BITS, bytes.copyOfRange(0, IV_BYTES)))
            cipher.doFinal(bytes, IV_BYTES, bytes.size - IV_BYTES)
        } catch (ex: java.security.GeneralSecurityException) {
            // A wrapped key that no longer opens is a key that is gone.
            null
        }
    }

    override fun store(key: ByteArray) {
        val cipher = Cipher.getInstance(TRANSFORM)
        cipher.init(Cipher.ENCRYPT_MODE, wrappingKey(create = true))
        val wrapped = cipher.iv + cipher.doFinal(key)
        check(prefs.edit().putString(WRAPPED, wrapped.joinToString("") { "%02x".format(it) }).commit()) { "could not keep the wrapped key" }
    }

    override fun destroy() {
        // The Keystore entry first: with it gone the wrapped bytes open under nothing, whatever becomes of the preferences file.
        val ks = keystore()
        if (ks.containsAlias(ALIAS)) ks.deleteEntry(ALIAS)
        prefs.edit().remove(WRAPPED).commit()
    }

    private companion object {
        const val PROVIDER = "AndroidKeyStore"
        const val ALIAS = "np-data-key-wrap"
        const val PREFS = "np-data-key"
        const val WRAPPED = "wrapped"
        const val TRANSFORM = "AES/GCM/NoPadding"
        const val IV_BYTES = 12
        const val TAG_BITS = 128
    }
}
