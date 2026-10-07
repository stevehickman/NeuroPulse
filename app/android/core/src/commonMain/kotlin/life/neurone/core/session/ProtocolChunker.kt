package life.neurone.core.session

import life.neurone.core.protocol.BleTransfer

// Port of iOS ProtocolChunker (app/ios/NeurOne/Protocol/ProtocolChunker.swift).
// Splits a compiled + signed session descriptor into BLE-MTU-sized framed chunks for the
// Mode-2 PROTOCOL_UPLOAD write characteristic. Framing (must match hub reassembly):
//   START  (0x01): header + 2-byte little-endian total length + ≤509 payload
//   CONT   (0x02): header + ≤511 payload
//   END    (0x03): header + remaining payload
//   SINGLE (0x04): header + payload (whole blob ≤509 bytes)
// Pure function (ByteArray → List<ByteArray>); no BLE dependency, fully unit-testable.
object ProtocolChunker {

    const val FRAME_START: Byte = 0x01
    const val FRAME_CONT: Byte = 0x02
    const val FRAME_END: Byte = 0x03
    const val FRAME_SINGLE: Byte = 0x04

    fun chunk(data: ByteArray): List<ByteArray> {
        // Single-chunk case (fits whole, including the empty blob).
        if (data.size <= BleTransfer.MAX_CHUNK_PAYLOAD) {
            return listOf(byteArrayOf(FRAME_SINGLE) + data)
        }

        val chunks = mutableListOf<ByteArray>()

        // START: header + 2-byte LE total length + first 509 payload bytes.
        val total = data.size and 0xFFFF
        val start = byteArrayOf(
            FRAME_START,
            (total and 0xFF).toByte(),          // little-endian low byte
            ((total ushr 8) and 0xFF).toByte(), // little-endian high byte
        ) + data.copyOfRange(0, BleTransfer.MAX_CHUNK_PAYLOAD)
        chunks.add(start)

        // Remaining payload → CONT chunks, END chunk last.
        var offset = BleTransfer.MAX_CHUNK_PAYLOAD
        while (offset < data.size) {
            val take = minOf(BleTransfer.MAX_CONT_PAYLOAD, data.size - offset)
            val sliceEnd = offset + take
            val isLast = sliceEnd == data.size
            val header = if (isLast) FRAME_END else FRAME_CONT
            chunks.add(byteArrayOf(header) + data.copyOfRange(offset, sliceEnd))
            offset = sliceEnd
        }

        return chunks
    }
}
