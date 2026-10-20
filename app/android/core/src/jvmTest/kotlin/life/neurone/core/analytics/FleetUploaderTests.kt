package life.neurone.core.analytics

import kotlinx.coroutines.runBlocking
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.data.hexToBytes
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import life.neurone.core.npps.NppsCore
import life.neurone.core.platform.toHex
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The one helmet most tests connect. */
private const val HUB = "AA:BB:CC:00:00:01"

/**
 * The SHDR fleet uploader, once for every host (`REQ-SHDR-01`, `-02`, `-05`, `-07`): the token logic and the composition rules on
 * plain JVM through the [DeviceTokenStore] and [HttpTransport] seams, and the sealed upload end to end against a gateway played by
 * the shared core. Moved here from the Android module, whose unit tests cannot load the core's native library.
 */
class FleetUploaderTests {

    private class FakeTokenStore : DeviceTokenStore {
        val byHub = mutableMapOf<String, String>()
        override fun load(hub: String): String? = byHub[hub]
        override fun save(hub: String, token: String) { byHub[hub] = token }
        val value: String? get() = byHub[HUB]
    }

    /** A transport that must not be reached. */
    private val never = object : HttpTransport {
        override suspend fun post(request: HttpRequest): HttpResponse? = error("a request was made")
    }

    private fun uploader(store: DeviceTokenStore, hub: String? = HUB, consented: Boolean = false, transport: HttpTransport = never, key: String = "") =
        FleetUploader(store, WarrantyAnalyticsGate(InMemoryKeyValueStore()).also { if (consented) it.grant() }, transport, gatewayKeyConfigHex = key)
            .also { it.setHubIdentifier(hub) }

    // @verifies REQ-SHDR-02
    @Test fun fallbackTokenIs64Hex() {
        val store = FakeTokenStore()
        val up = uploader(store)
        val token = up.deviceToken!!
        assertEquals(64, token.length)
        assertTrue(token.all { it in "0123456789abcdef" })
        assertEquals(token, store.value, "persisted on first read")
        assertEquals(token, up.deviceToken, "stable across reads")
    }

    // @verifies REQ-SHDR-05#a
    @Test fun tokenIsPerHelmetNotPerPhone() {
        val up = uploader(FakeTokenStore(), hub = "AA:BB:CC:00:00:01")
        val a = up.deviceToken
        up.setHubIdentifier(null)
        up.setHubIdentifier("AA:BB:CC:00:00:02")
        val b = up.deviceToken
        up.setHubIdentifier(null)
        up.setHubIdentifier("AA:BB:CC:00:00:01")
        assertNotNull(a)
        assertNotEquals(a, b)
        assertEquals(a, up.deviceToken)
    }

    // @verifies REQ-SHDR-05#b
    @Test fun noHelmetMeansNoTokenAndNoUpload() = runBlocking {
        val up = uploader(FakeTokenStore(), hub = null, consented = true, key = "00")
        assertNull(up.deviceToken)
        up.upgradeDeviceToken(ByteArray(32) { 1 })
        assertFalse(up.upload(byteArrayOf(1, 2, 3)))
    }

    // @verifies REQ-SHDR-05#c
    @Test fun hubTokenBelongsToItsHelmetOnly() {
        val up = uploader(FakeTokenStore(), hub = "AA:BB:CC:00:00:01")
        up.upgradeDeviceToken(ByteArray(32) { 0x42 })
        val a = up.deviceToken
        up.setHubIdentifier(null)
        up.setHubIdentifier("AA:BB:CC:00:00:02")
        assertNotEquals(a, up.deviceToken)
    }

    // @verifies REQ-SHDR-05#a
    @Test fun sealedStoreKeepsATokenPerHelmetAndDropsAPerPhoneValue() {
        val kv = InMemoryKeyValueStore()
        val store = SealedDeviceTokenStore(kv)
        kv.putString(FleetUploader.TOKEN_PREF, "a-token-written-before-tokens-were-per-helmet")
        assertNull(store.load("hub-1"))
        store.save("hub-1", "t1")
        store.save("hub-2", "t2")
        assertEquals("t1", store.load("hub-1"))
        assertEquals("t2", store.load("hub-2"))
    }

    // @verifies REQ-SHDR-02
    @Test fun upgradeAdoptsHubToken() {
        val store = FakeTokenStore()
        val up = uploader(store)
        val hub = ByteArray(32) { it.toByte() }
        up.upgradeDeviceToken(hub)
        assertEquals(hub.toHex(), store.value)
        assertEquals(hub.toHex(), up.deviceToken)
    }

    // @verifies REQ-SHDR-02
    @Test fun nullWarrantyTokenDoesNotDowngrade() {
        val store = FakeTokenStore()
        val up = uploader(store)
        FleetUploadWiring.applyWarrantyToken(ByteArray(32) { 0x7F }, up)
        val after = store.value
        assertNotNull(after)
        FleetUploadWiring.applyWarrantyToken(null, up)
        assertEquals(after, store.value)
    }

    // @verifies REQ-SHDR-02
    @Test fun shortOrLongHubTokenRejected() {
        val store = FakeTokenStore()
        val up = uploader(store)
        FleetUploadWiring.applyWarrantyToken(ByteArray(32) { 0x11 }, up)
        val before = store.value
        up.upgradeDeviceToken(ByteArray(31)); up.upgradeDeviceToken(ByteArray(33)); up.upgradeDeviceToken(ByteArray(0))
        assertEquals(before, store.value)
    }

