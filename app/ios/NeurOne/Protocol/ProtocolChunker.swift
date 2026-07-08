import Foundation

// BLE protocol blob chunking for Mode 2 protocol upload.
//
// The hub firmware caps a single BLE 5 GATT write at 512 bytes (ATT MTU).
// Compiled NPPS session descriptors can exceed this, so large blobs are split
// into framed chunks. Framing protocol (matches hub firmware reassembly):
//
//   START  (0x01): header byte + 2-byte little-endian total length + ≤509 payload
//   CONT   (0x02): header byte + ≤511 payload
//   END    (0x03): header byte + remaining payload
//   SINGLE (0x04): header byte + payload (whole blob fits in one chunk, ≤509 bytes)
//
// `chunk(_:)` is a pure function (Data → [Data]) with no side effects, so it is
// unit-testable without any BLE stack.
enum ProtocolChunker {

    /// Frame header bytes — must match hub firmware reassembly state machine.
    enum Frame: UInt8 {
        case start  = 0x01
        case cont   = 0x02
        case end    = 0x03
        case single = 0x04
    }

    /// Total BLE 5 ATT MTU write ceiling, in bytes.
    static let maxWriteSize = 512

    /// Max payload in a START chunk: 512 − 1 header − 2 length = 509 bytes.
    /// Also the threshold below/at which a blob ships as a single SINGLE chunk.
    static let maxChunkPayload = 509

    /// Max payload in a CONT or END chunk: 512 − 1 header = 511 bytes.
    private static let maxContPayload = 511

    /// Split a protocol blob into framed BLE-MTU-sized chunks.
    ///
    /// - Empty data → a single empty SINGLE chunk (`[Data([0x04])]`); the hub
    ///   accepts it.
    /// - `data.count ≤ 509` → one SINGLE chunk.
    /// - Otherwise → START chunk (509-byte payload) + zero or more CONT chunks
    ///   (511-byte payload each) + one END chunk with the remainder.
    static func chunk(_ data: Data) -> [Data] {
        // Single-chunk case: fits whole (including the empty blob).
        if data.count <= maxChunkPayload {
            var chunk = Data([Frame.single.rawValue])
            chunk.append(data)
            return [chunk]
        }

        var chunks: [Data] = []

        // START chunk: header + 2-byte LE total length + first 509 payload bytes.
        let total = UInt16(truncatingIfNeeded: data.count)
        var start = Data([Frame.start.rawValue])
        start.append(UInt8(total & 0xFF))          // little-endian low byte
        start.append(UInt8((total >> 8) & 0xFF))   // little-endian high byte
        let startEnd = data.index(data.startIndex, offsetBy: maxChunkPayload)
        start.append(contentsOf: data[data.startIndex..<startEnd])
        chunks.append(start)

        // Remaining payload, split into CONT chunks with an END chunk last.
        var offset = startEnd
        while offset < data.endIndex {
            let remaining = data.distance(from: offset, to: data.endIndex)
            let take = min(maxContPayload, remaining)
            let sliceEnd = data.index(offset, offsetBy: take)
            let isLast = sliceEnd == data.endIndex

            var chunk = Data([(isLast ? Frame.end : Frame.cont).rawValue])
            chunk.append(contentsOf: data[offset..<sliceEnd])
            chunks.append(chunk)

            offset = sliceEnd
        }

        return chunks
    }
}
