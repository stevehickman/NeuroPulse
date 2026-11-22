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
        val fromHub = mutableSetOf<String>()
        override fun load(hub: String): String? = byHub[hub]
        override fun save(hub: String, token: String) { byHub[hub] = token; fromHub -= hub }
        override fun loadHubToken(hub: String): String? = byHub[hub]?.takeIf { hub in fromHub }
        override fun saveHubToken(hub: String, token: String) { byHub[hub] = token; fromHub += hub }
        val value: String? get() = byHub[HUB]
    }

    /** Staging in memory, keyed as a host keys it. */
    private class MemoryStaging : ShdrStaging {
        val files = mutableMapOf<String, ByteArray>()
        override fun read(helmetKey: String): ByteArray? = files[helmetKey]
        override fun stage(helmetKey: String, record: ByteArray) { require(helmetKey.matches(ShdrStaging.HELMET_KEY)); files[helmetKey] = record }
        override fun clear(helmetKey: String) { files.remove(helmetKey) }
    }

    /** A transport that must not be reached. */
    private val never = object : HttpTransport {
        override suspend fun post(request: HttpRequest): HttpResponse? = error("a request was made")
    }

    private fun uploader(store: DeviceTokenStore, hub: String? = HUB, consented: Boolean = false, transport: HttpTransport = never, key: String = "") =
        FleetUploader(store, WarrantyAnalyticsGate(WarrantyOwnerStore(InMemoryKeyValueStore())).also { if (consented) it.grant(HUB) }, transport, gatewayKeyConfigHex = key)
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
        FleetUploadWiring.applyWarrantyToken(ByteArray(32) { 0x7F }, HUB, up)
        val after = store.value
        assertNotNull(after)
        FleetUploadWiring.applyWarrantyToken(null, HUB, up)
        assertEquals(after, store.value)
    }

    // @verifies REQ-SHDR-02
    @Test fun shortOrLongHubTokenRejected() {
        val store = FakeTokenStore()
        val up = uploader(store)
        FleetUploadWiring.applyWarrantyToken(ByteArray(32) { 0x11 }, HUB, up)
        val before = store.value
        up.upgradeDeviceToken(ByteArray(31)); up.upgradeDeviceToken(ByteArray(33)); up.upgradeDeviceToken(ByteArray(0))
        assertEquals(before, store.value)
    }

    // @verifies REQ-SHDR-01
    @Test fun uploadGatedByWarrantyConsent() = runBlocking {
        val records = WarrantyOwnerStore(InMemoryKeyValueStore(), staging = MemoryStaging())
        val gate = WarrantyAnalyticsGate(records).also { it.grant(HUB) }
        assertTrue(gate.stage(HUB, byteArrayOf(1, 2, 3)))
        gate.revoke(HUB)
        val up = FleetUploader(FakeTokenStore(), gate, never, gatewayKeyConfigHex = "00").also { it.setHubIdentifier(HUB) }
        assertFalse(up.uploadStaged(), "a closed gate makes no attempt")
        assertFalse(uploader(FakeTokenStore(), consented = false, key = "00").upload(byteArrayOf(1)), "and the uploader re-checks it")
    }

    // @verifies REQ-SHDR-02
    @Test fun uploadNoOpWithoutStagingPayload() = runBlocking {
        val gate = WarrantyAnalyticsGate(WarrantyOwnerStore(InMemoryKeyValueStore(), staging = MemoryStaging())).also { it.grant(HUB) }
        val up = FleetUploader(FakeTokenStore(), gate, never, gatewayKeyConfigHex = "00").also { it.setHubIdentifier(HUB) }
        assertFalse(up.uploadStaged(), "nothing staged, nothing sent")
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

    // @verifies REQ-SHDR-08#a
    @Test fun aHelmetWhoseOwnerHasNotConsentedIsNotUploadedThoughAnotherHelmetOnThisPhoneIs() = runBlocking {
        val records = WarrantyOwnerStore(InMemoryKeyValueStore())
        val gate = WarrantyAnalyticsGate(records).also { it.grant(HUB) }
        val gw = Gateway()
        val tokens = FakeTokenStore()
        val up = FleetUploader(tokens, gate, gw, gatewayKeyConfigHex = keyConfig)
        // The clinic's helmet connects to a phone whose own helmet is consented: nothing is sent, and no token is made for it.
        val clinic = "AA:BB:CC:00:00:02"
        up.setHubIdentifier(clinic)
        assertFalse(up.upload("record".encodeToByteArray()))
        assertTrue(gw.seen.isEmpty(), "no request was made")
        assertNull(tokens.byHub[clinic])
        // With no helmet connected nothing is sent either.
        up.setHubIdentifier(null)
        assertFalse(up.upload("record".encodeToByteArray()))
        assertTrue(gw.seen.isEmpty())
        // Control: the consented helmet reconnects and its record is taken.
        up.setHubIdentifier(HUB)
        assertTrue(up.upload("record".encodeToByteArray()))
        assertEquals(1, gw.seen.size)
        // And once the clinic's owner consents for its helmet, that helmet uploads under its own token.
        gate.grant(clinic)
        up.setHubIdentifier(clinic)
        assertTrue(up.upload("record".encodeToByteArray()))
        assertNotEquals(tokens.byHub[HUB], tokens.byHub[clinic])
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

    // ── Custody of the staged record and the owner's decisions (`OI-PRIV-63`) ───────────────────────────────────────

    private class Rig(val staging: MemoryStaging, val records: WarrantyOwnerStore, val gate: WarrantyAnalyticsGate, val tokens: FakeTokenStore)

    private fun rig(): Rig {
        val staging = MemoryStaging()
        val records = WarrantyOwnerStore(InMemoryKeyValueStore(), staging = staging)
        return Rig(staging, records, WarrantyAnalyticsGate(records), FakeTokenStore())
    }

    // @verifies REQ-SHDR-08#e
    @Test fun aStagedRecordIsSentOnlyForTheHelmetThatStagedIt() = runBlocking {
        val r = rig()
        val other = "AA:BB:CC:00:00:02"
        r.gate.grant(HUB)
        r.gate.grant(other)
        assertTrue(r.gate.stage(HUB, "record-of-A".encodeToByteArray()))
        val gw = Gateway()
        val up = FleetUploader(r.tokens, r.gate, gw, gatewayKeyConfigHex = keyConfig)
        // Helmet B, whose owner has consented too, connects: A's record does not leave under B's consent and token.
        up.setHubIdentifier(other)
        assertFalse(up.uploadStaged())
        assertTrue(gw.seen.isEmpty(), "no request was made")
        assertEquals(1, r.staging.files.size, "A's record is still waiting for A")
        // A reconnects: its record is sent under its own token, then removed.
        up.setHubIdentifier(HUB)
        assertTrue(up.uploadStaged())
        assertEquals("record-of-A".encodeToByteArray().toHex(), gw.innerBody)
        assertEquals(r.tokens.byHub[HUB], gw.innerToken)
        assertTrue(r.staging.files.isEmpty())
    }

    // @verifies REQ-SHDR-08#e
    @Test fun oneReadingOfTheHelmetNamesTheConsentTheTokenAndTheRecord() = runBlocking {
        val r = rig()
        val other = "AA:BB:CC:00:00:02"
        r.gate.grant(HUB)
        r.gate.stage(HUB, "a".encodeToByteArray())
        val gw = Gateway()
        val up = FleetUploader(r.tokens, r.gate, gw, gatewayKeyConfigHex = keyConfig)
        // The link has moved on to B (not consented) by the time the uploader's copy is read; the attempt is for the helmet read once.
        up.setHubIdentifier(other)
        assertTrue(up.uploadStaged(HUB))
        assertEquals(r.tokens.byHub[HUB], gw.innerToken, "A's token, not B's")
        assertNull(r.tokens.byHub[other], "no token was made for B")
    }

    // @verifies REQ-SHDR-08#e
    @Test fun nothingIsStagedForAHelmetWhoseOwnerHasNotConsented() {
        val r = rig()
        assertFalse(r.gate.stage(HUB, byteArrayOf(1)), "never asked")
        assertFalse(r.gate.stage(null, byteArrayOf(1)), "no helmet")
        r.gate.grant(HUB)
        r.gate.revoke(HUB)
        assertFalse(r.gate.stage(HUB, byteArrayOf(1)), "said no")
        assertTrue(r.staging.files.isEmpty())
    }

    // @verifies REQ-SHDR-08#f
    @Test fun aRevokeDeletesTheHelmetsStagedRecordSoALaterGrantSendsNothingFromBeforeIt() = runBlocking {
        val r = rig()
        val other = "AA:BB:CC:00:00:02"
        r.gate.grant(HUB)
        r.gate.grant(other)
        r.gate.stage(HUB, byteArrayOf(1))
        r.gate.stage(other, byteArrayOf(2))
        r.gate.revoke(HUB)
        assertNull(r.records.staged(HUB))
        assertEquals(listOf<Byte>(2), r.records.staged(other)?.toList(), "the other helmet's record stays")
        r.gate.grant(HUB)
        val up = FleetUploader(r.tokens, r.gate, never, gatewayKeyConfigHex = keyConfig).also { it.setHubIdentifier(HUB) }
        assertFalse(up.uploadStaged(), "the record from before the revoke is gone")
    }

    // @verifies REQ-SHDR-09#d
    @Test fun aDifferentHubTokenOverAKeptOneEndsTheHelmetsCustody() {
        val r = rig()
        val other = "AA:BB:CC:00:00:02"
        val enrolment = CharacterisationEnrolmentGate(r.records, r.gate)
        r.gate.grant(HUB); r.gate.grant(other)
        enrolment.grant(HUB); enrolment.grant(other)
        r.gate.stage(HUB, byteArrayOf(1)); r.gate.stage(other, byteArrayOf(2))
        val up = FleetUploader(r.tokens, r.gate, never).also { it.setHubIdentifier(HUB) }
        up.deviceToken // an interim token first
        up.upgradeDeviceToken(ByteArray(32) { 1 })
        assertTrue(r.gate.isOpen(HUB) && enrolment.isOpen(HUB), "a hub token over an interim one is the ordinary upgrade")
        up.upgradeDeviceToken(ByteArray(32) { 1 })
        assertTrue(r.gate.isOpen(HUB) && enrolment.isOpen(HUB), "the same hub token again changes nothing")
        // A factory reset or a new owner: the hub delivers a different token.
        up.upgradeDeviceToken(ByteArray(32) { 2 })
        assertFalse(r.gate.isDecided(HUB), "the old owner's consent is forgotten: the new owner is asked")
        assertFalse(enrolment.isOpen(HUB))
        assertEquals(0, enrolment.consentEpoch(HUB), "the new owner's first enrolment is genuinely their first")
        assertNull(r.records.staged(HUB), "the old owner's staged record is gone")
        assertEquals(ByteArray(32) { 2 }.toHex(), up.deviceToken)
        // Every other helmet is untouched.
        assertTrue(r.gate.isOpen(other) && enrolment.isOpen(other))
        assertEquals(1, enrolment.consentEpoch(other))
        assertEquals(listOf<Byte>(2), r.records.staged(other)?.toList())
        // And the new owner's grant starts a new custody: enrolment is not brought back.
        r.gate.grant(HUB)
        assertFalse(enrolment.isOpen(HUB))
        assertEquals(1, enrolment.grant(HUB))
    }

    // @verifies REQ-SHDR-09#d
    @Test fun theSealedStoreTellsAHubTokenFromAnInterimOne() {
        val store = SealedDeviceTokenStore(InMemoryKeyValueStore())
        store.save("hub-1", "aa".repeat(32))
        assertEquals("aa".repeat(32), store.load("hub-1"))
        assertNull(store.loadHubToken("hub-1"), "an interim token is not a hub token")
        store.saveHubToken("hub-1", "bb".repeat(32))
        assertEquals("bb".repeat(32), store.load("hub-1"), "the token sent is the bare hex")
        assertEquals("bb".repeat(32), store.loadHubToken("hub-1"))
        assertNull(store.loadHubToken("hub-2"))
    }

    // @verifies REQ-SHDR-09#d
    @Test fun theWiringKeepsAHubTokenForTheHelmetTheLinkNamesWhenItArrives() {
        val r = rig()
        val other = "AA:BB:CC:00:00:02"
        r.gate.grant(HUB)
        val up = FleetUploader(r.tokens, r.gate, never).also { it.setHubIdentifier(HUB) }
        FleetUploadWiring.applyWarrantyToken(ByteArray(32) { 1 }, HUB, up)
        // The uploader's copy still says A when B's token arrives: B's token is B's, and A's custody is not ended by it.
        FleetUploadWiring.applyWarrantyToken(ByteArray(32) { 2 }, other, up)
        assertTrue(r.gate.isOpen(HUB))
        assertEquals(ByteArray(32) { 1 }.toHex(), r.tokens.byHub[HUB])
        assertEquals(ByteArray(32) { 2 }.toHex(), r.tokens.byHub[other])
        FleetUploadWiring.applyWarrantyToken(ByteArray(32) { 3 }, null, up)
        assertEquals(ByteArray(32) { 1 }.toHex(), r.tokens.byHub[HUB], "with no helmet named nothing is kept")
    }

    // @verifies REQ-SHDR-08#e
    @Test fun theFileStagingKeepsOneFilePerHelmetKeyAndDropsTheUnboundFile() {
        val dir = kotlin.io.path.createTempDirectory("staging").toFile()
        try {
            java.io.File(dir, FileShdrStaging.LEGACY_FILE_NAME).writeBytes(byteArrayOf(9))
            val staging = FileShdrStaging(dir)
            assertFalse(java.io.File(dir, FileShdrStaging.LEGACY_FILE_NAME).exists(), "a record that names no helmet is never sent")
            val a = "a".repeat(64); val b = "b".repeat(64)
            staging.stage(a, byteArrayOf(1))
            assertNull(staging.read(b))
            assertEquals(listOf<Byte>(1), staging.read(a)?.toList())
            staging.clear(a)
            assertNull(staging.read(a))
            kotlin.test.assertFailsWith<IllegalArgumentException> { staging.read("../AA:BB:CC:00:00:01") }
            kotlin.test.assertFailsWith<IllegalArgumentException> { staging.stage("AA:BB:CC:00:00:01", byteArrayOf(1)) }
        } finally {
            dir.deleteRecursively()
        }
    }

    // @verifies REQ-SHDR-04#a
    @Test fun theRelayHasAPrimaryAndAtLeastOneDistinctBackupPin() {
        val pins = FleetConfig.PINNED_SPKI_HASHES
        assertTrue(pins.size >= 2)
        assertEquals(pins.size, pins.toSet().size)
        assertTrue(pins.all { it.startsWith("sha256/") })
    }
}
