import Foundation

// Decoders and reassembler for the two zone-module characteristics.
//
// The wire contract lives in `firmware/zone_announce/include/np_zone_notify.h`
// and is pinned by `firmware/zone_announce/tests/np_zone_notify_tests.c`. The
// constants below mirror it; if one moves, both sides move in the same commit.
//
// ── Two shapes over one header ───────────────────────────────────────────────
//
// SOCKET_MAP frames carry 8-byte descriptor records and are read once at link.
// ZONE_MODULE_STATUS frames carry 3-byte status records and arrive on every
// change. Both use the same 4-byte header; byte 1's MAP bit says which shape the
// body holds, so a decoder checks rather than infers — reading one as the other
// would misparse silently instead of failing.
//
// ── Why frames fragment ──────────────────────────────────────────────────────
//
// A full 80-socket map is ~644 bytes; a full status snapshot ~244. An ATT
// notification is only guaranteed to carry 20 bytes, so both arrive as ordered
// fragment runs, the last one flagged. `ZoneModuleFrameAssembler` accumulates a
// run and emits it only when the final fragment lands — a partial sequence is
// never handed on as a complete inventory.

/// Shared header constants and parsing for both frame kinds.
enum ZoneFrameFormat {

    /// Mirrors `NP_ZN_FORMAT_VERSION`. Version 2 is the static/dynamic split:
    /// status records dropped 4 → 3 bytes when anatomy moved into the socket
    /// map. Version 1 (anatomy inline on every change) was never deployed.
    /// Version 0 is the retired 5-byte one-byte-per-slot payload, which had no
    /// version byte at all — so a hub still speaking it decodes as 0 and is
    /// rejected rather than misread.
    static let version: UInt8 = 0x02
    static let headerBytes = 4
    static let statusRecordBytes = 3
    static let mapRecordBytes = 8

    /// Mirrors `NP_ZN_MAX_SOCKET_ID` — the full 7-bit major addressing domain.
    static let maxSocketID: UInt8 = 128

    static let flagSnapshot: UInt8 = 0x01
    static let flagLast: UInt8     = 0x02
    static let flagMap: UInt8      = 0x04

    static let recPresent: UInt8 = 0x01
    static let recFault: UInt8   = 0x02
    static let mapWired: UInt8   = 0x01

    struct Header {
        let isSnapshot: Bool
        let isLastFragment: Bool
        let isMap: Bool
        let fragmentIndex: UInt8
        let recordCount: Int
        /// Index of the first record byte, relative to the Data's own startIndex.
        let bodyStart: Int
    }

    /// Validate the header and confirm the body holds exactly the records it
    /// claims, at the record size implied by `expectMap`.
    ///
    /// Returns nil for anything not exactly well-formed — wrong version, wrong
    /// kind, short header, or a record count the payload cannot satisfy.
    /// Presence gates safety-critical placement checks, so a malformed frame is
    /// discarded rather than partially believed.
    static func parseHeader(_ data: Data, expectMap: Bool) -> Header? {
        guard data.count >= headerBytes else { return nil }

        // Index off startIndex — a Data sliced from a larger buffer does not
        // start at 0, and indexing it as if it did is a classic silent misparse.
        let base = data.startIndex
        guard data[base] == version else { return nil }

        let flags = data[base + 1]
        let isMap = (flags & flagMap) != 0
        guard isMap == expectMap else { return nil }

        let count = Int(data[base + 3])
        let recordBytes = isMap ? mapRecordBytes : statusRecordBytes
        guard data.count >= headerBytes + count * recordBytes else { return nil }

        return Header(isSnapshot: (flags & flagSnapshot) != 0,
                      isLastFragment: (flags & flagLast) != 0,
                      isMap: isMap,
                      fragmentIndex: data[base + 2],
                      recordCount: count,
                      bodyStart: headerBytes)
    }

