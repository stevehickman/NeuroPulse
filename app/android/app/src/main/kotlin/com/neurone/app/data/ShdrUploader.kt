package com.neurone.app.data

import android.content.Context
import androidx.security.crypto.EncryptedSharedPreferences
import androidx.security.crypto.MasterKey
import com.neurone.core.analytics.WarrantyAnalyticsGate
import java.security.SecureRandom
import okhttp3.CertificatePinner
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody

/**
 * SHDR fleet telemetry uploader — port of iOS SHDRUploader.swift.
 *
 * Consent subject: the WARRANTY OWNER (may be a clinic — CLAUDE.md §6.0).
 * Gated exclusively by [WarrantyAnalyticsGate]; this class must never
 * reference ConsentStore or ResearchAnalyticsGate (code-structural
 * independence of the two consent subjects).
 *
 * Device identity: an opaque locally generated 32-byte CSPRNG token stored in
 * Keystore-encrypted SharedPreferences — NEVER ANDROID_ID, advertising ID, or
 * any hardware identifier (parallel of the iOS Keychain token replacing
 * identifierForVendor, NP-PRIV-ANALYSIS-002 HIGH). Upgraded to the
 * hub-provisioned TRNG warranty token when the GATT characteristic ships
 * (OI-BLE-01 / OI-WA-03, NP-FW-EMMC-002 Rev A §A).
 *
 * Transport: TLS with SPKI SHA-256 certificate pinning (parallel of the iOS
 * SHDRFleetPinningDelegate). Replace the placeholder pins with hashes derived
 * from the production fleet endpoint certificates before deployment.
 */
class ShdrUploader(
    context: Context,
    private val warrantyGate: WarrantyAnalyticsGate,
) {
    companion object {
        const val FLEET_HOST = "fleet.neurone.life"
        const val FLEET_ENDPOINT = "https://$FLEET_HOST/v1/shdr"
        const val DEVICE_TOKEN_HEADER = "X-NP-Device-Token"
        private const val TOKEN_PREF = "np.shdr.device-token"
        private const val PREFS_FILE = "np-shdr-secure"

        // PLACEHOLDERS — replace with production SPKI SHA-256 pins before deploy.
        private val PINNED_SPKI_HASHES = listOf(
            "sha256/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
            "sha256/BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB=",
        )
    }

    private val prefs = run {
        val masterKey = MasterKey.Builder(context)
            .setKeyScheme(MasterKey.KeyScheme.AES256_GCM)
            .build()
        EncryptedSharedPreferences.create(
            context,
            PREFS_FILE,
            masterKey,
            EncryptedSharedPreferences.PrefKeyEncryptionScheme.AES256_SIV,
            EncryptedSharedPreferences.PrefValueEncryptionScheme.AES256_GCM,
        )
    }

    private val client = OkHttpClient.Builder()
        .certificatePinner(
            CertificatePinner.Builder()
                .apply { PINNED_SPKI_HASHES.forEach { add(FLEET_HOST, it) } }
                .build(),
        )
        .build()

    /**
     * Opaque device token. Generated once per install with SecureRandom;
     * replaced by [upgradeDeviceToken] when the hub provisions its TRNG token.
     */
    val deviceToken: String
        get() = prefs.getString(TOKEN_PREF, null) ?: run {
            val bytes = ByteArray(32).also { SecureRandom().nextBytes(it) }
            val token = bytes.joinToString("") { "%02x".format(it) }
            prefs.edit().putString(TOKEN_PREF, token).apply()
            token
        }

    /** OI-BLE-01: swap in the hub-provisioned TRNG warranty token (32 bytes). */
    fun upgradeDeviceToken(hubToken: ByteArray) {
        if (hubToken.size != 32) return
        prefs.edit()
            .putString(TOKEN_PREF, hubToken.joinToString("") { "%02x".format(it) })
            .apply()
    }

    /**
     * Upload an SHDR payload. No-ops unless the warranty owner has consented.
     * The payload is device telemetry only — the SHDR schema contains no user
     * biology by construction (CLAUDE.md §5.1); this class does not inspect it.
     */
    fun upload(shdrPayload: ByteArray, onResult: (Boolean) -> Unit) {
        if (!warrantyGate.isOpen) {
            onResult(false)
            return
        }
        val request = Request.Builder()
            .url(FLEET_ENDPOINT)
            .header(DEVICE_TOKEN_HEADER, deviceToken)
            .post(shdrPayload.toRequestBody("application/octet-stream".toMediaType()))
            .build()
        client.newCall(request).enqueue(object : okhttp3.Callback {
            override fun onFailure(call: okhttp3.Call, e: java.io.IOException) = onResult(false)
            override fun onResponse(call: okhttp3.Call, response: okhttp3.Response) {
                response.use { onResult(it.isSuccessful) }
            }
        })
    }
}
