package life.neurone.app.data

import life.neurone.core.analytics.WarrantyAnalyticsGate
import life.neurone.core.common.InMemoryKeyValueStore
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * Mirrors iOS SHDRUploaderTests OI-BLE-01 cases. Exercises the token identity
 * logic and the composition (ShdrUploadWiring) on plain JVM via the injected
 * [DeviceTokenStore] seam — no Android Keystore, no network.
 *
 * Parity note: the CSPRNG fallback is a 32-byte token rendered as 64 lowercase
 * hex chars; the hub-provisioned warranty token upgrade requires exactly 32 bytes.
 */
class ShdrUploaderTests {

    /** In-memory stand-in for EncryptedPrefsDeviceTokenStore. */
    private class FakeTokenStore(var value: String? = null) : DeviceTokenStore {
        override fun load(): String? = value
        override fun save(token: String) { value = token }
    }

    private fun uploader(store: DeviceTokenStore) =
        ShdrUploader(store, WarrantyAnalyticsGate(InMemoryKeyValueStore()))

    // 1. Fallback token: 32-byte CSPRNG → 64 lowercase-hex chars, generated once
    //    and persisted so the fleet-DB linkage is stable across reads.
    @Test
    fun fallbackTokenIs64Hex() {
        val store = FakeTokenStore()
        val up = uploader(store)

        val token = up.deviceToken
        assertEquals(64, token.length, "32-byte token must render as 64 hex chars")
        assertTrue(token.all { it in "0123456789abcdef" }, "token must be lowercase hex")
        assertEquals(token, store.value, "fallback token must be persisted on first read")
        assertEquals(token, up.deviceToken, "token must be stable across reads")
    }

    // 2. OI-BLE-01: upgrade adopts the hub-provisioned 32-byte TRNG token.
    @Test
    fun upgradeAdoptsHubToken() {
        val store = FakeTokenStore()
        val up = uploader(store)

        val hub = ByteArray(32) { it.toByte() }
        up.upgradeDeviceToken(hub)

        val expected = hub.joinToString("") { "%02x".format(it) }
        assertEquals(expected, store.value)
        assertEquals(expected, up.deviceToken)
    }

    // 3. OI-BLE-01: a null warrantyToken (disconnect) must NOT downgrade the token
    //    back to the CSPRNG fallback. The `?.let` in the collector is the null-drop.
    @Test
    fun nullWarrantyTokenDoesNotDowngrade() {
        val store = FakeTokenStore()
        val up = uploader(store)

        ShdrUploadWiring.applyWarrantyToken(ByteArray(32) { 0x7F.toByte() }, up) // adopt hub token
        val afterUpgrade = store.value
        assertNotNull(afterUpgrade)

        ShdrUploadWiring.applyWarrantyToken(null, up)                    // disconnect → null
        assertEquals(afterUpgrade, store.value, "null must never change the token")
    }

    // 4. A hub token that is not exactly 32 bytes is rejected (no-op).
    @Test
    fun shortOrLongHubTokenRejected() {
        val store = FakeTokenStore()
        val up = uploader(store)

        ShdrUploadWiring.applyWarrantyToken(ByteArray(32) { 0x11.toByte() }, up)  // known good token
        val before = store.value
        assertNotNull(before)

        up.upgradeDeviceToken(ByteArray(31))  // too short
        up.upgradeDeviceToken(ByteArray(33))  // too long
        up.upgradeDeviceToken(ByteArray(0))   // empty
        assertEquals(before, store.value, "non-32-byte tokens must be ignored")
    }

    // Wiring: closed warranty gate → no upload attempt (callback never fires).
    @Test
    fun uploadGatedByWarrantyConsent() {
        val up = uploader(FakeTokenStore())
        var result: Boolean? = null

        ShdrUploadWiring.applyUploadPending(
            pending = true,
            warrantyGateOpen = false,
            stagingPayload = byteArrayOf(1, 2, 3),
            uploader = up,
        ) { result = it }

        assertNull(result, "closed warranty gate must not invoke the upload callback")
    }

    // Wiring: pending but nothing staged → no upload attempt.
    @Test
    fun uploadNoOpWithoutStagingPayload() {
        val up = uploader(FakeTokenStore())
        var invoked = false

        ShdrUploadWiring.applyUploadPending(
            pending = true,
            warrantyGateOpen = true,
            stagingPayload = null,
            uploader = up,
        ) { invoked = true }

        assertFalse(invoked, "no staged payload → no upload attempt")
    }
}