    /// 0 is never emitted; ids above the domain would alias onto a real socket,
    /// which is a wrong-site targeting path, not a display bug.
    static func isValidSocketID(_ id: UInt8) -> Bool { id >= 1 && id <= maxSocketID }
}

// MARK: - Socket map frame (static, read once at link)

struct SocketMapFrame: Equatable {

    let isLastFragment: Bool
    let fragmentIndex: UInt8
    let descriptors: [SocketDescriptor]

    init?(_ data: Data) {
        guard let h = ZoneFrameFormat.parseHeader(data, expectMap: true) else { return nil }
        let base = data.startIndex

        var decoded: [SocketDescriptor] = []
        decoded.reserveCapacity(h.recordCount)

        for i in 0..<h.recordCount {
            let r = base + h.bodyStart + i * ZoneFrameFormat.mapRecordBytes
            let socketID = data[r]
            guard ZoneFrameFormat.isValidSocketID(socketID) else { return nil }

            let flags = data[r + 1]
            // Aircraft body axes, int16 little-endian: +x forward, +y right,
            // +z down. No lobe or side — the helmet reports position only.
            func int16(at offset: Int) -> Int16 {
                Int16(bitPattern: UInt16(data[r + offset])
                      | (UInt16(data[r + offset + 1]) << 8))
            }

            decoded.append(SocketDescriptor(
                socketID: socketID,
                position: SocketPosition(forwardMm: int16(at: 2),
                                         rightMm: int16(at: 4),
                                         downMm: int16(at: 6)),
                isWiredInShell: (flags & ZoneFrameFormat.mapWired) != 0))
        }

        self.isLastFragment = h.isLastFragment
        self.fragmentIndex = h.fragmentIndex
        self.descriptors = decoded
    }

    /// Direct initialiser for tests and for assembling a completed run.
    init(isLastFragment: Bool, fragmentIndex: UInt8, descriptors: [SocketDescriptor]) {
        self.isLastFragment = isLastFragment
        self.fragmentIndex = fragmentIndex
        self.descriptors = descriptors
    }
}

// MARK: - Status frame (dynamic, one per change)

struct ZoneModuleFrame: Equatable {

    let isSnapshot: Bool
    let isLastFragment: Bool
    let fragmentIndex: UInt8
    let records: [ZoneModuleStatus]

    init?(_ data: Data) {
        guard let h = ZoneFrameFormat.parseHeader(data, expectMap: false) else { return nil }
        let base = data.startIndex

        var decoded: [ZoneModuleStatus] = []
        decoded.reserveCapacity(h.recordCount)

        for i in 0..<h.recordCount {
            let r = base + h.bodyStart + i * ZoneFrameFormat.statusRecordBytes
            let socketID = data[r]
            guard ZoneFrameFormat.isValidSocketID(socketID) else { return nil }

            let typeRaw  = data[r + 1]
            let recFlags = data[r + 2]

            let fault = (recFlags & ZoneFrameFormat.recFault) != 0
            // Firmware clears presence on fault; re-assert it here so a hub that
            // ever sends both cannot produce a "present" faulted module.
            let present = (recFlags & ZoneFrameFormat.recPresent) != 0 && !fault

            decoded.append(ZoneModuleStatus(
                socketID: socketID,
                moduleType: ZoneModuleType(rawValue: typeRaw) ?? .unknown,
                isPresent: present,
                hasFault: fault))
        }

        self.isSnapshot     = h.isSnapshot
        self.isLastFragment = h.isLastFragment
        self.fragmentIndex  = h.fragmentIndex
        self.records        = decoded
    }

    /// Direct initialiser for tests and for assembling a completed run.
    init(isSnapshot: Bool, isLastFragment: Bool, fragmentIndex: UInt8,
         records: [ZoneModuleStatus]) {
        self.isSnapshot = isSnapshot
        self.isLastFragment = isLastFragment
        self.fragmentIndex = fragmentIndex
        self.records = records
    }
}

// MARK: - Fragment reassembly

