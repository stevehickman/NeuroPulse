package life.neurone.core.npps

import kotlinx.coroutines.runBlocking
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.int
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.cloud.AwsCredentials
import life.neurone.core.cloud.AwsSigningRequest
import life.neurone.core.cloud.NppsAwsSigV4
import life.neurone.core.common.EncryptedKeyValueStore
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.common.InMemorySecretKeyStore
import life.neurone.core.common.PassphraseGatedSecretKeyStore
import life.neurone.core.common.PassphraseResult
import life.neurone.core.data.ArchiveOpened
import life.neurone.core.data.BundleKeyDerivation
import life.neurone.core.data.BundleKeyMethod
import life.neurone.core.data.BundleSecretResult
import life.neurone.core.data.RandomSource
import life.neurone.core.data.RecordingArchive
import life.neurone.core.data.UhdrKeyDerivation
import life.neurone.core.platform.Ed25519
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertIs
import kotlin.test.assertTrue

/**
 * No key is text on its way from the Kotlin hosts to the core (SWR-MG-32 (e)). Each test does what an app does with a
 * secret it holds, records every request exactly as it is handed to the binding, and looks for the secret in them: as
 * the text it is, and as the hex it used to be sent as. It also counts the keys the core is left holding, which must be
 * what it held before: a key that is kept and never released stays in the core for the life of the process.
 */
class NoKeyInARequestTests {
    private val seen = mutableListOf<String>()
    private var counter = 0
    private val random = RandomSource { n -> ByteArray(n) { (++counter * 37 + 11).toByte() } }

    private fun hex(b: ByteArray) = b.joinToString("") { "%02x".format(it) }
    private fun held(): Int = NppsCore.migration(buildJsonObject { put("op", "keyCount") })["count"]!!.jsonPrimitive.int

    /** Run [work] with every request recorded, then check none holds any of [secrets] and the core holds no more keys than before. */
    private fun <T> recording(work: () -> T, secrets: (T) -> List<String>): T {
        val before = held()
        seen.clear()
        NppsCore.requestObserver = { seen += it }
        val result = try { work() } finally { NppsCore.requestObserver = null }
        assertTrue(seen.isNotEmpty(), "the work under test asked the core for something")
        for (secret in secrets(result)) {
            assertTrue(secret.length >= 12, "a secret too short to search for proves nothing")
            val found = seen.firstOrNull { it.contains(secret) }
            assertTrue(found == null, "a request holds a secret as text: ${found?.take(60)}…")
        }
        assertEquals(before, held(), "a key was kept in the core and not released")
        return result
    }

    @AfterTest fun stop() { NppsCore.requestObserver = null }

    // @verifies SWR-MG-32#e
    @Test
    fun theDataKeyIsInNoRequestWhenAValueIsSealedAndOpened() {
        val keys = InMemorySecretKeyStore()
        recording({
            val store = EncryptedKeyValueStore(InMemoryKeyValueStore(), keys, setOf("np.history"), random)
            store.putString("np.history", "the history of the person")
            assertEquals("the history of the person", store.getString("np.history"))
            keys.load()!!
        }) { key -> listOf(hex(key), hex(key).take(16)) }
    }

    // @verifies SWR-MG-32#e
    @Test
    fun thePassphraseTheWrappingKeyAndTheDataKeyAreInNoRequestOfThePassphraseGate() {
        val passphrase = "correct horse battery staple"
        val host = InMemoryKeyValueStore()
        val device = InMemorySecretKeyStore()
        recording({
            val first = PassphraseGatedSecretKeyStore(host, { device }, random)
            assertEquals(PassphraseResult.OK, runBlocking { first.setPassphrase(passphrase) })
            val key = first.load()!!
            val second = PassphraseGatedSecretKeyStore(host, { device }, random)
            assertEquals(PassphraseResult.WRONG, runBlocking { second.unlockWithPassphrase("not the passphrase at all") })
            assertEquals(PassphraseResult.OK, runBlocking { second.unlockWithPassphrase(passphrase) })
            assertContentEquals(key, second.load())
            assertEquals(PassphraseResult.OK, runBlocking { second.changePassphrase(passphrase, "another passphrase entirely") })
            key
        }) { key -> listOf(passphrase, "another passphrase entirely", "not the passphrase at all", hex(passphrase.encodeToByteArray()), hex(key), hex(key).take(16)) }
    }

    // @verifies SWR-MG-32#e
    @Test
    fun aRecoveryCodeAPassphraseAndTheBackupKeyAreInNoRequestWhenARecordingIsSealedAndOpened() {
        val recording = ByteArray(200_000) { (it * 31).toByte() }
        recording({
            val made = BundleKeyDerivation.newRecoveryCode(random)
            val kdf = BundleKeyDerivation.newKdf(BundleKeyMethod.RECOVERY_CODE, random)
            val typed = BundleKeyDerivation.recoveryCodeSecret(made.text)
            assertIs<BundleSecretResult.Ok>(typed)
            val key = BundleKeyDerivation.deriveKey(typed.secret, kdf)
            try {
                // Four chunks: the key used to be sent, as hex, once for each.
                val sealed = RecordingArchive.seal(recording, key, "recording:r1", random, kdf)
                val opened = RecordingArchive.open(sealed, key, "recording:r1")
                assertIs<ArchiveOpened.Ok>(opened)
                assertContentEquals(recording, opened.data)
            } finally {
                key.destroy()
            }
            val phrase = BundleKeyDerivation.passphraseSecret("café au lait noir, twice")
            assertIs<BundleSecretResult.Ok>(phrase)
            made.text
        }) { code ->
            // The secret a recovery code stands for is what the random source gave: the first 32 bytes it handed out.
            var n = 0
            val secret = ByteArray(32) { (++n * 37 + 11).toByte() }
            listOf(code, hex(secret), hex(secret).take(16), "café au lait noir, twice", hex("café au lait noir, twice".encodeToByteArray()))
        }
    }

