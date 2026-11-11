import Foundation

// Splits a compiled + signed session descriptor into the writes of the Mode 2 PROTOCOL_UPLOAD characteristic.
//
// The wire is the hub transport's own framing, the same on BLE and on USB-C (NP-FW-HUB-001 §2.3, REQ-FWHUB-04,
// REQ-FWHUB-80): a 4-byte little-endian length, then the blob, as one byte stream. The stream is cut into writes of at
// most the write size and carries nothing else: no per-write header, so a write boundary means nothing to the hub, which
// reassembles by the length (np_transport.c). Writes are made in order, each acknowledged before the next.
//
// The write size is the caller's: the link's negotiated ATT_MTU less 3 bytes (what CoreBluetooth reports as
// maximumWriteValueLength(for: .withResponse); 20 on a link that negotiated none), clamped here to 1...BleTransfer.MAX_WRITE_SIZE
// because the hub's characteristic takes 1 to 512 bytes a write (REQ-FWHUB-80, OI-SIMFW-63).
//
// An empty blob is one write of a zero length. The hub refuses a length of 0 (and one above its blob maximum, which the
// hub alone knows and checks); the chunker does not guess either, so a refusal is the hub's.
//
// `chunk(_:)` is a pure function (Data → [Data]) with no side effects, so it is unit-testable without any BLE stack.
// (This replaced a START / CONT / END / SINGLE framing that no hub reassembled: OI-SIMFW-49, the principal's option A.)
enum ProtocolChunker {

    /// The upload stream for `data`, as the writes to make, in order, each of at most `writeSize` bytes (clamped to 1...512).
    static func chunk(_ data: Data, writeSize: Int = BleTransfer.MAX_WRITE_SIZE) -> [Data] {
        let size = min(max(writeSize, 1), BleTransfer.MAX_WRITE_SIZE)
        let n = UInt32(truncatingIfNeeded: data.count)
        var stream = Data([
            UInt8(n & 0xFF),
            UInt8((n >> 8) & 0xFF),
            UInt8((n >> 16) & 0xFF),
            UInt8((n >> 24) & 0xFF),
        ])
        assert(stream.count == BleTransfer.PROTOCOL_LEN_PREFIX)
        stream.append(data)

        var chunks: [Data] = []
        var offset = 0
        while offset < stream.count {
            let end = min(offset + size, stream.count)
            chunks.append(Data(stream[offset..<end]))
            offset = end
        }
        return chunks
    }
}