    // @verifies REQ-SHDR-01
    @Test fun uploadGatedByWarrantyConsent() = runBlocking {
        var result: Boolean? = null
        FleetUploadWiring.applyUploadPending(true, false, byteArrayOf(1, 2, 3), uploader(FakeTokenStore())) { result = it }
        assertNull(result, "a closed gate makes no attempt")
        assertFalse(uploader(FakeTokenStore(), consented = false, key = "00").upload(byteArrayOf(1)), "and the uploader re-checks it")
    }

    // @verifies REQ-SHDR-02
    @Test fun uploadNoOpWithoutStagingPayload() = runBlocking {
        var invoked = false
        FleetUploadWiring.applyUploadPending(true, true, null, uploader(FakeTokenStore())) { invoked = true }
        assertFalse(invoked)
    }

    // ── The sealed upload, end to end ────────────────────────────────────────────────────────────────────────────────

    private fun core(op: String, build: kotlinx.serialization.json.JsonObjectBuilder.() -> Unit) =
        NppsCore.migration(buildJsonObject { put("op", op); build() })

    private val key = core("ohttpGenerateKey") { put("seed", ByteArray(32) { 5 }.toHex()); put("keyId", 1) }
    private val keyConfig = key["keyConfig"]!!.jsonPrimitive.content
    private val privateKey = key["privateKey"]!!.jsonPrimitive.content

    /** The relay and the gateway in one: opens the message, notes the token inside it, answers with [innerStatus] sealed. */
    private inner class Gateway(var innerStatus: Int = 204) : HttpTransport {
        val seen = mutableListOf<HttpRequest>()
        var innerToken: String? = null
        var innerBody: String? = null
        override suspend fun post(request: HttpRequest): HttpResponse? {
            seen += request
            val g = core("ohttpGatewayOpen") { put("keyConfig", keyConfig); put("privateKey", privateKey); put("request", request.body.toHex()) }
            innerToken = (((g["headers"] as JsonArray)[0]) as JsonArray)[1].jsonPrimitive.content
            innerBody = g["body"]!!.jsonPrimitive.content
            val reply = core("ohttpGatewaySeal") {
                put("enc", g["enc"]!!.jsonPrimitive.content); put("secret", g["secret"]!!.jsonPrimitive.content)
                put("status", innerStatus); put("headers", JsonArray(emptyList())); put("body", ""); put("entropy", ByteArray(16) { 1 }.toHex())
            }["response"]!!.jsonPrimitive.content.hexToBytes()
            return HttpResponse(200, mapOf("Content-Type" to FleetOhttp.RESPONSE_MEDIA_TYPE), reply)
        }
    }

    // @verifies REQ-SHDR-07#b
    @Test fun theRelayIsSentASealedMessageAndTheTokenTravelsInsideItOnly() = runBlocking {
        val gw = Gateway()
        val up = uploader(FakeTokenStore(), consented = true, transport = gw, key = keyConfig)
        assertTrue(up.upload("record".encodeToByteArray()))
        val sent = gw.seen.single()
        assertEquals(FleetConfig.RELAY_URL, sent.url)
        assertEquals("message/ohttp-req", sent.headers["Content-Type"])
        assertTrue(sent.headers.keys.none { it.equals(FleetConfig.DEVICE_TOKEN_HEADER, ignoreCase = true) }, "no token header on the wire")
        assertEquals(up.deviceToken, gw.innerToken)
        assertEquals("record".encodeToByteArray().toHex(), gw.innerBody)
    }

    // @verifies REQ-SHDR-07#c
    @Test fun theFleetServicesOwnRefusalCountsAsNotUploaded() = runBlocking {
        val up = uploader(FakeTokenStore(), consented = true, transport = Gateway(innerStatus = 500), key = keyConfig)
        assertFalse(up.upload("r".encodeToByteArray()))
    }

    // @verifies REQ-SHDR-07#c
    @Test fun noAnswerFromTheRelayCountsAsNotUploaded() = runBlocking {
        val none = object : HttpTransport { override suspend fun post(request: HttpRequest): HttpResponse? = null }
        assertFalse(uploader(FakeTokenStore(), consented = true, transport = none, key = keyConfig).upload("r".encodeToByteArray()))
    }

    // @verifies REQ-SHDR-07#d
    @Test fun withNoGatewayKeyNothingIsSent() = runBlocking {
        assertFalse(uploader(FakeTokenStore(), consented = true, transport = never, key = "").upload("r".encodeToByteArray()))
    }

    // @verifies REQ-SHDR-07#d
    @Test fun aGatewayKeyThatIsNotAKeyConfigurationSendsNothing() = runBlocking {
        assertFalse(uploader(FakeTokenStore(), consented = true, transport = never, key = "zz").upload("r".encodeToByteArray()))
    }

    // @verifies REQ-SHDR-04#a
    @Test fun theRelayHasAPrimaryAndAtLeastOneDistinctBackupPin() {
        val pins = FleetConfig.PINNED_SPKI_HASHES
        assertTrue(pins.size >= 2)
        assertEquals(pins.size, pins.toSet().size)
        assertTrue(pins.all { it.startsWith("sha256/") })
    }
}
