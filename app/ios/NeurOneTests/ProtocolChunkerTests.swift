import Testing
import Foundation
@testable import NeurOne

// Unit tests for ProtocolChunker: a pure Data → [Data] transform, no BLE needed.
//
// The upload's framing is the hub transport's: a 4-byte little-endian length, then the blob, as one byte stream cut into
// writes of at most BleTransfer.MAX_WRITE_SIZE bytes with nothing else added (NP-FW-HUB-001 §2.3, REQ-FWHUB-80).
// common/testdata/upload-stream-golden.json holds the exact bytes of every write for several blobs; the Kotlin chunker and
// the simulator's end-to-end test read the same file (OI-SIMFW-49).
struct ProtocolChunkerTests {

    private func pattern(_ count: Int) -> Data {
        Data((0..<count).map { UInt8(($0 * 7 + 3) & 0xFF) })
    }

    private func hex(_ data: Data) -> String {
        data.map { String(format: "%02x", $0) }.joined()
    }

    private func bytes(_ hex: String) -> Data {
        var out = Data()
        var index = hex.startIndex
        while index < hex.endIndex {
            let next = hex.index(index, offsetBy: 2)
            out.append(UInt8(hex[index..<next], radix: 16)!)
            index = next
        }
        return out
    }

    private static func fixture(file: StaticString = #filePath) throws -> [String: Any] {
        // .../app/ios/NeurOneTests/<this file> → the repository root, four levels up.
        var url = URL(fileURLWithPath: "\(file)")
        for _ in 0..<4 { url.deleteLastPathComponent() }
        url.appendPathComponent("common/testdata/upload-stream-golden.json")
        let object = try JSONSerialization.jsonObject(with: Data(contentsOf: url))
        return try #require(object as? [String: Any])
    }

    /// The bytes the hub's transport reassembles from the writes: all of them, in order.
    private func stream(_ chunks: [Data]) -> Data {
        chunks.reduce(into: Data()) { $0.append($1) }
    }

    // @verifies REQ-FWHUB-80#d
    @Test func theFixtureStatesTheWidthsTheChunkerUses() throws {
        let fixture = try Self.fixture()
        #expect(fixture["write_size"] as? Int == BleTransfer.MAX_WRITE_SIZE)
        #expect(fixture["prefix_bytes"] as? Int == BleTransfer.PROTOCOL_LEN_PREFIX)
    }

    // @verifies REQ-FWHUB-80#d
    @Test func everyWriteIsTheExactBytesTheFixtureNames() throws {
        let cases = try #require(try Self.fixture()["cases"] as? [[String: Any]])
        #expect(cases.count >= 6)
        for c in cases {
            let name = try #require(c["name"] as? String)
            let blob = bytes(try #require(c["blob"] as? String))
            let want = try #require(c["chunks"] as? [String])
            #expect(ProtocolChunker.chunk(blob).map(hex) == want, "\(name)")
        }
    }

    // @verifies REQ-FWHUB-80#d
    @Test func theStreamIsTheLittleEndianLengthThenTheBlobAndNothingElse() {
        for size in [1, 100, 508, 509, 510, 1020, 4096, 6148] {
            let data = pattern(size)
            let s = stream(ProtocolChunker.chunk(data))
            #expect(s.count == BleTransfer.PROTOCOL_LEN_PREFIX + size, "size \(size)")
            let length = Int(s[0]) | (Int(s[1]) << 8) | (Int(s[2]) << 16) | (Int(s[3]) << 24)
            #expect(length == size, "the prefix at size \(size)")
            #expect(Data(s.dropFirst(BleTransfer.PROTOCOL_LEN_PREFIX)) == data, "the blob at size \(size)")
        }
    }

    // @verifies REQ-FWHUB-80#d
    @Test func noWriteExceedsTheWriteSizeAndEveryWriteButTheLastIsFull() {
        for size in [0, 1, 507, 508, 509, 1020, 6148] {
            let chunks = ProtocolChunker.chunk(pattern(size))
            #expect(chunks.allSatisfy { $0.count >= 1 && $0.count <= BleTransfer.MAX_WRITE_SIZE }, "size \(size)")
            #expect(chunks.dropLast().allSatisfy { $0.count == BleTransfer.MAX_WRITE_SIZE }, "size \(size)")
        }
    }

    // @verifies REQ-FWHUB-80#d
    @Test func anEmptyBlobIsOneWriteOfAZeroLengthWhichTheHubRefuses() {
        let chunks = ProtocolChunker.chunk(Data())
        #expect(chunks == [Data([0, 0, 0, 0])])
    }

    // @verifies REQ-FWHUB-80#d
    @Test func theLargestBlobTheHubTakesIsThirteenWrites() {
        let chunks = ProtocolChunker.chunk(pattern(6148))
        #expect(chunks.count == 13)   // (4 + 6148) / 512 rounded up
        #expect(chunks.last?.count == 6152 - 12 * BleTransfer.MAX_WRITE_SIZE)
    }
}
