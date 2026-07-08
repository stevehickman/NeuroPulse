import Testing
import Foundation
@testable import NeurOne

// Unit tests for ProtocolChunker — pure Data → [Data] transform, no BLE needed.
struct ProtocolChunkerTests {

    // Frame header bytes, mirrored from ProtocolChunker.Frame.
    private let START:  UInt8 = 0x01
    private let CONT:   UInt8 = 0x02
    private let END:    UInt8 = 0x03
    private let SINGLE: UInt8 = 0x04

    @Test func emptyDataProducesSingleEmptyChunk() {
        let chunks = ProtocolChunker.chunk(Data())
        #expect(chunks == [Data([0x04])])
        #expect(chunks.count == 1)
        #expect(chunks[0].count == 1)
    }

    @Test func dataBelow509IsSingleChunk() {
        let data = Data(repeating: 0xAB, count: 100)
        let chunks = ProtocolChunker.chunk(data)
        #expect(chunks.count == 1)
        #expect(chunks[0].first == SINGLE)
        #expect(chunks[0].dropFirst() == data)
        #expect(chunks[0].count == 101)         // 1 header + 100 payload
        #expect(chunks[0].count <= ProtocolChunker.maxWriteSize)
    }

    @Test func dataExactly509IsSingleChunk() {
        let data = Data(repeating: 0x11, count: 509)
        let chunks = ProtocolChunker.chunk(data)
        #expect(chunks.count == 1)
        #expect(chunks[0].first == SINGLE)
        #expect(chunks[0].count == 510)         // 1 header + 509 payload
        #expect(chunks[0].dropFirst() == data)
    }

    @Test func dataExactly510IsStartPlusEnd() {
        let data = Data((0..<510).map { UInt8($0 & 0xFF) })
        let chunks = ProtocolChunker.chunk(data)

        #expect(chunks.count == 2)

        // START: header + LE length(510) + 509 payload bytes.
        let start = chunks[0]
        #expect(start.first == START)
        #expect(start.count == 512)             // 1 + 2 + 509
        let lenLow  = start[start.index(start.startIndex, offsetBy: 1)]
        let lenHigh = start[start.index(start.startIndex, offsetBy: 2)]
        let total = UInt16(lenLow) | (UInt16(lenHigh) << 8)
        #expect(total == 510)
        #expect(Array(start.dropFirst(3)) == Array(data.prefix(509)))

        // END: header + 1 remaining payload byte.
        let end = chunks[1]
        #expect(end.first == END)
        #expect(end.count == 2)                 // 1 header + 1 payload
        #expect(Array(end.dropFirst()) == [data[data.index(data.startIndex, offsetBy: 509)]])
    }

    @Test func largeDataProducesStartContEnd() {
        // 509 (START) + 511 (CONT) + 50 (END) = 1070 bytes.
        let data = Data((0..<1070).map { UInt8($0 & 0xFF) })
        let chunks = ProtocolChunker.chunk(data)

        #expect(chunks.count == 3)
        #expect(chunks[0].first == START)
        #expect(chunks[1].first == CONT)
        #expect(chunks[2].first == END)

        // No chunk exceeds the BLE MTU ceiling.
        for c in chunks {
            #expect(c.count <= ProtocolChunker.maxWriteSize)
        }

        // Declared total length matches.
        let lenLow  = chunks[0][chunks[0].index(chunks[0].startIndex, offsetBy: 1)]
        let lenHigh = chunks[0][chunks[0].index(chunks[0].startIndex, offsetBy: 2)]
        #expect((UInt16(lenLow) | (UInt16(lenHigh) << 8)) == 1070)
    }

    @Test func roundTripReassemblyMatchesOriginal() {
        // Reassemble payloads (stripping headers / length) and confirm equality.
        for size in [0, 1, 508, 509, 510, 511, 1020, 1021, 4096] {
            let data = Data((0..<size).map { UInt8(($0 * 7) & 0xFF) })
            let chunks = ProtocolChunker.chunk(data)

            var reassembled = Data()
            for (i, chunk) in chunks.enumerated() {
                let header = chunk.first
                if header == SINGLE {
                    reassembled.append(chunk.dropFirst())
                } else if header == START {
                    reassembled.append(chunk.dropFirst(3))   // skip header + 2 length bytes
                } else if header == CONT || header == END {
                    reassembled.append(chunk.dropFirst())
                } else {
                    Issue.record("unexpected frame header \(String(describing: header)) at index \(i)")
                }
            }
            #expect(reassembled == data, "round-trip failed for size \(size)")
        }
    }
}