    // @verifies SWR-MG-32#e
    @Test
    fun aSigningSeedTheDeviceKeysSeedAndACloudSecretAreInNoRequest() {
        val seed = ByteArray(32) { (it * 5 + 9).toByte() }
        val credentialSeed = ByteArray(32) { (it * 3 + 2).toByte() }
        val secret = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY"
        val token = "FQoGZXIvYXdzEXAMPLE-session-token"
        recording({
            val publicKey = Ed25519.publicKey(seed)
            val signature = Ed25519.sign(seed, "a protocol".encodeToByteArray())
            assertTrue(Ed25519.verify(publicKey, "a protocol".encodeToByteArray(), signature))
            val uhdr = UhdrKeyDerivation.deriveKey(credentialSeed, ByteArray(32) { 9 })
            val headers = NppsAwsSigV4().sign(
                AwsSigningRequest("GET", "examplebucket.s3.amazonaws.com", "/test.txt", emptyList(), emptyMap(), "UNSIGNED-PAYLOAD", "us-east-1", "s3",
                    AwsCredentials("AKIAIOSFODNN7EXAMPLE", secret, token), "20130524T000000Z"),
            )
            // The token is a header the caller sends, so it is in the answer; it is not in the request.
            assertEquals(token, headers["x-amz-security-token"])
            uhdr
        }) { uhdr -> listOf(hex(seed), hex(credentialSeed), hex(uhdr.k1), secret, token, hex(secret.encodeToByteArray())) }
    }

    // @verifies SWR-MG-32#g
    @Test
    fun anObliviousRequestsEntropyAndResponseSecretAreInNoRequestAndTheSecretIsGoneOnceTheAnswerIsOpenedOrGivenUp() {
        fun core(op: String, build: kotlinx.serialization.json.JsonObjectBuilder.() -> Unit) = NppsCore.migration(buildJsonObject { put("op", op); build() })
        val gateway = core("ohttpGenerateKey") { put("seed", "05".repeat(32)); put("keyId", 1) }
        val keyConfig = gateway["keyConfig"]!!.jsonPrimitive.content
        val entropy = ByteArray(32) { (it * 11 + 5).toByte() }
        // What the response secret is, worked out the older way (everything in the JSON), so it can be searched for.
        val secretHex = core("ohttpSeal") {
            put("keyConfig", keyConfig); put("method", "POST"); put("scheme", "https"); put("authority", "fleet.example"); put("path", "/v1/shdr")
            put("headers", kotlinx.serialization.json.JsonArray(listOf(
                kotlinx.serialization.json.JsonArray(listOf(kotlinx.serialization.json.JsonPrimitive("X-NP-Device-Token"), kotlinx.serialization.json.JsonPrimitive("tok"))),
                kotlinx.serialization.json.JsonArray(listOf(kotlinx.serialization.json.JsonPrimitive("content-type"), kotlinx.serialization.json.JsonPrimitive("application/octet-stream"))),
            )))
            put("body", hex("record".encodeToByteArray())); put("entropy", hex(entropy))
        }["secret"]!!.jsonPrimitive.content
        recording({
            // An answer that is opened: the gateway's reply, made the older way so this test holds no handle of its own.
            val sealed = life.neurone.core.analytics.FleetOhttp.seal(keyConfig, "fleet.example", "/v1/shdr", "X-NP-Device-Token", "tok", "record".encodeToByteArray(), entropy.copyOf())
            assertFalse(sealed.toString().contains(secretHex.take(16)))
            // The gateway below is this test's own stand-in, made the older way: it is not a host request, so it is not recorded.
            NppsCore.requestObserver = null
            val g = core("ohttpGatewayOpen") { put("keyConfig", keyConfig); put("privateKey", gateway["privateKey"]!!); put("request", hex(sealed.request)) }
            val reply = core("ohttpGatewaySeal") {
                put("enc", g["enc"]!!); put("secret", g["secret"]!!); put("status", 204)
                put("headers", kotlinx.serialization.json.JsonArray(emptyList())); put("body", ""); put("entropy", "01".repeat(16))
            }["response"]!!.jsonPrimitive.content
            NppsCore.requestObserver = { seen += it }
            val answer = life.neurone.core.analytics.FleetOhttp.open(sealed, reply.chunked(2).map { it.toInt(16).toByte() }.toByteArray())
            assertEquals(204, answer.status)
            sealed.close()   // after the answer: nothing left to do, and it does not throw
            // One whose answer never comes, and one the relay refuses: the secret is given up.
            life.neurone.core.analytics.FleetOhttp.seal(keyConfig, "fleet.example", "/v1/shdr", "X-NP-Device-Token", "tok", ByteArray(1), entropy.copyOf()).close()
            val refused = life.neurone.core.analytics.FleetOhttp.seal(keyConfig, "fleet.example", "/v1/shdr", "X-NP-Device-Token", "tok", ByteArray(1), entropy.copyOf())
            assertFalse(life.neurone.core.analytics.FleetOhttp.accepted(refused, relayOk = false, contentType = null, body = null))
            Unit
        }) { listOf(secretHex, secretHex.take(16), hex(entropy), hex(entropy).take(16)) }
    }
}