/// Accumulates ordered fragment runs. Generic over the payload element so the
/// same ordering rules govern both characteristics — the map and the status
/// snapshot have identical failure modes, and one implementation means one place
/// for them to be right.
///
/// Deliberately strict about ordering. A dropped or out-of-order fragment
/// abandons the run rather than committing with a hole in it — an inventory
/// missing a socket reads as "that module is not installed", which is exactly the
/// wrong answer for a placement gate.
struct FragmentRunAssembler<Element> {

    /// A run cannot legitimately describe more sockets than the addressing
    /// domain holds. Bounds the buffer so a hub that never sends a terminating
    /// fragment cannot grow it without limit.
    private let maxRecords = Int(ZoneFrameFormat.maxSocketID)

    private var pending: [Element] = []
    private var expectedFragment: UInt8 = 0
    private var accumulating = false

    var isAccumulating: Bool { accumulating }

    /// Feed one fragment's payload. Returns the completed run, or nil when more
    /// fragments are needed (or the run was abandoned).
    mutating func accept(fragmentIndex: UInt8, isLast: Bool,
                         elements: [Element]) -> [Element]? {
        if fragmentIndex == 0 {
            pending = []
            expectedFragment = 0
            accumulating = true
        } else if !accumulating || fragmentIndex != expectedFragment {
            reset()
            return nil
        }

        guard pending.count + elements.count <= maxRecords else {
            reset()
            return nil
        }

        pending.append(contentsOf: elements)
        expectedFragment = fragmentIndex &+ 1

        guard isLast else { return nil }
        let complete = pending
        reset()
        return complete
    }

    mutating func reset() {
        pending = []
        expectedFragment = 0
        accumulating = false
    }
}

/// Status-frame assembler: snapshots accumulate, deltas pass through.
struct ZoneModuleFrameAssembler {

    private var run = FragmentRunAssembler<ZoneModuleStatus>()
    /// Deltas that arrived mid-run. A snapshot describes the hardware as of when
    /// the hub started sending it, so a change observed DURING that run is newer
    /// and must survive the commit — otherwise pulling a tile mid-snapshot
    /// resurrects it as present, and presence gates placement checks.
    private var deferredDeltas: [ZoneModuleStatus] = []

    var isAccumulating: Bool { run.isAccumulating }

    mutating func accept(_ frame: ZoneModuleFrame) -> ZoneModuleFrame? {
        guard frame.isSnapshot else {
            guard frame.isLastFragment else { return nil }
            if run.isAccumulating {
                deferredDeltas.append(contentsOf: frame.records)
                return nil
            }
            return frame
        }

        guard let records = run.accept(fragmentIndex: frame.fragmentIndex,
                                       isLast: frame.isLastFragment,
                                       elements: frame.records) else {
            if !run.isAccumulating { deferredDeltas = [] }   // run abandoned
            return nil
        }

        // Deltas last, so a change seen during the run wins over the snapshot's
        // older view of the same socket.
        let merged = records + deferredDeltas
        deferredDeltas = []
        return ZoneModuleFrame(isSnapshot: true, isLastFragment: true,
                               fragmentIndex: 0, records: merged)
    }

    mutating func reset() {
        run.reset()
        deferredDeltas = []
    }
}

/// Socket-map assembler. A map is always a complete snapshot, so there are no
/// deltas to reconcile.
struct SocketMapFrameAssembler {

    private var run = FragmentRunAssembler<SocketDescriptor>()

    var isAccumulating: Bool { run.isAccumulating }

    mutating func accept(_ frame: SocketMapFrame) -> SocketMapFrame? {
        guard let descriptors = run.accept(fragmentIndex: frame.fragmentIndex,
                                           isLast: frame.isLastFragment,
                                           elements: frame.descriptors) else { return nil }
        return SocketMapFrame(isLastFragment: true, fragmentIndex: 0,
                              descriptors: descriptors)
    }

    mutating func reset() { run.reset() }
}
