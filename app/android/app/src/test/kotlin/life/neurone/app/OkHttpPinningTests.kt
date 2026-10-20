package life.neurone.app

import life.neurone.core.analytics.FleetConfig
import java.io.ByteArrayInputStream
import java.security.cert.CertificateFactory
import java.security.cert.X509Certificate
import javax.net.ssl.SSLPeerUnverifiedException
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

/**
 * `REQ-SHDR-04` on Android: the fleet client pins the relay. The pins themselves are `FleetConfig`'s, one list for every host. The
 * certificate below is a throwaway self-signed one (CN=not-the-fleet.test); it has never been the fleet's.
 */
class OkHttpPinningTests {

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

    // @verifies REQ-SHDR-04#a
    @Test
    fun theRelayHostHasAPrimaryAndAtLeastOneDistinctBackupPin() {
        val pins = FleetConfig.PINNED_SPKI_HASHES
        assertTrue(pins.size >= 2, "expected a primary and a backup pin, found $pins")
        assertEquals(pins.size, pins.toSet().size, "the pins are distinct")
        assertTrue(pins.all { it.startsWith("sha256/") }, "every pin is an SPKI SHA-256: $pins")
    }

    // @verifies REQ-SHDR-04#b
    @Test
    fun aLeafWhoseKeyIsNotPinnedIsRefusedForTheRelayHost() {
        val pinner = OkHttpTransport.pinnedClient(FleetConfig.RELAY_HOST, FleetConfig.PINNED_SPKI_HASHES).certificatePinner
        assertFailsWith<SSLPeerUnverifiedException> {
            pinner.check(FleetConfig.RELAY_HOST, listOf(stranger()))
        }
    }
}
