package life.neurone.app.data

import life.neurone.core.analytics.WarrantyAnalyticsGate
import life.neurone.core.common.InMemoryKeyValueStore
import java.io.ByteArrayInputStream
import java.security.cert.CertificateFactory
import java.security.cert.X509Certificate
import javax.net.ssl.SSLPeerUnverifiedException
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

/** The one helmet most tests connect. */
private const val HUB = "AA:BB:CC:00:00:01"

/**
 * Mirrors iOS SHDRUploaderTests OI-BLE-01 cases. Exercises the token identity
 * logic and the composition (ShdrUploadWiring) on plain JVM via the injected
 * [DeviceTokenStore] seam — no Android Keystore, no network.
 *
 * Parity note: the CSPRNG fallback is a 32-byte token rendered as 64 lowercase
 * hex chars; the hub-provisioned warranty token upgrade requires exactly 32 bytes.
 */
class ShdrUploaderTests {

    /** In-memory stand-in for SealedDeviceTokenStore. */
    private class FakeTokenStore : DeviceTokenStore {
        val byHub = mutableMapOf<String, String>()
        override fun load(hub: String): String? = byHub[hub]
        override fun save(hub: String, token: String) { byHub[hub] = token }
        /** The token kept for the one helmet a test connects. */
        val value: String? get() = byHub[HUB]
    }

    /** An uploader with a helmet connected, as it is whenever an upload is under way. */
    private fun uploader(store: DeviceTokenStore, hub: String? = HUB, consented: Boolean = false) =
        ShdrUploader(store, WarrantyAnalyticsGate(InMemoryKeyValueStore()).also { if (consented) it.grant() })
            .also { it.setHubIdentifier(hub) }

    // 1. Fallback token: 32-byte CSPRNG → 64 lowercase-hex chars, generated once
    //    and persisted so the fleet-DB linkage is stable across reads.
    // @verifies REQ-SHDR-02
    @Test
    fun fallbackTokenIs64Hex() {
        val store = FakeTokenStore()
        val up = uploader(store)

        val token = up.deviceToken!!
        assertEquals(64, token.length, "32-byte token must render as 64 hex chars")
        assertTrue(token.all { it in "0123456789abcdef" }, "token must be lowercase hex")
        assertEquals(token, store.value, "fallback token must be persisted on first read")
        assertEquals(token, up.deviceToken, "token must be stable across reads")
    }

    // Finding 6: the interim token is per helmet. Two helmets on one phone get two tokens; each keeps its own on reconnect.
    // @verifies REQ-SHDR-05#a
    @Test
    fun tokenIsPerHelmetNotPerPhone() {
        val store = FakeTokenStore()
        val up = uploader(store, hub = "AA:BB:CC:00:00:01")
        val a = up.deviceToken
        up.setHubIdentifier(null)
        up.setHubIdentifier("AA:BB:CC:00:00:02")
        val b = up.deviceToken
        up.setHubIdentifier(null)
        up.setHubIdentifier("AA:BB:CC:00:00:01")

        assertNotNull(a)
        assertNotEquals(a, b, "two helmets on one phone must not share a token")
        assertEquals(a, up.deviceToken, "a helmet keeps its own token across a reconnect")
    }

    // Finding 6: with no helmet named there is no token, so nothing is sent.
    // @verifies REQ-SHDR-05#b
    @Test
    fun noHelmetMeansNoTokenAndNoUpload() {
        // The warranty owner has consented, so only the missing helmet can stop the upload: a request would be async.
        val up = uploader(FakeTokenStore(), hub = null, consented = true)
        var result: Boolean? = null

        assertNull(up.deviceToken)
        up.upgradeDeviceToken(ByteArray(32) { 1 })   // no helmet to attach it to: ignored
        up.upload(byteArrayOf(1, 2, 3)) { result = it }

        assertEquals(false, result, "nothing may be sent for a helmet nobody can name")
    }

    // Finding 6: a token a helmet delivered is that helmet's alone; another helmet starts with its own.
    // @verifies REQ-SHDR-05#c
    @Test
    fun hubTokenBelongsToItsHelmetOnly() {
        val store = FakeTokenStore()
        val up = uploader(store, hub = "AA:BB:CC:00:00:01")
        up.upgradeDeviceToken(ByteArray(32) { 0x42 })
        val a = up.deviceToken

        up.setHubIdentifier(null)
        up.setHubIdentifier("AA:BB:CC:00:00:02")

        assertNotEquals(a, up.deviceToken, "helmet A's token must not identify helmet B")
    }

