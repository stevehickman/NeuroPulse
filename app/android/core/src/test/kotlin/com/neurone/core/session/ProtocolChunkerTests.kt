package com.neurone.core.session

import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** Port of the iOS ProtocolChunker tests. Verifies framing + reassembly across the boundaries. */
class ProtocolChunkerTests {

    /** Reassemble framed chunks back into the original payload (mirrors hub firmware). */
    private fun reassemble(chunks: List<ByteArray>): ByteArray {
        val out = mutableListOf<Byte>()
        for (c in chunks) {
            when (c[0]) {
                ProtocolChunker.FRAME_SINGLE -> out.addAll(c.drop(1))
                ProtocolChunker.FRAME_START -> out.addAll(c.drop(3)) // skip header + 2-byte length
                ProtocolChunker.FRAME_CONT, ProtocolChunker.FRAME_END -> out.addAll(c.drop(1))
                else -> error("unknown frame ${c[0]}")
            }
        }
        return out.toByteArray()
    }

    @Test
    fun emptyDataProducesSingleEmptyChunk() {
        val chunks = ProtocolChunker.chunk(ByteArray(0))
        assertEquals(1, chunks.size)
        assertContentEquals(byteArrayOf(ProtocolChunker.FRAME_SINGLE), chunks[0])
    }

    @Test
    fun smallPayloadIsOneSingleChunk() {
        val data = ByteArray(100) { it.toByte() }
        val chunks = ProtocolChunker.chunk(data)
        assertEquals(1, chunks.size)
        assertEquals(ProtocolChunker.FRAME_SINGLE, chunks[0][0])
        assertContentEquals(data, reassemble(chunks))
    }

    @Test
    fun payloadAt509BoundaryIsSingleChunk() {
        val data = ByteArray(509) { (it % 256).toByte() }
        val chunks = ProtocolChunker.chunk(data)
        assertEquals(1, chunks.size)
        assertEquals(ProtocolChunker.FRAME_SINGLE, chunks[0][0])
    }

    @Test
    fun payloadAt510StartsMultiChunk() {
        val data = ByteArray(510) { (it % 256).toByte() }
        val chunks = ProtocolChunker.chunk(data)
        assertEquals(2, chunks.size)
        assertEquals(ProtocolChunker.FRAME_START, chunks[0][0])
        assertEquals(ProtocolChunker.FRAME_END, chunks[1][0])
        // START length field is little-endian 510.
        assertEquals(510, (chunks[0][1].toInt() and 0xFF) or ((chunks[0][2].toInt() and 0xFF) shl 8))
        assertContentEquals(data, reassemble(chunks))
    }

    @Test
    fun largePayloadUsesStartContEndAndReassembles() {
        val data = ByteArray(2000) { (it % 256).toByte() }
        val chunks = ProtocolChunker.chunk(data)
        assertEquals(ProtocolChunker.FRAME_START, chunks.first()[0])
        assertEquals(ProtocolChunker.FRAME_END, chunks.last()[0])
        assertTrue(chunks.drop(1).dropLast(1).all { it[0] == ProtocolChunker.FRAME_CONT })
        // No chunk exceeds the BLE write ceiling.
        assertTrue(chunks.all { it.size <= ProtocolChunker.MAX_WRITE_SIZE })
        assertContentEquals(data, reassemble(chunks))
    }
}
