package life.neurone.core.analytics

import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.data.hexToBytes
import life.neurone.core.npps.NppsCore
import life.neurone.core.platform.toHex
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** The fleet upload as Oblivious HTTP, from the Kotlin side to a gateway played by the same core (`REQ-SW03-24`). */
class FleetOhttpTests {
    private fun core(op: String, build: kotlinx.serialization.json.JsonObjectBuilder.() -> Unit): JsonObject =
        NppsCore.migration(buildJsonObject { put("op", op); build() })

    private val key = core("ohttpGenerateKey") { put("seed", ByteArray(32) { 5 }.toHex()); put("keyId", 1) }
    private val keyConfig = key["keyConfig"]!!.jsonPrimitive.content
    private val privateKey = key["privateKey"]!!.jsonPrimitive.content

    private fun seal(payload: ByteArray) = FleetOhttp.seal(
        keyConfig, "fleet.example", "/v1/shdr", "X-NP-Device-Token", "tok", payload, entropy = ByteArray(32) { 9 },
    )

    // @verifies REQ-SW03-24#a
    @Test fun theGatewayReadsTheRecordAndTheTokenAndTheAnswerComesBack() {
        val sealed = seal("record".encodeToByteArray())
        val g = core("ohttpGatewayOpen") { put("keyConfig", keyConfig); put("privateKey", privateKey); put("request", sealed.request.toHex()) }
        assertEquals("/v1/shdr", g["path"]!!.jsonPrimitive.content)
        assertEquals("record".encodeToByteArray().toHex(), g["body"]!!.jsonPrimitive.content)
        val headers = g["headers"] as JsonArray
        assertEquals("x-np-device-token", (headers[0] as JsonArray)[0].jsonPrimitive.content)
        assertEquals("tok", (headers[0] as JsonArray)[1].jsonPrimitive.content)
        val reply = core("ohttpGatewaySeal") {
            put("enc", g["enc"]!!.jsonPrimitive.content); put("secret", g["secret"]!!.jsonPrimitive.content)
            put("status", 204); put("headers", JsonArray(emptyList())); put("body", ""); put("entropy", ByteArray(16) { 1 }.toHex())
        }["response"]!!.jsonPrimitive.content.hexToBytes()
        val answer = FleetOhttp.open(sealed, reply)
        assertEquals(204, answer.status)
        assertTrue(answer.accepted)
    }

    // @verifies REQ-SW03-24#b
    @Test fun whatTheRelayCanSeeNamesNeitherTheHostNorTheToken() {
        val wire = seal("record".encodeToByteArray()).request.toHex()
        assertFalse(wire.contains("fleet.example".encodeToByteArray().toHex()))
        assertFalse(wire.contains("tok".encodeToByteArray().toHex()))
        assertEquals(seal(ByteArray(1)).request.size, seal(ByteArray(300)).request.size, "two sizes in one bucket are one length")
    }

    // @verifies REQ-SW03-24#c
    @Test fun anAnswerThatDoesNotAuthenticateIsRefused() {
        val sealed = seal("r".encodeToByteArray())
        assertFailsWith<IllegalArgumentException> { FleetOhttp.open(sealed, ByteArray(64) { 3 }) }
    }

    // @verifies REQ-SW03-24#c
    @Test fun aKeyConfigurationForAnotherSuiteIsRefused() {
        val other = keyConfig.hexToBytes().also { it[2] = 0x10 }.toHex()
        assertFailsWith<IllegalArgumentException> { FleetOhttp.seal(other, "a", "/", "h", "t", ByteArray(1), entropy = ByteArray(32)) }
    }

    private fun gatewayReply(sealed: FleetOhttp.Sealed, status: Int): ByteArray {
        val g = core("ohttpGatewayOpen") { put("keyConfig", keyConfig); put("privateKey", privateKey); put("request", sealed.request.toHex()) }
        return core("ohttpGatewaySeal") {
            put("enc", g["enc"]!!.jsonPrimitive.content); put("secret", g["secret"]!!.jsonPrimitive.content)
            put("status", status); put("headers", JsonArray(emptyList())); put("body", ""); put("entropy", ByteArray(16) { 1 }.toHex())
        }["response"]!!.jsonPrimitive.content.hexToBytes()
    }

    // @verifies REQ-SW03-24#c
    @Test fun onlyTheFleetServicesOwnSuccessInsideAnAuthenticAnswerCountsAsUploaded() {
        val sealed = seal("r".encodeToByteArray())
        val ok = gatewayReply(sealed, 204)
        assertTrue(FleetOhttp.accepted(sealed, true, "message/ohttp-res", ok))
        assertFalse(FleetOhttp.accepted(sealed, true, "message/ohttp-res", gatewayReply(sealed, 500)), "the fleet service refused")
        assertFalse(FleetOhttp.accepted(sealed, false, "message/ohttp-res", ok), "the relay failed")
        assertFalse(FleetOhttp.accepted(sealed, true, "text/plain", ok), "not an oblivious answer")
        assertFalse(FleetOhttp.accepted(sealed, true, "message/ohttp-res", null))
        assertFalse(FleetOhttp.accepted(sealed, true, "message/ohttp-res", ok.also { it[it.size - 1] = (it[it.size - 1] + 1).toByte() }), "tampered")
    }
}
