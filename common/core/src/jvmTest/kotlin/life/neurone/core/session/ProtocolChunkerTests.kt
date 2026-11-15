package life.neurone.core.session

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.int
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import life.neurone.core.protocol.BleTransfer
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * The protocol upload's framing is the hub transport's: a 4-byte little-endian length, then the blob, as one byte stream cut
 * into writes of at most BleTransfer.MAX_WRITE_SIZE bytes with nothing else added (NP-FW-HUB-001 §2.3, REQ-FWHUB-80).
 * `common/testdata/upload-stream-golden.json` holds the exact bytes of every write for several blobs; the Swift chunker and the
 * simulator's end-to-end test read the same file (OI-SIMFW-49).
 */
class ProtocolChunkerTests {

    private fun hex(b: ByteArray) = b.joinToString("") { "%02x".format(it) }
    private fun bytes(h: String) = ByteArray(h.length / 2) { h.substring(it * 2, it * 2 + 2).toInt(16).toByte() }

    private val fixture by lazy {
        val text = javaClass.getResourceAsStream("/upload-stream-golden.json")!!.bufferedReader().readText()
        Json.parseToJsonElement(text).jsonObject
    }

    /** The bytes the hub's transport reassembles from the writes: all of them, in order. */
    private fun stream(chunks: List<ByteArray>): ByteArray = chunks.fold(ByteArray(0)) { acc, c -> acc + c }

    // @verifies REQ-FWHUB-80#d
    @Test
    fun theFixtureStatesTheWidthsTheChunkerUses() {
        assertEquals(BleTransfer.MAX_WRITE_SIZE, fixture["write_size"]!!.jsonPrimitive.int)
        assertEquals(BleTransfer.PROTOCOL_LEN_PREFIX, fixture["prefix_bytes"]!!.jsonPrimitive.int)
    }

    // @verifies REQ-FWHUB-80#d
    @Test
    fun everyWriteIsTheExactBytesTheFixtureNames() {
        val cases = fixture["cases"]!!.jsonArray
        assertTrue(cases.size >= 6)
        for (c in cases) {
            val name = c.jsonObject["name"]!!.jsonPrimitive.content
            val blob = bytes(c.jsonObject["blob"]!!.jsonPrimitive.content)
            val want = c.jsonObject["chunks"]!!.jsonArray.map { it.jsonPrimitive.content }
            assertEquals(want, ProtocolChunker.chunk(blob).map { hex(it) }, name)
        }
    }

    // @verifies REQ-FWHUB-80#d
    @Test
    fun theStreamIsTheLittleEndianLengthThenTheBlobAndNothingElse() {
        for (size in listOf(1, 100, 508, 509, 510, 1020, 4096, 6187)) {
            val data = ByteArray(size) { ((it * 7) and 0xFF).toByte() }
            val s = stream(ProtocolChunker.chunk(data))
            assertEquals(BleTransfer.PROTOCOL_LEN_PREFIX + size, s.size, "size $size")
            val length = (s[0].toInt() and 0xFF) or ((s[1].toInt() and 0xFF) shl 8) or
                ((s[2].toInt() and 0xFF) shl 16) or ((s[3].toInt() and 0xFF) shl 24)
            assertEquals(size, length, "the prefix at size $size")
            assertContentEquals(data, s.copyOfRange(BleTransfer.PROTOCOL_LEN_PREFIX, s.size), "the blob at size $size")
        }
    }

    // @verifies REQ-FWHUB-80#d
    @Test
    fun noWriteExceedsTheWriteSizeAndEveryWriteButTheLastIsFull() {
        for (size in listOf(0, 1, 507, 508, 509, 1020, 6187)) {
            val chunks = ProtocolChunker.chunk(ByteArray(size) { it.toByte() })
            assertTrue(chunks.all { it.size in 1..BleTransfer.MAX_WRITE_SIZE }, "size $size")
            assertTrue(chunks.dropLast(1).all { it.size == BleTransfer.MAX_WRITE_SIZE }, "size $size")
        }
    }

    // @verifies REQ-FWHUB-80#d
    @Test
    fun everyWriteSizeTheLinkCanReportGivesTheFixturesWrites() {
        val sizes = fixture["write_sizes"]!!.jsonArray.map { it.jsonPrimitive.int }
        assertEquals(listOf(20, 185, 244, 512), sizes, "the default MTU's 20, an iOS 185, the 244 of an MTU of 247, and the ceiling")
        var checked = 0
        for (c in fixture["cases"]!!.jsonArray) {
            val name = c.jsonObject["name"]!!.jsonPrimitive.content
            val blob = bytes(c.jsonObject["blob"]!!.jsonPrimitive.content)
            val bySize = c.jsonObject["chunks_by_write_size"]?.jsonObject ?: continue
            for (size in sizes) {
                val want = bySize[size.toString()]!!.jsonArray.map { it.jsonPrimitive.content }
                assertEquals(want, ProtocolChunker.chunk(blob, size).map { hex(it) }, "$name at $size")
                checked++
            }
        }
        assertTrue(checked >= 20, "five cases at four sizes")
    }

    // @verifies REQ-FWHUB-80#d
    @Test
    fun theWriteSizeIsClampedToTheHubsOneToFiveHundredTwelveAndTheStreamIsTheSameAtEverySize() {
        val data = ByteArray(1500) { ((it * 7) and 0xFF).toByte() }
        assertTrue(ProtocolChunker.chunk(data, 4096).all { it.size <= BleTransfer.MAX_WRITE_SIZE }, "above 512 is cut at 512")
        assertTrue(ProtocolChunker.chunk(data, 0).all { it.size == 1 }, "below 1 is cut at 1")
        assertTrue(ProtocolChunker.chunk(data, -5).all { it.size == 1 })
        val reference = stream(ProtocolChunker.chunk(data, BleTransfer.MAX_WRITE_SIZE))
        for (size in listOf(1, 20, 182, 185, 244, 512)) {
            val chunks = ProtocolChunker.chunk(data, size)
            assertContentEquals(reference, stream(chunks), "the stream is the same at write size $size")
            assertTrue(chunks.all { it.size <= size }, "no write over $size")
            assertTrue(chunks.dropLast(1).all { it.size == size }, "every write but the last is full at $size")
        }
    }

    // @verifies REQ-FWHUB-80#d
    @Test
    fun anEmptyBlobIsOneWriteOfAZeroLengthWhichTheHubRefuses() {
        val chunks = ProtocolChunker.chunk(ByteArray(0))
        assertEquals(1, chunks.size)
        assertContentEquals(ByteArray(4), chunks[0])
    }

    // @verifies REQ-FWHUB-80#d
    @Test
    fun theLargestBlobTheHubTakesIsThirteenWrites() {
        val chunks = ProtocolChunker.chunk(ByteArray(6187) { 1 })
        assertEquals(13, chunks.size)   // (4 + 6187) / 512 rounded up
        assertEquals(6191 - 12 * BleTransfer.MAX_WRITE_SIZE, chunks.last().size)
    }
}
