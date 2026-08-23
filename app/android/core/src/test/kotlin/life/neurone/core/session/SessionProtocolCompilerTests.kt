package life.neurone.core.session

import life.neurone.core.protocol.NPModalityParams
import life.neurone.core.protocol.NPPBMTarget
import life.neurone.core.protocol.NPPBMTranscranialParams
import life.neurone.core.protocol.NPProtocolDefinition
import life.neurone.core.protocol.NPProtocolModality
import life.neurone.core.protocol.NPTimingMode
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * Verifies the session-descriptor compiler: fromDefinition mapping, canonical JSON payload,
 * SHA-256-signed blob wire format, and chunk round-trip. Uses a fake signer so the pure-JVM
 * compile path is fully testable without platform Ed25519.
 */
class SessionProtocolCompilerTests {

    private val fakeSigner = ProtocolSigner { digest ->
        // Echo the digest as a deterministic 64-byte "signature" for assertions.
        SignatureResult(signature = digest + digest, publicKeyFingerprint = "deadbeef")
    }

    private fun pbmDefinition() = NPProtocolDefinition(
        name = "Test PBM",
        timingMode = NPTimingMode.Duration(1200),
        modalities = listOf(
            NPProtocolModality(
                params = NPModalityParams.PbmTranscranial(
                    NPPBMTranscranialParams(
                        target = NPPBMTarget.Named(listOf("Frontal Left")),
                        intensityPercent = 80.0, frequencyHz = 40.0, dutyCyclePercent = 25,
                    ),
                ),
            ),
        ),
    )

    @Test
    fun fromDefinitionMapsT1Modalities() {
        val proto = NPSessionProtocol.fromDefinition(pbmDefinition())
        assertEquals("Test PBM", proto.name)
        assertEquals(1200, proto.totalDurationSeconds)
        assertEquals(1, proto.modalities.size)
        val pbm = proto.modalities.first() as ModalityConfig.PbmTranscranial
        // Named zones resolve to real 1-based socket ids through SocketZones —
        // the generated map iOS resolves against too. The retired FRONT selector
        // used to yield the five-slot indices 0–2.
        assertContentEquals(
            listOf(1, 2, 4, 5, 6, 10, 11, 12, 13, 17, 18, 19, 20, 26, 27, 28, 29, 35, 36, 37),
            pbm.zones,
        )
        assertEquals(40.0, pbm.frequencyHz)
        assertEquals(1200, pbm.durationSeconds)
    }

    @Test
    fun disabledModalitiesAreDropped() {
        val def = NPProtocolDefinition(
            name = "Half off",
            modalities = listOf(
                NPProtocolModality(params = NPModalityParams.PbmTranscranial(NPPBMTranscranialParams()), enabled = false),
            ),
        )
        assertEquals(0, NPSessionProtocol.fromDefinition(def).modalities.size)
    }

    @Test
    fun blobWireFormatHasMagicLengthAndSignature() {
        val blob = SessionProtocolCompiler(fakeSigner).compile(pbmDefinition())
        val wire = blob.wireFormat

        // Magic "NPPR".
        assertContentEquals(SignedProtocolBlob.MAGIC, wire.copyOfRange(0, 4))
        // 4-byte LE payload length matches the payload.
        val len = (wire[4].toInt() and 0xFF) or
            ((wire[5].toInt() and 0xFF) shl 8) or
            ((wire[6].toInt() and 0xFF) shl 16) or
            ((wire[7].toInt() and 0xFF) shl 24)
        assertEquals(blob.payload.size, len)
        // Trailing 64-byte signature.
        assertEquals(64, blob.signature.size)
        assertEquals(8 + blob.payload.size + 64, wire.size)
    }

    @Test
    fun payloadIsValidJsonWithTypeDiscriminator() {
        val blob = SessionProtocolCompiler(fakeSigner).compile(pbmDefinition())
        val jsonText = String(blob.payload, Charsets.UTF_8)
        assertTrue(jsonText.contains("\"name\":\"Test PBM\""))
        assertTrue(jsonText.contains("\"type\":\"pbm_transcranial\""))
    }

    @Test
    fun compiledBlobChunksAndReassembles() {
        val blob = SessionProtocolCompiler(fakeSigner).compile(pbmDefinition())
        val chunks = ProtocolChunker.chunk(blob.wireFormat)
        assertTrue(chunks.all { it.size <= ProtocolChunker.MAX_WRITE_SIZE })
        // Small blob → single chunk carrying the whole wire format after the frame byte.
        val reassembled = chunks.flatMap { c ->
            when (c[0]) {
                ProtocolChunker.FRAME_SINGLE, ProtocolChunker.FRAME_CONT, ProtocolChunker.FRAME_END -> c.drop(1)
                ProtocolChunker.FRAME_START -> c.drop(3)
                else -> error("bad frame")
            }
        }.toByteArray()
        assertContentEquals(blob.wireFormat, reassembled)
    }
}
