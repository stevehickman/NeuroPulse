package life.neurone.core.session

import life.neurone.core.protocol.BleTransfer

// Splits a compiled + signed session descriptor into the writes of the Mode-2 PROTOCOL_UPLOAD characteristic.
//
// The wire is the hub transport's own framing, the same on BLE and on USB-C (NP-FW-HUB-001 §2.3, REQ-FWHUB-04, REQ-FWHUB-80):
// a 4-byte little-endian length, then the blob, as one byte stream. The stream is cut into writes of at most
// BleTransfer.MAX_WRITE_SIZE bytes and carries nothing else: no per-write header, so a write boundary means nothing to the
// hub, which reassembles by the length (np_transport.c). Writes are made in order, each acknowledged before the next.
//
// An empty blob is one write of a zero length. The hub refuses a length of 0 (and one above its blob maximum, which the hub
// alone knows and checks); the chunker does not guess either, so a refusal is the hub's.
//
// Pure function (ByteArray → List<ByteArray>); no BLE dependency, fully unit-testable.
// (This replaced a START / CONT / END / SINGLE framing that no hub reassembled: OI-SIMFW-49, the principal's option A.)
object ProtocolChunker {

    fun chunk(data: ByteArray): List<ByteArray> {
        val prefix = BleTransfer.PROTOCOL_LEN_PREFIX
        require(prefix == 4) { "the length prefix is a 32-bit little-endian value" }
        val stream = ByteArray(prefix + data.size)
        val n = data.size
        stream[0] = (n and 0xFF).toByte()
        stream[1] = ((n ushr 8) and 0xFF).toByte()
        stream[2] = ((n ushr 16) and 0xFF).toByte()
        stream[3] = ((n ushr 24) and 0xFF).toByte()
        data.copyInto(stream, prefix)

        val chunks = mutableListOf<ByteArray>()
        var offset = 0
        while (offset < stream.size) {
            val end = minOf(offset + BleTransfer.MAX_WRITE_SIZE, stream.size)
            chunks.add(stream.copyOfRange(offset, end))
            offset = end
        }
        return chunks
    }
}