    // The sealed store keeps every helmet's token in the one sealed value.
    // @verifies REQ-SHDR-05#a
    @Test
    fun sealedStoreKeepsATokenPerHelmetAndDropsAPerPhoneValue() {
        val kv = InMemoryKeyValueStore()
        val store = SealedDeviceTokenStore(kv)
        kv.putString(ShdrUploader.TOKEN_PREF, "a-token-written-before-tokens-were-per-helmet")

        assertNull(store.load("hub-1"), "a value with no helmet on it belongs to no helmet")
        store.save("hub-1", "t1")
        store.save("hub-2", "t2")

        assertEquals("t1", store.load("hub-1"))
        assertEquals("t2", store.load("hub-2"))
    }

    // 2. OI-BLE-01: upgrade adopts the hub-provisioned 32-byte TRNG token.
    // @verifies REQ-SHDR-02
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
    // @verifies REQ-SHDR-02
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
    // @verifies REQ-SHDR-02
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
    // @verifies REQ-SHDR-01
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
    // @verifies REQ-SHDR-02
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
    // ── REQ-SHDR-04: the upload endpoint is pinned ─────────────────────────────────────────────────────────────
    //
    // The default client is private, so these read it by reflection rather than widen its visibility for a test.
    // The certificate below is a throwaway self-signed one (CN=not-the-fleet.test); it has never been the fleet's.

    private val strangerPem = """
-----BEGIN CERTIFICATE-----
MIIBkTCCATegAwIBAgIUIU/Su4YjinPNDpP72jdOaBtrlQgwCgYIKoZIzj0EAwIw
HTEbMBkGA1UEAwwSbm90LXRoZS1mbGVldC50ZXN0MCAXDTI2MTAxMjE3MTIyNFoY
DzIxMjYwOTE4MTcxMjI0WjAdMRswGQYDVQQDDBJub3QtdGhlLWZsZWV0LnRlc3Qw
WTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAAS2BAjYUDfS/D7Bj7y9MrVmtaCojUYn
0Hi+z2L/YrKwsSiRL8L1r997SACopH0RUV6ECiwhDeP4G4FjiLjlCkJyo1MwUTAd
BgNVHQ4EFgQUpGUNrUPzMKaeeyWeb5Oc7tc5DjIwHwYDVR0jBBgwFoAUpGUNrUPz
MKaeeyWeb5Oc7tc5DjIwDwYDVR0TAQH/BAUwAwEB/zAKBggqhkjOPQQDAgNIADBF
AiEAkV1niWk0BONCofAdY2IKBDbMQHweNT4OZyHI28iOUzYCIBcF0K2bT93UklQ0
DQe5uIuf3oUzoLfX59gNvtxE5zZD
-----END CERTIFICATE-----
    """.trimIndent()

    private fun stranger(): X509Certificate =
        CertificateFactory.getInstance("X.509")
            .generateCertificate(ByteArrayInputStream(strangerPem.toByteArray())) as X509Certificate

    private fun defaultPinner(): okhttp3.CertificatePinner {
        val f = ShdrUploader::class.java.getDeclaredField("client").apply { isAccessible = true }
        return (f.get(uploader(FakeTokenStore())) as okhttp3.OkHttpClient).certificatePinner
    }

    /** The pinned set as the uploader defines it (a private companion value, read by reflection). */
    @Suppress("UNCHECKED_CAST")
    private fun pinnedSet(): List<String> =
        ShdrUploader::class.java.getDeclaredField("PINNED_SPKI_HASHES")
            .apply { isAccessible = true }.get(null) as List<String>

    // @verifies REQ-SHDR-04#a
    @Test
    fun theRelayHostHasAPrimaryAndAtLeastOneDistinctBackupPin() {
        val pins = pinnedSet()
        assertTrue(pins.size >= 2, "expected a primary and a backup pin, found $pins")
        assertEquals(pins.size, pins.toSet().size, "the pins are distinct")
        assertTrue(pins.all { it.startsWith("sha256/") }, "every pin is an SPKI SHA-256: $pins")
    }

    // @verifies REQ-SHDR-04#b
    @Test
    fun aLeafWhoseKeyIsNotPinnedIsRefusedForTheRelayHost() {
        assertFailsWith<SSLPeerUnverifiedException> {
            // The phone connects to the relay only (REQ-SHDR-07), so the pins are the relay's.
            defaultPinner().check(ShdrUploader.RELAY_HOST, listOf(stranger()))
        }
    }
}
